#include <cstring>
#include <cmath>

/*
	For the following functions:
	When the threshold is 0, the function applies filtering to the data
	Otherwise the function sums together the absolute filtered values without the modulo operation (PNG filtering heuristic that is generally used)
	If the threshold > 0, the function will exit early if the sum grows larger
	Return value is the sum or something larger than the threshold if the sum was calculated
*/

int filter0(const unsigned int len, const unsigned int offset, unsigned char *dest, const unsigned char *src1, const int threshold = 0) {
	if (threshold) {
		int count = 0;
		for (unsigned int i = 0; i < len; i++) {
			count += src1[i];
			if (threshold > 0 && count > threshold) return count;
		}
		return count;
	}
	dest[0] = 0;
	memcpy(dest + 1, src1, len);
	return 0;
}

int filter1(const unsigned int len, const unsigned int offset, unsigned char *dest, const unsigned char *src1, const int threshold = 0) {
	if (threshold) {
		int count = 0;
		for (unsigned int i = 0; i < len; i++) {
			if (i < offset) count += src1[i];
			else count += std::abs((int)src1[i] - (int)src1[i - offset]);
			if (threshold > 0 && count > threshold) return count;
		}
		return count;
	}
	dest[0] = 1;
	for (unsigned int i = 0; i < len; i++) {
		if (i < offset) dest[i + 1] = src1[i];
		else dest[i + 1] = ((int)src1[i] - (int)src1[i - offset]) % 256;
	}
	return 0;
}

int filter2(const unsigned int len, const unsigned int offset, unsigned char *dest, const unsigned char *src1, const unsigned char *src2, const int threshold = 0) {
	if (threshold) {
		int count = 0;
		for (unsigned int i = 0; i < len; i++) {
			count += std::abs((int)src1[i] - (int)src2[i]);
			if (threshold > 0 && count > threshold) return count;
		}
		return count;
	}
	dest[0] = 2;
	for (unsigned int i = 0; i < len; i++) dest[i + 1] = ((int)src1[i] - (int)src2[i]) % 256;
	return 0;
}

int filter3(const unsigned int len, const unsigned int offset, unsigned char *dest, const unsigned char *src1, const unsigned char *src2, const int threshold = 0) {
	if (threshold) {
		int count = 0;
		for (unsigned int i = 0; i < len; i++) {
			if (i < offset) count += std::abs((int)src1[i] - int(src2[i] / 2));
			else count += std::abs((int)src1[i] - ((int)src1[i - offset] + (int)src2[i]) / 2);
			if (threshold > 0 && count > threshold) return count;
		}
		return count;
	}
	dest[0] = 3;
	for (unsigned int i = 0; i < len; i++) {
		if (i < offset) dest[i + 1] = ((int)src1[i] - int(src2[i] / 2)) % 256;
		else dest[i + 1] = ((int)src1[i] - ((int)src1[i - offset] + (int)src2[i]) / 2) % 256;
	}
	return 0;
}

int filter4(const unsigned int len, const unsigned int offset, unsigned char *dest, const unsigned char *src1, const unsigned char *src2, const int threshold = 0) {
	if (threshold) {
		int count = 0;
		for (unsigned int i = 0; i < len; i++) {
			const int a = i < offset ? 0 : src1[i - offset];
			const int b = src2[i];
			const int c = i < offset ? 0 : src2[i - offset];
			const int p = a + b - c;
			const int pa = std::abs(p - a);
			const int pb = std::abs(p - b);
			const int pc = std::abs(p - c);
			if (pa <= pb && pa <= pc) count += std::abs((int)src1[i] - a);
			else if (pb <= pc) count += std::abs((int)src1[i] - b);
			else count += std::abs((int)src1[i] - c);
			if (threshold > 0 && count > threshold) return count;
		}
		return count;
	}
	dest[0] = 4;
	for (unsigned int i = 0; i < len; i++) {
		const int a = i < offset ? 0 : src1[i - offset];
		const int b = src2[i];
		const int c = i < offset ? 0 : src2[i - offset];
		const int p = a + b - c;
		const int pa = std::abs(p - a);
		const int pb = std::abs(p - b);
		const int pc = std::abs(p - c);
		if (pa <= pb && pa <= pc) dest[i + 1] = ((int)src1[i] - a) % 256;
		else if (pb <= pc) dest[i + 1] = ((int)src1[i] - b) % 256;
		else dest[i + 1] = ((int)src1[i] - c) % 256;
	}
	return 0;
}

/**
 * Apply the given filter number
 * Filter 5 uses the PNG heuristic to select a good filter
 * alreadyFiltered is used to inform that the destination already contains valid filtered data to avoid wasting time applying the same filter
 * Return value is the selected filter number for filter 5. Otherwise it comes from the other filter functions
 */
int applyfilter(const unsigned char filter, const unsigned int len, const unsigned int offset, unsigned char *dest, const unsigned char *src1, const unsigned char *src2, const int threshold = 0, const bool alreadyFiltered = false) {
	if (alreadyFiltered && !threshold && dest[0] == filter) return 0;
	switch (filter) {
		case 0: return filter0(len, offset, dest, src1, threshold);
		case 1: return filter1(len, offset, dest, src1, threshold);
		case 2: return filter2(len, offset, dest, src1, src2, threshold);
		case 3: return filter3(len, offset, dest, src1, src2, threshold);
		case 4: return filter4(len, offset, dest, src1, src2, threshold);
		case 5:
			unsigned char best = 0;
			int bestcount = -1;
			for (unsigned char f = 0; f < 5; f++) {
				const int count = applyfilter(f, len, offset, dest, src1, src2, bestcount, alreadyFiltered);
				if (bestcount < 0 || count < bestcount) {
					best = f;
					bestcount = count;
				}
			}
			applyfilter(best, len, offset, dest, src1, src2, 0, alreadyFiltered);
			return best;
	}
	return 0;
}
