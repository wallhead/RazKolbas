#pragma once
#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_6.h>

namespace rk {
struct FgSwapFacadeFacts {
    HRESULT getDesc{E_POINTER},swap1{E_POINTER},swap3{E_POINTER},
        swap4{E_POINTER},getD3D11Device{E_POINTER},getD3D12Device{E_POINTER};
    DXGI_SWAP_CHAIN_DESC desc{};
    bool expectedDeviceIdentity{};
};
// Read-only, bounded at creation. It retains no references after return.
FgSwapFacadeFacts inspectFgSwapFacade(IDXGISwapChain* swap,
    IUnknown* expectedDevice) noexcept;
}
