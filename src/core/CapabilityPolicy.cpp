#include "rk/Capabilities.hpp"
#include <algorithm>
#include <array>
namespace rk {
Selection selectSr(Provider requested, std::uint32_t renderVendor, const std::vector<FeatureCapability>& capabilities, bool allowExperimental) {
    auto capability = [&](Provider provider) -> const FeatureCapability* {
        const auto found = std::find_if(capabilities.begin(), capabilities.end(), [&](const auto& cap) { return cap.provider == provider; });
        return found == capabilities.end() ? nullptr : &*found;
    };
    auto usable = [&](Provider provider) {
        const auto cap = capability(provider);
        return cap && cap->available && (cap->validation == Validation::Validated || (allowExperimental && cap->validation == Validation::Experimental));
    };
    if (requested == Provider::Native) return {requested, Provider::Native, "Native requested"};
    if (requested != Provider::Auto) {
        if (usable(requested)) return {requested, requested, "Validated requested provider"};
        const auto cap = capability(requested);
        return {requested, Provider::Native, cap && !cap->reason.empty() ? cap->reason : "Requested provider is unavailable or unvalidated"};
    }
    const auto preferred = renderVendor == 0x10de ? Provider::DLSS : renderVendor == 0x8086 ? Provider::XeSS : Provider::FSR;
    for (const auto provider : std::array{preferred, Provider::DLSS, Provider::FSR, Provider::XeSS})
        if (usable(provider)) return {requested, provider, "Auto selected a validated provider on the render adapter"};
    return {requested, Provider::Native, "No validated SR provider available"};
}
}
