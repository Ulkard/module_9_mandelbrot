#include <gtest/gtest.h>

#include "mandelbrot_renderer.hpp"

class MandelbrotRendererTest : public ::testing::Test {
protected:
    void SetUp() override {
        default_viewport = mandelbrot::ViewPort{-2.5, 1.5, -2.0, 2.0};
        default_settings = RenderSettings{800, 600, 100, 2.0};
    }

    void TearDown() override {}

    RenderResult RenderSync(mandelbrot::ViewPort viewport, RenderSettings settings) {
        auto task = renderer.RenderAsync<THREAD_POOL_SIZE>(viewport, settings);
        auto result = stdexec::sync_wait(std::move(task));
        if (result) {
            return (std::get<0>(*result));
        } else {
            return RenderResult{};
        }
    }

    mandelbrot::ViewPort default_viewport;
    RenderSettings default_settings;
    MandelbrotRenderer renderer;
};

TEST_F(MandelbrotRendererTest, PixelToComplexConversion) {
    mandelbrot::ViewPort viewport{0.0, 2.0, 0.0, 2.0};
    uint32_t width = 2, height = 2;

    // Test corners
    auto top_left = mandelbrot::Pixel2DToComplex(0, 0, viewport, width, height);
    EXPECT_DOUBLE_EQ(top_left.real(), 0.0);
    EXPECT_DOUBLE_EQ(top_left.imag(), 0.0);

    auto bottom_right = mandelbrot::Pixel2DToComplex(1, 1, viewport, width, height);
    EXPECT_DOUBLE_EQ(bottom_right.real(), 1.0);
    EXPECT_DOUBLE_EQ(bottom_right.imag(), 1.0);

    auto center = mandelbrot::Pixel2DToComplex(1, 1, viewport, 4, 4);
    EXPECT_DOUBLE_EQ(center.real(), 0.5);
    EXPECT_DOUBLE_EQ(center.imag(), 0.5);
}

TEST_F(MandelbrotRendererTest, ColorConversion) {
    // max iterations (should be black)
    auto black_color = mandelbrot::IterationsToColor(100, 100);
    EXPECT_EQ(black_color.r, 0);
    EXPECT_EQ(black_color.g, 0);
    EXPECT_EQ(black_color.b, 0);

    // zero iterations
    auto color = mandelbrot::IterationsToColor(0, 100);
    // valid color (not necessarily black)
    EXPECT_LE(color.r, 255);
    EXPECT_LE(color.g, 255);
    EXPECT_LE(color.b, 255);

    // mid-range iterations
    color = mandelbrot::IterationsToColor(50, 100);
    EXPECT_LE(color.r, 255);
    EXPECT_LE(color.g, 255);
    EXPECT_LE(color.b, 255);
}

TEST_F(MandelbrotRendererTest, BasicRendering) {
    auto result = RenderSync(default_viewport, default_settings);

    EXPECT_EQ(result.viewport.x_min, default_viewport.x_min);
    EXPECT_EQ(result.viewport.x_max, default_viewport.x_max);
    EXPECT_EQ(result.viewport.y_min, default_viewport.y_min);
    EXPECT_EQ(result.viewport.y_max, default_viewport.y_max);

    EXPECT_EQ(result.settings.width, default_settings.width);
    EXPECT_EQ(result.settings.height, default_settings.height);
    EXPECT_EQ(result.settings.max_iterations, default_settings.max_iterations);
    EXPECT_EQ(result.settings.escape_radius, default_settings.escape_radius);
}

TEST_F(MandelbrotRendererTest, RenderResultCorrectStructure) {
    auto result = RenderSync(default_viewport, default_settings);

    // matrices have correct dimensions
    EXPECT_EQ(result.color_data.size(), default_settings.height);

    if (!result.color_data.empty()) {
        EXPECT_EQ(result.color_data[0].size(), default_settings.width);
    }
}

TEST_F(MandelbrotRendererTest, AlternativeViewport) {
    mandelbrot::ViewPort zoomed_viewport{-1.0, -0.5, -0.5, 0.5};
    auto result = RenderSync(zoomed_viewport, default_settings);

    EXPECT_EQ(result.viewport.x_min, zoomed_viewport.x_min);
    EXPECT_EQ(result.viewport.x_max, zoomed_viewport.x_max);
    EXPECT_EQ(result.viewport.y_min, zoomed_viewport.y_min);
    EXPECT_EQ(result.viewport.y_max, zoomed_viewport.y_max);
}

TEST_F(MandelbrotRendererTest, AlternativeSettingsSet) {
    RenderSettings high_res_settings{1920, 1080, 500, 4.0};
    auto result = RenderSync(default_viewport, high_res_settings);

    EXPECT_EQ(result.settings.width, high_res_settings.width);
    EXPECT_EQ(result.settings.height, high_res_settings.height);
    EXPECT_EQ(result.settings.max_iterations, high_res_settings.max_iterations);
    EXPECT_EQ(result.settings.escape_radius, high_res_settings.escape_radius);

    EXPECT_EQ(result.color_data.size(), high_res_settings.height);
}

TEST_F(MandelbrotRendererTest, PixelValuesValid) {
    auto result = RenderSync(default_viewport, default_settings);

    for (const auto &row : result.pixel_data) {
        for (auto pixel : row) {
            EXPECT_LE(pixel, default_settings.max_iterations);
        }
    }
}

TEST_F(MandelbrotRendererTest, ColorValuesValid) {
    auto result = RenderSync(default_viewport, default_settings);

    for (const auto &row : result.color_data) {
        for (const auto &color : row) {
            // RGB values should be valid (0-255)
            EXPECT_LE(color.r, 255);
            EXPECT_LE(color.g, 255);
            EXPECT_LE(color.b, 255);
        }
    }
}

TEST_F(MandelbrotRendererTest, VerySmallResolution) {
    RenderSettings tiny_settings{1, 1, 10, 2.0};
    auto result = RenderSync(default_viewport, tiny_settings);

    EXPECT_EQ(result.color_data.size(), 1);
    EXPECT_EQ(result.color_data[0].size(), 1);
}

TEST_F(MandelbrotRendererTest, MinimumIterations) {
    RenderSettings min_iter_settings{100, 100, 1, 2.0};
    auto result = RenderSync(default_viewport, min_iter_settings);

    for (const auto &row : result.pixel_data) {
        for (auto pixel : row) {
            EXPECT_LE(pixel, 1);  // Should be either 0 or 1
        }
    }
}

TEST_F(MandelbrotRendererTest, IterationCalculation) {
    // Point known to escape quickly
    mandelbrot::Complex quick_escape_point{2.0, 2.0};
    auto iterations = mandelbrot::CalculateIterationsForPoint(quick_escape_point, 100, 2.0);
    EXPECT_LT(iterations, 100);  // Should escape quickly

    // Point that should be in the set (origin)
    mandelbrot::Complex origin{0.0, 0.0};
    iterations = mandelbrot::CalculateIterationsForPoint(origin, 100, 2.0);
    EXPECT_EQ(iterations, 100);

    // Point on the boundary
    mandelbrot::Complex boundary_point{-1.0, 0.0};
    iterations = mandelbrot::CalculateIterationsForPoint(boundary_point, 100, 2.0);
    EXPECT_GT(iterations, 0);  // Should have some iterations
    EXPECT_LE(iterations, 100);
}
