#include "Shaders/DirectIllumination.hpp"

#include "Light/Light.hpp"
#include "Math/DiscreteDistribution.hpp"
#include "Math/Math.hpp"
#include "Math/RGB.hpp"
#include "Math/Random.hpp"
#include "Math/Vector.hpp"
#include "Primitive/BRDF.hpp"
#include "Primitive/Geometry/Mesh.hpp"
#include "Primitive/Geometry/Triangle.hpp"
#include "Primitive/Material.hpp"
#include "Primitive/Primitive.hpp"
#include "Ray/Intersection.hpp"
#include "Ray/Ray.hpp"
#include "Scene/Scene.hpp"
#include "glm/common.hpp"
#include "glm/exponential.hpp"
#include "glm/geometric.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <optional>
#include <variant>

namespace VI
{
namespace
{
constexpr float MIN_AREA_LIGHT_DISTANCE_SQUARED = 1e-4f;
constexpr float MIN_AREA_LIGHT_COSINE = 1e-4f;
constexpr float MAX_GEOMETRY_TERM = 50.0f;

struct AreaLightSample
{
  Point Position;
  Vector Normal;
  Vec2 TexCoord{0.f};
  float AreaPDF{0.f};
};

std::optional<AreaLightSample> SampleMeshAreaLight(const Mesh& mesh)
{
  const float total_area = mesh.GetArea();
  if (total_area <= EPSILON)
  {
    return std::nullopt;
  }

  const float target = Random::RandomFloat(0.f, 1.f) * total_area;
  float cumulative_area = 0.f;
  const Triangle* sampled_triangle = nullptr;

  for (size_t i = 0; i < mesh.GetTriangleCount(); ++i)
  {
    const Triangle& triangle = mesh.GetTriangle(i);
    cumulative_area += triangle.GetArea();
    if (target <= cumulative_area || i + 1 == mesh.GetTriangleCount())
    {
      sampled_triangle = &triangle;
      break;
    }
  }

  if (sampled_triangle == nullptr)
  {
    return std::nullopt;
  }

  const auto [v1, v2, v3] = sampled_triangle->GetVertices();
  const auto [uv1, uv2, uv3] = sampled_triangle->GetTexCoords();
  const float u1 = Random::RandomFloat(0.f, 1.f);
  const float u2 = Random::RandomFloat(0.f, 1.f);
  const float su1 = glm::sqrt(u1);
  const float b0 = 1.f - su1;
  const float b1 = su1 * (1.f - u2);
  const float b2 = su1 * u2;

  return AreaLightSample{
      .Position = b0 * v1 + b1 * v2 + b2 * v3,
      .Normal = glm::normalize(sampled_triangle->GetNormal()),
      .TexCoord = b0 * uv1 + b1 * uv2 + b2 * uv3,
      .AreaPDF = 1.f / total_area,
  };
}

SelectedLight SelectUniformLight(const Scene& scene)
{
  const auto& distribution = scene.GetLightSamplingDistribution();
  const int supported_light_count = static_cast<int>(distribution.LightIndices.size());
  if (supported_light_count <= 0)
  {
    return {};
  }

  const int sampled_light_index = std::min(static_cast<int>(Random::RandomFloat(0.f, 1.f) * supported_light_count), supported_light_count - 1);
  const int scene_light_index = distribution.LightIndices[sampled_light_index];

  return {.LightPtr = scene.GetLights()[scene_light_index].get(), .SelectionPDF = 1.0f / supported_light_count};
}

SelectedLight SelectImportanceLight(const Scene& scene)
{
  const auto& distribution = scene.GetLightSamplingDistribution();
  if (!distribution.IsValid())
  {
    return SelectUniformLight(scene);
  }

  const int sampled_light_index = SampleCDFIndex(distribution.CDF, Random::RandomFloat(0.f, 1.f));
  if (sampled_light_index < 0)
  {
    return SelectUniformLight(scene);
  }

  const float selection_pdf = GetPDFValue(distribution.PDF, sampled_light_index);
  if (selection_pdf <= 0.f)
  {
    return SelectUniformLight(scene);
  }

  const int scene_light_index = distribution.LightIndices[sampled_light_index];
  return {.LightPtr = scene.GetLights()[scene_light_index].get(), .SelectionPDF = selection_pdf};
}

} // namespace

RGB EvaluateBSDF(const Vector& wo_local, const Vector& wi_local, const Material& material, const Vec2& tex_coord)
{
  const float specular_weight = material.GetSpecularProbability(tex_coord);
  const float diffuse_weight = 1.0f - specular_weight;
  const LambertianBRDF lambertian{};
  const MicrofacetBRDF microfacet{};
  return diffuse_weight * lambertian.Evaluate(wo_local, wi_local, material, tex_coord) + specular_weight * microfacet.Evaluate(wo_local, wi_local, material, tex_coord);
}

RGB EstimateDirectIllumination(const Ray& ray, const Scene& scene, const Intersection& intersection, const Material& material, const Light* selected_light)
{
  assert(selected_light != nullptr);
  // get radiance
  const Material& light_material = scene.GetMaterial(selected_light->GetMaterialIndex());
  const RGB light_radiance = light_material.GetRadiance();

  // --------- AMBIENT LIGHT
  if (selected_light->GetType() == LightType::Ambient)
  {
    return (material.GetAlbedo(intersection.TexCoord) * light_radiance);
  }

  // --------- POINT LIGHT
  if (selected_light->GetType() == LightType::Point)
  {
    const auto* point_light = static_cast<const PointLight*>(selected_light);

    // light vector and distance
    const Vector to_light = point_light->GetPosition() - intersection.Position;
    const float light_distance = glm::length(to_light);
    if (light_distance <= EPSILON)
    {
      return RGB{0.f};
    }

    const Vector wi_world = to_light / light_distance;
    const Ray shadow_ray = Ray::WithOffset(intersection.Position, wi_world, intersection.Normal);
    // In shadow ?
    if (!scene.Visibility(shadow_ray, light_distance))
    {
      return RGB{0.f};
    }

    // create object reference frame around normal
    const OrthonormalBasis basis{intersection.Normal};
    const Vector wo_world = -ray.Direction;
    const Vector wo_local = basis.WorldToLocal(wo_world);
    const Vector wi_local = basis.WorldToLocal(wi_world);

    if (wi_local.z <= 0.f || wo_local.z <= 0.f)
    {
      return RGB{0.f};
    }

    // evaluate BSDF
    const RGB bsdf = EvaluateBSDF(wo_local, wi_local, material, intersection.TexCoord);

    // Lr = bsdf * radiance * cos (theta)
    return (bsdf * wi_local.z * light_radiance * scene.Transmittance(light_distance));
  }

  // --------- AREA LIGHT
  if (selected_light->GetType() == LightType::Area)
  {
    const auto* area_light = static_cast<const AreaLight*>(selected_light);
    const int object_index = area_light->GetObjectIndex();
    if (object_index < 0 || static_cast<size_t>(object_index) >= scene.GetPrimitiveCount())
    {
      return RGB{0.f};
    }

    const Primitive& light_primitive = scene.GetPrimitive(object_index);
    const auto* mesh = std::get_if<Mesh>(&light_primitive.Geometry);
    if (mesh == nullptr)
    {
      return RGB{0.f};
    }

    // select a point in light source area
    const auto area_sample = SampleMeshAreaLight(*mesh);
    if (!area_sample.has_value() || area_sample->AreaPDF <= 0.f)
    {
      return RGB{0.f};
    }

    // light vector
    const Vector to_light = (area_sample->Position + (area_sample->Normal * EPSILON)) - intersection.Position;
    const float distance_squared = glm::dot(to_light, to_light);
    if (distance_squared <= MIN_AREA_LIGHT_DISTANCE_SQUARED)
    {
      return RGB{0.f};
    }
    const float light_distance = glm::sqrt(distance_squared);
    const Vector wi_world = to_light / light_distance;
    const Vector shading_normal = FaceForward(intersection.Normal, -ray.Direction);

    const float cos_surface = glm::max(glm::dot(shading_normal, wi_world), 0.f);
    if (cos_surface <= 0.f)
    {
      return RGB{0.f};
    }

    const float cos_light = glm::max(glm::dot(area_sample->Normal, -wi_world), 0.f);
    if (cos_light <= MIN_AREA_LIGHT_COSINE)
    {
      return RGB{0.f};
    }

    // shadow ray
    const Ray shadow_ray = Ray::WithOffset(intersection.Position, wi_world, shading_normal);
    // in shadow ?
    if (!scene.Visibility(shadow_ray, light_distance))
    {
      return RGB{0.f};
    }

    // object local reference frame (around normal)
    const OrthonormalBasis basis{shading_normal};
    const Vector wo_world = -ray.Direction;
    const Vector wo_local = basis.WorldToLocal(wo_world);
    const Vector wi_local = basis.WorldToLocal(wi_world);
    if (wi_local.z <= 0.f || wo_local.z <= 0.f)
    {
      return RGB{0.f};
    }

    const RGB bsdf = EvaluateBSDF(wo_local, wi_local, material, intersection.TexCoord);

    // G = cos (theta_N) * cos (theta_L) / d^2
    const float geometry_term = glm::min((cos_surface * cos_light) / distance_squared, MAX_GEOMETRY_TERM);

    const RGB area_light_radiance = light_material.HasEmissionTexture() ? light_material.GetRadiance(area_sample->TexCoord) : light_radiance;

    // Lr = bsdf * L * G / pdf
    return (bsdf * area_light_radiance * geometry_term * scene.Transmittance(light_distance)) / area_sample->AreaPDF;
  }

  return RGB{0.f};
}

RGB SampleDirectIllumination(const Ray& ray, const Scene& scene, const Intersection& intersection, const Material& material, const DirectIlluminationMode mode)
{
  const auto& distribution = scene.GetLightSamplingDistribution();
  const int supported_light_count = static_cast<int>(distribution.LightIndices.size());
  if (supported_light_count <= 0)
  {
    return RGB{0.f};
  }

  switch (mode)
  {
    case DirectIlluminationMode::All:
    {
      RGB direct_lighting{0.f};

      for (const int light_index : distribution.LightIndices)
      {
        const Light* light = scene.GetLights()[light_index].get();
        direct_lighting += EstimateDirectIllumination(ray, scene, intersection, material, light);
      }

      return direct_lighting;
    }
    case DirectIlluminationMode::Uniform:
    {
      const SelectedLight selected_light = SelectUniformLight(scene);
      if (selected_light.LightPtr == nullptr || selected_light.SelectionPDF <= 0.f)
      {
        return RGB{0.f};
      }

      return EstimateDirectIllumination(ray, scene, intersection, material, selected_light.LightPtr) / selected_light.SelectionPDF;
    }
    case DirectIlluminationMode::Importance:
    {
      const SelectedLight selected_light = SelectImportanceLight(scene);
      if (selected_light.LightPtr == nullptr || selected_light.SelectionPDF <= 0.f)
      {
        return RGB{0.f};
      }

      return EstimateDirectIllumination(ray, scene, intersection, material, selected_light.LightPtr) / selected_light.SelectionPDF;
    }
  }

  return RGB{0.f};
}

} // namespace VI
