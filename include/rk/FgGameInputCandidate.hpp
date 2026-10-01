#pragma once
#include "rk/FgWorldGuideLatch.hpp"
#include "rk/FgUiPlanes.hpp"
#include "rk/FgInputLeaseRing.hpp"
#include <array>

namespace rk {
// A source-only, retained D3D11 candidate. It does not establish a lower
// presentation owner, provider token, copy completion or provider retirement.
struct FgGameInputCandidate {
    FgSourceFrame frame;
    std::array<Microsoft::WRL::ComPtr<ID3D11Texture2D>,5> textures{};
    FgInputSources sources() const noexcept;
};

Result<FgGameInputCandidate> pairFgGameInputs(
    const FgWorldGuideFrame& world,const FgUiPlaneFrame& ui);
}
