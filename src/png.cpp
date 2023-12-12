#include <png.h>
#include <iostream>
#include <fstream>

//#define DEBUG

#define errorlog std::cout

void userReadData(png_structp pngPtr, png_bytep data, png_size_t length) {
	png_voidp a = png_get_io_ptr(pngPtr);
	((std::istream*)a)->read((char*)data, length);
}

#define PNGSIGSIZE 8

unsigned char *loadPNG(const char *filename, unsigned int &width, unsigned int &height, unsigned int &_channels, unsigned int &rowbyte, unsigned int &_bitdepth, bool &tRNS, bool &interlaced) {
	#ifdef DEBUG
	std::cout << "LIBPNG VERSION: " << PNG_LIBPNG_VER_STRING << "   LOAD " << filename << std::endl;
	#endif

	std::ifstream file(filename, std::ifstream::in | std::ifstream::binary);
	if (file.bad() || !file.is_open()) {
		errorlog << "Could not load image " << filename << std::endl;
		file.close();
		return 0;
	}

	png_byte pngsig[PNGSIGSIZE];
	file.read((char*)pngsig, PNGSIGSIZE);
	if (png_sig_cmp(pngsig, 0, PNGSIGSIZE) != 0) {
		errorlog << "Not a valid PNG " << filename << std::endl;
		file.close();
		return 0;
	}

	png_structp pngPtr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!pngPtr) {
		errorlog << "Could not initialize png read struct" << std::endl;
		file.close();
		return 0;
	}
	png_infop infoPtr = png_create_info_struct(pngPtr);
	if (!infoPtr) {
		errorlog << "Could not initialize png info struct" << std::endl;
		png_destroy_read_struct(&pngPtr, (png_infopp)0, (png_infopp)0);
		file.close();
		return 0;
	}
	if (setjmp(png_jmpbuf(pngPtr))) {
		png_destroy_read_struct(&pngPtr, &infoPtr,(png_infopp)0);
		errorlog << "Error while reading PNG file " << filename << std::endl;
		file.close();
		return 0;
	}

	png_set_read_fn(pngPtr,(png_voidp)&file, userReadData);

	png_set_sig_bytes(pngPtr, PNGSIGSIZE);
	png_read_info(pngPtr, infoPtr);

	png_uint_32 imgWidth = png_get_image_width(pngPtr, infoPtr);
	png_uint_32 imgHeight = png_get_image_height(pngPtr, infoPtr);
	png_uint_32 bitdepth = png_get_bit_depth(pngPtr, infoPtr);
	png_uint_32 channels = png_get_channels(pngPtr, infoPtr);
	rowbyte = png_get_rowbytes(pngPtr, infoPtr);
	_bitdepth = bitdepth;
	tRNS = png_get_valid(pngPtr, infoPtr, PNG_INFO_tRNS);
	interlaced = png_get_interlace_type(pngPtr, infoPtr);

	#ifdef DEBUG
	std::cout << "Channels:   " << channels << std::endl;
	std::cout << "Bit depth:  " << bitdepth << std::endl;
	#endif

	png_bytep* rowPtrs = new png_bytep[imgHeight];
	char unsigned* data = new unsigned char[rowbyte * imgHeight]();
	for (size_t i = 0; i < imgHeight; i++) {
		png_uint_32 q = (imgHeight - i - 1) * rowbyte;
		rowPtrs[i] = (png_bytep)data + q;
	}
	png_read_image(pngPtr, rowPtrs);

	delete[] (png_bytep)rowPtrs;
	png_destroy_read_struct(&pngPtr, &infoPtr,(png_infopp)0);

	width = imgWidth;
	height = imgHeight;
	_channels = channels;

	return data;
}
