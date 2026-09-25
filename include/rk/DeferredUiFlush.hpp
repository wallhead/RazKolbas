#pragma once
#include "rk/Result.hpp"
#include <atomic>
#include <mutex>

namespace rk {
using DeferredUiFlushFn=void(*)(void*);

class DeferredUiFlushForwarder {
public:
    Result<bool> configure(DeferredUiFlushFn original,
        DeferredUiFlushFn beforeOriginal);
    void dispatch(void* renderer) noexcept;
private:
    std::mutex configureMutex_;
    std::atomic<DeferredUiFlushFn> original_{nullptr};
    std::atomic<DeferredUiFlushFn> beforeOriginal_{nullptr};
};
}
