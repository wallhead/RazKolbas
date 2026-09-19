#pragma once
#include "rk/Settings.hpp"
#include <vector>
namespace rk {
enum class Provider { Native, DLSS, FSR, XeSS, Auto };
enum class Validation { Unverified, Validated, Experimental };
struct FeatureCapability {
    Provider provider{Provider::Native};
    bool available{};
    Validation validation{Validation::Unverified};
    std::int64_t rawResult{};
    std::string reason;
};
struct Selection { Provider requested, effective; std::string reason; };
Selection selectSr(Provider requested, std::uint32_t renderVendor, const std::vector<FeatureCapability>& caps, bool allowExperimental = false);
}
