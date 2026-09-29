#pragma once
#include <d3d11.h>
#include <dxgi.h>
#include <cstdint>
#include <string_view>

namespace rk {
struct OwnedRouteSite;
struct ReshadeFactoryDelegateFacts {
    std::uintptr_t delegate{},vtable{},createMethod{};
    bool methodExecutable{};
};
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
