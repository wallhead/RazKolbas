#pragma once
#include "rk/RendererHook.hpp"
#include <dxgi1_4.h>
namespace rk {
inline constexpr std::string_view swapObserverPatchId="reshade673.swapchain-observe-v1";
bool validDisabledPatchIds(std::string_view ids);
bool patchDisabled(std::string_view ids,std::string_view id);
enum class SwapCall { Release, Present, Resize, Present1, Resize1 };
struct SwapEvent {
    SwapCall call{};
    bool before{};
    std::uintptr_t object{}; // Opaque only: Release may have destroyed it.
    HRESULT result{};
    ULONG references{};
    UINT interval{}, flags{}, buffers{}, width{}, height{};
    DXGI_FORMAT format{};
};
using SwapObserver=void(*)(const SwapEvent&);
using PresentFn=HRESULT(WINAPI*)(IDXGISwapChain*,UINT,UINT);
using Present1Fn=HRESULT(WINAPI*)(IDXGISwapChain1*,UINT,UINT,const DXGI_PRESENT_PARAMETERS*);
using ResizeFn=HRESULT(WINAPI*)(IDXGISwapChain*,UINT,UINT,UINT,DXGI_FORMAT,UINT);
using Resize1Fn=HRESULT(WINAPI*)(IDXGISwapChain3*,UINT,UINT,UINT,DXGI_FORMAT,UINT,const UINT*,IUnknown* const*);
using ReleaseFn=ULONG(WINAPI*)(IUnknown*);
HRESULT observePresent(PresentFn,IDXGISwapChain*,UINT,UINT,SwapObserver) noexcept;
HRESULT observePresent1(Present1Fn,IDXGISwapChain1*,UINT,UINT,const DXGI_PRESENT_PARAMETERS*,SwapObserver) noexcept;
HRESULT observeResize(ResizeFn,IDXGISwapChain*,UINT,UINT,UINT,DXGI_FORMAT,UINT,SwapObserver) noexcept;
HRESULT observeResize1(Resize1Fn,IDXGISwapChain3*,UINT,UINT,UINT,DXGI_FORMAT,UINT,const UINT*,IUnknown* const*,SwapObserver) noexcept;
ULONG observeRelease(ReleaseFn,IUnknown*,SwapObserver) noexcept;
struct SwapMethodProfile { unsigned slot; std::uint32_t rva; std::array<std::uint8_t,16> prologue; };
struct SwapTableProfile {
    std::string_view hash;
    std::size_t fileSize;
    std::uint32_t imageSize, tableRva;
    std::array<SwapMethodProfile,5> methods;
};
const SwapTableProfile& reshade673SwapProfile();
Result<bool> validateSwapTable(std::span<const std::uint8_t> image,std::uintptr_t base,
    std::string_view hash,std::size_t fileSize,std::uint32_t tableRva,const SwapTableProfile& profile);
}
