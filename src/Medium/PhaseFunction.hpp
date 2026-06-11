#pragma once

#include "Math/Vector.hpp"

namespace VI
{

class IsotropicPhaseFunction final
{
public:
  Vector Sample(const Vector& wo_world) const;
  float Evaluate(const Vector& wo_world, const Vector& wi_world) const;
  float PDF(const Vector& wo_world, const Vector& wi_world) const;
};

} // namespace VI
