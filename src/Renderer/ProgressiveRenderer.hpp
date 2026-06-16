#pragma once
#include <functional>
#include <optional>
#include "Image/Image.hpp"
#include "Scene/Scene.hpp"
#include "Camera/Camera.hpp"
#include "Shaders/PathTracingShader.hpp"

namespace VI {

    using FrameCallback = std::function<void(const Image&, int passNumber, bool& keepGoing)>;

    class ProgressiveRenderer {
    public:
        void Render(
            const Scene& scene,
            const Camera& camera,
            int maxPasses,
            FrameCallback onFrameReady
        );

        const Image* GetLastImage() const { return lastImage_ ? &*lastImage_ : nullptr; }

    private:
        std::vector<RGB> accumBuffer_;
        std::optional<Image> lastImage_;
        int width_  = 0;
        int height_ = 0;

        Image AverageBuffer(int passCount) const;
        void  ResetBuffer(int w, int h);
    };

} 