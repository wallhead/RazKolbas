#include "rk/FgNativePresentBackend.hpp"

namespace rk {
Result<FgBackendPresent> FgNativePresentBackend::presentReal(
    const FgSourceFrame&,bool enabled,const FgPresentCall& call) {
    if(enabled)return Error{ErrorCode::Conflict,
        "Native lower Present cannot generate frames"};
    lastResult_=lower_.present(call);
    if(!(call.flags&DXGI_PRESENT_TEST))++realCalls_;
    return FgBackendPresent{lastResult_,0};
}
}
