#include "rk/FgLiveFrameAdmission.hpp"

namespace rk {
namespace {
bool same(Extent a,Extent b) noexcept {
    return a.width==b.width&&a.height==b.height;
}
bool current(const FgResourceStamp& stamp,const FgSourceFrame& frame,
    Extent extent) noexcept {
    return stamp.ready&&stamp.source==frame.source&&
        stamp.generation==frame.generation&&
        stamp.resetEpoch==frame.resetEpoch&&same(stamp.extent,extent);
}
}
FgLiveFrameInspection inspectFgLiveFrame(const FgBoundarySample& boundary,
    const FgSourceFrame& frame,bool inputsRetained) noexcept {
    FgLiveFrameInspection result{};
    const auto gap=[&result](FgLiveGap value) {
        result.gaps|=static_cast<std::uint32_t>(value);
    };
    if(boundary.kind!=FgBoundaryKind::Ready||!boundary.phaseReady)
        gap(FgLiveGap::Phase);
    if(!frame.source||!frame.generation||!frame.presentToken||
       !frame.resetEpoch||!frame.render.valid()||!frame.display.valid()||
       frame.source!=boundary.world||
       frame.presentToken!=boundary.realPresent||
       frame.resetEpoch!=boundary.epoch)gap(FgLiveGap::Identity);
    if(!frame.ownerReady)gap(FgLiveGap::Owner);
    if(!frame.worldActive||frame.loading||frame.paused||frame.cameraCut)
        gap(FgLiveGap::Scene);
    if(!frame.cameraValid)gap(FgLiveGap::Camera);
    if(!current(frame.color,frame,frame.display))gap(FgLiveGap::Color);
    if(!current(frame.depth,frame,frame.render))gap(FgLiveGap::Depth);
    if(!current(frame.motion,frame,frame.render))gap(FgLiveGap::Motion);
    if(!current(frame.hudless,frame,frame.display))gap(FgLiveGap::Hudless);
    if(!current(frame.uiColorAlpha,frame,frame.display))gap(FgLiveGap::NativeUi);
    if(!inputsRetained)gap(FgLiveGap::Retirement);
    return result;
}
}
