#include "rk/RendererBootstrap.hpp"
#include "rk/PatchDescriptor.hpp"
#include "rk/PointerPatch.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>

namespace rk {
namespace {
struct ObserverLease {
    PointerPatch patch;
    CreateD3D11 original{};
    RendererObserved notification{};
    std::atomic<unsigned> observations{0};
    std::atomic<bool> armed{false};
};
// Process-lifetime lease: both callback DLL and prior owner's DLL are pinned.
// SKSE can FreeLibrary during shutdown; reachable callback code must remain valid.
std::atomic<ObserverLease*> lease{nullptr};
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
    spdlog::info("Actual render adapter: {}; vendor=0x{:04x}; device=0x{:04x}; LUID={:08x}:{:08x}; featureLevel=0x{:x}",
        snapshot.adapter,snapshot.vendorId,snapshot.deviceId,static_cast<std::uint32_t>(snapshot.luidHigh),
        snapshot.luidLow,static_cast<unsigned>(snapshot.featureLevel));
    spdlog::info("Actual swap chain: {}x{}; format={}; buffers={}; samples={}; swapEffect={}; windowed={}; deviceFlags=0x{:x}",
        snapshot.width,snapshot.height,static_cast<unsigned>(snapshot.format),snapshot.bufferCount,snapshot.sampleCount,
        static_cast<unsigned>(snapshot.swapEffect),snapshot.windowed,snapshot.deviceFlags);
    if (state->notification) state->notification(snapshot);
}
HRESULT WINAPI createProxy(IDXGIAdapter* adapter,D3D_DRIVER_TYPE driverType,HMODULE software,UINT flags,
    const D3D_FEATURE_LEVEL* levels,UINT levelCount,UINT sdkVersion,const DXGI_SWAP_CHAIN_DESC* swapDesc,
    IDXGISwapChain** swapChain,ID3D11Device** device,D3D_FEATURE_LEVEL* featureLevel,ID3D11DeviceContext** context) noexcept {
    auto* state=lease.load(std::memory_order_acquire);
    if (!state || !state->original) return E_UNEXPECTED;
    const DeviceCreationArgs args{adapter,driverType,software,flags,levels,levelCount,sdkVersion,
        swapDesc,swapChain,device,featureLevel,context};
    return observeDeviceCreation(state->original,args,&observed);
}
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
        try { spdlog::info("Renderer observation IAT installed; original chain preserved; SR/FG/NR inactive"); } catch (...) {}
        return true;
    } catch (const std::exception& error) {
        return Error{ErrorCode::Unavailable,std::string("Renderer observation preparation failed: ")+error.what()};
    }
}
}
