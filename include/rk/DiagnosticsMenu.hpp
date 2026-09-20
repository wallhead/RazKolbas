#pragma once
#include <Windows.h>
#include <dxgi.h>
#include <cstdint>
#include <optional>
#include <string_view>

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
};

std::optional<DiagnosticsSnapshot> worldDiagnosticsSnapshot(IDXGISwapChain* swap) noexcept;
void configureDiagnosticsMenu(bool enabled,std::string_view key,double fontScale) noexcept;
void drawDiagnosticsMenu(IDXGISwapChain* swap,const DiagnosticsSnapshot& snapshot) noexcept;
}
