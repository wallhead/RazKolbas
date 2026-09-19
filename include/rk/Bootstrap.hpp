#pragma once
#include <functional>

namespace rk {
enum class HostState { Created, BootstrapReady, RendererAttached, Running, Suspended, Stopping };
class Bootstrap {
public:
    bool start(bool safeMode, const std::function<bool()>& registerListener);
    bool attachRenderer(bool verifiedProfile);
    bool run();
    bool suspend();
    void stop();
    [[nodiscard]] HostState state() const { return state_; }
private:
    HostState state_{HostState::Created};
    bool safeMode_{};
};
}
