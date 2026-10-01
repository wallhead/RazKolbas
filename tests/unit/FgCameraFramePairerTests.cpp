#include <catch2/catch_test_macros.hpp>
#include "rk/FgCameraFramePairer.hpp"
#include <cstring>

namespace {
using Matrix=std::array<float,16>;
Matrix diagonal(float x,float y,float z) {
    Matrix m{};m[0]=x;m[5]=y;m[10]=z;m[15]=1.0f;return m;
}
std::array<std::uint8_t,720> camera(float currentZ,float previousZ,
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
rk::FgCameraWrite write(std::uint64_t revision,std::uint64_t buffer,
    std::uint64_t generation,float position,float previous) {
    rk::FgCameraWrite result{};
    result.revision=revision;result.buffer=buffer;
    result.generation=generation;result.thread=5;
    result.bytes=camera(-4,-4,position,previous);
    return result;
}
std::optional<rk::FgCameraProducerSample> frame(rk::FgCameraFramePairer& pairer,
    std::uint64_t source,const rk::FgCameraWrite& before,
    const rk::FgCameraWrite& present) {
    pairer.beforeUi(source,before);
    return pairer.beforePresent(source,present);
}
}

TEST_CASE("FG camera pairer waits for fresh consecutive world writes",
    "[fg_camera_pairer]") {
    rk::FgCameraFramePairer pairer;
    REQUIRE_FALSE(frame(pairer,10,write(100,9,2,10,9),write(100,9,2,10,9)));
    REQUIRE_FALSE(frame(pairer,11,write(110,9,2,11,10),write(110,9,2,11,10)));
    const auto paired=frame(pairer,12,write(120,9,2,12,11),
        write(120,9,2,12,11));
    REQUIRE(paired);
    REQUIRE(paired->source==12);
    REQUIRE(paired->revision==120);
    REQUIRE(paired->writeGeneration==2);
    REQUIRE(paired->camera.position[0]==12.0f);
}

TEST_CASE("FG camera pairer rejects phase changes, stale writes and generation gaps",
    "[fg_camera_pairer]") {
    rk::FgCameraFramePairer pairer;
    frame(pairer,10,write(100,9,2,10,9),write(100,9,2,10,9));
    frame(pairer,11,write(110,9,2,11,10),write(110,9,2,11,10));
    auto changed=write(121,9,2,12,11);
    REQUIRE_FALSE(frame(pairer,12,write(120,9,2,12,11),changed));
    REQUIRE_FALSE(frame(pairer,13,write(121,9,2,13,12),
        write(121,9,2,13,12)));
    REQUIRE_FALSE(frame(pairer,14,write(130,10,3,14,13),
        write(130,10,3,14,13)));
    REQUIRE_FALSE(frame(pairer,16,write(140,10,3,16,15),
        write(140,10,3,16,15)));
    pairer.clear();
    REQUIRE_FALSE(frame(pairer,17,write(150,10,3,17,16),
        write(150,10,3,17,16)));
}

TEST_CASE("FG camera pairer rejects decoded history discontinuity",
    "[fg_camera_pairer]") {
    rk::FgCameraFramePairer pairer;
    frame(pairer,10,write(100,9,2,10,9),write(100,9,2,10,9));
    frame(pairer,11,write(110,9,2,11,10),write(110,9,2,11,10));
    REQUIRE_FALSE(frame(pairer,12,write(120,9,2,12,7),
        write(120,9,2,12,7)));
    auto invalid=write(130,9,2,13,12);
    invalid.bytes[0]=0xff;
    REQUIRE_FALSE(frame(pairer,13,invalid,invalid));
}
