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
