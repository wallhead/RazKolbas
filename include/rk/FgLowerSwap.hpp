#pragma once
#include "rk/FgPresentationCoordinator.hpp"
#include <dxgi1_4.h>
#include <wrl/client.h>

namespace rk {
enum class FgResizeMethod { ResizeBuffers, ResizeBuffers1 };
struct FgResizeCall {
    FgResizeMethod method{FgResizeMethod::ResizeBuffers};
    UINT buffers{},width{},height{};
    DXGI_FORMAT format{DXGI_FORMAT_UNKNOWN};
    UINT flags{};
    const UINT* creationNodeMask{};
    IUnknown* const* presentQueue{};
};

// Retains the real lower chain. This is deliberately not a D3D11-facing COM
// proxy: the Skyrim wrapper chain must be validated before replacing it.
class FgLowerSwap {
public:
    static Result<FgLowerSwap> create(IDXGISwapChain* swap);
    HRESULT present(const FgPresentCall& call) const noexcept;
    HRESULT resize(const FgResizeCall& call) const noexcept;
    HRESULT queryInterface(REFIID interfaceId,void** result) const noexcept;
    HRESULT getBuffer(UINT index,REFIID interfaceId,void** result) const noexcept;
    HRESULT getDevice(REFIID interfaceId,void** result) const noexcept;
    HRESULT getDesc(DXGI_SWAP_CHAIN_DESC* result) const noexcept;
private:
    Microsoft::WRL::ComPtr<IDXGISwapChain> swap_;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap1_;
    Microsoft::WRL::ComPtr<IDXGISwapChain3> swap3_;
};
}
