cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 23)

project(VI-RT)

add_subdirectory(external/glm)

include(FetchContent)

set(OIDN_VERSION "2.4.1")

if(APPLE)
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
    set(OIDN_PACKAGE "oidn-${OIDN_VERSION}.arm64.macos")
  else()
    set(OIDN_PACKAGE "oidn-${OIDN_VERSION}.x86_64.macos")
  endif()

  set(OIDN_URL "https://github.com/RenderKit/oidn/releases/download/v${OIDN_VERSION}/${OIDN_PACKAGE}.tar.gz")

elseif(WIN32)
  set(OIDN_PACKAGE "oidn-${OIDN_VERSION}.x64.windows")
  set(OIDN_URL "https://github.com/RenderKit/oidn/releases/download/v${OIDN_VERSION}/${OIDN_PACKAGE}.zip")

elseif(UNIX)
  set(OIDN_PACKAGE "oidn-${OIDN_VERSION}.x86_64.linux")
  set(OIDN_URL "https://github.com/RenderKit/oidn/releases/download/v${OIDN_VERSION}/${OIDN_PACKAGE}.tar.gz")

else()
  message(FATAL_ERROR "Unsupported platform for automatic OIDN binary download")
endif()

FetchContent_Declare(
  oidn_prebuilt
  URL "${OIDN_URL}"
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(oidn_prebuilt)

list(APPEND CMAKE_PREFIX_PATH "${oidn_prebuilt_SOURCE_DIR}")

find_package(OpenImageDenoise REQUIRED CONFIG)
find_package(SFML 2.5 COMPONENTS graphics window system REQUIRED)

add_executable(VI-RT src/main.cpp
    src/Log/Log.hpp

    src/Utils/ProgressBar.hpp
    src/Utils/ProgressBar.cpp

    # Ray
    src/Ray/Ray.hpp
    src/Ray/Intersection.hpp

    # Renderer
    src/Renderer/Renderer.hpp
    src/Renderer/DummyRenderer.hpp
    src/Renderer/ProgressiveRenderer.cpp

    # Window
    src/Window/RenderWindow.cpp

    # Image
    src/Image/Denoiser.hpp
    src/Image/Denoiser.cpp

    src/Image/Image.hpp
    src/Image/FileImages.hpp
    src/Image/FileImages.cpp

    # Camera
    src/Camera/Camera.hpp
    src/Camera/Camera.cpp

    # Scene
    src/Scene/Scene.hpp
    src/Scene/Scene.cpp
    src/Scene/SceneBuilder.hpp
    src/Scene/SceneBuilder.cpp
    src/Scene/GltfSceneBuilder.cpp

    # Primitives
    src/Primitive/BRDF.hpp
    src/Primitive/BRDF.cpp
    src/Primitive/BoundingBox.hpp
    src/Primitive/Primitive.hpp
    src/Primitive/Material.hpp
    src/Primitive/Geometry/Geometry.hpp
    src/Primitive/Geometry/Geometry.cpp
    src/Primitive/Geometry/Sphere.hpp
    src/Primitive/Geometry/Sphere.cpp
    src/Primitive/Geometry/Triangle.hpp
    src/Primitive/Geometry/Triangle.cpp
    src/Primitive/Geometry/Mesh.hpp
    src/Primitive/Geometry/Mesh.cpp

    src/Primitive/AccelerationStructures/AccelerationStructure.hpp

    src/Primitive/AccelerationStructures/GridAccelerationStructure.hpp
    src/Primitive/AccelerationStructures/GridAccelerationStructure.cpp

    # Lights
    src/Light/Light.hpp

    # Medium
    src/Medium/Medium.hpp
    src/Medium/Medium.cpp
    src/Medium/PhaseFunction.hpp
    src/Medium/PhaseFunction.cpp

    # Math
    src/Math/Math.hpp
    src/Math/Math.cpp
    src/Math/DiscreteDistribution.hpp
    src/Math/DiscreteDistribution.cpp
    src/Math/RGB.hpp
    src/Math/Vector.hpp
    src/Math/Random.hpp
    src/Math/Random.cpp

    # Shaders
    src/Shaders/Shader.hpp
    src/Shaders/DummyShader.hpp
    src/Shaders/AmbientShader.hpp
    src/Shaders/AmbientShader.cpp
    src/Shaders/WhittedShader.hpp
    src/Shaders/WhittedShader.cpp
    src/Shaders/PathTracingShader.hpp
    src/Shaders/PathTracingShader.cpp
    src/Shaders/DirectIllumination.hpp
    src/Shaders/DirectIllumination.cpp
    src/Shaders/VeachShader.hpp
    src/Shaders/VeachShader.cpp
)

target_include_directories(VI-RT PRIVATE src)
target_include_directories(VI-RT SYSTEM PRIVATE external/tinygltf)
target_precompile_headers(VI-RT PRIVATE src/vi_pch.h)

target_link_libraries(VI-RT PRIVATE sfml-graphics sfml-window sfml-system)
target_link_libraries(VI-RT PRIVATE glm::glm)
target_link_libraries(VI-RT PRIVATE OpenImageDenoise)

if (MSVC)
  target_compile_options(VI-RT PRIVATE /W4)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
  target_compile_options(VI-RT PRIVATE -fcolor-diagnostics -Wall -Wextra -Wpedantic)
else()
  target_compile_options(VI-RT PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()
