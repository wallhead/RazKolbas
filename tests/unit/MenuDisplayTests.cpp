#include <catch2/catch_test_macros.hpp>
#include "rk/MenuDisplay.hpp"

namespace {
unsigned beforeCalls{},originalCalls{};
void* seenFirst{};
std::uint32_t seenSecond{},seenThird{},seenFourth{};
bool originalSawBefore{};

void before(void* first,std::uint32_t second,std::uint32_t third,
    std::uint32_t fourth) noexcept {
    ++beforeCalls;
    seenFirst=first;seenSecond=second;seenThird=third;seenFourth=fourth;
}
void original(void* first,std::uint32_t second,std::uint32_t third,
    std::uint32_t fourth) noexcept {
    ++originalCalls;
    originalSawBefore=beforeCalls==1;
    seenFirst=first;seenSecond=second;seenThird=third;seenFourth=fourth;
}
void other(void*,std::uint32_t,std::uint32_t,std::uint32_t) noexcept {}
void throwingBefore(void*,std::uint32_t,std::uint32_t,std::uint32_t) {
    ++beforeCalls;
    throw 7;
}
}

TEST_CASE("Menu-display forwarding preserves four arguments and runs the boundary first",
    "[patch][menu_display]") {
    rk::MenuDisplayForwarder forwarder;
    int object{};
    beforeCalls=originalCalls=0;originalSawBefore=false;
    REQUIRE(std::get<bool>(forwarder.configure(&original,&before)));
    forwarder.dispatch(&object,0x11223344,0x55667788,0x99aabbcc);
    REQUIRE(beforeCalls==1);
    REQUIRE(originalCalls==1);
    REQUIRE(originalSawBefore);
    REQUIRE(seenFirst==&object);
    REQUIRE(seenSecond==0x11223344);
    REQUIRE(seenThird==0x55667788);
    REQUIRE(seenFourth==0x99aabbcc);
}

TEST_CASE("Menu-display forwarding owner is immutable after activation",
    "[patch][menu_display]") {
    rk::MenuDisplayForwarder forwarder;
    REQUIRE(std::holds_alternative<rk::Error>(forwarder.configure(nullptr,&before)));
    REQUIRE(std::get<bool>(forwarder.configure(&original,&before)));
    REQUIRE(std::get<bool>(forwarder.configure(&original,&before)));
    REQUIRE(std::holds_alternative<rk::Error>(forwarder.configure(&other,&before)));
}

TEST_CASE("Menu-display forwarding survives an instrumentation exception",
    "[patch][menu_display]") {
    rk::MenuDisplayForwarder forwarder;
    beforeCalls=originalCalls=0;
    REQUIRE(std::get<bool>(forwarder.configure(&original,&throwingBefore)));
    forwarder.dispatch(nullptr,1,2,3);
    REQUIRE(beforeCalls==1);
    REQUIRE(originalCalls==1);
}
