#pragma once

#include <SFML/Graphics.hpp>
#include <SFML/Graphics/Color.hpp>
#include <algorithm>
#include <cstddef>
#include <print>
#include <stdexcept>
#include <stdexec/execution.hpp>

#include "types.hpp"

class SFMLRender {
public:
    template <typename Receiver>
    struct OperationState {
        Receiver receiver_;
        RenderResult render_result_;
        sf::Image &image_;
        sf::Texture &texture_;
        sf::Sprite &sprite_;
        sf::RenderWindow &window_;
        RenderSettings render_settings_;

        void start() noexcept {
            try {
                exec();
            } catch (...) {
                stdexec::set_error(std::move(receiver_), std::current_exception());
            }
        }

    private:
        void exec() {
            auto &raw_points = render_result_.color_data;
            if (raw_points.empty() || raw_points.front().empty()) {
                throw std::invalid_argument("cant create image from nothing");
            }

            uint32_t width = std::min<uint32_t>(render_settings_.width, raw_points.front().size());
            uint32_t height = std::min<uint32_t>(render_settings_.height, raw_points.size());
            image_.create(width, height);
            for (size_t y = 0; y < height; ++y) {
                for (size_t x = 0; x < width; ++x) {
                    image_.setPixel(x, y, sf::Color{raw_points[y][x].r, raw_points[y][x].g, raw_points[y][x].b});
                }
            }

            texture_.create(width, height);
            texture_.update(image_);

            sprite_.setTexture(texture_);
            window_.clear();
            window_.draw(sprite_);
            window_.display();

            receiver_.set_value();
        }
    };

    RenderResult render_result_;
    sf::Image &image_;
    sf::Texture &texture_;
    sf::Sprite &sprite_;
    sf::RenderWindow &window_;
    RenderSettings render_settings_;

    SFMLRender(RenderResult render_result, sf::Image &image, sf::Texture &texture, sf::Sprite &sprite,
               sf::RenderWindow &window, RenderSettings render_settings)
        : render_result_(render_result), image_{image}, texture_{texture}, sprite_{sprite}, window_{window},
          render_settings_{render_settings} {}

    using sender_concept = stdexec::sender_t;

    template <typename Env>
    auto get_completion_signatures(Env &&) const {
        return stdexec::completion_signatures<stdexec::set_value_t(), stdexec::set_error_t(std::exception_ptr)>{};
    }

    template <typename Receiver>
    auto connect(Receiver &&receiver) const {
        return OperationState<std::decay_t<Receiver>>(std::forward<Receiver>(receiver), render_result_, image_,
                                                      texture_, sprite_, window_, render_settings_);
    }
};
