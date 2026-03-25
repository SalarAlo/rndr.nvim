#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <filesystem>
#include <limits>
#include <sstream>
#include <unordered_map>

#include "rndr.hpp"
#include "stb_image.h"

SampledPixel sample_image_region(const ImageData& image, int x0, int x1, int y0, int y1) {
	x0 = std::clamp(x0, 0, image.width);
	x1 = std::clamp(x1, 0, image.width);
	y0 = std::clamp(y0, 0, image.height);
	y1 = std::clamp(y1, 0, image.height);

	if (x0 >= x1 || y0 >= y1) {
		return { { 0.0f, 0.0f, 0.0f }, 0.0f };
	}

	Rgb sum { 0.0f, 0.0f, 0.0f };
	float alpha_sum = 0.0f;
	int count = 0;

	for (int y = y0; y < y1; ++y) {
		for (int x = x0; x < x1; ++x) {
			const std::size_t index = static_cast<std::size_t>(y * image.width + x);
			const Rgb pixel = image.pixels[index];
			const float alpha = image.alpha.empty() ? 1.0f : image.alpha[index];
			sum = add(sum, multiply(pixel, alpha));
			alpha_sum += alpha;
			++count;
		}
	}

	if (count == 0) {
		return { { 0.0f, 0.0f, 0.0f }, 0.0f };
	}

	const float average_alpha = alpha_sum / static_cast<float>(count);
	if (alpha_sum <= 1e-6f) {
		return { { 0.0f, 0.0f, 0.0f }, average_alpha };
	}

	return {
		multiply(sum, 1.0f / alpha_sum),
		average_alpha,
	};
}

SampledPixel sample_image_supersampled(const ImageData& image, float x0, float x1, float y0, float y1, int supersample) {
	Rgb sum { 0.0f, 0.0f, 0.0f };
	float alpha_sum = 0.0f;
	const int sample_count = std::max(1, supersample);

	for (int sy = 0; sy < sample_count; ++sy) {
		const float sample_y0 = y0 + (y1 - y0) * static_cast<float>(sy) / static_cast<float>(sample_count);
		const float sample_y1 = y0 + (y1 - y0) * static_cast<float>(sy + 1) / static_cast<float>(sample_count);

		for (int sx = 0; sx < sample_count; ++sx) {
			const float sample_x0 = x0 + (x1 - x0) * static_cast<float>(sx) / static_cast<float>(sample_count);
			const float sample_x1 = x0 + (x1 - x0) * static_cast<float>(sx + 1) / static_cast<float>(sample_count);
			const SampledPixel sample = sample_image_region(
			    image,
			    static_cast<int>(std::floor(sample_x0)),
			    static_cast<int>(std::ceil(sample_x1)),
			    static_cast<int>(std::floor(sample_y0)),
			    static_cast<int>(std::ceil(sample_y1)));
			sum = add(sum, multiply(sample.color, sample.alpha));
			alpha_sum += sample.alpha;
		}
	}

	const float inv_count = 1.0f / static_cast<float>(sample_count * sample_count);
	const float average_alpha = alpha_sum * inv_count;
	if (alpha_sum <= 1e-6f) {
		return { { 0.0f, 0.0f, 0.0f }, average_alpha };
	}

	return {
		multiply(sum, 1.0f / alpha_sum),
		average_alpha,
	};
}

std::optional<ImageData> load_image(const char* file_path, std::string* error) {
	int width = 0;
	int height = 0;
	int channels = 0;
	unsigned char* image = stbi_load(file_path, &width, &height, &channels, 4);
	if (!image) {
		if (error) {
			const char* reason = stbi_failure_reason();
			*error = reason ? reason : "stb_image could not decode the file";
		}
		return std::nullopt;
	}

	ImageData data;
	data.width = width;
	data.height = height;
	data.pixels.reserve(static_cast<std::size_t>(width * height));
	data.alpha.reserve(static_cast<std::size_t>(width * height));

	for (int index = 0; index < width * height; ++index) {
		const int base = index * 4;
		data.pixels.push_back({
		    static_cast<float>(image[base]) / 255.0f,
		    static_cast<float>(image[base + 1]) / 255.0f,
		    static_cast<float>(image[base + 2]) / 255.0f,
		});
		data.alpha.push_back(static_cast<float>(image[base + 3]) / 255.0f);
	}

	stbi_image_free(image);
	return data;
}

std::optional<ImageData> load_image_memory(const unsigned char* bytes, int length, std::string* error) {
	int width = 0;
	int height = 0;
	int channels = 0;
	unsigned char* image = stbi_load_from_memory(bytes, length, &width, &height, &channels, 4);
	if (!image) {
		if (error) {
			const char* reason = stbi_failure_reason();
			*error = reason ? reason : "stb_image could not decode the embedded texture";
		}
		return std::nullopt;
	}

	ImageData data;
	data.width = width;
	data.height = height;
	data.pixels.reserve(static_cast<std::size_t>(width * height));
	data.alpha.reserve(static_cast<std::size_t>(width * height));

	for (int index = 0; index < width * height; ++index) {
		const int base = index * 4;
		data.pixels.push_back({
		    static_cast<float>(image[base]) / 255.0f,
		    static_cast<float>(image[base + 1]) / 255.0f,
		    static_cast<float>(image[base + 2]) / 255.0f,
		});
		data.alpha.push_back(static_cast<float>(image[base + 3]) / 255.0f);
	}

	stbi_image_free(image);
	return data;
}

namespace {

Vec3 to_vec3(const aiVector3D& value) {
	return { value.x, value.y, value.z };
}

Rgb material_color(const aiScene* scene, unsigned int material_index) {
	if (!scene || material_index >= scene->mNumMaterials) {
		return { 0.80f, 0.84f, 0.92f };
	}

	const aiMaterial* material = scene->mMaterials[material_index];
	aiColor3D diffuse(0.80f, 0.84f, 0.92f);
	if (material->Get(AI_MATKEY_BASE_COLOR, diffuse) != aiReturn_SUCCESS
	    && material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse) != aiReturn_SUCCESS
	    && material->Get(AI_MATKEY_COLOR_EMISSIVE, diffuse) != aiReturn_SUCCESS) {
		material->Get(AI_MATKEY_COLOR_AMBIENT, diffuse);
	}

	return {
		std::max(0.15f, diffuse.r),
		std::max(0.15f, diffuse.g),
		std::max(0.15f, diffuse.b),
	};
}

Rgb color_from_ai(const aiColor4D& color) {
	return {
		clamp01(color.r),
		clamp01(color.g),
		clamp01(color.b),
	};
}

std::string normalized_texture_path(const aiString& path) {
	std::string value = path.C_Str();
	std::replace(value.begin(), value.end(), '\\', '/');
	return value;
}

std::filesystem::path resolve_texture_path(const std::filesystem::path& model_path, const std::string& texture_path) {
	const std::filesystem::path candidate(texture_path);
	if (candidate.is_absolute()) {
		return candidate;
	}

	std::error_code error;
	const std::filesystem::path joined = model_path.parent_path() / candidate;
	const std::filesystem::path normalized = joined.lexically_normal();
	if (std::filesystem::exists(normalized, error)) {
		return normalized;
	}

	return joined;
}

std::optional<ImageData> load_embedded_texture(const aiTexture* texture) {
	if (!texture) {
		return std::nullopt;
	}

	if (texture->mHeight == 0) {
		return load_image_memory(
		    reinterpret_cast<const unsigned char*>(texture->pcData),
		    static_cast<int>(texture->mWidth));
	}

	ImageData data;
	data.width = static_cast<int>(texture->mWidth);
	data.height = static_cast<int>(texture->mHeight);
	data.pixels.reserve(static_cast<std::size_t>(data.width * data.height));
	data.alpha.reserve(static_cast<std::size_t>(data.width * data.height));

	for (unsigned int index = 0; index < texture->mWidth * texture->mHeight; ++index) {
		const aiTexel texel = texture->pcData[index];
		data.pixels.push_back({
		    static_cast<float>(texel.r) / 255.0f,
		    static_cast<float>(texel.g) / 255.0f,
		    static_cast<float>(texel.b) / 255.0f,
		});
		data.alpha.push_back(static_cast<float>(texel.a) / 255.0f);
	}

	return data;
}

std::optional<MaterialTexture> load_material_texture(
    const aiScene* scene,
    const std::filesystem::path& model_path,
    const aiMaterial* material) {
	if (!scene || !material) {
		return std::nullopt;
	}

	aiString texture_path;
	const aiTextureType texture_types[] = {
		aiTextureType_BASE_COLOR,
		aiTextureType_DIFFUSE,
		aiTextureType_UNKNOWN,
	};

	for (const aiTextureType texture_type : texture_types) {
		if (material->GetTextureCount(texture_type) == 0) {
			continue;
		}

		if (material->GetTexture(texture_type, 0, &texture_path) != aiReturn_SUCCESS) {
			continue;
		}

		const std::string normalized = normalized_texture_path(texture_path);
		unsigned int uv_channel = 0;
		material->Get(AI_MATKEY_UVWSRC(texture_type, 0), uv_channel);
		if (const aiTexture* embedded = scene->GetEmbeddedTexture(normalized.c_str())) {
			if (const auto image = load_embedded_texture(embedded)) {
				return MaterialTexture { *image, uv_channel };
			}
			continue;
		}

		const std::filesystem::path resolved = resolve_texture_path(model_path, normalized);
		if (const auto image = load_image(resolved.string().c_str())) {
			return MaterialTexture { *image, uv_channel };
		}
	}

	return std::nullopt;
}

int material_texture_index(
    ModelData& model,
    const aiScene* scene,
    const std::filesystem::path& model_path,
    unsigned int material_index,
    std::unordered_map<unsigned int, int>& cache) {
	if (!scene || material_index >= scene->mNumMaterials) {
		return -1;
	}

	if (const auto found = cache.find(material_index); found != cache.end()) {
		return found->second;
	}

	const aiMaterial* material = scene->mMaterials[material_index];
	const auto texture = load_material_texture(scene, model_path, material);
	const int texture_index = texture ? static_cast<int>(model.textures.size()) : -1;

	if (texture) {
		model.textures.push_back(texture->image);
		model.texture_uv_channels.push_back(texture->uv_channel);
	}

	cache.emplace(material_index, texture_index);
	return texture_index;
}

void update_bounds(Bounds& bounds, Vec3 point) {
	bounds.min.x = std::min(bounds.min.x, point.x);
	bounds.min.y = std::min(bounds.min.y, point.y);
	bounds.min.z = std::min(bounds.min.z, point.z);
	bounds.max.x = std::max(bounds.max.x, point.x);
	bounds.max.y = std::max(bounds.max.y, point.y);
	bounds.max.z = std::max(bounds.max.z, point.z);
}

Vec3 transform_position(const aiMatrix4x4& matrix, Vec3 value) {
	const aiVector3D transformed = matrix * aiVector3D(value.x, value.y, value.z);
	return to_vec3(transformed);
}

Vec3 transform_normal(const aiMatrix4x4& matrix, Vec3 value) {
	aiMatrix3x3 normal_matrix(matrix);
	normal_matrix.Inverse().Transpose();
	const aiVector3D transformed = normal_matrix * aiVector3D(value.x, value.y, value.z);
	return normalize(to_vec3(transformed));
}

void collect_triangles(
    const aiScene* scene,
    const aiNode* node,
    const aiMatrix4x4& parent_transform,
    const std::filesystem::path& model_path,
    ModelData& model,
    std::vector<Triangle>& triangles,
    Bounds& bounds,
    std::unordered_map<unsigned int, int>& texture_cache) {
	const aiMatrix4x4 transform = parent_transform * node->mTransformation;

	for (unsigned int mesh_index = 0; mesh_index < node->mNumMeshes; ++mesh_index) {
		const aiMesh* mesh = scene->mMeshes[node->mMeshes[mesh_index]];
		const Rgb mesh_material_color = material_color(scene, mesh->mMaterialIndex);
		const int texture_index = material_texture_index(model, scene, model_path, mesh->mMaterialIndex, texture_cache);
		const unsigned int uv_channel = texture_index >= 0
		        && texture_index < static_cast<int>(model.texture_uv_channels.size())
		    ? model.texture_uv_channels[static_cast<std::size_t>(texture_index)]
		    : 0;
		const bool has_uv = mesh->HasTextureCoords(uv_channel);
		const bool has_vertex_colors = mesh->HasVertexColors(0);

		for (unsigned int face_index = 0; face_index < mesh->mNumFaces; ++face_index) {
			const aiFace& face = mesh->mFaces[face_index];
			if (face.mNumIndices != 3) {
				continue;
			}

			Triangle triangle {};
			Vertex* vertices[3] = { &triangle.a, &triangle.b, &triangle.c };

			for (int i = 0; i < 3; ++i) {
				const unsigned int vertex_index = face.mIndices[i];
				const aiVector3D position = mesh->mVertices[vertex_index];
				const aiVector3D normal = mesh->HasNormals() ? mesh->mNormals[vertex_index] : aiVector3D(0.0f, 0.0f, 1.0f);
				vertices[i]->position = transform_position(transform, to_vec3(position));
				vertices[i]->normal = transform_normal(transform, to_vec3(normal));
				vertices[i]->uv = has_uv
				    ? Vec2 { mesh->mTextureCoords[uv_channel][vertex_index].x, mesh->mTextureCoords[uv_channel][vertex_index].y }
				    : Vec2 { 0.0f, 0.0f };
				vertices[i]->color = has_vertex_colors
				    ? color_from_ai(mesh->mColors[0][vertex_index])
				    : Rgb { 1.0f, 1.0f, 1.0f };
				update_bounds(bounds, vertices[i]->position);
			}

			triangle.material_color = mesh_material_color;
			triangle.texture_index = texture_index;
			triangle.uv_channel = uv_channel;
			triangle.has_uv = has_uv && texture_index >= 0;
			triangles.push_back(triangle);
		}
	}

	for (unsigned int child_index = 0; child_index < node->mNumChildren; ++child_index) {
		collect_triangles(scene, node->mChildren[child_index], transform, model_path, model, triangles, bounds, texture_cache);
	}
}

}

SampledPixel sample_texture(const ImageData& image, Vec2 uv) {
	if (image.width <= 0 || image.height <= 0 || image.pixels.empty()) {
		return { { 1.0f, 1.0f, 1.0f }, 1.0f };
	}

	const float clamped_u = clamp01(uv.x);
	const float clamped_v = clamp01(uv.y);
	const float tex_x = clamped_u * static_cast<float>(image.width - 1);
	const float tex_y = (1.0f - clamped_v) * static_cast<float>(image.height - 1);
	const int x0 = std::clamp(static_cast<int>(std::floor(tex_x)), 0, image.width - 1);
	const int x1 = std::clamp(x0 + 1, 0, image.width - 1);
	const int y0 = std::clamp(static_cast<int>(std::floor(tex_y)), 0, image.height - 1);
	const int y1 = std::clamp(y0 + 1, 0, image.height - 1);
	const float tx = tex_x - static_cast<float>(x0);
	const float ty = tex_y - static_cast<float>(y0);

	const auto fetch = [&](int x, int y) -> SampledPixel {
		const std::size_t index = static_cast<std::size_t>(y * image.width + x);
		return {
			image.pixels[index],
			image.alpha.empty() ? 1.0f : image.alpha[index],
		};
	};

	const SampledPixel p00 = fetch(x0, y0);
	const SampledPixel p10 = fetch(x1, y0);
	const SampledPixel p01 = fetch(x0, y1);
	const SampledPixel p11 = fetch(x1, y1);

	const Rgb top = lerp(p00.color, p10.color, tx);
	const Rgb bottom = lerp(p01.color, p11.color, tx);
	const float top_alpha = p00.alpha * (1.0f - tx) + p10.alpha * tx;
	const float bottom_alpha = p01.alpha * (1.0f - tx) + p11.alpha * tx;

	return {
		lerp(top, bottom, ty),
		top_alpha * (1.0f - ty) + bottom_alpha * ty,
	};
}

std::optional<ModelData> load_model(const char* file_path, std::string* error) {
	Assimp::Importer importer;
	const std::filesystem::path model_path(file_path);
	const aiScene* scene = importer.ReadFile(
	    file_path,
	    aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_ImproveCacheLocality | aiProcess_SortByPType | aiProcess_GenSmoothNormals | aiProcess_OptimizeMeshes);

	if (!scene || !scene->mRootNode || scene->mNumMeshes == 0) {
		if (error) {
			const std::string reason = importer.GetErrorString();
			*error = reason.empty() ? "Assimp could not load any meshes from the file" : reason;
		}
		return std::nullopt;
	}

	Bounds bounds {
		{ std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() },
		{ std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest() },
	};

	ModelData model;
	std::unordered_map<unsigned int, int> texture_cache;
	collect_triangles(scene, scene->mRootNode, aiMatrix4x4(), model_path, model, model.triangles, bounds, texture_cache);
	if (model.triangles.empty()) {
		return std::nullopt;
	}

	const Vec3 center = {
		(bounds.min.x + bounds.max.x) * 0.5f,
		(bounds.min.y + bounds.max.y) * 0.5f,
		(bounds.min.z + bounds.max.z) * 0.5f,
	};
	const Vec3 size = subtract(bounds.max, bounds.min);
	const float max_extent = std::max({ size.x, size.y, size.z, 1e-3f });
	const float scale = 2.0f / max_extent;

	for (Triangle& triangle : model.triangles) {
		triangle.a.position = multiply(subtract(triangle.a.position, center), scale);
		triangle.b.position = multiply(subtract(triangle.b.position, center), scale);
		triangle.c.position = multiply(subtract(triangle.c.position, center), scale);
		triangle.a.normal = normalize(triangle.a.normal);
		triangle.b.normal = normalize(triangle.b.normal);
		triangle.c.normal = normalize(triangle.c.normal);
	}

	return model;
}

bool load_cached_asset(CachedAsset& cache, const std::string& file_path, std::string* error) {
	if (cache.file_path == file_path && cache.kind != CachedAssetKind::none) {
		return true;
	}

	cache = CachedAsset {};
	cache.file_path = file_path;

	const std::string path_lower = lowercase(file_path);
	const bool prefer_model = has_suffix(path_lower, ".obj") || has_suffix(path_lower, ".fbx");
	std::string image_error;
	std::string model_error;

	if (!prefer_model) {
		if (const auto image = load_image(file_path.c_str(), &image_error)) {
			cache.kind = CachedAssetKind::image;
			cache.image = std::move(*image);
			return true;
		}
	}

	if (const auto model = load_model(file_path.c_str(), &model_error)) {
		cache.kind = CachedAssetKind::model;
		cache.model = std::move(*model);
		return true;
	}

	if (const auto image = load_image(file_path.c_str(), &image_error)) {
		cache.kind = CachedAssetKind::image;
		cache.image = std::move(*image);
		return true;
	}

	cache.kind = CachedAssetKind::none;
	if (error) {
		std::ostringstream detail;
		detail << "Could not load `" << file_path << "`";
		if (!model_error.empty()) {
			detail << " as a model: " << model_error;
		}
		if (!image_error.empty()) {
			if (!model_error.empty()) {
				detail << "; ";
			} else {
				detail << " as an image: ";
			}
			detail << image_error;
		}
		*error = detail.str();
	}
	return false;
}
