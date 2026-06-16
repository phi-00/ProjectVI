#include "Scene/SceneBuilder.hpp"

#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "Light/Light.hpp"
#include "Math/Vector.hpp"
#include "Math/Random.hpp"
#include "Primitive/Geometry/Mesh.hpp"
#include "Primitive/Geometry/Sphere.hpp"
#include "Primitive/Geometry/Triangle.hpp"
#include "Scene/Scene.hpp"

namespace VI
{
Scene SphereScene()
{
  Scene scene{};

  const int red_mat = scene.AddMaterial({.Name = "Red Material", .Albedo = {1.0f, 0.0f, 0.0f}});

  Sphere sphere{Point{0.0f, 0.0f, 3.0f}, 0.8f};
  scene.AddPrimitive(sphere, red_mat);

  const int light_mat = scene.AddMaterial({
      .Name = "Light",
      .EmissionColor = {1.0f, 1.0f, 1.0f},
      .EmissionPower = 10.0f,
  });

  scene.AddLight(std::make_unique<AmbientLight>(light_mat));

  return scene;
}

// added motion blur demo scene
Scene CreateMotionBlurScene(){
    Scene scene;

    const int light_mat = scene.AddMaterial({
      .Name = "Light",
      .EmissionColor = {0.6f, 0.7f, 1.0f},
      .EmissionPower = 0.8f,
    });
    scene.AddLight(std::make_unique<AmbientLight>(light_mat));
    
    // ground
    const int ground_material = scene.AddMaterial({.Name="Ground", .Albedo={0.5f,0.5f,0.5f}});
    scene.AddPrimitive(Sphere{Point{0.f, -1000.f, 0.f}, 1000.f}, ground_material);

    // smaller spheres
    for(int a=-11; a<11; a++){
        for(int b=-11; b<11; b++){
            const float rand_mat = Random::RandomFloat(0.0f, 1.0f);
            const Point sphere_center{a + 0.9f*Random::RandomFloat(0.0f,1.0f), 0.2f, b+0.9f*Random::RandomFloat(0.0f,1.0f)};

            if (glm::length(sphere_center - Point{4.0f, 0.2f, 0.0f}) > 0.9f){
                if (rand_mat < 0.8f){
                // Diffuse
                const RGB albedo = RGB{Random::RandomFloat(0.0f, 1.0f), Random::RandomFloat(0.0f, 1.0f), Random::RandomFloat(0.0f, 1.0f)} *
                                    RGB{Random::RandomFloat(0.0f, 1.0f), Random::RandomFloat(0.0f, 1.0f), Random::RandomFloat(0.0f, 1.0f)};
                const int mat = scene.AddMaterial({.Name = "Diffuse Sphere", .Albedo = albedo});

                const Point center2 = sphere_center + Vector{0.0f, Random::RandomFloat(0.0f, 0.5f), 0.0f};
                scene.AddPrimitive(Sphere{sphere_center, center2, 0.2f}, mat);
                }
                else{
                // Metal
                const RGB albedo = {Random::RandomFloat(0.5f, 1.0f), Random::RandomFloat(0.5f, 1.0f), Random::RandomFloat(0.5f, 1.0f)};
                const float fuzz = Random::RandomFloat(0.0f, 0.5f);
                const int mat = scene.AddMaterial({.Name = "Metal Sphere", .Albedo = albedo, .Roughness = fuzz, .Metallic = 1.0f});
                scene.AddPrimitive(Sphere{sphere_center, 0.2f}, mat);
                }
            }
        }
    }

    const int diffuse_mat = scene.AddMaterial({.Name = "Diffuse Big", .Albedo = {0.4f, 0.2f, 0.1f}});
    scene.AddPrimitive(Sphere{Point{-4.0f, 1.0f, 0.0f}, 1.0f}, diffuse_mat);

    const int metal_mat = scene.AddMaterial({.Name = "Metal Big", .Albedo = {0.7f, 0.6f, 0.5f}, .Roughness = 0.0f, .Metallic = 1.0f});
    scene.AddPrimitive(Sphere{Point{4.0f, 1.0f, 0.0f}, 1.0f}, metal_mat);

    return scene;
}

Scene WhittedCornellBox()
{
  Scene scene;

  const int white_mat = scene.AddMaterial({.Name = "White", .Albedo = {0.75f, 0.75f, 0.75f}});
  const int red_mat = scene.AddMaterial({.Name = "Red", .Albedo = {0.65f, 0.05f, 0.05f}});
  const int green_mat = scene.AddMaterial({.Name = "Green", .Albedo = {0.05f, 0.30f, 0.05f}});
  const int black_mat = scene.AddMaterial({.Name = "Black", .Albedo = {0.02f, 0.02f, 0.02f}});
  const int blue_mat = scene.AddMaterial({.Name = "Dark Navy", .Albedo = {0.0f, 0.0f, 1.0f}});
  const int wood_mat = scene.AddMaterial({.Name = "Wood", .Albedo = {0.52f, 0.30f, 0.10f}});
  const int mirror_mat = scene.AddMaterial({.Name = "Mirror", .Albedo = {1.f, 1.f, 1.f}, .Roughness = 0.f, .Metallic = 1.f});

  // Helper: generates all 12 triangles (6 faces) of an axis-aligned box
  auto make_box = [](float x1, float y1, float z1, float x2, float y2, float z2) -> std::vector<Triangle>
  {
    return {
        // Bottom (y=y1, normal down)
        Triangle{Point{x1, y1, z1}, Point{x1, y1, z2}, Point{x2, y1, z2}, Vector{0.f, -1.f, 0.f}},
        Triangle{Point{x1, y1, z1}, Point{x2, y1, z2}, Point{x2, y1, z1}, Vector{0.f, -1.f, 0.f}},
        // Top (y=y2, normal up)
        Triangle{Point{x1, y2, z1}, Point{x2, y2, z1}, Point{x2, y2, z2}, Vector{0.f, 1.f, 0.f}},
        Triangle{Point{x1, y2, z1}, Point{x2, y2, z2}, Point{x1, y2, z2}, Vector{0.f, 1.f, 0.f}},
        // Front (z=z1, normal toward camera)
        Triangle{Point{x1, y1, z1}, Point{x2, y1, z1}, Point{x2, y2, z1}, Vector{0.f, 0.f, -1.f}},
        Triangle{Point{x1, y1, z1}, Point{x2, y2, z1}, Point{x1, y2, z1}, Vector{0.f, 0.f, -1.f}},
        // Back (z=z2, normal away from camera)
        Triangle{Point{x1, y1, z2}, Point{x1, y2, z2}, Point{x2, y2, z2}, Vector{0.f, 0.f, 1.f}},
        Triangle{Point{x1, y1, z2}, Point{x2, y2, z2}, Point{x2, y1, z2}, Vector{0.f, 0.f, 1.f}},
        // Right face (x=x1, normal toward -X / green wall side)
        Triangle{Point{x1, y1, z1}, Point{x1, y2, z1}, Point{x1, y2, z2}, Vector{-1.f, 0.f, 0.f}},
        Triangle{Point{x1, y1, z1}, Point{x1, y2, z2}, Point{x1, y1, z2}, Vector{-1.f, 0.f, 0.f}},
        // Left face (x=x2, normal toward +X / red wall side)
        Triangle{Point{x2, y1, z1}, Point{x2, y1, z2}, Point{x2, y2, z2}, Vector{1.f, 0.f, 0.f}},
        Triangle{Point{x2, y1, z1}, Point{x2, y2, z2}, Point{x2, y2, z1}, Vector{1.f, 0.f, 0.f}},
    };
  };

  // Floor
  scene.AddPrimitive(Mesh{"Floor", std::vector<Triangle>{Triangle{Point{552.8f, 0.f, 0.f}, Point{-100.f, 0.f, 0.f}, Point{-100.f, 0.f, 859.2f}, Vector{0.f, 1.f, 0.f}}, Triangle{Point{549.6f, 0.f, 859.2f}, Point{552.8f, 0.f, 0.f}, Point{-100.f, 0.f, 859.2f}, Vector{0.f, 1.f, 0.f}}}}, white_mat);

  // Ceiling (dark/black)
  scene.AddPrimitive(
      Mesh{"Ceiling", std::vector<Triangle>{Triangle{Point{556.f, 548.8f, 0.f}, Point{-100.f, 548.8f, 0.f}, Point{-100.f, 548.8f, 459.2f}, Vector{0.f, -1.f, 0.f}}, Triangle{Point{556.f, 548.8f, 459.2f}, Point{556.f, 548.8f, 0.f}, Point{-100.f, 548.8f, 459.2f}, Vector{0.f, -1.f, 0.f}}}}, black_mat);

  // Back wall
  scene.AddPrimitive(
      Mesh{"Back Wall", std::vector<Triangle>{Triangle{Point{-100.f, 0.f, 459.2f}, Point{549.6f, 0.f, 459.2f}, Point{556.f, 548.8f, 459.2f}, Vector{0.f, 0.f, -1.f}}, Triangle{Point{-100.f, 0.f, 459.2f}, Point{-100.f, 548.8f, 459.2f}, Point{556.f, 548.8f, 459.2f}, Vector{0.f, 0.f, -1.f}}}},
      white_mat);

  // Right wall (green)
  scene.AddPrimitive(Mesh{"Right Wall", std::vector<Triangle>{Triangle{Point{-100.f, 0.f, 0.f}, Point{-100.f, 0.f, 459.2f}, Point{-100.f, 548.8f, 459.2f}, Vector{1.f, 0.f, 0.f}}, Triangle{Point{-100.f, 0.f, 0.f}, Point{-100.f, 548.8f, 0.f}, Point{-100.f, 548.8f, 459.2f}, Vector{1.f, 0.f, 0.f}}}},
                     green_mat);

  // Left wall (red)
  scene.AddPrimitive(Mesh{"Left Wall", std::vector<Triangle>{Triangle{Point{552.8f, 0.f, 0.f}, Point{549.6f, 0.f, 459.2f}, Point{549.6f, 548.8f, 459.2f}, Vector{-1.f, 0.f, 0.f}}, Triangle{Point{552.8f, 0.f, 0.f}, Point{552.8f, 548.8f, 0.f}, Point{549.6f, 548.8f, 459.2f}, Vector{-1.f, 0.f, 0.f}}}},
                     red_mat);

  // Mirror plane on left wall (roughness=0, facing into the room)
  scene.AddPrimitive(Mesh{"Mirror Plane", std::vector<Triangle>{Triangle{Point{540.f, 50.f, 50.f}, Point{540.f, 50.f, 409.f}, Point{540.f, 488.f, 409.f}, Vector{-1.f, 0.f, 0.f}}, Triangle{Point{540.f, 50.f, 50.f}, Point{540.f, 488.f, 409.f}, Point{540.f, 488.f, 50.f}, Vector{-1.f, 0.f, 0.f}}}},
                     mirror_mat);

  // Dark navy box 1 (left-center of scene)
  scene.AddPrimitive(Mesh{"Dark Box 1", make_box(310.f, 0.f, 220.f, 440.f, 320.f, 345.f)}, blue_mat);

  // Wooden box (right-center, closer to camera)
  scene.AddPrimitive(Mesh{"Wooden Box", make_box(45.f, 0.f, 60.f, 200.f, 195.f, 215.f)}, wood_mat);

  // Metallic sphere resting on top of the wooden box
  scene.AddPrimitive(Sphere{Point{122.f, 285.f, 137.f}, 90.f}, mirror_mat);

  return scene;
}

Scene CreateCornellBox()
{
  Scene scene;

  const int white_mat = scene.AddMaterial({.Name = "White Material", .Albedo = {0.8f, 0.8f, 0.8f}});
  const int red_mat = scene.AddMaterial({.Name = "Red Material", .Albedo = {0.5f, 0.1f, 0.1f}});
  const int green_mat = scene.AddMaterial({.Name = "Green Material", .Albedo = {0.f, 0.6f, 0.f}});
  const int blue_mat = scene.AddMaterial({.Name = "Blue Material", .Albedo = {0.f, 0.f, 0.6f}});
  const int orange_mat = scene.AddMaterial({.Name = "Orange Material", .Albedo = {0.66f, 0.44f, 0.f}});
  const int mirror_mat = scene.AddMaterial({.Name = "Mirror Material", .Albedo = {1.f, 1.f, 1.f}, .Roughness = 0.f, .Metallic = 1.f});

  const int light_mat = scene.AddMaterial({.Name = "Light", .Albedo = {1.f, 1.f, 1.f}, .EmissionColor = {1.f, 1.f, 1.f}, .EmissionPower = 20.f});

  // Area Light (on the ceiling)
  {
    float y = 548.7f; // Slightly below the ceiling
    scene.AddPrimitive(Mesh{"Ceiling Light", std::vector<Triangle>{Triangle{
                                                                       Point{158.0f, y, 160.0f},
                                                                       Point{298.0f, y, 160.0f},
                                                                       Point{298.0f, y, 300.0f},
                                                                       Vector{0.0f, -1.0f, 0.0f},
                                                                   },
                                                                   Triangle{
                                                                       Point{158.0f, y, 160.0f},
                                                                       Point{298.0f, y, 300.0f},
                                                                       Point{158.0f, y, 300.0f},
                                                                       Vector{0.0f, -1.0f, 0.0f},
                                                                   }}},
                       light_mat);
  }

  // Floor
  {
    scene.AddPrimitive(Mesh{"Floor", std::vector<Triangle>{Triangle{
                                                               Point{552.8, 0.0, 0.0},
                                                               Point{-100.0, 0.0, 0.0},
                                                               Point{-100.0, 0.0, 859.2},
                                                               Vector{0.0, 1.0, 0.0}, // normal for all three vertices
                                                           },
                                                           Triangle{
                                                               Point{549.6, 0.0, 859.2},
                                                               Point{552.8, 0.0, 0.0},
                                                               Point{-100.0, 0.0, 859.2},
                                                               Vector{0.0, 1.0, 0.0},
                                                           }}},
                       white_mat);
  }
  // Ceiling
  {
    scene.AddPrimitive(Mesh{"Ceiling", std::vector<Triangle>{Triangle{
                                                                 Point{556.0, 548.8, 0.0},
                                                                 Point{-100.0, 548.8, 0.0},
                                                                 Point{-100.0, 548.8, 459.2},
                                                                 Vector{0.0, -1.0, 0.0},
                                                             },
                                                             Triangle{
                                                                 Point{556.0, 548.8, 459.2},
                                                                 Point{556.0, 548.8, 0.0},
                                                                 Point{-100.0, 548.8, 459.2},
                                                                 Vector{0.0, -1.0, 0.0},
                                                             }}},
                       white_mat);
  }
  // Back wall
  {
    scene.AddPrimitive(Mesh{"Back Wall", std::vector<Triangle>{Triangle{
                                                                   Point{-100.0, 0.0, 459.2},
                                                                   Point{549.6, 0.0, 459.2},
                                                                   Point{556.0, 548.8, 459.2},
                                                                   Vector{0.0, 0.0, -1.0},
                                                               },
                                                               Triangle{
                                                                   Point{-100.0, 0.0, 459.2},
                                                                   Point{-100.0, 548.8, 459.2},
                                                                   Point{556.0, 548.8, 459.2},
                                                                   Vector{0.0, 0.0, -1.0},
                                                               }}},
                       white_mat);
  }
  // Right Wall
  {
    scene.AddPrimitive(Mesh{"Right Wall", std::vector<Triangle>{Triangle{
                                                                    Point{-100.0, 0.0, 0.0},
                                                                    Point{-100.0, 0.0, 459.2},
                                                                    Point{-100.0, 548.8, 459.2},
                                                                    Vector{1.0, 0.0, 0.0},
                                                                },
                                                                Triangle{
                                                                    Point{-100.0, 0.0, 0.0},
                                                                    Point{-100.0, 548.8, 0.0},
                                                                    Point{-100.0, 548.8, 459.2},
                                                                    Vector{1.0, 0.0, 0.0},
                                                                }}},
                       green_mat);
  }

  { // Left Wall Mirror
    scene.AddPrimitive(Mesh{"Right Wall Mirror", std::vector<Triangle>{Triangle{
                                                                           Point{540, 50.0, 50.0},
                                                                           Point{540, 50.0, 509.2},
                                                                           Point{540, 488.8, 509.2},
                                                                           Vector{-1.0, 0.0, 0.0},
                                                                       },
                                                                       Triangle{
                                                                           Point{540, 50.0, 50.},
                                                                           Point{540, 488.8, 50.},
                                                                           Point{540, 488.8, 509.2},
                                                                           Vector{-1.0, 0.0, 0.0},
                                                                       }}},
                       mirror_mat);
  }

  // Left Wall
  {
    scene.AddPrimitive(Mesh{"Left Wall", std::vector<Triangle>{Triangle{
                                                                   Point{552.8, 0.0, 0.0},
                                                                   Point{549.6, 0.0, 459.2},
                                                                   Point{549.6, 548.8, 459.2},
                                                                   Vector{-1.0, 0.0, 0.0},
                                                               },
                                                               Triangle{
                                                                   Point{552.8, 0.0, 0.0},
                                                                   Point{552.8, 548.8, 0.0},
                                                                   Point{549.6, 548.8, 459.2},
                                                                   Vector{-1.0, 0.0, 0.0},
                                                               }}},
                       red_mat);
  }
  // short block
  {
    scene.AddPrimitive(
        Mesh{"Short Block",
             std::vector<Triangle>{
                 // top
                 Triangle{
                     Point{130.0, 165.0, 65.0},
                     Point{82.0, 165.0, 225.0},
                     Point{240.0, 165.0, 272.0},
                     Vector{0.0, 1.0, 0.0},
                 },
                 Triangle{
                     Point{130.0, 165.0, 65.0},
                     Point{290.0, 165.0, 114.0},
                     Point{240.0, 165.0, 272.0},
                     Vector{0.0, 1.0, 0.0},
                 },
                 // bottom
                 Triangle{
                     Point{130.0, 0.01, 65.0},
                     Point{82.0, 0.01, 225.0},
                     Point{240.0, 0.01, 272.0},
                     Vector{0.0, -1.0, 0.0},
                 },
                 Triangle{
                     Point{130.0, 0.01, 65.0},
                     Point{290.0, 0.01, 114.0},
                     Point{240.0, 0.01, 272.0},
                     Vector{0.0, -1.0, 0.0},
                 },
                 // left
                 Triangle{
                     Point{290.0, 0.0, 114.0},
                     Point{290.0, 165.0, 114.0},
                     Point{240.0, 165.0, 272.0},
                     Vector{0.8944, 0.0, 0.4472},
                 },
                 Triangle{
                     Point{290.0, 0.0, 114.0},
                     Point{240.0, 0.0, 272.0},
                     Point{240.0, 165.0, 272.0},
                     Vector{0.8944, 0.0, 0.4472},
                 },
                 // back
                 Triangle{
                     Point{240.0, 0.0, 272.0},
                     Point{240.0, 165.0, 272.0},
                     Point{82.0, 165.0, 225.0},
                     Vector{0.0, 0.0, 1.0},
                 },
                 Triangle{
                     Point{240.0, 0.0, 272.0},
                     Point{82.0, 0.0, 225.0},
                     Point{82.0, 165.0, 225.0},
                     Vector{0.0, 0.0, 1.0},
                 },
                 // right
                 Triangle{
                     Point{82.0, 0.0, 225.0},
                     Point{82.0, 165.0, 225.0},
                     Point{130.0, 165.0, 65.0},
                     Vector{-0.8944, 0.0, -0.4472},
                 },
                 Triangle{
                     Point{82.0, 0.0, 225.0},
                     Point{130.0, 0.0, 65.0},
                     Point{130.0, 165.0, 65.0},
                     Vector{-0.8944, 0.0, -0.4472},
                 },
                 // front
                 Triangle{
                     Point{130.0, 0.0, 65.0},
                     Point{130.0, 165.0, 65.0},
                     Point{290.0, 165.0, 114.0},
                     Vector{0.0, 0.0, -1.0},
                 },
                 Triangle{
                     Point{130.0, 0.0, 65.0},
                     Point{290.0, 0.0, 114.0},
                     Point{290.0, 165.0, 114.0},
                     Vector{0.0, 0.0, -1.0},
                 },
             }},
        orange_mat);
  }
  // tall block
  {
    scene.AddPrimitive(Mesh{"Tall Block",
                            std::vector<Triangle>{
                                // top
                                Triangle{
                                    Point{423.0, 330.0, 247.0},
                                    Point{265.0, 330.0, 296.0},
                                    Point{314.0, 330.0, 456.0},
                                    Vector{0.0, 1.0, 0.0},
                                },
                                Triangle{
                                    Point{423.0, 330.0, 247.0},
                                    Point{472.0, 330.0, 406.0},
                                    Point{314.0, 330.0, 456.0},
                                    Vector{0.0, 1.0, 0.0},
                                },
                                // bottom
                                Triangle{
                                    Point{423.0, 0.1, 247.0},
                                    Point{265.0, 0.1, 296.0},
                                    Point{314.0, 0.1, 456.0},
                                    Vector{0.0, -1.0, 0.0},
                                },
                                Triangle{
                                    Point{423.0, 0.1, 247.0},
                                    Point{472.0, 0.1, 406.0},
                                    Point{314.0, 0.1, 456.0},
                                    Vector{0.0, -1.0, 0.0},
                                },
                                // left
                                Triangle{
                                    Point{423.0, 0.0, 247.0},
                                    Point{423.0, 330.0, 247.0},
                                    Point{472.0, 330.0, 406.0},
                                    Vector{0.8944, 0.0, 0.4472},
                                },
                                Triangle{
                                    Point{423.0, 0.0, 247.0},
                                    Point{472.0, 0.0, 406.0},
                                    Point{472.0, 330.0, 406.0},
                                    Vector{0.8944, 0.0, 0.4472},
                                },
                                // back
                                Triangle{
                                    Point{472.0, 0.0, 406.0},
                                    Point{472.0, 330.0, 406.0},
                                    Point{314.0, 330.0, 456.0},
                                    Vector{0.0, 0.0, 1.0},
                                },
                                Triangle{
                                    Point{472.0, 0.0, 406.0},
                                    Point{314.0, 0.0, 456.0},
                                    Point{314.0, 330.0, 456.0},
                                    Vector{0.0, 0.0, 1.0},
                                },
                                // right
                                Triangle{
                                    Point{314.0, 0.0, 456.0},
                                    Point{314.0, 330.0, 456.0},
                                    Point{265.0, 330.0, 296.0},
                                    Vector{-0.8944, 0.0, -0.4472},
                                },
                                Triangle{
                                    Point{314.0, 0.0, 456.0},
                                    Point{265.0, 0.0, 296.0},
                                    Point{265.0, 330.0, 296.0},
                                    Vector{-0.8944, 0.0, -0.4472},
                                },
                                // front
                                Triangle{
                                    Point{265.0, 0.0, 296.0},
                                    Point{265.0, 330.0, 296.0},
                                    Point{423.0, 330.0, 247.0},
                                    Vector{0.0, 0.0, -1.0},
                                },
                                Triangle{
                                    Point{265.0, 0.0, 296.0},
                                    Point{423.0, 0.0, 247.0},
                                    Point{423.0, 330.0, 247.0},
                                    Vector{0.0, 0.0, -1.0},
                                },
                            }},
                       blue_mat);
  }

  return scene;
}

Scene CreateImportanceSamplingCornellBox()
{
  Scene scene;

  const int white_mat = scene.AddMaterial({.Name = "White Material", .Albedo = {0.8f, 0.8f, 0.8f}});
  const int red_mat = scene.AddMaterial({.Name = "Red Material", .Albedo = {0.5f, 0.1f, 0.1f}});
  const int green_mat = scene.AddMaterial({.Name = "Green Material", .Albedo = {0.f, 0.6f, 0.f}});
  const int blue_mat = scene.AddMaterial({.Name = "Blue Material", .Albedo = {0.f, 0.f, 0.6f}});
  const int orange_mat = scene.AddMaterial({.Name = "Orange Material", .Albedo = {0.66f, 0.44f, 0.f}});
  const int mirror_mat = scene.AddMaterial({.Name = "Mirror Material", .Albedo = {1.f, 1.f, 1.f}, .Roughness = 0.f, .Metallic = 1.f});

  const int dominant_light_mat = scene.AddMaterial({
      .Name = "Dominant Light",
      .Albedo = {1.f, 1.f, 1.f},
      .EmissionColor = {1.f, 1.f, 1.f},
      .EmissionPower = 20.f,
  });
  const int weak_light_1_mat = scene.AddMaterial({
      .Name = "Weak Light 1",
      .Albedo = {1.f, 1.f, 1.f},
      .EmissionColor = {1.f, 1.f, 1.f},
      .EmissionPower = 2.f,
  });
  const int weak_light_2_mat = scene.AddMaterial({
      .Name = "Weak Light 2",
      .Albedo = {1.f, 1.f, 1.f},
      .EmissionColor = {1.f, 1.f, 1.f},
      .EmissionPower = 2.f,
  });
  const int weak_light_3_mat = scene.AddMaterial({
      .Name = "Weak Light 3",
      .Albedo = {1.f, 1.f, 1.f},
      .EmissionColor = {1.f, 1.f, 1.f},
      .EmissionPower = 2.f,
  });

  auto add_area_light = [&scene](std::string_view name, int material_index, float x0, float x1, float z0, float z1)
  {
    constexpr float y = 548.7f;
    scene.AddPrimitive(Mesh{std::string{name},
                            std::vector<Triangle>{
                                Triangle{
                                    Point{x0, y, z0},
                                    Point{x1, y, z0},
                                    Point{x1, y, z1},
                                    Vector{0.0f, -1.0f, 0.0f},
                                },
                                Triangle{
                                    Point{x0, y, z0},
                                    Point{x1, y, z1},
                                    Point{x0, y, z1},
                                    Vector{0.0f, -1.0f, 0.0f},
                                },
                            }},
                       material_index);
  };

  // Spread the emitters across the ceiling so the sampling difference is more
  // visible spatially than with a tight 2x2 cluster.
  add_area_light("Ceiling Light Dominant", dominant_light_mat, 70.0f, 140.0f, 60.0f, 130.0f);
  add_area_light("Ceiling Light Weak 1", weak_light_1_mat, 320.0f, 390.0f, 70.0f, 140.0f);
  add_area_light("Ceiling Light Weak 2", weak_light_2_mat, 80.0f, 150.0f, 280.0f, 350.0f);
  add_area_light("Ceiling Light Weak 3", weak_light_3_mat, 330.0f, 400.0f, 290.0f, 360.0f);

  // Floor
  {
    scene.AddPrimitive(Mesh{"Floor", std::vector<Triangle>{Triangle{
                                                               Point{552.8, 0.0, 0.0},
                                                               Point{-100.0, 0.0, 0.0},
                                                               Point{-100.0, 0.0, 859.2},
                                                               Vector{0.0, 1.0, 0.0},
                                                           },
                                                           Triangle{
                                                               Point{549.6, 0.0, 859.2},
                                                               Point{552.8, 0.0, 0.0},
                                                               Point{-100.0, 0.0, 859.2},
                                                               Vector{0.0, 1.0, 0.0},
                                                           }}},
                       white_mat);
  }
  // Ceiling
  {
    scene.AddPrimitive(Mesh{"Ceiling", std::vector<Triangle>{Triangle{
                                                                 Point{556.0, 548.8, 0.0},
                                                                 Point{-100.0, 548.8, 0.0},
                                                                 Point{-100.0, 548.8, 459.2},
                                                                 Vector{0.0, -1.0, 0.0},
                                                             },
                                                             Triangle{
                                                                 Point{556.0, 548.8, 459.2},
                                                                 Point{556.0, 548.8, 0.0},
                                                                 Point{-100.0, 548.8, 459.2},
                                                                 Vector{0.0, -1.0, 0.0},
                                                             }}},
                       white_mat);
  }
  // Back wall
  {
    scene.AddPrimitive(Mesh{"Back Wall", std::vector<Triangle>{Triangle{
                                                                   Point{-100.0, 0.0, 459.2},
                                                                   Point{549.6, 0.0, 459.2},
                                                                   Point{556.0, 548.8, 459.2},
                                                                   Vector{0.0, 0.0, -1.0},
                                                               },
                                                               Triangle{
                                                                   Point{-100.0, 0.0, 459.2},
                                                                   Point{-100.0, 548.8, 459.2},
                                                                   Point{556.0, 548.8, 459.2},
                                                                   Vector{0.0, 0.0, -1.0},
                                                               }}},
                       white_mat);
  }
  // Right Wall
  {
    scene.AddPrimitive(Mesh{"Right Wall", std::vector<Triangle>{Triangle{
                                                                    Point{-100.0, 0.0, 0.0},
                                                                    Point{-100.0, 0.0, 459.2},
                                                                    Point{-100.0, 548.8, 459.2},
                                                                    Vector{1.0, 0.0, 0.0},
                                                                },
                                                                Triangle{
                                                                    Point{-100.0, 0.0, 0.0},
                                                                    Point{-100.0, 548.8, 0.0},
                                                                    Point{-100.0, 548.8, 459.2},
                                                                    Vector{1.0, 0.0, 0.0},
                                                                }}},
                       green_mat);
  }

  { // Left Wall Mirror
    scene.AddPrimitive(Mesh{"Right Wall Mirror", std::vector<Triangle>{Triangle{
                                                                           Point{540, 50.0, 50.0},
                                                                           Point{540, 50.0, 509.2},
                                                                           Point{540, 488.8, 509.2},
                                                                           Vector{-1.0, 0.0, 0.0},
                                                                       },
                                                                       Triangle{
                                                                           Point{540, 50.0, 50.},
                                                                           Point{540, 488.8, 50.},
                                                                           Point{540, 488.8, 509.2},
                                                                           Vector{-1.0, 0.0, 0.0},
                                                                       }}},
                       mirror_mat);
  }

  // Left Wall
  {
    scene.AddPrimitive(Mesh{"Left Wall", std::vector<Triangle>{Triangle{
                                                                   Point{552.8, 0.0, 0.0},
                                                                   Point{549.6, 0.0, 459.2},
                                                                   Point{549.6, 548.8, 459.2},
                                                                   Vector{-1.0, 0.0, 0.0},
                                                               },
                                                               Triangle{
                                                                   Point{552.8, 0.0, 0.0},
                                                                   Point{552.8, 548.8, 0.0},
                                                                   Point{549.6, 548.8, 459.2},
                                                                   Vector{-1.0, 0.0, 0.0},
                                                               }}},
                       red_mat);
  }
  // short block
  {
    scene.AddPrimitive(
        Mesh{"Short Block",
             std::vector<Triangle>{
                 Triangle{
                     Point{130.0, 165.0, 65.0},
                     Point{82.0, 165.0, 225.0},
                     Point{240.0, 165.0, 272.0},
                     Vector{0.0, 1.0, 0.0},
                 },
                 Triangle{
                     Point{130.0, 165.0, 65.0},
                     Point{290.0, 165.0, 114.0},
                     Point{240.0, 165.0, 272.0},
                     Vector{0.0, 1.0, 0.0},
                 },
                 Triangle{
                     Point{130.0, 0.01, 65.0},
                     Point{82.0, 0.01, 225.0},
                     Point{240.0, 0.01, 272.0},
                     Vector{0.0, -1.0, 0.0},
                 },
                 Triangle{
                     Point{130.0, 0.01, 65.0},
                     Point{290.0, 0.01, 114.0},
                     Point{240.0, 0.01, 272.0},
                     Vector{0.0, -1.0, 0.0},
                 },
                 Triangle{
                     Point{290.0, 0.0, 114.0},
                     Point{290.0, 165.0, 114.0},
                     Point{240.0, 165.0, 272.0},
                     Vector{0.8944, 0.0, 0.4472},
                 },
                 Triangle{
                     Point{290.0, 0.0, 114.0},
                     Point{240.0, 0.0, 272.0},
                     Point{240.0, 165.0, 272.0},
                     Vector{0.8944, 0.0, 0.4472},
                 },
                 Triangle{
                     Point{240.0, 0.0, 272.0},
                     Point{240.0, 165.0, 272.0},
                     Point{82.0, 165.0, 225.0},
                     Vector{0.0, 0.0, 1.0},
                 },
                 Triangle{
                     Point{240.0, 0.0, 272.0},
                     Point{82.0, 0.0, 225.0},
                     Point{82.0, 165.0, 225.0},
                     Vector{0.0, 0.0, 1.0},
                 },
                 Triangle{
                     Point{82.0, 0.0, 225.0},
                     Point{82.0, 165.0, 225.0},
                     Point{130.0, 165.0, 65.0},
                     Vector{-0.8944, 0.0, -0.4472},
                 },
                 Triangle{
                     Point{82.0, 0.0, 225.0},
                     Point{130.0, 0.0, 65.0},
                     Point{130.0, 165.0, 65.0},
                     Vector{-0.8944, 0.0, -0.4472},
                 },
                 Triangle{
                     Point{130.0, 0.0, 65.0},
                     Point{130.0, 165.0, 65.0},
                     Point{290.0, 165.0, 114.0},
                     Vector{0.0, 0.0, -1.0},
                 },
                 Triangle{
                     Point{130.0, 0.0, 65.0},
                     Point{290.0, 0.0, 114.0},
                     Point{290.0, 165.0, 114.0},
                     Vector{0.0, 0.0, -1.0},
                 },
             }},
        orange_mat);
  }
  // tall block
  {
    scene.AddPrimitive(Mesh{"Tall Block",
                            std::vector<Triangle>{
                                Triangle{
                                    Point{423.0, 330.0, 247.0},
                                    Point{265.0, 330.0, 296.0},
                                    Point{314.0, 330.0, 456.0},
                                    Vector{0.0, 1.0, 0.0},
                                },
                                Triangle{
                                    Point{423.0, 330.0, 247.0},
                                    Point{472.0, 330.0, 406.0},
                                    Point{314.0, 330.0, 456.0},
                                    Vector{0.0, 1.0, 0.0},
                                },
                                Triangle{
                                    Point{423.0, 0.1, 247.0},
                                    Point{265.0, 0.1, 296.0},
                                    Point{314.0, 0.1, 456.0},
                                    Vector{0.0, -1.0, 0.0},
                                },
                                Triangle{
                                    Point{423.0, 0.1, 247.0},
                                    Point{472.0, 0.1, 406.0},
                                    Point{314.0, 0.1, 456.0},
                                    Vector{0.0, -1.0, 0.0},
                                },
                                Triangle{
                                    Point{423.0, 0.0, 247.0},
                                    Point{423.0, 330.0, 247.0},
                                    Point{472.0, 330.0, 406.0},
                                    Vector{0.8944, 0.0, 0.4472},
                                },
                                Triangle{
                                    Point{423.0, 0.0, 247.0},
                                    Point{472.0, 0.0, 406.0},
                                    Point{472.0, 330.0, 406.0},
                                    Vector{0.8944, 0.0, 0.4472},
                                },
                                Triangle{
                                    Point{472.0, 0.0, 406.0},
                                    Point{472.0, 330.0, 406.0},
                                    Point{314.0, 330.0, 456.0},
                                    Vector{0.0, 0.0, 1.0},
                                },
                                Triangle{
                                    Point{472.0, 0.0, 406.0},
                                    Point{314.0, 0.0, 456.0},
                                    Point{314.0, 330.0, 456.0},
                                    Vector{0.0, 0.0, 1.0},
                                },
                                Triangle{
                                    Point{314.0, 0.0, 456.0},
                                    Point{314.0, 330.0, 456.0},
                                    Point{265.0, 330.0, 296.0},
                                    Vector{-0.8944, 0.0, -0.4472},
                                },
                                Triangle{
                                    Point{314.0, 0.0, 456.0},
                                    Point{265.0, 0.0, 296.0},
                                    Point{265.0, 330.0, 296.0},
                                    Vector{-0.8944, 0.0, -0.4472},
                                },
                                Triangle{
                                    Point{265.0, 0.0, 296.0},
                                    Point{265.0, 330.0, 296.0},
                                    Point{423.0, 330.0, 247.0},
                                    Vector{0.0, 0.0, -1.0},
                                },
                                Triangle{
                                    Point{265.0, 0.0, 296.0},
                                    Point{423.0, 0.0, 247.0},
                                    Point{423.0, 330.0, 247.0},
                                    Vector{0.0, 0.0, -1.0},
                                },
                            }},
                       blue_mat);
  }

  return scene;
}

// Classic Veach MIS test scene.
// 3 horizontal plates with increasing roughness sit on a flat floor.
// 3 area lights of very different sizes (and inversely proportional power)
// hang above the scene.  The combination reveals where each sampling strategy
// fails and where MIS succeeds:
//   - tiny bright light  + smooth plate  → only light-sampling works well
//   - large dim  light  + rough  plate  → only BRDF-sampling works well
//   - MIS handles all cases with low variance

Scene CreateVeachScene()
{
  Scene scene;

  // ── Materials ──────────────────────────────────────────────────────────────
  const int floor_mat = scene.AddMaterial({.Name = "Floor", .Albedo = {0.8f, 0.2f, 0.2f}, .Roughness = .3f});

  // Three reflective bands from sharp to broad reflections.
  const int plate1_mat = scene.AddMaterial({.Name = "Plate Mirror-Like", .Albedo = {0.92f, 0.48f, 0.48f}, .Roughness = 0.05f, .Metallic = 1.f});
  const int plate2_mat = scene.AddMaterial({.Name = "Plate Glossy", .Albedo = {0.48f, 0.92f, 0.48f}, .Roughness = 0.2f, .Metallic = 1.f});
  const int plate3_mat = scene.AddMaterial({.Name = "Plate Broad Gloss", .Albedo = {0.48f, 0.48f, 0.92f}, .Roughness = 0.35f, .Metallic = 1.f});

  // Three square lights with the same tiny/medium/large progression as the
  // reference image, kept roughly equal in total emitted flux.
  const int light1_mat = scene.AddMaterial({
      .Name = "Light Tiny",
      .EmissionColor = {1.f, 0.95f, 0.8f},
      .EmissionPower = 150.f,
  });
  const int light2_mat = scene.AddMaterial({
      .Name = "Light Medium",
      .EmissionColor = {1.f, 0.95f, 0.8f},
      .EmissionPower = 30.f,
  });
  const int light3_mat = scene.AddMaterial({
      .Name = "Light Large",
      .EmissionColor = {1.f, 0.95f, 0.8f},
      .EmissionPower = 6.f,
  });

  // ── Floor (Y=0) ────────────────────────────────────────────────────────────
  scene.AddPrimitive(Mesh{"Floor", {Triangle{Point{-550.f, 0.f, -550.f}, Point{550.f, 0.f, -550.f}, Point{550.f, 0.f, 550.f}, Vector{0.f, 1.f, 0.f}}, Triangle{Point{-550.f, 0.f, -550.f}, Point{-550.f, 0.f, 550.f}, Point{550.f, 0.f, 550.f}, Vector{0.f, 1.f, 0.f}}}}, floor_mat);

  // ── Reflective bands, stepped like the reference composition ──────────────
  auto make_plate = [&](float cx, float cy, float cz, float half_width, float half_depth, float tilt_degrees, int mat_idx)
  {
    const float tilt = glm::radians(tilt_degrees);
    const Vector tangent_x{1.f, 0.f, 0.f};
    // Build the plate like a shallow tray: the near edge drops toward the
    // camera and the far edge lifts away, while the plate stays broad in X.
    const Vector tangent_depth{0.f, glm::sin(tilt), glm::cos(tilt)};
    const Point center{cx, cy, cz};
    const Point p0 = center - tangent_x * half_width - tangent_depth * half_depth;
    const Point p1 = center + tangent_x * half_width - tangent_depth * half_depth;
    const Point p2 = center + tangent_x * half_width + tangent_depth * half_depth;
    const Point p3 = center - tangent_x * half_width + tangent_depth * half_depth;
    const Vector normal = glm::normalize(glm::cross(p1 - p0, p2 - p0));
    scene.AddPrimitive(Mesh{"Plate", {Triangle{p0, p1, p2, normal}, Triangle{p0, p2, p3, normal}}}, mat_idx);
  };
  const float  half_width = 8.f;
  const float  half_depth = 1.f;
  // Roughness increases from plate1_mat to plate4_mat
  //make_plate(0.f, .001f, +3.f, 10.f, .9f, 0.f, plate1_mat);
  //make_plate(0.f, .001f, +1.f, 10.f, .9f, 0.f, plate2_mat);
  //make_plate(0.f, .001f, -1.f, 10.f, .9f, 0.f, plate3_mat);
    float const Zbase = 0.f;
  make_plate(0.f, 4.f, Zbase+0.f, half_width, half_depth, 29.f, plate1_mat);
  make_plate(0.f, 2.f, Zbase-2.f, half_width, half_depth, 14.f, plate2_mat);
  make_plate(0.f, .001f, Zbase-5.f, half_width, half_depth, 0.f, plate3_mat);

  // ── Square area lights tilted toward the camera and the plates ────────────
  auto make_light = [&](float cx, float cy, float cz, float half, int mat_idx)
  {
    const Vector tangent_x{1.f, 0.f, 0.f};
    //const Vector tangent_y = glm::normalize(Vector{0.f, 0.62f, -0.78f});
      const Vector tangent_depth = glm::normalize(Vector{0.f, 0.f, -1.f});
    const Point center{cx, cy, cz};
    const Point p0 = center - tangent_x * half - tangent_depth * half;
    const Point p1 = center + tangent_x * half - tangent_depth * half;
    const Point p2 = center + tangent_x * half + tangent_depth * half;
    const Point p3 = center - tangent_x * half + tangent_depth * half;
    const Vector normal = glm::normalize(glm::cross(p2 - p0, p1 - p0));
    scene.AddPrimitive(Mesh{"Light", {Triangle{p0, p2, p1, normal}, Triangle{p0, p3, p2, normal}}}, mat_idx);
  };
  make_light(-5.f, 8.5f, 0.f, .05f, light1_mat);  // tiny square, left
  make_light(+0.f, 8.5f, 0.f, .3f, light2_mat); // medium square, center
  make_light(+5.f, 8.5f, 0.f, 1.0f, light3_mat); // large square, right

  return scene;
}

} // namespace VI
