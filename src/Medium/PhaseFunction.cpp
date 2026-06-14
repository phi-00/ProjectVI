#include "Medium/PhaseFunction.hpp"

#include "Math/Random.hpp"

#include <glm/common.hpp>
#include <glm/ext/scalar_constants.hpp>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

namespace VI
{
namespace
{
constexpr float INV_FOUR_PI = 1.0f / (4.0f * glm::pi<float>());
}

Vector IsotropicPhaseFunction::Sample(const Vector& wo_world [[maybe_unused]]) const
{
  const float u1 = Random::RandomFloat(0.0f, 1.0f);
  const float u2 = Random::RandomFloat(0.0f, 1.0f);

  const float z = 1.0f - 2.0f * u1;
  const float r = glm::sqrt(glm::max(0.0f, 1.0f - z * z));
  const float phi = 2.0f * glm::pi<float>() * u2;

  return Vector{r * glm::cos(phi), r * glm::sin(phi), z};
}

float IsotropicPhaseFunction::Evaluate(const Vector& wo_world [[maybe_unused]], const Vector& wi_world [[maybe_unused]]) const
{
  return INV_FOUR_PI;
}

float IsotropicPhaseFunction::PDF(const Vector& wo_world [[maybe_unused]], const Vector& wi_world [[maybe_unused]]) const
{
  return INV_FOUR_PI;
}

} // namespace VI
