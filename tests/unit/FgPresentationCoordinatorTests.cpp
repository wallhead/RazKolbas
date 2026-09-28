#include <catch2/catch_test_macros.hpp>
#include "rk/FgPresentationCoordinator.hpp"
#include <optional>
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
    bool failPresent{};
    HRESULT resultCode{};
    std::optional<std::uint32_t> generatedOverride;
    rk::FgPresentCall lastCall{};
    rk::FgCapability capability() const noexcept override {
        return {rk::FgProvider::Dlss,true,true,1,
            rk::fgSrMask(rk::FgSrProvider::Dlss),true};
    }
    rk::Result<bool> setMode(bool enabled) override {
        calls.push_back(enabled?"on":"off-drain");
        return enabled?!failEnable:!failDisable;
    }
    rk::Result<rk::FgBackendPresent> presentReal(
        const rk::FgSourceFrame&,bool enabled,
        const rk::FgPresentCall& call) override {
        calls.push_back("present");
        lastCall=call;
        if(failPresent)return rk::Error{rk::ErrorCode::Unavailable,
            "Lower Present result unavailable"};
        return rk::FgBackendPresent{resultCode,
            generatedOverride.value_or(enabled?1u:0u)};
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
    backend.failDisable=false;
    REQUIRE(std::holds_alternative<rk::FgPresentOutcome>(owner.present(frame(2),false)));
    REQUIRE(backend.calls==std::vector<const char*>{
        "on","present","off-drain","off-drain","present"});
}

TEST_CASE("DXGI Present1 arguments and HRESULT pass through unchanged",
    "[fg_presentation]") {
    Backend backend;
    backend.resultCode=DXGI_STATUS_OCCLUDED;
    const rk::FgProviderSession session({false,rk::FgProvider::Dlss,1},
        rk::FgSrProvider::Dlss,0x10de,{backend.capability()});
    rk::FgPresentationCoordinator owner(session,backend);
    RECT dirty{1,2,10,12};
    DXGI_PRESENT_PARAMETERS parameters{};
    parameters.DirtyRectsCount=1;
    parameters.pDirtyRects=&dirty;
    const rk::FgPresentCall call{rk::FgPresentMethod::Present1,0,
        DXGI_PRESENT_DO_NOT_WAIT,&parameters};
    const auto result=owner.present(frame(1),false,call);
    REQUIRE(std::holds_alternative<rk::FgPresentOutcome>(result));
    REQUIRE(std::get<rk::FgPresentOutcome>(result).resultCode==DXGI_STATUS_OCCLUDED);
    REQUIRE(backend.lastCall.method==rk::FgPresentMethod::Present1);
    REQUIRE(backend.lastCall.interval==0);
    REQUIRE(backend.lastCall.flags==DXGI_PRESENT_DO_NOT_WAIT);
    REQUIRE(backend.lastCall.parameters==&parameters);
    REQUIRE(backend.lastCall.parameters->pDirtyRects==&dirty);
    backend.resultCode=E_FAIL;
    const auto failed=owner.present(frame(2),false,call);
    REQUIRE(std::holds_alternative<rk::FgPresentOutcome>(failed));
    REQUIRE(std::get<rk::FgPresentOutcome>(failed).resultCode==E_FAIL);
    REQUIRE(std::holds_alternative<rk::Error>(owner.present(frame(2),false,call)));
}

TEST_CASE("DXGI test Present bypasses generation and does not consume a source",
    "[fg_presentation]") {
    Backend backend;
    const rk::FgProviderSession session({false,rk::FgProvider::Dlss,1},
        rk::FgSrProvider::Dlss,0x10de,{backend.capability()});
    rk::FgPresentationCoordinator owner(session,backend);
    const rk::FgPresentCall test{rk::FgPresentMethod::Present,0,DXGI_PRESENT_TEST};
    const auto probe=owner.present(frame(1),true,test);
    REQUIRE(std::holds_alternative<rk::FgPresentOutcome>(probe));
    REQUIRE(std::get<rk::FgPresentOutcome>(probe).decision.reason==
        rk::FgReason::PresentTest);
    REQUIRE(backend.lastCall.flags==DXGI_PRESENT_TEST);
    REQUIRE(std::holds_alternative<rk::FgPresentOutcome>(owner.present(frame(1),true)));
    REQUIRE(backend.calls==std::vector<const char*>{"present","on","present"});
    const auto probeWhileOn=owner.present(frame(2),true,test);
    REQUIRE(std::holds_alternative<rk::FgPresentOutcome>(probeWhileOn));
    REQUIRE(backend.calls==std::vector<const char*>{
        "present","on","present","off-drain","present"});
    REQUIRE(std::holds_alternative<rk::FgPresentOutcome>(owner.present(frame(2),true)));
}

TEST_CASE("An uncertain lower Present error consumes its source token",
    "[fg_presentation]") {
    Backend backend;
    backend.failPresent=true;
    const rk::FgProviderSession session({false,rk::FgProvider::Dlss,1},
        rk::FgSrProvider::Dlss,0x10de,{backend.capability()});
    rk::FgPresentationCoordinator owner(session,backend);
    REQUIRE(std::holds_alternative<rk::Error>(owner.present(frame(1),false)));
    backend.failPresent=false;
    REQUIRE(std::holds_alternative<rk::Error>(owner.present(frame(1),false)));
    REQUIRE(backend.calls==std::vector<const char*>{"present"});
}

TEST_CASE("Backend-generated count cannot exceed the requested count",
    "[fg_presentation]") {
    Backend backend;
    backend.generatedOverride=2;
    const rk::FgProviderSession session({false,rk::FgProvider::Dlss,1},
        rk::FgSrProvider::Dlss,0x10de,{backend.capability()});
    rk::FgPresentationCoordinator owner(session,backend);
    REQUIRE(std::holds_alternative<rk::Error>(owner.present(frame(1),true)));
    REQUIRE(std::holds_alternative<rk::Error>(owner.present(frame(1),true)));
}
