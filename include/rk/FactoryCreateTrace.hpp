#pragma once
#include <d3d11.h>
#include <dxgi.h>

namespace rk {
using FactoryCreateFn=HRESULT(STDMETHODCALLTYPE*)(IDXGIFactory*,IUnknown*,
    DXGI_SWAP_CHAIN_DESC*,IDXGISwapChain**);
using FactoryCreatedFn=void(*)(IDXGIFactory*,IUnknown*,
    const DXGI_SWAP_CHAIN_DESC*,IDXGISwapChain*,HRESULT,void*) noexcept;
HRESULT observeFactoryCreate(FactoryCreateFn next,IDXGIFactory* factory,
    IUnknown* device,DXGI_SWAP_CHAIN_DESC* description,IDXGISwapChain** output,
    FactoryCreatedFn observed,void* context) noexcept;
}
