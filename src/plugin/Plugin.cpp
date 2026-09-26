#include <SKSE/Impl/PCH.h>
#include <SKSE/Interfaces.h>
#include <Windows.h>
#include "rk/Bootstrap.hpp"
#include "rk/Logging.hpp"
#include "rk/Settings.hpp"
#include "rk/RendererBootstrap.hpp"
#include "rk/DiagnosticsMenu.hpp"
#include "rk/MenuInputCapture.hpp"
#include <fstream>
#include <mutex>

// No DllMain: no SDK loading, GPU work or waits under loader lock.
namespace {
rk::Bootstrap host;
std::mutex hostMutex;
std::unique_ptr<rk::Settings> requestedSettings;
bool supportedHost(const SKSE::detail::SKSEInterface* skse) {
    if (!skse || skse->isEditor) return false;
    return skse->runtimeVersion == REL::Version(1, 5, 97, 0).pack() ||
           skse->runtimeVersion == REL::Version(1, 6, 1170, 0).pack();
}
rk::MenuInputProfile menuInputProfile(const SKSE::detail::SKSEInterface* skse) {
    if(skse->runtimeVersion==REL::Version(1,5,97,0).pack())
        return rk::menuInputProfile(rk::MenuInputRuntime::SkyrimSe1597);
    if(skse->runtimeVersion==REL::Version(1,6,1170,0).pack())
        return rk::menuInputProfile(rk::MenuInputRuntime::SkyrimAe161170);
    return {};
}
void onMessage(SKSE::MessagingInterface::Message* message) {
    if (!message) return;
    // T05 must prove an early renderer boundary. PostLoad is not that proof.
    if (message->type == SKSE::MessagingInterface::kPostLoad)
        spdlog::info("SKSE PostLoad: renderer observation {}; SR/FG/NR inactive",rk::rendererObserverArmed() ? "armed" : "disabled");
}
void rendererObserved(const rk::RendererSnapshot&) {
    std::scoped_lock lock(hostMutex);
    if (host.attachRenderer(true)) spdlog::info("RendererAttached: device identity captured; observation only, no frame processing");
}
rk::Settings loadSettings(std::filesystem::path& path) {
    HMODULE self = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(&onMessage), &self)) throw std::runtime_error("Cannot resolve plugin module");
    wchar_t filename[32768];
    const auto count = GetModuleFileNameW(self, filename, 32768);
    if (!count || count >= 32768) throw std::runtime_error("Cannot resolve plugin path");
    path = std::filesystem::path(filename).parent_path() / "RazKolbas.ini";
    if (!std::filesystem::exists(path)) return rk::defaultSettings();
    std::ifstream input(path);
    if (!input) throw std::runtime_error("Cannot read RazKolbas.ini");
    const std::string text((std::istreambuf_iterator<char>(input)), {});
    auto parsed = rk::parseIni(text);
    if (const auto error = std::get_if<rk::Error>(&parsed)) throw std::runtime_error(error->message);
    return std::get<rk::Settings>(std::move(parsed));
}
}

extern "C" __declspec(dllexport) constinit SKSE::PluginVersionData SKSEPlugin_Version = [] {
    SKSE::PluginVersionData metadata;
    metadata.PluginVersion({0, 1, 87, 0});
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
        std::filesystem::path iniPath;
        auto settings = loadSettings(iniPath);
        const auto inputProfile=menuInputProfile(skse);
        rk::configureDiagnosticsMenu(settings.get<bool>("Interface.Enabled"),
            settings.get<rk::Text>("Interface.ToggleMenuKey").value,
            settings.get<double>("Interface.FontScale"),settings,iniPath,
            inputProfile.controlMapSingletonRva,
            inputProfile.ignoreKeyboardMouseOffset);
        spdlog::info("Diagnostics input profile: ControlMap RVA=0x{:x}, ignoreKeyboardMouse=+0x{:x}",
            inputProfile.controlMapSingletonRva,inputProfile.ignoreKeyboardMouseOffset);
        const auto messaging = static_cast<SKSE::detail::SKSEMessagingInterface*>(skse->QueryInterface(SKSE::LoadInterface::kMessaging));
        if (!messaging || !messaging->RegisterListener || messaging->interfaceVersion < SKSE::MessagingInterface::kVersion) {
            spdlog::error("SKSE messaging interface unavailable; initialization aborted");
            return false;
        }
        const bool started = host.start(settings.get<bool>("General.SafeMode") || !settings.get<bool>("General.Enabled"), [&] {
            return messaging->RegisterListener(skse->GetPluginHandle(), "SKSE", reinterpret_cast<void*>(&onMessage));
        });
        if (started) requestedSettings = std::make_unique<rk::Settings>(std::move(settings));
        spdlog::info("RazKolbas native host bootstrap {}; experimental SDR DLAA and same-frame stage pair", started ? "ready" : "failed");
        if (started) {
            const auto observer=rk::installRendererObserver(*requestedSettings,&rendererObserved);
            if (const auto error=std::get_if<rk::Error>(&observer))
                spdlog::warn("Renderer observation not installed: {}",error->message);
        }
        return started;
    } catch (const std::exception& error) {
        OutputDebugStringA(error.what());
        return false;
    } catch (...) {
        OutputDebugStringA("RazKolbas bootstrap failed");
        return false;
    }
}
