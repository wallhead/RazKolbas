#pragma once
#include "rk/FrameContracts.hpp"
#include <cstdint>

namespace rk {
enum class RenderDomain { World, Ui };
struct ScissorExtent {
    std::uint32_t x{},y{},width{},height{};
};
struct RenderSizePlan {
    Extent display{},render{};
    bool reduced{};
    bool valid() const noexcept { return display.valid() && render.valid() &&
        render.width<=display.width && render.height<=display.height; }
};

// Policy only. A caller must independently own the reduced world target and
// a display-sized output/fallback before enabling any engine size adapter.
RenderSizePlan chooseRenderSize(Extent display,Extent requested,
    bool worldTargetReady,bool displayPathReady) noexcept;
// Skyrim's verified scissor ABI uses x,y,width,height. UI stays native.
ScissorExtent adaptScissor(const RenderSizePlan& plan,RenderDomain domain,
    ScissorExtent source) noexcept;
}
