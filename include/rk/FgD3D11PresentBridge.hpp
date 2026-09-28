#pragma once
#include "rk/FgLowerSwap.hpp"
#include "rk/FgSharedInputs.hpp"
#include <d3d11.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <memory>
#include <vector>

namespace rk {
// Offline proof of the physical D3D11-to-D3D12 colour path. The game-facing
// IDXGISwapChain facade, resize handling and provider integration are separate.
class FgD3D11PresentBridge {
public:
    ~FgD3D11PresentBridge() noexcept;
    static Result<std::unique_ptr<FgD3D11PresentBridge>> create(
        ID3D11Device* d11,ID3D11DeviceContext* context,ID3D12Device* d12,
        ID3D12CommandQueue* queue,IDXGISwapChain* lower);
    UINT currentIndex() const noexcept;
    ID3D11Texture2D* renderBuffer(UINT index) const noexcept;
    HRESULT copyToCurrent() noexcept;
    HRESULT presentPrepared(const FgPresentCall& call) noexcept;
    HRESULT resize(const FgResizeCall& call) noexcept;
    bool prepared() const noexcept { return prepared_; }
private:
    FgD3D11PresentBridge(FgLowerSwap lower,
        std::unique_ptr<FgSharedInputs> interop) noexcept;
    FgLowerSwap lower_;
    std::unique_ptr<FgSharedInputs> interop_;
    Microsoft::WRL::ComPtr<ID3D11Device> d11_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<ID3D12Device> d12_;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue_;
    Microsoft::WRL::ComPtr<IDXGISwapChain3> swap3_;
    std::vector<Microsoft::WRL::ComPtr<ID3D11Texture2D>> render_;
    std::vector<FgSharedSurface> shared_;
    Microsoft::WRL::ComPtr<ID3D12Resource> inFlightBack_;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> inFlightAllocator_;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> inFlightCommands_;
    Microsoft::WRL::ComPtr<ID3D12Fence> inFlightFence_;
    bool prepared_{},poisoned_{};
    UINT preparedIndex_{};
};
}
