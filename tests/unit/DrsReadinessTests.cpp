#include "rk/DrsReadiness.hpp"
#include <catch2/catch_test_macros.hpp>
#include <limits>

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
