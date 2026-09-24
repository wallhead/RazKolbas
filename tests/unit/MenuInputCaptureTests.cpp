#include "rk/MenuInputCapture.hpp"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Menu mouse capture blocks Skyrim input and restores the previous state",
    "[menu_input_capture]") {
    rk::MenuInputCapture capture;

    REQUIRE(capture.update(true, false) == true);
    REQUIRE(capture.active());
    REQUIRE_FALSE(capture.update(true, true).has_value());

    REQUIRE(capture.update(false, true) == false);
    REQUIRE_FALSE(capture.active());
    REQUIRE_FALSE(capture.update(false, false).has_value());
}

TEST_CASE("Menu mouse capture preserves an input block owned by Skyrim or another mod",
    "[menu_input_capture]") {
    rk::MenuInputCapture capture;

    REQUIRE_FALSE(capture.update(true, true).has_value());
    REQUIRE(capture.active());
    REQUIRE_FALSE(capture.update(false, true).has_value());
    REQUIRE_FALSE(capture.active());
}

TEST_CASE("Menu mouse capture reasserts blocking and releases it on focus loss",
    "[menu_input_capture]") {
    rk::MenuInputCapture capture;

    REQUIRE(capture.update(true, false) == true);
    REQUIRE(capture.update(true, false) == true);
    REQUIRE(capture.update(false, true) == false);

    REQUIRE(capture.update(true, false) == true);
    REQUIRE(capture.update(false, true) == false);
}
