#include "rk/FgStreamlineFrameSession.hpp"
#include "rk/FgStreamlineRuntime.hpp"
#include <sl_reflex.h>
#include <utility>

namespace rk {
Result<FgStreamlineFrameCalls> makeFgStreamlineFrameCalls(
    std::shared_ptr<FgStreamlineRuntime> runtime) {
    if(!runtime||!runtime->initialized())return Error{ErrorCode::Unavailable,
        "FG frame calls require an initialized pinned Streamline runtime"};
    void* markerFunction{};void* sleepFunction{};
    if(runtime->getFeatureFunction(sl::kFeaturePCL,"slPCLSetMarker",markerFunction)!=
           sl::Result::eOk||!markerFunction||
       runtime->getFeatureFunction(sl::kFeatureReflex,"slReflexSleep",sleepFunction)!=
           sl::Result::eOk||!sleepFunction)
        return Error{ErrorCode::Unavailable,"Pinned FG marker/sleep functions are unavailable"};
    const auto marker=reinterpret_cast<PFun_slPCLSetMarker*>(markerFunction);
    const auto sleep=reinterpret_cast<PFun_slReflexSleep*>(sleepFunction);
    FgStreamlineFrameCalls calls{};
    calls.currentThread=[]{return GetCurrentThreadId();};
    calls.newToken=[runtime](sl::FrameToken*& token){return runtime->newFrameToken(token);};
    calls.sleep=[runtime,sleep](const sl::FrameToken& token) {
        return runtime->initialized()?sleep(token):sl::Result::eErrorInvalidState;
    };
    calls.marker=[runtime,marker](sl::PCLMarker phase,const sl::FrameToken& token) {
        return runtime->initialized()?marker(phase,token):sl::Result::eErrorInvalidState;
    };
    calls.inputs.setConstants=[runtime](const sl::Constants& constants,
        const sl::FrameToken& token,const sl::ViewportHandle& viewport) {
        return runtime->setConstants(constants,token,viewport);
    };
    calls.inputs.setTags=[runtime](const sl::FrameToken& token,
        const sl::ViewportHandle& viewport,const sl::ResourceTag* tags,
        std::uint32_t count,sl::CommandBuffer* commands) {
        return runtime->setTags(token,viewport,tags,count,commands);
    };
    return calls;
}
FgStreamlineFrameSession::FgStreamlineFrameSession(FgStreamlineFrameCalls calls,
    sl::ViewportHandle viewport):calls_(std::move(calls)),viewport_(viewport) {}

Error FgStreamlineFrameSession::sdkError(const char* operation,sl::Result result) {
    phase_=Phase::Failed;
    return {ErrorCode::Unavailable,std::string("FG frame session rejected ")+operation+
        ": "+std::to_string(static_cast<int>(result))};
}
Error FgStreamlineFrameSession::exceptionError() {
    phase_=Phase::Failed;
    return {ErrorCode::Unavailable,"FG frame session callback threw; session invalidated"};
}
bool FgStreamlineFrameSession::tokenMatches() const {
    return token_&&haveSdkFrameIndex_&&
        static_cast<std::uint32_t>(*token_)==sdkFrameIndex_;
}
Result<bool> FgStreamlineFrameSession::check(Phase expected) {
    if(phase_!=expected)return Error{ErrorCode::Conflict,
        "FG frame session phase differs or session is invalidated"};
    try {
        if(!calls_.currentThread||calls_.currentThread()!=thread_)
            return Error{ErrorCode::Conflict,"FG frame session changed its owner thread"};
        if(!tokenMatches())return sdkError("changed SDK frame index",sl::Result::eErrorInvalidState);
    }catch(...) {return exceptionError();}
    return true;
}
Result<bool> FgStreamlineFrameSession::begin(const FgSourceFrame& frame) {
    if(phase_!=Phase::Idle&&phase_!=Phase::Complete)
        return Error{ErrorCode::Conflict,"FG frame session is active or invalidated"};
    if(!frame.source||!frame.generation||!frame.presentToken||!frame.resetEpoch||
       !frame.ownerReady||!frame.render.valid()||!frame.display.valid()||
       frame.source<=frame_.source||frame.presentToken<=frame_.presentToken||
       frame.generation<frame_.generation||frame.resetEpoch<frame_.resetEpoch)
        return Error{ErrorCode::Conflict,"FG frame session identity is invalid or stale"};
    if(!calls_.currentThread||!calls_.newToken||!calls_.sleep||!calls_.marker||
       !calls_.inputs.setConstants||!calls_.inputs.setTags)
        return Error{ErrorCode::Unavailable,"FG frame session SDK calls are incomplete"};
    try {
        const auto thread=calls_.currentThread();
        // Present may move to another Skyrim thread between real frames.
        // `check` still requires every phase of this frame on its new owner.
        if(!thread)return Error{ErrorCode::Conflict,
            "FG frame session has no owner thread"};
        thread_=thread;frame_=frame;token_=nullptr;
        // Consume the identity before any SDK side effect. Exceptions and
        // partial marker/token operations cannot be retried on this owner.
        phase_=Phase::Failed;
        auto result=calls_.newToken(token_);
        if(result!=sl::Result::eOk||!token_)return sdkError("new frame token",result);
        const auto index=static_cast<std::uint32_t>(*token_);
        if(haveSdkFrameIndex_&&index<=sdkFrameIndex_)
            return sdkError("recycled SDK frame index",sl::Result::eErrorInvalidState);
        sdkFrameIndex_=index;haveSdkFrameIndex_=true;
        result=calls_.sleep(*token_);
        if(result!=sl::Result::eOk)return sdkError("Reflex sleep",result);
        if(!tokenMatches())return sdkError("changed SDK frame index",sl::Result::eErrorInvalidState);
        result=calls_.marker(sl::PCLMarker::eSimulationStart,*token_);
        if(result!=sl::Result::eOk)return sdkError("simulation start",result);
        if(!tokenMatches())return sdkError("changed SDK frame index",sl::Result::eErrorInvalidState);
        phase_=Phase::Simulation;return true;
    }catch(...) {return exceptionError();}
}
Result<bool> FgStreamlineFrameSession::mark(Phase expected,Phase next,
    sl::PCLMarker marker) {
    const auto valid=check(expected);
    if(const auto error=std::get_if<Error>(&valid))return *error;
    try {
        phase_=Phase::Failed;
        const auto result=calls_.marker(marker,*token_);
        if(result!=sl::Result::eOk)return sdkError("phase marker",result);
        if(!tokenMatches())return sdkError("changed SDK frame index",sl::Result::eErrorInvalidState);
        phase_=next;return true;
    }catch(...) {return exceptionError();}
}
Result<bool> FgStreamlineFrameSession::simulationEnd() {
    return mark(Phase::Simulation,Phase::SimulationDone,sl::PCLMarker::eSimulationEnd);
}
Result<bool> FgStreamlineFrameSession::renderSubmitStart() {
    return mark(Phase::SimulationDone,Phase::Render,sl::PCLMarker::eRenderSubmitStart);
}
Result<bool> FgStreamlineFrameSession::renderSubmitEnd() {
    return mark(Phase::Render,Phase::Ready,sl::PCLMarker::eRenderSubmitEnd);
}
Result<bool> FgStreamlineFrameSession::submit(const FgStreamlineFrameInputs& packet) {
    const auto valid=check(Phase::Ready);
    if(const auto error=std::get_if<Error>(&valid))return *error;
    if(packet.source!=frame_.source||packet.generation!=frame_.generation||
       packet.presentToken!=frame_.presentToken||packet.resetEpoch!=frame_.resetEpoch||
       !packet.tags||packet.tags->presentToken()!=frame_.presentToken)
        return Error{ErrorCode::Conflict,"FG input packet differs from the owned real-frame token"};
    try {
        phase_=Phase::Failed;
        const FgStreamlineTokenBinding binding{frame_.source,frame_.generation,
            frame_.presentToken,frame_.resetEpoch,token_};
        // An SDK token may reuse its storage. Check its numeric frame index
        // both before and after each call, including between constants/tags.
        FgStreamlineCalls guarded{};
        guarded.setConstants=[&](const sl::Constants& values,const sl::FrameToken& token,
            const sl::ViewportHandle& viewport) {
            if(!tokenMatches())return sl::Result::eErrorInvalidState;
            const auto result=calls_.inputs.setConstants(values,token,viewport);
            return tokenMatches()?result:sl::Result::eErrorInvalidState;
        };
        guarded.setTags=[&](const sl::FrameToken& token,const sl::ViewportHandle& viewport,
            const sl::ResourceTag* tags,std::uint32_t count,sl::CommandBuffer* commands) {
            if(!tokenMatches())return sl::Result::eErrorInvalidState;
            const auto result=calls_.inputs.setTags(token,viewport,tags,count,commands);
            return tokenMatches()?result:sl::Result::eErrorInvalidState;
        };
        const auto result=submitFgStreamlineInputs(packet,binding,viewport_,guarded);
        if(const auto error=std::get_if<Error>(&result))return *error;
        phase_=Phase::Submitted;return true;
    }catch(...) {return exceptionError();}
}
Result<HRESULT> FgStreamlineFrameSession::present(
    const std::function<HRESULT()>& lowerPresent) {
    const auto valid=check(Phase::Submitted);
    if(const auto error=std::get_if<Error>(&valid))return *error;
    if(!lowerPresent)return Error{ErrorCode::Unavailable,"FG lower Present callback is absent"};
    HRESULT presented{};bool attempted=false;
    try {
        phase_=Phase::Failed;
        const auto start=calls_.marker(sl::PCLMarker::ePresentStart,*token_);
        if(start!=sl::Result::eOk)return sdkError("Present start",start);
        if(!tokenMatches())return sdkError("changed SDK frame index",sl::Result::eErrorInvalidState);
        try {presented=lowerPresent();}
        catch(...) {
            try {
                if(tokenMatches())calls_.marker(sl::PCLMarker::ePresentEnd,*token_);
            }catch(...) {}
            return exceptionError();
        }
        attempted=true;
        if(!tokenMatches()) {
            if(FAILED(presented))return presented;
            return sdkError("changed SDK frame index",sl::Result::eErrorInvalidState);
        }
        const auto end=calls_.marker(sl::PCLMarker::ePresentEnd,*token_);
        // Preserve the primary DXGI failure even if the end marker also fails.
        if(FAILED(presented))return presented;
        if(end!=sl::Result::eOk)return sdkError("Present end",end);
        if(!tokenMatches())return sdkError("changed SDK frame index",sl::Result::eErrorInvalidState);
        phase_=Phase::Complete;return presented;
    }catch(...) {
        if(attempted&&FAILED(presented)){phase_=Phase::Failed;return presented;}
        return exceptionError();
    }
}
}
