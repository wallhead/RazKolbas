#pragma once
#include "rk/FrameContracts.hpp"
#include <d3d11.h>
#include <optional>
#include <span>
#include <string_view>

namespace rk {
// At most eight candidate reads per 600-frame window, 64 windows total.
// A successful HDR observation closes its window early.
class EnbTargetProbeBudget {
public:
    bool admit(std::uint64_t frame) noexcept;
    void accepted() noexcept { attempts_=8; }
private:
    std::uint64_t windowStart_{};
    unsigned windows_{},attempts_{};
};
// Diagnostics only. No private-data writes, shader changes or target binding.
bool validateEnbTargetProbeImage(std::span<const std::uint8_t> mapped,
    std::string_view verifiedHash) noexcept;
// The caller must validate and pin the exact module before supplying its base.
std::optional<Extent> readEnbTargetProbeExtent(std::uintptr_t verifiedBase) noexcept;
struct EnbTargetSample {
    Extent actual{},metadata{};
    std::uint32_t format{},metadataFormat{},boundMask{};
    bool metadataValid{};
};
EnbTargetSample inspectEnbTargets(ID3D11DeviceContext* context,
    ID3D11RenderTargetView* input) noexcept;
}
