#include "rk/RenderSizePolicy.hpp"
#include <algorithm>
#include <cmath>

namespace rk {
std::optional<UpscaleQuality> parseUpscaleQuality(std::string_view value) noexcept {
    if(value=="NativeAA")return UpscaleQuality::NativeAA;
    if(value=="Quality")return UpscaleQuality::Quality;
    if(value=="Balanced")return UpscaleQuality::Balanced;
    if(value=="Performance")return UpscaleQuality::Performance;
    if(value=="UltraPerformance")return UpscaleQuality::UltraPerformance;
    return std::nullopt;
}
Extent planEarlyOwnedScene(Extent display,UpscaleQuality quality,
    double manualScale) noexcept {
    if(!display.valid()||display.width>8192||display.height>8192||
       !std::isfinite(manualScale)||manualScale<0.0||manualScale>=1.0||
       (manualScale>0.0&&manualScale<0.125)||quality==UpscaleQuality::NativeAA)
        return {};
    double scale=manualScale;
    if(scale==0.0) {
        switch(quality) {
        case UpscaleQuality::Quality:scale=2.0/3.0;break;
        case UpscaleQuality::Balanced:scale=0.58;break;
        case UpscaleQuality::Performance:scale=0.5;break;
        case UpscaleQuality::UltraPerformance:scale=1.0/3.0;break;
        default:return {};
        }
    }
    const auto width=static_cast<std::uint32_t>(std::lround(display.width*scale));
    const auto height=static_cast<std::uint32_t>(std::lround(display.height*scale));
    if(!width||!height||width>=display.width&&height>=display.height)return {};
    return {width,height};
}
RenderSizePlan chooseRenderSize(Extent display,Extent requested,
    bool worldTargetReady,bool displayPathReady) noexcept {
    if(!display.valid()) return {};
    const bool bounded=requested.valid() && requested.width<=display.width &&
        requested.height<=display.height;
    const bool smaller=bounded && (requested.width<display.width ||
        requested.height<display.height);
    if(worldTargetReady && displayPathReady && smaller)
        return {display,requested,true};
    return {display,display,false};
}

namespace {
std::uint32_t lower(std::uint64_t value,std::uint32_t render,
    std::uint32_t display) noexcept {
    return static_cast<std::uint32_t>(value*render/display);
}
std::uint32_t upper(std::uint64_t value,std::uint32_t render,
    std::uint32_t display) noexcept {
    const auto product=value*render;
    return static_cast<std::uint32_t>(product/display+(product%display!=0));
}
}

ScissorExtent adaptScissor(const RenderSizePlan& plan,RenderDomain domain,
    ScissorExtent source) noexcept {
    if(!plan.valid() || !plan.reduced || domain!=RenderDomain::World)
        return source;
    const auto left=std::min<std::uint64_t>(source.x,plan.display.width);
    const auto top=std::min<std::uint64_t>(source.y,plan.display.height);
    const auto right=std::min<std::uint64_t>(
        std::uint64_t(source.x)+source.width,plan.display.width);
    const auto bottom=std::min<std::uint64_t>(
        std::uint64_t(source.y)+source.height,plan.display.height);
    const auto x=lower(left,plan.render.width,plan.display.width);
    const auto y=lower(top,plan.render.height,plan.display.height);
    const auto xEnd=source.width==0?x:upper(right,plan.render.width,plan.display.width);
    const auto yEnd=source.height==0?y:upper(bottom,plan.render.height,plan.display.height);
    return {x,y,xEnd-x,yEnd-y};
}
}
