#include "Medium/Medium.hpp"

#include "Math/Math.hpp"
#include "Math/Random.hpp"
#include "Ray/Ray.hpp"

#include <glm/common.hpp>
#include <glm/exponential.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace VI
{

ConstantDensityMedium::ConstantDensityMedium(float density, const RGB& scattering_albedo) : m_Density{glm::max(density, 0.0f)}, m_ScatteringAlbedo{glm::clamp(scattering_albedo, RGB{0.0f}, RGB{1.0f})}
{
}

bool ConstantDensityMedium::IsEnabled() const noexcept
{
  return m_Density > EPSILON;
}

float ConstantDensityMedium::GetDensity() const noexcept
{
  return m_Density;
}

RGB ConstantDensityMedium::GetScatteringAlbedo() const noexcept
{
  return m_ScatteringAlbedo;
}

RGB ConstantDensityMedium::Transmittance(float distance) const
{
  if (!IsEnabled() || distance <= 0.0f)
  {
    return RGB{1.0f};
  }

  const float attenuation = glm::exp(-m_Density * distance);
  return RGB{attenuation};
}

std::optional<MediumInteraction> ConstantDensityMedium::SampleInteraction(const Ray& ray, float max_distance) const
{
  if (!IsEnabled() || max_distance <= EPSILON || !std::isfinite(max_distance))
  {
    return std::nullopt;
  }

  const float u = std::clamp(Random::RandomFloat(0.0f, 1.0f), EPSILON, std::nextafter(1.0f, 0.0f));
  const float sampled_distance = -std::log(1.0f - u) / m_Density;
  if (sampled_distance >= max_distance)
  {
    return std::nullopt;
  }

  return MediumInteraction{
      .Position = ray.Origin + sampled_distance * ray.Direction,
      .Distance = sampled_distance,
      .Throughput = m_ScatteringAlbedo,
  };
}

} // namespace VI
