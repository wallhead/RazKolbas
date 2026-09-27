#include <catch2/catch_test_macros.hpp>
#include "rk/NativeHudDimensions.hpp"
#include <array>

TEST_CASE("HUD dimension window restores reduced pairs after native UI",
    "[native_hud_dimensions]") {
    std::array<std::uint32_t,4> pairs{1707,960,1707,960};
    {
        rk::NativeHudDimensions window(pairs.data(),{1707,960},{2560,1440});
        REQUIRE(window.active());
        REQUIRE(pairs==std::array<std::uint32_t,4>{2560,1440,2560,1440});
    }
    REQUIRE(pairs==std::array<std::uint32_t,4>{1707,960,1707,960});
}

TEST_CASE("HUD dimension window rejects a mixed or native game state",
    "[native_hud_dimensions]") {
    std::array<std::uint32_t,4> mixed{1707,960,2560,1440};
    rk::NativeHudDimensions mixedWindow(mixed.data(),{1707,960},{2560,1440});
    REQUIRE_FALSE(mixedWindow.active());
    REQUIRE(mixed==std::array<std::uint32_t,4>{1707,960,2560,1440});
    std::array<std::uint32_t,4> native{2560,1440,2560,1440};
    rk::NativeHudDimensions nativeWindow(native.data(),{2560,1440},{2560,1440});
    REQUIRE_FALSE(nativeWindow.active());
}

TEST_CASE("HUD dimension window respects a later writer during restoration",
    "[native_hud_dimensions]") {
    std::array<std::uint32_t,4> pairs{1707,960,1707,960};
    rk::NativeHudDimensions window(pairs.data(),{1707,960},{2560,1440});
    REQUIRE(window.active());
    pairs[2]=1920;
    REQUIRE_FALSE(window.restore());
    REQUIRE(pairs==std::array<std::uint32_t,4>{1707,960,1920,960});
    REQUIRE_FALSE(window.active());
}
