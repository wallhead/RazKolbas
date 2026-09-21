#pragma once
#include "rk/Result.hpp"
#include <atomic>
#include <cstdint>
#include <mutex>

namespace rk {
using MenuDisplayFn=void(*)(void*,std::uint32_t,std::uint32_t,std::uint32_t) noexcept;

class MenuDisplayForwarder {
public:
    Result<bool> configure(MenuDisplayFn original,MenuDisplayFn beforeOriginal);
    void dispatch(void* first,std::uint32_t second,std::uint32_t third,
        std::uint32_t fourth) noexcept;
private:
    std::mutex configureMutex_;
    std::atomic<MenuDisplayFn> original_{nullptr};
    std::atomic<MenuDisplayFn> beforeOriginal_{nullptr};
};
}
