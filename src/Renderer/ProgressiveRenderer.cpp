#include "ProgressiveRenderer.hpp"
#include "Renderer/Renderer.hpp"
#include "Shaders/PathTracingShader.hpp"

namespace VI {

void ProgressiveRenderer::ResetBuffer(int w, int h) {
    width_  = w;
    height_ = h;
    accumBuffer_.assign(w * h, RGB{0, 0, 0});
}

Image ProgressiveRenderer::AverageBuffer(int passCount) const {
    Image out{width_, height_};
    const float inv = 1.0f / static_cast<float>(passCount);
    for (int y = 0; y < height_; ++y)
        for (int x = 0; x < width_; ++x)
            out.Set(x, y, accumBuffer_[x + y * width_] * inv);
    return out;
}

void ProgressiveRenderer::Render(
    const Scene& scene,
    const Camera& camera,
    int maxPasses,
    FrameCallback onFrameReady)
{
    auto [fw, fh] = camera.GetResolution();
    ResetBuffer(static_cast<int>(fw), static_cast<int>(fh));

    Renderer renderer;
    PathTracingShader shader{{0.f, 0.f, 0.f}, DirectIlluminationMode::Importance};

    int pass = 0;
    while (maxPasses == 0 || pass < maxPasses) {
        ++pass;

        Image singlePass = renderer.Render(scene, camera, shader, 1, true);

        for (int y = 0; y < height_; ++y)
            for (int x = 0; x < width_; ++x)
                accumBuffer_[x + y * width_] += singlePass.Get(x, y);

        lastImage_ = AverageBuffer(pass);
        bool keepGoing = true;
        onFrameReady(*lastImage_, pass, keepGoing);
        if (!keepGoing) break;
    }
}

} 
