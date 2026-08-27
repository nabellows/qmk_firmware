#pragma once

#include "compat.hpp"
#include "timer.h"
#include "util.hpp"

enum class DeferredExecutors {
    RGB_FLASH,
    ENUM_END
};

using DeferredCallback = void(*)();

template<DeferredExecutors name>
struct DeferredExecutor {
    DeferredCallback callback = nullptr;
    uint32_t timer = 0;
    struct {
        bool started : 1 = false;
        uint32_t duration_ms : 31 = 0;
    };
    constexpr DeferredExecutor() {}
    DeferredExecutor(uint32_t duration, DeferredCallback callback)
    : callback{ callback }, duration_ms{ duration } {
        start();
    }
    DeferredExecutor(DeferredCallback callback)
    : DeferredExecutor{ 0, callback } {}

    DeferredExecutor& operator=(DeferredExecutor<name>&& other) {
        callback = other.callback;
        started = other.started;
        duration_ms = other.duration_ms;
        start();
        return *this;
    }

    DeferredExecutor& operator=(DeferredCallback callback) {
        this->callback = callback;
        start();
        return *this;
    }

    void start() {
        timer = timer_read32();
        started = true;
    }

    void start(uint32_t duration) {
        this->duration_ms = duration;
        start();
    }

    void poll() {
        if (callback && started && timer_elapsed32(timer) >= duration_ms) {
            started = false;
            callback();
        }
    }
};

template<DeferredExecutors name>
inline DeferredExecutor<name> kDeferredExecutor;

inline void deferred_executor_housekeeping() {
    foreach_enum<DeferredExecutors>([]<auto name>(){
        kDeferredExecutor<name>.poll();
    });
}
