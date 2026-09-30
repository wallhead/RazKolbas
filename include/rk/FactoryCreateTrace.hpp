#pragma once
#include <d3d11.h>
#include <dxgi.h>
#include <cstdint>
#include <array>
#include <string_view>

namespace rk {
struct OwnedRouteSite;
struct ReshadeFactoryDelegateFacts {
    std::uintptr_t delegate{},vtable{},createMethod{};
    bool methodExecutable{};
};
struct FactoryMethodCodeFacts {
    std::array<std::uint8_t,64> bytes{},targetBytes{};
    std::uintptr_t allocationBase{},jumpTarget{},indirectSlot{};
    std::uint32_t protection{},state{},type{};
    bool readable{},targetReadable{};
};
// Read-only diagnostic. Recognizes only entry E9, FF25 and MOV RAX/JMP RAX;
// follows one target and never treats a recognized jump as an approved hook.
FactoryMethodCodeFacts inspectFactoryMethodCode(std::uintptr_t method) noexcept;
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
