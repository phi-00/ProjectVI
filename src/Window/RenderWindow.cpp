#include "RenderWindow.hpp"
#include <algorithm>
#include <cmath>

namespace VI {

    RenderWindow::RenderWindow(int w, int h, const std::string& title)
        : window_(sf::VideoMode(w, h), title),
        width_(w), height_(h)
    {
        texture_.create(w, h);
        sprite_.setTexture(texture_);
        pixelBuffer_.resize(w * h * 4); 
    }

    bool RenderWindow::IsOpen() const {
        return window_.isOpen();
    }

    void RenderWindow::PollEvents() {
        sf::Event event;
        while (window_.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window_.close();
            
            if (event.type == sf::Event::KeyPressed &&
                event.key.code == sf::Keyboard::Escape)
                window_.close();
        }
    }

    void RenderWindow::Display(const Image& image) {
        ImageToRGBA(image);
        texture_.update(pixelBuffer_.data());
        window_.clear();
        window_.draw(sprite_);
        window_.display();
    }

    void RenderWindow::ImageToRGBA(const Image& image) {
        auto tonemapChannel = [](float v) -> sf::Uint8 {
            v = std::max(0.f, std::min(1.f, v));
            v = std::pow(v, 1.f / 2.2f);
            return static_cast<sf::Uint8>(v * 255.f);
        };

        for (int y = 0; y < height_; ++y) {
            for (int x = 0; x < width_; ++x) {
                const RGB& c = image.Get(x, y);
                int i = y * width_ + x;
                pixelBuffer_[i * 4 + 0] = tonemapChannel(c.r);
                pixelBuffer_[i * 4 + 1] = tonemapChannel(c.g);
                pixelBuffer_[i * 4 + 2] = tonemapChannel(c.b);
                pixelBuffer_[i * 4 + 3] = 255;
            }
        }
    }

} 