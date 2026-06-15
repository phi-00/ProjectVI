#include "Camera/Camera.hpp"

#include "Math/Random.hpp"
#include "Math/Vector.hpp"
#include "Ray/Ray.hpp"

#include <glm/ext/vector_float2.hpp>
#include <glm/geometric.hpp>

#include <cmath>

namespace VI
{
Camera::Camera(Point eye, Point at, Vector up, int width, int height, float fov_h, float defocus_angle, float focus_dist, float time1, float time2) : 
  m_Eye{eye}, m_At{at}, m_Up{up}, m_Width{width}, m_Height{height}, m_DefocusAngle{defocus_angle}, m_Time1{time1}, m_Time2{time2}
{

  Vector forward = glm::normalize(m_At - m_Eye);
  Vector right = glm::normalize(glm::cross(forward, up));

  m_Up = glm::normalize(glm::cross(right, forward));

  float tan_halfh = std::tan(fov_h / 2.f);
  float viewport_height = 2.0 * tan_halfh * focus_dist;
  float viewport_width = viewport_height * m_Width / m_Height;

  Vector viewport_u = viewport_width * right;
  Vector viewport_v = -viewport_height * m_Up;

  m_PixelDeltaU = viewport_u / static_cast<float>(m_Width);
  m_PixelDeltaV = viewport_v / static_cast<float>(m_Height);

  Point viewport_upper_left = m_Eye + focus_dist * forward;
  viewport_upper_left = viewport_upper_left - (viewport_u / 2.f) - (viewport_v / 2.f);
  m_Pixel00Location = viewport_upper_left + 0.5f * (m_PixelDeltaU + m_PixelDeltaV);

  float defocus_radius = focus_dist * std::tan(defocus_angle / 2.f);

  m_DefocusDiskRight = right * defocus_radius;
  m_DefocusDiskUp = m_Up * defocus_radius;
}

Ray Camera::GenerateRay(int x, int y, glm::vec2 jitter) const
{
  Point pc{x + jitter.x, y + jitter.y, 0};

  Point pixel_sample = m_Pixel00Location + (pc.x * m_PixelDeltaU) + (pc.y * m_PixelDeltaV);

  Point origin = m_Eye;

  if (m_DefocusAngle > 0.f)
  {
    Point p = Random::RandomInUnitDisk();
    origin = m_Eye + p.x * m_DefocusDiskRight + p.y * m_DefocusDiskUp;
  }

  Vector direction = glm::normalize(pixel_sample - origin);

  float time = Random::RandomFloat(m_Time1, m_Time2);

  return {.Origin = origin, .Direction = direction, .Time = time};
}

} // namespace VI
