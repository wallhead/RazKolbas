#pragma once
#include "rk/FgFrameContract.hpp"
#include "rk/FgSharedInputs.hpp"
#include <array>
#include <optional>

namespace rk {
// Inputs are converted, single-sample D3D11 textures. The pointers only need
// to live through prepare(); the ring retains its own shared copies.
struct FgInputSources {
    std::array<ID3D11Texture2D*,5> textures{}; // colour, depth, motion, HUD-less, UI
};
struct FgInputLease {
    std::size_t slot{};
    std::uint64_t generation{},serial{},source{};
    FgCopyTicket lastCopy{};
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>,5> resources{};
};

// Single presentation-thread owner. A lease is not reusable until actual
// producer, copy, provider, Present and allocator progress all retire.
class FgInputLeaseRing {
public:
    FgInputLeaseRing(FgSharedInputs& bridge,std::uint64_t generation) noexcept;
    Result<FgInputLease> prepare(const FgSourceFrame& frame,
        const FgInputSources& sources,ID3D11DeviceContext* context,
        const FgFenceProgress& progress);
    bool submit(const FgInputLease& lease,
        const FgRetirementSet& retirement) noexcept;
    bool discard(const FgInputLease& lease) noexcept;
    bool advanceGeneration(std::uint64_t next,
        const FgFenceProgress& progress) noexcept;
    bool canAdvanceGeneration(std::uint64_t next,
        const FgFenceProgress& progress) const noexcept;
    std::uint64_t generation() const noexcept { return pool_.generation(); }
    bool failed() const noexcept { return failed_; }
private:
    struct Slot {
        std::array<std::optional<FgSharedSurface>,5> surfaces{};
        std::array<D3D11_TEXTURE2D_DESC,5> descs{};
        std::uint64_t serial{};
        bool prepared{};
    };
    bool current(const FgInputLease& lease) const noexcept;
    FgSharedInputs& bridge_;
    FgLeasePool pool_;
    std::array<Slot,FgLeasePool::slotCount> slots_{};
    std::uint64_t nextSerial_{};
    bool failed_{};
};
}
