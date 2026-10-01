#include "rk/FgReShadeEffectOwner.hpp"
#include "rk/FactoryCreateTrace.hpp"
#include "rk/OwnedRouteProfile.hpp"
#include "rk/PatchDescriptor.hpp"
#include <d3d12.h>
#include <wrl/client.h>
#include <cstring>

namespace rk {
namespace {
using Microsoft::WRL::ComPtr;
constexpr std::uint32_t initRuntimeEvent=9; // ReShade 6.8 public add-on ABI
constexpr std::uint32_t destroyRuntimeEvent=10;
constexpr std::uint32_t setEffectsStateEvent=94;
constexpr std::uint32_t beginEffectsEvent=76;
constexpr std::uint32_t addonApiVersion=14;
constexpr std::uint32_t d3d12Api=0xc000;

void** tableOf(void* object) noexcept {
    return *reinterpret_cast<void***>(object);
}
std::uint32_t runtimeApi(void* runtime) noexcept {
    auto* device=reinterpret_cast<void*(*)(void*)>(tableOf(runtime)[3])(runtime);
    return device?reinterpret_cast<std::uint32_t(*)(void*)>(
        tableOf(device)[3])(device):0;
}
void* runtimeNative(void* runtime) noexcept {
    return reinterpret_cast<void*>(
        reinterpret_cast<std::uint64_t(*)(void*)>(tableOf(runtime)[0])(runtime));
}
bool effectsEnabled(void* runtime) noexcept {
    return reinterpret_cast<bool(*)(void*)>(tableOf(runtime)[60])(runtime);
}
void setEffects(void* runtime,bool enabled) noexcept {
    reinterpret_cast<void(*)(void*,bool)>(tableOf(runtime)[61])(runtime,enabled);
}
bool sameLuid(const LUID& a,const LUID& b) noexcept {
    return std::memcmp(&a,&b,sizeof(LUID))==0;
}
}

std::atomic<FgReShadeEffectOwner*> FgReShadeEffectOwner::active_{};

Result<std::unique_ptr<FgReShadeEffectOwner>> FgReShadeEffectOwner::arm(
    IDXGIAdapter* adapter,const DXGI_SWAP_CHAIN_DESC& gameDesc,
    bool addonAlreadyRegisteredForProbe) {
    if(!adapter||!gameDesc.OutputWindow||!gameDesc.BufferDesc.Width||
       !gameDesc.BufferDesc.Height)
        return Error{ErrorCode::InvalidInput,"ReShade effect owner needs the selected adapter and game swap"};
    ComPtr<IDXGIFactory6> factory;
    if(FAILED(adapter->GetParent(IID_PPV_ARGS(&factory))))
        return Error{ErrorCode::Unsupported,"Selected adapter has no ReShade factory"};
    auto** factoryTable=tableOf(factory.Get());
    HMODULE reshade{};
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
        reinterpret_cast<LPCWSTR>(factoryTable),&reshade))
        return Error{ErrorCode::Unsupported,"ReShade factory has no module owner"};
    wchar_t path[32768]{};
    const auto length=GetModuleFileNameW(reshade,path,32768);
    const auto hash=length&&length<32768?sha256File(path):Result<std::string>{
        Error{ErrorCode::Unsupported,"ReShade module path unavailable"}};
    if(!std::holds_alternative<std::string>(hash)||
       !isReshadeFactoryDelegateSite(factory.Get(),
           reinterpret_cast<std::uintptr_t>(reshade),std::get<std::string>(hash),
           reinterpret_cast<FactoryCreateFn>(factoryTable[10]),
           reshade680FactoryCreateSite())) {
        FreeLibrary(reshade);
        return Error{ErrorCode::Unsupported,"Effect owner requires the exact ReShade 6.8 factory"};
    }
    using RegisterAddon=bool(*)(HMODULE,std::uint32_t);
    const auto registerAddon=reinterpret_cast<RegisterAddon>(
        GetProcAddress(reshade,"ReShadeRegisterAddon"));
    const auto registerEvent=reinterpret_cast<RegisterEvent>(
        GetProcAddress(reshade,"ReShadeRegisterEvent"));
    const auto unregisterEvent=reinterpret_cast<RegisterEvent>(
        GetProcAddress(reshade,"ReShadeUnregisterEvent"));
    const auto unregisterAddon=reinterpret_cast<UnregisterAddon>(
        GetProcAddress(reshade,"ReShadeUnregisterAddon"));
    HMODULE self{};
    if(!registerAddon||!registerEvent||!unregisterEvent||!unregisterAddon||
       !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|
           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
           reinterpret_cast<LPCWSTR>(&initialized),&self)) {
        FreeLibrary(reshade);
        return Error{ErrorCode::Unavailable,"ReShade 6.8 add-on API is incomplete"};
    }
    auto owner=std::unique_ptr<FgReShadeEffectOwner>(new FgReShadeEffectOwner);
    owner->reshade_=reshade;owner->addon_=self;
    owner->registerEvent_=registerEvent;
    owner->unregisterEvent_=unregisterEvent;
    owner->unregisterAddon_=unregisterAddon;
    owner->window_=gameDesc.OutputWindow;
    owner->width_=gameDesc.BufferDesc.Width;
    owner->height_=gameDesc.BufferDesc.Height;
    owner->thread_=GetCurrentThreadId();
    DXGI_ADAPTER_DESC adapterDesc{};
    if(FAILED(adapter->GetDesc(&adapterDesc)))
        return Error{ErrorCode::Unavailable,"Selected adapter identity unavailable"};
    owner->adapter_=adapterDesc.AdapterLuid;
    if(!addonAlreadyRegisteredForProbe) {
        if(!registerAddon(self,addonApiVersion))
            return Error{ErrorCode::Unavailable,"ReShade rejected the single-effect add-on"};
        owner->registeredAddon_=true;
    }
    auto* expected=static_cast<FgReShadeEffectOwner*>(nullptr);
    if(!active_.compare_exchange_strong(expected,owner.get(),
        std::memory_order_acq_rel))
        return Error{ErrorCode::Conflict,"A ReShade effect owner is already armed"};
    registerEvent(initRuntimeEvent,reinterpret_cast<void*>(&initialized));
    registerEvent(destroyRuntimeEvent,reinterpret_cast<void*>(&destroyed));
    registerEvent(beginEffectsEvent,reinterpret_cast<void*>(&beginningEffects));
    registerEvent(setEffectsStateEvent,reinterpret_cast<void*>(&changingState));
    owner->registeredEvents_=true;
    return owner;
}

void FgReShadeEffectOwner::onInit(void* runtime) noexcept {
    if(!runtime||runtimeApi(runtime)!=d3d12Api||
       reinterpret_cast<HWND(*)(void*)>(tableOf(runtime)[4])(runtime)!=window_)
        return;
    auto* native=runtimeNative(runtime);
    if(!native)return;
    const auto tracked=runtime_.load(std::memory_order_acquire);
    if(tracked) {
        if(tracked!=runtime||nativeSwap_.load(std::memory_order_acquire)!=native)
            return;
    } else {
        if(!accepting_.load(std::memory_order_acquire)||
           GetCurrentThreadId()!=thread_)return;
        ComPtr<IDXGISwapChain> swap;
        ComPtr<ID3D12Device> device;
        DXGI_SWAP_CHAIN_DESC desc{};
        auto* object=reinterpret_cast<IUnknown*>(native);
        if(FAILED(object->QueryInterface(IID_PPV_ARGS(&swap)))||
           FAILED(swap->GetDesc(&desc))||
           FAILED(swap->GetDevice(IID_PPV_ARGS(&device)))||
           desc.BufferCount!=2||desc.BufferDesc.Width!=width_||
           desc.BufferDesc.Height!=height_||
           desc.OutputWindow!=window_||
           !sameLuid(device->GetAdapterLuid(),adapter_))return;
        priorEnabled_=effectsEnabled(runtime);
        nativeSwap_.store(native,std::memory_order_release);
        runtime_.store(runtime,std::memory_order_release);
    }
    live_.store(true,std::memory_order_release);
    setEffects(runtime,false);
    if(effectsEnabled(runtime))failed_.store(true,std::memory_order_release);
    else suppressedInits_.fetch_add(1,std::memory_order_acq_rel);
}

void FgReShadeEffectOwner::onDestroy(void* runtime) noexcept {
    if(runtime==runtime_.load(std::memory_order_acquire))
        live_.store(false,std::memory_order_release);
}
void FgReShadeEffectOwner::initialized(void* runtime) noexcept {
    if(auto* owner=active_.load(std::memory_order_acquire))owner->onInit(runtime);
}
void FgReShadeEffectOwner::destroyed(void* runtime) noexcept {
    if(auto* owner=active_.load(std::memory_order_acquire))owner->onDestroy(runtime);
}
void FgReShadeEffectOwner::beginningEffects(void* runtime,void*,
    std::uint64_t,std::uint64_t) noexcept {
    auto* owner=active_.load(std::memory_order_acquire);
    if(owner&&runtime==owner->runtime_.load(std::memory_order_acquire)&&
       owner->live_.load(std::memory_order_acquire)&&effectsEnabled(runtime)) {
        setEffects(runtime,false);
        owner->reassertions_.fetch_add(1,std::memory_order_acq_rel);
    }
}
bool FgReShadeEffectOwner::changingState(void* runtime,bool enabled) noexcept {
    auto* owner=active_.load(std::memory_order_acquire);
    return owner&&enabled&&owner->live_.load(std::memory_order_acquire)&&
        runtime==owner->runtime_.load(std::memory_order_acquire);
}
bool FgReShadeEffectOwner::initialSuppressed() const noexcept {
    return live_.load(std::memory_order_acquire)&&
        suppressedInits_.load(std::memory_order_acquire)>0&&
        !failed_.load(std::memory_order_acquire);
}
FgReShadeEffectOwner::~FgReShadeEffectOwner() noexcept {
    auto* expected=this;
    active_.compare_exchange_strong(expected,nullptr,std::memory_order_acq_rel);
    if(registeredEvents_) {
        unregisterEvent_(setEffectsStateEvent,
            reinterpret_cast<void*>(&changingState));
        unregisterEvent_(beginEffectsEvent,
            reinterpret_cast<void*>(&beginningEffects));
        unregisterEvent_(destroyRuntimeEvent,
            reinterpret_cast<void*>(&destroyed));
        unregisterEvent_(initRuntimeEvent,
            reinterpret_cast<void*>(&initialized));
    }
    if(live_.load(std::memory_order_acquire)&&priorEnabled_)
        setEffects(runtime_.load(std::memory_order_acquire),true);
    if(registeredAddon_)unregisterAddon_(addon_);
    if(reshade_)FreeLibrary(reshade_);
}
}
