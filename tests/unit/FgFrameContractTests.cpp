#include <catch2/catch_test_macros.hpp>
#include "rk/FgFrameContract.hpp"
#include "rk/FrameContracts.hpp"

namespace {
rk::FgSourceFrame source() {
    rk::FgSourceFrame frame{};
    frame.source=41;
    frame.generation=7;
    frame.presentToken=9001;
    frame.resetEpoch=3;
    frame.render={1485,835};
    frame.display={2560,1440};
    frame.sr=rk::FgSrProvider::Dlss;
    frame.renderVendor=0x10de;
    frame.ownerReady=true;
    frame.worldActive=true;
    frame.cameraValid=true;
    frame.color={41,7,{2560,1440},true,3};
    frame.depth={41,7,{1485,835},true,3};
    frame.motion={41,7,{1485,835},true,3};
    frame.hudless={41,7,{2560,1440},true,3};
    frame.uiColorAlpha={41,7,{2560,1440},true,3};
    return frame;
}
rk::FgCapability capability(rk::FgProvider provider,
    std::uint32_t pairings=rk::fgSrMask(rk::FgSrProvider::Dlss)) {
    return {provider,true,true,1,pairings,true};
}
}

TEST_CASE("Auto selects one validated FG provider on the actual NVIDIA adapter",
    "[fg_contract]") {
    const auto frame=source();
    const rk::FgRequest request{true,rk::FgProvider::Auto,1};
    const auto both=rk::decideFg(frame,request,{
        capability(rk::FgProvider::Fsr),capability(rk::FgProvider::Dlss)});
    REQUIRE(both.requested==rk::FgProvider::Auto);
    REQUIRE(both.effective==rk::FgProvider::Dlss);
    REQUIRE(both.extraFrames==1);
    REQUIRE(both.reason==rk::FgReason::Ready);
    const auto fsr=rk::decideFg(frame,request,{capability(rk::FgProvider::Fsr)});
    REQUIRE(fsr.effective==rk::FgProvider::Fsr);
    for(const auto vendor:{0x1002u,0x8086u}) {
        auto other=frame;
        other.renderVendor=vendor;
        REQUIRE(rk::decideFg(other,request,{
            capability(rk::FgProvider::Dlss),capability(rk::FgProvider::Fsr)}).effective==
            rk::FgProvider::Fsr);
        REQUIRE(rk::decideFg(other,{true,rk::FgProvider::Dlss,1},{
            capability(rk::FgProvider::Dlss)}).reason==
            rk::FgReason::UnsupportedAdapter);
        const rk::FgProviderSession session({true,rk::FgProvider::Auto,1},other.sr,
            vendor,{capability(rk::FgProvider::Dlss),capability(rk::FgProvider::Fsr)});
        REQUIRE(session.boundProvider()==rk::FgProvider::Fsr);
    }
    auto unknown=frame;
    unknown.renderVendor=0x1234;
    REQUIRE(rk::decideFg(unknown,request,{capability(rk::FgProvider::Fsr)}).reason==
        rk::FgReason::UnsupportedAdapter);
}

TEST_CASE("Explicit FSR request persists when its SR pairing is unvalidated",
    "[fg_contract]") {
    const auto frame=source();
    const rk::FgRequest request{true,rk::FgProvider::Fsr,1};
    const auto rejected=rk::decideFg(frame,request,{
        capability(rk::FgProvider::Fsr,rk::fgSrMask(rk::FgSrProvider::Fsr))});
    REQUIRE(rejected.requested==rk::FgProvider::Fsr);
    REQUIRE(rejected.effective==rk::FgProvider::Off);
    REQUIRE(rejected.reason==rk::FgReason::UnvalidatedPairing);
    const auto accepted=rk::decideFg(frame,request,{
        capability(rk::FgProvider::Fsr,rk::fgSrMask(rk::FgSrProvider::Dlss))});
    REQUIRE(accepted.effective==rk::FgProvider::Fsr);
}

TEST_CASE("FG session pins one provider even when its live capability drops",
    "[fg_contract]") {
    const auto frame=source();
    const rk::FgRequest request{true,rk::FgProvider::Auto,1};
    const rk::FgProviderSession session(request,frame.sr,frame.renderVendor,{
        capability(rk::FgProvider::Fsr),capability(rk::FgProvider::Dlss)});
    REQUIRE(session.boundProvider()==rk::FgProvider::Dlss);
    REQUIRE(session.decide(frame,capability(rk::FgProvider::Dlss)).effective==
        rk::FgProvider::Dlss);
    auto failed=capability(rk::FgProvider::Dlss);
    failed.supported=false;
    const auto off=session.decide(frame,failed);
    REQUIRE(off.requested==rk::FgProvider::Auto);
    REQUIRE(off.effective==rk::FgProvider::Off);
    REQUIRE(off.reason==rk::FgReason::ProviderUnavailable);
    REQUIRE(session.decide(frame,capability(rk::FgProvider::Fsr)).effective==
        rk::FgProvider::Off);
}

TEST_CASE("FG session can toggle Off and On without changing its bound provider",
    "[fg_contract]") {
    const auto frame=source();
    const rk::FgRequest startup{false,rk::FgProvider::Auto,1};
    const rk::FgProviderSession session(startup,frame.sr,frame.renderVendor,{
        capability(rk::FgProvider::Fsr),capability(rk::FgProvider::Dlss)});
    REQUIRE(session.boundProvider()==rk::FgProvider::Dlss);
    const auto cap=capability(rk::FgProvider::Dlss);
    REQUIRE(session.decide(frame,cap,false).reason==rk::FgReason::Disabled);
    REQUIRE(session.decide(frame,cap,true).effective==rk::FgProvider::Dlss);
    REQUIRE(session.decide(frame,cap,false).effective==rk::FgProvider::Off);
    REQUIRE(session.boundProvider()==rk::FgProvider::Dlss);
}

TEST_CASE("FG rejects stale guides and a non-native UI plane", "[fg_contract]") {
    const rk::FgRequest request{true,rk::FgProvider::Dlss,1};
    const auto cap=capability(rk::FgProvider::Dlss);
    auto frame=source();
    frame.motion.source=40;
    REQUIRE(rk::decideFg(frame,request,{cap}).reason==rk::FgReason::StaleInput);
    frame=source();
    frame.motion.resetEpoch=2;
    REQUIRE(rk::decideFg(frame,request,{cap}).reason==rk::FgReason::StaleInput);
    frame=source();
    frame.uiColorAlpha.extent=frame.render;
    REQUIRE(rk::decideFg(frame,request,{cap}).reason==rk::FgReason::MissingNativeUi);
    frame=source();
    frame.loading=true;
    REQUIRE(rk::decideFg(frame,request,{cap}).reason==rk::FgReason::InactiveScene);
    frame=source();
    frame.cameraCut=true;
    REQUIRE(rk::decideFg(frame,request,{cap}).reason==rk::FgReason::HistoryReset);
}

TEST_CASE("FG retirement waits for provider input and allocator fences independently",
    "[fg_contract]") {
    const rk::FgRetirementSet lease{7,10,12,14,16,18};
    REQUIRE_FALSE(lease.ready({7,10,12,13,16,18}));
    REQUIRE_FALSE(lease.ready({7,10,12,14,16,17}));
    REQUIRE_FALSE(lease.ready({8,10,12,14,16,18}));
    REQUIRE(lease.ready({7,10,12,14,16,18}));
}

TEST_CASE("FG input slots cannot be reused across unretired provider work or resize",
    "[fg_contract]") {
    rk::FgLeasePool pool(7);
    const rk::FgFenceProgress start{7,0,0,0,0,0};
    const auto first=pool.acquire(start);
    REQUIRE(first.has_value());
    REQUIRE_FALSE(pool.submit(*first,{8,10,12,14,16,18}));
    REQUIRE(pool.submit(*first,{7,10,12,14,16,18}));
    REQUIRE_FALSE(pool.releaseUnsubmitted(*first));
    REQUIRE_FALSE(pool.advanceGeneration(8,{7,10,12,13,16,18}));
    const auto second=pool.acquire(start);
    const auto third=pool.acquire(start);
    REQUIRE(second.has_value());
    REQUIRE(third.has_value());
    REQUIRE_FALSE(pool.acquire(start).has_value());
    REQUIRE(pool.releaseUnsubmitted(*second));
    REQUIRE(pool.releaseUnsubmitted(*third));
    const auto reused=pool.acquire({7,10,12,13,16,18});
    REQUIRE(reused.has_value());
    REQUIRE(*reused!=*first);
    REQUIRE(pool.releaseUnsubmitted(*reused));
    REQUIRE(pool.advanceGeneration(8,{7,10,12,14,16,18}));
    REQUIRE(pool.generation()==8);
    REQUIRE(pool.acquire({8,0,0,0,0,0}).has_value());
}

TEST_CASE("A real frame has one Present while generated output leaves source identity alone",
    "[fg_contract]") {
    rk::FgPresentLedger ledger;
    rk::FrameIdentity identity;
    const auto first=identity.beginSource();
    auto frame=source();
    frame.source=first;
    frame.color.source=first;
    frame.depth.source=first;
    frame.motion.source=first;
    frame.hudless.source=first;
    frame.uiColorAlpha.source=first;
    REQUIRE(ledger.acceptReal(frame));
    REQUIRE_FALSE(ledger.acceptReal(frame));
    REQUIRE(identity.presentGenerated(first));
    REQUIRE(identity.source()==first);
    REQUIRE(identity.generated()==1);
    frame.source=identity.beginSource();
    frame.presentToken=9002;
    REQUIRE(ledger.acceptReal(frame));
    REQUIRE_FALSE(identity.presentGenerated(first));
}
