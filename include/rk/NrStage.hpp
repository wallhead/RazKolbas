#pragma once
#include "rk/Result.hpp"
#include "rk/Settings.hpp"
#include "rk/SrInput.hpp"
#include <d3d11.h>
#include <memory>
#include <string>

namespace rk {
enum class NrRuntimePhase { Off, Selected, Loaded, Initialized,
    FeatureCreated, EvaluationSubmitted, RetainedAfterFailure };
struct NrRuntimeStatus {
    std::string requestedProfile{"Auto"},effectiveProfile,path,sha256,reason;
    NrRuntimePhase phase{NrRuntimePhase::Off};
    std::uint32_t vendorId{},deviceId{},subsystemId{},luidLow{};
    std::int32_t luidHigh{};
};
// Exact-runtime experimental DLSS Neural Rendering stage. A successful call
// replaces PreparedSrInputs::color in place before DLSS SR consumes it. Every
// failure leaves the original prepared colour untouched.
class NrStage final {
public:
    NrStage();
    ~NrStage();
    NrStage(const NrStage&)=delete;
    NrStage& operator=(const NrStage&)=delete;
    Result<bool> configure(const Settings& settings);
    // Applies evaluation-only controls to an active or not-yet-started stage.
    // Feature-creation controls remain restart-bound. A changed live setting
    // resets temporal NR history on the next submitted frame.
    Result<bool> updateRuntime(const Settings& settings);
    Result<bool> process(ID3D11Device* device,ID3D11DeviceContext* context,
        PreparedSrInputs& frame,bool reset);
    Result<bool> stop();
    bool enabled() const noexcept;
    std::uint64_t submittedFrames() const noexcept;
    std::uint64_t saturatedFrames() const noexcept;
    std::uint64_t cpuFenceWaitCalls() const noexcept;
    NrRuntimeStatus runtimeStatus() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
