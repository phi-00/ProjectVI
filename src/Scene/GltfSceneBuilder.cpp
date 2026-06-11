#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define STB_IMAGE_IMPLEMENTATION
#include <tiny_gltf.h>

#include <glm/ext/matrix_float3x3.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/fwd.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/matrix.hpp>

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <string_view>
#include <utility>
#include <vector>

#include <cstdint>
#include <filesystem>
#include <format>
#include <optional>
#include <stdexcept>
#include <string>

#include "Scene/Scene.hpp"
#include "Scene/SceneBuilder.hpp"

#include "Image/Image.hpp"
#include "Light/Light.hpp"
#include "Math/RGB.hpp"
#include "Math/Vector.hpp"
#include "Primitive/Geometry/Mesh.hpp"
#include "Primitive/Geometry/Triangle.hpp"
#include "Primitive/Material.hpp"

namespace VI
{
namespace
{

const unsigned char* GetAccessorData(const tinygltf::Model& model, const tinygltf::Accessor& accessor)
{
  if (accessor.bufferView < 0 || static_cast<size_t>(accessor.bufferView) >= model.bufferViews.size())
  {
    throw std::runtime_error("glTF accessor is missing a valid buffer view");
  }

  const tinygltf::BufferView& buffer_view = model.bufferViews[accessor.bufferView];
  if (buffer_view.buffer < 0 || static_cast<size_t>(buffer_view.buffer) >= model.buffers.size())
  {
    throw std::runtime_error("glTF buffer view is missing a valid buffer");
  }

  const tinygltf::Buffer& buffer = model.buffers[buffer_view.buffer];
  return buffer.data.data() + buffer_view.byteOffset + accessor.byteOffset;
}

int GetAccessorStride(const tinygltf::Model& model, const tinygltf::Accessor& accessor)
{
  const tinygltf::BufferView& buffer_view = model.bufferViews[accessor.bufferView];
  const int stride = accessor.ByteStride(buffer_view);
  if (stride <= 0)
  {
    throw std::runtime_error("glTF accessor has invalid byte stride");
  }
  return stride;
}

const tinygltf::Accessor& GetAccessor(const tinygltf::Model& model, int accessor_index)
{
  if (accessor_index < 0 || static_cast<size_t>(accessor_index) >= model.accessors.size())
  {
    throw std::runtime_error("glTF primitive references an invalid accessor");
  }
  return model.accessors[accessor_index];
}

int FindAttribute(const tinygltf::Primitive& primitive, const std::string& name)
{
  const auto it = primitive.attributes.find(name);
  return it == primitive.attributes.end() ? -1 : it->second;
}

Vec2 ReadVec2(const tinygltf::Model& model, const tinygltf::Accessor& accessor, size_t index)
{
  if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || accessor.type != TINYGLTF_TYPE_VEC2)
  {
    throw std::runtime_error("glTF TEXCOORD_0 accessor must be FLOAT VEC2");
  }

  const unsigned char* data = GetAccessorData(model, accessor);
  const int stride = GetAccessorStride(model, accessor);
  const auto* values = reinterpret_cast<const float*>(data + index * stride);
  return Vec2{values[0], values[1]};
}

Vector ReadVec3(const tinygltf::Model& model, const tinygltf::Accessor& accessor, size_t index, std::string_view attribute_name)
{
  if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || accessor.type != TINYGLTF_TYPE_VEC3)
  {
    throw std::runtime_error(std::string{"glTF "} + std::string{attribute_name} + " accessor must be FLOAT VEC3");
  }

  const unsigned char* data = GetAccessorData(model, accessor);
  const int stride = GetAccessorStride(model, accessor);
  const auto* values = reinterpret_cast<const float*>(data + index * stride);
  return Vector{values[0], values[1], values[2]};
}

uint32_t ReadIndexValue(const unsigned char* data, size_t index, int component_type)
{
  switch (component_type)
  {
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
      return static_cast<uint32_t>(data[index]);
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
      return static_cast<uint32_t>(reinterpret_cast<const uint16_t*>(data)[index]);
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
      return reinterpret_cast<const uint32_t*>(data)[index];
    default:
      throw std::runtime_error("glTF indices must use unsigned byte, short, or int components");
  }
}

std::vector<uint32_t> ReadIndices(const tinygltf::Model& model, const tinygltf::Primitive& primitive, size_t vertex_count)
{
  if (primitive.indices < 0)
  {
    std::vector<uint32_t> indices(vertex_count);
    for (size_t i = 0; i < vertex_count; ++i)
    {
      indices[i] = static_cast<uint32_t>(i);
    }
    return indices;
  }

  const tinygltf::Accessor& accessor = GetAccessor(model, primitive.indices);
  if (accessor.type != TINYGLTF_TYPE_SCALAR)
  {
    throw std::runtime_error("glTF index accessor must be SCALAR");
  }

  const unsigned char* data = GetAccessorData(model, accessor);
  std::vector<uint32_t> indices(accessor.count);
  for (size_t i = 0; i < accessor.count; ++i)
  {
    indices[i] = ReadIndexValue(data, i, accessor.componentType);
  }
  return indices;
}

TextureWrapMode ConvertWrapMode(int wrap)
{
  switch (wrap)
  {
    case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
      return TextureWrapMode::ClampToEdge;
    case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
      return TextureWrapMode::MirroredRepeat;
    case TINYGLTF_TEXTURE_WRAP_REPEAT:
    default:
      return TextureWrapMode::Repeat;
  }
}

TextureFilterMode ConvertFilterMode(int min_filter, int mag_filter)
{
  if (min_filter == TINYGLTF_TEXTURE_FILTER_NEAREST || mag_filter == TINYGLTF_TEXTURE_FILTER_NEAREST)
  {
    return TextureFilterMode::Nearest;
  }
  return TextureFilterMode::Linear;
}

TextureSampler CreateTextureSampler(const tinygltf::Model& model, int texture_index)
{
  if (texture_index < 0 || static_cast<size_t>(texture_index) >= model.textures.size())
  {
    throw std::runtime_error("glTF material references an invalid texture");
  }

  const tinygltf::Texture& texture = model.textures[texture_index];
  if (texture.source < 0 || static_cast<size_t>(texture.source) >= model.images.size())
  {
    throw std::runtime_error("glTF texture references an invalid image");
  }

  const tinygltf::Image& source = model.images[texture.source];
  if (source.width <= 0 || source.height <= 0 || source.image.empty())
  {
    throw std::runtime_error("glTF image has no decoded pixels");
  }
  if (source.bits != 8)
  {
    throw std::runtime_error("Only 8-bit glTF base color textures are supported");
  }

  Image image{source.width, source.height};
  const int component_count = std::max(source.component, 1);
  for (int y = 0; y < source.height; ++y)
  {
    for (int x = 0; x < source.width; ++x)
    {
      const size_t pixel_index = static_cast<size_t>((x + y * source.width) * component_count);
      const float r = static_cast<float>(source.image[pixel_index]) / 255.0f;
      const float g = static_cast<float>(source.image[pixel_index + std::min(1, component_count - 1)]) / 255.0f;
      const float b = static_cast<float>(source.image[pixel_index + std::min(2, component_count - 1)]) / 255.0f;
      image.Set(x, y, RGB{r, g, b});
    }
  }

  TextureSampler sampler{.Data = std::move(image)};
  if (texture.sampler >= 0 && static_cast<size_t>(texture.sampler) < model.samplers.size())
  {
    const tinygltf::Sampler& gltf_sampler = model.samplers[texture.sampler];
    sampler.WrapS = ConvertWrapMode(gltf_sampler.wrapS);
    sampler.WrapT = ConvertWrapMode(gltf_sampler.wrapT);
    sampler.Filter = ConvertFilterMode(gltf_sampler.minFilter, gltf_sampler.magFilter);
  }
  return sampler;
}

float ReadEmissiveStrength(const tinygltf::Material& material)
{
  const auto extension_it = material.extensions.find("KHR_materials_emissive_strength");
  if (extension_it == material.extensions.end() || !extension_it->second.IsObject())
  {
    return 1.0f;
  }

  const tinygltf::Value& strength = extension_it->second.Get("emissiveStrength");
  return strength.IsNumber() ? static_cast<float>(strength.GetNumberAsDouble()) : 1.0f;
}

std::vector<int> ImportMaterials(const tinygltf::Model& model, Scene& scene)
{
  std::vector<int> material_map{};
  material_map.reserve(model.materials.size());

  for (size_t material_index = 0; material_index < model.materials.size(); ++material_index)
  {
    const tinygltf::Material& gltf_material = model.materials[material_index];
    const auto& pbr = gltf_material.pbrMetallicRoughness;

    const std::string name = gltf_material.name.empty() ? std::format("glTF Material {}", material_index) : gltf_material.name;
    const RGB base_color{
        static_cast<float>(pbr.baseColorFactor[0]),
        static_cast<float>(pbr.baseColorFactor[1]),
        static_cast<float>(pbr.baseColorFactor[2]),
    };
    const RGB emission_color{
        static_cast<float>(gltf_material.emissiveFactor[0]),
        static_cast<float>(gltf_material.emissiveFactor[1]),
        static_cast<float>(gltf_material.emissiveFactor[2]),
    };

    std::optional<TextureSampler> albedo_texture = std::nullopt;
    if (pbr.baseColorTexture.index >= 0)
    {
      albedo_texture = CreateTextureSampler(model, pbr.baseColorTexture.index);
    }
    std::optional<TextureSampler> metallic_roughness_texture = std::nullopt;
    if (pbr.metallicRoughnessTexture.index >= 0)
    {
      metallic_roughness_texture = CreateTextureSampler(model, pbr.metallicRoughnessTexture.index);
    }
    std::optional<TextureSampler> emission_texture = std::nullopt;
    if (gltf_material.emissiveTexture.index >= 0)
    {
      emission_texture = CreateTextureSampler(model, gltf_material.emissiveTexture.index);
    }

    material_map.push_back(scene.AddMaterial({
        .Name = name,
        .Albedo = base_color,
        .Roughness = static_cast<float>(pbr.roughnessFactor),
        .Metallic = static_cast<float>(pbr.metallicFactor),
        .EmissionColor = emission_color,
        .EmissionPower = glm::length(emission_color) <= 0.0f ? 0.0f : ReadEmissiveStrength(gltf_material),
        .AlbedoTexture = std::move(albedo_texture),
        .MetallicRoughnessTexture = std::move(metallic_roughness_texture),
        .EmissionTexture = std::move(emission_texture),
    }));
  }

  return material_map;
}

glm::mat4 GetNodeTransform(const tinygltf::Node& node)
{
  if (node.matrix.size() == 16)
  {
    glm::mat4 transform{1.f};
    for (int column = 0; column < 4; ++column)
    {
      for (int row = 0; row < 4; ++row)
      {
        transform[column][row] = static_cast<float>(node.matrix[static_cast<size_t>(column * 4 + row)]);
      }
    }
    return transform;
  }

  glm::mat4 transform{1.f};
  if (node.translation.size() == 3)
  {
    transform = glm::translate(transform, Vector{
                                              static_cast<float>(node.translation[0]),
                                              static_cast<float>(node.translation[1]),
                                              static_cast<float>(node.translation[2]),
                                          });
  }

  if (node.rotation.size() == 4)
  {
    const glm::quat rotation{
        static_cast<float>(node.rotation[3]),
        static_cast<float>(node.rotation[0]),
        static_cast<float>(node.rotation[1]),
        static_cast<float>(node.rotation[2]),
    };
    transform *= glm::mat4_cast(rotation);
  }

  if (node.scale.size() == 3)
  {
    transform = glm::scale(transform, Vector{
                                          static_cast<float>(node.scale[0]),
                                          static_cast<float>(node.scale[1]),
                                          static_cast<float>(node.scale[2]),
                                      });
  }

  return transform;
}

Vector ComputeFaceNormal(const Point& v0, const Point& v1, const Point& v2)
{
  const Vector normal = glm::cross(v1 - v0, v2 - v0);
  const float length = glm::length(normal);
  return length > 0.0f ? normal / length : Vector{0.f, 1.f, 0.f};
}

std::optional<Camera> CreateCameraFromNode(const tinygltf::Model& model, const tinygltf::Node& node, const glm::mat4& transform, int camera_width, int camera_height)
{
  if (node.camera < 0)
  {
    return std::nullopt;
  }
  if (static_cast<size_t>(node.camera) >= model.cameras.size())
  {
    throw std::runtime_error("glTF node references an invalid camera");
  }

  const tinygltf::Camera& gltf_camera = model.cameras[node.camera];
  if (gltf_camera.type != "perspective")
  {
    // This renderer's Camera is perspective-only. glTF also supports
    // orthographic cameras, but importing those would require a different ray
    // generation model.
    return std::nullopt;
  }

  const double yfov = gltf_camera.perspective.yfov;
  if (yfov <= 0.0 || !std::isfinite(yfov))
  {
    throw std::runtime_error("glTF perspective camera has an invalid yfov");
  }

  // In glTF, a camera looks down its local -Z axis and its local +Y axis is up.
  // The node transform places that local camera frame into world space.
  const glm::vec4 world_eye = transform * glm::vec4{0.f, 0.f, 0.f, 1.f};
  const glm::vec4 world_forward = transform * glm::vec4{0.f, 0.f, -1.f, 0.f};
  const glm::vec4 world_up = transform * glm::vec4{0.f, 1.f, 0.f, 0.f};
  const Vector forward = glm::normalize(Vector{world_forward.x, world_forward.y, world_forward.z});
  const Vector up = glm::normalize(Vector{world_up.x, world_up.y, world_up.z});

  const Point eye{world_eye.x, world_eye.y, world_eye.z};
  const Point at = eye + forward;

  // glTF stores vertical field of view for perspective cameras. This Camera
  // implementation also uses that angle to define the viewport height; the
  // final aspect ratio comes from the render resolution.
  return Camera{eye, at, up, camera_width, camera_height, static_cast<float>(yfov)};
}

Camera CreateDefaultCameraForBounds(const BoundingBox& bounds, int camera_width, int camera_height)
{
  const Point center = 0.5f * (bounds.Min + bounds.Max);
  const Vector size = bounds.Max - bounds.Min;
  const float fov = 45.0f * glm::pi<float>() / 180.0f;
  const float aspect = static_cast<float>(camera_width) / static_cast<float>(camera_height);
  const float tan_half_fov = std::tan(fov * 0.5f);
  const float half_depth = 0.5f * size.z;
  const float distance_for_height = (0.5f * size.y) / tan_half_fov;
  const float distance_for_width = (0.5f * size.x) / (tan_half_fov * aspect);
  const float distance = half_depth + std::max(distance_for_height, distance_for_width) * 1.1f;

  // If the glTF file has no camera, frame the imported geometry from the
  // negative Z direction. This is much better for object-style sample models
  // than reusing a hard-coded camera from another scene.
  const Point eye = center + Vector{0.0f, size.y * 0.1f, -distance};
  return Camera{eye, center, Vector{0.f, 1.f, 0.f}, camera_width, camera_height, fov};
}

bool HasImportedRenderableLight(const Scene& scene)
{
  for (const auto& light : scene.GetLights())
  {
    if (light->GetType() == LightType::Area || light->GetType() == LightType::Point)
    {
      return true;
    }
  }

  return false;
}

void ImportPrimitive(const tinygltf::Model& model, const tinygltf::Mesh& mesh, const tinygltf::Primitive& primitive, const glm::mat4& transform, const std::vector<int>& material_map, int default_material, Scene& scene)
{
  if (primitive.mode != -1 && primitive.mode != TINYGLTF_MODE_TRIANGLES)
  {
    throw std::runtime_error("Only glTF triangle primitives are supported");
  }

  const int position_accessor_index = FindAttribute(primitive, "POSITION");
  if (position_accessor_index < 0)
  {
    throw std::runtime_error("glTF primitive is missing POSITION");
  }

  const tinygltf::Accessor& position_accessor = GetAccessor(model, position_accessor_index);
  const int normal_accessor_index = FindAttribute(primitive, "NORMAL");
  const int uv_accessor_index = FindAttribute(primitive, "TEXCOORD_0");

  const tinygltf::Accessor* normal_accessor = normal_accessor_index >= 0 ? &GetAccessor(model, normal_accessor_index) : nullptr;
  const tinygltf::Accessor* uv_accessor = uv_accessor_index >= 0 ? &GetAccessor(model, uv_accessor_index) : nullptr;

  const std::vector<uint32_t> indices = ReadIndices(model, primitive, position_accessor.count);
  if (indices.size() % 3 != 0)
  {
    throw std::runtime_error("glTF triangle primitive index count is not divisible by three");
  }

  std::vector<Triangle> triangles{};
  triangles.reserve(indices.size() / 3);

  const glm::mat3 normal_transform = glm::transpose(glm::inverse(glm::mat3(transform)));
  for (size_t i = 0; i < indices.size(); i += 3)
  {
    const uint32_t i0 = indices[i + 0];
    const uint32_t i1 = indices[i + 1];
    const uint32_t i2 = indices[i + 2];
    if (i0 >= position_accessor.count || i1 >= position_accessor.count || i2 >= position_accessor.count)
    {
      throw std::runtime_error("glTF primitive index is out of range");
    }

    const glm::vec4 hp0 = transform * glm::vec4{ReadVec3(model, position_accessor, i0, "POSITION"), 1.0f};
    const glm::vec4 hp1 = transform * glm::vec4{ReadVec3(model, position_accessor, i1, "POSITION"), 1.0f};
    const glm::vec4 hp2 = transform * glm::vec4{ReadVec3(model, position_accessor, i2, "POSITION"), 1.0f};
    const Point p0{hp0.x, hp0.y, hp0.z};
    const Point p1{hp1.x, hp1.y, hp1.z};
    const Point p2{hp2.x, hp2.y, hp2.z};

    Vector normal = ComputeFaceNormal(p0, p1, p2);
    if (normal_accessor != nullptr && i0 < normal_accessor->count && i1 < normal_accessor->count && i2 < normal_accessor->count)
    {
      const Vector n0 = normal_transform * ReadVec3(model, *normal_accessor, i0, "NORMAL");
      const Vector n1 = normal_transform * ReadVec3(model, *normal_accessor, i1, "NORMAL");
      const Vector n2 = normal_transform * ReadVec3(model, *normal_accessor, i2, "NORMAL");
      const Vector averaged = n0 + n1 + n2;
      if (glm::length(averaged) > 0.0f)
      {
        normal = glm::normalize(averaged);
      }
    }

    const Vec2 uv0 = uv_accessor != nullptr && i0 < uv_accessor->count ? ReadVec2(model, *uv_accessor, i0) : Vec2{0.f};
    const Vec2 uv1 = uv_accessor != nullptr && i1 < uv_accessor->count ? ReadVec2(model, *uv_accessor, i1) : Vec2{0.f};
    const Vec2 uv2 = uv_accessor != nullptr && i2 < uv_accessor->count ? ReadVec2(model, *uv_accessor, i2) : Vec2{0.f};
    triangles.emplace_back(p0, p1, p2, normal, uv0, uv1, uv2);
  }

  const int material = primitive.material >= 0 && static_cast<size_t>(primitive.material) < material_map.size() ? material_map[primitive.material] : default_material;
  const std::string mesh_name = mesh.name.empty() ? "glTF Mesh" : mesh.name;
  const int primitive_index = static_cast<int>(scene.GetPrimitiveCount());
  scene.AddPrimitive(Mesh{mesh_name, std::move(triangles)}, material);

  if (scene.GetMaterial(material).GetEmissionPower() > 0.0f)
  {
    scene.AddLight(std::make_unique<AreaLight>(material, primitive_index));
  }
}

void ImportNode(const tinygltf::Model& model, int node_index, const glm::mat4& parent_transform, const std::vector<int>& material_map, int default_material, int camera_width, int camera_height, Scene& scene)
{
  if (node_index < 0 || static_cast<size_t>(node_index) >= model.nodes.size())
  {
    throw std::runtime_error("glTF scene references an invalid node");
  }

  const tinygltf::Node& node = model.nodes[node_index];
  const glm::mat4 transform = parent_transform * GetNodeTransform(node);

  if (scene.GetCamera() == nullptr)
  {
    if (std::optional<Camera> camera = CreateCameraFromNode(model, node, transform, camera_width, camera_height))
    {
      scene.SetCamera(std::move(camera.value()));
    }
  }

  if (node.mesh >= 0)
  {
    if (static_cast<size_t>(node.mesh) >= model.meshes.size())
    {
      throw std::runtime_error("glTF node references an invalid mesh");
    }

    const tinygltf::Mesh& mesh = model.meshes[node.mesh];
    for (const tinygltf::Primitive& primitive : mesh.primitives)
    {
      ImportPrimitive(model, mesh, primitive, transform, material_map, default_material, scene);
    }
  }

  for (const int child : node.children)
  {
    ImportNode(model, child, transform, material_map, default_material, camera_width, camera_height, scene);
  }
}

tinygltf::Model LoadModel(const std::filesystem::path& path)
{
  tinygltf::Model model{};
  tinygltf::TinyGLTF loader{};
  loader.SetPreserveImageChannels(false);

  std::string error{};
  std::string warning{};
  const std::string filename = path.string();
  const bool loaded = path.extension() == ".glb" ? loader.LoadBinaryFromFile(&model, &error, &warning, filename) : loader.LoadASCIIFromFile(&model, &error, &warning, filename);
  if (!loaded)
  {
    throw std::runtime_error("Failed to load glTF scene '" + filename + "': " + error);
  }
  if (!error.empty())
  {
    throw std::runtime_error("glTF loader error for '" + filename + "': " + error);
  }

  return model;
}

} // namespace

Scene CreateGltfScene(const std::filesystem::path& path, int camera_width, int camera_height)
{
  tinygltf::Model model = LoadModel(path);

  Scene scene{};
  const int default_material = scene.AddMaterial({.Name = "Default glTF Material", .Albedo = RGB{0.8f}, .Roughness = 1.0f});
  const std::vector<int> material_map = ImportMaterials(model, scene);

  if (model.scenes.empty())
  {
    throw std::runtime_error("glTF file contains no scenes");
  }

  const int scene_index = model.defaultScene >= 0 ? model.defaultScene : 0;
  if (scene_index < 0 || static_cast<size_t>(scene_index) >= model.scenes.size())
  {
    throw std::runtime_error("glTF default scene index is invalid");
  }

  for (const int node : model.scenes[scene_index].nodes)
  {
    ImportNode(model, node, glm::mat4{1.f}, material_map, default_material, camera_width, camera_height, scene);
  }

  if (scene.GetCamera() == nullptr && scene.GetPrimitiveCount() > 0)
  {
    scene.SetCamera(CreateDefaultCameraForBounds(scene.ComputeBoundingBox(), camera_width, camera_height));
  }

  if (!HasImportedRenderableLight(scene))
  {
    // Most object-style glTF sample models do not contain renderable lights.
    // Add a soft preview ambient light so classroom assets such as Lantern are
    // visible without hand-building a separate lighting setup around them.
    const int preview_ambient_material = scene.AddMaterial({
        .Name = "glTF Preview Ambient",
        .Albedo = RGB{1.f},
        .EmissionColor = RGB{1.f},
        .EmissionPower = 0.35f,
    });
    scene.AddLight(std::make_unique<AmbientLight>(preview_ambient_material));
  }

  return scene;
}

} // namespace VI
