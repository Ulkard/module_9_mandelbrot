#pragma once

#include <cstdint>
#include <stdexec/execution.hpp>

#include "mandelbrot_fractal_utils.hpp"
#include "types.hpp"

template <typename Receiver>
struct MandelbrotOperationState {

    Receiver receiver_;
    mandelbrot::ViewPort viewport_;
    RenderSettings settings_;
    PixelRegion region_;

    void start() noexcept {
        try {
            exec();
        } catch (...) {
            stdexec::set_error(std::move(receiver_), std::current_exception());
        }
    }

private:
    void exec() {
        using namespace mandelbrot;
        PixelMatrix result;
        uint32_t width = region_.end_col - region_.start_col;
        uint32_t height = region_.end_row - region_.start_row;
        result.reserve(height);

        for (uint32_t y = region_.start_row; y < region_.end_row; ++y) {
            result.push_back(std::vector<uint32_t>{});
            result.back().reserve(width);
            for (uint32_t x = region_.start_col; x < region_.end_col; ++x) {
                std::complex<double> point = Pixel2DToComplex(x, y, viewport_, settings_.width, settings_.height);
                uint32_t iter = CalculateIterationsForPoint(point, settings_.max_iterations, settings_.escape_radius);
                result.back().push_back(iter);
            }
        }

        receiver_.set_value(result);
    }
};

template <typename Receiver>
struct MandelbrotSender {
    mandelbrot::ViewPort viewport_;
    RenderSettings settings_;
    PixelRegion region_;

    MandelbrotSender(mandelbrot::ViewPort viewport, RenderSettings settings, PixelRegion region)
        : viewport_(viewport), settings_(settings), region_(region) {}

    using sender_concept = stdexec::sender_t;

    template <typename Env>
    auto get_completion_signatures(Env &&) const {
        return stdexec::completion_signatures<stdexec::set_value_t(PixelMatrix),
                                              stdexec::set_error_t(std::exception_ptr)>{};
    }

    template <typename Receiver>
    auto connect(Receiver &&receiver) const {
        return MandelbrotOperationState<std::decay_t<Receiver>>(std::forward<Receiver>(receiver));
    }
};

[[nodiscard]] inline auto makeMandelbrotSender(mandelbrot::ViewPort viewport, RenderSettings settings,
                                               PixelRegion region) {
    return MandelbrotSender<void>{viewport, settings, region};
}
