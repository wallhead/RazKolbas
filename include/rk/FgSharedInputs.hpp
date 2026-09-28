#pragma once
#include "rk/Result.hpp"
#include <d3d11_4.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>
#include <memory>
#include <mutex>

namespace rk {
struct FgCopyTicket {
    std::uint64_t producer{},copy{};
};
enum class FgCopyStatus { Pending, Complete, DeviceRemoved, Unknown };
FgCopyStatus classifyFgCopyStatus(std::uint64_t completed,
    std::uint64_t requested,HRESULT deviceHealth) noexcept;

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
    Result<FgCopyTicket> copy(ID3D11DeviceContext* context,
        ID3D11Texture2D* source,const FgSharedSurface& target);
    bool producerComplete(std::uint64_t value) const noexcept;
    FgCopyStatus copyStatus(std::uint64_t value) const noexcept;
    bool copyComplete(std::uint64_t value) const noexcept;
    bool waitCopy(std::uint64_t value) const noexcept;
    bool healthy() const noexcept;
private:
    FgSharedInputs()=default;
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
