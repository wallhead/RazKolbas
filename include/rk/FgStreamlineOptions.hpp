#pragma once
#include "rk/FgSubmission.hpp"
#include <sl_dlss_g.h>

namespace rk {
// Options for one-extra-frame DLSS-G on a verified lower D3D12 swap.
// Capability, focus, marker timing and Off/drain are separate provider gates.
Result<sl::DLSSGOptions> makeFgStreamlineOptionsOn(
    const FgPreparedSubmission& frame,DXGI_FORMAT lowerBackbufferFormat);
}
