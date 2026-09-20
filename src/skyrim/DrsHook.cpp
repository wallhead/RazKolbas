#include "rk/DrsHook.hpp"

namespace rk {
Result<bool> installDrsProbe(HMODULE, std::string_view, const Settings& settings) {
    if (settings.get<bool>("Diagnostics.ProbeReducedWorld")) {
        return Error{ErrorCode::Unsupported,
            "Reduced-world ratio probe retired: Skyrim uses a shared DRS counter "
            "and allocates scene targets at display size"};
    }
    return false;
}

void bindDrsDisplay(std::uint32_t, std::uint32_t) noexcept {}
bool drsProbeActive() noexcept { return false; }
bool drsProbeHasRun() noexcept { return false; }
bool drsProbeConfirmNativeRecovery(Extent, Extent, Extent, Extent) noexcept { return false; }
std::optional<Extent> drsProbeRenderExtent() noexcept { return std::nullopt; }
}
