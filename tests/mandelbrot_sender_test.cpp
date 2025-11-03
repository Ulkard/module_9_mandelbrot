#include <gtest/gtest.h>

#include "mandelbrot_sender.hpp"
#include "test_helpers.h"
#include "types.hpp"

class MandelbrotSenderTest : public ::testing::Test {
protected:
    mandelbrot::ViewPort viewport{-2.0, -1.5, 1.0, 1.5};
    RenderSettings settings{800, 600, 100, 2.0};
    PixelRegion region = {0, 100, 0, 100};  // 100x100 region

    Storage<PixelMatrix> storage;

    using ReceiverT = SimpleReceiver<PixelMatrix>;
};

TEST_F(MandelbrotSenderTest, SenderConcept) {
    static_assert(stdexec::sender<MandelbrotSender>, "CalculateMandelbrotAsyncSender should be a sender");
}

TEST_F(MandelbrotSenderTest, ConnectReturnsOperationState) {
    MandelbrotSender sender(viewport, settings, region);
    ReceiverT receiver(storage);

    auto op_state = sender.connect(receiver);

    EXPECT_TRUE(std::is_move_constructible_v<decltype(op_state)>);
}

TEST_F(MandelbrotSenderTest, ComputesMandelbrot) {
    MandelbrotSender sender(viewport, settings, region);
    ReceiverT receiver(storage);

    auto op_state = sender.connect(receiver);
    op_state.start();

    EXPECT_TRUE(storage.value_received);
    EXPECT_FALSE(storage.error_received);
    EXPECT_FALSE(storage.stopped_received);

    // Check result dimensions
    EXPECT_EQ(storage.result.size(), region.end_row - region.start_row);
    if (!storage.result.empty()) {
        EXPECT_EQ(storage.result[0].size(), region.end_col - region.start_col);
    }
}

TEST_F(MandelbrotSenderTest, DifferentRegionSizes) {
    // Test with 1x1 region
    PixelRegion small_region = {0, 1, 0, 1};
    MandelbrotSender sender(viewport, settings, small_region);
    ReceiverT receiver(storage);

    auto op_state = sender.connect(receiver);
    op_state.start();

    EXPECT_TRUE(storage.value_received);
    EXPECT_EQ(storage.result.size(), 1);
    EXPECT_EQ(storage.result[0].size(), 1);

    // Test with larger region
    PixelRegion large_region = {0, 200, 0, 150};
    storage = {};
    MandelbrotSender sender2(viewport, settings, large_region);
    ReceiverT receiver2(storage);

    auto op_state2 = sender2.connect(receiver2);
    op_state2.start();

    EXPECT_TRUE(storage.value_received);
    EXPECT_EQ(storage.result.size(), 200);
    EXPECT_EQ(storage.result[0].size(), 150);
}

TEST_F(MandelbrotSenderTest, LargeRegion) {
    PixelRegion large_region = {0, 1000, 0, 1000};
    RenderSettings settings = {1000, 1000, 100, 2.0};

    MandelbrotSender sender(viewport, settings, large_region);
    ReceiverT receiver(storage);

    auto start_time = std::chrono::high_resolution_clock::now();

    auto op_state = sender.connect(receiver);
    op_state.start();

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    std::println("LargeRegion test: {}ms", duration.count());

    EXPECT_TRUE(storage.value_received);
    EXPECT_EQ(storage.result.size(), 1000);
    EXPECT_EQ(storage.result[0].size(), 1000);
}

TEST_F(MandelbrotSenderTest, EmptyRegion) {
    PixelRegion empty_region = {0, 0, 0, 0};
    MandelbrotSender sender(viewport, settings, empty_region);
    ReceiverT receiver(storage);

    auto op_state = sender.connect(receiver);
    op_state.start();

    EXPECT_TRUE(storage.value_received);
    EXPECT_EQ(storage.result.size(), 0);
}

TEST_F(MandelbrotSenderTest, DifferentViewports) {
    // Test with zoomed-in viewport
    mandelbrot::ViewPort zoomed_viewport = {-0.5, -0.5, 0.5, 0.5};
    MandelbrotSender sender(zoomed_viewport, settings, region);
    ReceiverT receiver(storage);

    auto op_state = sender.connect(receiver);
    op_state.start();

    EXPECT_TRUE(storage.value_received);
    EXPECT_FALSE(storage.error_received);
}

TEST_F(MandelbrotSenderTest, DifferentIterationSettings) {
    RenderSettings low_iter_settings = {100, 100, 10, 2.0};  // Low iterations
    MandelbrotSender sender(viewport, low_iter_settings, region);
    ReceiverT receiver(storage);

    auto op_state = sender.connect(receiver);
    op_state.start();

    EXPECT_TRUE(storage.value_received);
    EXPECT_FALSE(storage.error_received);

    // Check iterations within bounds
    for (const auto &row : storage.result) {
        for (uint32_t iter : row) {
            EXPECT_LE(iter, low_iter_settings.max_iterations);
        }
    }
}

TEST_F(MandelbrotSenderTest, OperationStateIsMovable) {
    MandelbrotSender sender(viewport, settings, region);
    ReceiverT receiver(storage);

    auto op_state1 = sender.connect(receiver);
    auto op_state2 = std::move(op_state1);  // Move construct

    // Should be able to start the moved operation state
    EXPECT_NO_THROW({ op_state2.start(); });
}
