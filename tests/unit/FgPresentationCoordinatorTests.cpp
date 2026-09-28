#include <catch2/catch_test_macros.hpp>
#include "rk/FgPresentationCoordinator.hpp"
#include <vector>

namespace {
rk::FgSourceFrame frame(std::uint64_t source) {
    rk::FgSourceFrame f{};
    f.source=source;f.generation=7;f.presentToken=9000+source;
    f.resetEpoch=3;f.render={1485,835};f.display={2560,1440};
    f.sr=rk::FgSrProvider::Dlss;f.renderVendor=0x10de;
    f.ownerReady=true;f.worldActive=true;f.cameraValid=true;
    f.color={source,7,{2560,1440},true,3};
    f.depth={source,7,{1485,835},true,3};
    f.motion={source,7,{1485,835},true,3};
    f.hudless={source,7,{2560,1440},true,3};
    f.uiColorAlpha={source,7,{2560,1440},true,3};
    return f;
}
struct Backend final : rk::IFgPresentBackend {
    std::vector<const char*> calls;
    bool failEnable{};
    bool failDisable{};
    rk::FgCapability capability() const noexcept override {
        return {rk::FgProvider::Dlss,true,true,1,
            rk::fgSrMask(rk::FgSrProvider::Dlss),true};
    }
    rk::Result<bool> setMode(bool enabled) override {
        calls.push_back(enabled?"on":"off-drain");
        return enabled?!failEnable:!failDisable;
    }
    rk::Result<rk::FgBackendPresent> presentReal(
        const rk::FgSourceFrame&,bool enabled) override {
        calls.push_back("present");
        return rk::FgBackendPresent{0,enabled?1u:0u};
    }
};
}

TEST_CASE("FG toggle configures Off before one real Present", "[fg_presentation]") {
    Backend backend;
    const rk::FgProviderSession session({false,rk::FgProvider::Dlss,1},
        rk::FgSrProvider::Dlss,0x10de,{backend.capability()});
    rk::FgPresentationCoordinator owner(session,backend);
    const auto on=owner.present(frame(1),true);
    REQUIRE(std::holds_alternative<rk::FgPresentOutcome>(on));
    REQUIRE(std::get<rk::FgPresentOutcome>(on).actualGeneratedFrames==1);
    const auto off=owner.present(frame(2),false);
    REQUIRE(std::holds_alternative<rk::FgPresentOutcome>(off));
    REQUIRE(std::get<rk::FgPresentOutcome>(off).decision.reason==
        rk::FgReason::Disabled);
    REQUIRE(std::get<rk::FgPresentOutcome>(off).actualGeneratedFrames==0);
    REQUIRE(backend.calls==std::vector<const char*>{"on","present","off-drain","present"});
    REQUIRE(std::holds_alternative<rk::Error>(owner.present(frame(2),false)));
    REQUIRE(backend.calls.size()==4);
}

TEST_CASE("Failed FG enable presents the real frame with FG Off",
    "[fg_presentation]") {
    Backend backend;
    backend.failEnable=true;
    const rk::FgProviderSession session({false,rk::FgProvider::Dlss,1},
        rk::FgSrProvider::Dlss,0x10de,{backend.capability()});
    rk::FgPresentationCoordinator owner(session,backend);
    const auto result=owner.present(frame(1),true);
    REQUIRE(std::holds_alternative<rk::FgPresentOutcome>(result));
    const auto& outcome=std::get<rk::FgPresentOutcome>(result);
    REQUIRE(outcome.decision.effective==rk::FgProvider::Off);
    REQUIRE(outcome.actualGeneratedFrames==0);
    REQUIRE(backend.calls==std::vector<const char*>{"on","off-drain","present"});
}

TEST_CASE("Missing native UI turns an active FG backend Off before Present",
    "[fg_presentation]") {
    Backend backend;
    const rk::FgProviderSession session({false,rk::FgProvider::Dlss,1},
        rk::FgSrProvider::Dlss,0x10de,{backend.capability()});
    rk::FgPresentationCoordinator owner(session,backend);
    REQUIRE(std::holds_alternative<rk::FgPresentOutcome>(owner.present(frame(1),true)));
    auto missing=frame(2);
    missing.uiColorAlpha.ready=false;
    const auto result=owner.present(missing,true);
    REQUIRE(std::holds_alternative<rk::FgPresentOutcome>(result));
    REQUIRE(std::get<rk::FgPresentOutcome>(result).decision.reason==
        rk::FgReason::MissingNativeUi);
    REQUIRE(backend.calls==std::vector<const char*>{"on","present","off-drain","present"});
}

TEST_CASE("A failed Off drain does not call the lower Present", "[fg_presentation]") {
    Backend backend;
    const rk::FgProviderSession session({false,rk::FgProvider::Dlss,1},
        rk::FgSrProvider::Dlss,0x10de,{backend.capability()});
    rk::FgPresentationCoordinator owner(session,backend);
    REQUIRE(std::holds_alternative<rk::FgPresentOutcome>(owner.present(frame(1),true)));
    backend.failDisable=true;
    REQUIRE(std::holds_alternative<rk::Error>(owner.present(frame(2),false)));
    REQUIRE(backend.calls==std::vector<const char*>{"on","present","off-drain"});
}
