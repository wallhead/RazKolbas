#include "rk/FgPrivateSwapRoute.hpp"
#include "rk/FgD3D11AuxSwapSource.hpp"
#include "rk/FgD3D11SwapFacade.hpp"
#include "rk/OwnedRouteProfile.hpp"
#include "rk/PatchDescriptor.hpp"
#include <sl_dlss_g.h>
#include <cstring>

namespace rk {
namespace {
using Microsoft::WRL::ComPtr;
bool sameIdentity(IUnknown* a,IUnknown* b) noexcept {
    ComPtr<IUnknown> left,right;
    return a&&b&&SUCCEEDED(a->QueryInterface(IID_PPV_ARGS(&left)))&&
        SUCCEEDED(b->QueryInterface(IID_PPV_ARGS(&right)))&&left.Get()==right.Get();
}
bool ownedByPinnedInterposer(void* method) {
    HMODULE owner{};
    if(!method||!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
        reinterpret_cast<LPCWSTR>(method),&owner))return false;
    wchar_t path[32768]{};
    const auto length=GetModuleFileNameW(owner,path,32768);
    FreeLibrary(owner);
    const auto& site=streamline2141FactoryCreateSite();
    if(!length||length>=32768)return false;
    const auto hash=sha256File(path);
    return std::holds_alternative<std::string>(hash)&&
        std::get<std::string>(hash)==site.moduleSha256;
}
bool exactReshadeFactory(IDXGIFactory6* factory) {
    if(!factory)return false;
    auto** table=*reinterpret_cast<void***>(factory);
    HMODULE owner{};
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
        reinterpret_cast<LPCWSTR>(table),&owner))return false;
    wchar_t path[32768]{};
    const auto length=GetModuleFileNameW(owner,path,32768);
    const auto base=reinterpret_cast<std::uintptr_t>(owner);
    FreeLibrary(owner);
    if(!length||length>=32768)return false;
    const auto hash=sha256File(path);
    return std::holds_alternative<std::string>(hash)&&
        isReshadeFactoryDelegateSite(factory,base,
            std::get<std::string>(hash),
            reinterpret_cast<FactoryCreateFn>(table[10]),
            reshade680FactoryCreateSite());
}
bool sameAdapter(ID3D11Device* d11,const LUID& expected) noexcept {
    ComPtr<IDXGIDevice> dxgi;
    ComPtr<IDXGIAdapter> adapter;
    DXGI_ADAPTER_DESC desc{};
    return d11&&SUCCEEDED(d11->QueryInterface(IID_PPV_ARGS(&dxgi)))&&
        SUCCEEDED(dxgi->GetAdapter(&adapter))&&
        SUCCEEDED(adapter->GetDesc(&desc))&&
        std::memcmp(&desc.AdapterLuid,&expected,sizeof(LUID))==0;
}
}
FgPrivateSwapRoute::~FgPrivateSwapRoute() noexcept {retire();}
void FgPrivateSwapRoute::retire() noexcept {
    lower_.Reset();queue_.Reset();upgradedFactory_.Reset();parentFactory_.Reset();
    upgradedD12_.Reset();verifiedNative_.Reset();d12_.Reset();
    if(runtime_&&runtime_.use_count()==1) {
        const auto result=runtime_->shutdown();
        if(std::holds_alternative<Error>(result)) {
            // Keep the DLL and its search directory pinned after a failed
            // shutdown; a Streamline callback may still be reachable.
            // Its destructor deliberately pins an initialized runtime.
            runtime_.reset();
            return;
        }
        runtime_.reset();
    }
    // A live or quarantined bridge still owns provider proxies. It retains
    // the runtime; shutdown must not invalidate callbacks under those owners.
    runtime_.reset();
}
Result<std::unique_ptr<FgPrivateSwapRoute>> FgPrivateSwapRoute::prepare(
    IDXGIAdapter* selected,const DXGI_SWAP_CHAIN_DESC& gameDesc,
    const std::filesystem::path& privateRuntime) {
    if(!selected||!gameDesc.OutputWindow||!IsWindow(gameDesc.OutputWindow)||
       !gameDesc.Windowed||!gameDesc.BufferDesc.Width||!gameDesc.BufferDesc.Height||
       gameDesc.BufferDesc.Width>8192||gameDesc.BufferDesc.Height>8192||
       gameDesc.BufferDesc.Format!=DXGI_FORMAT_R8G8B8A8_UNORM||
       gameDesc.SampleDesc.Count!=1||gameDesc.SampleDesc.Quality||
       gameDesc.BufferCount<2||gameDesc.BufferCount>4||
       !(gameDesc.BufferUsage&DXGI_USAGE_RENDER_TARGET_OUTPUT)||
       (gameDesc.SwapEffect!=DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL&&
        gameDesc.SwapEffect!=DXGI_SWAP_EFFECT_FLIP_DISCARD))
        return Error{ErrorCode::Unsupported,"Game swap is outside the FG-Off facade contract"};
    DXGI_ADAPTER_DESC adapterDesc{};
    if(FAILED(selected->GetDesc(&adapterDesc))||adapterDesc.VendorId!=0x10de)
        return Error{ErrorCode::Unsupported,"Selected renderer adapter is not NVIDIA"};
    auto loaded=FgStreamlineRuntime::initialize(privateRuntime);
    if(const auto error=std::get_if<Error>(&loaded))return *error;
    auto route=std::unique_ptr<FgPrivateSwapRoute>(new FgPrivateSwapRoute);
    route->runtime_=std::move(std::get<
        std::unique_ptr<FgStreamlineRuntime>>(loaded));
    route->adapterLuid_=adapterDesc.AdapterLuid;
    route->expected_={GetCurrentThreadId(),
        reinterpret_cast<std::uintptr_t>(gameDesc.OutputWindow),
        gameDesc.BufferDesc.Width,gameDesc.BufferDesc.Height,true};
    if(FAILED(D3D12CreateDevice(selected,D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&route->d12_))))
        return Error{ErrorCode::Unavailable,"Selected adapter cannot create D3D12 device"};
    const auto d12Luid=route->d12_->GetAdapterLuid();
    if(std::memcmp(&route->adapterLuid_,&d12Luid,sizeof(LUID))!=0)
        return Error{ErrorCode::Conflict,"D3D12 renderer adapter LUID differs"};
    if(route->runtime_->setD3DDevice(route->d12_.Get())!=sl::Result::eOk)
        return Error{ErrorCode::Unavailable,"Streamline rejected D3D12 device"};
    if(FAILED(selected->GetParent(IID_PPV_ARGS(&route->parentFactory_)))||
       !exactReshadeFactory(route->parentFactory_.Get()))
        return Error{ErrorCode::Unsupported,"Selected adapter parent is not exact ReShade 6.8 factory"};
    auto* upgradedFactory=route->parentFactory_.Get();
    if(route->runtime_->upgradeInterface(
        reinterpret_cast<void**>(&upgradedFactory))!=sl::Result::eOk||
       !upgradedFactory)
        return Error{ErrorCode::Unavailable,"Streamline factory upgrade failed"};
    if(upgradedFactory==route->parentFactory_.Get())
        route->upgradedFactory_=upgradedFactory;
    else route->upgradedFactory_.Attach(upgradedFactory);
    auto* upgradedDevice=route->d12_.Get();
    if(route->runtime_->upgradeInterface(
        reinterpret_cast<void**>(&upgradedDevice))!=sl::Result::eOk||
       !upgradedDevice)
        return Error{ErrorCode::Unavailable,"Streamline D3D12 device upgrade failed"};
    if(upgradedDevice==route->d12_.Get())route->upgradedD12_=upgradedDevice;
    else route->upgradedD12_.Attach(upgradedDevice);
    if(!ownedByPinnedInterposer(
        (*reinterpret_cast<void***>(route->upgradedFactory_.Get()))[10]))
        return Error{ErrorCode::Conflict,"Upgraded factory is not the pinned Streamline owner"};
    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    if(FAILED(route->upgradedD12_->CreateCommandQueue(&queueDesc,
        IID_PPV_ARGS(&route->queue_))))
        return Error{ErrorCode::Unavailable,"Streamline D3D12 queue creation failed"};
    DXGI_SWAP_CHAIN_DESC1 lowerDesc{};
    lowerDesc.Width=gameDesc.BufferDesc.Width;
    lowerDesc.Height=gameDesc.BufferDesc.Height;
    lowerDesc.Format=gameDesc.BufferDesc.Format;
    lowerDesc.SampleDesc=gameDesc.SampleDesc;
    lowerDesc.BufferUsage=gameDesc.BufferUsage;
    lowerDesc.BufferCount=gameDesc.BufferCount;
    lowerDesc.SwapEffect=gameDesc.SwapEffect;
    lowerDesc.Flags=gameDesc.Flags;
    ComPtr<IDXGISwapChain1> lower;
    if(FAILED(route->upgradedFactory_->CreateSwapChainForHwnd(
        route->queue_.Get(),gameDesc.OutputWindow,&lowerDesc,nullptr,
        nullptr,&lower))||!lower||FAILED(lower.As(&route->lower_)))
        return Error{ErrorCode::Unavailable,"Streamline lower swap creation failed"};
    void* nativeRaw{};
    if(route->runtime_->getNativeInterface(route->upgradedD12_.Get(),
        &nativeRaw)!=sl::Result::eOk||!nativeRaw)
        return Error{ErrorCode::Unavailable,"Lower native D3D12 interface unavailable"};
    ComPtr<IUnknown> nativeOwner;
    nativeOwner.Attach(static_cast<IUnknown*>(nativeRaw));
    ComPtr<ID3D12Device> lowerDevice;
    if(FAILED(nativeOwner.As(&route->verifiedNative_))||
       FAILED(route->lower_->GetDevice(IID_PPV_ARGS(&lowerDevice)))||
       !sameIdentity(lowerDevice.Get(),route->verifiedNative_.Get())||
       !ownedByPinnedInterposer(
           (*reinterpret_cast<void***>(route->lower_.Get()))[8]))
        return Error{ErrorCode::Conflict,"Lower swap or native D3D12 ownership differs"};
    void* function{};
    if(route->runtime_->getFeatureFunction(sl::kFeatureDLSS_G,
        "slDLSSGSetOptions",function)!=sl::Result::eOk||!function)
        return Error{ErrorCode::Unavailable,"Pinned DLSS-G Off option unavailable"};
    auto* setOptions=reinterpret_cast<PFun_slDLSSGSetOptions*>(function);
    sl::DLSSGOptions options{};
    options.mode=sl::DLSSGMode::eOff;
    if(setOptions(sl::ViewportHandle{0u},options)!=sl::Result::eOk)
        return Error{ErrorCode::Unavailable,"DLSS-G Off option was rejected"};
    // The ReShade-first process order can make this exact Streamline proxy
    // return E_ABORT on a real Present even though Present(TEST) succeeds.
    // Probe before publishing the replacement, while native fallback is safe.
    const auto test=route->lower_->Present(0,DXGI_PRESENT_TEST);
    const auto presented=SUCCEEDED(test)?route->lower_->Present(0,0):test;
    if(FAILED(presented))
        return Error{ErrorCode::Unavailable,
            "Private lower Present preflight failed: "+
                std::to_string(static_cast<std::uint32_t>(presented))};
    return route;
}
Result<ComPtr<IDXGISwapChain4>> FgPrivateSwapRoute::createFacade(
    FactoryCreateFn nativeCreate,IDXGIFactory* nativeFactory,
    ID3D11Device* nativeD11,const DXGI_SWAP_CHAIN_DESC& request,
    bool nativeMethodOwner) noexcept {
    std::scoped_lock lock(mutex_);
    if(attempted_.exchange(true))
        return Error{ErrorCode::Conflict,"Private FG facade was already attempted"};
    try {
        const FgPrivateSwapAdmission actual{GetCurrentThreadId(),
            reinterpret_cast<std::uintptr_t>(request.OutputWindow),
            request.BufferDesc.Width,request.BufferDesc.Height,nativeMethodOwner};
        if(!admitPrivateFgSwap(expected_,actual)||!nativeCreate||
           !nativeFactory||!sameAdapter(nativeD11,adapterLuid_))
            return Error{ErrorCode::Conflict,"Native factory callback did not match the prepared game swap"};
        ComPtr<ID3D11DeviceContext> context;
        nativeD11->GetImmediateContext(&context);
        if(!context)return Error{ErrorCode::Unavailable,"Native D3D11 context unavailable"};
        auto auxiliary=FgD3D11AuxSwapSource::create(nativeCreate,nativeFactory,
            nativeD11,request);
        if(const auto error=std::get_if<Error>(&auxiliary))return *error;
        auto result=FgD3D11SwapFacade::create(nativeD11,context.Get(),
            upgradedD12_.Get(),queue_.Get(),lower_.Get(),
            verifiedNative_.Get(),std::move(std::get<
                std::unique_ptr<FgD3D11AuxSwapSource>>(auxiliary)),runtime_);
        if(std::holds_alternative<ComPtr<IDXGISwapChain4>>(result))
            issued_.store(true,std::memory_order_release);
        return result;
    } catch(const std::exception& error) {
        return Error{ErrorCode::Unavailable,error.what()};
    } catch(...) {
        return Error{ErrorCode::Unavailable,"Private FG facade creation failed"};
    }
}
}
