#pragma once
#include "rk/FgStreamlineConstants.hpp"
#include "rk/FgStreamlineOptions.hpp"
#include "rk/FgStreamlineTags.hpp"

namespace rk {
// One coherent source-frame packet for a future DLSS-G Present owner.
// This has no SL token or provider submission side effect. The owner must
// prove the GPU copy/transition fence before use and retain tags through
// SL's reported input-processing completion.
struct FgStreamlineFrameInputs {
    std::uint64_t source{},generation{},presentToken{},resetEpoch{};
    sl::Constants constants{};
    sl::DLSSGOptions options{};
    std::unique_ptr<FgStreamlineTagBundle> tags;
};
Result<FgStreamlineFrameInputs> prepareFgStreamlineFrameInputs(
    const FgSourceFrame& frame,const FgPreparedSubmission& prepared,
    const FgStreamlineReadStates& states,
    DXGI_FORMAT lowerBackbufferFormat);
}
