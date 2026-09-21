#pragma once
#include "rk/Result.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace rk {
struct CallSiteDescriptor {
    std::string id;
    std::string gameSha256;
    std::size_t imageSize{};
    std::uint32_t siteRva{}, originalTargetRva{};
    std::array<std::uint8_t,5> expected{};
};
struct CallSitePlan {
    std::string id;
    std::uint32_t siteRva{}, originalTargetRva{};
    std::array<std::uint8_t,5> original{};
};
// Inspects decoded live code only after the caller independently verifies the
// executable file identity. No write, hook installation, or ownership claim.
Result<CallSitePlan> prepareCallSite(std::span<const std::uint8_t> live,
    std::string_view verifiedGameHash,std::size_t imageSize,const CallSiteDescriptor& descriptor);
// Exact decoded instructions observed in the user-run Skyrim 1.6.1170 build.
// Must be checked alongside prepareCallSite before claiming the two-argument
// forwarding ABI. Does not infer the semantic type of the game pointer.
Result<bool> verifySkyrim1170WorldCallAbi(std::span<const std::uint8_t> caller,
    std::span<const std::uint8_t> originalTarget);
// Exact call immediately before IMenu::PostDisplay in Skyrim 1.6.1170.
// The four Win64 arguments are already materialized at this boundary.
Result<bool> verifySkyrim1170MenuDisplayCallAbi(
    std::span<const std::uint8_t> caller,
    std::span<const std::uint8_t> originalTarget);
// Read-only exact-code gate for the decoded DRS update CALL and scissor
// function. A successful check does not activate reduced rendering or grant
// permission to modify either site during an active game session.
Result<bool> verifySkyrim1170DrsAbi(std::span<const std::uint8_t> caller,
    std::span<const std::uint8_t> originalTarget,
    std::span<const std::uint8_t> scissor);
// Renderer Begin passes the graphics state to this direct CALL; its exact
// caller and original jitter target must match before any forwarding hook.
Result<bool> verifySkyrim1170JitterCallAbi(std::span<const std::uint8_t> caller,
    std::span<const std::uint8_t> originalTarget);
// Builds the complete direct-CALL instruction for a verified plan. This is
// preparation only: the caller must separately establish exclusive execution
// quiescence, recheck the live bytes, and own the target's lifetime before write.
Result<std::array<std::uint8_t,5>> encodeCallSiteReplacement(const CallSitePlan& plan,
    std::uintptr_t imageBase,std::uintptr_t detourTarget);
// RIP-relative indirect jump plus an absolute 64-bit target. It preserves
// registers and flags when a process-lifetime relay is needed within rel32
// reach of the CALL. Allocation and executable lifetime remain the caller's.
Result<std::array<std::uint8_t,14>> encodeRegisterPreservingJump(std::uintptr_t target);

// Owns a prepared W^X relay. Destruction is valid only while no installed
// CALL or in-flight callback can reach it; a future game patch lease must
// retain it until safe removal and quiescence are proven.
class NearCallRelay {
public:
    ~NearCallRelay();
    NearCallRelay(const NearCallRelay&)=delete;
    NearCallRelay& operator=(const NearCallRelay&)=delete;
    NearCallRelay(NearCallRelay&& other) noexcept;
    NearCallRelay& operator=(NearCallRelay&&)=delete;
    void* entry() const noexcept { return memory_; }
    const std::array<std::uint8_t,5>& callBytes() const noexcept { return callBytes_; }
    std::uintptr_t target() const noexcept { return target_; }
private:
    friend Result<NearCallRelay> prepareNearCallRelay(const CallSitePlan&,std::uintptr_t,std::uintptr_t);
    NearCallRelay(void* memory,std::array<std::uint8_t,5> callBytes,std::uintptr_t target) noexcept:
        memory_(memory),callBytes_(callBytes),target_(target) {}
    void* memory_{};
    std::array<std::uint8_t,5> callBytes_{};
    std::uintptr_t target_{};
};
// Allocates and seals a relay within CALL rel32 reach. It never changes the
// game instruction; activation requires a separate safe patch transaction.
Result<NearCallRelay> prepareNearCallRelay(const CallSitePlan& plan,
    std::uintptr_t imageBase,std::uintptr_t target);

// The caller must prove exclusive execution at the write site. The enum
// records that proof's boundary; it does not suspend unrelated game threads.
enum class CallWriteBoundary { None, OwnedFixtureExclusive, SkyrimStartupBeforeWorldThreads };
Result<bool> applyCallInstruction(const CallSitePlan& plan,std::uintptr_t imageBase,
    const NearCallRelay& relay,CallWriteBoundary boundary);
Result<bool> restoreCallInstruction(const CallSitePlan& plan,std::uintptr_t imageBase,
    const NearCallRelay& relay,CallWriteBoundary boundary);
}
