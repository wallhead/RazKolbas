#include "rk/FgStreamlineInputLease.hpp"
#include <exception>
#include <mutex>
#include <vector>

namespace rk {
namespace {
std::mutex& quarantineMutex() {
    static auto* mutex=new std::mutex;
    return *mutex;
}
std::vector<FgStreamlineFrameInputs>& quarantine() {
    static auto* frames=new std::vector<FgStreamlineFrameInputs>;
    return *frames;
}
}
FgStreamlineInputLease::FgStreamlineInputLease(
    FgStreamlineFrameInputs&& inputs) noexcept : inputs_(std::move(inputs)) {}

FgStreamlineInputLease::~FgStreamlineInputLease() noexcept {
    if(!inputs_.tags||retired())return;
    try {
        std::scoped_lock lock(quarantineMutex());
        quarantine().push_back(std::move(inputs_));
    } catch(...) {
        std::terminate();
    }
}

Result<bool> FgStreamlineInputLease::observeCompletion(
    const sl::DLSSGState& state) noexcept {
    if(!inputs_.tags||fence_)
        return Error{ErrorCode::Conflict,
            "FG input lease is absent or already has a completion fence"};
    if(!state.inputsProcessingCompletionFence||
       !state.lastPresentInputsProcessingCompletionFenceValue||
       state.lastPresentInputsProcessingCompletionFenceValue==UINT64_MAX)
        return Error{ErrorCode::Unavailable,
            "FG provider did not report a usable input-completion fence"};
    fence_=reinterpret_cast<ID3D12Fence*>(
        state.inputsProcessingCompletionFence);
    value_=state.lastPresentInputsProcessingCompletionFenceValue;
    return true;
}

bool FgStreamlineInputLease::retired() const noexcept {
    if(!inputs_.tags)return true;
    if(!fence_||!value_)return false;
    const auto completed=fence_->GetCompletedValue();
    return completed!=UINT64_MAX&&completed>=value_;
}

bool FgStreamlineInputLease::releaseIfRetired() noexcept {
    if(!inputs_.tags||!retired())return false;
    inputs_={};
    fence_.Reset();value_=0;
    return true;
}

std::size_t FgStreamlineInputLease::quarantinedCount() noexcept {
    std::scoped_lock lock(quarantineMutex());
    return quarantine().size();
}
}
