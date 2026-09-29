#pragma once
#include "rk/FgPrivateSwapAdmission.hpp"
#include "rk/FgStreamlineRuntime.hpp"
#include "rk/FactoryCreateTrace.hpp"
#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <atomic>
#include <memory>
#include <mutex>

namespace rk {
// Prepared before Skyrim's D3D11 swap creation. The object must outlive the
// returned facade and every Streamline proxy callback that can reach it.
class FgPrivateSwapRoute final {
public:
    static Result<std::unique_ptr<FgPrivateSwapRoute>> prepare(
        IDXGIAdapter* selected,const DXGI_SWAP_CHAIN_DESC& gameDesc,
        const std::filesystem::path& privateRuntime);
    ~FgPrivateSwapRoute() noexcept;
    FgPrivateSwapRoute(const FgPrivateSwapRoute&)=delete;
    FgPrivateSwapRoute& operator=(const FgPrivateSwapRoute&)=delete;
    Result<Microsoft::WRL::ComPtr<IDXGISwapChain4>> createFacade(
        FactoryCreateFn nativeCreate,IDXGIFactory* nativeFactory,
        ID3D11Device* nativeD11,const DXGI_SWAP_CHAIN_DESC& request,
        bool nativeMethodOwner) noexcept;
    const FgPrivateSwapAdmission& admission() const noexcept {return expected_;}
    bool issued() const noexcept {return issued_.load(std::memory_order_acquire);}
    void abandon() noexcept {
        std::scoped_lock lock(mutex_);
        if(!issued())retire();
    }
private:
    FgPrivateSwapRoute()=default;
    void retire() noexcept;
    std::unique_ptr<FgStreamlineRuntime> runtime_;
    Microsoft::WRL::ComPtr<ID3D12Device> d12_;
    Microsoft::WRL::ComPtr<ID3D12Device> upgradedD12_;
    Microsoft::WRL::ComPtr<ID3D12Device> verifiedNative_;
    Microsoft::WRL::ComPtr<IDXGIFactory6> parentFactory_;
    Microsoft::WRL::ComPtr<IDXGIFactory6> upgradedFactory_;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue_;
    Microsoft::WRL::ComPtr<IDXGISwapChain4> lower_;
    LUID adapterLuid_{};
    FgPrivateSwapAdmission expected_{};
    std::mutex mutex_;
    std::atomic<bool> attempted_{false};
    std::atomic<bool> issued_{false};
};
}
