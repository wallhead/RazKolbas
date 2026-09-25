#include <catch2/catch_test_macros.hpp>
#include "rk/DeferredUiFlush.hpp"

namespace {
unsigned beforeCalls{},originalCalls{};
void* seenRenderer{};
bool originalSawBefore{};
void before(void* renderer) {
    ++beforeCalls;seenRenderer=renderer;
}
void original(void* renderer) {
    ++originalCalls;originalSawBefore=beforeCalls==1;seenRenderer=renderer;
}
void other(void*) {}
void throwingBefore(void*) {++beforeCalls;throw 7;}
}

TEST_CASE("Deferred UI flush reasserts state before forwarding EndFrame",
    "[patch][deferred_ui_flush]") {
    rk::DeferredUiFlushForwarder forwarder;
    int renderer{};
    beforeCalls=originalCalls=0;originalSawBefore=false;seenRenderer=nullptr;
    REQUIRE(std::get<bool>(forwarder.configure(&original,&before)));
    forwarder.dispatch(&renderer);
    REQUIRE(beforeCalls==1);
    REQUIRE(originalCalls==1);
    REQUIRE(originalSawBefore);
    REQUIRE(seenRenderer==&renderer);
}

TEST_CASE("Deferred UI flush forwarding owner is immutable",
    "[patch][deferred_ui_flush]") {
    rk::DeferredUiFlushForwarder forwarder;
    REQUIRE(std::holds_alternative<rk::Error>(forwarder.configure(nullptr,&before)));
    REQUIRE(std::get<bool>(forwarder.configure(&original,&before)));
    REQUIRE(std::get<bool>(forwarder.configure(&original,&before)));
    REQUIRE(std::holds_alternative<rk::Error>(forwarder.configure(&other,&before)));
}

TEST_CASE("Deferred UI flush always forwards after instrumentation failure",
    "[patch][deferred_ui_flush]") {
    rk::DeferredUiFlushForwarder forwarder;
    beforeCalls=originalCalls=0;
    REQUIRE(std::get<bool>(forwarder.configure(&original,&throwingBefore)));
    forwarder.dispatch(nullptr);
    REQUIRE(beforeCalls==1);
    REQUIRE(originalCalls==1);
}
