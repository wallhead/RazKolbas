#include "rk/RenderSizePolicy.hpp"
#include <algorithm>

namespace rk {
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
