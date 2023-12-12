#include "../zopfli/src/zopfli/zopfli.h"

size_t zopfliDeflate(const unsigned char *source, const size_t len, unsigned char **dest, const bool testonly, const unsigned char i = 1, const unsigned char bs = 0, const unsigned char bsmax = 0) {
	ZopfliOptions options;
	ZopfliInitOptions(&options);
	options.numiterations = i;
	options.blocksplitting = bs; // 1
	options.blocksplittingmax = bsmax; // 15

	unsigned char* out = 0;
	size_t outsize = 0;
	ZopfliCompress(&options, ZOPFLI_FORMAT_ZLIB, source, len, &out, &outsize);

	if (dest) *dest = out;
	if (testonly) free(out);

	return outsize;
}
