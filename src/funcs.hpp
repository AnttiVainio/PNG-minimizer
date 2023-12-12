bool handleArguments(const int argc, char **argv);
void printSettings(const int argc, char **argv);

unsigned char *loadPNG(const char *filename, unsigned int &width, unsigned int &height, unsigned int &_channels, unsigned int &rowbyte, unsigned int &_bitdepth, bool &tRNS, bool &interlaced);

int def(const unsigned char *source, int len, unsigned char *dest, unsigned int &outlen, const bool testonly,
	const unsigned char c = 9, const unsigned char m = 8, const unsigned char s = 0, const unsigned char window = 15);
int inflen(std::basic_istringstream<unsigned char> &source, unsigned int &outlen);
void zerr(int ret);

size_t zopfliDeflate(const unsigned char *source, const size_t len, unsigned char **dest, const bool testonly, const unsigned char i = 1, const unsigned char bs = 0, const unsigned char bsmax = 0);

void make_crc_table();
unsigned long update_crc(unsigned long crc, unsigned char *buf, int len);

int applyfilter(const unsigned char filter, const unsigned int len, const unsigned int offset, unsigned char *dest, const unsigned char *src1, const unsigned char *src2, const int threshold = 0, const bool alreadyFiltered = false);

void findFiltering(const unsigned char *data, unsigned char *transmit, const unsigned int height, const unsigned int bpp, const unsigned int rowbyte, unsigned int WINDOW, const unsigned char level, const bool refine = false, const unsigned char s = 255);
unsigned int findSettings(const unsigned char *data, const unsigned int height, const unsigned int bpp, const unsigned int rowbyte, const unsigned char *filtered, const unsigned int len, unsigned char **dest, const unsigned int previousBest, unsigned char &bs, unsigned char &bsmax, unsigned char &filtering, const unsigned char level, const unsigned char s = 255);

void printChange(const unsigned int a, const unsigned int b);
bool fileExists(const char *name);
bool outputExists(const std::string &name);
