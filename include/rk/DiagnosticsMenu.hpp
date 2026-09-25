#pragma once
#include <Windows.h>
#include <dxgi.h>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include "rk/Settings.hpp"

namespace rk {
enum class DisplayMode : std::uint8_t { Native, Dlaa, DlssSr, SpatialFallback };

// A snapshot of what was submitted on the most recent source frame.
struct DiagnosticsSnapshot {
    DisplayMode mode{DisplayMode::Native};
    std::uint32_t renderWidth{},renderHeight{};
    std::uint32_t displayWidth{},displayHeight{};
    std::uint64_t worldFrames{},dlssFrames{},skippedFrames{};
    bool dlssDisabled{},skyrimTaaActive{true},engineDrsKnown{},dlaaSuspendedByDrs{};
    bool srRequested{},srSourceReady{},ownedSceneActive{};
    float postSharpness{};
};

struct SharpeningUpdate {
    bool enabled{};
    float sharpness{};
};

std::optional<DiagnosticsSnapshot> worldDiagnosticsSnapshot(IDXGISwapChain* swap) noexcept;
void configureDiagnosticsMenu(bool enabled,std::string_view key,double fontScale) noexcept;
void configureDiagnosticsMenu(bool enabled,std::string_view key,double fontScale,
    const Settings& settings,const std::filesystem::path& iniPath,
    std::uintptr_t controlMapSingletonRva,
    std::uintptr_t ignoreKeyboardMouseOffset) noexcept;
std::optional<SharpeningUpdate> consumeDiagnosticsSharpeningUpdate() noexcept;
// Returns the most recent menu snapshot containing live NR evaluation controls.
// Creation-time fields in the snapshot remain restart-bound.
std::optional<Settings> consumeDiagnosticsNrRuntimeUpdate();
bool diagnosticsMenuCapturingInput() noexcept;
void drawDiagnosticsMenu(IDXGISwapChain* swap,const DiagnosticsSnapshot& snapshot) noexcept;
}
