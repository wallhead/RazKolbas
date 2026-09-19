#include <catch2/catch_test_macros.hpp>
#include "rk/Capabilities.hpp"
#include "rk/FrameContracts.hpp"

TEST_CASE("Unavailable explicit DLSS retains request and explains native fallback", "[policy]") {
    const auto selection = rk::selectSr(rk::Provider::DLSS, 0x10de, {{rk::Provider::DLSS, false, rk::Validation::Unverified, -8, "runtime missing"}});
    REQUIRE(selection.requested == rk::Provider::DLSS);
    REQUIRE(selection.effective == rk::Provider::Native);
    REQUIRE(selection.reason.find("runtime missing") != std::string::npos);
}
TEST_CASE("Auto requires validation and chooses actual render vendor first", "[policy]") {
    std::vector<rk::FeatureCapability> caps{
        {rk::Provider::DLSS, true, rk::Validation::Unverified},
        {rk::Provider::FSR, true, rk::Validation::Validated},
        {rk::Provider::XeSS, true, rk::Validation::Validated}};
    REQUIRE(rk::selectSr(rk::Provider::Auto, 0x10de, caps).effective == rk::Provider::FSR);
    REQUIRE(rk::selectSr(rk::Provider::Auto, 0x8086, caps).effective == rk::Provider::XeSS);
    caps[0].validation = rk::Validation::Experimental;
    REQUIRE(rk::selectSr(rk::Provider::Auto, 0x10de, caps).effective == rk::Provider::FSR);
    REQUIRE(rk::selectSr(rk::Provider::Auto, 0x10de, caps, true).effective == rk::Provider::DLSS);
}
TEST_CASE("Older successful evaluation cannot consume a newer reset", "[history]") {
    rk::HistoryEpoch history;
    for (int i=0; i<6; ++i) history.request();
    const auto captured = history.capture();
    REQUIRE(captured == 7);
    REQUIRE(history.request() == 8);
    history.consume(captured, true);
    REQUIRE(history.pending());
    history.consume(8, false);
    REQUIRE(history.pending());
    history.consume(8, true);
    REQUIRE_FALSE(history.pending());
    history.consume(7, true);
    REQUIRE_FALSE(history.pending());
}
TEST_CASE("Generated displays leave real-frame identity unchanged", "[frame_identity]") {
    rk::FrameIdentity identity;
    REQUIRE_FALSE(identity.presentGenerated(0));
    REQUIRE(identity.beginSource() == 1);
    for (int i=0; i<3; ++i) REQUIRE(identity.presentGenerated(1));
    REQUIRE(identity.source() == 1);
    REQUIRE(identity.generated() == 3);
    REQUIRE(identity.beginSource() == 2);
    REQUIRE_FALSE(identity.presentGenerated(1));
    REQUIRE_FALSE(rk::Extent{0, 1080}.valid());
}
