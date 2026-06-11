# VI-RT-2526

To compile this project you need to do the following:

```
# Setup dependencies
git submodule update --init --recursive

mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

## Docs

- [Texture Mapping Practical](docs/texture-mapping-practical.md)
- [NVIDIA FLIP Practical](docs/nvidia-flip-practical.md)
- [glTF and Texture Mapping Diagrams](docs/diagrams.md)

## Participating Media Demo

The constant-density participating media demo uses the bundled
`scenes/cornell-box.gltf` Cornell Box and renders it with a global homogeneous
medium:

```powershell
$env:Path = "C:\Users\38240\Documents\GitHub\ProjectVI\build\_deps\oidn_prebuilt-src\bin;$env:Path"
.\build\Debug\VI-RT.exe --medium-demo --spp 1
```

For a clearer preview, increase the sample count:

```powershell
.\build\Debug\VI-RT.exe --medium-demo --spp 8
```

To make the medium denser:

```powershell
.\build\Debug\VI-RT.exe --medium-demo --medium-density 0.45 --spp 8
```

The renderer writes `image.ppm` and, when denoising is enabled, `image-OIDN.ppm`
in the project root. `--medium-demo` is a shortcut for loading the bundled
`scenes/cornell-box.gltf` scene with a default medium density of `0.15`.

For report comparisons, use the same bundled glTF scene and change only the
medium setting:

```powershell
.\build\Debug\VI-RT.exe --scene scenes\cornell-box.gltf --spp 8
Rename-Item image.ppm image-no-medium.ppm
Rename-Item image-OIDN.ppm image-no-medium-OIDN.ppm

.\build\Debug\VI-RT.exe --scene scenes\cornell-box.gltf --medium-density 0.15 --spp 8
Rename-Item image.ppm image-medium-density-015.ppm
Rename-Item image-OIDN.ppm image-medium-density-015-OIDN.ppm

.\build\Debug\VI-RT.exe --scene scenes\cornell-box.gltf --medium-density 0.45 --spp 8
Rename-Item image.ppm image-medium-density-045.ppm
Rename-Item image-OIDN.ppm image-medium-density-045-OIDN.ppm
```
