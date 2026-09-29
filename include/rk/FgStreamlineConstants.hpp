#pragma once
#include "rk/FgSubmission.hpp"
#include <sl.h>

namespace rk {
// Pinned Streamline 2.14.1 common-constants adapter. The separate admission
// check in prepareFgSubmission must validate resources and copy completion.
Result<sl::Constants> makeFgStreamlineConstants(
    const FgSourceFrame& frame,const FgCameraData& camera);
}
