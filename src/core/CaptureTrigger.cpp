#include "rk/CaptureTrigger.hpp"
namespace rk {
CaptureSignal CaptureTrigger::poll(std::uint64_t now,bool foreground,bool down) noexcept {
    const bool resumed=polled_&&now-previousPoll_>250;
    polled_=true;previousPoll_=now;
    if(resumed) {
        const bool interrupted=pending_;
        ready_=false;pending_=false;
        if(interrupted)return CaptureSignal::Interrupted;
    }
    if(!foreground) { ready_=false;pending_=false;return CaptureSignal::None; }
    if(!down)ready_=true;
    else if(ready_) {
        ready_=false;
        if(!pending_&&(!requests_||now-requestedAt_>=5000)) {
            if(requests_>=6)return CaptureSignal::LimitReached;
            ++requests_;attempts_=0;requestedAt_=now;pending_=true;
        }
    }
    if(!pending_)return CaptureSignal::None;
    if(now-requestedAt_>=2000||attempts_>=32) { pending_=false;return CaptureSignal::Expired; }
    if(attempts_&&now-previousAttempt_<50)return CaptureSignal::None;
    previousAttempt_=now;++attempts_;return CaptureSignal::Attempt;
}
void CaptureTrigger::consume() noexcept { pending_=false; }
}
