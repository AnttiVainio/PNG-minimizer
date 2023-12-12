#include <iostream>
#include <iomanip>
#include <fstream>
#include "settings.hpp"

void printChange(const unsigned int a, const unsigned int b) {
	if (LOG && a) {
		if (a != b) {
			if (LOG > 2) std::cout << (a / 1024) << " -> " << (b / 1024) << "   " << (int(10000.0 * float(b) / float(a)) / 100.0);
			else std::cout << std::fixed << std::setprecision(2) << std::setw(7) << (int(10000.0 * float(b) / float(a)) / 100.0) << "  " << (a / 1024) << " -> " << (b / 1024);
		}
		else std::cout << "No change";
		if (LOG > 2) std::cout << std::endl;
	}
}

bool fileExists(const char *name) {
	std::ifstream f(name);
	return f.good();
}

bool outputExists(const std::string &name) {
	if (OVERWRITE) return false;
	if (fileExists(name.c_str())) {
		std::cout << "ERROR: Output file " << name << " already exist" << std::endl;
		return true;
	}
	return false;
}
