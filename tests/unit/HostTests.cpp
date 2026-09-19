#include <catch2/catch_test_macros.hpp>
#include "rk/Bootstrap.hpp"
#include "rk/ProbeRetirement.hpp"

TEST_CASE("Native bootstrap needs no vendor runtime and registers once", "[host]") {
    rk::Bootstrap host;
    int registrations = 0;
    const auto subscribe = [&] { ++registrations; return true; };
    REQUIRE(host.start(false, subscribe));
    REQUIRE(host.start(false, subscribe));
    REQUIRE(registrations == 1);
    REQUIRE(host.state() == rk::HostState::BootstrapReady);
    REQUIRE_FALSE(host.attachRenderer(false));
    REQUIRE_FALSE(host.run());
}
TEST_CASE("Safe mode cannot attach rendering even with a profile", "[host]") {
    rk::Bootstrap host;
    REQUIRE(host.start(true, [] { return true; }));
    REQUIRE_FALSE(host.attachRenderer(true));
    REQUIRE_FALSE(host.run());
    REQUIRE(host.state() == rk::HostState::BootstrapReady);
}
TEST_CASE("Failed registration and terminal shutdown preserve lifecycle", "[host]") {
    rk::Bootstrap host;
    REQUIRE_FALSE(host.start(false, [] { return false; }));
    REQUIRE(host.state() == rk::HostState::Created);
    REQUIRE(host.start(false, [] { return true; }));
    REQUIRE(host.attachRenderer(true));
    REQUIRE(host.run());
    REQUIRE(host.suspend());
    REQUIRE(host.run());
    host.stop();
    host.stop();
    REQUIRE(host.state() == rk::HostState::Stopping);
    REQUIRE_FALSE(host.start(false, [] { return true; }));
    REQUIRE_FALSE(host.run());
}
TEST_CASE("Failed native probe shutdown retains runtime and patch until process termination", "[host]") {
    bool restored = false, unloaded = false;
    const auto result = rk::retireProbeRuntime([] { return 0xbad00002U; }, [&]() -> rk::Result<bool> { restored = true; return true; }, [&] { unloaded = true; });
    REQUIRE(std::holds_alternative<rk::Error>(result));
    REQUIRE_FALSE(restored);
    REQUIRE_FALSE(unloaded);
}
TEST_CASE("Native probe unload follows successful shutdown and pointer restoration", "[host]") {
    bool unloaded = false;
    const auto blocked = rk::retireProbeRuntime([] { return 1U; }, []() -> rk::Result<bool> { return rk::Error{rk::ErrorCode::Conflict,"owner changed"}; }, [&] { unloaded = true; });
    REQUIRE(std::holds_alternative<rk::Error>(blocked));
    REQUIRE_FALSE(unloaded);
    const auto success = rk::retireProbeRuntime([] { return 1U; }, []() -> rk::Result<bool> { return true; }, [&] { unloaded = true; });
    REQUIRE(std::get<bool>(success));
    REQUIRE(unloaded);
}
