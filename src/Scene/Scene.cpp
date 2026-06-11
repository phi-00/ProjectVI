#include "Scene/Scene.hpp"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <memory>
#include <span>
#include <utility>
#include <variant>
#include <vector>

#include "Camera/Camera.hpp"
#include "Light/Light.hpp"
#include "Math/DiscreteDistribution.hpp"
#include "Math/Math.hpp"
#include "Math/RGB.hpp"
#include "Primitive/AccelerationStructures/GridAccelerationStructure.hpp"
#include "Primitive/BoundingBox.hpp"
#include "Primitive/Geometry/Geometry.hpp"
#include "Primitive/Geometry/Mesh.hpp"
#include "Primitive/Geometry/Triangle.hpp"
#include "Primitive/Material.hpp"
#include "Primitive/Primitive.hpp"
#include "Ray/Intersection.hpp"
#include "Ray/Ray.hpp"

namespace VI
{
namespace
{

bool IsSupportedDirectLightType(const LightType light_type)
{
  return light_type == LightType::Ambient || light_type == LightType::Point || light_type == LightType::Area;
}

float ComputeTriangleArea(const Triangle& triangle)
{
  const auto [v1, v2, v3] = triangle.GetVertices();
  return 0.5f * glm::length(glm::cross(v2 - v1, v3 - v1));
}

float ComputeMeshArea(const Mesh& mesh)
{
  float total_area = 0.f;
  for (size_t i = 0; i < mesh.GetTriangleCount(); ++i)
  {
    total_area += ComputeTriangleArea(mesh.GetTriangle(i));
  }
  return total_area;
}

float ComputeLuminance(const RGB& radiance)
{
  return 0.2126f * radiance.r + 0.7152f * radiance.g + 0.0722f * radiance.b;
}

float ComputeLightWeight(const Scene& scene, const Light& light)
{
  const Material& material = scene.GetMaterial(light.GetMaterialIndex());
  const float radiance_luminance = ComputeLuminance(material.GetRadiance());
  if (radiance_luminance <= 0.f)
  {
    return 0.f;
  }

  switch (light.GetType())
  {
    case LightType::Ambient:
    case LightType::Point:
      return radiance_luminance;
    case LightType::Area:
    {
      const auto* area_light = static_cast<const AreaLight*>(&light);
      const int object_index = area_light->GetObjectIndex();
      if (object_index < 0 || static_cast<size_t>(object_index) >= scene.GetPrimitiveCount())
      {
        return 0.f;
      }

      const Primitive& primitive = scene.GetPrimitive(object_index);
      const auto* mesh = std::get_if<Mesh>(&primitive.Geometry);
      if (mesh == nullptr)
      {
        return 0.f;
      }

      return ComputeMeshArea(*mesh) * radiance_luminance;
    }
  }

  return 0.f;
}

LightSamplingDistribution BuildLightSamplingDistribution(const Scene& scene)
{
  LightSamplingDistribution distribution{};
  std::vector<float> weights{};

  const auto& lights = scene.GetLights();
  distribution.LightIndices.reserve(lights.size());
  weights.reserve(lights.size());

  for (size_t light_index = 0; light_index < lights.size(); ++light_index)
  {
    const Light& light = *lights[light_index];
    if (!IsSupportedDirectLightType(light.GetType()))
    {
      continue;
    }

    distribution.LightIndices.push_back(static_cast<int>(light_index));
    weights.push_back(std::max(ComputeLightWeight(scene, light), 0.f));
  }

  distribution.PDF = NormalizeWeightsToPDF(weights);
  distribution.CDF = BuildCDF(distribution.PDF);
  return distribution;
}

} // namespace

void Scene::Build()
{
  m_AccelerationStructure = GridAccelerationStructure::Create(*this);
  m_LightSamplingDistribution = BuildLightSamplingDistribution(*this);
}

bool Scene::Trace(const Ray& ray, Intersection& intersection) const
{
  intersection.Distance = -1;
  return m_AccelerationStructure.Trace(ray, *this, intersection);
}

bool Scene::Visibility(const Ray& ray, float max_distance) const
{
  Intersection intersection{};
  return !Trace(ray, intersection) || intersection.Distance >= max_distance - EPSILON;
}

void Scene::AddPrimitive(Geometry primitive, int material_index)
{
  assert(material_index >= 0 && static_cast<std::size_t>(material_index) < m_Materials.size() && "Material index out of range");

  m_Primitives.push_back(Primitive{.Geometry = std::move(primitive), .MaterialIndex = material_index});

  auto& material = m_Materials[material_index];
  if (material.GetEmissionPower() > 0.f)
  {
    m_Lights.emplace_back(std::make_unique<AreaLight>(material_index, static_cast<int>(m_Primitives.size() - 1)));
  }
}

int Scene::AddMaterial(MaterialDesc&& desc)
{
  m_Materials.emplace_back(std::move(desc));
  return m_Materials.size() - 1;
}

void Scene::AddLight(std::unique_ptr<Light> light)
{
  m_Lights.emplace_back(std::move(light));
}

void Scene::SetCamera(Camera camera)
{
  m_Camera = std::move(camera);
}

const Camera* Scene::GetCamera() const
{
  return m_Camera.has_value() ? &m_Camera.value() : nullptr;
}

void Scene::SetGlobalMedium(ConstantDensityMedium medium)
{
  m_GlobalMedium = std::move(medium);
}

const ConstantDensityMedium* Scene::GetGlobalMedium() const
{
  return m_GlobalMedium.has_value() && m_GlobalMedium->IsEnabled() ? &m_GlobalMedium.value() : nullptr;
}

RGB Scene::Transmittance(float distance) const
{
  const ConstantDensityMedium* medium = GetGlobalMedium();
  return medium != nullptr ? medium->Transmittance(distance) : RGB{1.0f};
}

const Primitive& Scene::GetPrimitive(int primitive_index) const
{
  return m_Primitives[primitive_index];
}

const Material& Scene::GetMaterial(int material_index) const
{
  return m_Materials[material_index];
}

std::span<const std::unique_ptr<Light>> Scene::GetLights() const
{
  return m_Lights;
}

size_t Scene::GetPrimitiveCount() const
{
  return m_Primitives.size();
}

const LightSamplingDistribution& Scene::GetLightSamplingDistribution() const
{
  return m_LightSamplingDistribution;
}

BoundingBox Scene::ComputeBoundingBox() const
{
  if (m_Primitives.empty())
  {
    return BoundingBox{};
  }

  BoundingBox bounding_box = GetBoundingBox(m_Primitives[0].Geometry);
  for (size_t i = 1; i < m_Primitives.size(); ++i)
  {
    bounding_box.Update(GetBoundingBox(m_Primitives[i].Geometry));
  }

  return bounding_box;
}

} // namespace VI
