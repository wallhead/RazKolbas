#pragma once
#include "rk/Result.hpp"
#include <d3d11.h>
#include <cstdint>
#include <vector>

namespace rk {
struct PipelineResourceSlot {
    unsigned slot{};
    std::uintptr_t resourceIdentity{};
    DXGI_FORMAT format{DXGI_FORMAT_UNKNOWN};
    UINT width{},height{};
    bool matchesScene{};
};
struct PipelineBoundary {
    bool pixelShaderBound{},vertexShaderBound{};
    std::uintptr_t pixelShaderIdentity{},vertexShaderIdentity{};
    D3D11_PRIMITIVE_TOPOLOGY topology{D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED};
    UINT viewportCount{};
    std::vector<PipelineResourceSlot> resources;
};
// Read-only immediate-context snapshot at the post-world boundary. COM getters
// retain their results only during inspection; the caller's bindings are intact.
Result<PipelineBoundary> inspectPipelineBoundary(ID3D11DeviceContext* context,
    ID3D11Texture2D* scene);
}
