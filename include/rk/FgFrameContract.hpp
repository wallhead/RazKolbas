#pragma once
#include "rk/FrameContracts.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>

namespace rk {
enum class FgProvider { Off, Auto, Dlss, Fsr };
enum class FgSrProvider : std::uint32_t { Native, Dlss, Fsr, Xess };
constexpr std::uint32_t fgSrMask(FgSrProvider provider) noexcept {
    return 1u<<static_cast<std::uint32_t>(provider);
}
enum class FgReason {
    Ready, LimitedFrameCount, Disabled, UnsupportedAdapter,
    PresentationOwnerUnavailable, InactiveScene, HistoryReset,
    StaleInput, MissingNativeUi, ProviderUnavailable,
    UnvalidatedPairing, UnvalidatedUiContract
};

// These stamps describe real-frame inputs, not ownership of GPU resources.
// The later interop lease must retain the corresponding textures through the
// selected provider's completion fence.
struct FgResourceStamp {
    std::uint64_t source{},generation{};
    Extent extent{};
    bool ready{};
    std::uint64_t resetEpoch{};
};
struct FgSourceFrame {
    std::uint64_t source{},generation{},presentToken{},resetEpoch{};
    Extent render{},display{};
    FgSrProvider sr{FgSrProvider::Native};
    std::uint32_t renderVendor{};
    bool ownerReady{},worldActive{},cameraValid{},loading{},paused{},cameraCut{};
    FgResourceStamp color{},depth{},motion{},hudless{},uiColorAlpha{};
};
struct FgCapability {
    FgProvider provider{FgProvider::Off};
    bool supported{},validated{};
    std::uint32_t maxExtraFrames{};
    std::uint32_t validatedSrPairings{};
    bool separateUiSupported{};
};
struct FgRequest {
    bool enabled{};
    FgProvider provider{FgProvider::Off};
    std::uint32_t extraFrames{1};
};
struct FgDecision {
    FgProvider requested{FgProvider::Off},effective{FgProvider::Off};
    std::uint32_t extraFrames{};
    FgReason reason{FgReason::Disabled};
};

FgDecision decideFg(const FgSourceFrame& frame,const FgRequest& request,
    std::initializer_list<FgCapability> capabilities) noexcept;

// Provider choice is frozen when the presentation owner is created. A later
// capability loss turns FG Off; it never hot-swaps the lower swap-chain owner.
class FgProviderSession {
public:
    FgProviderSession(FgRequest request,FgSrProvider sr,
        std::initializer_list<FgCapability> capabilities) noexcept;
    FgProvider boundProvider() const noexcept { return bound_; }
    FgDecision decide(const FgSourceFrame& frame,
        const FgCapability& liveCapability) const noexcept;
private:
    FgRequest request_{};
    FgProvider bound_{FgProvider::Off};
};

struct FgFenceProgress {
    std::uint64_t generation{},producer{},copy{},providerInput{},present{},allocator{};
};
struct FgRetirementSet {
    std::uint64_t generation{},producer{},copy{},providerInput{},present{},allocator{};
    bool ready(const FgFenceProgress& progress) const noexcept;
};

// Fixed slots own future GPU leases. Submission pins each slot until every
// consumer is complete; resize cannot advance the generation while any slot
// remains acquired or in flight.
class FgLeasePool {
public:
    explicit FgLeasePool(std::uint64_t generation) noexcept : generation_(generation) {}
    static constexpr std::size_t slotCount=3;
    std::optional<std::size_t> acquire(const FgFenceProgress& progress) noexcept;
    bool submit(std::size_t slot,const FgRetirementSet& retirement) noexcept;
    bool releaseUnsubmitted(std::size_t slot) noexcept;
    bool advanceGeneration(std::uint64_t next,
        const FgFenceProgress& progress) noexcept;
    std::uint64_t generation() const noexcept { return generation_; }
private:
    enum class State { Free, Acquired, InFlight };
    struct Slot { State state{State::Free}; FgRetirementSet retirement{}; };
    void retire(const FgFenceProgress& progress) noexcept;
    std::uint64_t generation_{};
    std::array<Slot,slotCount> slots_{};
};

// The lower presentation owner calls this once for each real frame. Generated
// output is counted by the provider and never claims a new source frame.
class FgPresentLedger {
public:
    bool acceptReal(const FgSourceFrame& frame) noexcept;
private:
    std::uint64_t source_{},generation_{},token_{};
};
}
