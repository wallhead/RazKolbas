#include <catch2/catch_test_macros.hpp>
#include "rk/FgPrivateSwapAdmission.hpp"

TEST_CASE("Private FG swap admission is exact and one shot", "[fg_private_swap_admission]") {
    rk::FgPrivateSwapAdmission expected{42, 77, 2560, 1440, true};
    REQUIRE(rk::admitPrivateFgSwap(expected, expected));
    auto changed=expected;
    changed.thread=43;
    REQUIRE_FALSE(rk::admitPrivateFgSwap(expected, changed));
    changed=expected;
    changed.window=78;
    REQUIRE_FALSE(rk::admitPrivateFgSwap(expected, changed));
    changed=expected;
    changed.width=1920;
    REQUIRE_FALSE(rk::admitPrivateFgSwap(expected, changed));
    changed=expected;
    changed.height=1080;
    REQUIRE_FALSE(rk::admitPrivateFgSwap(expected, changed));
    changed=expected;
    changed.nativeFactory=false;
    REQUIRE_FALSE(rk::admitPrivateFgSwap(expected, changed));
}
