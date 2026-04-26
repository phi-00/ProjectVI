#include "Camera/Camera.hpp"
#include "Image/FileImages.hpp"
#include "Math/Vector.hpp"
#include "Renderer/Renderer.hpp"
#include "Scene/Scene.hpp"
#include "Scene/SceneBuilder.hpp"
#include "Shaders/AmbientShader.hpp"
#include "Shaders/DirectIllumination.hpp"
#include "Shaders/PathTracingShader.hpp"
#include "Shaders/VeachShader.hpp"
#include "Shaders/WhittedShader.hpp"

#include <chrono>
#include <iostream>

using namespace VI;

int main()
{
  auto begin = std::chrono::system_clock::now();

  constexpr int w = 800;
  constexpr int h = 600;

   /*
    // Path Tracing Cornell Box Camera
    constexpr Point Eye = {278, 273, -800};
    constexpr Point At = {278, 273, 200};
    constexpr Vector Up = {0, 1, 0};
    constexpr float fovH = 40.f;
    constexpr float fovHrad = fovH * 3.14f / 180.f;
    Camera camera{Eye, At, Up, w, h, fovHrad};
  PathTracingShader veach_shader{{0.0f, 0.0f, 0.0f}};
  Scene scene = CreateCornellBox();
   */
    
  // /*
   // Veach Camera
   // Camera for the Veach demo scene: centered composition with the plate stack
     // directly under the square lights and a less dominant floor presence.
     constexpr Point Eye = {0, 15, -15};
     constexpr Point At = {0, 0, 0};
     constexpr Vector Up = {0, 1, 0};
     constexpr float fovH = 45.f;
   
   constexpr float fovHrad = fovH * 3.14f / 180.f;
   Camera camera{Eye, At, Up, w, h, fovHrad};
  VeachShader veach_shader{{0.0f, 0.0f, 0.0f}};
  Scene scene = CreateVeachScene2 ();
  // */
    
  scene.Build();
  Renderer renderer;
  constexpr int spp = 8;
  const auto image = renderer.Render(scene, camera, veach_shader, spp, true);

  ImagePPM::Save(image, "image.ppm");

  auto end = std::chrono::system_clock::now();

  auto duration = std::chrono::duration<double>(end - begin);

  std::cout << "Time it took to render: " << duration.count() << " sec" << '\n';

  return 0;
}
