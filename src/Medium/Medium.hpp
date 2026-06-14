#pragma once

#include "Math/RGB.hpp"
#include "Math/Vector.hpp"

#include <optional>

namespace VI
{

struct Ray;

struct MediumInteraction
{
  Point Position{};
  float Distance{0.0f};
  RGB Throughput{0.0f};
};

class ConstantDensityMedium final
{
public:
  ConstantDensityMedium(float density = 0.0f, const RGB& scattering_albedo = RGB{1.0f});

  bool IsEnabled() const noexcept;
  float GetDensity() const noexcept;
  RGB GetScatteringAlbedo() const noexcept;

  RGB Transmittance(float distance) const;
  std::optional<MediumInteraction> SampleInteraction(const Ray& ray, float max_distance) const;

private:
  float m_Density{0.0f};
  RGB m_ScatteringAlbedo{1.0f};
};

} // namespace VI
