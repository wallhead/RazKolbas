#pragma once
#include "rk/FgStreamlineFrameInputs.hpp"
#include <functional>

namespace rk {
struct FgStreamlineTokenBinding {
    std::uint64_t source{},generation{},presentToken{},resetEpoch{};
    const sl::FrameToken* token{};
};
struct FgStreamlineCalls {
    std::function<sl::Result(const sl::Constants&,
        const sl::FrameToken&,const sl::ViewportHandle&)> setConstants;
    std::function<sl::Result(const sl::FrameToken&,
        const sl::ViewportHandle&,const sl::ResourceTag*,
        std::uint32_t,sl::CommandBuffer*)> setTags;
};

// Calls the pinned SL API for a prevalidated packet, but does not Present.
// The caller retains packet/tags and the five resources through SL's reported
// input-processing completion, including when a call returns an error.
Result<bool> submitFgStreamlineInputs(
    const FgStreamlineFrameInputs& packet,
    const FgStreamlineTokenBinding& binding,
    const sl::ViewportHandle& viewport,const FgStreamlineCalls& calls);
}
