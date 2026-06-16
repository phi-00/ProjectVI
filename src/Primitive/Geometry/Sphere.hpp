#pragma once

#include "Math/Vector.hpp"
#include "Primitive/BoundingBox.hpp"

#include "glm/common.hpp"

namespace VI
{
struct Ray;
struct Intersection;

class Sphere final
{
public:
  // static sphere
  Sphere(Point center, float radius) : m_Center(Ray{.Origin=center, .Direction={0,0,0}}), m_Radius(radius), m_BoundingBox(center - radius, center + radius) {}
  // moving sphere
  Sphere(Point center1, Point center2, float radius) : m_Center(Ray{.Origin=center1, .Direction=center2 - center1}), m_Radius(radius), m_BoundingBox((glm::min(center1, center2) - radius, glm::max(center1, center2) + radius)) {}

  bool Intersect(const Ray& r, Intersection& i) const;
  inline const BoundingBox& GetBoundingBox() const
  {
    return m_BoundingBox;
  }

private:
  Ray m_Center;
  float m_Radius;
  BoundingBox m_BoundingBox;
};
} // namespace VI
