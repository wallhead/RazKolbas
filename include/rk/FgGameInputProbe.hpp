#pragma once
#include "rk/FgGameInputCandidate.hpp"
#include "rk/FgSubmission.hpp"
#include <memory>
#include <optional>

namespace rk {
enum class FgGameCopyState { Pending, Complete, Failed };

// A bounded sequence of source-only D3D11->D3D12 copies on one companion
// device. No provider, lower swap or generated Present consumes these leases.
// poll() never waits for the GPU; an uncertain copy is quarantined by the
// input ring on destruction.
class FgGameInputProbe {
public:
    static Result<std::unique_ptr<FgGameInputProbe>> begin(
        ID3D11Device* device,ID3D11DeviceContext* context,
        const FgGameInputCandidate& candidate);
    // The caller supplies the verified native presentation device/queue.
    // The caller proves this queue created the lower swap; this method checks
    // queue/device identity only. No replacement device is created. Retain the provider through copies,
    // including process-lifetime quarantine of uncertain work.
    static Result<std::unique_ptr<FgGameInputProbe>> beginOnOwner(
        ID3D11Device* device,ID3D11DeviceContext* context,
        ID3D12Device* nativeDevice,ID3D12CommandQueue* nativeQueue,
        std::shared_ptr<void> providerLifetime,
        const FgGameInputCandidate& candidate);
    Result<FgCopyTicket> enqueue(ID3D11DeviceContext* context,
        const FgGameInputCandidate& candidate);
    // Metadata-only admission for the pending copied lease. This does not
    // prove GPU completion, lower-swap ownership or provider readiness.
    Result<FgPreparedSubmission> inspectPrepared(
        const FgGameInputCandidate& candidate,const FgUiPlaneFrame& ui,
        const FgCameraData& camera,std::uint32_t physicalOutputIndex,
        std::uint32_t swapBufferCount) const;
    FgGameCopyState poll() noexcept;
    bool close() noexcept;
    bool pending() const noexcept { return state_==FgGameCopyState::Pending; }
    std::uint64_t source() const noexcept { return source_; }
    std::uint64_t presentToken() const noexcept { return presentToken_; }
    std::uint64_t generation() const noexcept { return generation_; }
    FgCopyTicket copyTicket() const noexcept { return ticket_; }
private:
    FgGameInputProbe()=default;
    std::shared_ptr<void> providerLifetime_;
    Microsoft::WRL::ComPtr<ID3D12Device> d12_;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue_;
    std::unique_ptr<FgSharedInputs> bridge_;
    std::unique_ptr<FgInputLeaseRing> ring_;
    std::optional<FgInputLease> lease_;
    std::uint64_t source_{},presentToken_{},generation_{};
    FgCopyTicket ticket_{};
    FgGameCopyState state_{FgGameCopyState::Complete};
};
}
