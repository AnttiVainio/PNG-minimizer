#include <iostream>
#include <memory>
#include <cstring>
#include <cmath>
#if defined(_OPENMP)
	#include <omp.h>
#endif
#include "funcs.hpp"
#include "settings.hpp"

/**
 * Find the best filter for each row of the image
 *
 * Filtering levels:
 * 0: 0,5 / 0,*,5 (first pass / refine pass ; * means keep applied filter )
 * 1: 0-4
 * 2: 0-4 + 0,*,5 (current row + next row ; * means use same as current row)
 * 3: 0-4 + 0-4   (current row + next row)
 */
void findFiltering(const unsigned char *data, unsigned char *transmit, const unsigned int height, const unsigned int bpp, const unsigned int rowbyte, unsigned int WINDOW, const unsigned char level, const bool refine, const unsigned char s) {
	unsigned char TEST[] = {2, 5, 15, 25};
	if (refine) TEST[0] = 3;
	if (level >= 2) WINDOW = std::max(WINDOW, rowbyte * 2);

	const std::unique_ptr<const unsigned char[]> zerorow(new const unsigned char[rowbyte]());
	unsigned char bestfilter;
	unsigned int bestlen;
	unsigned int print[] = {0,0,0,0,0};
	const unsigned int printlimit = std::max(1u, (unsigned int)std::ceil(height / 75.0));

	if (LOG > 2) {
		for (unsigned char i = 0; i < std::min(height, (unsigned int)std::ceil((float)height / printlimit)); i++) std::cout << ' ';
		std::cout << "|\r" << std::flush;
	}

	// Optimize worst case
	// Max threads 4 is quite common and level 1 has 5 loops
	#if defined(_OPENMP)
		if (level == 1 && MAX_THREADS == 4) omp_set_num_threads(5);
	#endif

	#pragma omp parallel
	{
		unsigned int deflatelen2;
		const std::unique_ptr<unsigned char[]> filtered(new unsigned char[(rowbyte + 1) * height]);
		if (refine) memcpy(filtered.get(), transmit, (rowbyte + 1) * height);

		// Go through the image rows to select the best filter for each row
		for (unsigned int y = 0; y < height; y++) {
			#pragma omp single
			{
				bestfilter = 0;
				bestlen = 0;
			}
			unsigned char *filteredpos = filtered.get() + y * (rowbyte + 1);
			const unsigned char prevFilter = filteredpos[0];
			if (!refine) filteredpos[0] = 255;
			const unsigned char level0filters[] = {0, 5, prevFilter};

			#pragma omp for schedule(dynamic)
			for (unsigned char test = 0; test < TEST[level]; test++) {
				const unsigned char filter = test % 5;
				const unsigned char filter2 = test / 5;
				bool skip = false;

				// Skip when the filtering would be tested in another iteration
				// Filter current row
				if (!level && filter == 2 && !prevFilter) skip = true;
				else {
					const unsigned char applied1 = applyfilter(level ? filter : level0filters[filter],
						rowbyte, std::max(1u, bpp), filteredpos, data + (height - y - 1) * rowbyte, y ? data + (height - y) * rowbyte : zerorow.get(), 0, refine);
					if (!level && filter == 1 && (!applied1 || (refine && applied1 == prevFilter))) skip = true;
				}

				// Filter next row
				if (level >= 2 && y + 1 != height) {
					if (level == 2 && !filter && filter2 == 1) skip = true;
					else {
						const unsigned char applied2 = applyfilter(level == 2 ? (filter2 == 2 ? 5 : (filter2 ? filter : 0)) : filter2,
							rowbyte, std::max(1u, bpp), filteredpos + rowbyte + 1, data + (height - y - 2) * rowbyte, data + (height - y - 1) * rowbyte, 0, refine);
						if (level == 2 && filter2 == 2 && (!applied2 || applied2 == filter)) skip = true;
					}
				}

				// Test compressing with zlib
				if (!skip) {
					const unsigned int y0 = level < 2 ? y + 1 : std::min(y + 2, height);
					const unsigned int amount = std::min(WINDOW, (rowbyte + 1) * y0);
					const unsigned int pos1 = (rowbyte + 1) * y0 - amount;
					// When refining, test beyond the current row as all rows already have some valid filtering
					if (refine) {
						const unsigned int amount2 = std::min(amount + WINDOW / 3, (rowbyte + 1) * height - pos1);
						if (s <= 3) zerr(def(filtered.get() + pos1, amount2, 0, deflatelen2, true, 9, 9, s));
						else deflatelen2 = zopfliDeflate(filtered.get() + pos1, amount2, 0, true);
					}
					else {
						if (s <= 3) zerr(def(filtered.get() + pos1, amount, 0, deflatelen2, true, 9, 9, s));
						else deflatelen2 = zopfliDeflate(filtered.get() + pos1, amount, 0, true);
					}

					#pragma omp critical
					if (!bestlen || deflatelen2 < bestlen || (deflatelen2 == bestlen && filteredpos[0] < bestfilter)) {
						bestfilter = filteredpos[0];
						bestlen = deflatelen2;
					}
				}
			}
			if (bestfilter != filteredpos[0]) {
				applyfilter(bestfilter, rowbyte, std::max(1u, bpp), filteredpos, data + (height - y - 1) * rowbyte, y ? data + (height - y) * rowbyte : zerorow.get());
			}
			#pragma omp single
			if (LOG > 2) {
				print[bestfilter]++;
				if (print[bestfilter] == printlimit) {
					print[bestfilter] = 0;
					std::cout << (int)bestfilter << std::flush;
				}
			}
		}
		#pragma omp single
		memcpy(transmit, filtered.get(), (rowbyte + 1) * height);
	}
	if (LOG > 2) std::cout << std::endl;

	#if defined(_OPENMP)
		if (level == 1 && MAX_THREADS == 4) omp_set_num_threads(MAX_THREADS);
	#endif
}

/**
 * Find the best filtering and zopfli compression settings
 * Filtering 6 is the filtering done by this program
 *
 * Tested settings on each level (additive):
 * 0: bs=0 bsmax=0 filtering=6
 * 1: bs=0/1
 * 2: filtering=0/5/6
 * 3: bsmax=0/15
 */
unsigned int findSettings(const unsigned char *data, const unsigned int height, const unsigned int bpp, const unsigned int rowbyte, const unsigned char *filtered, const unsigned int len, unsigned char **dest, const unsigned int previousBest, unsigned char &bs, unsigned char &bsmax, unsigned char &filtering, const unsigned char level, const unsigned char s) {
	#define TESTS2 6
	#define TESTS3 9
	const unsigned char TESTS[] = {1, 2, TESTS2, TESTS3};
	const unsigned char TESTS_2[] = {1, 3, 3 * 9, 3 * 9};

	unsigned char progress = 0;
	unsigned int previousBest2 = 0;
	unsigned int best = 0;
	unsigned char besti = 0;
	*dest = 0;

	std::unique_ptr<const unsigned char[]> zerorow;
	std::unique_ptr<unsigned char[]> filtered5;
	std::unique_ptr<unsigned char[]> filtered0;

	if (level >= 2 || (s <= 3 && level)) {
		zerorow.reset(new const unsigned char[rowbyte]());
		filtered5.reset(new unsigned char[len]);
		filtered0.reset(new unsigned char[len]);

		#pragma omp parallel for
		for (unsigned int y = 0; y < height; y++) {
			unsigned char *filteredpos5 = filtered5.get() + y * (rowbyte + 1);
			filteredpos5[0] = 255;
			applyfilter(5, rowbyte, std::max(1u, bpp), filteredpos5, data + (height - y - 1) * rowbyte, y ? data + (height - y) * rowbyte : zerorow.get());
			unsigned char *filteredpos0 = filtered0.get() + y * (rowbyte + 1);
			filteredpos0[0] = 255;
			applyfilter(0, rowbyte, std::max(1u, bpp), filteredpos0, data + (height - y - 1) * rowbyte, y ? data + (height - y) * rowbyte : zerorow.get());
		}
	}

	// Run the tests in a different order so that slower tests are earlier (except keep 0 as first)
	// This should give slight speed improvement with multiple threads
	const unsigned char reorder2[TESTS2] = {0, 7, 1, 4, 3, 6};
	const unsigned char reorder3[TESTS3] = {0, 7, 1, 4, 2, 5, 8, 3, 6};

	const unsigned char *f[] = {filtered, filtered5.get(), filtered0.get()};

	#pragma omp parallel for schedule(dynamic)
	for (unsigned char i0 = 0; i0 < (s <= 3 ? TESTS_2[level] : TESTS[level]); i0++) {
		unsigned char i = i0;
		unsigned char *dest2 = 0;
		unsigned char bs2;
		unsigned char bsmax2;
		unsigned char filtering2;
		unsigned int deflatelen2;

		if (s <= 3) {
			bs2 = s;
			bsmax2 = i / 3 ? i / 3 : 9;
			filtering2 = i % 3;

			dest2 = new unsigned char[len + 35000];
			zerr(def(f[filtering2], len, dest2, deflatelen2, false, 9, bsmax2, s));
			// deflatelen2 can be 0 if deflate fails
		}
		else {
			if (level == 2) i = reorder2[i0];
			if (level == 3) i = reorder3[i0];

			bs2 = i % 3 == 0 ? 0 : 1;
			bsmax2 = i % 3 == 2 ? 15 : 0;
			filtering2 = i / 3;

			deflatelen2 = zopfliDeflate(f[filtering2], len, &dest2, false, I, bs2, bsmax2);
		}

		#pragma omp critical
		{
			if (!i) {
				if (LOG > 2) {
					previousBest2 = deflatelen2 ? deflatelen2 : len;
					printChange(previousBest, deflatelen2);
					std::cout << "|";
					for (unsigned char i = 0; i < (s <= 3 ? TESTS_2[level] : TESTS[level]); i++) std::cout << ' ';
					std::cout << "|\r|";
					for (unsigned char i = 0; i < progress; i++) std::cout << '-';
				}
			}
			if (deflatelen2 && (!best || deflatelen2 < best || (deflatelen2 == best && i < besti))) {
				best = deflatelen2;
				besti = i;
				if (*dest) {
					if (s <= 3) delete [] *dest;
					else free(*dest);
				}
				*dest = dest2;
				bs = bs2;
				bsmax = bsmax2;
				filtering = filtering2;
			}
			else {
				if (s <= 3) delete [] dest2;
				else free(dest2);
			}
			if (LOG > 2) {
				if (previousBest2) std::cout << '-' << std::flush;
				else progress++;
			}
		}
	}

	// For logging purposes
	const unsigned char tmp[] = {6,5,0};
	filtering = tmp[filtering];

	if (LOG > 2) {
		std::cout << '\r';
		for (unsigned char i = 0; i < 2 + (s <= 3 ? TESTS_2[level] : TESTS[level]); i++) std::cout << ' ';
		if (s <= 3) std::cout << "\rs = " << (int)bs << "   m = " << (int)bsmax << "   filtering = " << (int)filtering << std::endl;
		else std::cout << "\rbs = " << (int)bs << "   bsmax = " << (int)bsmax << "   filtering = " << (int)filtering << std::endl;
		printChange(previousBest2, best);
	}
	return best;
}
