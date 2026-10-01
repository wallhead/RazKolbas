#pragma once
#include "rk/FgGameInputCandidate.hpp"
#include <memory>
#include <optional>

namespace rk {
enum class FgGameCopyState { Pending, Complete, Failed };

// One sampled, source-only D3D11->D3D12 copy. No provider, lower swap or
// generated Present consumes this lease. poll() never waits for the GPU;
// an uncertain copy is quarantined by FgInputLeaseRing on destruction.
class FgGameInputProbe {
public:
    static Result<std::unique_ptr<FgGameInputProbe>> begin(
        ID3D11Device* device,ID3D11DeviceContext* context,
        const FgGameInputCandidate& candidate);
    FgGameCopyState poll() noexcept;
    std::uint64_t source() const noexcept { return source_; }
    std::uint64_t presentToken() const noexcept { return presentToken_; }
    std::uint64_t generation() const noexcept { return generation_; }
    FgCopyTicket copyTicket() const noexcept { return ticket_; }
private:
    FgGameInputProbe()=default;
    Microsoft::WRL::ComPtr<ID3D12Device> d12_;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue_;
    std::unique_ptr<FgSharedInputs> bridge_;
    std::unique_ptr<FgInputLeaseRing> ring_;
    std::optional<FgInputLease> lease_;
    std::uint64_t source_{},presentToken_{},generation_{};
    FgCopyTicket ticket_{};
    FgGameCopyState state_{FgGameCopyState::Pending};
};
}
