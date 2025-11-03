#include <gtest/gtest.h>

#include "mandelbrot.hpp"
#include "test_helpers.h"
#include "types.hpp"

class CalculateMandelbrotAsyncSenderTest : public testing::Test {
protected:
    void SetUp() override {
        state.viewport = {-2.0, 1.0, -1.5, 1.5};
        state.need_rerender = true;
        settings.width = 100;
        settings.height = 100;
        settings.max_iterations = 50;
        settings.escape_radius = 2.0;
    }

    AppState state;
    RenderSettings settings;
    MandelbrotRenderer renderer{2};
    Storage<RenderResult> storage;

    using ReceiverT = SimpleReceiver<RenderResult>;
};

TEST_F(CalculateMandelbrotAsyncSenderTest, SenderConcept) {
    static_assert(stdexec::sender<CalculateMandelbrotAsyncSender>, "CalculateMandelbrotAsyncSender should be a sender");
}

TEST_F(CalculateMandelbrotAsyncSenderTest, ConnectAndExecuteWithRerender) {
    CalculateMandelbrotAsyncSender sender{state, settings, renderer};
    state.need_rerender = true;

    ReceiverT receiver{storage};

    auto operation = sender.connect(std::move(receiver));
    operation.start();

    EXPECT_TRUE(storage.value_received || storage.error_received || storage.stopped_received);

    if (storage.value_received) {
        EXPECT_EQ(storage.result.viewport.x_min, -2.0);
        EXPECT_EQ(storage.result.viewport.x_max, 1.0);
        EXPECT_EQ(storage.result.settings.width, 100);
        EXPECT_EQ(storage.result.settings.height, 100);
        EXPECT_FALSE(storage.error_received);
        EXPECT_FALSE(storage.stopped_received);
    } else if (storage.error_received) {
        EXPECT_TRUE(storage.exception_ptr != nullptr);
    }
}

TEST_F(CalculateMandelbrotAsyncSenderTest, NoRerenderReturnsEmptyResult) {
    CalculateMandelbrotAsyncSender sender{state, settings, renderer};
    state.need_rerender = false;

    ReceiverT receiver{storage};

    auto operation = sender.connect(std::move(receiver));
    operation.start();

    EXPECT_TRUE(storage.value_received);
    EXPECT_FALSE(storage.error_received);
    EXPECT_FALSE(storage.stopped_received);

    EXPECT_TRUE(storage.result.pixel_data.empty());
    EXPECT_TRUE(storage.result.color_data.empty());
    EXPECT_EQ(storage.result.render_time, std::chrono::milliseconds{});
}

TEST_F(CalculateMandelbrotAsyncSenderTest, RenderAsyncFailurePropagatesError) {
    // invalid settings
    settings.width = 0;
    settings.height = 0;
    CalculateMandelbrotAsyncSender sender{state, settings, renderer};

    ReceiverT receiver{storage};

    auto operation = sender.connect(std::move(receiver));
    operation.start();

    EXPECT_FALSE(storage.value_received);
    EXPECT_TRUE(storage.error_received);
    EXPECT_FALSE(storage.stopped_received);

    if (storage.error_received) {
        EXPECT_TRUE(storage.exception_ptr != nullptr);
        try {
            std::rethrow_exception(storage.exception_ptr);
        } catch (const std::runtime_error &e) {
            EXPECT_STREQ(e.what(), "RenderAsync failed");
        } catch (const std::exception &e) {
            // Other exceptions are also possible due to invalid settings
            SUCCEED() << "Received expected exception: " << e.what();
        } catch (...) {
            FAIL() << "Unexpected exception type";
        }
    }
}

TEST_F(CalculateMandelbrotAsyncSenderTest, DifferentViewPorts) {
    std::vector<mandelbrot::ViewPort> viewports = {
        {-2.0, 1.0, -1.5, 1.5}, {-1.0, 1.0, -1.0, 1.0}, {-0.5, -0.4, -0.5, -0.4}  // Zoomed in region
    };

    for (const auto &viewport : viewports) {
        state.viewport = viewport;
        CalculateMandelbrotAsyncSender sender{state, settings, renderer};

        ReceiverT receiver{storage};

        auto operation = sender.connect(std::move(receiver));
        operation.start();

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        if (storage.value_received) {
            EXPECT_EQ(storage.result.viewport.x_min, viewport.x_min);
            EXPECT_EQ(storage.result.viewport.x_max, viewport.x_max);
            EXPECT_EQ(storage.result.viewport.y_min, viewport.y_min);
            EXPECT_EQ(storage.result.viewport.y_max, viewport.y_max);
        }

        storage = {};
    }
}

TEST_F(CalculateMandelbrotAsyncSenderTest, DifferentRenderSettings) {
    std::vector<RenderSettings> testSettings = {{800, 600, 100, 2.0}, {100, 100, 50, 2.0}, {1920, 1080, 200, 2.0}};

    for (const auto &testSetting : testSettings) {
        CalculateMandelbrotAsyncSender sender{state, testSetting, renderer};

        ReceiverT receiver{storage};

        auto operation = sender.connect(std::move(receiver));
        operation.start();

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        if (storage.value_received) {
            EXPECT_EQ(storage.result.settings.width, testSetting.width);
            EXPECT_EQ(storage.result.settings.height, testSetting.height);
            EXPECT_EQ(storage.result.settings.max_iterations, testSetting.max_iterations);
            EXPECT_EQ(storage.result.settings.escape_radius, testSetting.escape_radius);
        }

        storage = {};
    }
}
