# VI-RT-2526

## Prerequisites

- CMake 3.10+
- A C++23 compiler (GCC 13+, Clang 16+, or MSVC via Visual Studio 2022+)
- OIDN is downloaded automatically by CMake via FetchContent

**Windows only:** [SFML 2.6.2 (vc17 64-bit)](https://www.sfml-dev.org/download.php) — extract to `SFML-2.6.2/` in the project root

**Linux only:** Install SFML via your package manager:
```bash
sudo apt install libsfml-dev   # Debian/Ubuntu
sudo dnf install SFML-devel    # Fedora
```

## Build

```bash
git submodule update --init --recursive
cmake . -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

**Windows:** Run the cmake commands from a **VS x64 Developer Command Prompt** (search for it in the Start menu). If using PowerShell, call `VsDevCmd.bat -arch=x64` first. Also pass `-DSFML_DIR="SFML-2.6.2/lib/cmake/SFML"` to the first cmake command.

The executable is placed at `VI-RT.exe` (Windows) or `./VI-RT` (Linux) in the project root.

## Run

**Windows** — SFML and OIDN DLLs must be on PATH:
```powershell
$env:Path = "$PWD\SFML-2.6.2\bin;$PWD\_deps\oidn_prebuilt-src\bin;$env:Path"
.\VI-RT.exe
```

**Linux:**
```bash
./VI-RT
```

## Docs

- [Texture Mapping Practical](docs/texture-mapping-practical.md)
- [NVIDIA FLIP Practical](docs/nvidia-flip-practical.md)
- [glTF and Texture Mapping Diagrams](docs/diagrams.md)

## Progressive Rendering Demo

Pass `--progressive` to enable the interactive SFML window with progressive path tracing. Without this flag the renderer does a fixed-spp offline render and writes `image.ppm` directly.

```powershell
$env:Path = "$PWD\SFML-2.6.2\bin;$PWD\_deps\oidn_prebuilt-src\bin;$env:Path"
.\VI-RT.exe --progressive
.\VI-RT.exe --scene scenes\cornell-box.gltf --progressive
```

Close the window to stop rendering and save `image.ppm`.

## Participating Media Demo

The constant-density participating media demo uses the bundled
`scenes/cornell-box.gltf` Cornell Box and renders it with a global homogeneous
medium:

```powershell
$env:Path = "$PWD\_deps\oidn_prebuilt-src\bin;$env:Path"
.\VI-RT.exe --medium-demo --spp 1
```

For a clearer preview, increase the sample count:

```powershell
.\VI-RT.exe --medium-demo --spp 8
```

To make the medium denser:

```powershell
.\VI-RT.exe --medium-demo --medium-density 0.45 --spp 8
```

The renderer writes `image.ppm` and, when denoising is enabled, `image-OIDN.ppm`
in the project root. `--medium-demo` is a shortcut for loading the bundled
`scenes/cornell-box.gltf` scene with a default medium density of `0.15`.

For report comparisons, use the same bundled glTF scene and change only the
medium setting:

```powershell
.\VI-RT.exe --scene scenes\cornell-box.gltf --spp 8
Rename-Item image.ppm image-no-medium.ppm
Rename-Item image-OIDN.ppm image-no-medium-OIDN.ppm

.\VI-RT.exe --scene scenes\cornell-box.gltf --medium-density 0.15 --spp 8
Rename-Item image.ppm image-medium-density-015.ppm
Rename-Item image-OIDN.ppm image-medium-density-015-OIDN.ppm

.\VI-RT.exe --scene scenes\cornell-box.gltf --medium-density 0.45 --spp 8
Rename-Item image.ppm image-medium-density-045.ppm
Rename-Item image-OIDN.ppm image-medium-density-045-OIDN.ppm
```
