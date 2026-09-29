#include <catch2/catch_test_macros.hpp>
#include "rk/FgGameCameraBuffer.hpp"
#include <cstring>
#include <limits>
#include <variant>

namespace {
using Matrix=std::array<float,16>;
Matrix diagonal(float x,float y,float z) {
    Matrix m{};m[0]=x;m[5]=y;m[10]=z;m[15]=1.0f;return m;
}
std::array<std::uint8_t,720> gameBuffer(float currentZ,float previousZ,
    float positionX,float previousPositionX) {
    std::array<std::uint8_t,720> bytes{};
    const auto put=[&](std::size_t offset,const auto& value) {
        std::memcpy(bytes.data()+offset,value.data(),sizeof(value));
    };
    put(0x000,diagonal(1,1,-1));
    put(0x040,diagonal(2,3,4));
    put(0x080,diagonal(2,3,currentZ));
    put(0x0c0,diagonal(2,3,currentZ));
    put(0x100,diagonal(2,3,previousZ));
    put(0x140,diagonal(0.5f,1.0f/3,0.25f));
    put(0x180,diagonal(2,3,4));
    put(0x1c0,diagonal(1,1,-1));
    put(0x200,diagonal(0.5f,1.0f/3,-0.25f));
    put(0x240,diagonal(0.5f,1.0f/3,0.25f));
    put(0x280,std::array<float,4>{positionX,20,30,0});
    put(0x290,std::array<float,4>{previousPositionX,20,30,0});
    return bytes;
}
}

TEST_CASE("FG game camera decoder reads independently derived current and previous fields",
    "[fg_game_camera]") {
    const auto raw=gameBuffer(-4,-3,10,9);
    const auto decoded=rk::decodeFgGameCameraBuffer(raw);
    REQUIRE(std::holds_alternative<rk::FgGameCameraSample>(decoded));
    const auto& sample=std::get<rk::FgGameCameraSample>(decoded);
    REQUIRE(sample.currentViewProjection[10]==-4.0f);
    REQUIRE(sample.previousViewProjection[10]==-3.0f);
    REQUIRE(sample.inverseViewProjection[10]==-0.25f);
    REQUIRE(sample.position==std::array<float,3>{10,20,30});
    REQUIRE(sample.previousPosition==std::array<float,3>{9,20,30});
}

TEST_CASE("FG game camera decoder rejects a view-projection unrelated to the view",
    "[fg_game_camera]") {
    auto raw=gameBuffer(-4,-3,10,9);
    const float corrupted=7.0f;
    std::memcpy(raw.data()+0x080,&corrupted,sizeof(corrupted));
    REQUIRE(std::holds_alternative<rk::Error>(
        rk::decodeFgGameCameraBuffer(raw)));
}

TEST_CASE("FG game camera decoder rejects an adjacent-frame-scale matrix mix",
    "[fg_game_camera]") {
    auto raw=gameBuffer(-4,-3,10,9);
    const float nextFrameLike=2.00004f;
    std::memcpy(raw.data()+0x080,&nextFrameLike,sizeof(nextFrameLike));
    REQUIRE(std::holds_alternative<rk::Error>(
        rk::decodeFgGameCameraBuffer(raw)));
}

TEST_CASE("FG game camera decoder rejects a false inverse view-projection",
    "[fg_game_camera]") {
    auto raw=gameBuffer(-4,-3,10,9);
    const float corrupted=2.0f;
    std::memcpy(raw.data()+0x200,&corrupted,sizeof(corrupted));
    REQUIRE(std::holds_alternative<rk::Error>(
        rk::decodeFgGameCameraBuffer(raw)));
}

TEST_CASE("FG game camera decoder rejects an adjacent-frame-scale inverse mix",
    "[fg_game_camera]") {
    auto raw=gameBuffer(-4,-3,10,9);
    const float nextFrameLike=0.50004f;
    std::memcpy(raw.data()+0x200,&nextFrameLike,sizeof(nextFrameLike));
    REQUIRE(std::holds_alternative<rk::Error>(
        rk::decodeFgGameCameraBuffer(raw)));
}

TEST_CASE("FG game camera decoder rejects false inverse projection or view",
    "[fg_game_camera]") {
    for(const std::size_t offset:{0x140u,0x1c0u}) {
        auto raw=gameBuffer(-4,-3,10,9);
        const float corrupted=2.0f;
        std::memcpy(raw.data()+offset,&corrupted,sizeof(corrupted));
        REQUIRE(std::holds_alternative<rk::Error>(
            rk::decodeFgGameCameraBuffer(raw)));
    }
}

TEST_CASE("FG game camera decoder rejects nonfinite history and position",
    "[fg_game_camera]") {
    for(const std::size_t offset:{0x100u,0x280u,0x290u}) {
        auto raw=gameBuffer(-4,-3,10,9);
        const float invalid=std::numeric_limits<float>::quiet_NaN();
        std::memcpy(raw.data()+offset,&invalid,sizeof(invalid));
        REQUIRE(std::holds_alternative<rk::Error>(
            rk::decodeFgGameCameraBuffer(raw)));
    }
}

TEST_CASE("FG game camera consecutive check rejects a stale previous frame",
    "[fg_game_camera]") {
    const auto first=std::get<rk::FgGameCameraSample>(
        rk::decodeFgGameCameraBuffer(gameBuffer(-4,-3,10,9)));
    const auto next=std::get<rk::FgGameCameraSample>(
        rk::decodeFgGameCameraBuffer(gameBuffer(-4,-4,11,10)));
    const auto staleMatrix=std::get<rk::FgGameCameraSample>(
        rk::decodeFgGameCameraBuffer(gameBuffer(-4,-3,11,10)));
    const auto stalePosition=std::get<rk::FgGameCameraSample>(
        rk::decodeFgGameCameraBuffer(gameBuffer(-4,-4,11,8)));
    REQUIRE(rk::fgGameCameraConsecutive(first,next));
    REQUIRE_FALSE(rk::fgGameCameraConsecutive(first,staleMatrix));
    REQUIRE_FALSE(rk::fgGameCameraConsecutive(first,stalePosition));
}
