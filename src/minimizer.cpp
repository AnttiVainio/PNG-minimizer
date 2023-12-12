#include <png.h>
#include <iostream>
#include <iomanip>
#include <memory>
#include <fstream>
#include <sstream>
#include <chrono>
#include <cstring>
#include "funcs.hpp"
#include "settings.hpp"

/**
 * Possible improvements:
 *   support interlaced
 *   support stripping certain chunks
 *   support optimizing when IDAT cannot be reduced
 */

int main(int argc, char **argv) {
	const auto benchmark = std::chrono::high_resolution_clock::now();

	if (handleArguments(argc, argv)) return 1;

	if (!fileExists(argv[argc - 1])) {
		std::cout << "ERROR: Input file " << argv[argc - 1] << " doesn't exist" << std::endl;
		return 1;
	}

	const std::string outputname(std::string((char*)argv[argc - 1]) + (KEEP ? "_2.png" : ".bak"));
	if (outputExists(outputname)) return 1;

	unsigned int width, height, channels, rowbyte, bitdepth;
	bool tRNS, interlaced;
	unsigned int inflatelen;
	std::unique_ptr<const unsigned char[]> data;
	std::basic_string<unsigned char> beforeidat, afteridat;
	unsigned int originalSize = 0;

	bool fail = false;
	unsigned int idatcount = 0;

	#pragma omp parallel for
	for (unsigned char i = 0; i < 2; i++) {
		if (!i) {
			make_crc_table();
			data.reset(loadPNG(argv[argc - 1], width, height, channels, rowbyte, bitdepth, tRNS, interlaced));
			if (!data) {
				std::cout << "ERROR: Could not load image " << argv[argc - 1] << std::endl;
				fail = true;
			}
		}
		else {
			std::ifstream file(argv[argc - 1], std::ifstream::binary | std::ios::ate);
			if (file.bad() || !file.is_open()) {
				std::cout << "ERROR: Could not load image " << argv[argc - 1] << std::endl;
				file.close();
				fail = true;
			}

			std::streamsize size = file.tellg();
			file.seekg(0, std::ios::beg);
			const std::unique_ptr<unsigned char[]> f(new unsigned char[size]);
			file.read((char*)f.get(), size);
			file.close();

			// Find the data in IDAT chunks
			std::basic_string<unsigned char> src;
			unsigned int pos = 12;
			while (pos + 4 < size) {
				const unsigned int chunksize = f[pos - 1] + (f[pos - 2] << 8) + (f[pos - 3] << 16) + (f[pos - 4] << 24);
				const std::string name((char*)(f.get() + pos), 4);
				if (name == "IEND") break;
				if (name == "IDAT") {
					if (!idatcount) beforeidat = std::basic_string<unsigned char>(f.get(), pos - 4);
					src += std::basic_string<unsigned char>(f.get() + pos + 4, chunksize);
					idatcount++;
				}
				pos += 12 + chunksize;
			}
			afteridat = std::basic_string<unsigned char>(f.get() + pos - 4, size - pos + 4);

			originalSize = src.size();

			std::basic_istringstream<unsigned char> source(src);
			zerr(inflen(source, inflatelen));
		}
	}

	if (MULTIIDAT && idatcount <= 1) {
		if (LOG) std::cout << "Input has 1 IDAT chunk - skipping" << std::endl;
		return 1;
	}

	WINDOW = std::max(WINDOW, rowbyte * (F1 >= 3 && F2 >= 3 ? 2 : 1));

	printSettings(argc, argv);

	if (LOG > 2) {
		std::cout
			<< "\nBefore IDAT size: " << std::setw(5) << beforeidat.size()
			<< "\nAfter IDAT size:  " << std::setw(5) << afteridat.size()
			<< "\nIDAT count:       " << std::setw(5) << idatcount
			<< "\nUncompressed size:" << std::setw(5) << ((rowbyte + 1) * height / 1024) << std::endl;
		if (tRNS) std::cout << "NOTE: has tRNS\n";
		std::cout << std::endl;
	}
	if (interlaced) {
		std::cout << "INTERLACED NOT SUPPORTED\n" << std::endl;
		fail = true;
	}
	if (inflatelen / height != rowbyte + 1) {
		std::cout << "ERROR: Rowbyte doesn't match" << std::endl;
		fail = true;
	}
	if (fail) return 1;

	unsigned int bestsize = 0;
	unsigned char *dest = 0;
	unsigned char bestbs = 0;
	unsigned char bestbsmax = 0;
	unsigned char bestfiltering = 0;
	for (unsigned char s = 2; s <= 4; s++) {
		const std::unique_ptr<unsigned char[]> bestfiltered(new unsigned char[(rowbyte + 1) * height]);

		findFiltering(data.get(), bestfiltered.get(), height, (channels * bitdepth) / 8, rowbyte, WINDOW, F1 - 1, false, s);

		if (F2) findFiltering(data.get(), bestfiltered.get(), height, (channels * bitdepth) / 8, rowbyte, WINDOW, F2 - 1, true, s);

		unsigned char bestbs2;
		unsigned char bestbsmax2;
		unsigned char bestfiltering2;
		unsigned char* dest2 = 0;
		const unsigned int bestsize2 = findSettings(data.get(), height, (channels * bitdepth) / 8, rowbyte, bestfiltered.get(), (rowbyte + 1) * height, &dest2, originalSize, bestbs2, bestbsmax2, bestfiltering2, S, s);

		if (bestsize2 && (!bestsize || bestsize2 < bestsize)) {
			bestsize = bestsize2;
			bestbs = bestbs2;
			bestbsmax = bestbsmax2;
			bestfiltering = bestfiltering2;
			if (dest) delete [] dest;
			dest = dest2;
		}
		else if (dest2) delete [] dest2;

		if (LOG > 2) {
			printChange(originalSize, bestsize2);
			std::cout << std::endl;
		}
	}

	if (LOG >= 2) {
		if (bestbs >= 2) std::cout << "Selecting   s = " << (int)bestbs << "   m = " << (int)bestbsmax << "   filtering = " << (int)bestfiltering << "      ";
		else std::cout << "Selecting   bs = " << (int)bestbs << "   bsmax = " << (int)bestbsmax << "   filtering = " << (int)bestfiltering << "      ";
		if (LOG > 2) std::cout << std::endl;
	}
	if (BM && bestsize > originalSize) bestsize = originalSize;

	/* Save the file */

	bool outputted = false;
	if (!DRY && (BM || bestsize < originalSize)) {
		outputted = true;
		if (outputExists(outputname)) return 1;
		std::ofstream output(outputname.c_str(), std::ofstream::binary);
		if (!output.good()) {
			std::cout << "ERROR: Output file is not good" << std::endl;
			return 1;
		}

		if (!BM) {
			unsigned char IDAT[4] = {'I','D','A','T'};
			unsigned long crcv = update_crc(0xffffffffL, IDAT, 4);
			crcv = update_crc(crcv, dest, bestsize) ^ 0xffffffffL;

			output.write((char*)beforeidat.data(), beforeidat.size());
			output.put((bestsize >> 24) & 255);
			output.put((bestsize >> 16) & 255);
			output.put((bestsize >> 8) & 255);
			output.put(bestsize & 255);
			output.write("IDAT", 4);
			output.write((char*)dest, bestsize);
			output.put((crcv >> 24) & 255);
			output.put((crcv >> 16) & 255);
			output.put((crcv >> 8) & 255);
			output.put(crcv & 255);
			output.write((char*)afteridat.data(), afteridat.size());
		}
		else {
			// Just write some arbitrary data when benchmarking
			unsigned int amount = beforeidat.size() + 12 + bestsize + afteridat.size();
			while (amount) {
				const unsigned int amount2 = std::min(amount, bestsize);
				output.write((char*)dest, amount2);
				amount -= amount2;
			}
		}

		if (output.fail()) {
			std::cout << "ERROR: Write failed" << std::endl;
			return 1;
		}
		output.close();
	}
	free(dest);

	/* Check that the output image matches the original */

	bool removed = false;
	if (!BM && outputted) {
		unsigned int width2, height2, channels2, rowbyte2, bitdepth2;
		unsigned int errors = 0;
		{
			const std::unique_ptr<const unsigned char[]> testdata(loadPNG(outputname.c_str(), width2, height2, channels2, rowbyte2, bitdepth2, tRNS, interlaced));
			if (width != width2) std::cout << "ERROR: Width doesn't match" << std::endl;
			if (height != height2) std::cout << "ERROR: Height doesn't match" << std::endl;
			if (channels != channels2) std::cout << "ERROR: Channels doesn't match" << std::endl;
			if (rowbyte != rowbyte2) std::cout << "ERROR: Rowbyte doesn't match" << std::endl;
			if (bitdepth != bitdepth2) std::cout << "ERROR: Bitdepth doesn't match" << std::endl;

			if (width != width2 || height != height2 || channels != channels2 || rowbyte != rowbyte2 || bitdepth != bitdepth2) {
				if (std::remove(outputname.c_str())) std::cout << "ERROR: Could not remove " << outputname << std::endl;
				removed = true;
			}

			for (unsigned int i = 0; i < rowbyte * height; i++) {
				if (data[i] != testdata[i]) errors++;
			}
			if (errors) std::cout << "ERROR: " << errors << " errors" << std::endl;
		}
		if (!removed && errors) {
			if (std::remove(outputname.c_str())) std::cout << "ERROR: Could not remove " << outputname << std::endl;
			removed = true;
		}
		if (!removed && !KEEP) {
			if (std::remove(argv[argc - 1])) std::cout << "ERROR: Could not remove " << argv[argc - 1] << std::endl;
			if (std::rename(outputname.c_str(), argv[argc - 1])) std::cout << "ERROR: Could not move " << outputname << " to " << argv[argc - 1] << std::endl;
		}
	}

	if (LOG > 1) printChange(originalSize, bestsize);
	if (LOG > 2 && !DRY) {
		std::cout << '\n';
		if (removed) std::cout << "There were errors in the output file" << std::endl;
		if (outputted) {
			if (KEEP) std::cout << "Wrote a new file " << outputname << std::endl;
			else std::cout << "Replaced the old file " << argv[argc - 1] << std::endl;
		}
		else std::cout << "Didn't write a new file" << std::endl;
	}

	if (LOG) {
		const auto end = std::chrono::high_resolution_clock::now();
		if (LOG > 1) std::cout << (LOG > 2 ? "\nTime taken: " : "      ")
			<< ((std::chrono::duration_cast<std::chrono::nanoseconds>(end-benchmark).count() / 10000000) / 100.0)
			<< " seconds\n" << std::endl;
		else {
			#if defined(_OPENMP)
				std::cout << MAX_THREADS << " ";
			#endif
			std::cout << (int)bestbs << (int)bestbsmax << (int)bestfiltering << std::setw(5) << ((std::chrono::duration_cast<std::chrono::nanoseconds>(end-benchmark).count() / 1000000000)) << "s  ";
			printChange(originalSize, bestsize);
			std::cout << "   " << argv[argc - 1] << std::endl;
		}
	}
	return 0;
}
