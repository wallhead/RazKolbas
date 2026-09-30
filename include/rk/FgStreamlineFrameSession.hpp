#pragma once
#include "rk/FgStreamlineSubmit.hpp"
#include <sl_pcl.h>
#include <functional>
#include <memory>

namespace rk {
struct FgStreamlineFrameCalls {
    std::function<std::uint64_t()> currentThread;
    std::function<sl::Result(sl::FrameToken*&)> newToken;
    std::function<sl::Result(const sl::FrameToken&)> sleep;
    std::function<sl::Result(sl::PCLMarker,const sl::FrameToken&)> marker;
    FgStreamlineCalls inputs;
};
class FgStreamlineRuntime;
// Resolve calls once from the initialized, pinned runtime after device binding.
// The returned functions retain that runtime; they do not enable FG/Reflex.
Result<FgStreamlineFrameCalls> makeFgStreamlineFrameCalls(
    std::shared_ptr<FgStreamlineRuntime> runtime);

// Single-owner-thread per real frame SL token/marker owner. The owner may
// migrate only after a completed real Present. The game must call
// each method at its verified simulation/render/Present phase; this class
// cannot establish an engine hook's timing. Generated frames never enter it.
// The caller configures one Reflex pacing policy and retains the submitted
// input lease through provider completion, including partial SDK failures.
class FgStreamlineFrameSession {
public:
    FgStreamlineFrameSession(FgStreamlineFrameCalls calls,sl::ViewportHandle viewport);
    FgStreamlineFrameSession(const FgStreamlineFrameSession&)=delete;
    FgStreamlineFrameSession& operator=(const FgStreamlineFrameSession&)=delete;
    FgStreamlineFrameSession(FgStreamlineFrameSession&&)=delete;
    FgStreamlineFrameSession& operator=(FgStreamlineFrameSession&&)=delete;
    Result<bool> begin(const FgSourceFrame& frame);
    Result<bool> simulationEnd();
    Result<bool> renderSubmitStart();
    Result<bool> renderSubmitEnd();
    Result<bool> submit(const FgStreamlineFrameInputs& packet);
    // Consumes at most one lower Present attempt. TEST/status queries bypass
    // this real-frame session. Never retry a failed Present or SDK call.
    Result<HRESULT> present(const std::function<HRESULT()>& lowerPresent);
    bool failed() const noexcept { return phase_==Phase::Failed; }
private:
    enum class Phase {Idle,Simulation,SimulationDone,Render,Ready,Submitted,
        Complete,Failed};
    Result<bool> check(Phase expected);
    Result<bool> mark(Phase expected,Phase next,sl::PCLMarker marker);
    Error sdkError(const char* operation,sl::Result result);
    Error exceptionError();
    bool tokenMatches() const;
    FgStreamlineFrameCalls calls_;
    sl::ViewportHandle viewport_;
    FgSourceFrame frame_{};
    sl::FrameToken* token_{};
    std::uint64_t thread_{};
    std::uint32_t sdkFrameIndex_{};
    bool haveSdkFrameIndex_{};
    Phase phase_{Phase::Idle};
};
}
