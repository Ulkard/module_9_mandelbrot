#pragma once

#include <cstddef>
#include <cstdint>
#include <exec/static_thread_pool.hpp>
#include <stdexec/execution.hpp>

#include "mandelbrot_sender.hpp"
#include "stdexec/__detail/__execution_fwd.hpp"
#include "stdexec/__detail/__just.hpp"
#include "stdexec/__detail/__let.hpp"
#include "stdexec/__detail/__then.hpp"
#include "stdexec/__detail/__when_all.hpp"
#include "types.hpp"
#include <print>

class MandelbrotRenderer {
private:
    exec::static_thread_pool thread_pool_;

public:
    explicit MandelbrotRenderer(std::uint32_t num_threads = std::thread::hardware_concurrency())
        : thread_pool_{num_threads} {}

    template <size_t N>
    [[nodiscard]] auto RenderAsync(mandelbrot::ViewPort viewport, RenderSettings settings) {
        namespace ex = stdexec;
        const uint32_t rows_per_task = settings.height / N;
        auto scheduler = thread_pool_.get_scheduler();

        auto makeMandelbrotSenderWithIdx = [&](uint32_t i) {
            return MandelbrotSender(
                viewport, settings,
                {i * rows_per_task, std::min((i + 1) * rows_per_task - 1, settings.height), 0, settings.width});
        };

        auto combined = [&]<size_t... Is>(std::index_sequence<Is...>) {
            return ex::when_all(
                (ex::schedule(scheduler) | ex::let_value([&]() { return makeMandelbrotSenderWithIdx(Is); }) |
                 ex::then([&](PixelMatrix &&iters) { return itersToColors(std::move(iters), settings); }))...);
        }(std::make_index_sequence<N>{});

        return combined | ex::then([&](auto... color_matrices) {
                   return ex::just(
                       makeRenderResult<N>(viewport, settings, rows_per_task, std::move(color_matrices)...));
               });
    }

private:
    ColorMatrix itersToColors(PixelMatrix pixel_data, RenderSettings settings) {
        ColorMatrix color_data;
        color_data.reserve(pixel_data.size());

        for (const auto &row : pixel_data) {
            color_data.push_back(std::vector<mandelbrot::RgbColor>{});
            color_data.back().reserve(row.size());

            for (uint32_t iter : row) {
                color_data.back().push_back(mandelbrot::IterationsToColor(iter, settings.max_iterations));
            }
        }

        return color_data;
    }

    template <size_t N>
    RenderResult makeRenderResult(mandelbrot::ViewPort viewport, RenderSettings settings, uint32_t rows_per_task,
                                  auto &&...color_matrices) {
        ColorMatrix color_data(settings.height, std::vector<mandelbrot::RgbColor>(settings.width));

        // Extract each ColorMatrix from the tuple and combine them
        [&]<size_t... Is>(std::index_sequence<Is...>) {
            ((processColorMatrix(color_matrices, color_data, Is * rows_per_task, settings)), ...);
        }(std::make_index_sequence<N>{});

        return RenderResult{.pixel_data = {},  // Not used in current flow
                            .color_data = std::move(color_data),
                            .viewport = viewport,
                            .settings = settings,
                            .render_time = {}};
    }

    void processColorMatrix(const ColorMatrix &source, ColorMatrix &dest, uint32_t start_row,
                            const RenderSettings &settings) {
        for (uint32_t y = 0; y < source.size() && (start_row + y) < settings.height; ++y) {
            for (uint32_t x = 0; x < source[y].size() && x < settings.width; ++x) {
                dest[start_row + y][x] = source[y][x];
            }
        }
    }
};
