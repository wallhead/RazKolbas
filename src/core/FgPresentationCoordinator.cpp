#include "rk/FgPresentationCoordinator.hpp"

namespace rk {
FgPresentationCoordinator::FgPresentationCoordinator(
    FgProviderSession session,IFgPresentBackend& backend) noexcept :
    session_(session),backend_(backend) {}

Result<FgPresentOutcome> FgPresentationCoordinator::present(
    const FgSourceFrame& frame,bool requestedEnabled,const FgPresentCall& call) {
    if((call.method==FgPresentMethod::Present&&call.parameters)||
       (call.method==FgPresentMethod::Present1&&!call.parameters))
        return Error{ErrorCode::InvalidInput,"FG lower Present arguments do not match the DXGI method"};
    const auto capability=backend_.capability();
    if(session_.boundProvider()!=capability.provider)
        return Error{ErrorCode::Conflict,"FG backend does not match the startup-bound provider"};
    const bool presentTest=(call.flags&DXGI_PRESENT_TEST)!=0;
    if(!presentTest&&!ledger_.canAcceptReal(frame))
        return Error{ErrorCode::Conflict,"FG source frame was already presented or is stale"};
    auto decision=session_.decide(frame,capability,
        requestedEnabled&&!presentTest);
    if(presentTest)decision.reason=FgReason::PresentTest;
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
    // An attempted lower Present consumes the token even if DXGI later
    // reports failure: retrying it could submit a real frame twice.
    if(!presentTest&&!ledger_.acceptReal(frame))
        return Error{ErrorCode::Conflict,"FG source frame changed before Present"};
    const auto presented=backend_.presentReal(frame,enabled_,call);
    if(const auto error=std::get_if<Error>(&presented))return *error;
    const auto& result=std::get<FgBackendPresent>(presented);
    if((!enabled_&&result.actualGeneratedFrames)||
       result.actualGeneratedFrames>decision.extraFrames)
        return Error{ErrorCode::Conflict,"FG backend reported an invalid generated-frame count"};
    return FgPresentOutcome{decision,result.resultCode,
        result.actualGeneratedFrames};
}
}
