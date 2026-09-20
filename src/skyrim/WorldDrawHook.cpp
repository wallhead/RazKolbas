#include "rk/WorldDrawHook.hpp"
#include "rk/CallSite.hpp"
#include "rk/FrameProbe.hpp"
#include "rk/RendererHook.hpp"
#include "rk/SwapObserver.hpp"
#include "rk/SrInput.hpp"
#include "rk/WorldDraw.hpp"
#ifdef RK_WITH_NGX
#include "rk/OffscreenDlssProbe.hpp"
#endif
#include <spdlog/spdlog.h>
#include <array>
#include <atomic>
#include <cstring>
#include <memory>
#include <optional>

namespace rk {
namespace {
struct WorldState {
    WorldDrawForwarder forwarder;
    std::atomic<std::uint64_t> forwarded{0};
    std::uintptr_t expectedRenderer{};
    std::atomic_flag creationBound=ATOMIC_FLAG_INIT;
    std::atomic<std::uintptr_t> createdDevice{0},createdContext{0},createdSwap{0};
    enum class CopyStatus { NotAttempted, Pending, Complete, Failed };
    std::atomic<CopyStatus> copyStatus{CopyStatus::NotAttempted};
    Microsoft::WRL::ComPtr<ID3D11Query> copyEvent;
    std::optional<PreparedSrInputs> copiedFrame;
#ifdef RK_WITH_NGX
    OffscreenDlssProbe dlssProbe;
    bool probeFailed{};
#endif
};
std::atomic<WorldState*> active{nullptr};
struct WorldNumbers {
    std::uintptr_t device{},context{},swap{},colour{},motion{},depth{},lockOwner{};
    std::int32_t lockRecursion{};
    bool valid{};
};
WorldNumbers readWorldNumbers(void* world,std::uintptr_t expected) noexcept {
    WorldNumbers result{};
    if(!world||reinterpret_cast<std::uintptr_t>(world)!=expected)return result;
    std::array<std::uint8_t,renderer1170Size> bytes{};
    SIZE_T copied{};
    if(!ReadProcessMemory(GetCurrentProcess(),world,bytes.data(),bytes.size(),&copied)||copied!=bytes.size())
        return result;
    const auto pointer=[&](std::size_t offset) {
        std::uintptr_t value{};std::memcpy(&value,bytes.data()+offset,sizeof(value));return value;
    };
    result.device=pointer(0x48);result.context=pointer(0x50);result.swap=pointer(0x70);
    result.colour=pointer(0xa58+0x30);result.motion=pointer(0xa58+7*0x30);
    result.depth=pointer(0x2018);result.lockOwner=pointer(0x27f0+16);
    std::memcpy(&result.lockRecursion,bytes.data()+0x27f0+12,sizeof(result.lockRecursion));
    result.valid=true;
    return result;
}
void copyWorldInputsOnce(WorldState* state,const WorldNumbers& numbers) noexcept {
    const auto status=state->copyStatus.load(std::memory_order_acquire);
    if(status==WorldState::CopyStatus::Complete||status==WorldState::CopyStatus::Failed)return;
    const auto thread=GetCurrentThreadId();
    const auto device=state->createdDevice.load(std::memory_order_acquire);
    const auto context=state->createdContext.load(std::memory_order_relaxed);
    const auto swap=state->createdSwap.load(std::memory_order_relaxed);
    if(!numbers.valid||numbers.lockOwner!=thread||numbers.lockRecursion<=0||
       !device||!context||!swap||numbers.device!=device||numbers.context!=context||
       numbers.swap!=swap||!numbers.colour||!numbers.motion||!numbers.depth)return;
    auto* immediate=reinterpret_cast<ID3D11DeviceContext*>(context);
    if(status==WorldState::CopyStatus::Pending) {
        const auto result=immediate->GetData(state->copyEvent.Get(),nullptr,0,
            D3D11_ASYNC_GETDATA_DONOTFLUSH);
        if(result==S_OK) {
            const auto removed=reinterpret_cast<ID3D11Device*>(device)->GetDeviceRemovedReason();
            if(FAILED(removed)) {
                state->copyStatus.store(WorldState::CopyStatus::Failed,std::memory_order_release);
                try { spdlog::warn("Owned SR input copy device removed: HRESULT=0x{:08x}; resources retained",static_cast<std::uint32_t>(removed)); } catch (...) {}
                return;
            }
#ifdef RK_WITH_NGX
            try {
                const auto begun=state->dlssProbe.begin(reinterpret_cast<ID3D11Device*>(device),
                    immediate,*state->copiedFrame);
                if(const auto error=std::get_if<Error>(&begun)) {
                    state->probeFailed=true;
                    spdlog::warn("Offscreen DLSS probe rejected: {}; owned resources retained",error->message);
                } else {
                    spdlog::info("Offscreen DLAA evaluation submitted on game device; no display write");
                }
            } catch(const std::exception& error) {
                state->probeFailed=true;
                try { spdlog::warn("Offscreen DLSS probe exception: {}; owned resources retained",error.what()); } catch(...) {}
            } catch(...) {
                state->probeFailed=true;
                try { spdlog::warn("Offscreen DLSS probe exception; owned resources retained"); } catch(...) {}
            }
#else
            state->copiedFrame.reset();
#endif
            state->copyEvent.Reset();
            state->copyStatus.store(WorldState::CopyStatus::Complete,std::memory_order_release);
            try { spdlog::info("Owned SR input copies completed on game GPU: original colour/motion/native typeless depth; no NGX evaluation or display write"); } catch (...) {}
        } else if(FAILED(result)) {
            state->copyStatus.store(WorldState::CopyStatus::Failed,std::memory_order_release);
            // Completion is uncertain; retain resources until process exit.
            try { spdlog::warn("Owned SR input copy completion failed: HRESULT=0x{:08x}; resources retained",static_cast<std::uint32_t>(result)); } catch (...) {}
        }
        return;
    }
    state->copyStatus.store(WorldState::CopyStatus::Failed,std::memory_order_release);
    try {
        auto* actualDevice=reinterpret_cast<ID3D11Device*>(device);
        Microsoft::WRL::ComPtr<ID3D11Query> event;
        const D3D11_QUERY_DESC query{D3D11_QUERY_EVENT,0};
        const auto created=actualDevice->CreateQuery(&query,&event);
        if(FAILED(created)) {
            spdlog::warn("Owned SR input copy event unavailable: HRESULT=0x{:08x}",static_cast<std::uint32_t>(created));return;
        }
        const std::array sources{
            reinterpret_cast<ID3D11Texture2D*>(numbers.colour),
            reinterpret_cast<ID3D11Texture2D*>(numbers.motion),
            reinterpret_cast<ID3D11Texture2D*>(numbers.depth)};
        auto prepared=prepareSrInputs(immediate,sources);
        if(const auto error=std::get_if<Error>(&prepared)) {
            spdlog::warn("Owned SR input copy rejected: {}",error->message);return;
        }
        state->copiedFrame.emplace(std::move(std::get<PreparedSrInputs>(prepared)));
        state->copyEvent=std::move(event);
        immediate->End(state->copyEvent.Get());
        immediate->Flush();
        state->copyStatus.store(WorldState::CopyStatus::Pending,std::memory_order_release);
        try { spdlog::info("Owned SR input copies queued: {}x{} colour/motion/native typeless depth; original resources unchanged; no NGX evaluation",
            state->copiedFrame->width(),state->copiedFrame->height()); } catch (...) {}
    } catch(const std::exception& error) {
        state->copyStatus.store(WorldState::CopyStatus::Failed,std::memory_order_release);
        try { spdlog::warn("Owned SR input copy aborted: {}",error.what()); } catch (...) {}
    } catch(...) {
        state->copyStatus.store(WorldState::CopyStatus::Failed,std::memory_order_release);
        try { spdlog::warn("Owned SR input copy aborted by unknown exception"); } catch (...) {}
    }
}
void afterOriginal(void*,std::uint32_t) noexcept {
    active.load(std::memory_order_acquire)->forwarded.fetch_add(1,std::memory_order_relaxed);
}
void worldDrawProxy(void* world,std::uint32_t flags) noexcept {
    auto* state=active.load(std::memory_order_acquire);
    const auto sequence=state->forwarded.load(std::memory_order_relaxed)+1;
    const bool sample=sequence<=3||sequence%600==0;
    const auto before=sample?readWorldNumbers(world,state->expectedRenderer):WorldNumbers{};
    state->forwarder.dispatch(world,flags);
    if(const auto status=state->copyStatus.load(std::memory_order_acquire);
       status==WorldState::CopyStatus::NotAttempted||status==WorldState::CopyStatus::Pending)
        copyWorldInputsOnce(state,readWorldNumbers(world,state->expectedRenderer));
#ifdef RK_WITH_NGX
    if(state->dlssProbe.pending()&&!state->probeFailed) {
        const auto numbers=readWorldNumbers(world,state->expectedRenderer);
        if(numbers.valid&&numbers.lockOwner==GetCurrentThreadId()&&numbers.lockRecursion>0&&
           numbers.device==state->createdDevice.load(std::memory_order_acquire)&&
           numbers.context==state->createdContext.load(std::memory_order_relaxed)&&
           numbers.swap==state->createdSwap.load(std::memory_order_relaxed)) {
            try {
                const auto polled=state->dlssProbe.poll(reinterpret_cast<ID3D11Device*>(numbers.device),
                    reinterpret_cast<ID3D11DeviceContext*>(numbers.context));
                if(const auto error=std::get_if<Error>(&polled)) {
                    state->probeFailed=true;
                    spdlog::warn("Offscreen DLSS probe failed: {}; owned resources retained",error->message);
                } else if(!std::get<std::string>(polled).empty()) {
                    spdlog::info("Offscreen DLAA output validated: {}x{} finite nonuniform RGB; SHA256={}; no display write",
                        state->dlssProbe.width(),state->dlssProbe.height(),std::get<std::string>(polled));
                    state->copiedFrame.reset();
                }
            } catch(const std::exception& error) {
                state->probeFailed=true;
                try { spdlog::warn("Offscreen DLSS probe exception: {}; owned resources retained",error.what()); } catch(...) {}
            } catch(...) {
                state->probeFailed=true;
                try { spdlog::warn("Offscreen DLSS probe exception; owned resources retained"); } catch(...) {}
            }
        }
    }
#endif
    if(!sample)return;
    const auto after=readWorldNumbers(world,state->expectedRenderer);
    try {
        spdlog::info("World stage #{}: thread={}; flags=0x{:x}; rendererMatch={}; beforeRead={}; afterRead={}; "
            "beforeLock={}/{}; afterLock={}/{}; device=0x{:x}; context=0x{:x}; swap=0x{:x}; "
            "colour=0x{:x}->0x{:x}; motion=0x{:x}->0x{:x}; depth=0x{:x}->0x{:x}; read-only",
            sequence,GetCurrentThreadId(),flags,
            reinterpret_cast<std::uintptr_t>(world)==state->expectedRenderer,before.valid,after.valid,
            before.lockOwner,before.lockRecursion,after.lockOwner,after.lockRecursion,
            after.device,after.context,after.swap,
            before.colour,after.colour,before.motion,after.motion,before.depth,after.depth);
    } catch (...) {}
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
void bindWorldDrawRenderer(ID3D11Device* device,ID3D11DeviceContext* context,
    IDXGISwapChain* swap) noexcept {
    auto* state=active.load(std::memory_order_acquire);
    if(!state||!device||!context||!swap||state->creationBound.test_and_set(std::memory_order_acq_rel))return;
    state->createdContext.store(reinterpret_cast<std::uintptr_t>(context),std::memory_order_relaxed);
    state->createdSwap.store(reinterpret_cast<std::uintptr_t>(swap),std::memory_order_relaxed);
    state->createdDevice.store(reinterpret_cast<std::uintptr_t>(device),std::memory_order_release);
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
    pending->expectedRenderer=base+renderer1170Rva;
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
