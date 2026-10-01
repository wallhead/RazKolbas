#include <catch2/catch_test_macros.hpp>
#include "rk/FgLiveFrameAdmission.hpp"

namespace {
rk::FgSourceFrame frame() {
    rk::FgSourceFrame f{};
    f.source=41; f.generation=7; f.presentToken=9; f.resetEpoch=3;
    f.render={1485,835}; f.display={2560,1440};
    f.ownerReady=true; f.worldActive=true; f.cameraValid=true;
    f.color={41,7,f.display,true,3};
    f.depth={41,7,f.render,true,3};
    f.motion={41,7,f.render,true,3};
    f.hudless={41,7,f.display,true,3};
    f.uiColorAlpha={41,7,f.display,true,3};
    return f;
}
rk::FgBoundarySample boundary() {
    rk::FgBoundarySample b{};
    b.kind=rk::FgBoundaryKind::Ready;
    b.phaseReady=true; b.world=41; b.epoch=3; b.realPresent=9;
    return b;
}
}

TEST_CASE("FG live packet requires phase and rejects loading despite phase-ready",
    "[fg_live_admission]") {
    auto f=frame();
    auto b=boundary();
    REQUIRE(rk::inspectFgLiveFrame(b,f,true).ready());
    f.loading=true;
    REQUIRE(rk::inspectFgLiveFrame(b,f,true).has(rk::FgLiveGap::Scene));
    f.loading=false;
    f.paused=true;
    REQUIRE(rk::inspectFgLiveFrame(b,f,true).has(rk::FgLiveGap::Scene));
    f.paused=false;
    b.phaseReady=false;
    REQUIRE(rk::inspectFgLiveFrame(b,f,true).has(rk::FgLiveGap::Phase));
}

TEST_CASE("FG live packet rejects stale identity and each missing guide",
    "[fg_live_admission]") {
    auto f=frame();
    const auto b=boundary();
    f.presentToken=8;
    REQUIRE(rk::inspectFgLiveFrame(b,f,true).has(rk::FgLiveGap::Identity));
    f=frame(); f.motion.source=40;
    REQUIRE(rk::inspectFgLiveFrame(b,f,true).has(rk::FgLiveGap::Motion));
    f=frame(); f.depth.generation=6;
    REQUIRE(rk::inspectFgLiveFrame(b,f,true).has(rk::FgLiveGap::Depth));
    f=frame(); f.color.extent=f.render;
    REQUIRE(rk::inspectFgLiveFrame(b,f,true).has(rk::FgLiveGap::Color));
    f=frame(); f.hudless.ready=false;
    REQUIRE(rk::inspectFgLiveFrame(b,f,true).has(rk::FgLiveGap::Hudless));
    f=frame(); f.uiColorAlpha.resetEpoch=2;
    REQUIRE(rk::inspectFgLiveFrame(b,f,true).has(rk::FgLiveGap::NativeUi));
    f=frame(); f.cameraValid=false;
    REQUIRE(rk::inspectFgLiveFrame(b,f,true).has(rk::FgLiveGap::Camera));
    f=frame();
    REQUIRE(rk::inspectFgLiveFrame(b,f,false).has(rk::FgLiveGap::Retirement));
}

TEST_CASE("FG live packet reports all unowned inputs without manufacturing readiness",
    "[fg_live_admission]") {
    auto f=frame();
    f.ownerReady=false; f.cameraValid=false;
    f.color={}; f.depth={}; f.motion={}; f.hudless={}; f.uiColorAlpha={};
    const auto result=rk::inspectFgLiveFrame(boundary(),f,false);
    REQUIRE_FALSE(result.ready());
    REQUIRE(result.has(rk::FgLiveGap::Owner));
    REQUIRE(result.has(rk::FgLiveGap::Camera));
    REQUIRE(result.has(rk::FgLiveGap::Color));
    REQUIRE(result.has(rk::FgLiveGap::Depth));
    REQUIRE(result.has(rk::FgLiveGap::Motion));
    REQUIRE(result.has(rk::FgLiveGap::Hudless));
    REQUIRE(result.has(rk::FgLiveGap::NativeUi));
    REQUIRE(result.has(rk::FgLiveGap::Retirement));
}
