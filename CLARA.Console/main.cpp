#include "stdafx.h"
#include <CLARA/Compiler.h>

std::vector<std::string> inputPaths;
std::string outputPath;

int main(int argc, char* argv[]) {
	/*if (argc < 2) {
		std::cout << "syntax: " << argv[0] << " <input...> <output>";
		return 1;
	}
	for (int i = 1; i < argc; ++i) {
		if (i == (argc - 1)) outputPath = argv[i];
		else inputPaths.push_back(argv[i]);
	}*/

	const char* pathin = argv[1], *pathout;
	std::string spathout;
	if (argc < 3) {
		spathout = pathin;
		auto pos = spathout.find_last_of('.');
		if (pos == spathout.npos) {
			std::cout << "syntax: " << argv[0] << " <input_path> <output_path>";
			return 1;
		}

		spathout = spathout.substr(0, pos);
		spathout += ".clo";

		pathout = spathout.c_str();
	}
	else pathout = argv[2];

	CLARA::SetErrorHandler([](CLARA::CLARA_ERROR code, const char* error) {
		std::cerr << error << std::endl;
		return true;
	});
	CLARA::SetOutputHandler([](const char * msg) {
		std::cout << msg << std::endl;
		return true;
	});
	bool b = CLARA::Compile(pathin, pathout) == CLARA::CLARA_ERROR_NONE;
	ASSERT(b);
	return 0;
}