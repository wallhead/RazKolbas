#pragma once
#include "rk/RendererHook.hpp"
#include "rk/SwapObserver.hpp"
#include <filesystem>
#include <vector>
namespace rk {
// Exact 1.6.1170 read-only layout, independently correlated with live renderer
// pointers. These are candidates, not validated SR guide semantics.
inline constexpr std::uint32_t renderer1170Rva=0x32887c0;
inline constexpr std::size_t renderer1170Size=0x2840;
bool frameProbeBoundary(std::string_view profile,SwapCall call) noexcept;
Result<std::array<std::uintptr_t,3>> rendererCandidatePointers(std::span<const std::uint8_t> renderer,
    std::uint32_t thread,std::uintptr_t device,std::uintptr_t context,std::uintptr_t swap);
struct ProbeImage {
    D3D11_TEXTURE2D_DESC descriptor{};
    std::size_t rowBytes{};
    std::vector<std::uint8_t> pixels;
};
// Caller owns resources and exclusive immediate-context access for this call.
// No render-state mutation. Local staging resources retire after Map/Unmap;
// only tightly packed CPU bytes leave the function.
Result<std::vector<ProbeImage>> readbackCandidates(ID3D11DeviceContext* context,
    std::span<ID3D11Texture2D* const> textures,std::size_t budget=64*1024*1024);
// Writes a bounded, already-read-back bundle to a new absolute directory.
// Existing captures are never replaced; the manifest appears only when all
// raw files have been written successfully.
Result<bool> saveProbeBundle(const std::filesystem::path& directory,
    std::span<const ProbeImage> images,std::span<const std::string_view> names,
    std::string_view description="RazKolbas prepared SR input capture; no NGX submission");
}
