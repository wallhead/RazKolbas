#pragma once
#include <d3d11.h>
#include <dxgi.h>
#include <cstdint>
#include <array>
#include <string_view>
#include <string>
#include "rk/Result.hpp"

namespace rk {
struct OwnedRouteSite;
struct ReshadeFactoryDelegateFacts {
    std::uintptr_t delegate{},vtable{},createMethod{};
    bool methodExecutable{};
};
struct FactoryMethodCodeFacts {
    std::array<std::uint8_t,64> bytes{},targetBytes{};
    std::uintptr_t allocationBase{},jumpTarget{},callTarget{},indirectSlot{};
    std::uint32_t protection{},state{},type{};
    bool readable{},targetReadable{};
};
// Read-only diagnostic. Recognizes entry E9, FF25, MOV RAX/JMP RAX and E8;
// follows one jump or call target and never treats it as an approved hook.
FactoryMethodCodeFacts inspectFactoryMethodCode(std::uintptr_t method) noexcept;
struct SteamFactoryInlineProfile {
    std::string_view id,moduleHash;
    std::size_t fileSize{};
    std::uint32_t imageSize{},callbackRva{},originalPointerRva{};
    std::array<std::uint8_t,129> callbackCode{};
};
struct SteamFactoryInlineFacts {
    std::uintptr_t nativeMethod{},relay{},callback{},overlayBase{},originalTrampoline{};
    std::string overlayHash;
    std::size_t overlayFileSize{},overlayImageSize{};
    std::array<std::uint8_t,16> entry{};
    std::array<std::uint8_t,14> relayCode{};
    std::array<std::uint8_t,10> trampolineCode{};
    std::array<std::uint8_t,129> callbackCode{};
};
const SteamFactoryInlineProfile& steamFactoryInlineProfile() noexcept;
Result<bool> validateSteamFactoryInline(const SteamFactoryInlineFacts& facts);
// Validates the exact native E9 -> private FF25 relay -> pinned Steam
// callback and original trampoline. Pins Steam only after full validation.
// Does not patch the native entry, relay, trampoline or Steam data/code.
Result<bool> inspectAndPinSteamFactoryInline(std::uintptr_t nativeMethod);
// Exact ReShade 6.8 factory method reads [this+8] as its downstream
// IDXGIFactory. Caller must first verify the ReShade build and vtable site.
// This structural probe does not call any COM method or retain the object.
ReshadeFactoryDelegateFacts inspectReshadeFactoryDelegate(
    IDXGIFactory* verifiedReshadeFactory) noexcept;
using FactoryCreateFn=HRESULT(STDMETHODCALLTYPE*)(IDXGIFactory*,IUnknown*,
    DXGI_SWAP_CHAIN_DESC*,IDXGISwapChain**);
// Verify the wrapper's own vtable, not the swap chain returned by it.
bool isReshadeFactoryDelegateSite(IDXGIFactory* factory,
    std::uintptr_t moduleBase,std::string_view moduleHash,
    FactoryCreateFn originalMethod,const OwnedRouteSite& site) noexcept;
using FactoryCreatedFn=void(*)(IDXGIFactory*,IUnknown*,
    const DXGI_SWAP_CHAIN_DESC*,IDXGISwapChain*,HRESULT,void*) noexcept;
HRESULT observeFactoryCreate(FactoryCreateFn next,IDXGIFactory* factory,
    IUnknown* device,DXGI_SWAP_CHAIN_DESC* description,IDXGISwapChain** output,
    FactoryCreatedFn observed,void* context) noexcept;
// Select only the captured game factory and the verified native SDR flip
// contract before consuming the process-lifetime early-route attempt.
bool isOwnedSceneFactoryCandidate(IDXGIFactory* factory,
    IDXGIFactory* expected,const DXGI_SWAP_CHAIN_DESC* description) noexcept;
}
