#include "Shaders/PathTracingShader.hpp"

#include "Light/Light.hpp"
#include "Medium/Medium.hpp"
#include "Medium/PhaseFunction.hpp"
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
#include "Shaders/DirectIllumination.hpp"

#include <glm/common.hpp>
#include <glm/exponential.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <optional>
#include <variant>

// Balance heuristic (beta=1) for two strategies with one sample each
float BalanceHeuristic(float pdf_a, float pdf_b)
{
  const float denom = pdf_a + pdf_b;
  if (denom <= 0.f)
    return 0.f;
  return pdf_a / denom;
}

// Power heuristic (beta=2) for two strategies with one sample each
float PowerHeuristic(float pdf_a, float pdf_b)
{
    const float pa2 = pdf_a * pdf_a;
    const float pb2 = pdf_b * pdf_b;
  const float denom = pa2 + pb2;
  if (denom <= 0.f)
    return 0.f;
  return pa2 / denom;
}

namespace VI
{
constexpr float MAX_DEPTH = 5;
constexpr int RUSSIAN_ROULETTE_DEPTH = 2;
constexpr float MAX_SAMPLE_RADIANCE = 10.0f;
constexpr float MIN_AREA_LIGHT_DISTANCE_SQUARED = 1e-4f;
constexpr float MIN_AREA_LIGHT_COSINE = 1e-4f;

namespace
{
struct MediumLightSample
{
  Point Position;
  Vector Normal;
  Vec2 TexCoord{0.0f};
  float AreaPDF{0.0f};
};

std::optional<MediumLightSample> SampleMediumMeshAreaLight(const Mesh& mesh)
{
  const float total_area = mesh.GetArea();
  if (total_area <= EPSILON)
  {
    return std::nullopt;
  }

  const float target = Random::RandomFloat(0.0f, 1.0f) * total_area;
  float cumulative_area = 0.0f;
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
  const float u1 = Random::RandomFloat(0.0f, 1.0f);
  const float u2 = Random::RandomFloat(0.0f, 1.0f);
  const float su1 = glm::sqrt(u1);
  const float b0 = 1.0f - su1;
  const float b1 = su1 * (1.0f - u2);
  const float b2 = su1 * u2;

  return MediumLightSample{
      .Position = b0 * v1 + b1 * v2 + b2 * v3,
      .Normal = glm::normalize(sampled_triangle->GetNormal()),
      .TexCoord = b0 * uv1 + b1 * uv2 + b2 * uv3,
      .AreaPDF = 1.0f / total_area,
  };
}

SelectedLight SelectUniformMediumLight(const Scene& scene)
{
  const auto& distribution = scene.GetLightSamplingDistribution();
  const int supported_light_count = static_cast<int>(distribution.LightIndices.size());
  if (supported_light_count <= 0)
  {
    return {};
  }

  const int sampled_light_index = std::min(static_cast<int>(Random::RandomFloat(0.0f, 1.0f) * supported_light_count), supported_light_count - 1);
  const int scene_light_index = distribution.LightIndices[sampled_light_index];
  return {.LightPtr = scene.GetLights()[scene_light_index].get(), .SelectionPDF = 1.0f / supported_light_count};
}

SelectedLight SelectImportanceMediumLight(const Scene& scene)
{
  const auto& distribution = scene.GetLightSamplingDistribution();
  if (!distribution.IsValid())
  {
    return SelectUniformMediumLight(scene);
  }

  const int sampled_light_index = SampleCDFIndex(distribution.CDF, Random::RandomFloat(0.0f, 1.0f));
  if (sampled_light_index < 0)
  {
    return SelectUniformMediumLight(scene);
  }

  const float selection_pdf = GetPDFValue(distribution.PDF, sampled_light_index);
  if (selection_pdf <= 0.0f)
  {
    return SelectUniformMediumLight(scene);
  }

  const int scene_light_index = distribution.LightIndices[sampled_light_index];
  return {.LightPtr = scene.GetLights()[scene_light_index].get(), .SelectionPDF = selection_pdf};
}

RGB EstimateMediumDirectIllumination(const Ray& ray, const Scene& scene, const Point& position, const Light* selected_light)
{
  assert(selected_light != nullptr);

  const IsotropicPhaseFunction phase_function{};
  const Material& light_material = scene.GetMaterial(selected_light->GetMaterialIndex());
  const RGB light_radiance = light_material.GetRadiance();

  if (selected_light->GetType() == LightType::Ambient)
  {
    const float phase = phase_function.Evaluate(-ray.Direction, Vector{0.0f});
    return phase * light_radiance;
  }

  if (selected_light->GetType() == LightType::Point)
  {
    const auto* point_light = static_cast<const PointLight*>(selected_light);
    const Vector to_light = point_light->GetPosition() - position;
    const float light_distance = glm::length(to_light);
    if (light_distance <= EPSILON)
    {
      return RGB{0.0f};
    }

    const Vector wi_world = to_light / light_distance;
    const float phase = phase_function.Evaluate(-ray.Direction, wi_world);
    const Ray shadow_ray = Ray::WithOffset(position, wi_world, wi_world);
    if (!scene.Visibility(shadow_ray, light_distance))
    {
      return RGB{0.0f};
    }

    return phase * light_radiance * scene.Transmittance(light_distance);
  }

  if (selected_light->GetType() == LightType::Area)
  {
    const auto* area_light = static_cast<const AreaLight*>(selected_light);
    const int object_index = area_light->GetObjectIndex();
    if (object_index < 0 || static_cast<size_t>(object_index) >= scene.GetPrimitiveCount())
    {
      return RGB{0.0f};
    }

    const Primitive& light_primitive = scene.GetPrimitive(object_index);
    const auto* mesh = std::get_if<Mesh>(&light_primitive.Geometry);
    if (mesh == nullptr)
    {
      return RGB{0.0f};
    }

    const auto area_sample = SampleMediumMeshAreaLight(*mesh);
    if (!area_sample.has_value() || area_sample->AreaPDF <= 0.0f)
    {
      return RGB{0.0f};
    }

    const Vector to_light = (area_sample->Position + area_sample->Normal * EPSILON) - position;
    const float distance_squared = glm::dot(to_light, to_light);
    if (distance_squared <= MIN_AREA_LIGHT_DISTANCE_SQUARED)
    {
      return RGB{0.0f};
    }

    const float light_distance = glm::sqrt(distance_squared);
    const Vector wi_world = to_light / light_distance;
    const float phase = phase_function.Evaluate(-ray.Direction, wi_world);
    const float cos_light = glm::max(glm::dot(area_sample->Normal, -wi_world), 0.0f);
    if (cos_light <= MIN_AREA_LIGHT_COSINE)
    {
      return RGB{0.0f};
    }

    const Ray shadow_ray = Ray::WithOffset(position, wi_world, wi_world);
    if (!scene.Visibility(shadow_ray, light_distance))
    {
      return RGB{0.0f};
    }

    const RGB area_light_radiance = light_material.HasEmissionTexture() ? light_material.GetRadiance(area_sample->TexCoord) : light_radiance;
    return (phase * area_light_radiance * cos_light * scene.Transmittance(light_distance)) / (distance_squared * area_sample->AreaPDF);
  }

  return RGB{0.0f};
}

} // namespace

RGB ClampRadiance(const RGB& radiance)
{
  return glm::clamp(radiance, RGB{0.0f}, RGB{MAX_SAMPLE_RADIANCE});
}

RGB PathTracingShader::Execute(const Ray& ray, const Scene& scene) const
{
  return TracePath(ray, scene);
}

RGB PathTracingShader::TracePath(const Ray& ray, const Scene& scene, int depth, bool allow_emissive) const
{
  if (depth > MAX_DEPTH)
  {
    return RGB{0.0f};
  }

  Intersection intersection{};
  if (!scene.Trace(ray, intersection))
  {
    return m_BackgroundColor;
  }

  if (const ConstantDensityMedium* medium = scene.GetGlobalMedium())
  {
    if (const auto medium_interaction = medium->SampleInteraction(ray, intersection.Distance))
    {
      const IsotropicPhaseFunction phase_function{};
      const Vector wo_world = -ray.Direction;

      RGB color = MediumDirectIllumination(ray, scene, medium_interaction->Position);

      const Vector wi_world = phase_function.Sample(wo_world);
      const float phase_pdf = phase_function.PDF(wo_world, wi_world);
      if (phase_pdf > 0.0f)
      {
        const float phase_value = phase_function.Evaluate(wo_world, wi_world);
        float continuation_probability = 1.0f;
        if (depth >= RUSSIAN_ROULETTE_DEPTH)
        {
          continuation_probability = glm::clamp(std::max(medium_interaction->Throughput.x, std::max(medium_interaction->Throughput.y, medium_interaction->Throughput.z)), 0.05f, 0.95f);
          if (Random::RandomFloat(0.0f, 1.0f) >= continuation_probability)
          {
            return medium_interaction->Throughput * color;
          }
        }

        const Ray scattered_ray = Ray::WithOffset(medium_interaction->Position, wi_world, wi_world);
        color += phase_value * TracePath(scattered_ray, scene, depth + 1, false) / (phase_pdf * continuation_probability);
      }

      return ClampRadiance(medium_interaction->Throughput * color);
    }
  }

  return DoExecute(ray, scene, intersection, depth, allow_emissive);
}

RGB PathTracingShader::DoExecute(const Ray& ray, const Scene& scene, const Intersection& intersection, int depth, bool allow_emissive) const
{
  RGB color{0.0f};
  if (depth > MAX_DEPTH)
  {
    return color;
  }

  const Primitive& primitive = scene.GetPrimitive(intersection.ObjectIndex);
  const Material& material = scene.GetMaterial(primitive.MaterialIndex);

  if (material.GetEmissionPower() > 0.0f)
  {
    const RGB radiance = material.GetRadiance(intersection.TexCoord);
    if (std::max(radiance.x, std::max(radiance.y, radiance.z)) > 0.0f)
    {
      return allow_emissive ? radiance : RGB{0.0f};
    }
  }

  color += IndirectIllumination(ray, scene, intersection, material, depth, allow_emissive);

  color += DirectIllumination(ray, scene, intersection);

  return color;
}

RGB PathTracingShader::DirectIllumination(const Ray& ray, const Scene& scene, const Intersection& intersection) const
{
  const auto& object = scene.GetPrimitive(intersection.ObjectIndex);
  const auto& material = scene.GetMaterial(object.MaterialIndex);

  return SampleDirectIllumination(ray, scene, intersection, material, m_DirectIlluminationMode);
}

RGB PathTracingShader::MediumDirectIllumination(const Ray& ray, const Scene& scene, const Point& position) const
{
  const auto& distribution = scene.GetLightSamplingDistribution();
  const int supported_light_count = static_cast<int>(distribution.LightIndices.size());
  if (supported_light_count <= 0)
  {
    return RGB{0.0f};
  }

  switch (m_DirectIlluminationMode)
  {
    case DirectIlluminationMode::All:
    {
      RGB direct_lighting{0.0f};
      for (const int light_index : distribution.LightIndices)
      {
        direct_lighting += EstimateMediumDirectIllumination(ray, scene, position, scene.GetLights()[light_index].get());
      }
      return direct_lighting;
    }
    case DirectIlluminationMode::Uniform:
    {
      const SelectedLight selected_light = SelectUniformMediumLight(scene);
      if (selected_light.LightPtr == nullptr || selected_light.SelectionPDF <= 0.0f)
      {
        return RGB{0.0f};
      }
      return EstimateMediumDirectIllumination(ray, scene, position, selected_light.LightPtr) / selected_light.SelectionPDF;
    }
    case DirectIlluminationMode::Importance:
    {
      const SelectedLight selected_light = SelectImportanceMediumLight(scene);
      if (selected_light.LightPtr == nullptr || selected_light.SelectionPDF <= 0.0f)
      {
        return RGB{0.0f};
      }
      return EstimateMediumDirectIllumination(ray, scene, position, selected_light.LightPtr) / selected_light.SelectionPDF;
    }
  }

  return RGB{0.0f};
}

RGB PathTracingShader::IndirectIllumination(const Ray& ray, const Scene& scene, const Intersection& intersection, const Material& material, int depth, bool allow_emissive [[maybe_unused]]) const
{
  const Vector shading_normal = FaceForward(intersection.Normal, -ray.Direction);
  const OrthonormalBasis basis{shading_normal};
  const Vector wo_local = basis.WorldToLocal(-ray.Direction);
  if (wo_local.z <= 0.f)
  {
    return RGB{0.0f};
  }

  MfacetLambertBRDF microfacetBRDF{};
  // the probability of selecting the specular (GGX) path is BRDF dependent
  const float microfacet_probability = material.GetSpecularProbability(intersection.TexCoord);
  const float diffuse_probability = 1.0f - microfacet_probability;

  // stochastically select whether to sample the direction according to specular (microfacet) or diffuse (lambertian)
  const bool sample_microfacet = Random::RandomFloat(0.f, 1.f) < microfacet_probability;
    
  // sample the direction according to the selected BRDF mode
  const Vector wi_local = microfacetBRDF.Sample(
            wo_local, material,
            sample_microfacet ? MODE::GGX_MODE : MODE::LAMBERT_MODE,
            intersection.TexCoord);
  if (wi_local.z <= 0.f)    return RGB{0.0f};

  // get the BRDF value
  RGB const f = microfacetBRDF.Evaluate(wo_local, wi_local, material, intersection.TexCoord);

  // get the 2 probabilities with which the direction
  // could have been sampled
  const float diffuse_pdf = microfacetBRDF.PDF(wo_local, wi_local, material, MODE::LAMBERT_MODE, intersection.TexCoord);
  const float microfacet_pdf = microfacetBRDF.PDF(wo_local, wi_local, material, MODE::GGX_MODE, intersection.TexCoord);
  // this is the actual mixture probability with which this direction could
  // have been sampled by either BRDF branch.
  const float pdf = microfacet_probability * microfacet_pdf + diffuse_probability * diffuse_pdf;
  if (pdf <= 0.f)    return RGB{0.0f};
     
  const float cos_theta = wi_local.z;
  const RGB throughput = f * cos_theta / pdf;
    
  // Russian Roulette
  float continuation_probability = 1.0f;
  if (depth >= RUSSIAN_ROULETTE_DEPTH)
  {
    continuation_probability = glm::clamp(std::max(throughput.x, std::max(throughput.y, throughput.z)), 0.05f, 0.95f);
    if (Random::RandomFloat(0.f, 1.f) >= continuation_probability)      return RGB{0.0f};
  }

  // follow up ray
  const Vector wi_world = glm::normalize(basis.LocalToWorld(wi_local));
  const Ray scattered_ray = Ray::WithOffset(intersection.Position, wi_world, shading_normal);

  // Direct light sampling handles non-primary emitter contributions. Keep
  // primary emitter visibility, but avoid randomly accepting light hits based
  // on which BRDF branch happened to generate the scattered ray.
  constexpr bool allow_emissive_on_indirect_hit = false;
  const RGB incoming_radiance = TracePath(scattered_ray, scene, depth + 1, allow_emissive_on_indirect_hit);

  return ClampRadiance(throughput * incoming_radiance / continuation_probability);
}

} // namespace VI
