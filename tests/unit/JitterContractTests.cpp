#include <catch2/catch_test_macros.hpp>
#include "rk/JitterContract.hpp"
#include <array>
#include <cmath>
#include <cstring>
#include <limits>

namespace {
std::array<std::uint8_t,0x4c> camera(std::uint32_t width,std::uint32_t height,
    float projectionX,float projectionY) {
    std::array<std::uint8_t,0x4c> bytes{};
    std::memcpy(bytes.data()+0x24,&width,sizeof(width));
    std::memcpy(bytes.data()+0x28,&height,sizeof(height));
    std::memcpy(bytes.data()+0x44,&projectionX,sizeof(projectionX));
    std::memcpy(bytes.data()+0x48,&projectionY,sizeof(projectionY));
    return bytes;
}
}

TEST_CASE("Observed Skyrim projection jitter maps to NGX pixel offsets", "[jitter_contract]") {
    // First live 0.1.21 frame, 2560x1440; negative Y sign follows the
    // reference's camera projection and NGX payload writes.
    const auto first=camera(2560,1440,-0.0001953125f,0.0002314815f);
    const auto converted=rk::ngxJitterFromGameCamera(first,2560,1440);
    REQUIRE(std::holds_alternative<rk::NgxJitter>(converted));
    const auto offsets=std::get<rk::NgxJitter>(converted);
    REQUIRE(std::abs(offsets.x+0.25f)<0.00001f);
    REQUIRE(std::abs(offsets.y+1.0f/6.0f)<0.00001f);
    const auto eighth=camera(2560,1440,0.0f,-0.00023148146f);
    const auto cycle=rk::ngxJitterFromGameCamera(eighth,2560,1440);
    REQUIRE(std::holds_alternative<rk::NgxJitter>(cycle));
    REQUIRE(std::abs(std::get<rk::NgxJitter>(cycle).x)<0.00001f);
    REQUIRE(std::abs(std::get<rk::NgxJitter>(cycle).y-1.0f/6.0f)<0.00001f);
}

TEST_CASE("Camera jitter rejects missing, mismatched and impossible samples", "[jitter_contract]") {
    const auto valid=camera(2560,1440,0.0f,0.0f);
    REQUIRE(std::holds_alternative<rk::Error>(rk::ngxJitterFromGameCamera(
        std::span(valid).first(0x4b),2560,1440)));
    REQUIRE(std::holds_alternative<rk::Error>(rk::ngxJitterFromGameCamera(valid,1920,1080)));
    REQUIRE(std::holds_alternative<rk::Error>(rk::ngxJitterFromGameCamera(
        camera(2560,1440,std::numeric_limits<float>::quiet_NaN(),0.0f),2560,1440)));
    REQUIRE(std::holds_alternative<rk::Error>(rk::ngxJitterFromGameCamera(
        camera(2560,1440,0.001f,0.0f),2560,1440)));
}
