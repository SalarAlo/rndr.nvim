#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct Rgb {
	float r;
	float g;
	float b;
};

struct Vec2 {
	float x;
	float y;
};

struct Vec3 {
	float x;
	float y;
	float z;
};

struct Vertex {
	Vec3 position;
	Vec3 normal;
	Vec2 uv;
	Rgb color;
};

struct Triangle {
	Vertex a;
	Vertex b;
	Vertex c;
	Rgb material_color;
	int texture_index = -1;
	unsigned int uv_channel = 0;
	bool has_uv = false;
};

struct ImageData {
	int width;
	int height;
	std::vector<Rgb> pixels;
	std::vector<float> alpha;
};

struct SampledPixel {
	Rgb color;
	float alpha;
};

struct MaterialTexture {
	ImageData image;
	unsigned int uv_channel = 0;
};

struct ModelData {
	std::vector<Triangle> triangles;
	std::vector<ImageData> textures;
	std::vector<unsigned int> texture_uv_channels;
};

struct Bounds {
	Vec3 min;
	Vec3 max;
};

struct ToneSettings {
	float brightness;
	float saturation;
	float contrast;
	float gamma;
};

struct RenderRequest {
	std::string file_path;
	int max_term_w;
	int max_term_h;
	int supersample;
	float yaw;
	float pitch;
	ToneSettings tone;
	Rgb background;
};

enum class CachedAssetKind {
	none,
	image,
	model,
};

struct CachedAsset {
	CachedAssetKind kind = CachedAssetKind::none;
	std::string file_path;
	ImageData image;
	ModelData model;
};

std::optional<Rgb> parse_hex_color(const std::string& text);
bool is_finite(float value);
float clamp01(float value);
bool is_finite(Vec3 value);
bool is_finite(Rgb value);
Vec3 add(Vec3 a, Vec3 b);
Vec3 subtract(Vec3 a, Vec3 b);
Vec3 multiply(Vec3 value, float scalar);
float dot(Vec3 a, Vec3 b);
Vec3 cross(Vec3 a, Vec3 b);
float length(Vec3 value);
Vec3 normalize(Vec3 value);
Vec3 rotate_x(Vec3 value, float radians);
Vec3 rotate_y(Vec3 value, float radians);
Rgb multiply(Rgb color, float scalar);
Rgb add(Rgb a, Rgb b);
Rgb multiply(Rgb a, Rgb b);
Rgb lerp(Rgb from, Rgb to, float alpha);
Rgb enhance(Rgb color, const ToneSettings& tone);
void append_hex(std::string& output, Rgb color);
std::optional<int> parse_positive_int(const char* text);
std::optional<float> parse_float_arg(const char* text);
std::vector<std::string_view> split_fields(std::string_view line, char delimiter);
std::optional<int> parse_positive_int(std::string_view text);
std::optional<float> parse_float_arg(std::string_view text);
std::optional<RenderRequest> parse_render_request(const std::string& line);
bool has_suffix(const std::string& value, const std::string& suffix);
std::string lowercase(std::string value);

SampledPixel sample_image_region(const ImageData& image, int x0, int x1, int y0, int y1);
SampledPixel sample_image_supersampled(const ImageData& image, float x0, float x1, float y0, float y1, int supersample);
std::optional<ImageData> load_image(const char* file_path, std::string* error = nullptr);
std::optional<ImageData> load_image_memory(const unsigned char* bytes, int length, std::string* error = nullptr);
std::optional<ModelData> load_model(const char* file_path, std::string* error = nullptr);
SampledPixel sample_texture(const ImageData& image, Vec2 uv);
bool load_cached_asset(CachedAsset& cache, const std::string& file_path, std::string* error = nullptr);

void emit_terminal_frame(const std::vector<Rgb>& pixels, int width, int height, const ToneSettings& tone);
std::vector<Rgb> downsample_pixels(const std::vector<Rgb>& source, int src_w, int src_h, int dst_w, int dst_h);
std::vector<Rgb> rasterize_model(const ModelData& model, int width, int height, float yaw_degrees, float pitch_degrees, Rgb clear_color);
void render_image(const ImageData& image, int max_term_w, int max_term_h, int supersample, Rgb clear_color, const ToneSettings& tone);
void render_model(const ModelData& model, int max_term_w, int max_term_h, int supersample, float yaw_degrees, float pitch_degrees, Rgb clear_color, const ToneSettings& tone);
int render_request(const RenderRequest& request, CachedAsset& cache);
