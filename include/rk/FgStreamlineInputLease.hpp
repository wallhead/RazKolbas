#pragma once
#include "rk/FgStreamlineFrameInputs.hpp"
#include <wrl/client.h>

namespace rk {
// Owns one submitted packet until the completion fence returned by the
// same-frame, present-thread DLSS-G state query has actually passed.
// Unknown retirement is quarantined for process lifetime, not guessed.
class FgStreamlineInputLease {
public:
    explicit FgStreamlineInputLease(FgStreamlineFrameInputs&& inputs) noexcept;
    ~FgStreamlineInputLease() noexcept;
    FgStreamlineInputLease(const FgStreamlineInputLease&)=delete;
    FgStreamlineInputLease& operator=(const FgStreamlineInputLease&)=delete;
    FgStreamlineInputLease(FgStreamlineInputLease&&) noexcept=default;
    FgStreamlineInputLease& operator=(FgStreamlineInputLease&&)=delete;
    const FgStreamlineFrameInputs& inputs() const noexcept { return inputs_; }
    Result<bool> observeCompletion(const sl::DLSSGState& state) noexcept;
    ID3D12Fence* completionFence() const noexcept { return fence_.Get(); }
    std::uint64_t completionValue() const noexcept { return value_; }
    bool retired() const noexcept;
    bool releaseIfRetired() noexcept;
    static std::size_t quarantinedCount() noexcept;
private:
    FgStreamlineFrameInputs inputs_;
    Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
    std::uint64_t value_{};
};
}
