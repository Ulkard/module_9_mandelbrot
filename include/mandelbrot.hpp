#pragma once

#include "mandelbrot_renderer.hpp"
#include "types.hpp"
#include <print>

template <typename Receiver>
struct CalculateOperationState {

    Receiver receiver_;
    mandelbrot::ViewPort viewport_;
    RenderSettings settings_;
    PixelRegion region_;
    AppState &state_;

    void start() noexcept {
        try {
            exec();
        } catch (...) {
            stdexec::set_error(std::move(receiver_), std::current_exception());
        }
    }

private:
    MandelbrotRenderer renderer_;

    void exec() {
        using namespace mandelbrot;
        if (state_.need_rerender) {
            receiver_.set_value(renderer_.RenderAsync<THREAD_POOL_SIZE>(viewport_, settings_));
        } else {
            std::print("CalculateOperationState::state_.need_rerender = false");
        }
    }
};

class CalculateMandelbrotAsyncSender {
public:
    explicit CalculateMandelbrotAsyncSender(AppState &state, RenderSettings render_settings,
                                            MandelbrotRenderer &renderer)
        : state_(state), render_settings_{render_settings}, renderer_{renderer} {}

    using sender_concept = stdexec::sender_t;

    template <typename Env>
    auto get_completion_signatures(Env &&) const {
        return stdexec::completion_signatures<stdexec::set_value_t(RenderResult),
                                              stdexec::set_error_t(std::exception_ptr)>{};
    }

    template <typename Receiver>
    auto connect(Receiver &&receiver) const {
        return CalculateOperationState<std::decay_t<Receiver>>(std::forward<Receiver>(receiver));
    }

private:
    RenderSettings render_settings_;
    MandelbrotRenderer &renderer_;
    AppState &state_;
};
