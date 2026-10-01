#pragma once
#include "rk/FgLowerSwap.hpp"
#include "rk/FgSharedInputs.hpp"
#include "rk/FgD3D11AuxSwapSource.hpp"
#include <d3d11.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <memory>
#include <chrono>
#include <vector>

namespace rk {
// A physical lower buffer and the commands that last wrote it. Its allocator
// may be reset only after fenceValue has completed on the owning queue.
struct FgD3D11CopySlot {
    Microsoft::WRL::ComPtr<ID3D12Resource> back;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commands;
    std::uint64_t fenceValue{};
};
// Sampled CPU wall time only. GPU execution, outer ReShade and ENB work are
// outside these intervals; a long stage is evidence for follow-up tracing.
struct FgBridgeCpuSample {
    std::uint64_t realPresent{};
    std::uint64_t slotWaitNs{},sharedCopyNs{},d3d12CopyNs{},
        lowerPresentNs{},totalNs{};
    bool queued{};
    HRESULT result{S_OK};
};
// D3D11-to-D3D12 colour path. D3D11 callers render to one stable
// logical buffer zero; valid facade indices alias it while the lower D3D12
// chain rotates physical destinations.
// Provider integration is separate.
class FgD3D11PresentBridge {
public:
    ~FgD3D11PresentBridge() noexcept;
    // verifiedLowerNative is optional for native DXGI. For a proxy lower,
    // the caller must obtain and verify it from that proxy's official
    // native-interface API; adapter equality alone is not sufficient.
    // queueOwnsLowerSwap is true only when the caller created the lower swap
    // using this exact direct queue. Other callers retain CPU completion.
    static Result<std::unique_ptr<FgD3D11PresentBridge>> create(
        ID3D11Device* d11,ID3D11DeviceContext* context,ID3D12Device* d12,
        ID3D12CommandQueue* queue,IDXGISwapChain* lower,
        ID3D12Device* verifiedLowerNative=nullptr,
        std::unique_ptr<FgD3D11AuxSwapSource> auxiliary=nullptr,
        std::shared_ptr<void> providerLifetime=nullptr,
        bool queueOwnsLowerSwap=false);
    UINT currentIndex() const noexcept;
    ID3D11Texture2D* renderBuffer(UINT index) const noexcept;
    HRESULT copyToCurrent() noexcept;
    HRESULT presentPrepared(const FgPresentCall& call) noexcept;
    HRESULT resize(const FgResizeCall& call) noexcept;
    bool prepared() const noexcept { return prepared_; }
    const char* copyPhase() const noexcept { return copyPhase_; }
    HRESULT copyResult() const noexcept { return copyResult_; }
    const std::string& copyDetail() const noexcept { return copyDetail_; }
    const char* presentPhase() const noexcept { return presentPhase_; }
    const char* firstCopyFailurePhase() const noexcept { return firstCopyFailurePhase_; }
    HRESULT firstCopyFailure() const noexcept { return firstCopyFailure_; }
    std::uint64_t copyCommandAllocations() const noexcept {
        return copyCommandAllocations_;
    }
    void beginCpuSample(std::uint64_t realPresent) noexcept;
    FgBridgeCpuSample endCpuSample(HRESULT result) noexcept;
private:
    HRESULT copyToCurrentImpl() noexcept;
    HRESULT copyToCurrentQueued(UINT index) noexcept;
    bool waitCopyFence(std::uint64_t value) const noexcept;
    bool drainCopySlots() const noexcept;
    FgD3D11PresentBridge(FgLowerSwap lower,
        std::unique_ptr<FgSharedInputs> interop) noexcept;
    std::shared_ptr<void> providerLifetime_;
    FgLowerSwap lower_;
    std::unique_ptr<FgSharedInputs> interop_;
    Microsoft::WRL::ComPtr<ID3D11Device> d11_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<ID3D12Device> d12_;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue_;
    Microsoft::WRL::ComPtr<IDXGISwapChain3> swap3_;
    std::vector<Microsoft::WRL::ComPtr<ID3D11Texture2D>> render_;
    FgSourceLease sourceLease_;
    std::unique_ptr<FgD3D11AuxSwapSource> auxiliary_;
    std::vector<FgSharedSurface> shared_;
    std::vector<FgD3D11CopySlot> copySlots_;
    Microsoft::WRL::ComPtr<ID3D12Fence> copyFence_;
    std::uint64_t nextCopyFenceValue_{};
    std::uint64_t copyCommandAllocations_{};
    bool queueOwnsLowerSwap_{};
    Microsoft::WRL::ComPtr<ID3D12Resource> inFlightBack_;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> inFlightAllocator_;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> inFlightCommands_;
    Microsoft::WRL::ComPtr<ID3D12Fence> inFlightFence_;
    bool prepared_{},poisoned_{};
    UINT preparedIndex_{};
    const char* copyPhase_{"not-called"};
    HRESULT copyResult_{S_OK};
    const char* presentPhase_{"not-called"};
    std::string copyDetail_;
    const char* firstCopyFailurePhase_{"none"};
    HRESULT firstCopyFailure_{S_OK};
    bool cpuSampleActive_{};
    std::chrono::steady_clock::time_point cpuSampleStart_{};
    FgBridgeCpuSample cpuSample_{};
};
}
