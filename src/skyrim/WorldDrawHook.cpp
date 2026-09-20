#include "rk/WorldDrawHook.hpp"
#include "rk/CallSite.hpp"
#include "rk/RendererHook.hpp"
#include "rk/SwapObserver.hpp"
#include "rk/WorldDraw.hpp"
#include <spdlog/spdlog.h>
#include <array>
#include <atomic>
#include <cstring>
#include <memory>

namespace rk {
namespace {
struct WorldState {
    WorldDrawForwarder forwarder;
    std::atomic<std::uint64_t> forwarded{0};
};
std::atomic<WorldState*> active{nullptr};
void afterOriginal(void*,std::uint32_t) noexcept {
    active.load(std::memory_order_acquire)->forwarded.fetch_add(1,std::memory_order_relaxed);
}
void worldDrawProxy(void* world,std::uint32_t flags) noexcept {
    active.load(std::memory_order_acquire)->forwarder.dispatch(world,flags);
}
bool read(std::uintptr_t address,void* destination,std::size_t size) {
    SIZE_T copied{};
    return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(address),destination,size,&copied)&&copied==size;
}
}
std::uint64_t worldDrawForwardedCalls() noexcept {
    const auto* state=active.load(std::memory_order_acquire);
    return state?state->forwarded.load(std::memory_order_relaxed):0;
}
Result<bool> installWorldDrawPassThrough(HMODULE game,std::string_view verifiedGameHash,
    const Settings& settings) {
    if(!settings.get<bool>("General.Enabled")||settings.get<bool>("General.SafeMode")||
       !settings.get<bool>("Patching.EnableVersionedPatches")||
       !settings.get<bool>("Patching.ExperimentalPatches")||
       patchDisabled(settings.get<Text>("Patching.DisabledPatchIds").value,worldDrawPatchId)) {
        spdlog::info("World-draw pass-through disabled by configuration");
        return false;
    }
    if(active.load(std::memory_order_acquire))return true;
    const auto& profile=skyrim1170CreationProfile();
    if(!game||verifiedGameHash!=profile.gameSha256)
        return Error{ErrorCode::Unsupported,"World-draw executable identity differs"};
    constexpr std::array<std::uint8_t,5> expected{0xe8,0xd1,0xf7,0xe9,0xff};
    const CallSiteDescriptor descriptor{std::string(worldDrawPatchId),std::string(profile.gameSha256),
        profile.imageSize,0xfa507a,0xe44850,expected};
    const auto base=reinterpret_cast<std::uintptr_t>(game);
    std::array<std::uint8_t,17> caller{};
    std::array<std::uint8_t,33> target{};
    if(!read(base+0xfa5071,caller.data(),caller.size())||
       !read(base+descriptor.originalTargetRva,target.data(),target.size()))
        return Error{ErrorCode::Io,"Cannot read decoded world-call ABI"};
    if(const auto abi=verifySkyrim1170WorldCallAbi(caller,target);
       const auto error=std::get_if<Error>(&abi))return *error;
    const auto planned=prepareCallSite(std::span(caller).subspan(9,5),verifiedGameHash,
        profile.imageSize,descriptor);
    if(const auto error=std::get_if<Error>(&planned))return *error;
    const auto& plan=std::get<CallSitePlan>(planned);
    auto pending=std::make_unique<WorldState>();
    const auto configured=pending->forwarder.configure(
        reinterpret_cast<WorldDrawFn>(base+plan.originalTargetRva),&afterOriginal);
    if(const auto error=std::get_if<Error>(&configured))return *error;
    auto relayResult=prepareNearCallRelay(plan,base,reinterpret_cast<std::uintptr_t>(&worldDrawProxy));
    if(const auto error=std::get_if<Error>(&relayResult))return *error;
    auto relay=std::make_unique<NearCallRelay>(std::move(std::get<NearCallRelay>(relayResult)));
    HMODULE pinnedSelf{};
    constexpr DWORD flags=GET_MODULE_HANDLE_EX_FLAG_PIN|GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS;
    if(!GetModuleHandleExW(flags,reinterpret_cast<LPCWSTR>(&worldDrawProxy),&pinnedSelf))
        return Error{ErrorCode::Unavailable,"Cannot pin world-draw callback DLL"};
    spdlog::info("Preparing {}: CALL RVA=0x{:x}, original RVA=0x{:x}, relay=0x{:x}; SKSEPlugin_Load startup boundary; thread={}",
        worldDrawPatchId,plan.siteRva,plan.originalTargetRva,
        reinterpret_cast<std::uintptr_t>(relay->entry()),GetCurrentThreadId());
    auto* published=pending.release();
    active.store(published,std::memory_order_release);
    const auto applied=applyCallInstruction(plan,base,*relay,
        CallWriteBoundary::SkyrimStartupBeforeWorldThreads);
    if(const auto error=std::get_if<Error>(&applied)) {
        active.store(nullptr,std::memory_order_release);
        delete published;
        return *error;
    }
    relay.release(); // Reachable for process lifetime; never freed while CALL is installed.
    try { spdlog::info("Installed {}: exact five-byte CALL, original-first pass-through; no SR work",worldDrawPatchId); } catch (...) {}
    return true;
}
}
