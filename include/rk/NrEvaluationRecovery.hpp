#pragma once
#include <cstdint>
#include <utility>

namespace rk {
enum class NrEvaluationAction { Retry, Disable, DeviceLost };

// Evaluation failures are retried only after the D3D11 queue has accepted and
// flushed a wait for the failed D3D12 submission.
class NrEvaluationRecovery final {
public:
    template<class Handoff,class Flush>
    NrEvaluationAction failed(Handoff&& handoff,Flush&& flush) {
        if(!std::forward<Handoff>(handoff)())return NrEvaluationAction::DeviceLost;
        std::forward<Flush>(flush)();
        return ++consecutiveFailures_>=3?
            NrEvaluationAction::Disable:NrEvaluationAction::Retry;
    }
    void succeeded() noexcept { consecutiveFailures_=0; }
    unsigned consecutiveFailures() const noexcept { return consecutiveFailures_; }
private:
    unsigned consecutiveFailures_{};
};

// A failed evaluation can submit an output fence after the preceding frame's
// copyback fence. Retiring that older copyback must not retire the new output.
class NrPendingFences final {
public:
    void markOutput(std::uint64_t value) noexcept { output_=value; }
    void markConsumer(std::uint64_t value) noexcept { consumer_=value; }
    std::uint64_t output() const noexcept { return output_; }
    std::uint64_t consumer() const noexcept { return consumer_; }
    bool retireConsumer() noexcept {
        const bool coversOutput=output_!=0&&output_<=consumer_;
        if(coversOutput)output_=0;
        consumer_=0;
        return coversOutput;
    }
    void retireOutput() noexcept { output_=0; }
private:
    std::uint64_t output_{},consumer_{};
};
}
