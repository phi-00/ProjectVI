#pragma once
#include <SFML/Graphics.hpp>
#include "Image/Image.hpp"
#include <string>

namespace VI {

class RenderWindow {
public:
    RenderWindow(int width, int height, const std::string& title);

    bool IsOpen() const;

    void Display(const Image& image);

    void PollEvents();

private:
    sf::RenderWindow window_;
    sf::Texture      texture_;
    sf::Sprite       sprite_;
    std::vector<sf::Uint8> pixelBuffer_;
    int width_, height_;

    void ImageToRGBA(const Image& image);
};

}