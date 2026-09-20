#pragma once
#include "rk/Result.hpp"
#include "rk/Settings.hpp"
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <cstdint>
#include <string_view>

namespace rk {
inline constexpr std::string_view worldDrawPatchId="skyrim1170.world-draw.sr-v1";
// Called only at SKSEPlugin_Load, before any renderer/world-draw execution.
// An installed hook and relay remain process-lifetime; no hot unpatch.
Result<bool> installWorldDrawPassThrough(HMODULE game,std::string_view verifiedGameHash,
    const Settings& settings);
std::uint64_t worldDrawForwardedCalls() noexcept;
// First verified creation outputs are numeric lifetime anchors. This does not
// retain COM references; the world callback validates them under the engine lock.
void bindWorldDrawRenderer(ID3D11Device* device,ID3D11DeviceContext* context,
    IDXGISwapChain* swap) noexcept;
}
