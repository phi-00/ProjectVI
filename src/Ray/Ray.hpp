#pragma once

#include "Math/Math.hpp"
#include "Math/Vector.hpp"

namespace VI
{
struct Ray
{
  Point Origin;
  Vector Direction;
  // adding time
  float Time = 0.0f;

  static Ray WithOffset(const Point& origin, const Vector& direction, const Vector& normal, float epsilon = EPSILON)
  {
    return {.Origin = OffsetPoint(origin, normal, epsilon), .Direction = direction};
  }

  // new constructor with time
  static Ray WithOffsetTime(const Point& origin, const Vector& direction, const Vector& normal, float time, float epsilon = EPSILON){
    return {.Origin = OffsetPoint(origin, normal, epsilon), .Direction = direction, .Time = time};
  }

  Point At(float t) const{
    return Origin + t * Direction;
  }
};
} // namespace VI
