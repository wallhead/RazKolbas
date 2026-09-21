#pragma once
#include "rk/Result.hpp"
#include "rk/FrameContracts.hpp"
#include "rk/Settings.hpp"
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <cstdint>
#include <string_view>

namespace rk {
inline constexpr std::string_view worldDrawPatchId="skyrim1170.world-draw.sr-v1";
inline constexpr std::string_view menuDisplayPatchId=
    "skyrim1170.menu-post-display.start-v1";
// Called only at SKSEPlugin_Load, before any renderer/world-draw execution.
// An installed hook and relay remain process-lifetime; no hot unpatch.
Result<bool> installWorldDrawPassThrough(HMODULE game,std::string_view verifiedGameHash,
    const Settings& settings);
std::uint64_t worldDrawForwardedCalls() noexcept;
// First verified creation outputs are numeric lifetime anchors. This does not
// retain COM references; the world callback validates them under the engine lock.
void bindWorldDrawRenderer(ID3D11Device* device,ID3D11DeviceContext* context,
    IDXGISwapChain* swap) noexcept;
// Same presenter/session that will evaluate the first owned reduced frame.
// Call only after the renderer creation result is captured and before the
// game's view-cache GetBuffer consumer resumes.
Result<Extent> prepareWorldOwnedSrPlan(ID3D11Device* device,
    ID3D11DeviceContext* context,Extent display);
Result<bool> abandonWorldOwnedSrPlan(ID3D11DeviceContext* context);
// One bounded, read-only D3D11 target identity snapshot on the next eligible
// ENB Present after a world-like frame. Never dereferences renderer memory.
void probePresentationTargets(IDXGISwapChain* swap) noexcept;
// Called at the verified nested ReShade Present entry. In the installed
// ENB -> ReShade chain this is after ENB has returned from its effect work
// and before ReShade forwards/presents. The callback only records two
// bounded pixel snapshots requested by the owned-scene controller.
void probePostEnbPresentationTarget(IDXGISwapChain* swap) noexcept;
}
