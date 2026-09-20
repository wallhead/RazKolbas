#include "rk/DrsReadiness.hpp"
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <vector>

TEST_CASE("Native DLAA refuses a reduced or untrusted engine DRS history", "[drs_readiness]") {
    REQUIRE(rk::nativeDlaaRatiosReady(1.0f,1.0f,1.0f,1.0f));
    REQUIRE_FALSE(rk::nativeDlaaRatiosReady(0.666f,1.0f,1.0f,1.0f));
    REQUIRE_FALSE(rk::nativeDlaaRatiosReady(1.0f,0.666f,1.0f,1.0f));
    REQUIRE_FALSE(rk::nativeDlaaRatiosReady(1.0f,1.0f,0.666f,1.0f));
    REQUIRE_FALSE(rk::nativeDlaaRatiosReady(1.0f,1.0f,1.0f,0.666f));
    REQUIRE_FALSE(rk::nativeDlaaRatiosReady(
        std::numeric_limits<float>::quiet_NaN(),1.0f,1.0f,1.0f));
    REQUIRE_FALSE(rk::nativeDlaaRatiosReady(
        std::numeric_limits<float>::infinity(),1.0f,1.0f,1.0f));
}

TEST_CASE("Engine DRS target reports the game-quantized world extent", "[drs_readiness]") {
    const auto target=rk::engineDrsTarget(2560,1440,0.665625f,0.6666667f);
    REQUIRE(target.has_value());
    REQUIRE(target->width==1704);
    REQUIRE(target->height==960);
    REQUIRE_FALSE(rk::engineDrsTarget(2560,1440,0.0f,1.0f));
    REQUIRE_FALSE(rk::engineDrsTarget(2560,1440,1.2f,1.0f));
    REQUIRE_FALSE(rk::engineDrsTarget(0,1440,1.0f,1.0f));
}

TEST_CASE("DRS diagnostics wait for each stable four-ratio generation", "[drs_readiness]") {
    rk::StableDrsTupleGate gate;
    REQUIRE_FALSE(gate.observe({0.666f,1.0f,1.0f,1.0f}));
    REQUIRE_FALSE(gate.observe({0.666f,0.666f,1.0f,1.0f}));
    REQUIRE(gate.observe({0.666f,0.666f,1.0f,1.0f})==2);
    REQUIRE_FALSE(gate.observe({0.666f,0.666f,1.0f,1.0f}));
    REQUIRE_FALSE(gate.observe({0.666f,0.666f,0.666f,0.666f}));
    REQUIRE(gate.observe({0.666f,0.666f,0.666f,0.666f})==3);
    REQUIRE_FALSE(gate.observe({0.0f,0.666f,0.666f,0.666f}));
    REQUIRE_FALSE(gate.observe({1.0f,1.0f,1.0f,1.0f}));
    REQUIRE(gate.observe({1.0f,1.0f,1.0f,1.0f})==4);
}

TEST_CASE("Reduced SDR source gate rejects an already enlarged frame", "[drs_readiness]") {
    constexpr std::uint32_t displayWidth=48,displayHeight=36;
    constexpr std::uint32_t renderWidth=32,renderHeight=24;
    std::vector<std::uint8_t> rgba(displayWidth*displayHeight*4);
    for(std::uint32_t y=0;y<renderHeight;++y)
        for(std::uint32_t x=0;x<renderWidth;++x)
            rgba[(y*displayWidth+x)*4]=static_cast<std::uint8_t>(x*7+y*3);
    REQUIRE(rk::reducedSdrRegionLooksUnscaled(rgba,displayWidth,displayHeight,
        displayWidth*4,renderWidth,renderHeight));
    for(std::uint32_t y=0;y<displayHeight;++y)
        for(std::uint32_t x=renderWidth;x<displayWidth;++x)
            rgba[(y*displayWidth+x)*4]=static_cast<std::uint8_t>(x*7+y*3);
    REQUIRE_FALSE(rk::reducedSdrRegionLooksUnscaled(rgba,displayWidth,displayHeight,
        displayWidth*4,renderWidth,renderHeight));
    REQUIRE_FALSE(rk::reducedSdrRegionLooksUnscaled(rgba,displayWidth,displayHeight,
        displayWidth*4,displayWidth,displayHeight));
}
