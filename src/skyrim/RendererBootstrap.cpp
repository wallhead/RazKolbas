#include "rk/RendererBootstrap.hpp"
#include "rk/MenuInputHook.hpp"
#include "rk/PatchDescriptor.hpp"
#include "rk/PointerPatch.hpp"
#include "rk/SwapObserver.hpp"
#include "rk/DiagnosticsMenu.hpp"
#include "rk/FrameProbeRuntime.hpp"
#include "rk/FrameProbe.hpp"
#include "rk/WorldDrawHook.hpp"
#include "rk/DrsHook.hpp"
#include "rk/FactoryCreateTrace.hpp"
#include "rk/OwnedRouteProfile.hpp"
#include "rk/EnbTargetProbe.hpp"
#include "rk/OwnedSwapBufferRoute.hpp"
#include "rk/NativeUiRedirector.hpp"
#include "rk/NativeFlipTarget.hpp"
#include "rk/SpatialFallback.hpp"
#include "rk/RendererLogicalSize.hpp"
#include "rk/SamplerBiasCache.hpp"
#include "rk/RipCall6.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <intrin.h>
#include <wrl/client.h>

namespace rk {
namespace {
struct ObserverLease {
    PointerPatch patch;
    CreateD3D11 original{};
    RendererObserved notification{};
    std::atomic<unsigned> observations{0};
    std::atomic<bool> armed{false};
    std::string disabledPatchIds;
    HMODULE exactEnbOwner{}; // pinned with the verified creation export owner
};
// Process-lifetime lease: both callback DLL and prior owner's DLL are pinned.
// SKSE can FreeLibrary during shutdown; reachable callback code must remain valid.
std::atomic<ObserverLease*> lease{nullptr};
std::atomic_flag factoryProvenanceLogged=ATOMIC_FLAG_INIT;
struct FactoryTraceLease {
    PointerPatch patch;
    FactoryCreateFn next{};
    IDXGIFactory* target{}; // identity only; do not retain the factory object
    HMODULE exactEnbOwner{};
    std::atomic<unsigned> calls{0};
};
std::atomic<FactoryTraceLease*> factoryTrace{nullptr};
std::atomic_flag factoryTraceAttempted=ATOMIC_FLAG_INIT;
struct BufferTraceLease {
    PointerPatch getBufferPatch,getDescPatch,presentPatch;
    SwapGetBufferFn next{};
    SwapGetDescFn nextDesc{};
    PresentFn nextPresent{};
    IDXGISwapChain* selectedSwap{}; // retained by the route before activation
    IDXGISwapChain* outerSwap{}; // identity only; renderer owns the swap
    std::unique_ptr<OwnedSwapBufferRoute> route;
    Microsoft::WRL::ComPtr<ID3D11Device> earlyDevice;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> earlyContext;
    std::uintptr_t gameBase{};
    std::string gameHash;
    std::string enbHash;
    std::atomic<std::uintptr_t> enbBase{0};
    std::atomic<bool> ownedArmed{false};
    std::atomic<bool> integrationReady{false};
    std::atomic<bool> cleanupPending{false};
    std::atomic<unsigned> calls{0};
    std::atomic<unsigned> emergencyFrames{0},emergencyFailures{0};
};
std::atomic<BufferTraceLease*> bufferTrace{nullptr};
std::atomic_flag bufferTraceAttempted=ATOMIC_FLAG_INIT;
OwnedSceneDomain ownedDomain;
std::atomic<bool> ownedEverActive{false};
std::atomic<DWORD> ownedFrameThread{0};
struct UiHookLease {
    explicit UiHookLease(OwnedSceneDomain& domain) noexcept:redirect(domain) {}
    PointerPatch omPatch,viewportPatch,scissorPatch,psPatch;
    std::array<PointerPatch,6> samplerPatches;
    using Sampler=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,UINT,
        ID3D11SamplerState* const*);
    std::array<Sampler,6> nextSamplers{};
    std::array<void**,6> samplerSlots{};
    std::array<void*,6> samplerProxies{};
    UiContextNext next{};
    NativeUiRedirector redirect;
    SamplerBiasCache samplerBias;
    std::size_t loggedSamplerReplacements{};
    std::atomic<std::uint64_t> samplerOwnershipChecks{};
    std::atomic<unsigned> lastSamplerOwnershipMask{~0u};
    std::atomic<bool> armed{false};
    std::uintptr_t enbProbeBase{};
    EnbTargetProbeBudget enbProbeBudget;
    unsigned enbProbeSamples{};
};
std::atomic<UiHookLease*> uiHook{nullptr};
std::mutex uiInstallMutex;
std::mutex uiDispatchMutex;
struct RectHookLease {
    RendererLogicalSize::Prior prior{};
    std::unique_ptr<NearRipCall6Cell> cell;
    std::atomic<RendererLogicalSize*> logical{nullptr};
    OwnedSceneDomain* domain{};
    HWND gameWindow{};
};
std::atomic<RectHookLease*> rectHook{nullptr};
BOOL WINAPI rendererRectProxy(HWND window,RECT* rect) noexcept {
    auto* state=rectHook.load(std::memory_order_acquire);
    if(!state||!state->prior)return FALSE;
    auto* logical=state->logical.load(std::memory_order_acquire);
    if(!logical||window!=state->gameWindow)return state->prior(window,rect);
    auto& domain=*state->domain;
    const auto phase=domain.phase();
    if(phase==ScenePhase::Dormant) {
        const auto frame=worldDrawForwardedCalls()+1;
        if(domain.begin(frame,domain.plan().generation,GetCurrentThreadId()))
            ownedFrameThread.store(GetCurrentThreadId(),std::memory_order_release);
    }
    return logical->query(window,rect);
}
void STDMETHODCALLTYPE uiOmProxy(ID3D11DeviceContext* context,UINT count,
    ID3D11RenderTargetView* const* views,ID3D11DepthStencilView* depth) noexcept {
    std::scoped_lock lock(uiDispatchMutex);
    auto* state=uiHook.load(std::memory_order_acquire);
    if(!state)return;
    if(state->armed.load(std::memory_order_acquire)) {
        const bool faultBefore=state->redirect.compatibilityFault();
        const auto phaseBefore=ownedDomain.phase();
        const auto worldBefore=worldDrawForwardedCalls();
        state->redirect.onOMSetRenderTargets(context,count,views,depth);
        // Sample after the existing downstream bind; never issue a replacement
        // bind. Throttled through the same render-dispatch lock as the hook.
        if(state->enbProbeBase&&phaseBefore==ScenePhase::World&&count>=2&&
            count<=8&&views&&views[0]&&state->enbProbeBudget.admit(worldBefore)) {
            const auto sample=inspectEnbTargets(context,views[0]);
            if(sample.format==10||sample.format==26) {
                ++state->enbProbeSamples;
                state->enbProbeBudget.accepted();
                const auto reference=readEnbTargetProbeExtent(state->enbProbeBase);
                const bool match=reference&&sample.metadataValid&&
                    reference->width==sample.metadata.width&&reference->height==sample.metadata.height;
                try {spdlog::info("ENB target probe #{}: worldForwarded={}; requestedCount={}; actual={}x{} format={}; metadataValid={} metadata={}x{} format={}; referenceReadable={} reference={}x{}; dimensionMatch={}; boundMask=0x{:02x}; slots5and6={}; observation-only",
                    state->enbProbeSamples,worldBefore,count,sample.actual.width,sample.actual.height,
                    sample.format,sample.metadataValid,sample.metadata.width,sample.metadata.height,
                    sample.metadataFormat,reference.has_value(),reference?reference->width:0,
                    reference?reference->height:0,match,sample.boundMask,(sample.boundMask&0x60)==0x60);
                } catch(...) {}
            }
        }
        if(!faultBefore&&state->redirect.compatibilityFault()) {
            const auto fault=state->redirect.compatibilityFaultInfo();
            try {spdlog::warn("Owned UI bind incompatible: phase={} worldForwarded={} thread={} targetCount={} sceneSlot={} depth={} depthExtent={}x{} depthId=0x{:x} expectedDepthId=0x{:x} unknownTargetSlot={} unknownTargetId=0x{:x} unknownTargetFormat={} unknownTargetMips={} unknownTargetBindFlags=0x{:x}",
                static_cast<unsigned>(phaseBefore),worldBefore,GetCurrentThreadId(),
                fault.targetCount,fault.sceneSlot,fault.hasDepth,
                fault.depthWidth,fault.depthHeight,fault.depthId,fault.expectedDepthId,
                fault.unknownTargetSlot,fault.unknownTargetId,
                static_cast<unsigned>(fault.unknownTargetFormat),
                fault.unknownTargetMips,fault.unknownTargetBindFlags);} catch(...) {}
        }
    } else state->next.om(context,count,views,depth);
}
void STDMETHODCALLTYPE uiViewportProxy(ID3D11DeviceContext* context,UINT count,
    const D3D11_VIEWPORT* views) noexcept {
    std::scoped_lock lock(uiDispatchMutex);
    auto* state=uiHook.load(std::memory_order_acquire);
    if(!state)return;
    if(state->armed.load(std::memory_order_acquire))
        state->redirect.onRSSetViewports(context,count,views);
    else state->next.viewport(context,count,views);
}
void STDMETHODCALLTYPE uiScissorProxy(ID3D11DeviceContext* context,UINT count,
    const D3D11_RECT* rects) noexcept {
    std::scoped_lock lock(uiDispatchMutex);
    auto* state=uiHook.load(std::memory_order_acquire);
    if(!state)return;
    if(state->armed.load(std::memory_order_acquire))
        state->redirect.onRSSetScissorRects(context,count,rects);
    else state->next.scissor(context,count,rects);
}
void STDMETHODCALLTYPE uiPsProxy(ID3D11DeviceContext* context,UINT start,UINT count,
    ID3D11ShaderResourceView* const* views) noexcept {
    std::scoped_lock lock(uiDispatchMutex);
    auto* state=uiHook.load(std::memory_order_acquire);
    if(!state)return;
    if(state->armed.load(std::memory_order_acquire))
        state->redirect.onPSSetShaderResources(context,start,count,views);
    else state->next.ps(context,start,count,views);
}
template<std::size_t Stage>
void STDMETHODCALLTYPE uiSamplerProxy(ID3D11DeviceContext* context,UINT start,UINT count,
    ID3D11SamplerState* const* samplers) noexcept {
    std::scoped_lock lock(uiDispatchMutex);
    auto* state=uiHook.load(std::memory_order_acquire);
    if(!state||!state->nextSamplers[Stage])return;
    if(!state->armed.load(std::memory_order_acquire)||!samplers||!count||
       count>D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT||
       start>D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT-count) {
        state->nextSamplers[Stage](context,start,count,samplers);return;
    }
    std::array<ID3D11SamplerState*,D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT> mapped{};
    const auto changed=state->samplerBias.remap(context,
        {samplers,count},{mapped.data(),count});
    state->nextSamplers[Stage](context,start,count,
        changed?mapped.data():samplers);
    const auto replacements=state->samplerBias.replacementCount();
    if(replacements!=state->loggedSamplerReplacements) {
        state->loggedSamplerReplacements=replacements;
        try {spdlog::info("Owned reduced sampler bias active: bias={}; cached replacements={}",
            state->samplerBias.bias(),replacements);}catch(...) {}
    }
}
void releasePreparedUiHook(bool unbindNative=false) noexcept {
    std::scoped_lock lock(uiDispatchMutex);
    if(auto* state=uiHook.load(std::memory_order_acquire)) {
        state->armed.store(false,std::memory_order_release);
        state->redirect.releaseAfterRetirement(unbindNative);
    }
}
struct FileIdentity { std::string hash; std::size_t size; };
FileIdentity identify(HMODULE module) {
    wchar_t name[32768];
    const auto count=GetModuleFileNameW(module,name,32768);
    if (!count || count>=32768) throw std::runtime_error("Cannot resolve loaded module path");
    struct FileLock { HANDLE handle; ~FileLock() { if(handle!=INVALID_HANDLE_VALUE)CloseHandle(handle); } };
    FileLock lock{CreateFileW(name,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr)};
    if (lock.handle==INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot lock module for identity check");
    std::ifstream file(std::filesystem::path(name),std::ios::binary);
    if (!file) throw std::runtime_error("Cannot read loaded module file");
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)),{});
    if (file.bad()) throw std::runtime_error("Module file read failed");
    return {sha256(bytes),bytes.size()};
}
void logFactoryBeforeCreation(IDXGIAdapter* adapter) noexcept {
    if(factoryProvenanceLogged.test_and_set(std::memory_order_acq_rel))return;
    if(!adapter) {
        try { spdlog::info("Pre-create factory provenance: adapter=null; no factory table inspected"); } catch(...) {}
        return;
    }
    try {
        Microsoft::WRL::ComPtr<IDXGIFactory> factory;
        const auto result=adapter->GetParent(IID_PPV_ARGS(&factory));
        if(FAILED(result)||!factory) {
            spdlog::warn("Pre-create factory provenance: adapter=0x{:x}; GetParent HRESULT=0x{:08x}",
                reinterpret_cast<std::uintptr_t>(adapter),static_cast<std::uint32_t>(result));
            return;
        }
        auto** table=*reinterpret_cast<void***>(factory.Get());
        const auto create=std::atomic_ref<void*>(table[10]).load(std::memory_order_acquire);
        HMODULE tableOwner{},methodOwner{};
        constexpr DWORD flags=GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS;
        GetModuleHandleExW(flags,reinterpret_cast<LPCWSTR>(table),&tableOwner);
        if(!GetModuleHandleExW(flags,reinterpret_cast<LPCWSTR>(create),&methodOwner)) {
            if(tableOwner)FreeLibrary(tableOwner);
            spdlog::warn("Pre-create factory provenance: CreateSwapChain method has no loaded-module owner");
            return;
        }
        struct Refs {HMODULE a,b;~Refs(){if(a)FreeLibrary(a);if(b)FreeLibrary(b);}} refs{tableOwner,methodOwner};
        const auto tableIdentity=tableOwner?identify(tableOwner):FileIdentity{"unowned",0};
        const auto methodIdentity=methodOwner==tableOwner?tableIdentity:identify(methodOwner);
        const auto tableBase=reinterpret_cast<std::uintptr_t>(tableOwner);
        const auto methodBase=reinterpret_cast<std::uintptr_t>(methodOwner);
        spdlog::info("Pre-create factory provenance: adapter=0x{:x}; factory=0x{:x}; tableOwnerSHA256={}; tableSize={}; tableRVA=0x{:x}; CreateSwapChainOwnerSHA256={}; methodSize={}; methodRVA=0x{:x}; read-only",
            reinterpret_cast<std::uintptr_t>(adapter),reinterpret_cast<std::uintptr_t>(factory.Get()),
            tableIdentity.hash,tableIdentity.size,
            tableOwner?reinterpret_cast<std::uintptr_t>(table)-tableBase:0,
            methodIdentity.hash,methodIdentity.size,reinterpret_cast<std::uintptr_t>(create)-methodBase);
    } catch(const std::exception& error) {
        try { spdlog::warn("Pre-create factory provenance unavailable: {}",error.what()); } catch(...) {}
    } catch(...) {
        try { spdlog::warn("Pre-create factory provenance unavailable"); } catch(...) {}
    }
}
std::vector<std::uint8_t> snapshotModule(HMODULE module,std::size_t size) {
    const auto base=reinterpret_cast<std::uintptr_t>(module);
    std::vector<std::uint8_t> result(size);
    for (std::size_t offset=0;offset<size;) {
        MEMORY_BASIC_INFORMATION page{};
        if (!VirtualQuery(reinterpret_cast<const void*>(base+offset),&page,sizeof(page)) ||
            page.AllocationBase!=module || !page.RegionSize) throw std::runtime_error("Mapped module boundary changed");
        const auto consumed=base+offset-reinterpret_cast<std::uintptr_t>(page.BaseAddress);
        if (consumed>=page.RegionSize) throw std::runtime_error("Invalid mapped page extent");
        const auto count=std::min(size-offset,page.RegionSize-consumed);
        const auto protection=page.Protect&0xff;
        if (page.State==MEM_COMMIT && !(page.Protect&PAGE_GUARD) &&
            protection!=PAGE_NOACCESS && protection!=PAGE_EXECUTE)
            std::memcpy(result.data()+offset,reinterpret_cast<const void*>(base+offset),count);
        offset+=count;
    }
    return result;
}
std::uintptr_t resolveVerifiedEnbBase(BufferTraceLease& state,
    std::uintptr_t caller) noexcept {
    if(const auto cached=state.enbBase.load(std::memory_order_acquire))return cached;
    try {
        HMODULE owner=nullptr;
        if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
            reinterpret_cast<LPCWSTR>(caller),&owner)||!owner)return 0;
        struct Reference {HMODULE value;~Reference(){if(value)FreeLibrary(value);}} reference{owner};
        const auto identity=identify(owner);
        if(identity.hash!=state.enbHash||!findEnbContextSites(identity.hash).om)return 0;
        const auto base=reinterpret_cast<std::uintptr_t>(owner);
        std::uintptr_t expected{};
        state.enbBase.compare_exchange_strong(expected,base,
            std::memory_order_acq_rel,std::memory_order_acquire);
        return state.enbBase.load(std::memory_order_acquire);
    } catch(...) { return 0; }
}
HRESULT WINAPI swapGetBufferTrace(IDXGISwapChain* swap,UINT index,
    REFIID iid,void** output) noexcept {
    auto* state=bufferTrace.load(std::memory_order_acquire);
    if(!state||!state->next)return E_UNEXPECTED;
    const auto caller=_ReturnAddress();
    const auto owned=state->ownedArmed.load(std::memory_order_acquire);
    const auto enbBase=owned?resolveVerifiedEnbBase(*state,
        reinterpret_cast<std::uintptr_t>(caller)):0;
    const auto result=owned?state->route->getBufferForConsumers(
        reinterpret_cast<std::uintptr_t>(caller),state->gameBase,state->gameHash,
        enbBase,state->enbHash,
        swap,index,iid,output):state->next(swap,index,iid,output);
    const auto sequence=state->calls.fetch_add(1,std::memory_order_relaxed)+1;
    if(sequence<=64) {
        try {
            HMODULE owner=nullptr;
            GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                reinterpret_cast<LPCWSTR>(caller),&owner);
            struct ModuleReference {HMODULE value;~ModuleReference(){if(value)FreeLibrary(value);}} reference{owner};
            wchar_t name[32768]{};
            if(owner)GetModuleFileNameW(owner,name,32768);
            Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
            if(SUCCEEDED(result)&&output&&*output)
                reinterpret_cast<IUnknown*>(*output)->QueryInterface(IID_PPV_ARGS(&texture));
            D3D11_TEXTURE2D_DESC desc{};
            if(texture)texture->GetDesc(&desc);
            spdlog::info("Nested GetBuffer #{}: swap=0x{:x}; index={}; IID.Data1=0x{:08x}; callerModule={}; callerRVA=0x{:x}; HRESULT=0x{:08x}; texture={}x{}; ownedRouteArmed={}",
                sequence,reinterpret_cast<std::uintptr_t>(swap),index,
                iid.Data1,
                std::filesystem::path(name).filename().string(),
                owner?reinterpret_cast<std::uintptr_t>(caller)-reinterpret_cast<std::uintptr_t>(owner):0,
                static_cast<std::uint32_t>(result),desc.Width,desc.Height,owned);
        } catch(...) {
            try {spdlog::warn("Nested GetBuffer trace #{} unavailable",sequence);} catch(...) {}
        }
    }
    return result;
}
HRESULT WINAPI swapGetDescTrace(IDXGISwapChain* swap,
    DXGI_SWAP_CHAIN_DESC* output) noexcept {
    auto* state=bufferTrace.load(std::memory_order_acquire);
    if(!state||!state->nextDesc)return E_UNEXPECTED;
    const auto caller=reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    if(!state->ownedArmed.load(std::memory_order_acquire)||!state->route)
        return state->nextDesc(swap,output);
    const auto enbBase=resolveVerifiedEnbBase(*state,caller);
    return state->route->getDescForCaller(caller,enbBase,
        state->enbHash,
        swap,output);
}
void publishEarlySpatialFallback(BufferTraceLease& state,
    IDXGISwapChain* swap) noexcept {
    if(!state.ownedArmed.load(std::memory_order_acquire)||
       state.integrationReady.load(std::memory_order_acquire)||
       !state.route||swap!=state.selectedSwap||!state.earlyDevice||!state.earlyContext)
        return;
    try {
        auto native=acquireNativeFlipTarget(swap,state.earlyDevice.Get(),
            state.route->displayExtent());
        if(const auto error=std::get_if<Error>(&native))
            throw std::runtime_error(error->message);
        auto frame=publishSdrSpatialFallbackToDisplay(state.earlyContext.Get(),
            state.route->sceneTexture(),std::get<NativeFlipTarget>(native).texture.Get());
        if(const auto error=std::get_if<Error>(&frame))
            throw std::runtime_error(error->message);
        const auto count=state.emergencyFrames.fetch_add(1,std::memory_order_relaxed)+1;
        if(count<=3||count%600==0)
            spdlog::warn("Early route emergency spatial publication #{} completed before nested Present; late native-UI integration is not ready",
                count);
    } catch(const std::exception& error) {
        const auto count=state.emergencyFailures.fetch_add(1,std::memory_order_relaxed)+1;
        if(count<=3||count%600==0)
            try {spdlog::error("Early route emergency spatial publication #{} failed: {}",
                count,error.what());}catch(...) {}
    } catch(...) {
        const auto count=state.emergencyFailures.fetch_add(1,std::memory_order_relaxed)+1;
        if(count<=3||count%600==0)
            try {spdlog::error("Early route emergency spatial publication #{} failed",
                count);}catch(...) {}
    }
}
HRESULT WINAPI swapPresentTrace(IDXGISwapChain* swap,UINT interval,UINT flags) noexcept {
    auto* state=bufferTrace.load(std::memory_order_acquire);
    if(!state||!state->nextPresent)return E_UNEXPECTED;
    if(!(flags&DXGI_PRESENT_TEST)) {
        probePostEnbPresentationTarget(swap);
        publishEarlySpatialFallback(*state,swap);
    }
    return state->nextPresent(swap,interval,flags);
}
Result<bool> installSwapGetBufferTrace(IDXGISwapChain* swap) {
    if(!swap||bufferTraceAttempted.test(std::memory_order_acquire))return false;
    auto** table=*reinterpret_cast<void***>(swap);
    HMODULE owner=nullptr;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
        reinterpret_cast<LPCWSTR>(table),&owner))
        return Error{ErrorCode::Unsupported,"Returned swap table has no loaded-module owner"};
    struct ModuleReference {HMODULE value;~ModuleReference(){FreeLibrary(value);}} reference{owner};
    const auto id=identify(owner);
    const auto* sitePtr=findSwapGetBufferSite(id.hash);
    const auto* descPtr=findSwapGetDescSite(id.hash);
    if(!sitePtr||!descPtr)return Error{ErrorCode::Unsupported,
        "Returned swap is not a verified ReShade version"};
    const auto& site=*sitePtr;
    const auto& descSite=*descPtr;
    const auto base=reinterpret_cast<std::uintptr_t>(owner);
    const auto tableAddress=reinterpret_cast<std::uintptr_t>(table);
    if(tableAddress<base||tableAddress-base!=site.tableRva)
        return Error{ErrorCode::Unsupported,"Returned swap is not the verified ReShade table"};
    const auto mapped=snapshotModule(owner,site.imageSize);
    const auto& swapProfile=id.hash==reshade680SwapProfile().hash?
        reshade680SwapProfile():reshade673SwapProfile();
    const auto swapValidated=validateSwapTable(mapped,base,id.hash,id.size,
        static_cast<std::uint32_t>(tableAddress-base),swapProfile);
    if(const auto error=std::get_if<Error>(&swapValidated))return *error;
    const auto validated=validateOwnedRouteSite(mapped,base,id.hash,id.size,
        static_cast<std::uint32_t>(tableAddress-base),site);
    if(const auto error=std::get_if<Error>(&validated))return *error;
    const auto descValidated=validateOwnedRouteSite(mapped,base,id.hash,id.size,
        static_cast<std::uint32_t>(tableAddress-base),descSite);
    if(const auto error=std::get_if<Error>(&descValidated))return *error;
    if(bufferTraceAttempted.test_and_set(std::memory_order_acq_rel))return false;
    auto pending=std::make_unique<BufferTraceLease>();
    if(const auto* factory=factoryTrace.load(std::memory_order_acquire);
       factory&&factory->exactEnbOwner)
        pending->enbHash=identify(factory->exactEnbOwner).hash;
    pending->next=reinterpret_cast<SwapGetBufferFn>(base+site.methodRva);
    pending->nextDesc=reinterpret_cast<SwapGetDescFn>(base+descSite.methodRva);
    pending->nextPresent=reinterpret_cast<PresentFn>(base+swapProfile.methods[1].rva);
    pending->selectedSwap=swap;
    HMODULE pinnedSelf=nullptr,pinnedOwner=nullptr;
    constexpr DWORD pin=GET_MODULE_HANDLE_EX_FLAG_PIN|GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS;
    if(!GetModuleHandleExW(pin,reinterpret_cast<LPCWSTR>(&swapGetBufferTrace),&pinnedSelf)||
       !GetModuleHandleExW(pin,reinterpret_cast<LPCWSTR>(table),&pinnedOwner))
        return Error{ErrorCode::Unavailable,"Cannot pin nested swap trace lifetimes"};
    auto* published=pending.release();
    bufferTrace.store(published,std::memory_order_release);
    const auto applied=published->getBufferPatch.apply(table+site.slot,
        reinterpret_cast<void*>(published->next),reinterpret_cast<void*>(&swapGetBufferTrace));
    if(const auto error=std::get_if<Error>(&applied))return *error;
    const auto described=published->getDescPatch.apply(table+descSite.slot,
        reinterpret_cast<void*>(published->nextDesc),reinterpret_cast<void*>(&swapGetDescTrace));
    if(const auto error=std::get_if<Error>(&described)) {
        const auto restored=published->getBufferPatch.restore();
        if(const auto rollback=std::get_if<Error>(&restored);
           rollback&&rollback->code!=ErrorCode::Conflict)std::terminate();
        return *error;
    }
    const auto present=published->presentPatch.apply(table+swapProfile.methods[1].slot,
        reinterpret_cast<void*>(published->nextPresent),reinterpret_cast<void*>(&swapPresentTrace));
    if(const auto error=std::get_if<Error>(&present)) {
        const auto descRestored=published->getDescPatch.restore();
        const auto restored=published->getBufferPatch.restore();
        if(const auto rollback=std::get_if<Error>(&descRestored);
           rollback&&rollback->code!=ErrorCode::Conflict)std::terminate();
        if(const auto rollback=std::get_if<Error>(&restored);
           rollback&&rollback->code!=ErrorCode::Conflict)std::terminate();
        return *error;
    }
    spdlog::info("Installed {} plus {} and verified nested Present stage trace: tableRVA=0x{:x}; slots={},{},{}; pass-through until early scene publication",
        site.id,descSite.id,site.tableRva,site.slot,descSite.slot,
        swapProfile.methods[1].slot);
    return true;
}
Result<bool> validateEarlyEnbUiContract(HMODULE module) {
    if(!module)return Error{ErrorCode::Unsupported,
        "Exact ENB creation owner is unavailable"};
    const auto identity=identify(module);
    const auto sites=findEnbContextSites(identity.hash);
    if(!sites.om)return Error{ErrorCode::Unsupported,"ENB creation owner identity differs"};
    const auto& om=*sites.om;
    if(identity.hash!=om.moduleSha256||identity.size!=om.fileSize)
        return Error{ErrorCode::Unsupported,"ENB creation owner identity differs"};
    const auto base=reinterpret_cast<std::uintptr_t>(module);
    const auto image=snapshotModule(module,om.imageSize);
    for(const auto* site:{sites.om,sites.viewport,sites.psResources}) {
        const auto checked=validateOwnedRouteSite(image,base,identity.hash,
            identity.size,site->tableRva,*site);
        if(const auto error=std::get_if<Error>(&checked))return *error;
    }
    return true;
}
void prepareEarlyOwnedScene(IUnknown* creationDevice,
    const DXGI_SWAP_CHAIN_DESC* requested,IDXGISwapChain* swap) noexcept {
#ifdef RK_WITH_NGX
    auto* trace=bufferTrace.load(std::memory_order_acquire);
    if(!trace||trace->selectedSwap!=swap||!trace->next||!trace->nextDesc||
       trace->ownedArmed.load(std::memory_order_acquire)||
       !rectHook.load(std::memory_order_acquire)||!creationDevice||!requested)return;
    try {
        DXGI_SWAP_CHAIN_DESC desc{};
        const auto described=trace->nextDesc(swap,&desc);
        if(FAILED(described))throw std::runtime_error("Native nested description unavailable");
        const Extent display{desc.BufferDesc.Width,desc.BufferDesc.Height};
        if(requested->BufferDesc.Width&&requested->BufferDesc.Height&&
           (requested->BufferDesc.Width!=display.width||
            requested->BufferDesc.Height!=display.height))
            throw std::runtime_error("Requested and returned nested dimensions differ");
        auto planned=planWorldOwnedScene(display);
        if(const auto error=std::get_if<Error>(&planned))
            throw std::runtime_error(error->message);
        Microsoft::WRL::ComPtr<ID3D11Device> device;
        if(FAILED(creationDevice->QueryInterface(IID_PPV_ARGS(&device)))||!device)
            throw std::runtime_error("Nested creation device is not D3D11");
        auto surface=createReducedSdrSurface(device.Get(),display,std::get<Extent>(planned));
        if(const auto error=std::get_if<Error>(&surface))
            throw std::runtime_error(error->message);
        auto route=std::make_unique<OwnedSwapBufferRoute>();
        if(FAILED(route->configure(swap,trace->next,trace->nextDesc,
            std::move(std::get<ReducedSdrSurface>(surface)),1)))
            throw std::runtime_error("Verified nested swap rejected early scene");
        const auto render=route->renderExtent();
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> immediate;
        device->GetImmediateContext(immediate.GetAddressOf());
        if(!immediate)throw std::runtime_error("Nested immediate context is unavailable");
        trace->gameBase=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        trace->gameHash=std::string(skyrim1170CreationProfile().gameSha256);
        trace->earlyDevice=device;
        trace->earlyContext=std::move(immediate);
        trace->route=std::move(route);
        trace->ownedArmed.store(true,std::memory_order_release);
        try {spdlog::info("Early owned scene published before ENB GetDesc/GetBuffer: render={}x{} display={}x{}; exact ENB and Skyrim consumers selected",
            render.width,render.height,display.width,display.height);}catch(...) {}
    } catch(const std::exception& error) {
        try {spdlog::warn("Early owned scene unavailable; nested buffer and description remain native: {}",error.what());}catch(...) {}
    } catch(...) {
        try {spdlog::warn("Early owned scene unavailable; nested buffer and description remain native");}catch(...) {}
    }
#else
    (void)creationDevice;(void)requested;(void)swap;
#endif
}
void factoryCreated(IDXGIFactory* factory,IUnknown* device,
    const DXGI_SWAP_CHAIN_DESC* requested,IDXGISwapChain* swap,HRESULT result,
    void* context) noexcept {
    auto* state=static_cast<FactoryTraceLease*>(context);
    const auto sequence=state->calls.fetch_add(1,std::memory_order_relaxed)+1;
    if(sequence>4)return;
    try {
        spdlog::info("Nested factory CreateSwapChain #{}: factory=0x{:x}; adapterParentMatch={}; device=0x{:x}; requested={}x{}; result=0x{:08x}; returnedSwap=0x{:x}; thread={}; pass-through",
            sequence,reinterpret_cast<std::uintptr_t>(factory),
            factory==state->target,
            reinterpret_cast<std::uintptr_t>(device),
            requested?requested->BufferDesc.Width:0,requested?requested->BufferDesc.Height:0,
            static_cast<std::uint32_t>(result),reinterpret_cast<std::uintptr_t>(swap),
            GetCurrentThreadId());
        if(FAILED(result)||!swap)return;
        if(!isOwnedSceneFactoryCandidate(factory,state->target,requested)) {
            spdlog::info("Nested swap #{} ignored: factory or native SDR creation contract differs",
                sequence);
            return;
        }
        if(const auto enb=validateEarlyEnbUiContract(state->exactEnbOwner);
           const auto error=std::get_if<Error>(&enb)) {
            spdlog::warn("Nested swap #{} remains native: {}",sequence,error->message);
            return;
        }
        auto** table=*reinterpret_cast<void***>(swap);
        const auto method=std::atomic_ref<void*>(table[9]).load(std::memory_order_acquire);
        HMODULE owner=nullptr,methodOwner=nullptr;
        constexpr DWORD flags=GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS;
        GetModuleHandleExW(flags,reinterpret_cast<LPCWSTR>(table),&owner);
        GetModuleHandleExW(flags,reinterpret_cast<LPCWSTR>(method),&methodOwner);
        struct ModuleRefs {HMODULE a,b;~ModuleRefs(){if(a)FreeLibrary(a);if(b)FreeLibrary(b);}} refs{owner,methodOwner};
        const auto tableId=owner?identify(owner):FileIdentity{"unowned",0};
        const auto methodId=methodOwner==owner?tableId:
            methodOwner?identify(methodOwner):FileIdentity{"unowned",0};
        DXGI_SWAP_CHAIN_DESC desc{};
        const auto got=swap->GetDesc(&desc);
        spdlog::info("Nested swap #{}: tableSHA256={}; tableSize={}; tableRVA=0x{:x}; GetBufferOwnerSHA256={}; methodRVA=0x{:x}; GetDesc=0x{:08x} {}x{}; read-only",
            sequence,tableId.hash,tableId.size,
            owner?reinterpret_cast<std::uintptr_t>(table)-reinterpret_cast<std::uintptr_t>(owner):0,
            methodId.hash,methodOwner?reinterpret_cast<std::uintptr_t>(method)-
                reinterpret_cast<std::uintptr_t>(methodOwner):0,
            static_cast<std::uint32_t>(got),desc.BufferDesc.Width,desc.BufferDesc.Height);
        const auto traced=installSwapGetBufferTrace(swap);
        if(const auto error=std::get_if<Error>(&traced))
            spdlog::warn("Nested GetBuffer trace not installed: {}",error->message);
        else if(std::get<bool>(traced))prepareEarlyOwnedScene(device,requested,swap);
    } catch(const std::exception& error) {
        try {spdlog::warn("Nested factory trace unavailable: {}",error.what());} catch(...) {}
    } catch(...) {
        try {spdlog::warn("Nested factory trace unavailable");} catch(...) {}
    }
}
HRESULT WINAPI factoryCreateProxy(IDXGIFactory* factory,IUnknown* device,
    DXGI_SWAP_CHAIN_DESC* requested,IDXGISwapChain** swap) noexcept {
    auto* state=factoryTrace.load(std::memory_order_acquire);
    if(!state||!state->next)return E_UNEXPECTED;
    return observeFactoryCreate(state->next,factory,device,requested,swap,
        &factoryCreated,state);
}
Result<bool> installFactoryCreationTrace(IDXGIAdapter* adapter) {
    auto* observer=lease.load(std::memory_order_acquire);
    if(!observer||!observer->exactEnbOwner)return false;
    const auto enbSites=findEnbContextSites(identify(observer->exactEnbOwner).hash);
    if(!enbSites.om||patchDisabled(observer->disabledPatchIds,enbSites.om->id)||
       patchDisabled(observer->disabledPatchIds,enbSites.viewport->id)||
       patchDisabled(observer->disabledPatchIds,enbSites.psResources->id))return false;
    if(factoryTraceAttempted.test_and_set(std::memory_order_acq_rel))return false;
    if(!adapter)return Error{ErrorCode::Unavailable,"No adapter for early factory trace"};
    Microsoft::WRL::ComPtr<IDXGIFactory> factory;
    if(FAILED(adapter->GetParent(IID_PPV_ARGS(&factory)))||!factory)
        return Error{ErrorCode::Unavailable,"Adapter parent factory unavailable"};
    auto** table=*reinterpret_cast<void***>(factory.Get());
    HMODULE owner=nullptr;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
        reinterpret_cast<LPCWSTR>(table),&owner))
        return Error{ErrorCode::Unsupported,"Factory table has no loaded-module owner"};
    struct ModuleReference {HMODULE value;~ModuleReference(){FreeLibrary(value);}} reference{owner};
    const auto id=identify(owner);
    const auto* sitePtr=findFactoryCreateSite(id.hash);
    if(!sitePtr)return Error{ErrorCode::Unsupported,
        "Factory owner is not a verified ReShade version"};
    const auto& site=*sitePtr;
    const auto base=reinterpret_cast<std::uintptr_t>(owner);
    const auto tableAddress=reinterpret_cast<std::uintptr_t>(table);
    if(tableAddress<base||tableAddress-base!=site.tableRva)
        return Error{ErrorCode::Unsupported,"Factory table is not the verified ReShade site"};
    const auto mapped=snapshotModule(owner,site.imageSize);
    const auto validated=validateOwnedRouteSite(mapped,base,id.hash,id.size,
        static_cast<std::uint32_t>(tableAddress-base),site);
    if(const auto error=std::get_if<Error>(&validated))return *error;
    auto pending=std::make_unique<FactoryTraceLease>();
    pending->next=reinterpret_cast<FactoryCreateFn>(base+site.methodRva);
    pending->target=factory.Get();
    pending->exactEnbOwner=observer->exactEnbOwner;
    HMODULE pinnedSelf=nullptr,pinnedOwner=nullptr;
    constexpr DWORD pin=GET_MODULE_HANDLE_EX_FLAG_PIN|GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS;
    if(!GetModuleHandleExW(pin,reinterpret_cast<LPCWSTR>(&factoryCreateProxy),&pinnedSelf)||
       !GetModuleHandleExW(pin,reinterpret_cast<LPCWSTR>(table),&pinnedOwner))
        return Error{ErrorCode::Unavailable,"Cannot pin factory callback lifetimes"};
    auto* published=pending.release();
    factoryTrace.store(published,std::memory_order_release);
    const auto applied=published->patch.apply(table+site.slot,
        reinterpret_cast<void*>(published->next),reinterpret_cast<void*>(&factoryCreateProxy));
    if(const auto error=std::get_if<Error>(&applied))return *error;
    spdlog::info("Installed {}: tableRVA=0x{:x}; slot={}; downstreamRVA=0x{:x}; pass-through nested creation trace",
        site.id,site.tableRva,site.slot,site.methodRva);
    return true;
}
struct SwapLease {
    std::string_view profileId;
    std::array<PointerPatch,5> patches;
    std::array<void*,5> originals{};
    std::atomic<std::uint64_t> presents{0}, tests{0}, occluded{0}, failed{0}, resizes{0};
};
std::atomic<SwapLease*> swapLease{nullptr};
std::mutex swapInstallMutex;
void retireOwnedSceneForResize(std::uintptr_t swap) noexcept {
    auto* trace=bufferTrace.load(std::memory_order_acquire);
    if(!trace||reinterpret_cast<std::uintptr_t>(trace->outerSwap)!=swap||
       !trace->ownedArmed.exchange(false,std::memory_order_acq_rel))return;
    trace->integrationReady.store(false,std::memory_order_release);
    const auto owner=ownedFrameThread.load(std::memory_order_acquire);
    if(resizeNeedsOwnerThread(owner,GetCurrentThreadId())) {
        trace->cleanupPending.store(true,std::memory_order_release);
        try {spdlog::warn("Owned scene routing stopped for cross-thread ResizeBuffers; resize rejected until UI cleanup runs on the render thread");}
        catch(...) {}
        return;
    }
    ownedDomain.suspend();
    releasePreparedUiHook(true);
    try {spdlog::warn("Owned scene routing stopped before ResizeBuffers; old scene retained for outstanding references and native queries restored until next launch");}
    catch(...) {}
}
void drainOwnedResizeCleanup() noexcept {
    auto* trace=bufferTrace.load(std::memory_order_acquire);
    if(!trace||!trace->cleanupPending.load(std::memory_order_acquire))return;
    const auto owner=ownedFrameThread.load(std::memory_order_acquire);
    if(owner&&owner!=GetCurrentThreadId())return;
    ownedDomain.suspend();
    releasePreparedUiHook(true);
    trace->cleanupPending.store(false,std::memory_order_release);
    try {spdlog::info("Deferred owned resize cleanup completed on render thread");}
    catch(...) {}
}
void swapObserved(const SwapEvent& event) {
    auto* state=swapLease.load(std::memory_order_acquire);
    if(!state)return;
    if(event.call==SwapCall::Release) {
        if(!event.before&&!event.references)
            spdlog::info("Swap Release returned zero: object=0x{:x}; process call totals: presents={}; tests={}; occluded={}; failed={}; resizes={}; no retained COM/backbuffer resources",
                event.object,state->presents.load(),state->tests.load(),state->occluded.load(),state->failed.load(),state->resizes.load());
        return;
    }
    if(event.call==SwapCall::Resize||event.call==SwapCall::Resize1) {
        if(event.before) {
            state->resizes.fetch_add(1);
            retireOwnedSceneForResize(event.object);
        }
        spdlog::info("Swap {} {}: object=0x{:x}; requested={}x{}; buffers={}; format={}; flags=0x{:x}; HRESULT=0x{:08x}; thread={}",
            event.call==SwapCall::Resize?"ResizeBuffers":"ResizeBuffers1",event.before?"begin":"end",
            event.object,event.width,event.height,event.buffers,static_cast<unsigned>(event.format),event.flags,
            static_cast<std::uint32_t>(event.result),GetCurrentThreadId());
        return;
    }
    if(event.before) {
        drainOwnedResizeCleanup();
        if(!(event.flags&DXGI_PRESENT_TEST)&&frameProbeBoundary(state->profileId,event.call)) {
            probePresentationTargets(reinterpret_cast<IDXGISwapChain*>(event.object));
            try { probePresentCandidates(reinterpret_cast<IDXGISwapChain*>(event.object)); }
            catch(const std::exception& error) { spdlog::warn("Candidate probe aborted: {}",error.what()); }
            if(const auto status=worldDiagnosticsSnapshot(
                reinterpret_cast<IDXGISwapChain*>(event.object)))
                drawDiagnosticsMenu(reinterpret_cast<IDXGISwapChain*>(event.object),*status);
        }
        return;
    }
    const auto count=state->presents.fetch_add(1)+1;
    if(event.flags&DXGI_PRESENT_TEST)state->tests.fetch_add(1);
    if(event.result==DXGI_STATUS_OCCLUDED)state->occluded.fetch_add(1);
    if(FAILED(event.result))state->failed.fetch_add(1);
    if(count<=3||count%600==0)
        spdlog::info("Swap {} observation #{}: object=0x{:x}; interval={}; flags=0x{:x}; HRESULT=0x{:08x}; testCalls={}; occluded={}; failed={}; worldForwarded={}; thread={}; observation only",
            event.call==SwapCall::Present?"Present":"Present1",count,event.object,event.interval,event.flags,
            static_cast<std::uint32_t>(event.result),state->tests.load(),state->occluded.load(),state->failed.load(),
            worldDrawForwardedCalls(),GetCurrentThreadId());
}
ULONG WINAPI swapRelease(IUnknown* s) noexcept {
    const auto* state=swapLease.load(std::memory_order_acquire);
    return observeRelease(reinterpret_cast<ReleaseFn>(state->originals[0]),s,&swapObserved);
}
HRESULT WINAPI swapPresent(IDXGISwapChain* s,UINT interval,UINT flags) noexcept {
    const auto* state=swapLease.load(std::memory_order_acquire);
    return observePresent(reinterpret_cast<PresentFn>(state->originals[1]),s,interval,flags,&swapObserved);
}
bool prepareOwnedResize(std::uintptr_t swap) noexcept {
    auto* trace=bufferTrace.load(std::memory_order_acquire);
    if(!trace||reinterpret_cast<std::uintptr_t>(trace->outerSwap)!=swap)return false;
    const auto owner=ownedFrameThread.load(std::memory_order_acquire);
    const auto foreign=resizeNeedsOwnerThread(owner,GetCurrentThreadId());
    if(trace->cleanupPending.load(std::memory_order_acquire)) {
        if(foreign)return true;
        drainOwnedResizeCleanup();
    }
    return trace->ownedArmed.load(std::memory_order_acquire)&&foreign;
}
HRESULT rejectOwnedResize(SwapCall call,std::uintptr_t object,UINT count,
    UINT width,UINT height,DXGI_FORMAT format,UINT flags) noexcept {
    SwapEvent event{call,true,object,DXGI_ERROR_INVALID_CALL,0,0,flags,
        count,width,height,format};
    swapObserved(event);
    event.before=false;
    swapObserved(event);
    return DXGI_ERROR_INVALID_CALL;
}
HRESULT WINAPI swapResize(IDXGISwapChain* s,UINT count,UINT width,UINT height,DXGI_FORMAT format,UINT flags) noexcept {
    const auto* state=swapLease.load(std::memory_order_acquire);
    if(prepareOwnedResize(reinterpret_cast<std::uintptr_t>(s)))
        return rejectOwnedResize(SwapCall::Resize,reinterpret_cast<std::uintptr_t>(s),
            count,width,height,format,flags);
    return observeResize(reinterpret_cast<ResizeFn>(state->originals[2]),s,count,width,height,format,flags,&swapObserved);
}
HRESULT WINAPI swapPresent1(IDXGISwapChain1* s,UINT interval,UINT flags,const DXGI_PRESENT_PARAMETERS* params) noexcept {
    const auto* state=swapLease.load(std::memory_order_acquire);
    return observePresent1(reinterpret_cast<Present1Fn>(state->originals[3]),s,interval,flags,params,&swapObserved);
}
HRESULT WINAPI swapResize1(IDXGISwapChain3* s,UINT count,UINT width,UINT height,DXGI_FORMAT format,UINT flags,const UINT* nodes,IUnknown* const* queues) noexcept {
    const auto* state=swapLease.load(std::memory_order_acquire);
    if(prepareOwnedResize(reinterpret_cast<std::uintptr_t>(s)))
        return rejectOwnedResize(SwapCall::Resize1,reinterpret_cast<std::uintptr_t>(s),
            count,width,height,format,flags);
    return observeResize1(reinterpret_cast<Resize1Fn>(state->originals[4]),s,count,width,height,format,flags,nodes,queues,&swapObserved);
}
void logSwapTableOwners(void** table,HMODULE tableOwner,const FileIdentity& tableIdentity) {
    const auto base=reinterpret_cast<std::uintptr_t>(tableOwner);
    spdlog::info("Swap table provenance: ownerSHA256={}; ownerFileSize={}; tableRVA=0x{:x}",
        tableIdentity.hash,tableIdentity.size,reinterpret_cast<std::uintptr_t>(table)-base);
    // Only slots guaranteed by the already-validated IDXGISwapChain interface.
    // Do not assume extended interfaces or read wrapper-private object fields.
    for(const unsigned slot:{2U,8U,13U}) {
        const auto method=std::atomic_ref<void*>(table[slot]).load(std::memory_order_acquire);
        HMODULE owner=nullptr;
        if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<LPCWSTR>(method),&owner)) {
            spdlog::info("Swap method provenance: slot={}; no loaded-module owner",slot);continue;
        }
        struct Reference { HMODULE value;~Reference(){FreeLibrary(value);} } reference{owner};
        const auto identity=owner==tableOwner?tableIdentity:identify(owner);
        spdlog::info("Swap method provenance: slot={}; ownerSHA256={}; ownerFileSize={}; methodRVA=0x{:x}",
            slot,identity.hash,identity.size,reinterpret_cast<std::uintptr_t>(method)-reinterpret_cast<std::uintptr_t>(owner));
    }
}
Result<bool> installSwapObserver(IDXGISwapChain* swap,std::string_view disabledPatchIds) {
    std::scoped_lock lock(swapInstallMutex);
    if(swapLease.load())return false; // A process-lifetime installation is attempted only once after preparation.
    auto** table=*reinterpret_cast<void***>(swap);
    HMODULE owner=nullptr;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<LPCWSTR>(table),&owner))
        return Error{ErrorCode::Unsupported,"Swap table is not owned by a loaded module"};
    struct Reference { HMODULE value;~Reference(){FreeLibrary(value);} } reference{owner};
    const auto identity=identify(owner);
    logSwapTableOwners(table,owner,identity);
    const auto base=reinterpret_cast<std::uintptr_t>(owner),address=reinterpret_cast<std::uintptr_t>(table);
    const auto* selected=address>=base?findSwapProfile(identity.hash,address-base):nullptr;
    if(!selected||identity.size!=selected->fileSize)
        return Error{ErrorCode::Unsupported,"Unknown swap table identity/location; Present and resize unchanged"};
    const auto& profile=*selected;
    if(patchDisabled(disabledPatchIds,profile.id)) { spdlog::info("Swap observation disabled by ID: {}",profile.id);return false; }
    const auto mapped=snapshotModule(owner,profile.imageSize);
    const auto checked=validateSwapTable(mapped,base,identity.hash,identity.size,static_cast<std::uint32_t>(address-base),profile);
    if(const auto error=std::get_if<Error>(&checked))return *error;
    const std::array<void*,5> replacements{reinterpret_cast<void*>(&swapRelease),reinterpret_cast<void*>(&swapPresent),
        reinterpret_cast<void*>(&swapResize),reinterpret_cast<void*>(&swapPresent1),reinterpret_cast<void*>(&swapResize1)};
    auto pending=std::make_unique<SwapLease>();
    pending->profileId=profile.id;
    for(std::size_t i=0;i<profile.methodCount;++i)pending->originals[i]=reinterpret_cast<void*>(base+profile.methods[i].rva);
    HMODULE pinnedSelf=nullptr,pinnedOwner=nullptr;
    constexpr DWORD flags=GET_MODULE_HANDLE_EX_FLAG_PIN|GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS;
    if(!GetModuleHandleExW(flags,reinterpret_cast<LPCWSTR>(&swapPresent),&pinnedSelf)||
       !GetModuleHandleExW(flags,reinterpret_cast<LPCWSTR>(table),&pinnedOwner))return Error{ErrorCode::Unavailable,"Cannot pin swap observer lifetimes"};
    auto* published=pending.release();swapLease.store(published,std::memory_order_release);
    for(std::size_t i=0;i<profile.methodCount;++i) {
        const auto result=published->patches[i].apply(table+profile.methods[i].slot,published->originals[i],replacements[i]);
        if(const auto error=std::get_if<Error>(&result)) {
            // All callbacks independently forward; atomic restoration is safe for
            // in-flight calls because state and both modules remain pinned.
            for(std::size_t j=i;j>0;--j) {
                const auto restored=published->patches[j-1].restore();
                if(const auto problem=std::get_if<Error>(&restored);problem&&problem->code!=ErrorCode::Conflict)std::terminate();
            }
            return *error;
        }
    }
    spdlog::info("Installed {}: vtable RVA=0x{:x}; methods={}; process-lifetime pass-through; no retained resources",profile.id,profile.tableRva,profile.methodCount);
    return true;
}
void prepareOwnedSceneAtCreation(const DeviceCreationArgs& args,
    const RendererSnapshot& snapshot,std::string_view disabledPatchIds) noexcept {
#ifdef RK_WITH_NGX
    auto* trace=bufferTrace.load(std::memory_order_acquire);
    if(!trace||!trace->selectedSwap||!trace->route||
       !trace->ownedArmed.load(std::memory_order_acquire)||
       trace->integrationReady.load(std::memory_order_acquire)||
       !rectHook.load(std::memory_order_acquire)||
       !args.device||!*args.device||!args.context||!*args.context||
       !args.swapChain||!*args.swapChain)return;
    const Extent display{snapshot.width,snapshot.height};
    auto* device=*args.device;
    auto* context=*args.context;
    auto* swap=*args.swapChain;
    trace->outerSwap=swap;
    bool sessionAttempted=false;
    bool committed=false;
    try {
        Microsoft::WRL::ComPtr<ID3D11Device> contextDevice,swapDevice;
        context->GetDevice(contextDevice.GetAddressOf());
        if(FAILED(swap->GetDevice(IID_PPV_ARGS(swapDevice.GetAddressOf())))||
           !contextDevice||!swapDevice||
           !trace->route->belongsToDevice(device)||
           !trace->route->belongsToDevice(contextDevice.Get())||
           !trace->route->belongsToDevice(swapDevice.Get()))
            throw std::runtime_error("Early scene, outer swap and immediate context use different D3D11 devices");
        auto native=acquireNativeFlipTarget(swap,device,display);
        if(const auto error=std::get_if<Error>(&native)) {
            spdlog::warn("Owned scene preparation deferred: {}",error->message);return;
        }
        sessionAttempted=true;
        auto plan=prepareWorldOwnedSrPlan(device,context,display);
        if(const auto error=std::get_if<Error>(&plan)) {
            spdlog::warn("Owned DLSS plan unavailable; early scene will use display-sized spatial fallback: {}",
                error->message);
            useWorldOwnedSpatialFallback();
            const auto stopped=abandonWorldOwnedSrPlan(context);
            if(const auto problem=std::get_if<Error>(&stopped))
                spdlog::warn("Owned DLSS preflight teardown unavailable: {}",problem->message);
            sessionAttempted=false;
        }
        const auto render=trace->route->renderExtent();
        if(const auto provider=std::get_if<Extent>(&plan);
           provider&&(provider->width!=render.width||provider->height!=render.height)) {
            spdlog::warn("Owned DLSS plan {}x{} differs from early scene {}x{}; display-sized spatial fallback selected",
                provider->width,provider->height,render.width,render.height);
            useWorldOwnedSpatialFallback();
            const auto stopped=abandonWorldOwnedSrPlan(context);
            if(const auto problem=std::get_if<Error>(&stopped))
                spdlog::warn("Owned DLSS mismatch teardown unavailable: {}",problem->message);
            sessionAttempted=false;
        }
        if(display.width!=trace->route->displayExtent().width||
           display.height!=trace->route->displayExtent().height)
            throw std::runtime_error("Outer display differs from early owned scene extent");
        if(!ownedDomain.configure({render,display,1}))
            throw std::runtime_error("Owned world phase plan rejected");
        const auto mipBias=worldOwnedMipBias(render,display);
        if(const auto error=std::get_if<Error>(&mipBias))
            throw std::runtime_error(error->message);
        const auto ui=installOwnedUiContextHooks(context,ownedDomain,
            trace->route->sceneTexture(),std::get<NativeFlipTarget>(native).view.Get(),
            std::get<float>(mipBias),disabledPatchIds);
        if(const auto error=std::get_if<Error>(&ui))
            throw std::runtime_error(error->message);
        if(!std::get<bool>(ui))
            throw std::runtime_error("Verified ENB UI context hook disabled");
        DXGI_SWAP_CHAIN_DESC chain{};
        if(FAILED(swap->GetDesc(&chain))||!chain.OutputWindow)
            throw std::runtime_error("Owned game window unavailable");
        trace->gameBase=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        trace->gameHash=std::string(skyrim1170CreationProfile().gameSha256);
        const auto rect=activateOwnedRendererRect(ownedDomain,chain.OutputWindow);
        if(const auto error=std::get_if<Error>(&rect))
            throw std::runtime_error(error->message);
        if(!std::get<bool>(rect))
            throw std::runtime_error("Owned renderer rectangle was already activated");
        trace->integrationReady.store(true,std::memory_order_release);
        ownedEverActive.store(true,std::memory_order_release);
        committed=true;
        try {spdlog::info("Early owned scene integration completed before Skyrim view-cache GetBuffer: render={}x{} display={}x{}; native flip index={}; provider/UI/rect hooks prepared",
            render.width,render.height,display.width,display.height,
            std::get<NativeFlipTarget>(native).index);}catch(...) {}
    } catch(const std::exception& error) {
        try {spdlog::warn("Late owned scene integration failed after early publication; stable early resource remains selected until restart: {}",error.what());}
        catch(...) {}
        if(!committed)releasePreparedUiHook();
        if(sessionAttempted&&!committed) {
            const auto stopped=abandonWorldOwnedSrPlan(context);
            if(const auto problem=std::get_if<Error>(&stopped))
                try {spdlog::warn("Owned scene NGX teardown failed: {}",problem->message);}catch(...) {}
        }
    } catch(...) {
        try {spdlog::warn("Late owned scene integration failed after early publication; stable early resource remains selected until restart");}catch(...) {}
        if(!committed)releasePreparedUiHook();
        if(sessionAttempted&&!committed)abandonWorldOwnedSrPlan(context);
    }
#else
    (void)args;(void)snapshot;(void)disabledPatchIds;
#endif
}
void observed(const DeviceCreationArgs& args,HRESULT result) {
    auto* state=lease.load(std::memory_order_acquire);
    if (!state) return;
    const auto sequence=state->observations.fetch_add(1)+1;
    spdlog::info("Renderer observation #{}: original creation HRESULT=0x{:08x}; thread={}",
        sequence,static_cast<std::uint32_t>(result),GetCurrentThreadId());
    auto captured=captureRendererSnapshot(args,result);
    if (const auto error=std::get_if<Error>(&captured)) {
        spdlog::warn("Renderer observation unavailable: {}",error->message); return;
    }
    const auto& snapshot=std::get<RendererSnapshot>(captured);
    bindFrameProbe(args);
    bindWorldDrawRenderer(*args.device,args.context?*args.context:nullptr,*args.swapChain);
    bindDrsDisplay(snapshot.width,snapshot.height);
    spdlog::info("Creation pointer provenance: device=0x{:x}; context=0x{:x}; swap=0x{:x}",
        reinterpret_cast<std::uintptr_t>(*args.device),args.context?reinterpret_cast<std::uintptr_t>(*args.context):0,
        reinterpret_cast<std::uintptr_t>(*args.swapChain));
    spdlog::info("Actual render adapter: {}; vendor=0x{:04x}; device=0x{:04x}; LUID={:08x}:{:08x}; featureLevel=0x{:x}",
        snapshot.adapter,snapshot.vendorId,snapshot.deviceId,static_cast<std::uint32_t>(snapshot.luidHigh),
        snapshot.luidLow,static_cast<unsigned>(snapshot.featureLevel));
    spdlog::info("Actual swap chain: {}x{}; format={}; buffers={}; samples={}; swapEffect={}; windowed={}; deviceFlags=0x{:x}",
        snapshot.width,snapshot.height,static_cast<unsigned>(snapshot.format),snapshot.bufferCount,snapshot.sampleCount,
        static_cast<unsigned>(snapshot.swapEffect),snapshot.windowed,snapshot.deviceFlags);
    {
        const auto hooked=installSwapObserver(*args.swapChain,state->disabledPatchIds);
        if(const auto error=std::get_if<Error>(&hooked))spdlog::warn("Swap observation not installed: {}",error->message);
    }
    if(sequence==1)prepareOwnedSceneAtCreation(args,snapshot,state->disabledPatchIds);
    if (state->notification) state->notification(snapshot);
}
HRESULT WINAPI createProxy(IDXGIAdapter* adapter,D3D_DRIVER_TYPE driverType,HMODULE software,UINT flags,
    const D3D_FEATURE_LEVEL* levels,UINT levelCount,UINT sdkVersion,const DXGI_SWAP_CHAIN_DESC* swapDesc,
    IDXGISwapChain** swapChain,ID3D11Device** device,D3D_FEATURE_LEVEL* featureLevel,ID3D11DeviceContext** context) noexcept {
    auto* state=lease.load(std::memory_order_acquire);
    if (!state || !state->original) return E_UNEXPECTED;
    logFactoryBeforeCreation(adapter);
    try {
        const auto traced=installFactoryCreationTrace(adapter);
        if(const auto error=std::get_if<Error>(&traced))
            spdlog::warn("Early factory trace not installed: {}",error->message);
    } catch(const std::exception& error) {
        try {spdlog::warn("Early factory trace unavailable: {}",error.what());} catch(...) {}
    } catch(...) {
        try {spdlog::warn("Early factory trace unavailable");} catch(...) {}
    }
    const DeviceCreationArgs args{adapter,driverType,software,flags,levels,levelCount,sdkVersion,
        swapDesc,swapChain,device,featureLevel,context};
    return observeDeviceCreation(state->original,args,&observed);
}
}
Result<bool> installOwnedUiContextHooks(ID3D11DeviceContext* context,
    OwnedSceneDomain& domain,ID3D11Texture2D* reducedScene,
    ID3D11RenderTargetView* nativeTarget,float mipBias,
    std::string_view disabledPatchIds) {
    std::scoped_lock lock(uiInstallMutex);
    if(uiHook.load(std::memory_order_acquire))return false;
    if(!context||context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE||
       !reducedScene||!nativeTarget||!domain.plan().valid())
        return Error{ErrorCode::InvalidInput,"Owned UI context is not prepared"};
    try {
        auto** table=*reinterpret_cast<void***>(context);
        HMODULE owner=nullptr;
        if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
            reinterpret_cast<LPCWSTR>(table),&owner))
            return Error{ErrorCode::Unsupported,"UI context table has no loaded-module owner"};
        struct ModuleReference {HMODULE value;~ModuleReference(){FreeLibrary(value);}} reference{owner};
        const auto id=identify(owner);
        const auto sites=findEnbContextSites(id.hash);
        if(!sites.om)return Error{ErrorCode::Unsupported,
            "UI context owner is not a verified ENB version"};
        const auto& omSite=*sites.om;
        const auto& viewportSite=*sites.viewport;
        const auto& scissorSite=*sites.scissor;
        const auto& psSite=*sites.psResources;
        const auto samplerSites=sites.samplers;
        if(patchDisabled(disabledPatchIds,omSite.id)||
           patchDisabled(disabledPatchIds,viewportSite.id)||
           patchDisabled(disabledPatchIds,scissorSite.id)||
           patchDisabled(disabledPatchIds,psSite.id))return false;
        for(const auto& site:samplerSites)
            if(patchDisabled(disabledPatchIds,site.id))return false;
        const auto base=reinterpret_cast<std::uintptr_t>(owner);
        const auto tableAddress=reinterpret_cast<std::uintptr_t>(table);
        if(tableAddress<base||tableAddress-base!=omSite.tableRva||
            omSite.tableRva!=viewportSite.tableRva||
            omSite.tableRva!=scissorSite.tableRva||
            omSite.tableRva!=psSite.tableRva||samplerSites.size()!=6)
            return Error{ErrorCode::Unsupported,"UI context is not the verified ENB table"};
        for(const auto& site:samplerSites)if(site.tableRva!=omSite.tableRva)
            return Error{ErrorCode::Unsupported,"Sampler context is not the verified ENB table"};
        const auto mapped=snapshotModule(owner,omSite.imageSize);
        for(const auto* site:{&omSite,&viewportSite,&scissorSite,&psSite}) {
            const auto checked=validateOwnedRouteSite(mapped,base,id.hash,id.size,
                static_cast<std::uint32_t>(tableAddress-base),*site);
            if(const auto error=std::get_if<Error>(&checked))return *error;
        }
        for(const auto& site:samplerSites) {
            const auto checked=validateOwnedRouteSite(mapped,base,id.hash,id.size,
                static_cast<std::uint32_t>(tableAddress-base),site);
            if(const auto error=std::get_if<Error>(&checked))return *error;
        }
        auto pending=std::make_unique<UiHookLease>(domain);
        if(validateEnbTargetProbeImage(mapped,id.hash))pending->enbProbeBase=base;
        pending->next={reinterpret_cast<UiContextNext::OM>(base+omSite.methodRva),
            reinterpret_cast<UiContextNext::VP>(base+viewportSite.methodRva),
            reinterpret_cast<UiContextNext::SC>(base+scissorSite.methodRva),
            reinterpret_cast<UiContextNext::PS>(base+psSite.methodRva)};
        for(std::size_t i=0;i<samplerSites.size();++i)
            pending->nextSamplers[i]=reinterpret_cast<UiHookLease::Sampler>(
                base+samplerSites[i].methodRva);
        const auto observationLayout=id.hash==enb505SwapProfile().hash?
            UiObservationLayout::FourPairsThenSceneBind:
            UiObservationLayout::FourPairs;
        const auto configured=pending->redirect.configure(context,GetCurrentThreadId(),
            pending->next,reducedScene,nativeTarget,observationLayout);
        if(FAILED(configured))
            return Error{ErrorCode::Conflict,"Owned UI redirector rejected the ENB context or target"};
        if(FAILED(pending->samplerBias.configure(context,mipBias)))
            return Error{ErrorCode::Conflict,"Owned sampler-bias cache rejected the ENB context or value"};
        HMODULE pinnedSelf=nullptr,pinnedOwner=nullptr;
        constexpr DWORD pin=GET_MODULE_HANDLE_EX_FLAG_PIN|GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS;
        if(!GetModuleHandleExW(pin,reinterpret_cast<LPCWSTR>(&uiOmProxy),&pinnedSelf)||
           !GetModuleHandleExW(pin,reinterpret_cast<LPCWSTR>(table),&pinnedOwner))
            return Error{ErrorCode::Unavailable,"Cannot pin owned UI callback lifetimes"};
        auto* published=pending.release();
        uiHook.store(published,std::memory_order_release);
        const auto om=published->omPatch.apply(table+omSite.slot,
            reinterpret_cast<void*>(published->next.om),reinterpret_cast<void*>(&uiOmProxy));
        if(const auto error=std::get_if<Error>(&om))return *error;
        const auto viewport=published->viewportPatch.apply(table+viewportSite.slot,
            reinterpret_cast<void*>(published->next.viewport),
            reinterpret_cast<void*>(&uiViewportProxy));
        if(const auto error=std::get_if<Error>(&viewport)) {
            const auto restored=published->omPatch.restore();
            if(const auto rollback=std::get_if<Error>(&restored);
               rollback&&rollback->code!=ErrorCode::Conflict)std::terminate();
            return *error;
        }
        const auto scissor=published->scissorPatch.apply(table+scissorSite.slot,
            reinterpret_cast<void*>(published->next.scissor),
            reinterpret_cast<void*>(&uiScissorProxy));
        if(const auto error=std::get_if<Error>(&scissor)) {
            const auto restoredViewport=published->viewportPatch.restore();
            const auto restoredOm=published->omPatch.restore();
            const auto* viewportError=std::get_if<Error>(&restoredViewport);
            const auto* omError=std::get_if<Error>(&restoredOm);
            if((viewportError&&viewportError->code!=ErrorCode::Conflict)||
               (omError&&omError->code!=ErrorCode::Conflict))std::terminate();
            return *error;
        }
        const auto ps=published->psPatch.apply(table+psSite.slot,
            reinterpret_cast<void*>(published->next.ps),
            reinterpret_cast<void*>(&uiPsProxy));
        if(const auto error=std::get_if<Error>(&ps)) {
            const auto restoredScissor=published->scissorPatch.restore();
            const auto restoredViewport=published->viewportPatch.restore();
            const auto restoredOm=published->omPatch.restore();
            const auto* scissorError=std::get_if<Error>(&restoredScissor);
            const auto* viewportError=std::get_if<Error>(&restoredViewport);
            const auto* omError=std::get_if<Error>(&restoredOm);
            if((scissorError&&scissorError->code!=ErrorCode::Conflict)||
               (viewportError&&viewportError->code!=ErrorCode::Conflict)||
               (omError&&omError->code!=ErrorCode::Conflict))std::terminate();
            return *error;
        }
        constexpr std::array replacements{
            &uiSamplerProxy<0>,&uiSamplerProxy<1>,&uiSamplerProxy<2>,
            &uiSamplerProxy<3>,&uiSamplerProxy<4>,&uiSamplerProxy<5>};
        for(std::size_t i=0;i<samplerSites.size();++i) {
            published->samplerSlots[i]=table+samplerSites[i].slot;
            published->samplerProxies[i]=reinterpret_cast<void*>(replacements[i]);
            const auto applied=published->samplerPatches[i].apply(
                published->samplerSlots[i],
                reinterpret_cast<void*>(published->nextSamplers[i]),
                reinterpret_cast<void*>(replacements[i]));
            if(const auto error=std::get_if<Error>(&applied)) {
                const auto restore=[](PointerPatch& patch) {
                    const auto result=patch.restore();
                    if(const auto problem=std::get_if<Error>(&result);
                       problem&&problem->code!=ErrorCode::Conflict)std::terminate();
                };
                for(std::size_t j=i;j>0;--j)restore(published->samplerPatches[j-1]);
                restore(published->psPatch);restore(published->scissorPatch);
                restore(published->viewportPatch);
                restore(published->omPatch);
                return *error;
            }
        }
        published->armed.store(true,std::memory_order_release);
        try {spdlog::info("Installed verified ENB UI context slots {}, {}, {} and {} plus six sampler stages; PS resource route is observation-only; mip bias={}",
            omSite.id,viewportSite.id,scissorSite.id,psSite.id,mipBias);}catch(...) {}
        return true;
    } catch(const std::exception& error) {
        return Error{ErrorCode::Unavailable,
            std::string("Owned UI context preparation failed: ")+error.what()};
    }
}
NativeUiRedirector* ownedUiRedirector() noexcept {
    auto* state=uiHook.load(std::memory_order_acquire);
    if(!state||!state->armed.load(std::memory_order_acquire))return nullptr;
    const auto check=state->samplerOwnershipChecks.fetch_add(1,
        std::memory_order_relaxed)+1;
    if((check&(check-1))==0) {
        unsigned mask{};
        for(std::size_t i=0;i<state->samplerSlots.size();++i)
            if(state->samplerSlots[i]&&
               std::atomic_ref<void*>(*state->samplerSlots[i]).load(
                   std::memory_order_acquire)==state->samplerProxies[i])
                mask|=1u<<i;
        const auto prior=state->lastSamplerOwnershipMask.exchange(mask,
            std::memory_order_relaxed);
        if(mask!=prior||check==1)try {spdlog::info(
            "Owned sampler hook ownership: check={} mask=0x{:02x}/0x3f",
            check,mask);}catch(...) {}
    }
    return &state->redirect;
}
OwnedSceneDomain* activeOwnedSceneDomain() noexcept {
    auto* state=bufferTrace.load(std::memory_order_acquire);
    return state&&state->ownedArmed.load(std::memory_order_acquire)?&ownedDomain:nullptr;
}
ID3D11Texture2D* activeOwnedSceneTexture() noexcept {
    auto* state=bufferTrace.load(std::memory_order_acquire);
    return state&&state->ownedArmed.load(std::memory_order_acquire)?
        state->route->sceneTexture():nullptr;
}
bool ownedScenePreviouslyActive() noexcept {
    return ownedEverActive.load(std::memory_order_acquire);
}
Result<bool> installOwnedRendererRectHook(HMODULE game,
    std::string_view verifiedGameHash,const Settings& settings) {
    constexpr std::string_view id="skyrim1170.renderer-client-rect-v1";
    if(!settings.get<bool>("General.Enabled")||
       settings.get<bool>("General.SafeMode")||
       !settings.get<bool>("Patching.EnableVersionedPatches")||
       !settings.get<bool>("Patching.ExperimentalPatches")||
       patchDisabled(settings.get<Text>("Patching.DisabledPatchIds").value,id))return false;
    if(rectHook.load(std::memory_order_acquire))return true;
    const auto& profile=skyrim1170CreationProfile();
    if(!game||verifiedGameHash!=profile.gameSha256)
        return Error{ErrorCode::Unsupported,"Renderer rectangle executable identity differs"};
    const RipCall6Descriptor descriptor{std::string(id),std::string(profile.gameSha256),
        profile.imageSize,0xe4471b,0x174f928,{0xff,0x15,0x07,0xb2,0x90,0x00}};
    const auto base=reinterpret_cast<std::uintptr_t>(game);
    auto readExact=[](std::uintptr_t address,void* out,std::size_t size) {
        SIZE_T copied{};
        return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(address),
            out,size,&copied)&&copied==size;
    };
    std::array<std::uint8_t,73> caller{};
    std::array<std::uint8_t,6> instruction{};
    void* prior{};
    if(!readExact(base+0xe446d8,caller.data(),caller.size())||
       !readExact(base+descriptor.siteRva,instruction.data(),instruction.size())||
       !readExact(base+descriptor.originalCellRva,&prior,sizeof(prior)))
        return Error{ErrorCode::Io,"Cannot read verified renderer rectangle CALL"};
    if(const auto abi=verifySkyrim1170ClientRectAbi(caller);
       const auto error=std::get_if<Error>(&abi))return *error;
    const auto planned=prepareRipCall6(instruction,verifiedGameHash,
        profile.imageSize,descriptor);
    if(const auto error=std::get_if<Error>(&planned))return *error;
    const auto user32=GetModuleHandleW(L"user32.dll");
    if(!user32||prior!=reinterpret_cast<void*>(GetProcAddress(user32,"GetClientRect")))
        return Error{ErrorCode::Conflict,"Renderer rectangle import no longer targets User32 GetClientRect"};
    const auto& plan=std::get<RipCall6Plan>(planned);
    auto prepared=prepareNearRipCall6Cell(plan,base,
        reinterpret_cast<std::uintptr_t>(&rendererRectProxy));
    if(const auto error=std::get_if<Error>(&prepared))return *error;
    auto pending=std::make_unique<RectHookLease>();
    pending->prior=reinterpret_cast<RendererLogicalSize::Prior>(prior);
    pending->cell=std::make_unique<NearRipCall6Cell>(
        std::move(std::get<NearRipCall6Cell>(prepared)));
    HMODULE pinnedSelf=nullptr,pinnedOwner=nullptr;
    constexpr DWORD pin=GET_MODULE_HANDLE_EX_FLAG_PIN|GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS;
    if(!GetModuleHandleExW(pin,reinterpret_cast<LPCWSTR>(&rendererRectProxy),&pinnedSelf)||
       !GetModuleHandleExW(pin,reinterpret_cast<LPCWSTR>(prior),&pinnedOwner))
        return Error{ErrorCode::Unavailable,"Cannot pin renderer rectangle callback chain"};
    auto* published=pending.release();
    rectHook.store(published,std::memory_order_release);
    const auto applied=applyRipCall6(plan,base,*published->cell,
        CallWriteBoundary::SkyrimStartupBeforeWorldThreads);
    if(const auto error=std::get_if<Error>(&applied)) {
        rectHook.store(nullptr,std::memory_order_release);
        delete published;
        return *error;
    }
    try {spdlog::info("Installed {} at RVA 0x{:x}; User32 forwarding until owned scene activation",
        id,descriptor.siteRva);}catch(...) {}
    return true;
}
Result<bool> activateOwnedRendererRect(OwnedSceneDomain& domain,HWND gameWindow) {
    auto* state=rectHook.load(std::memory_order_acquire);
    if(!state||!gameWindow||!domain.plan().valid())
        return Error{ErrorCode::InvalidInput,"Owned renderer rectangle is not ready"};
    if(state->logical.load(std::memory_order_acquire))return false;
    auto logical=std::make_unique<RendererLogicalSize>(domain,state->prior,
        gameWindow,GetCurrentThreadId());
    state->domain=&domain;state->gameWindow=gameWindow;
    state->logical.store(logical.release(),std::memory_order_release);
    return true;
}
bool rendererObserverArmed() noexcept {
    const auto* state=lease.load(std::memory_order_acquire);
    return state && state->armed.load();
}
Result<bool> installRendererObserver(const Settings& settings,RendererObserved notification) {
    if (!rendererObserverRequested(settings)) {
        spdlog::info("Renderer observation disabled by configuration; experimental opt-in is required"); return false;
    }
    if (const auto* existing=lease.load()) return existing->armed.load();
    try {
        const auto game=GetModuleHandleW(nullptr);
        const auto identity=identify(game);
        const auto& profile=skyrim1170CreationProfile();
        if (identity.hash!=profile.gameSha256 || identity.size!=profile.fileSize)
            return Error{ErrorCode::Unsupported,"Unknown game hash; renderer IAT was not modified"};
        const auto mapped=snapshotModule(game,profile.imageSize);
        const auto checked=validateCreationImport(mapped,identity.hash,identity.size,profile);
        if (const auto error=std::get_if<Error>(&checked)) return *error;
        armFrameProbe(game,identity.hash,settings);
        auto** slot=reinterpret_cast<void**>(reinterpret_cast<std::uint8_t*>(game)+std::get<std::uint32_t>(checked));
        // Read without writing the IAT page. CAS in PointerPatch revalidates the
        // prior owner atomically after all identity/ABI checks are complete.
        static_assert(std::atomic_ref<void*>::is_always_lock_free);
        auto* original=std::atomic_ref<void*>(*slot).load(std::memory_order_acquire);
        if (!original) return Error{ErrorCode::Unavailable,"Unresolved D3D11 creation import"};
        HMODULE owner=nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
            reinterpret_cast<LPCWSTR>(original),&owner)) return Error{ErrorCode::Unsupported,"Creation pointer has no module owner"};
        struct ModuleReference { HMODULE module; ~ModuleReference() { FreeLibrary(module); } } ownerReference{owner};
        const auto ownerIdentity=identify(owner);
        const auto* ownerProfile=creationOwnerProfile(ownerIdentity.hash);
        if (!ownerProfile || ownerIdentity.size!=ownerProfile->fileSize)
            return Error{ErrorCode::Unsupported,"Unverified D3D11 owner; existing hook left untouched"};
        const auto expected=reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(owner)+ownerProfile->exportRva);
        if (original!=expected || original!=reinterpret_cast<void*>(GetProcAddress(owner,"D3D11CreateDeviceAndSwapChain")))
            return Error{ErrorCode::Conflict,"Creation pointer differs from verified owner export"};
        const auto ownerImage=snapshotModule(owner,ownerProfile->imageSize);
        if (!std::equal(ownerProfile->prologue.begin(),ownerProfile->prologue.end(),ownerImage.begin()+ownerProfile->exportRva))
            return Error{ErrorCode::Conflict,"D3D11 export prologue changed; chain not verified"};
        auto pending=std::make_unique<ObserverLease>();
        pending->original=reinterpret_cast<CreateD3D11>(original); pending->notification=notification;
        pending->disabledPatchIds=settings.get<Text>("Patching.DisabledPatchIds").value;
        if(const auto sites=findEnbContextSites(ownerIdentity.hash);
           sites.om&&ownerIdentity.size==sites.om->fileSize)
            pending->exactEnbOwner=owner;
        HMODULE pinnedSelf=nullptr,pinnedOwner=nullptr;
        constexpr DWORD pinFlags=GET_MODULE_HANDLE_EX_FLAG_PIN|GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS;
        if (!GetModuleHandleExW(pinFlags,reinterpret_cast<LPCWSTR>(&createProxy),&pinnedSelf) ||
            !GetModuleHandleExW(pinFlags,reinterpret_cast<LPCWSTR>(original),&pinnedOwner))
            return Error{ErrorCode::Unavailable,"Cannot pin creation-hook callback lifetimes"};
        spdlog::info("Preparing skyrim1170.device-create.observe-v1: IAT RVA=0x{:x}; owner={}; activation=SKSEPlugin_Load outside DllMain; thread={}",
            profile.iatRva,ownerProfile->name,GetCurrentThreadId());
        // Publish initialized forwarding state before the atomic IAT replacement.
        // Keep it resident even if another mod wins the slot race.
        auto* published=pending.release(); lease.store(published,std::memory_order_release);
        const auto applied=published->patch.apply(slot,original,reinterpret_cast<void*>(&createProxy));
        if (const auto error=std::get_if<Error>(&applied)) return *error;
        published->armed.store(true);
        const auto world=installWorldDrawPassThrough(game,identity.hash,settings);
        if(const auto error=std::get_if<Error>(&world))
            spdlog::warn("World-draw pass-through not installed: {}",error->message);
        const auto menuInput=installMenuInputDispatchHook(game,identity.hash,settings);
        if(const auto error=std::get_if<Error>(&menuInput))
            spdlog::warn("Menu input dispatch hook not installed: {}",error->message);
        const auto rect=installOwnedRendererRectHook(game,identity.hash,settings);
        if(const auto error=std::get_if<Error>(&rect))
            spdlog::warn("Owned renderer rectangle pass-through not installed: {}",
                error->message);
        const auto drs=installDrsProbe(game,identity.hash,settings);
        if(const auto error=std::get_if<Error>(&drs))
            spdlog::warn("Experimental DRS probe not installed: {}",error->message);
        try { spdlog::info("Renderer observation IAT installed; original chain preserved; SR/FG/NR inactive"); } catch (...) {}
        return true;
    } catch (const std::exception& error) {
        return Error{ErrorCode::Unavailable,std::string("Renderer observation preparation failed: ")+error.what()};
    }
}
}
