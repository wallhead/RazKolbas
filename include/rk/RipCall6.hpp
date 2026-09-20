#pragma once
#include "rk/Result.hpp"
#include "rk/CallSite.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace rk {
// x64 FF 15 disp32 calls through a pointer cell, unlike a five-byte E8 call.
struct RipCall6Descriptor {
    std::string id;
    std::string gameSha256;
    std::size_t imageSize{};
    std::uint32_t siteRva{}, originalCellRva{};
    std::array<std::uint8_t,6> expected{};
};
struct RipCall6Plan {
    std::string id;
    std::uint32_t siteRva{}, originalCellRva{};
    std::array<std::uint8_t,6> original{};
};
Result<std::uintptr_t> decodeRipCall6(std::uintptr_t site,
    std::span<const std::uint8_t> instruction);
Result<std::array<std::uint8_t,6>> encodeRipCall6(std::uintptr_t site,
    std::uintptr_t cell);
Result<RipCall6Plan> prepareRipCall6(std::span<const std::uint8_t> live,
    std::string_view verifiedGameHash,std::size_t imageSize,
    const RipCall6Descriptor& descriptor);
// The exact local 1.6.1170 Renderer::Begin call passes HWND in RCX and
// &stack RECT in RDX. This validates the local decoded caller, not UI timing.
Result<bool> verifySkyrim1170ClientRectAbi(std::span<const std::uint8_t> caller);

// The pointed-to cell is data, not executable code. It is intentionally kept
// separate from the game's shared import cell so only this caller is changed.
class NearRipCall6Cell final {
public:
    ~NearRipCall6Cell();
    NearRipCall6Cell(const NearRipCall6Cell&)=delete;
    NearRipCall6Cell& operator=(const NearRipCall6Cell&)=delete;
    NearRipCall6Cell(NearRipCall6Cell&& other) noexcept;
    NearRipCall6Cell& operator=(NearRipCall6Cell&&)=delete;
    void* address() const noexcept { return memory_; }
    std::uintptr_t callback() const noexcept { return callback_; }
    const std::array<std::uint8_t,6>& callBytes() const noexcept { return callBytes_; }
private:
    friend Result<NearRipCall6Cell> prepareNearRipCall6Cell(const RipCall6Plan&,
        std::uintptr_t,std::uintptr_t);
    NearRipCall6Cell(void* memory,std::uintptr_t callback,
        std::array<std::uint8_t,6> bytes) noexcept:
        memory_(memory),callback_(callback),callBytes_(bytes) {}
    void* memory_{};
    std::uintptr_t callback_{};
    std::array<std::uint8_t,6> callBytes_{};
};
Result<NearRipCall6Cell> prepareNearRipCall6Cell(const RipCall6Plan& plan,
    std::uintptr_t imageBase,std::uintptr_t callback);
Result<bool> applyRipCall6(const RipCall6Plan& plan,std::uintptr_t imageBase,
    const NearRipCall6Cell& cell,CallWriteBoundary boundary);
Result<bool> restoreRipCall6(const RipCall6Plan& plan,std::uintptr_t imageBase,
    const NearRipCall6Cell& cell,CallWriteBoundary boundary);
}
