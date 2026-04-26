#include "Shaders/VeachShader.hpp"

#include "Light/Light.hpp"
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
#include <cstddef>
#include <optional>
#include <variant>

namespace VI
{
namespace
{

// Balance heuristic (beta=1) for two strategies with one sample each
float BalanceHeuristic(float pdf_a, float pdf_b)
{
  if (pdf_a + pdf_b <= 0.f)
    return 0.f;
  return pdf_a / (pdf_a + pdf_b);
}

// Power heuristic (beta=2) for two strategies with one sample each
float PowerHeuristic(float pdf_a, float pdf_b)
{
  const float a2 = pdf_a * pdf_a;
  const float b2 = pdf_b * pdf_b;
  if (a2 + b2 <= 0.f)
    return 0.f;
  return a2 / (a2 + b2);
}

float ComputeTriangleArea(const Triangle& tri)
{
  const auto [v1, v2, v3] = tri.GetVertices();
  return 0.5f * glm::length(glm::cross(v2 - v1, v3 - v1));
}

float ComputeMeshArea(const Mesh& mesh)
{
  float total = 0.f;
  for (size_t i = 0; i < mesh.GetTriangleCount(); ++i)
    total += ComputeTriangleArea(mesh.GetTriangle(i));
  return total;
}

struct MeshSample
{
  Point Position;
  Vector Normal;
  float AreaPDF{0.f};
};

std::optional<MeshSample> SampleMesh(const Mesh& mesh)
{
  const float total_area = ComputeMeshArea(mesh);
  if (total_area <= EPSILON)
    return std::nullopt;

  float target = Random::RandomFloat(0.f, 1.f) * total_area;
  float cumulative = 0.f;
  const Triangle* sampled = nullptr;
  for (size_t i = 0; i < mesh.GetTriangleCount(); ++i)
  {
    const Triangle& tri = mesh.GetTriangle(i);
    cumulative += tri.GetArea();
    if (target <= cumulative || i + 1 == mesh.GetTriangleCount())
    {
      sampled = &tri;
      break;
    }
  }
  if (!sampled)
    return std::nullopt;

  const auto [v1, v2, v3] = sampled->GetVertices();
  const float u1 = Random::RandomFloat(0.f, 1.f);
  const float u2 = Random::RandomFloat(0.f, 1.f);
  const float su1 = glm::sqrt(u1);
  return MeshSample{
      .Position = (1.f - su1) * v1 + su1 * (1.f - u2) * v2 + su1 * u2 * v3,
      .Normal = glm::normalize(sampled->GetNormal()),
      .AreaPDF = 1.f / total_area,
  };
}

int CountAreaLights(const Scene& scene)
{
  int count = 0;
  for (const auto& l : scene.GetLights())
    if (l->GetType() == LightType::Area)
      ++count;
  return count;
}

const AreaLight* GetNthAreaLight(const Scene& scene, int n)
{
  int count = 0;
  for (const auto& l : scene.GetLights())
  {
    if (l->GetType() == LightType::Area)
    {
      if (count == n)
        return static_cast<const AreaLight*>(l.get());
      ++count;
    }
  }
  return nullptr;
}

const AreaLight* FindAreaLightForPrimitive(const Scene& scene, int prim_index)
{
  for (const auto& l : scene.GetLights())
  {
    if (l->GetType() == LightType::Area)
    {
      const auto* al = static_cast<const AreaLight*>(l.get());
      if (al->GetObjectIndex() == prim_index)
        return al;
    }
  }
  return nullptr;
}

} // namespace

RGB VeachShader::Execute(const Ray& ray, const Scene& scene) const
{
  Intersection intersection{};
  if (!scene.Trace(ray, intersection))
    return m_BackgroundColor;

  const Primitive& prim = scene.GetPrimitive(intersection.ObjectIndex);
  const Material& mat = scene.GetMaterial(prim.MaterialIndex);

  if (mat.GetEmissionPower() > 0.f) return mat.GetRadiance();

  if constexpr (kVeachMode == VeachMode::NEEOnly)
  {
    return EvaluateLightStrategy(ray, scene, intersection, mat,
                                 /*apply_mis=*/false);
  }

  else if constexpr (kVeachMode == VeachMode::BRDFOnly)
  {
    return EvaluateBRDFStrategy(ray, scene, intersection, mat,
                                /*apply_mis=*/false);
  }

  else if constexpr (kVeachMode == VeachMode::MIS) {
      // One-sample MIS for the depth-2 direct-light integral: choose one strategy
      // at the primary hit, divide by its selection probability, and weight it
      // against the competing PDF expressed at the same vertex.
      constexpr float p_select = 0.5f;
      RGB radiance;
      if (Random::RandomFloat(0.f, 1.f) < p_select)  {
          radiance = EvaluateLightStrategy(ray, scene, intersection, mat,
                                           /*apply_mis=*/true);
      } else {
          radiance =  EvaluateBRDFStrategy(ray, scene, intersection, mat,
                                           /*apply_mis=*/true);
      }
      return radiance / p_select;
  }
}

RGB VeachShader::EvaluateLightStrategy(const Ray& ray, const Scene& scene, const Intersection& intersection, const Material& material, bool apply_mis) const
{
    const int num_lights = CountAreaLights(scene);
    if (num_lights == 0) return {};
    
    // Uniformly select one area light
    const int selected_idx = std::min(static_cast<int>(Random::RandomFloat(0.f, 1.f) * num_lights), num_lights - 1);
    const AreaLight* area_light = GetNthAreaLight(scene, selected_idx);
    if (!area_light) return {};
    
    const int obj_idx = area_light->GetObjectIndex();
    if (obj_idx < 0 || static_cast<size_t>(obj_idx) >= scene.GetPrimitiveCount()) return {};
    
    const Primitive& light_prim = scene.GetPrimitive(obj_idx);
    const auto* mesh = std::get_if<Mesh>(&light_prim.Geometry);
    if (!mesh) return {};
    
    // get a point in the light's area
    const auto sample = SampleMesh(*mesh);
    if (!sample.has_value() || sample->AreaPDF <= 0.f) return {};
    
    const Vector shading_normal = FaceForward(intersection.Normal, -ray.Direction);
    const Vector to_light = (sample->Position + sample->Normal * EPSILON) - intersection.Position;
    const float dist_sq = glm::dot(to_light, to_light);
    if (dist_sq <= EPSILON * EPSILON) return {};
    
    const float dist = glm::sqrt(dist_sq);
    const Vector wi_world = to_light / dist;
    
    const float cos_surface = glm::dot(shading_normal, wi_world);
    if (cos_surface <= 0.f) return {};
    
    const float cos_light = glm::dot(sample->Normal, -wi_world);
    if (cos_light <= 0.f) return {};
    
    const Ray shadow_ray = Ray::WithOffset(intersection.Position, wi_world, shading_normal);
    if (!scene.Visibility(shadow_ray, dist)) return {};
    
    const OrthonormalBasis basis{shading_normal};
    const Vector wo_local = basis.WorldToLocal(-ray.Direction);
    const Vector wi_local = basis.WorldToLocal(wi_world);
    if (wi_local.z <= 0.f || wo_local.z <= 0.f)  return {};
    
    const RGB f = EvaluateBSDF(wo_local, wi_local, material);
    
    // Solid-angle PDF = p_selection * p_area * d² / cos_light
    const float p_light_sa = (1.f / num_lights) * sample->AreaPDF;
    if (p_light_sa <= 0.f)    return {};
    
    const float G_term = cos_light * cos_surface / dist_sq;
    if (G_term <= 0.f)    return {};
    
    const Material& light_mat = scene.GetMaterial(area_light->GetMaterialIndex());
    const RGB Le = light_mat.GetRadiance();
    
    const float w = 1.f;
    if (apply_mis) {
        const MicrofacetBRDF microfacet{};
        const float p_microfacet_sa =
        microfacet.PDF(wo_local, wi_local, material);
        const float w = PowerHeuristic(p_light_sa, p_microfacet_sa);
    }
    return (f * Le * G_term * w) / p_light_sa;
}

RGB VeachShader::EvaluateBRDFStrategy(const Ray& ray, const Scene& scene, const Intersection& intersection, const Material& material, bool apply_mis) const
{
    const Vector shading_normal = FaceForward(intersection.Normal, -ray.Direction);
    const OrthonormalBasis basis{shading_normal};
    const Vector wo_local = basis.WorldToLocal(-ray.Direction);
    if (wo_local.z <= 0.f) return {};
    
    // Sample the direction according to the GGX lobe
    const MicrofacetBRDF microfacet{};
    const Vector wi_local = microfacet.Sample(wo_local, material);
    const float p_brdf = microfacet.PDF(wo_local, wi_local, material);
    if (wi_local.z <= 0.f || p_brdf <= 0.f) return {};
    
    
    const RGB f = EvaluateBSDF(wo_local, wi_local, material);
    //microfacet.Evaluate(wo_local, wi_local, material);
    
    const float cos_surface = wi_local.z;
    
    // handle the secondary ray now
    const Vector wi_world = glm::normalize(basis.LocalToWorld(wi_local));
    const Ray scattered = Ray::WithOffset(intersection.Position, wi_world, shading_normal);
    
    Intersection secondary{};
    if (!scene.Trace(scattered, secondary)) return {};
    
    const Primitive& sec_prim = scene.GetPrimitive(secondary.ObjectIndex);
    const Material& sec_mat = scene.GetMaterial(sec_prim.MaterialIndex);
    
    if (sec_mat.GetEmissionPower() > 0.f) {
        // Secondary hit is emissive: optionally apply MIS weight
        const RGB Le = sec_mat.GetRadiance();
        
        if (apply_mis) {
            const AreaLight* area_light = FindAreaLightForPrimitive(scene, secondary.ObjectIndex);
            if (area_light)  {
                const int num_lights = CountAreaLights(scene);
                const Primitive& lp = scene.GetPrimitive(area_light->GetObjectIndex());
                const auto* lmesh = std::get_if<Mesh>(&lp.Geometry);
                if (lmesh) {
                    const float mesh_area = ComputeMeshArea(*lmesh);
                    if (mesh_area > EPSILON)    {
                        const Vector light_normal = FaceForward(secondary.Normal, -wi_world);
                        const float cos_light = glm::max(glm::dot(light_normal, -wi_world), 0.f);
                        if (cos_light > 0.f) {
                            const float dist_sq = secondary.Distance * secondary.Distance;
                            const float p_light_sa = 1.f / (num_lights * mesh_area);
                            const float G_term = cos_surface * cos_light / dist_sq  ;
                            const float w = PowerHeuristic(p_brdf, p_light_sa);
                            return (f * Le * G_term * w) / p_brdf;
                        } // cos_light > 0
                    } // mesh_area > EPSILON
                }  // lmesh
            }  // area light
        }  // mis==true
        // apply_mis=false, or couldn't compute light PDF: unweighted
        return (f * Le * cos_surface) / p_brdf;
    } // secondary hit is emissive
    
    // Depth-2 teaching estimator: terminate after the first secondary ray.
    // Non-emissive hits contribute nothing to the direct-light integral.
    return {};
}

} // namespace VI
