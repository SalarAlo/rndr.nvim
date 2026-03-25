#include <cmath>

#include <iostream>
#include <limits>

#include "rndr.hpp"

namespace {

Vec3 rotate_position(Vec3 position, float yaw_radians, float pitch_radians) {
	return rotate_x(rotate_y(position, yaw_radians), pitch_radians);
}

Vec3 rotate_normal(Vec3 normal, float yaw_radians, float pitch_radians) {
	return normalize(rotate_x(rotate_y(normal, yaw_radians), pitch_radians));
}

float edge_function(Vec2 a, Vec2 b, Vec2 p) {
	return (p.x - a.x) * (b.y - a.y) - (p.y - a.y) * (b.x - a.x);
}

}

void emit_terminal_frame(const std::vector<Rgb>& pixels, int width, int height, const ToneSettings& tone) {
	const int term_h = std::max(1, height / 2);
	const std::string glyph = "▀";
	std::string text_row;
	text_row.reserve(static_cast<std::size_t>(width) * glyph.size());
	for (int x = 0; x < width; ++x) {
		text_row += glyph;
	}

	std::string frame;
	frame.reserve(static_cast<std::size_t>(term_h) * static_cast<std::size_t>(width) * 18 + 32);
	frame += "<FRAME>\n";

	for (int y = 0; y < term_h; ++y) {
		std::string fg_colors;
		std::string bg_colors;
		fg_colors.reserve(static_cast<std::size_t>(width) * 6);
		bg_colors.reserve(static_cast<std::size_t>(width) * 6);

		for (int x = 0; x < width; ++x) {
			const int top_y = std::min(height - 1, y * 2);
			const int bottom_y = std::min(height - 1, y * 2 + 1);
			const Rgb top = enhance(pixels[static_cast<std::size_t>(top_y * width + x)], tone);
			const Rgb bottom = enhance(pixels[static_cast<std::size_t>(bottom_y * width + x)], tone);
			append_hex(fg_colors, top);
			append_hex(bg_colors, bottom);
		}

		frame += "<TEXT>";
		frame += text_row;
		frame += '\n';
		frame += "<FG>";
		frame += fg_colors;
		frame += '\n';
		frame += "<BG>";
		frame += bg_colors;
		frame += '\n';
	}

	frame += "<END>\n";
	std::cout << frame;
}

std::vector<Rgb> downsample_pixels(const std::vector<Rgb>& source, int src_w, int src_h, int dst_w, int dst_h) {
	std::vector<Rgb> output(static_cast<std::size_t>(dst_w * dst_h), { 0.0f, 0.0f, 0.0f });

	for (int y = 0; y < dst_h; ++y) {
		const int src_y0 = (y * src_h) / dst_h;
		const int src_y1 = std::max(src_y0 + 1, ((y + 1) * src_h) / dst_h);

		for (int x = 0; x < dst_w; ++x) {
			const int src_x0 = (x * src_w) / dst_w;
			const int src_x1 = std::max(src_x0 + 1, ((x + 1) * src_w) / dst_w);
			Rgb sum { 0.0f, 0.0f, 0.0f };
			int count = 0;

			for (int sy = src_y0; sy < src_y1; ++sy) {
				for (int sx = src_x0; sx < src_x1; ++sx) {
					sum = add(sum, source[static_cast<std::size_t>(sy * src_w + sx)]);
					++count;
				}
			}

			output[static_cast<std::size_t>(y * dst_w + x)] = count == 0
			    ? Rgb { 0.0f, 0.0f, 0.0f }
			    : multiply(sum, 1.0f / static_cast<float>(count));
		}
	}

	return output;
}

std::vector<Rgb> rasterize_model(const ModelData& model, int width, int height, float yaw_degrees, float pitch_degrees, Rgb clear_color) {
	const int pixel_count = width * height;
	std::vector<Rgb> color_buffer(static_cast<std::size_t>(pixel_count), clear_color);
	std::vector<float> depth_buffer(static_cast<std::size_t>(pixel_count), std::numeric_limits<float>::infinity());

	const float yaw = yaw_degrees * 3.1415926535f / 180.0f;
	const float pitch = pitch_degrees * 3.1415926535f / 180.0f;
	const Vec3 light_dir = normalize({ 0.45f, 0.55f, 1.0f });
	const float focal = static_cast<float>(std::min(width, height)) * 0.95f;
	const float camera_distance = 3.2f;
	const float near_plane = 0.1f;

	for (const Triangle& source : model.triangles) {
		Vertex transformed[3] = { source.a, source.b, source.c };
		Vec2 screen[3];
		float depth[3];
		bool visible = true;

		for (int i = 0; i < 3; ++i) {
			transformed[i].position = rotate_position(transformed[i].position, yaw, pitch);
			transformed[i].normal = rotate_normal(transformed[i].normal, yaw, pitch);
			if (!is_finite(transformed[i].position) || !is_finite(transformed[i].normal)) {
				visible = false;
				break;
			}

			const float view_z = transformed[i].position.z + camera_distance;
			if (!is_finite(view_z) || view_z <= near_plane) {
				visible = false;
				break;
			}

			depth[i] = view_z;
			screen[i].x = transformed[i].position.x * (focal / view_z) + static_cast<float>(width) * 0.5f;
			screen[i].y = -transformed[i].position.y * (focal / view_z) + static_cast<float>(height) * 0.5f;
			if (!is_finite(screen[i].x) || !is_finite(screen[i].y)) {
				visible = false;
				break;
			}
		}

		if (!visible) {
			continue;
		}

		const float area = edge_function(screen[0], screen[1], screen[2]);
		if (!is_finite(area) || std::abs(area) <= 1e-5f) {
			continue;
		}

		const float min_x = std::floor(std::min({ screen[0].x, screen[1].x, screen[2].x }));
		const float max_x = std::ceil(std::max({ screen[0].x, screen[1].x, screen[2].x }));
		const float min_y = std::floor(std::min({ screen[0].y, screen[1].y, screen[2].y }));
		const float max_y = std::ceil(std::max({ screen[0].y, screen[1].y, screen[2].y }));

		const int x0 = std::max(0, static_cast<int>(min_x));
		const int x1 = std::min(width - 1, static_cast<int>(max_x));
		const int y0 = std::max(0, static_cast<int>(min_y));
		const int y1 = std::min(height - 1, static_cast<int>(max_y));

		for (int y = y0; y <= y1; ++y) {
			for (int x = x0; x <= x1; ++x) {
				const Vec2 p { static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f };
				const float w0 = edge_function(screen[1], screen[2], p) / area;
				const float w1 = edge_function(screen[2], screen[0], p) / area;
				const float w2 = edge_function(screen[0], screen[1], p) / area;

				if (!is_finite(w0) || !is_finite(w1) || !is_finite(w2) || w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) {
					continue;
				}

				const float pixel_depth = depth[0] * w0 + depth[1] * w1 + depth[2] * w2;
				const std::size_t index = static_cast<std::size_t>(y * width + x);
				if (!is_finite(pixel_depth) || pixel_depth >= depth_buffer[index]) {
					continue;
				}

				const float inv_depth0 = 1.0f / depth[0];
				const float inv_depth1 = 1.0f / depth[1];
				const float inv_depth2 = 1.0f / depth[2];
				const float perspective_w0 = w0 * inv_depth0;
				const float perspective_w1 = w1 * inv_depth1;
				const float perspective_w2 = w2 * inv_depth2;
				const float perspective_sum = perspective_w0 + perspective_w1 + perspective_w2;
				if (!is_finite(perspective_sum) || perspective_sum <= 1e-6f) {
					continue;
				}

				const float pw0 = perspective_w0 / perspective_sum;
				const float pw1 = perspective_w1 / perspective_sum;
				const float pw2 = perspective_w2 / perspective_sum;
				if (!is_finite(pw0) || !is_finite(pw1) || !is_finite(pw2)) {
					continue;
				}

				Vec3 interpolated_normal = normalize(add(
				    add(multiply(transformed[0].normal, pw0), multiply(transformed[1].normal, pw1)),
				    multiply(transformed[2].normal, pw2)));
				const Vec3 interpolated_position = add(
				    add(multiply(transformed[0].position, pw0), multiply(transformed[1].position, pw1)),
				    multiply(transformed[2].position, pw2));
				if (!is_finite(interpolated_normal) || !is_finite(interpolated_position)) {
					continue;
				}

				const Vec3 view_dir = normalize({
				    -interpolated_position.x,
				    -interpolated_position.y,
				    -(interpolated_position.z + camera_distance),
				});
				if (dot(interpolated_normal, view_dir) < 0.0f) {
					interpolated_normal = multiply(interpolated_normal, -1.0f);
				}

				const float diffuse = std::max(0.0f, dot(interpolated_normal, light_dir));
				const float ambient = 0.65f;
				const float depth_falloff = clamp01(1.15f - (pixel_depth - near_plane) / (camera_distance + 1.5f));
				const float shade = ambient + diffuse * 0.95f;
				const Rgb vertex_color = add(
				    add(multiply(source.a.color, pw0), multiply(source.b.color, pw1)),
				    multiply(source.c.color, pw2));
				Rgb albedo = multiply(source.material_color, vertex_color);
				if (!is_finite(albedo)) {
					continue;
				}

				if (source.has_uv && source.texture_index >= 0
				    && source.texture_index < static_cast<int>(model.textures.size())) {
					const Vec2 uv {
						source.a.uv.x * pw0 + source.b.uv.x * pw1 + source.c.uv.x * pw2,
						source.a.uv.y * pw0 + source.b.uv.y * pw1 + source.c.uv.y * pw2,
					};
					const SampledPixel texture_sample = sample_texture(model.textures[static_cast<std::size_t>(source.texture_index)], uv);
					if (texture_sample.alpha <= 1e-4f) {
						continue;
					}

					albedo = multiply(albedo, texture_sample.color);
					if (!is_finite(albedo)) {
						continue;
					}

					const Rgb shaded = multiply(albedo, shade * depth_falloff);
					if (!is_finite(shaded)) {
						continue;
					}

					color_buffer[index] = lerp(color_buffer[index], shaded, texture_sample.alpha);
					depth_buffer[index] = pixel_depth;
					continue;
				}

				const Rgb shaded = multiply(albedo, shade * depth_falloff);
				if (!is_finite(shaded)) {
					continue;
				}
				color_buffer[index] = shaded;
				depth_buffer[index] = pixel_depth;
			}
		}
	}

	return color_buffer;
}

void render_image(const ImageData& image, int max_term_w, int max_term_h, int supersample, Rgb clear_color, const ToneSettings& tone) {
	const float aspect = static_cast<float>(image.height) / static_cast<float>(image.width);
	int term_w = max_term_w;
	int term_h = std::max(1, static_cast<int>(std::round(term_w * aspect * 0.5f)));

	if (term_h > max_term_h) {
		term_h = max_term_h;
		term_w = std::max(1, static_cast<int>(std::round((term_h * 2.0f) / aspect)));
		term_w = std::min(term_w, max_term_w);
	}

	std::vector<Rgb> pixels(static_cast<std::size_t>(term_w * term_h * 2));
	for (int y = 0; y < term_h * 2; ++y) {
		for (int x = 0; x < term_w; ++x) {
			const float x0 = static_cast<float>(x) * static_cast<float>(image.width) / static_cast<float>(term_w);
			const float x1 = std::max(x0 + (1.0f / static_cast<float>(supersample)), static_cast<float>(x + 1) * static_cast<float>(image.width) / static_cast<float>(term_w));
			const float y0 = static_cast<float>(y) * static_cast<float>(image.height) / static_cast<float>(term_h * 2);
			const float y1 = std::max(y0 + (1.0f / static_cast<float>(supersample)), static_cast<float>(y + 1) * static_cast<float>(image.height) / static_cast<float>(term_h * 2));
			const SampledPixel sample = sample_image_supersampled(image, x0, x1, y0, y1, supersample);
			pixels[static_cast<std::size_t>(y * term_w + x)] = lerp(clear_color, sample.color, sample.alpha);
		}
	}

	emit_terminal_frame(pixels, term_w, term_h * 2, tone);
}

void render_model(const ModelData& model, int max_term_w, int max_term_h, int supersample, float yaw_degrees, float pitch_degrees, Rgb clear_color, const ToneSettings& tone) {
	const int target_w = std::max(1, max_term_w);
	const int target_h = std::max(1, max_term_h * 2);
	const int render_w = std::max(1, target_w * std::max(1, supersample));
	const int render_h = std::max(2, target_h * std::max(1, supersample));

	const std::vector<Rgb> color_buffer = rasterize_model(model, render_w, render_h, yaw_degrees, pitch_degrees, clear_color);
	const std::vector<Rgb> pixels = supersample == 1
	    ? color_buffer
	    : downsample_pixels(color_buffer, render_w, render_h, target_w, target_h);

	emit_terminal_frame(pixels, target_w, target_h, tone);
}

int render_request(const RenderRequest& request, CachedAsset& cache) {
	std::string error;
	if (!load_cached_asset(cache, request.file_path, &error)) {
		std::cerr << (error.empty() ? "Failed to load file as image or model" : error) << '\n';
		return 1;
	}

	if (cache.kind == CachedAssetKind::model) {
		render_model(
		    cache.model,
		    request.max_term_w,
		    request.max_term_h,
		    request.supersample,
		    request.yaw,
		    request.pitch,
		    request.background,
		    request.tone);
		return 0;
	}

	if (cache.kind == CachedAssetKind::image) {
		render_image(
		    cache.image,
		    request.max_term_w,
		    request.max_term_h,
		    request.supersample,
		    request.background,
		    request.tone);
		return 0;
	}

	std::cerr << "Renderer cache ended in an invalid state\n";
	return 1;
}
