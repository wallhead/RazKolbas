#pragma once
#include "rk/Result.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <dxgi.h>

namespace rk {
// Once native UI publication has been admitted, keep that boundary stable.
// A transient source rejection may select spatial fallback for the scene, but
// must not move publication after UI and change composition order.
constexpr bool shouldUseMenuPublication(bool routeActivated,bool sourceReady,
    bool routeAvailable) noexcept {
    return routeAvailable&&(routeActivated||sourceReady);
}
// A sparse readback admits the first provider frame. Once NGX has successfully
// published for this exact owned-scene generation, later diagnostic samples do
// not demote individual frames to a different presentation path. A resource
// generation change still requires fresh colour/depth admission.
constexpr bool shouldSubmitOwnedProvider(bool sourceReady,
    std::uint64_t admittedGeneration,std::uint64_t currentGeneration) noexcept {
    return sourceReady||(currentGeneration&&admittedGeneration==currentGeneration);
}
struct OwnedRouteSite {
    std::string_view id,moduleSha256;
    std::size_t fileSize{};
    std::uint32_t imageSize{},tableRva{},methodRva{};
    unsigned slot{};
    std::array<std::uint8_t,16> prologue{};
};
const OwnedRouteSite& reshade673FactoryCreateSite() noexcept;
const OwnedRouteSite& reshade673SwapGetBufferSite() noexcept;
const OwnedRouteSite& reshade673SwapGetDescSite() noexcept;
const OwnedRouteSite& enbContextOmSite() noexcept;
const OwnedRouteSite& enbContextViewportSite() noexcept;
const OwnedRouteSite& enbContextScissorSite() noexcept;
const OwnedRouteSite& enbContextPsResourcesSite() noexcept;
std::span<const OwnedRouteSite> enbContextSamplerSites() noexcept;
// Mapped image bytes, not disk file layout. The caller separately verifies the
// loaded file's hash and the actual COM object's table address before patching.
Result<bool> validateOwnedRouteSite(std::span<const std::uint8_t> image,
    std::uintptr_t moduleBase,std::string_view verifiedFileHash,
    std::size_t verifiedFileSize,std::uint32_t actualTableRva,
    const OwnedRouteSite& site);
// The only live-confirmed game request that caches the SDR scene views.
// The caller first verifies the game image's identity at renderer startup.
bool isSkyrim1170OwnedSceneBufferCall(std::uintptr_t returnAddress,
    std::uintptr_t gameBase,std::string_view verifiedGameHash,
    IDXGISwapChain* swap,IDXGISwapChain* selectedSwap,
    UINT index,REFIID iid) noexcept;
bool isEnb20260508OwnedSceneBufferCall(std::uintptr_t returnAddress,
    std::uintptr_t enbBase,std::string_view verifiedEnbHash) noexcept;
bool isEnb20260508ReducedDescriptionCall(std::uintptr_t returnAddress,
    std::uintptr_t enbBase,std::string_view verifiedEnbHash) noexcept;
}
