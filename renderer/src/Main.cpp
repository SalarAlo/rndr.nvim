#include <iostream>
#include <optional>
#include <string>

#include "rndr.hpp"

namespace {

int run_stdio_mode() {
	CachedAsset cache;
	std::string line;

	while (std::getline(std::cin, line)) {
		if (line.empty()) {
			continue;
		}

		const auto request = parse_render_request(line);
		if (!request) {
			std::cerr << "Invalid render arguments\n";
			std::cout << "<DONE>\n";
			std::cout.flush();
			continue;
		}

		render_request(*request, cache);
		std::cout << "<DONE>\n";
		std::cout.flush();
	}

	return 0;
}

}

int main(int argc, char** argv) {
	if (argc == 2 && std::string(argv[1]) == "--stdio") {
		return run_stdio_mode();
	}

	if (argc < 4 || argc > 12) {
		std::cerr << "Usage: rndr <file> <term-width> <term-height> [supersample] [yaw] [pitch] [brightness] [saturation] [contrast] [gamma] [background]\n";
		return 1;
	}

	const char* file_path = argv[1];
	const auto max_term_w = parse_positive_int(argv[2]);
	const auto max_term_h = parse_positive_int(argv[3]);
	const auto supersample = argc >= 5 ? parse_positive_int(argv[4]) : std::optional<int> { 1 };
	const auto yaw = argc >= 6 ? parse_float_arg(argv[5]) : std::optional<float> { 0.0f };
	const auto pitch = argc >= 7 ? parse_float_arg(argv[6]) : std::optional<float> { 0.0f };
	const auto brightness = argc >= 8 ? parse_float_arg(argv[7]) : std::optional<float> { 1.0f };
	const auto saturation = argc >= 9 ? parse_float_arg(argv[8]) : std::optional<float> { 1.18f };
	const auto contrast = argc >= 10 ? parse_float_arg(argv[9]) : std::optional<float> { 1.08f };
	const auto gamma = argc >= 11 ? parse_float_arg(argv[10]) : std::optional<float> { 0.92f };
	const std::string background_arg = argc >= 12 ? argv[11] : "0d0f14";
	const auto background = parse_hex_color(background_arg);

	if (!max_term_w || !max_term_h || !supersample || !yaw || !pitch || !brightness || !saturation || !contrast || !gamma
	    || !background || *brightness < 0.0f || *saturation < 0.0f || *contrast < 0.0f || *gamma <= 0.0f) {
		std::cerr << "Invalid render arguments\n";
		return 1;
	}

	CachedAsset cache;
	return render_request(RenderRequest {
	                          file_path,
	                          *max_term_w,
	                          *max_term_h,
	                          *supersample,
	                          *yaw,
	                          *pitch,
	                          ToneSettings { *brightness, *saturation, *contrast, *gamma },
	                          *background,
	                      },
	    cache);
}
