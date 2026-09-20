#pragma once
#include <Windows.h>
#include <dxgi.h>
#include <cstdint>
#include <optional>
#include <string_view>

namespace rk {
enum class DisplayMode : std::uint8_t { Native, Dlaa, DlssSr };

// A snapshot of what was submitted on the most recent source frame. DLSS SR
// is reserved for the future reduced-render transaction; this build only
// publishes Native and Dlaa.
struct DiagnosticsSnapshot {
    DisplayMode mode{DisplayMode::Native};
    std::uint32_t renderWidth{},renderHeight{};
    std::uint32_t displayWidth{},displayHeight{};
    std::uint64_t worldFrames{},dlssFrames{},skippedFrames{};
    bool dlssDisabled{},skyrimTaaActive{true},engineDrsKnown{},dlaaSuspendedByDrs{};
};

std::optional<DiagnosticsSnapshot> worldDiagnosticsSnapshot(IDXGISwapChain* swap) noexcept;
void configureDiagnosticsMenu(bool enabled,std::string_view key,double fontScale) noexcept;
void drawDiagnosticsMenu(IDXGISwapChain* swap,const DiagnosticsSnapshot& snapshot) noexcept;
}
