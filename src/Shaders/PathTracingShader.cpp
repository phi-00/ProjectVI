#include "Shaders/PathTracingShader.hpp"

#include "Math/Math.hpp"
#include "Math/RGB.hpp"
#include "Math/Random.hpp"
#include "Math/Vector.hpp"
#include "Primitive/BRDF.hpp"
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

namespace VI
{
constexpr float MAX_DEPTH = 5;
constexpr int RUSSIAN_ROULETTE_DEPTH = 2;

RGB PathTracingShader::Execute(const Ray& ray, const Scene& scene) const
{
  Intersection intersection{};
  if (!scene.Trace(ray, intersection))
  {
    return m_BackgroundColor;
  }

  return DoExecute(ray, scene, intersection);
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
    return allow_emissive ? material.GetRadiance() : RGB{0.0f};
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

RGB PathTracingShader::IndirectIllumination(const Ray& ray, const Scene& scene, const Intersection& intersection, const Material& material, int depth, bool allow_emissive [[maybe_unused]]) const
{
  const Vector shading_normal = FaceForward(intersection.Normal, -ray.Direction);
  const OrthonormalBasis basis{shading_normal};
  const Vector wo_local = basis.WorldToLocal(-ray.Direction);
  if (wo_local.z <= 0.f)
  {
    return RGB{0.0f};
  }

  const LambertianBRDF lambertian{};
  const MicrofacetBRDF microfacet{};
  // the probability of selecting the specular (GGX) path is BRDF dependent
  const float microfacet_probability = material.GetSpecularProbability();

  // stochastically select whether to sample the direction according to specular (microfacet) or diffuse (lambertian)
  const bool sample_microfacet = Random::RandomFloat(0.f, 1.f) < microfacet_probability;
    
  const Vector wi_local = sample_microfacet ?
    microfacet.Sample(wo_local, material) :
    lambertian.Sample(wo_local, material);
  if (wi_local.z <= 0.f)
  {
    return RGB{0.0f};
  }

  RGB const f = EvaluateBSDF(wo_local, wi_local, material);

  const float diffuse_pdf = lambertian.PDF(wo_local, wi_local, material);
  const float microfacet_pdf = microfacet.PDF(wo_local, wi_local, material);
  const float pdf = diffuse_pdf + microfacet_pdf;
  if (pdf <= 0.f)
  {
    return RGB{0.0f};
  }

  const float cos_theta = wi_local.z;
  const RGB throughput = (f * cos_theta) / pdf;
  float continuation_probability = 1.0f;
  if (depth >= RUSSIAN_ROULETTE_DEPTH)
  {
    continuation_probability = glm::clamp(std::max(throughput.x, std::max(throughput.y, throughput.z)), 0.05f, 0.95f);
    if (Random::RandomFloat(0.f, 1.f) >= continuation_probability)
    {
      return RGB{0.0f};
    }
  }

  const Vector wi_world = glm::normalize(basis.LocalToWorld(wi_local));
  const Ray scattered_ray = Ray::WithOffset(intersection.Position, wi_world, shading_normal);

  Intersection scattered_intersection{};
  RGB incoming_radiance = m_BackgroundColor;
  if (scene.Trace(scattered_ray, scattered_intersection))
  {
    const bool next_allow_emissive = sample_microfacet;
    incoming_radiance = DoExecute(scattered_ray, scene, scattered_intersection, depth + 1, next_allow_emissive);
  }

  return throughput * incoming_radiance / continuation_probability;
}

} // namespace VI
