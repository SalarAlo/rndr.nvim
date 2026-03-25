#include "rndr.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

std::optional<Rgb> parse_hex_color(const std::string& text) {
	if (text.size() != 6) {
		return std::nullopt;
	}

	auto hex_value = [](char ch) -> int {
		if (ch >= '0' && ch <= '9') {
			return ch - '0';
		}
		if (ch >= 'a' && ch <= 'f') {
			return 10 + (ch - 'a');
		}
		if (ch >= 'A' && ch <= 'F') {
			return 10 + (ch - 'A');
		}
		return -1;
	};

	const int r_hi = hex_value(text[0]);
	const int r_lo = hex_value(text[1]);
	const int g_hi = hex_value(text[2]);
	const int g_lo = hex_value(text[3]);
	const int b_hi = hex_value(text[4]);
	const int b_lo = hex_value(text[5]);
	if (r_hi < 0 || r_lo < 0 || g_hi < 0 || g_lo < 0 || b_hi < 0 || b_lo < 0) {
		return std::nullopt;
	}

	return Rgb {
		static_cast<float>((r_hi << 4) | r_lo) / 255.0f,
		static_cast<float>((g_hi << 4) | g_lo) / 255.0f,
		static_cast<float>((b_hi << 4) | b_lo) / 255.0f,
	};
}

bool is_finite(float value) {
	return std::isfinite(value);
}

float clamp01(float value) {
	if (!is_finite(value)) {
		return 0.0f;
	}

	return std::clamp(value, 0.0f, 1.0f);
}

bool is_finite(Vec3 value) {
	return is_finite(value.x) && is_finite(value.y) && is_finite(value.z);
}

bool is_finite(Rgb value) {
	return is_finite(value.r) && is_finite(value.g) && is_finite(value.b);
}

Vec3 add(Vec3 a, Vec3 b) {
	return { a.x + b.x, a.y + b.y, a.z + b.z };
}

Vec3 subtract(Vec3 a, Vec3 b) {
	return { a.x - b.x, a.y - b.y, a.z - b.z };
}

Vec3 multiply(Vec3 value, float scalar) {
	return { value.x * scalar, value.y * scalar, value.z * scalar };
}

float dot(Vec3 a, Vec3 b) {
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 cross(Vec3 a, Vec3 b) {
	return {
		a.y * b.z - a.z * b.y,
		a.z * b.x - a.x * b.z,
		a.x * b.y - a.y * b.x,
	};
}

float length(Vec3 value) {
	if (!is_finite(value)) {
		return 0.0f;
	}

	return std::sqrt(dot(value, value));
}

Vec3 normalize(Vec3 value) {
	const float len = length(value);
	if (!is_finite(len) || len <= 1e-6f) {
		return { 0.0f, 0.0f, 1.0f };
	}

	return multiply(value, 1.0f / len);
}

Vec3 rotate_x(Vec3 value, float radians) {
	const float s = std::sin(radians);
	const float c = std::cos(radians);
	return {
		value.x,
		value.y * c - value.z * s,
		value.y * s + value.z * c,
	};
}

Vec3 rotate_y(Vec3 value, float radians) {
	const float s = std::sin(radians);
	const float c = std::cos(radians);
	return {
		value.x * c + value.z * s,
		value.y,
		-value.x * s + value.z * c,
	};
}

Rgb multiply(Rgb color, float scalar) {
	return {
		color.r * scalar,
		color.g * scalar,
		color.b * scalar,
	};
}

Rgb add(Rgb a, Rgb b) {
	return {
		a.r + b.r,
		a.g + b.g,
		a.b + b.b,
	};
}

Rgb multiply(Rgb a, Rgb b) {
	return {
		a.r * b.r,
		a.g * b.g,
		a.b * b.b,
	};
}

Rgb lerp(Rgb from, Rgb to, float alpha) {
	return add(multiply(from, 1.0f - alpha), multiply(to, alpha));
}

Rgb enhance(Rgb color, const ToneSettings& tone) {
	if (!is_finite(color)) {
		return { 0.0f, 0.0f, 0.0f };
	}

	const float luma = 0.2126f * color.r + 0.7152f * color.g + 0.0722f * color.b;

	color.r = luma + (color.r - luma) * tone.saturation;
	color.g = luma + (color.g - luma) * tone.saturation;
	color.b = luma + (color.b - luma) * tone.saturation;

	color = multiply(color, tone.brightness);

	color.r = (color.r - 0.5f) * tone.contrast + 0.5f;
	color.g = (color.g - 0.5f) * tone.contrast + 0.5f;
	color.b = (color.b - 0.5f) * tone.contrast + 0.5f;

	color.r = std::pow(clamp01(color.r), tone.gamma);
	color.g = std::pow(clamp01(color.g), tone.gamma);
	color.b = std::pow(clamp01(color.b), tone.gamma);

	return {
		clamp01(color.r),
		clamp01(color.g),
		clamp01(color.b),
	};
}

void append_hex(std::string& output, Rgb color) {
	static constexpr char hex_digits[] = "0123456789abcdef";
	const int r = static_cast<int>(std::round(clamp01(color.r) * 255.0f));
	const int g = static_cast<int>(std::round(clamp01(color.g) * 255.0f));
	const int b = static_cast<int>(std::round(clamp01(color.b) * 255.0f));

	output.push_back(hex_digits[(r >> 4) & 0xF]);
	output.push_back(hex_digits[r & 0xF]);
	output.push_back(hex_digits[(g >> 4) & 0xF]);
	output.push_back(hex_digits[g & 0xF]);
	output.push_back(hex_digits[(b >> 4) & 0xF]);
	output.push_back(hex_digits[b & 0xF]);
}

std::optional<int> parse_positive_int(const char* text) {
	try {
		const int value = std::stoi(text);
		if (value > 0) {
			return value;
		}
	} catch (...) {
	}

	return std::nullopt;
}

std::optional<float> parse_float_arg(const char* text) {
	try {
		return std::stof(text);
	} catch (...) {
		return std::nullopt;
	}
}

std::vector<std::string_view> split_fields(std::string_view line, char delimiter) {
	std::vector<std::string_view> fields;
	std::size_t start = 0;

	while (start <= line.size()) {
		const std::size_t end = line.find(delimiter, start);
		if (end == std::string_view::npos) {
			fields.push_back(line.substr(start));
			break;
		}
		fields.push_back(line.substr(start, end - start));
		start = end + 1;
	}

	return fields;
}

std::optional<int> parse_positive_int(std::string_view text) {
	try {
		const int value = std::stoi(std::string(text));
		if (value > 0) {
			return value;
		}
	} catch (...) {
	}

	return std::nullopt;
}

std::optional<float> parse_float_arg(std::string_view text) {
	try {
		return std::stof(std::string(text));
	} catch (...) {
		return std::nullopt;
	}
}

std::optional<RenderRequest> parse_render_request(const std::string& line) {
	const std::vector<std::string_view> fields = split_fields(line, '\t');
	if (fields.size() != 11) {
		return std::nullopt;
	}

	const auto max_term_w = parse_positive_int(fields[1]);
	const auto max_term_h = parse_positive_int(fields[2]);
	const auto supersample = parse_positive_int(fields[3]);
	const auto yaw = parse_float_arg(fields[4]);
	const auto pitch = parse_float_arg(fields[5]);
	const auto brightness = parse_float_arg(fields[6]);
	const auto saturation = parse_float_arg(fields[7]);
	const auto contrast = parse_float_arg(fields[8]);
	const auto gamma = parse_float_arg(fields[9]);
	const auto background = parse_hex_color(std::string(fields[10]));

	if (!max_term_w || !max_term_h || !supersample || !yaw || !pitch || !brightness || !saturation || !contrast || !gamma
	    || !background || *brightness < 0.0f || *saturation < 0.0f || *contrast < 0.0f || *gamma <= 0.0f) {
		return std::nullopt;
	}

	return RenderRequest {
		std::string(fields[0]),
		*max_term_w,
		*max_term_h,
		*supersample,
		*yaw,
		*pitch,
		ToneSettings { *brightness, *saturation, *contrast, *gamma },
		*background,
	};
}

bool has_suffix(const std::string& value, const std::string& suffix) {
	if (value.size() < suffix.size()) {
		return false;
	}

	return value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string lowercase(std::string value) {
	std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
		return static_cast<char>(std::tolower(ch));
	});
	return value;
}
