#pragma once
#include "rk/Result.hpp"
#include <d3d11_4.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>
#include <memory>
#include <mutex>

namespace rk {
class FgD3D11AuxSwapSource;
// A retained, immutable source whose device ownership was verified before a
// later wrapper can change the resource's GetDevice report.
class FgSourceLease {
public:
    FgSourceLease()=default;
private:
    friend class FgSharedInputs;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> source_;
    std::uint64_t ownerId_{};
};
struct FgCopyTicket {
    std::uint64_t producer{},copy{};
};
enum class FgCopyStatus { Pending, Complete, DeviceRemoved, Unknown };
FgCopyStatus classifyFgCopyStatus(std::uint64_t completed,
    std::uint64_t requested,HRESULT deviceHealth) noexcept;
// Holds the interop devices, queue and fences when work has uncertain
// retirement and its shared surfaces must outlive the bridge object.
struct FgInteropLifetime {
    Microsoft::WRL::ComPtr<ID3D11Device5> d11;
    Microsoft::WRL::ComPtr<ID3D12Device> d12;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue;
    Microsoft::WRL::ComPtr<ID3D11Fence> producer;
    Microsoft::WRL::ComPtr<ID3D12Fence> producerGate,consumer;
};

class FgSharedSurface {
public:
    ID3D11Texture2D* d11() const noexcept { return d11_.Get(); }
    ID3D12Resource* d12() const noexcept { return d12_.Get(); }
private:
    friend class FgSharedInputs;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> d11_;
    Microsoft::WRL::ComPtr<ID3D12Resource> d12_;
    std::uint64_t ownerId_{};
};

// Copies one real-frame input into a shared texture. Its ticket proves the
// D3D11 producer finished before the D3D12 queue reached its separate,
// single-writer copy fence.
// Provider-input and Present retirement remain the caller's separate duties.
class FgSharedInputs {
public:
    static Result<std::unique_ptr<FgSharedInputs>> create(ID3D11Device* d11,
        ID3D12Device* d12,ID3D12CommandQueue* queue);
    Result<FgSharedSurface> makeSurface(const D3D11_TEXTURE2D_DESC& desc) const;
    Result<FgSourceLease> captureSource(ID3D11Texture2D* source) const;
    Result<FgSourceLease> captureSource(const FgD3D11AuxSwapSource& source) const;
    Result<FgCopyTicket> copy(ID3D11DeviceContext* context,
        ID3D11Texture2D* source,const FgSharedSurface& target);
    Result<FgCopyTicket> copy(ID3D11DeviceContext* context,
        const FgSourceLease& source,const FgSharedSurface& target);
    bool producerComplete(std::uint64_t value) const noexcept;
    FgCopyStatus copyStatus(std::uint64_t value) const noexcept;
    bool copyComplete(std::uint64_t value) const noexcept;
    bool waitCopy(std::uint64_t value) const noexcept;
    bool healthy() const noexcept;
    FgInteropLifetime retainLifetime() const noexcept;
private:
    FgSharedInputs()=default;
    Result<FgCopyTicket> copyImpl(ID3D11DeviceContext* context,
        ID3D11Texture2D* source,const FgSharedSurface& target);
    Microsoft::WRL::ComPtr<ID3D11Device5> d11_;
    Microsoft::WRL::ComPtr<ID3D12Device> d12_;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue_;
    Microsoft::WRL::ComPtr<ID3D11Fence> producerFence_;
    Microsoft::WRL::ComPtr<ID3D12Fence> producerGate_;
    Microsoft::WRL::ComPtr<ID3D12Fence> consumerFence_;
    std::mutex mutex_;
    std::uint64_t id_{};
    std::uint64_t nextProducerValue_{},nextCopyValue_{};
    bool failed_{};
};
}
