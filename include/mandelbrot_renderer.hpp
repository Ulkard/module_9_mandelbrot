#pragma once

#include <cstdint>
#include <exec/static_thread_pool.hpp>
#include <stdexec/execution.hpp>

#include "mandelbrot_sender.hpp"
#include "stdexec/__detail/__execution_fwd.hpp"
#include "stdexec/__detail/__then.hpp"
#include "stdexec/__detail/__when_all.hpp"
#include "types.hpp"

class MandelbrotRenderer {
private:
    exec::static_thread_pool thread_pool_;

public:
    explicit MandelbrotRenderer(std::uint32_t num_threads = std::thread::hardware_concurrency())
        : thread_pool_{num_threads} {}

    template <size_t N>
    [[nodiscard]] auto RenderAsync(mandelbrot::ViewPort viewport, RenderSettings settings) {
        const uint32_t rows_per_task = settings.height / N;
        auto scheduler = thread_pool_.get_scheduler();

        auto makeMandelbrotSenderWithIdx = [&](uint32_t i) {
            return makeMandelbrotSender(
                viewport, settings,
                {i * rows_per_task, std::min((i + 1) * rows_per_task - 1, settings.height), 0, settings.width});
        };

        return [&]<size_t... Is>(std::index_sequence<Is...>) {
            return stdexec::when_all(
                (stdexec::schedule(scheduler) | makeMandelbrotSenderWithIdx(Is) |
                 stdexec::then([&](PixelMatrix iters) { return itersToColors(iters, settings); }))...);
        }(std::make_index_sequence<N>{}) |
               stdexec::then([&](std::array<ColorMatrix, N> color_matrices) {
                   return compileRenderResult<N>(color_matrices, viewport, settings, rows_per_task);
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
    RenderResult compileRenderResult(std::array<ColorMatrix, N> color_matrices, mandelbrot::ViewPort viewport,
                                     RenderSettings settings, uint32_t rows_per_task) {
        // Создаем результирующие матрицы
        PixelMatrix pixel_data(settings.height, std::vector<uint32_t>(settings.width, 0));
        ColorMatrix color_data(settings.height, std::vector<mandelbrot::RgbColor>(settings.width));

        // Объединяем результаты из всех областей
        for (uint32_t i = 0; i < N; ++i) {
            uint32_t start_row = i * rows_per_task;
            const auto &matrix = color_matrices[i];

            for (uint32_t y = 0; y < matrix.size(); ++y) {
                for (uint32_t x = 0; x < matrix[y].size(); ++x) {
                    uint32_t global_y = start_row + y;
                    if (global_y < settings.height && x < settings.width) {
                        color_data[global_y][x] = matrix[y][x];
                    }
                }
            }
        }

        return RenderResult{
            .pixel_data = {},
            .color_data = std::move(color_data),
            .viewport = viewport,
            .settings = settings,
            .render_time = {}  // TODO: measure ttc
        };
    }
};
