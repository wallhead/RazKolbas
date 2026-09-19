#include <SKSE/Impl/PCH.h>
#include <SKSE/Interfaces.h>
#include <Windows.h>
#include "rk/Bootstrap.hpp"
#include "rk/Logging.hpp"
#include <mutex>

// No DllMain: no SDK loading, GPU work or waits under loader lock.
namespace {
rk::Bootstrap host;
std::mutex hostMutex;
bool supportedHost(const SKSE::detail::SKSEInterface* skse) {
    if (!skse || skse->isEditor) return false;
    return skse->runtimeVersion == REL::Version(1, 5, 97, 0).pack() ||
           skse->runtimeVersion == REL::Version(1, 6, 1170, 0).pack();
}
void onMessage(SKSE::MessagingInterface::Message* message) {
    if (!message) return;
    // T05 must prove an early renderer boundary. PostLoad is not that proof.
    if (message->type == SKSE::MessagingInterface::kPostLoad)
        spdlog::info("Native mode: no verified rendering hook profile attached; SR/FG/NR inactive");
}
}

extern "C" __declspec(dllexport) constinit SKSE::PluginVersionData SKSEPlugin_Version = [] {
    SKSE::PluginVersionData metadata;
    metadata.PluginVersion({0, 1, 0, 0});
    metadata.PluginName("RazKolbas");
    metadata.AuthorName("RazKolbas contributors");
    metadata.UsesNoStructs();
    metadata.CompatibleVersions({{1, 5, 97, 0}, {1, 6, 1170, 0}});
    return metadata;
}();

extern "C" __declspec(dllexport) bool SKSEPlugin_Query(
    const SKSE::detail::SKSEInterface* skse, SKSE::detail::PluginInfo* info) noexcept {
    if (!info) return false;
    info->infoVersion = 1;
    info->name = "RazKolbas";
    info->version = 1;
    return supportedHost(skse);
}

extern "C" __declspec(dllexport) bool SKSEPlugin_Load(const SKSE::detail::SKSEInterface* skse) noexcept {
    if (!supportedHost(skse) || !skse->QueryInterface || !skse->GetPluginHandle) return false;
    try {
        std::scoped_lock lock(hostMutex);
        if (host.state() != rk::HostState::Created) return host.state() != rk::HostState::Stopping;
        if (!rk::initializeLogging()) return false;
        const auto messaging = static_cast<SKSE::detail::SKSEMessagingInterface*>(skse->QueryInterface(SKSE::LoadInterface::kMessaging));
        if (!messaging || !messaging->RegisterListener || messaging->interfaceVersion < SKSE::MessagingInterface::kVersion) {
            spdlog::error("SKSE messaging interface unavailable; initialization aborted");
            return false;
        }
        const bool started = host.start(true, [&] {
            return messaging->RegisterListener(skse->GetPluginHandle(), "SKSE", reinterpret_cast<void*>(&onMessage));
        });
        spdlog::info("RazKolbas native host bootstrap {}; no rendering profile or vendor provider initialized", started ? "ready" : "failed");
        return started;
    } catch (const std::exception& error) {
        OutputDebugStringA(error.what());
        return false;
    } catch (...) {
        OutputDebugStringA("RazKolbas bootstrap failed");
        return false;
    }
}
