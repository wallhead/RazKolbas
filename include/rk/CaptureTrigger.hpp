#pragma once
#include <cstdint>
namespace rk {
enum class CaptureSignal { None, Attempt, Expired, Interrupted, LimitReached };
// Polled at the eligible Present boundary under the probe mutex. Each request
// consumes one of six session slots, including failed/expired requests.
class CaptureTrigger {
public:
    CaptureSignal poll(std::uint64_t now,bool foreground,bool down) noexcept;
    void consume() noexcept;
    unsigned requests() const noexcept { return requests_; }
    unsigned attempts() const noexcept { return attempts_; }
private:
    bool ready_=false,pending_=false;
    bool polled_=false;
    unsigned requests_=0,attempts_=0;
    std::uint64_t requestedAt_=0,previousAttempt_=0;
    std::uint64_t previousPoll_=0;
};
}
