#include "rk/ProbeRetirement.hpp"
namespace rk {
Result<bool> retireProbeRuntime(const std::function<std::uint32_t()>& shutdown,
                               const std::function<Result<bool>()>& restore,
                               const std::function<void()>& unload) {
    if (shutdown() != 1U) return Error{ErrorCode::Unavailable,"Shutdown did not confirm retirement; runtime and patch must remain loaded until controlled process termination"};
    const auto restored = restore();
    if (const auto error = std::get_if<Error>(&restored)) return *error;
    if (!std::get<bool>(restored)) return Error{ErrorCode::Conflict,"Patch restoration incomplete; runtime retained"};
    unload();
    return true;
}
}
