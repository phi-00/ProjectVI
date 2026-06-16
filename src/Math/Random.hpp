#pragma once

#include "Math/Vector.hpp"

#include <cstdint>
#include <random>

namespace VI
{
class Random
{
public:
  static void Seed(uint64_t seed);
  static float RandomFloat(float min = 0.f, float max = 1.f);
  static Vector RandomVec3(float min = -1.f, float max = 1.f);
  static Point RandomInUnitDisk();

private:
  static thread_local std::mt19937 s_Rng;
};
} // namespace VI
