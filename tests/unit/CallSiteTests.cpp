#include <catch2/catch_test_macros.hpp>
#include "rk/CallSite.hpp"
#include <array>

namespace {
constexpr std::array<std::uint8_t,5> worldCall{0xe8,0xd1,0xf7,0xe9,0xff};
rk::CallSiteDescriptor worldDescriptor() {
    return {"skyrim1170.world-draw.sr-v1",
            "c434208894f07f604b852f29b8edc3a58c4de63de783373733e72b2b73f33be9",
            0x3870000,0xfa507a,0xe44850,worldCall};
}
}

TEST_CASE("Verified Skyrim world call resolves to the original engine target", "[patch][call_site]") {
    const auto d=worldDescriptor();
    const auto result=rk::prepareCallSite(worldCall,d.gameSha256,d.imageSize,d);
    REQUIRE(std::holds_alternative<rk::CallSitePlan>(result));
    const auto& plan=std::get<rk::CallSitePlan>(result);
    REQUIRE(plan.siteRva==0xfa507a);
    REQUIRE(plan.originalTargetRva==0xe44850);
}

TEST_CASE("Changed world call, identity, and expected target all reject before a code write", "[patch][call_site]") {
    auto d=worldDescriptor();
    auto changed=worldCall;
    changed[4]^=1;
    REQUIRE(std::holds_alternative<rk::Error>(rk::prepareCallSite(changed,d.gameSha256,d.imageSize,d)));
    REQUIRE(std::holds_alternative<rk::Error>(rk::prepareCallSite(worldCall,std::string(64,'0'),d.imageSize,d)));
    d.originalTargetRva++;
    REQUIRE(std::holds_alternative<rk::Error>(rk::prepareCallSite(worldCall,d.gameSha256,d.imageSize,d)));
}

TEST_CASE("Call-site planner rejects truncated, non-CALL, and out-of-image sites", "[patch][call_site]") {
    auto d=worldDescriptor();
    REQUIRE(std::holds_alternative<rk::Error>(rk::prepareCallSite(
        std::span<const std::uint8_t>(worldCall.data(),4),d.gameSha256,d.imageSize,d)));
    d.expected[0]=0xe9;
    REQUIRE(std::holds_alternative<rk::Error>(rk::prepareCallSite(worldCall,d.gameSha256,d.imageSize,d)));
    d=worldDescriptor();
    d.siteRva=static_cast<std::uint32_t>(d.imageSize-4);
    REQUIRE(std::holds_alternative<rk::Error>(rk::prepareCallSite(worldCall,d.gameSha256,d.imageSize,d)));
}
