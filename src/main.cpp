#include <chrono>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include "Camera/Camera.hpp"
#include "Image/FileImages.hpp"
#include "Image/Image.hpp"
#include "Medium/Medium.hpp"
#include "Math/Vector.hpp"
#include "Renderer/Renderer.hpp"
#include "Scene/Scene.hpp"
#include "Scene/SceneBuilder.hpp"
#include "Shaders/AmbientShader.hpp"
#include "Shaders/DirectIllumination.hpp"
#include "Shaders/PathTracingShader.hpp"
#include "Shaders/VeachShader.hpp"
#include "Shaders/WhittedShader.hpp"
#include "Window/RenderWindow.hpp"
#include "Renderer/ProgressiveRenderer.hpp"

/*  uncomment the folowiing line to perform
    post-rendering denoising (Intel OIDN)
 */
#define __DENOISE__
#if defined(__DENOISE__)
#include "Image/Denoiser.hpp"
#endif

using namespace VI;

namespace
{

struct CommandLineOptions
{
  std::optional<std::filesystem::path> ScenePath = std::nullopt;
  int SamplesPerPixel = 128;
  //int SamplesPerPixel = 100;
  int Width = 1280;
  //int Width = 400;
  int Height = 720;
  bool MediumDemo = false;
  std::optional<float> MediumDensity = std::nullopt;
  bool Progressive = false;
};

CommandLineOptions ParseCommandLine(int argc, char** argv)
{
  CommandLineOptions options{};
  for (int i = 1; i < argc; ++i)
  {
    const std::string_view arg{argv[i]};
    if (arg == "--scene")
    {
      if (i + 1 >= argc)
      {
        throw std::invalid_argument("--scene requires a glTF path");
      }
      options.ScenePath = std::filesystem::path{argv[++i]};
      continue;
    }

    if (arg == "--spp")
    {
      if (i + 1 >= argc)
      {
        throw std::invalid_argument("--spp requires a positive sample count");
      }
      options.SamplesPerPixel = std::stoi(argv[++i]);
      if (options.SamplesPerPixel <= 0)
      {
        throw std::invalid_argument("--spp requires a positive sample count");
      }
      continue;
    }

    if (arg == "--width")
    {
      if (i + 1 >= argc)
      {
        throw std::invalid_argument("--width requires a positive value");
      }
      options.Width = std::stoi(argv[++i]);
      if (options.Width <= 0)
      {
        throw std::invalid_argument("--width requires a positive value");
      }
      continue;
    }

    if (arg == "--height")
    {
      if (i + 1 >= argc)
      {
        throw std::invalid_argument("--height requires a positive value");
      }
      options.Height = std::stoi(argv[++i]);
      if (options.Height <= 0)
      {
        throw std::invalid_argument("--height requires a positive value");
      }
      continue;
    }

    if (arg == "--medium-demo")
    {
      options.MediumDemo = true;
      continue;
    }

    if (arg == "--progressive")
    {
      options.Progressive = true;
      continue;
    }

    if (arg == "--medium-density")
    {
      if (i + 1 >= argc)
      {
        throw std::invalid_argument("--medium-density requires a positive value");
      }
      options.MediumDensity = std::stof(argv[++i]);
      if (*options.MediumDensity <= 0.0f)
      {
        throw std::invalid_argument("--medium-density requires a positive value");
      }
      continue;
    }

    if(arg == "--motion-blur-demo"){
      options.MotionBlurDemo = true;
      continue;
    }

    throw std::invalid_argument("Unknown argument: " + std::string{arg});
  }
  return options;
}

} // namespace

int main(int argc, char** argv)
{
  auto begin = std::chrono::system_clock::now();
  const auto options = ParseCommandLine(argc, argv);

  const int w = options.Width;
  const int h = options.Height;

  // /*
  // Path Tracing Cornell Box Camera
  // constexpr Point Eye = {278, 273, -800};
  // constexpr Point At = {278, 273, 200};
  // constexpr Vector Up = {0, 1, 0};
  // constexpr float fovH = 40.f;
  // constexpr float fovHrad = fovH * 3.14f / 180.f;
  // Camera camera{Eye, At, Up, w, h, fovHrad};
  // PathTracingShader veach_shader{{0.0f, 0.0f, 0.0f}};
  // Scene scene = CreateCornellBox();
  // */

  // Veach Camera
  // Camera for the Veach demo scene: centered composition with the plate stack
  // directly under the square lights and a less dominant floor presence.
  constexpr Point Eye = {0, 2, -7};
  constexpr Point At = {0, 1, 2};
  constexpr Vector Up = {0, 1, 0};
  constexpr float fovH = 45.f;

  // Motion Blur Scene Camera Settings
  constexpr Point MBEye = {13,2,3};
  constexpr Point MBAt = {0,0,0};
  //constexpr Vector MBUp = {0,1,0}
  constexpr float MBFovH = 20.f;
  constexpr float MBFovHrad = MBFovH * 3.14f/180.f;
  // teste de debug com defocus_angle = 0.0f (previamente 0.6f) (e focus_dist = 13.5f (de 10.0f))
  Camera MBCamera{MBEye, MBAt, Up, w, h, MBFovHrad, 0.0f, 13.5f, 0.0f, 1.0f};

  constexpr float fovHrad = fovH * 3.14f / 180.f;
  Camera camera{Eye, At, Up, w, h, fovHrad};
  Image image{w, h};

  const bool use_default_medium_scene = options.MediumDemo || options.MediumDensity.has_value();
  const std::optional<std::filesystem::path> scene_path =
      options.ScenePath.has_value() ? options.ScenePath
                                    : (use_default_medium_scene ? std::optional<std::filesystem::path>{"scenes/cornell-box.gltf"} : std::nullopt);

  if (scene_path.has_value())
  {
    Scene scene = CreateGltfScene(*scene_path, w, h);
    if (options.MediumDensity.has_value() || options.MediumDemo)
    {
      const float density = options.MediumDensity.value_or(0.15f);
      scene.SetGlobalMedium(ConstantDensityMedium{density, RGB{0.82f, 0.9f, 1.0f}});
    }
    scene.Build();
    const Camera& render_camera = scene.GetCamera() != nullptr ? *scene.GetCamera() : camera;

    if (options.Progressive)
    {
      RenderWindow win{w, h, "VI Renderer"};
      ProgressiveRenderer progressive;
      progressive.Render(scene, render_camera, 0,
        [&](const Image& frame, int pass, bool& keepGoing) {
          win.PollEvents();
          if (!win.IsOpen()) { keepGoing = false; return; }
          win.Display(frame);
          std::cout << "\rPass " << pass << std::flush;
        }
      );
      std::cout << '\n';
      if (const Image* last = progressive.GetLastImage())
        image = *last;
    }
    else
    {
      PathTracingShader shader{{0.f, 0.f, 0.f}, DirectIlluminationMode::Importance};
      Renderer renderer;
      image = renderer.Render(scene, render_camera, shader, options.SamplesPerPixel, true);
    }
  }
  else
  {
    VeachShader veach_shader{{0.0f, 0.0f, 0.0f}};
    Scene scene = CreateVeachScene();
    scene.Build();

    if (options.Progressive)
    {
      RenderWindow win{w, h, "VI Renderer - Veach"};
      ProgressiveRenderer progressive;
      progressive.Render(scene, camera, 0,
        [&](const Image& frame, int pass, bool& keepGoing) {
          win.PollEvents();
          if (!win.IsOpen()) { keepGoing = false; return; }
          win.Display(frame);
          std::cout << "\rPass " << pass << std::flush;
        }
      );
      std::cout << '\n';
      if (const Image* last = progressive.GetLastImage())
        image = *last;
    }
    else
    {
      Renderer renderer;
      image = renderer.Render(scene, camera, veach_shader, options.SamplesPerPixel, true);
    }
  }

  ImagePPM::Save(image, "image.ppm");

  auto end = std::chrono::system_clock::now();

  auto duration = std::chrono::duration<double>(end - begin);

  std::cout << "Time to render: " << duration.count() << " sec" << '\n';

#if defined(__DENOISE__)
  begin = std::chrono::system_clock::now();

  Denoiser denoise(w, h);
  const auto denoised_image = denoise.Execute(image);

  ImagePPM::Save(denoised_image, "image-OIDN.ppm");

  end = std::chrono::system_clock::now();

  duration = std::chrono::duration<double>(end - begin);

  std::cout << "Time to denoise: " << duration.count() << " sec" << '\n';
#endif

  return 0;
}
