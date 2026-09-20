#pragma once
#include "rk/Result.hpp"
#include <atomic>
#include <cstdint>
#include <mutex>

namespace rk {
// Static reference callback preserves RCX and EDX before invoking the game
// target. The observed call-site continuation overwrites RAX immediately.
// The semantic meaning of these two arguments is not yet established.
using WorldDrawFn=void(*)(void*,std::uint32_t) noexcept;

class WorldDrawForwarder {
public:
    Result<bool> configure(WorldDrawFn original,WorldDrawFn afterOriginal);
    void dispatch(void* first,std::uint32_t second) noexcept;
private:
    std::mutex configureMutex_;
    std::atomic<WorldDrawFn> original_{nullptr};
    std::atomic<WorldDrawFn> afterOriginal_{nullptr};
};
}
