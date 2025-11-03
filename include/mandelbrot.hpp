#pragma once

#include "mandelbrot_fractal_utils.hpp"
#include "mandelbrot_renderer.hpp"
#include "types.hpp"
#include <print>
#include <stdexcept>

template <typename Receiver>
struct CalculateOperationState {

    Receiver receiver_;
    mandelbrot::ViewPort viewport_;
    RenderSettings settings_;
    AppState &state_;

    explicit CalculateOperationState(Receiver &&r, RenderSettings settings, AppState &state)
        : receiver_(r), viewport_(state.viewport), settings_(settings), state_(state) {}

    void start() noexcept {
        try {
            exec();
        } catch (...) {
            stdexec::set_error(std::move(receiver_), std::current_exception());
        }
    }

private:
    MandelbrotRenderer renderer_;
    RenderResult result_;

    void exec() {
        using namespace mandelbrot;

        TimeChecker tc("RenderAsync");
        if (state_.need_rerender) {
            auto task = renderer_.RenderAsync<THREAD_POOL_SIZE>(viewport_, settings_);
            auto result = stdexec::sync_wait(std::move(task));
            if (result) {
                result_ = std::get<0>(*result);
            } else {
                receiver_.set_error(std::runtime_error("RenderAsync failed"));
            }
        } else {
            std::println("CalculateOperationState::state_.need_rerender = false");
        }
        tc.count();

        receiver_.set_value(result_);
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
        return CalculateOperationState<std::decay_t<Receiver>>(std::forward<Receiver>(receiver), render_settings_,
                                                               state_);
    }

private:
    RenderSettings render_settings_;
    MandelbrotRenderer &renderer_;
    AppState &state_;
};
