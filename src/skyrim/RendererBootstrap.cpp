#include "rk/RendererBootstrap.hpp"
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
#include "rk/OwnedSwapBufferRoute.hpp"
#include "rk/NativeUiRedirector.hpp"
#include "rk/RendererLogicalSize.hpp"
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
};
// Process-lifetime lease: both callback DLL and prior owner's DLL are pinned.
// SKSE can FreeLibrary during shutdown; reachable callback code must remain valid.
std::atomic<ObserverLease*> lease{nullptr};
std::atomic_flag factoryProvenanceLogged=ATOMIC_FLAG_INIT;
struct FactoryTraceLease {
    PointerPatch patch;
    FactoryCreateFn next{};
    IDXGIFactory* target{}; // identity only; do not retain the factory object
    std::atomic<unsigned> calls{0};
};
std::atomic<FactoryTraceLease*> factoryTrace{nullptr};
std::atomic_flag factoryTraceAttempted=ATOMIC_FLAG_INIT;
struct BufferTraceLease {
    PointerPatch patch;
    SwapGetBufferFn next{};
    std::atomic<unsigned> calls{0};
};
std::atomic<BufferTraceLease*> bufferTrace{nullptr};
std::atomic_flag bufferTraceAttempted=ATOMIC_FLAG_INIT;
struct UiHookLease {
    explicit UiHookLease(OwnedSceneDomain& domain) noexcept:redirect(domain) {}
    PointerPatch omPatch,viewportPatch;
    UiContextNext next{};
    NativeUiRedirector redirect;
    std::atomic<bool> armed{false};
};
std::atomic<UiHookLease*> uiHook{nullptr};
std::mutex uiInstallMutex;
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
    if(phase==ScenePhase::Dormant||phase==ScenePhase::NativeUi) {
        const auto frame=worldDrawForwardedCalls()+1;
        domain.begin(frame,domain.plan().generation,GetCurrentThreadId());
    }
    return logical->query(window,rect);
}
void STDMETHODCALLTYPE uiOmProxy(ID3D11DeviceContext* context,UINT count,
    ID3D11RenderTargetView* const* views,ID3D11DepthStencilView* depth) noexcept {
    auto* state=uiHook.load(std::memory_order_acquire);
    if(!state)return;
    if(state->armed.load(std::memory_order_acquire))
        state->redirect.onOMSetRenderTargets(context,count,views,depth);
    else state->next.om(context,count,views,depth);
}
void STDMETHODCALLTYPE uiViewportProxy(ID3D11DeviceContext* context,UINT count,
    const D3D11_VIEWPORT* views) noexcept {
    auto* state=uiHook.load(std::memory_order_acquire);
    if(!state)return;
    if(state->armed.load(std::memory_order_acquire))
        state->redirect.onRSSetViewports(context,count,views);
    else state->next.viewport(context,count,views);
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
HRESULT WINAPI swapGetBufferTrace(IDXGISwapChain* swap,UINT index,
    REFIID iid,void** output) noexcept {
    auto* state=bufferTrace.load(std::memory_order_acquire);
    if(!state||!state->next)return E_UNEXPECTED;
    const auto caller=_ReturnAddress();
    const auto result=state->next(swap,index,iid,output);
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
            spdlog::info("Nested GetBuffer #{}: swap=0x{:x}; index={}; IID.Data1=0x{:08x}; callerModule={}; callerRVA=0x{:x}; HRESULT=0x{:08x}; texture={}x{}; pass-through",
                sequence,reinterpret_cast<std::uintptr_t>(swap),index,
                iid.Data1,
                std::filesystem::path(name).filename().string(),
                owner?reinterpret_cast<std::uintptr_t>(caller)-reinterpret_cast<std::uintptr_t>(owner):0,
                static_cast<std::uint32_t>(result),desc.Width,desc.Height);
        } catch(...) {
            try {spdlog::warn("Nested GetBuffer trace #{} unavailable",sequence);} catch(...) {}
        }
    }
    return result;
}
Result<bool> installSwapGetBufferTrace(IDXGISwapChain* swap) {
    if(!swap||bufferTraceAttempted.test(std::memory_order_acquire))return false;
    auto** table=*reinterpret_cast<void***>(swap);
    HMODULE owner=nullptr;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
        reinterpret_cast<LPCWSTR>(table),&owner))
        return Error{ErrorCode::Unsupported,"Returned swap table has no loaded-module owner"};
    struct ModuleReference {HMODULE value;~ModuleReference(){FreeLibrary(value);}} reference{owner};
    const auto& site=reshade673SwapGetBufferSite();
    const auto id=identify(owner);
    const auto base=reinterpret_cast<std::uintptr_t>(owner);
    const auto tableAddress=reinterpret_cast<std::uintptr_t>(table);
    if(tableAddress<base||tableAddress-base!=site.tableRva)
        return Error{ErrorCode::Unsupported,"Returned swap is not the verified ReShade table"};
    const auto mapped=snapshotModule(owner,site.imageSize);
    const auto validated=validateOwnedRouteSite(mapped,base,id.hash,id.size,
        static_cast<std::uint32_t>(tableAddress-base),site);
    if(const auto error=std::get_if<Error>(&validated))return *error;
    if(bufferTraceAttempted.test_and_set(std::memory_order_acq_rel))return false;
    auto pending=std::make_unique<BufferTraceLease>();
    pending->next=reinterpret_cast<SwapGetBufferFn>(base+site.methodRva);
    HMODULE pinnedSelf=nullptr,pinnedOwner=nullptr;
    constexpr DWORD pin=GET_MODULE_HANDLE_EX_FLAG_PIN|GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS;
    if(!GetModuleHandleExW(pin,reinterpret_cast<LPCWSTR>(&swapGetBufferTrace),&pinnedSelf)||
       !GetModuleHandleExW(pin,reinterpret_cast<LPCWSTR>(table),&pinnedOwner))
        return Error{ErrorCode::Unavailable,"Cannot pin GetBuffer trace lifetimes"};
    auto* published=pending.release();
    bufferTrace.store(published,std::memory_order_release);
    const auto applied=published->patch.apply(table+site.slot,
        reinterpret_cast<void*>(published->next),reinterpret_cast<void*>(&swapGetBufferTrace));
    if(const auto error=std::get_if<Error>(&applied))return *error;
    spdlog::info("Installed {}: tableRVA=0x{:x}; slot={}; pass-through caller/texture trace",
        site.id,site.tableRva,site.slot);
    return true;
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
    const auto& site=reshade673FactoryCreateSite();
    const auto id=identify(owner);
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
        if(event.before)state->resizes.fetch_add(1);
        spdlog::info("Swap {} {}: object=0x{:x}; requested={}x{}; buffers={}; format={}; flags=0x{:x}; HRESULT=0x{:08x}; thread={}",
            event.call==SwapCall::Resize?"ResizeBuffers":"ResizeBuffers1",event.before?"begin":"end",
            event.object,event.width,event.height,event.buffers,static_cast<unsigned>(event.format),event.flags,
            static_cast<std::uint32_t>(event.result),GetCurrentThreadId());
        return;
    }
    if(event.before) {
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
HRESULT WINAPI swapResize(IDXGISwapChain* s,UINT count,UINT width,UINT height,DXGI_FORMAT format,UINT flags) noexcept {
    const auto* state=swapLease.load(std::memory_order_acquire);
    return observeResize(reinterpret_cast<ResizeFn>(state->originals[2]),s,count,width,height,format,flags,&swapObserved);
}
HRESULT WINAPI swapPresent1(IDXGISwapChain1* s,UINT interval,UINT flags,const DXGI_PRESENT_PARAMETERS* params) noexcept {
    const auto* state=swapLease.load(std::memory_order_acquire);
    return observePresent1(reinterpret_cast<Present1Fn>(state->originals[3]),s,interval,flags,params,&swapObserved);
}
HRESULT WINAPI swapResize1(IDXGISwapChain3* s,UINT count,UINT width,UINT height,DXGI_FORMAT format,UINT flags,const UINT* nodes,IUnknown* const* queues) noexcept {
    const auto* state=swapLease.load(std::memory_order_acquire);
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
    ID3D11RenderTargetView* nativeTarget,std::string_view disabledPatchIds) {
    std::scoped_lock lock(uiInstallMutex);
    if(uiHook.load(std::memory_order_acquire))return false;
    if(!context||context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE||
       !reducedScene||!nativeTarget||!domain.plan().valid())
        return Error{ErrorCode::InvalidInput,"Owned UI context is not prepared"};
    const auto& omSite=enbContextOmSite();
    const auto& viewportSite=enbContextViewportSite();
    if(patchDisabled(disabledPatchIds,omSite.id)||
       patchDisabled(disabledPatchIds,viewportSite.id))return false;
    try {
        auto** table=*reinterpret_cast<void***>(context);
        HMODULE owner=nullptr;
        if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
            reinterpret_cast<LPCWSTR>(table),&owner))
            return Error{ErrorCode::Unsupported,"UI context table has no loaded-module owner"};
        struct ModuleReference {HMODULE value;~ModuleReference(){FreeLibrary(value);}} reference{owner};
        const auto id=identify(owner);
        const auto base=reinterpret_cast<std::uintptr_t>(owner);
        const auto tableAddress=reinterpret_cast<std::uintptr_t>(table);
        if(tableAddress<base||tableAddress-base!=omSite.tableRva||
           omSite.tableRva!=viewportSite.tableRva)
            return Error{ErrorCode::Unsupported,"UI context is not the verified ENB table"};
        const auto mapped=snapshotModule(owner,omSite.imageSize);
        for(const auto* site:{&omSite,&viewportSite}) {
            const auto checked=validateOwnedRouteSite(mapped,base,id.hash,id.size,
                static_cast<std::uint32_t>(tableAddress-base),*site);
            if(const auto error=std::get_if<Error>(&checked))return *error;
        }
        auto pending=std::make_unique<UiHookLease>(domain);
        pending->next={reinterpret_cast<UiContextNext::OM>(base+omSite.methodRva),
            reinterpret_cast<UiContextNext::VP>(base+viewportSite.methodRva)};
        const auto configured=pending->redirect.configure(context,GetCurrentThreadId(),
            pending->next,reducedScene,nativeTarget);
        if(FAILED(configured))
            return Error{ErrorCode::Conflict,"Owned UI redirector rejected the ENB context or target"};
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
        published->armed.store(true,std::memory_order_release);
        spdlog::info("Installed verified ENB UI context slots {} and {}; dormant until owned NativeUi phase",
            omSite.id,viewportSite.id);
        return true;
    } catch(const std::exception& error) {
        return Error{ErrorCode::Unavailable,
            std::string("Owned UI context preparation failed: ")+error.what()};
    }
}
NativeUiRedirector* ownedUiRedirector() noexcept {
    auto* state=uiHook.load(std::memory_order_acquire);
    return state&&state->armed.load(std::memory_order_acquire)?&state->redirect:nullptr;
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
    spdlog::info("Installed {} at RVA 0x{:x}; User32 forwarding until owned scene activation",
        id,descriptor.siteRva);
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
