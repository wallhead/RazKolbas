#pragma once
#include <cstdint>

namespace rk {
struct FgPrivateSwapAdmission {
    std::uint32_t thread{};
    std::uintptr_t window{};
    std::uint32_t width{};
    std::uint32_t height{};
    bool nativeFactory{};
};
constexpr bool admitPrivateFgSwap(const FgPrivateSwapAdmission& expected,
    const FgPrivateSwapAdmission& actual) noexcept {
    return expected.thread&&expected.window&&expected.width&&expected.height&&
        expected.nativeFactory&&actual.nativeFactory&&
        expected.thread==actual.thread&&expected.window==actual.window&&
        expected.width==actual.width&&expected.height==actual.height;
}
}
