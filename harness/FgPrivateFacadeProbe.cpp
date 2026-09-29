#include "rk/FgStreamlineRuntime.hpp"
#include "rk/FgD3D11SwapFacade.hpp"
#include "rk/FactoryCreateTrace.hpp"
#include "rk/OwnedRouteProfile.hpp"
#include "rk/PatchDescriptor.hpp"
#include "rk/PointerPatch.hpp"
#include "../tests/support/FgObservedSwap.hpp"
#include <sl_dlss_g.h>
#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <atomic>
#include <cstring>
#include <iostream>
#include <variant>

using Microsoft::WRL::ComPtr;
namespace {
struct Module {
    HMODULE value{};
    ~Module(){if(value)FreeLibrary(value);}
};
struct Window {
    HWND value{};
    ~Window(){if(value)DestroyWindow(value);}
};
bool sameIdentity(IUnknown* a,IUnknown* b) {
    ComPtr<IUnknown> left,right;
    return a&&b&&SUCCEEDED(a->QueryInterface(IID_PPV_ARGS(&left)))&&
        SUCCEEDED(b->QueryInterface(IID_PPV_ARGS(&right)))&&
        left.Get()==right.Get();
}
bool sameAdapter(IUnknown* device,const LUID& expected) {
    ComPtr<IDXGIDevice> dxgi;
    ComPtr<IDXGIAdapter> adapter;
    DXGI_ADAPTER_DESC description{};
    return device&&SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&dxgi)))&&
        SUCCEEDED(dxgi->GetAdapter(&adapter))&&
        SUCCEEDED(adapter->GetDesc(&description))&&
        std::memcmp(&description.AdapterLuid,&expected,sizeof(LUID))==0;
}
bool ownedMethod(void* method,std::string_view digest) {
    HMODULE owner{};
    if(!method||!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
        reinterpret_cast<LPCWSTR>(method),&owner))return false;
    wchar_t path[32768]{};
    const auto count=GetModuleFileNameW(owner,path,32768);
    FreeLibrary(owner);
    if(!count||count>=32768)return false;
    const auto hash=rk::sha256File(path);
    return std::holds_alternative<std::string>(hash)&&
        std::get<std::string>(hash)==digest;
}
struct Insertion {
    ~Insertion(){
        facade.Reset();
        auxiliarySwap.Reset();
        if(auxiliaryWindow)DestroyWindow(auxiliaryWindow);
    }
    rk::FactoryCreateFn original{};
    IDXGIFactory* delegate{};
    IUnknown* device{};
    ID3D12Device* d12{};
    ID3D12CommandQueue* queue{};
    IDXGISwapChain4* lower{};
    ID3D12Device* verifiedNative{};
    ComPtr<IDXGISwapChain4> facade;
    ComPtr<ID3D11Device> nativeD11;
    ComPtr<IDXGISwapChain> auxiliarySwap;
    HWND auxiliaryWindow{};
    LUID adapterLuid{};
    HWND window{};
    std::atomic<unsigned> attempts{0};
    std::atomic<unsigned> calls{0};
};
std::atomic<Insertion*> activeInsertion{};
HRESULT WINAPI insertFacade(IDXGIFactory* factory,IUnknown* device,
    DXGI_SWAP_CHAIN_DESC* desc,IDXGISwapChain** output) noexcept {
    auto* state=activeInsertion.load(std::memory_order_acquire);
    if(!state||!state->original)return E_UNEXPECTED;
    state->attempts.fetch_add(1,std::memory_order_relaxed);
    std::cout<<"private factory callback: factory=0x"<<std::hex<<
        reinterpret_cast<std::uintptr_t>(factory)<<" expected=0x"<<
        reinterpret_cast<std::uintptr_t>(state->delegate)<<" device=0x"<<
        reinterpret_cast<std::uintptr_t>(device)<<" expected=0x"<<
        reinterpret_cast<std::uintptr_t>(state->device)<<std::dec<<
        " sameIdentity="<<sameIdentity(device,state->device)<<
        " sameAdapter="<<sameAdapter(device,state->adapterLuid)<<'\n';
    if(factory!=state->delegate||!sameAdapter(device,state->adapterLuid)||
       !desc||!output||desc->OutputWindow!=state->window||
       desc->BufferDesc.Width!=160||desc->BufferDesc.Height!=96)
        return state->original(factory,device,desc,output);
    try {
        ComPtr<ID3D11Device> nativeD11;
        if(FAILED(device->QueryInterface(IID_PPV_ARGS(&nativeD11))))
            return E_NOINTERFACE;
        state->nativeD11=nativeD11;
        ComPtr<ID3D11DeviceContext> nativeContext;
        nativeD11->GetImmediateContext(&nativeContext);
        state->auxiliaryWindow=CreateWindowExW(0,L"STATIC",L"RazKolbas auxiliary D3D11",
            WS_POPUP,0,0,160,96,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        if(!state->auxiliaryWindow)return E_FAIL;
        auto auxiliaryDesc=*desc;
        auxiliaryDesc.OutputWindow=state->auxiliaryWindow;
        auxiliaryDesc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
        auxiliaryDesc.BufferCount=1;
        auxiliaryDesc.Flags=0;
        const auto auxResult=state->original(factory,nativeD11.Get(),
            &auxiliaryDesc,&state->auxiliarySwap);
        ComPtr<ID3D11Texture2D> auxiliaryBuffer;
        D3D11_RENDER_TARGET_VIEW_DESC srgbView{};
        srgbView.Format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        srgbView.ViewDimension=D3D11_RTV_DIMENSION_TEXTURE2D;
        ComPtr<ID3D11RenderTargetView> auxiliarySrgb,auxiliaryDefault;
        const auto auxBufferResult=SUCCEEDED(auxResult)?state->auxiliarySwap->GetBuffer(
            0,IID_PPV_ARGS(&auxiliaryBuffer)):E_FAIL;
        const auto auxSrgbResult=SUCCEEDED(auxBufferResult)?nativeD11->CreateRenderTargetView(
            auxiliaryBuffer.Get(),&srgbView,&auxiliarySrgb):E_FAIL;
        const auto auxDefaultResult=SUCCEEDED(auxBufferResult)?nativeD11->CreateRenderTargetView(
            auxiliaryBuffer.Get(),nullptr,&auxiliaryDefault):E_FAIL;
        std::cout<<"aux swap=0x"<<std::hex<<static_cast<unsigned>(auxResult)<<
            " buffer=0x"<<static_cast<unsigned>(auxBufferResult)<<
            " srgb=0x"<<static_cast<unsigned>(auxSrgbResult)<<
            " default=0x"<<static_cast<unsigned>(auxDefaultResult)<<std::dec<<'\n';
        if(FAILED(auxResult)||FAILED(auxBufferResult)||FAILED(auxSrgbResult)||
           FAILED(auxDefaultResult))return E_FAIL;
        auto made=rk::FgD3D11SwapFacade::create(nativeD11.Get(),
            nativeContext.Get(),state->d12,state->queue,state->lower,
            state->verifiedNative,auxiliaryBuffer.Get());
        if(!std::holds_alternative<ComPtr<IDXGISwapChain4>>(made))return E_FAIL;
        state->facade=std::move(std::get<ComPtr<IDXGISwapChain4>>(made));
        *output=state->facade.Get();
        state->facade->AddRef();
        state->calls.fetch_add(1,std::memory_order_relaxed);
        return S_OK;
    } catch(...) {return E_FAIL;}
}
int run(rk::FgStreamlineRuntime& runtime,const wchar_t* reshadePath) {
    ComPtr<IDXGIFactory6> nativeFactory;
    if(FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&nativeFactory))))return 10;
    ComPtr<IDXGIAdapter1> adapter;
    ComPtr<ID3D12Device> d12;
    for(UINT index=0;;++index) {
        ComPtr<IDXGIAdapter1> candidate;
        const auto enumerated=nativeFactory->EnumAdapters1(index,&candidate);
        if(enumerated==DXGI_ERROR_NOT_FOUND)break;
        if(FAILED(enumerated)||!candidate)return 11;
        DXGI_ADAPTER_DESC1 description{};
        if(SUCCEEDED(candidate->GetDesc1(&description))&&
           description.VendorId==0x10de&&
           SUCCEEDED(D3D12CreateDevice(candidate.Get(),
               D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&d12)))) {
            adapter=candidate;
            break;
        }
    }
    if(!adapter||!d12)return 12;
    if(runtime.setD3DDevice(d12.Get())!=sl::Result::eOk)return 13;
    auto* factoryRaw=nativeFactory.Get();
    if(runtime.upgradeInterface(reinterpret_cast<void**>(&factoryRaw))!=
       sl::Result::eOk)return 14;
    ComPtr<IDXGIFactory6> factory;
    if(factoryRaw==nativeFactory.Get())factory=factoryRaw;
    else factory.Attach(factoryRaw);
    auto* deviceRaw=d12.Get();
    if(runtime.upgradeInterface(reinterpret_cast<void**>(&deviceRaw))!=
       sl::Result::eOk)return 15;
    ComPtr<ID3D12Device> device;
    if(deviceRaw==d12.Get())device=deviceRaw;
    else device.Attach(deviceRaw);
    if(!ownedMethod((*reinterpret_cast<void***>(factory.Get()))[10],
        rk::streamline2141FactoryCreateSite().moduleSha256))return 16;
    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    ComPtr<ID3D12CommandQueue> queue;
    if(FAILED(device->CreateCommandQueue(&queueDesc,
        IID_PPV_ARGS(&queue))))return 17;
    Window window{CreateWindowExW(0,L"STATIC",L"RazKolbas private FG facade",
        WS_POPUP,0,0,160,96,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr)};
    if(!window.value)return 18;
    DXGI_SWAP_CHAIN_DESC1 swapDesc{};
    swapDesc.Width=160;swapDesc.Height=96;
    swapDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    swapDesc.SampleDesc.Count=1;
    swapDesc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapDesc.BufferCount=2;
    swapDesc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
    ComPtr<IDXGISwapChain1> lower;
    const auto lowerMade=factory->CreateSwapChainForHwnd(queue.Get(),
        window.value,&swapDesc,nullptr,nullptr,&lower);
    if(FAILED(lowerMade)||!lower)return 19;
    void* rawNative{};
    if(runtime.getNativeInterface(device.Get(),&rawNative)!=sl::Result::eOk||
       !rawNative)return 20;
    ComPtr<IUnknown> nativeOwner;
    nativeOwner.Attach(static_cast<IUnknown*>(rawNative));
    ComPtr<ID3D12Device> verifiedNative;
    if(FAILED(nativeOwner.As(&verifiedNative)))return 21;
    ComPtr<ID3D12Device> lowerDevice;
    if(FAILED(lower->GetDevice(IID_PPV_ARGS(&lowerDevice)))||
       !sameIdentity(lowerDevice.Get(),verifiedNative.Get()))return 22;
    if(!ownedMethod((*reinterpret_cast<void***>(lower.Get()))[8],
        rk::streamline2141FactoryCreateSite().moduleSha256))return 23;
    std::cout<<"private lower swap and Present owner verified\n";
    const auto reshadeHash=rk::sha256File(reshadePath);
    const auto& reshadeSite=rk::reshade680FactoryCreateSite();
    if(!std::holds_alternative<std::string>(reshadeHash)||
       std::get<std::string>(reshadeHash)!=reshadeSite.moduleSha256)return 28;
    Module reshade{LoadLibraryW(reshadePath)};
    if(!reshade.value)return 29;
    std::cout<<"ReShade loaded\n";
    ComPtr<ID3D11Device> d11;
    ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL level{};
    if(FAILED(D3D11CreateDevice(adapter.Get(),D3D_DRIVER_TYPE_UNKNOWN,
       nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,
       D3D11_SDK_VERSION,&d11,&level,&context)))return 24;
    std::cout<<"system D3D11CreateDevice returned\n";
    ComPtr<IDXGIDevice> dxgiDevice;
    ComPtr<IDXGIAdapter> d11Adapter;
    DXGI_ADAPTER_DESC d11Desc{};
    const auto d12Luid=d12->GetAdapterLuid();
    if(FAILED(d11.As(&dxgiDevice))||
       FAILED(dxgiDevice->GetAdapter(&d11Adapter))||
       FAILED(d11Adapter->GetDesc(&d11Desc))||
       std::memcmp(&d11Desc.AdapterLuid,&d12Luid,sizeof(LUID))!=0)return 25;
    ComPtr<IDXGISwapChain4> lower4;
    if(FAILED(lower.As(&lower4)))return 26;
    std::atomic<unsigned> lowerPresents{0};
    ComPtr<IDXGISwapChain4> observed;
    observed.Attach(new rk_test::FgObservedSwap(lower4.Get(),
        [&](UINT) noexcept {
            lowerPresents.fetch_add(1,std::memory_order_relaxed);
            return S_OK;
        }));
    using CreateFactory=HRESULT(WINAPI*)(REFIID,void**);
    const auto create=reinterpret_cast<CreateFactory>(
        GetProcAddress(reshade.value,"CreateDXGIFactory1"));
    ComPtr<IDXGIFactory1> wrapper;
    if(!create||FAILED(create(IID_PPV_ARGS(&wrapper))))return 30;
    const auto reshadeBase=reinterpret_cast<std::uintptr_t>(reshade.value);
    const auto method=reinterpret_cast<rk::FactoryCreateFn>(
        reshadeBase+reshadeSite.methodRva);
    if(!rk::isReshadeFactoryDelegateSite(wrapper.Get(),reshadeBase,
       reshadeSite.moduleSha256,method,reshadeSite))return 31;
    const auto facts=rk::inspectReshadeFactoryDelegate(wrapper.Get());
    const auto& nativeSite=rk::win11DxgiFactoryCreateSite();
    if(!facts.methodExecutable||!facts.delegate||
       !ownedMethod(reinterpret_cast<void*>(facts.createMethod),
           nativeSite.moduleSha256))return 32;
    Module nativeModule{};
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
        reinterpret_cast<LPCWSTR>(facts.vtable),&nativeModule.value))return 33;
    const auto nativeBase=reinterpret_cast<std::uintptr_t>(nativeModule.value);
    if(facts.vtable!=nativeBase+nativeSite.tableRva||
       facts.createMethod!=nativeBase+nativeSite.methodRva)return 34;
    DXGI_SWAP_CHAIN_DESC gameDesc{};
    if(FAILED(lower->GetDesc(&gameDesc)))return 35;
    Insertion insertion{};
    insertion.original=reinterpret_cast<rk::FactoryCreateFn>(facts.createMethod);
    insertion.delegate=reinterpret_cast<IDXGIFactory*>(facts.delegate);
    insertion.device=d11.Get();
    insertion.d12=device.Get();
    insertion.queue=queue.Get();
    insertion.lower=observed.Get();
    insertion.verifiedNative=verifiedNative.Get();
    insertion.adapterLuid=d12Luid;
    insertion.window=window.value;
    rk::PointerPatch patch;
    auto** nativeTable=reinterpret_cast<void**>(facts.vtable);
    activeInsertion.store(&insertion,std::memory_order_release);
    const auto installed=patch.apply(nativeTable+nativeSite.slot,
        reinterpret_cast<void*>(facts.createMethod),
        reinterpret_cast<void*>(&insertFacade));
    if(!std::holds_alternative<bool>(installed)) {
        activeInsertion.store(nullptr,std::memory_order_release);
        return 36;
    }
    ComPtr<IDXGISwapChain> upper;
    const auto wrapped=wrapper->CreateSwapChain(d11.Get(),&gameDesc,&upper);
    const auto restored=patch.restore();
    activeInsertion.store(nullptr,std::memory_order_release);
    std::cout<<"ReShade CreateSwapChain=0x"<<std::hex<<
        static_cast<unsigned>(wrapped)<<std::dec<<
        " upper="<<static_cast<bool>(upper)<<
        " attempts="<<insertion.attempts.load()<<
        " substitutions="<<insertion.calls.load()<<'\n';
    if(!std::holds_alternative<bool>(restored))return 37;
    if(FAILED(wrapped)||!upper||insertion.calls.load()!=1)return 38;
    auto facade=std::move(insertion.facade);
    if(!facade)return 38;
    ComPtr<ID3D11Texture2D> facadeBuffer;
    if(FAILED(facade->GetBuffer(0,IID_PPV_ARGS(&facadeBuffer))))return 39;
    D3D11_TEXTURE2D_DESC facadeBufferDesc{};
    facadeBuffer->GetDesc(&facadeBufferDesc);
    D3D11_RENDER_TARGET_VIEW_DESC srgbView{};
    srgbView.Format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    srgbView.ViewDimension=D3D11_RTV_DIMENSION_TEXTURE2D;
    ComPtr<ID3D11RenderTargetView> nativeSrgb,wrappedSrgb;
    const auto nativeSrgbResult=insertion.nativeD11->CreateRenderTargetView(
        facadeBuffer.Get(),&srgbView,&nativeSrgb);
    const auto wrappedSrgbResult=d11->CreateRenderTargetView(
        facadeBuffer.Get(),&srgbView,&wrappedSrgb);
    std::cout<<"facade buffer format="<<static_cast<unsigned>(facadeBufferDesc.Format)<<
        " nativeSRGB=0x"<<std::hex<<static_cast<unsigned>(nativeSrgbResult)<<
        " wrappedSRGB=0x"<<static_cast<unsigned>(wrappedSrgbResult)<<std::dec<<'\n';
    if(FAILED(nativeSrgbResult)||FAILED(wrappedSrgbResult))return 39;
    std::cout<<"private ReShade upper=0x"<<std::hex<<
        reinterpret_cast<std::uintptr_t>(upper.Get())<<" facade=0x"<<
        reinterpret_cast<std::uintptr_t>(facade.Get())<<std::dec<<
        " sameIdentity="<<sameIdentity(upper.Get(),facade.Get())<<'\n';
    const bool reshadePresent=ownedMethod(
        (*reinterpret_cast<void***>(upper.Get()))[8],
        reshadeSite.moduleSha256);
    std::cout<<"ReShade Present owner="<<reshadePresent<<'\n';
    if(!reshadePresent)return 40;
    ComPtr<ID3D11Device> upperDevice;
    ComPtr<ID3D12Device> hidden;
    if(FAILED(upper->GetDevice(IID_PPV_ARGS(&upperDevice)))||
       !sameIdentity(upperDevice.Get(),d11.Get())||
       upper->GetDevice(IID_PPV_ARGS(&hidden))!=E_NOINTERFACE)return 41;
    ComPtr<ID3D11Texture2D> colour;
    if(FAILED(upper->GetBuffer(0,IID_PPV_ARGS(&colour))))return 42;
    ComPtr<ID3D11RenderTargetView> rtv;
    if(FAILED(d11->CreateRenderTargetView(colour.Get(),nullptr,&rtv)))return 43;
    void* optionFunction{};
    if(runtime.getFeatureFunction(sl::kFeatureDLSS_G,"slDLSSGSetOptions",
       optionFunction)!=sl::Result::eOk||!optionFunction)return 44;
    auto* setOptions=reinterpret_cast<PFun_slDLSSGSetOptions*>(optionFunction);
    sl::DLSSGOptions options{};
    options.mode=sl::DLSSGMode::eOff;
    if(setOptions(sl::ViewportHandle{0u},options)!=sl::Result::eOk)return 45;
    const auto test=upper->Present(0,DXGI_PRESENT_TEST);
    const float colours[3][4]{{1.f,0.f,0.f,1.f},
        {0.f,1.f,0.f,1.f},{0.f,0.f,1.f,1.f}};
    HRESULT presented=S_OK;
    for(const auto& colour:colours) {
        context->ClearRenderTargetView(rtv.Get(),colour);
        presented=upper->Present(0,0);
        if(FAILED(presented))break;
    }
    std::cout<<"ReShade returned swap distinct=1; Present(TEST)=0x"<<
        std::hex<<static_cast<unsigned>(test)<<" Present=0x"<<
        static_cast<unsigned>(presented)<<std::dec<<
        " lowerCallbacks="<<lowerPresents.load()<<'\n';
    if(FAILED(test)||FAILED(presented)||lowerPresents.load()!=3)return 46;
    void* stateFunction{};
    if(runtime.getFeatureFunction(sl::kFeatureDLSS_G,"slDLSSGGetState",
       stateFunction)!=sl::Result::eOk||!stateFunction)return 47;
    auto* getState=reinterpret_cast<PFun_slDLSSGGetState*>(stateFunction);
    sl::DLSSGState state{};
    const auto stateResult=getState(sl::ViewportHandle{0u},state,nullptr);
    std::cout<<"FG-Off state="<<static_cast<int>(stateResult)<<
        " actualPresented="<<state.numFramesActuallyPresented<<'\n';
    if(stateResult!=sl::Result::eOk||
       state.numFramesActuallyPresented!=1)return 48;
    const auto deniedResize=facade->ResizeBuffers(2,192,108,
        DXGI_FORMAT_R8G8B8A8_UNORM,0);
    std::cout<<"external surface resize=0x"<<std::hex<<
        static_cast<unsigned>(deniedResize)<<std::dec<<'\n';
    if(deniedResize!=DXGI_ERROR_UNSUPPORTED)return 50;
    return 0;
}
}
int wmain(int argc,wchar_t** argv) {
    std::cout.setf(std::ios::unitbuf);
    std::cerr.setf(std::ios::unitbuf);
    if(argc!=3)return 1;
    auto loaded=rk::FgStreamlineRuntime::initialize(argv[1]);
    if(const auto error=std::get_if<rk::Error>(&loaded)) {
        std::cerr<<"private loader: "<<error->message<<'\n';
        return 2;
    }
    auto runtime=std::move(std::get<std::unique_ptr<rk::FgStreamlineRuntime>>(
        loaded));
    int outcome=0;
    try {outcome=run(*runtime,argv[2]);}
    catch(const std::exception& error) {
        std::cerr<<"probe exception: "<<error.what()<<'\n';
        outcome=49;
    } catch(...) {outcome=49;}
    const auto stopped=runtime->shutdown();
    if(const auto error=std::get_if<rk::Error>(&stopped)) {
        std::cerr<<"private shutdown: "<<error->message<<'\n';
        return 3;
    }
    std::cout<<"private shutdown=ok; result="<<outcome<<'\n';
    return outcome;
}
