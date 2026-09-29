#pragma once
#include "rk/FgSubmission.hpp"
#include <sl.h>
#include <array>
#include <memory>

namespace rk {
// The state snapshot must be taken after the D3D11-to-D3D12 copy and any
// D3D12 transition have completed. A numeric state alone cannot prove GPU
// completion; that fence check belongs to the presentation owner.
struct FgStreamlineReadStates {
    FgCopyTicket completedCopy{};
    std::array<D3D12_RESOURCE_STATES,5> actual{};
};

// Stable-address, retained Streamline tags for one real source frame. The
// bundle keeps its textures alive but does not replace provider-input fence
// retirement after Present.
class FgStreamlineTagBundle {
public:
    FgStreamlineTagBundle(const FgStreamlineTagBundle&)=delete;
    FgStreamlineTagBundle& operator=(const FgStreamlineTagBundle&)=delete;
    static constexpr std::size_t tagCount=5;
    const sl::ResourceTag* tags() const noexcept { return tags_.data(); }
    std::uint64_t presentToken() const noexcept { return presentToken_; }
    ID3D12Resource* sourceColor() const noexcept { return owned_[0].Get(); }
private:
    friend Result<std::unique_ptr<FgStreamlineTagBundle>>
        makeFgStreamlineTags(const FgPreparedSubmission&,
            const FgStreamlineReadStates&);
    FgStreamlineTagBundle(const FgPreparedSubmission&,
        const FgStreamlineReadStates&);
    std::uint64_t presentToken_{};
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>,5> owned_{};
    sl::Extent renderExtent_{};
    sl::Extent displayExtent_{};
    std::array<sl::Resource,4> slResources_;
    std::array<sl::ResourceTag,tagCount> tags_;
};

Result<std::unique_ptr<FgStreamlineTagBundle>> makeFgStreamlineTags(
    const FgPreparedSubmission& submission,
    const FgStreamlineReadStates& states);
}
