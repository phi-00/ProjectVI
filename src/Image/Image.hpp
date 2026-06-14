#pragma once

#include "Math/RGB.hpp"

#include <cstddef>
#include <stdexcept>
#include <vector>

namespace VI
{
class Image final
{
public:
  Image(int width, int height) : m_Width{width}, m_Height{height}, m_Data{static_cast<size_t>(width * height)}
  {
    if (m_Width <= 0 || m_Height <= 0)
    {
      throw std::invalid_argument("Invalid image dimensions");
    }
  }

  Image(int width, int height, const float* color_ptr) : m_Width{width}, m_Height{height}, m_Data{static_cast<size_t>(width * height)}
  {
    SetFromBuffer(color_ptr);
  }

  constexpr int GetWidth() const
  {
    return m_Width;
  }

  constexpr int GetHeight() const
  {
    return m_Height;
  }

  constexpr void Set(int x, int y, const RGB& rgb)
  {
    m_Data[x + y * m_Width] = rgb;
  }

  constexpr const RGB& Get(int x, int y) const
  {
    return m_Data[x + y * m_Width];
  }

  constexpr void Add(int x, int y, const RGB& rgb)
  {
    m_Data[x + y * m_Width] += rgb;
  }

  constexpr float* GetFloatPtr() const
  {
    return (float*)m_Data.data();
  }

private:
  void SetFromBuffer(float const* colorPtr)
  {
    for (int y = 0; y < m_Height; y++)
    {
      for (int x = 0; x < m_Width; x++)
      {
        RGB pix{*colorPtr, *(colorPtr + 1), *(colorPtr + 2)};
        m_Data[x + y * m_Width] = pix;
        colorPtr += 3;
      }
    }
  }

  int m_Width, m_Height;
  std::vector<RGB> m_Data;
};
} // namespace VI
