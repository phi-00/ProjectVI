#pragma once

#include "Math/RGB.hpp"
#include "Shaders/Shader.hpp"

namespace VI
{
struct Ray;
struct Intersection;
class Material;
class Scene;

enum class VeachMode
{
  BRDFOnly, // Depth-2 direct lighting via BRDF sampling only
  NEEOnly,  // Depth-2 direct lighting via light sampling only
  MIS,      // Depth-2 direct lighting with 50/50 one-sample MIS
};

// Change this constant to switch rendering mode
constexpr VeachMode kVeachMode = VeachMode::MIS;

class VeachShader final
{
public:
  explicit VeachShader(const RGB& background_color = {}) : m_BackgroundColor{background_color} {}

  RGB Execute(const Ray& ray, const Scene& scene) const;

private:
  // Returns the first-bounce direct-light contribution estimated by sampling
  // a light source at the primary hit. If apply_mis=true, applies MIS weights.
  RGB EvaluateLightStrategy(const Ray& ray, const Scene& scene, const Intersection& intersection, const Material& material, bool apply_mis) const;

  // Returns the first-bounce direct-light contribution estimated by sampling
  // the BRDF at the primary hit and tracing exactly one secondary ray.
  // If the secondary ray does not hit an emissive surface, the contribution is
  // zero. If apply_mis=true, emissive hits are MIS-weighted.
  RGB EvaluateBRDFStrategy(const Ray& ray, const Scene& scene, const Intersection& intersection, const Material& material, bool apply_mis) const;

  RGB m_BackgroundColor;
};

static_assert(Shader<VeachShader>);
} // namespace VI
