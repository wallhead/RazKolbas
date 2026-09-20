#include <catch2/catch_test_macros.hpp>
#include "rk/WorldDraw.hpp"

namespace {
int originalCalls{},observedCalls{},originalFirst{},originalSecond{};
bool observerSawOriginal{};
void original(void* world,std::uint32_t flags) noexcept {
    ++originalCalls;
    originalFirst=static_cast<int>(*static_cast<std::uint32_t*>(world));
    originalSecond=static_cast<int>(flags);
}
void afterOriginal(void* world,std::uint32_t flags) noexcept {
    observerSawOriginal=originalCalls==1&&*static_cast<std::uint32_t*>(world)==42&&flags==0x1234;
    ++observedCalls;
}
void other(void*,std::uint32_t) noexcept {}
}

TEST_CASE("World-draw pass-through retains two arguments and original-first ordering", "[patch][world_draw]") {
    rk::WorldDrawForwarder forwarder;
    std::uint32_t world=42;
    originalCalls=observedCalls=originalFirst=originalSecond=0;
    observerSawOriginal=false;
    REQUIRE(std::get<bool>(forwarder.configure(&original,&afterOriginal)));
    forwarder.dispatch(&world,0x1234);
    REQUIRE(originalCalls==1);
    REQUIRE(observedCalls==1);
    REQUIRE(originalFirst==42);
    REQUIRE(originalSecond==0x1234);
    REQUIRE(observerSawOriginal);
}

TEST_CASE("World-draw forwarding owner is fixed before activation", "[patch][world_draw]") {
    rk::WorldDrawForwarder forwarder;
    REQUIRE(std::holds_alternative<rk::Error>(forwarder.configure(nullptr,nullptr)));
    REQUIRE(std::get<bool>(forwarder.configure(&original,nullptr)));
    REQUIRE(std::get<bool>(forwarder.configure(&original,nullptr)));
    REQUIRE(std::holds_alternative<rk::Error>(forwarder.configure(&other,nullptr)));
}
