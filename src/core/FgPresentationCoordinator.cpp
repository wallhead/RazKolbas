#include "rk/FgPresentationCoordinator.hpp"

namespace rk {
FgPresentationCoordinator::FgPresentationCoordinator(
    FgProviderSession session,IFgPresentBackend& backend) noexcept :
    session_(session),backend_(backend) {}

Result<FgPresentOutcome> FgPresentationCoordinator::present(
    const FgSourceFrame& frame,bool requestedEnabled) {
    const auto capability=backend_.capability();
    if(session_.boundProvider()!=capability.provider)
        return Error{ErrorCode::Conflict,"FG backend does not match the startup-bound provider"};
    if(!ledger_.acceptReal(frame))
        return Error{ErrorCode::Conflict,"FG source frame was already presented or is stale"};
    auto decision=session_.decide(frame,capability,requestedEnabled);
    const bool wanted=decision.effective!=FgProvider::Off;
    if(wanted!=enabled_) {
        const auto changed=backend_.setMode(wanted);
        const bool accepted=std::holds_alternative<bool>(changed)&&
            std::get<bool>(changed);
        if(!accepted) {
            if(!wanted)return Error{ErrorCode::Unavailable,
                "FG backend could not turn Off and drain before Present"};
            const auto drained=backend_.setMode(false);
            if(!std::holds_alternative<bool>(drained)||!std::get<bool>(drained))
                return Error{ErrorCode::Unavailable,
                    "FG backend rejected On and could not prove Off/drain"};
            decision.effective=FgProvider::Off;
            decision.extraFrames=0;
            decision.reason=FgReason::ProviderUnavailable;
            enabled_=false;
        } else enabled_=wanted;
    }
    const auto presented=backend_.presentReal(frame,enabled_);
    if(const auto error=std::get_if<Error>(&presented))return *error;
    const auto& result=std::get<FgBackendPresent>(presented);
    if(result.resultCode<0)
        return Error{ErrorCode::Unavailable,"FG lower real Present failed"};
    return FgPresentOutcome{decision,result.resultCode,
        result.actualGeneratedFrames};
}
}
