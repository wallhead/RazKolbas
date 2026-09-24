#include "rk/MenuInputHook.hpp"
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <limits>

namespace {
constexpr std::array<std::uint8_t,32> chainedCall{
    0x48,0x89,0x4c,0x24,0x40,0x48,0x8b,0xce,
    0xe8,0xcf,0xb1,0x30,0xff,
    0x48,0x8b,0x0d,0xa1,0x84,0x4d,0x02,
    0xe8,0xd4,0x1c,0x00,0x00,0x0f,0xb6,0x86,0xe0,0x00,0x00,0x00};
}

TEST_CASE("Skyrim 1.6.1170 input dispatch accepts a chained direct CALL",
    "[menu_input_hook]") {
    REQUIRE(std::get<bool>(rk::verifySkyrim1170InputDispatchCall(chainedCall)));
    auto anotherOwner=chainedCall;
    anotherOwner[9]=0x12;
    anotherOwner[10]=0x34;
    anotherOwner[11]=0x56;
    anotherOwner[12]=0x78;
    REQUIRE(std::get<bool>(rk::verifySkyrim1170InputDispatchCall(anotherOwner)));

    auto changed=chainedCall;
    changed[8]=0xe9;
    REQUIRE(std::holds_alternative<rk::Error>(
        rk::verifySkyrim1170InputDispatchCall(changed)));
    changed=chainedCall;
    changed[27]=0x90;
    REQUIRE(std::holds_alternative<rk::Error>(
        rk::verifySkyrim1170InputDispatchCall(changed)));
    REQUIRE(std::holds_alternative<rk::Error>(
        rk::verifySkyrim1170InputDispatchCall(std::span(chainedCall).first(31))));
}

TEST_CASE("Visible diagnostics menu dispatches an empty input event list",
    "[menu_input_hook]") {
    void* first=reinterpret_cast<void*>(0x1234);
    void* const events[]{first,nullptr};
    REQUIRE(rk::selectMenuInputEvents(false,events)==events);
    const auto blocked=rk::selectMenuInputEvents(true,events);
    REQUIRE(blocked!=events);
    REQUIRE(*blocked==nullptr);
}

TEST_CASE("Existing rel32 input hook becomes an owned chained call plan",
    "[menu_input_hook]") {
    constexpr std::uintptr_t site=0x7ff76a398fbb;
    constexpr std::array<std::uint8_t,5> call{0xe8,0xcf,0xb1,0x30,0xff};
    const auto prepared=rk::prepareChainedInputCall(site,call);
    REQUIRE(std::holds_alternative<rk::ChainedInputCall>(prepared));
    const auto& chain=std::get<rk::ChainedInputCall>(prepared);
    REQUIRE(chain.syntheticBase+chain.plan.siteRva==site);
    REQUIRE(chain.priorTarget==0x7ff7696a418f);
    REQUIRE(chain.syntheticBase+chain.plan.originalTargetRva==chain.priorTarget);
    REQUIRE(chain.plan.original==call);

    auto changed=call;
    changed[0]=0xe9;
    REQUIRE(std::holds_alternative<rk::Error>(
        rk::prepareChainedInputCall(site,changed)));
    REQUIRE(std::holds_alternative<rk::Error>(
        rk::prepareChainedInputCall(0,call)));
    REQUIRE(std::holds_alternative<rk::Error>(rk::prepareChainedInputCall(
        std::numeric_limits<std::uintptr_t>::max()-2,call)));
}
