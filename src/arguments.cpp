#include <iostream>
#if defined(_OPENMP)
	#include <omp.h>
#endif
#include "settings.hpp"

#if defined(_OPENMP)
unsigned int MAX_THREADS = 1;
#endif

unsigned char LOG = 3;
unsigned int WINDOW = 500;
short I  = 6;
short F1 = 2;
short F2 = 0;
short S  = 2;
bool BM = false;
bool DRY = false;
bool KEEP = false;
bool MULTIIDAT = false;
bool OVERWRITE = false;

const unsigned int PRESETS[8][5] = {
	{   1, 3,0,0,0},
	{   1, 4,1,0,1},
	{   1, 5,1,0,2},
	{ 500, 6,2,0,2},
	{1000, 7,3,0,2},
	{1500, 8,3,2,2},
	{2000, 9,4,2,2},
	{2500,10,4,4,3},
};

void printInfo() {
	std::cout << VERSION << "\n\n" <<
		"Give input file as the last argument.\n"
		"\n"
		"Compression settings:\n"
		"-p0 -> -p6 settings preset. individual settings can be separately overridden. (default -p3 -w500 -i6 -f12 -f20 -s2)\n"
		"-w1 -> -w999... compression testing window in bytes. window is automatically increased to cover at least one scanline\n"
		"-i1 -> -i255 iteration count for the Zopfli compressor\n"
		"-f10 -> -f14 ; -f20 -> -f24 set the first or second filtering level\n"
		"-s0 -> -s3 set the level of tested filtering options and compression settings\n"
		"\n"
		"Minimum setting is at least level 1 for at least one of the filtering options (f11 or f21)\n"
		"This is automatically enforced if selected settings are less than that\n"
		"\n"
		"Other options:\n"
		"-b benchmark mode which is slightly faster and writes an arbitrary output file with correct length (forces -k and -o)\n"
		"-d dry run that will not write files. overrides -b setting\n"
		"-k keep the original file instead of overwriting. new file is named [name]_2.png\n"
		"-m run only if the input file has multiple IDAT chunks (PNG files tend to have multiple IDATs while PNG optimizers generate only one IDAT)\n"
		"-o allow overwriting the output file with -k (still doesn't write if the result is worse)\n"
		"\n"
		"Logging level:\n"
		"-l0 no output unless there are errors (errors are always printed)\n"
		"-l1 output one line once finished\n"
		"-l2 output one line at the beginning and once finished\n"
		"-l3 a lot of logging about progress and stats (default)\n"
	<< std::flush;
}

bool handleArguments(const int argc, char **argv) {
	if (argc == 1) {
		printInfo();
		return true;
	}

	for (unsigned char i = 1; i < argc - 1; i++) {
		if (argv[i][0] == '-' && argv[i][1] == 'p') {
			const short P = argv[i][2] - '0';
			if (P < 0 || P > 7) {
				std::cout << "ERROR: P is not 0-7" << std::endl;
				return true;
			}
			WINDOW = PRESETS[P][0];
			I  = PRESETS[P][1];
			F1 = PRESETS[P][2];
			F2 = PRESETS[P][3];
			S  = PRESETS[P][4];
		}
	}

	for (unsigned char i = 1; i < argc; i++) {
		if (argv[i][0] == '-' && (argv[i][1] == 'v' || (argv[i][1] == '-' && argv[i][2] == 'v'))) {
			std::cout << VERSION << std::endl;
			return true;
		}
		if (i == argc - 1) continue;
		if (argv[i][0] == '-') {
			switch (argv[i][1]) {
				case 'i': I = std::stoi(argv[i] + 2); break;
				case 'f':
					switch (argv[i][2]) {
						case '1': F1 = argv[i][3] - '0'; break;
						case '2': F2 = argv[i][3] - '0'; break;
						default: std::cout << "ERROR: Unknown argument " << argv[i] << std::endl; return true;
					} break;
				case 's': S = argv[i][2] - '0'; break;
				case 'w': WINDOW = std::stoi(argv[i] + 2); break;
				case 'l': LOG = argv[i][2] - '0'; break;
				case 'b': BM = true; break;
				case 'd': DRY = true; break;
				case 'k': KEEP = true; break;
				case 'm': MULTIIDAT = true; break;
				case 'o': OVERWRITE = true; break;
				case 'p': break;
				default: std::cout << "ERROR: Unknown argument " << argv[i] << std::endl; return true;
			}
		}
		else {
			std::cout << "ERROR: Unknown argument " << argv[i] << std::endl;
			return true;
		}
	}

	if (I < 1 || I > 255)  { std::cout << "ERROR: I is not 1-255" << std::endl; return true; }
	if (F1 < 0 || F1 > 4) { std::cout << "ERROR: F1 is not 0-4" << std::endl; return true; }
	if (F2 < 0 || F2 > 4) { std::cout << "ERROR: F2 is not 0-4" << std::endl; return true; }
	if (S < 0 || S > 3)   { std::cout << "ERROR: S is not 0-3"  << std::endl; return true; }

	if (!F1 || !F2) {
		F1 = std::max(F1, F2);
		F2 = 0;
		if (!F1) F1 = 1;
	}
	if (DRY) {
		BM = false;
		OVERWRITE = true;
	}
	else if (BM) {
		KEEP = true;
		OVERWRITE = true;
	}

	#if defined(_OPENMP)
		MAX_THREADS = omp_get_max_threads();
	#endif

	return false;
}

void printSettings(const int argc, char **argv) {
	#if defined(_OPENMP)
		if (LOG > 1) std::cout << "T=" << MAX_THREADS << "   ";
	#endif
	if (LOG > 1) {
		std::cout << "W=" << WINDOW << "   I=" << I << "   F1=" << F1 << "   F2=" << F2 << "   S=" << S;
		if (DRY) std::cout << "   DRY";
		else if (BM) std::cout << "   BENCHMARK";
		else {
			if (KEEP) std::cout << "   KEEP";
			if (OVERWRITE) std::cout << "   OVERWRITE";
		}
		std::cout << "   " << argv[argc - 1] << "\n";
	}
}
