#include "rk/Bootstrap.hpp"
namespace rk {
bool Bootstrap::start(bool safeMode, const std::function<bool()>& registerListener) {
    if (state_ == HostState::Stopping) return false;
    if (state_ != HostState::Created) return true;
    if (!registerListener()) return false;
    safeMode_ = safeMode;
    state_ = HostState::BootstrapReady;
    return true;
}
bool Bootstrap::attachRenderer(bool verifiedProfile) {
    if (state_ != HostState::BootstrapReady || safeMode_ || !verifiedProfile) return false;
    state_ = HostState::RendererAttached;
    return true;
}
bool Bootstrap::run() {
    if (state_ != HostState::RendererAttached && state_ != HostState::Suspended) return false;
    state_ = HostState::Running;
    return true;
}
bool Bootstrap::suspend() {
    if (state_ != HostState::Running) return false;
    state_ = HostState::Suspended;
    return true;
}
void Bootstrap::stop() { state_ = HostState::Stopping; }
}
