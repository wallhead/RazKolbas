#include "rk/DrsHook.hpp"
#include "rk/CallSite.hpp"
#include "rk/DrsState.hpp"
#include "rk/RenderSizePolicy.hpp"
#include "rk/RendererHook.hpp"
#include "rk/SwapObserver.hpp"
#include <spdlog/spdlog.h>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <memory>

namespace rk {
namespace {
using JitterFn=void(*)(void*);
struct ProbeState {
    JitterFn original{};
    std::uintptr_t expectedState{},worldTextureSlot{};
    float scale{};
    std::atomic<std::uint32_t> displayWidth{0},displayHeight{0};
    std::atomic<std::uint32_t> nativeLockWaits{0};
    std::atomic<bool> ownsLock{false},active{false},everActivated{false},rejected{false};
};
std::atomic<ProbeState*> probe{nullptr};
bool read(std::uintptr_t address,void* destination,std::size_t size) noexcept {
    SIZE_T copied{};
    return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(address),
        destination,size,&copied)&&copied==size;
}
void writeTransition(std::uint8_t* bytes,const DrsTransition& transition) noexcept {
    std::memcpy(bytes+0x10c,&transition.previousWidth,sizeof(float));
    std::memcpy(bytes+0x110,&transition.previousHeight,sizeof(float));
    std::memcpy(bytes+0x104,&transition.currentWidth,sizeof(float));
    std::memcpy(bytes+0x108,&transition.currentHeight,sizeof(float));
    std::memcpy(bytes+0x118,&transition.lock,sizeof(std::uint32_t));
}
void jitterProxy(void* gameState) noexcept {
    auto* state=probe.load(std::memory_order_acquire);
    if(!state||!state->original)return;
    state->original(gameState);
    if(state->rejected.load(std::memory_order_relaxed)||
       reinterpret_cast<std::uintptr_t>(gameState)!=state->expectedState)return;
    const auto width=state->displayWidth.load(std::memory_order_acquire);
    const auto height=state->displayHeight.load(std::memory_order_relaxed);
    auto* bytes=static_cast<std::uint8_t*>(gameState);
    DrsStateSnapshot snapshot{};
    std::memcpy(&snapshot.displayWidth,bytes+0x24,sizeof(snapshot.displayWidth));
    std::memcpy(&snapshot.displayHeight,bytes+0x28,sizeof(snapshot.displayHeight));
    std::memcpy(&snapshot.currentWidth,bytes+0x104,sizeof(snapshot.currentWidth));
    std::memcpy(&snapshot.currentHeight,bytes+0x108,sizeof(snapshot.currentHeight));
    std::memcpy(&snapshot.lock,bytes+0x118,sizeof(snapshot.lock));
    const Extent display{width,height};
    const Extent requested{
        static_cast<std::uint32_t>(std::floor(width*state->scale)),
        static_cast<std::uint32_t>(std::floor(height*state->scale))};
    // The game owns kMAIN. This opt-in probe measures whether engine DRS
    // reduces it; it does not satisfy the product's owned-SR readiness gate.
    const RenderSizePlan plan{display,requested,true};
    std::uintptr_t worldTexture{};
    const bool sourceReady=read(state->worldTextureSlot,&worldTexture,sizeof(worldTexture))&&worldTexture;
    if(!width||!height||!sourceReady||snapshot.displayWidth!=width||
       snapshot.displayHeight!=height) {
        if(state->ownsLock.load(std::memory_order_acquire)) {
            if(const auto release=planDrsRelease(plan,snapshot,true)) {
                writeTransition(bytes,*release);
                state->ownsLock.store(false,std::memory_order_release);
                state->active.store(false,std::memory_order_release);
                state->rejected.store(true,std::memory_order_release);
                try { spdlog::warn("Experimental DRS probe restored native ratio after output/world extent changed"); } catch(...) {}
            } else {
                state->ownsLock.store(false,std::memory_order_release);
                state->active.store(false,std::memory_order_release);
                state->rejected.store(true,std::memory_order_release);
                try { spdlog::warn("Experimental DRS probe lost ratio/lock ownership after extent change; no foreign state overwritten"); } catch(...) {}
            }
        }
        return;
    }
    const auto transition=planDrsTransition(plan,snapshot,
        state->ownsLock.load(std::memory_order_acquire));
    if(!transition) {
        if(!state->ownsLock.load(std::memory_order_acquire)&&
           drsStateMayRetryAfterNativeLock(plan,snapshot)) {
            const auto waits=state->nativeLockWaits.fetch_add(1,std::memory_order_relaxed)+1;
            if(waits==1||waits%600==0)
                try { spdlog::info("Experimental DRS probe waiting for native-sized lock to clear: samples={}; lock=1 ratio=1; no state write",waits); } catch(...) {}
            return;
        }
        if(state->ownsLock.load(std::memory_order_acquire)) {
            if(const auto release=planDrsRelease(plan,snapshot,true)) {
                writeTransition(bytes,*release);
                state->ownsLock.store(false,std::memory_order_release);
                state->active.store(false,std::memory_order_release);
            } else {
                state->ownsLock.store(false,std::memory_order_release);
                state->active.store(false,std::memory_order_release);
            }
        }
        if(!state->rejected.exchange(true,std::memory_order_acq_rel))
            try { spdlog::warn("Experimental DRS probe rejected state: game={}x{} display={}x{} lock={}; ratio=({},{}); native forwarding retained",
                snapshot.displayWidth,snapshot.displayHeight,width,height,snapshot.lock,
                snapshot.currentWidth,snapshot.currentHeight); } catch(...) {}
        return;
    }
    writeTransition(bytes,*transition);
    state->ownsLock.store(true,std::memory_order_release);
    state->everActivated.store(true,std::memory_order_release);
    if(!state->active.exchange(true,std::memory_order_acq_rel))
        try { spdlog::info("Experimental engine DRS probe applied: {}x{} -> {}x{} after {} native-lock waits; original jitter update forwarded; DLSS SR not implied",
            width,height,requested.width,requested.height,
            state->nativeLockWaits.load(std::memory_order_relaxed)); } catch(...) {}
}
}
Result<bool> installDrsProbe(HMODULE game,std::string_view verifiedGameHash,
    const Settings& settings) {
    if(!settings.get<bool>("General.Enabled")||settings.get<bool>("General.SafeMode")||
       !settings.get<bool>("Patching.EnableVersionedPatches")||
       !settings.get<bool>("Patching.ExperimentalPatches")||
       !settings.get<bool>("Diagnostics.ProbeReducedWorld")||
       patchDisabled(settings.get<Text>("Patching.DisabledPatchIds").value,drsProbePatchId))
        return false;
    const auto scale=settings.get<double>("Upscaling.ManualRenderScale");
    if(!std::isfinite(scale)||scale<0.5||scale>=1.0)
        return Error{ErrorCode::InvalidInput,"DRS probe requires ManualRenderScale in [0.5,1)"};
    if(probe.load(std::memory_order_acquire))return true;
    const auto& profile=skyrim1170CreationProfile();
    if(!game||verifiedGameHash!=profile.gameSha256)
        return Error{ErrorCode::Unsupported,"DRS probe game identity differs"};
    const auto base=reinterpret_cast<std::uintptr_t>(game);
    std::array<std::uint8_t,23> caller{};
    std::array<std::uint8_t,17> originalTarget{};
    if(!read(base+0xe44664,caller.data(),caller.size())||
       !read(base+0xe58a10,originalTarget.data(),originalTarget.size()))
        return Error{ErrorCode::Io,"Cannot read decoded Renderer Begin jitter call"};
    const auto abi=verifySkyrim1170JitterCallAbi(caller,originalTarget);
    if(const auto error=std::get_if<Error>(&abi))return *error;
    const CallSiteDescriptor descriptor{std::string(drsProbePatchId),
        std::string(profile.gameSha256),profile.imageSize,0xe44672,0xe58a10,
        {0xe8,0x99,0x43,0x01,0x00}};
    const auto planned=prepareCallSite(std::span(caller).subspan(14,5),verifiedGameHash,
        profile.imageSize,descriptor);
    if(const auto error=std::get_if<Error>(&planned))return *error;
    const auto& plan=std::get<CallSitePlan>(planned);
    auto pending=std::make_unique<ProbeState>();
    pending->original=reinterpret_cast<JitterFn>(base+plan.originalTargetRva);
    pending->expectedState=base+0x328cc20;
    pending->worldTextureSlot=base+0x32887c0+0xa88;
    pending->scale=static_cast<float>(scale);
    auto relayResult=prepareNearCallRelay(plan,base,reinterpret_cast<std::uintptr_t>(&jitterProxy));
    if(const auto error=std::get_if<Error>(&relayResult))return *error;
    auto relay=std::make_unique<NearCallRelay>(std::move(std::get<NearCallRelay>(relayResult)));
    HMODULE pinnedSelf{};
    constexpr DWORD flags=GET_MODULE_HANDLE_EX_FLAG_PIN|GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS;
    if(!GetModuleHandleExW(flags,reinterpret_cast<LPCWSTR>(&jitterProxy),&pinnedSelf))
        return Error{ErrorCode::Unavailable,"Cannot pin DRS probe callback DLL"};
    auto* published=pending.release();
    probe.store(published,std::memory_order_release);
    const auto applied=applyCallInstruction(plan,base,*relay,
        CallWriteBoundary::SkyrimStartupBeforeWorldThreads);
    if(const auto error=std::get_if<Error>(&applied)) {
        probe.store(nullptr,std::memory_order_release);
        delete published;
        return *error;
    }
    relay.release();
    spdlog::info("Installed {} at jitter CALL RVA 0xe44672; original forwarded; render scale {} applies only after display/world readiness",
        drsProbePatchId,scale);
    return true;
}
void bindDrsDisplay(std::uint32_t width,std::uint32_t height) noexcept {
    auto* state=probe.load(std::memory_order_acquire);
    if(!state||!width||!height)return;
    state->displayHeight.store(height,std::memory_order_relaxed);
    state->displayWidth.store(width,std::memory_order_release);
}
bool drsProbeActive() noexcept {
    const auto* state=probe.load(std::memory_order_acquire);
    return state&&state->active.load(std::memory_order_acquire);
}
bool drsProbeHasRun() noexcept {
    const auto* state=probe.load(std::memory_order_acquire);
    return state&&state->everActivated.load(std::memory_order_acquire);
}
std::optional<Extent> drsProbeRenderExtent() noexcept {
    const auto* state=probe.load(std::memory_order_acquire);
    if(!state||!state->active.load(std::memory_order_acquire))return std::nullopt;
    const auto width=state->displayWidth.load(std::memory_order_acquire);
    const auto height=state->displayHeight.load(std::memory_order_relaxed);
    if(!width||!height)return std::nullopt;
    return Extent{static_cast<std::uint32_t>(std::floor(width*state->scale)),
        static_cast<std::uint32_t>(std::floor(height*state->scale))};
}
}
