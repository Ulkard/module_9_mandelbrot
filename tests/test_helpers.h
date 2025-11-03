#include "stdexec/__detail/__receivers.hpp"
#include <exception>
#include <print>

template <typename T>
struct Storage {
    T result;
    std::exception_ptr exception_ptr;
    bool value_received = false;
    bool error_received = false;
    bool stopped_received = false;
};

template <typename T>
class SimpleReceiver {
public:
    using receiver_concept = stdexec::receiver_t;

    SimpleReceiver(Storage<T> &storage) : storage_(storage) {};

    void set_value(T value) noexcept {
        storage_.result = std::move(value);
        storage_.value_received = true;
    }

    void set_error(std::exception_ptr ep) noexcept {
        storage_.exception_ptr = ep;
        storage_.error_received = true;
    }

    void set_stopped() noexcept { storage_.stopped_received = true; }

    Storage<T> &storage_;
};
