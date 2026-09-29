#include "rk/FgPrivateSwapRoute.hpp"
#include "rk/FactoryCreateTrace.hpp"
#include "rk/OwnedRouteProfile.hpp"
#include "rk/PatchDescriptor.hpp"
#include "rk/PointerPatch.hpp"
#include <d3d11.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <iostream>
#include <variant>

using Microsoft::WRL::ComPtr;
namespace {
struct Module {HMODULE value{};~Module(){if(value)FreeLibrary(value);}};
struct Window {HWND value{};~Window(){if(value)DestroyWindow(value);}};
struct Callback {
    rk::FgPrivateSwapRoute* route{};
    rk::FactoryCreateFn next{};
    IDXGIFactory* factory{};
    unsigned substitutions{};
    ComPtr<IDXGISwapChain4> facade;
};
Callback* active{};
rk::FactoryCreateFn wrapperNext{};
unsigned wrapperCalls{};
HRESULT WINAPI wrapFactory(IDXGIFactory* factory,IUnknown* device,
    DXGI_SWAP_CHAIN_DESC* desc,IDXGISwapChain** output) noexcept {
    ++wrapperCalls;
    return wrapperNext?wrapperNext(factory,device,desc,output):E_UNEXPECTED;
}
HRESULT WINAPI replace(IDXGIFactory* factory,IUnknown* device,
    DXGI_SWAP_CHAIN_DESC* desc,IDXGISwapChain** output) noexcept {
    auto* state=active;
    if(!state||!state->next)return E_UNEXPECTED;
    if(factory!=state->factory||!desc||!output)
        return state->next(factory,device,desc,output);
    ComPtr<ID3D11Device> d11;
    if(!device||FAILED(device->QueryInterface(IID_PPV_ARGS(&d11))))
        return state->next(factory,device,desc,output);
    auto made=state->route->createFacade(state->next,factory,d11.Get(),
        *desc,true);
    if(auto* facade=std::get_if<ComPtr<IDXGISwapChain4>>(&made)) {
        state->facade=*facade;
        *output=facade->Detach();
        ++state->substitutions;
        return S_OK;
    }
    state->route->abandon();
    return state->next(factory,device,desc,output);
}
int run(const wchar_t* runtimeDirectory,const wchar_t* reshadePath) {
    const auto hash=rk::sha256File(reshadePath);
    if(!std::holds_alternative<std::string>(hash)||
       std::get<std::string>(hash)!=rk::reshade680FactoryCreateSite().moduleSha256)
        return 13;
    Module reshade{LoadLibraryW(reshadePath)};
    if(!reshade.value)return 14;
    ComPtr<IDXGIFactory1> wrapper;
    using CreateFactory=HRESULT(WINAPI*)(REFIID,void**);
    auto* create=reinterpret_cast<CreateFactory>(
        GetProcAddress(reshade.value,"CreateDXGIFactory1"));
    if(!create||FAILED(create(IID_PPV_ARGS(&wrapper))))return 18;
    ComPtr<IDXGIAdapter1> adapter;
    for(UINT index=0;;++index) {
        ComPtr<IDXGIAdapter1> candidate;
        const auto hr=wrapper->EnumAdapters1(index,&candidate);
        if(hr==DXGI_ERROR_NOT_FOUND)break;
        if(FAILED(hr))return 11;
        DXGI_ADAPTER_DESC1 desc{};
        if(SUCCEEDED(candidate->GetDesc1(&desc))&&
           desc.VendorId==0x10de) {adapter=candidate;break;}
    }
    if(!adapter)return 12;
    ComPtr<ID3D11Device> d11;
    ComPtr<ID3D11DeviceContext> context;
    if(FAILED(D3D11CreateDevice(adapter.Get(),D3D_DRIVER_TYPE_UNKNOWN,
        nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,
        &d11,nullptr,&context)))return 15;
    Window window{CreateWindowExW(0,L"STATIC",L"RazKolbas game FG route",
        WS_POPUP,0,0,160,96,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr)};
    if(!window.value)return 16;
    DXGI_SWAP_CHAIN_DESC game{};
    game.BufferDesc.Width=160;game.BufferDesc.Height=96;
    game.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    game.SampleDesc.Count=1;game.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
    game.BufferCount=2;game.OutputWindow=window.value;game.Windowed=TRUE;
    game.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
    auto prepared=rk::FgPrivateSwapRoute::prepare(adapter.Get(),game,
        runtimeDirectory);
    if(const auto* error=std::get_if<rk::Error>(&prepared)) {
        std::cerr<<"game-route preparation: "<<error->message<<'\n';
        return 17;
    }
    auto route=std::move(std::get<std::unique_ptr<rk::FgPrivateSwapRoute>>(
        prepared));
    const auto& reshadeSite=rk::reshade680FactoryCreateSite();
    auto* wrapperMethod=reinterpret_cast<rk::FactoryCreateFn>(
        (*reinterpret_cast<void***>(wrapper.Get()))[10]);
    if(!rk::isReshadeFactoryDelegateSite(wrapper.Get(),
        reinterpret_cast<std::uintptr_t>(reshade.value),
        reshadeSite.moduleSha256,wrapperMethod,reshadeSite))return 29;
    const auto facts=rk::inspectReshadeFactoryDelegate(wrapper.Get());
    const auto& site=rk::win11DxgiFactoryCreateSite();
    HMODULE owner{};
    if(!facts.methodExecutable||!facts.delegate||
       !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
           reinterpret_cast<LPCWSTR>(facts.vtable),&owner))return 19;
    Module ownerRef{owner};
    wchar_t nativePath[32768]{};
    if(!GetModuleFileNameW(owner,nativePath,32768))return 20;
    const auto nativeHash=rk::sha256File(nativePath);
    if(!std::holds_alternative<std::string>(nativeHash)||
       std::get<std::string>(nativeHash)!=site.moduleSha256||
       facts.vtable!=reinterpret_cast<std::uintptr_t>(owner)+site.tableRva||
       facts.createMethod!=reinterpret_cast<std::uintptr_t>(owner)+site.methodRva)
        return 20;
    Callback callback{route.get(),
        reinterpret_cast<rk::FactoryCreateFn>(facts.createMethod),
        reinterpret_cast<IDXGIFactory*>(facts.delegate),0};
    rk::PointerPatch patch;
    auto** table=reinterpret_cast<void**>(facts.vtable);
    active=&callback;
    const auto installed=patch.apply(table+site.slot,
        reinterpret_cast<void*>(facts.createMethod),
        reinterpret_cast<void*>(&replace));
    if(!std::holds_alternative<bool>(installed)) {active=nullptr;return 21;}
    rk::PointerPatch wrapperPatch;
    auto** wrapperTable=*reinterpret_cast<void***>(wrapper.Get());
    wrapperNext=wrapperMethod;
    const auto wrapperInstalled=wrapperPatch.apply(
        wrapperTable+reshadeSite.slot,
        reinterpret_cast<void*>(wrapperMethod),
        reinterpret_cast<void*>(&wrapFactory));
    if(!std::holds_alternative<bool>(wrapperInstalled)) {
        patch.restore();active=nullptr;return 30;
    }
    ComPtr<IDXGISwapChain> upper;
    const auto created=wrapper->CreateSwapChain(d11.Get(),&game,&upper);
    const auto wrapperRestored=wrapperPatch.restore();
    const auto restored=patch.restore();
    active=nullptr;
    if(!std::holds_alternative<bool>(restored)||
       !std::holds_alternative<bool>(wrapperRestored)||FAILED(created)||!upper||
       callback.substitutions!=1||wrapperCalls!=1||!route->issued())return 22;
    const auto directTest=callback.facade->Present(0,DXGI_PRESENT_TEST);
    const auto upperTest=upper->Present(0,DXGI_PRESENT_TEST);
    std::cout<<"direct/upper TEST=0x"<<std::hex<<
        static_cast<unsigned>(directTest)<<"/0x"<<
        static_cast<unsigned>(upperTest)<<std::dec<<'\n';
    ComPtr<ID3D11Texture2D> colour;
    ComPtr<ID3D11RenderTargetView> view;
    if(FAILED(upper->GetBuffer(0,IID_PPV_ARGS(&colour)))||
       FAILED(d11->CreateRenderTargetView(colour.Get(),nullptr,&view)))return 23;
    const float red[4]{1.f,0.f,0.f,1.f};
    context->ClearRenderTargetView(view.Get(),red);
    const auto presented=upper->Present(0,0);
    std::cout<<"Game route first Present=0x"<<std::hex<<
        static_cast<unsigned>(presented)<<std::dec<<'\n';
    if(FAILED(presented))return 24;
    view.Reset();colour.Reset();context->ClearState();context->Flush();
    if(FAILED(upper->ResizeBuffers(2,192,108,
        DXGI_FORMAT_R8G8B8A8_UNORM,0)))return 25;
    DXGI_SWAP_CHAIN_DESC after{};
    if(FAILED(upper->GetDesc(&after))||after.BufferDesc.Width!=192||
       after.BufferDesc.Height!=108)return 26;
    upper.Reset();context->ClearState();context->Flush();
    std::cout<<"Game route FG-Off: substitution=1, Present=ok, resize=192x108\n";
    callback.facade.Reset();
    std::cout<<"facade released\n";
    wrapper.Reset();
    std::cout<<"wrapper released\n";
    route.reset();
    std::cout<<"route shutdown\n";
    context.Reset();d11.Reset();
    std::cout<<"D3D11 released\n";
    adapter.Reset();
    std::cout<<"adapter/factory released\n";
    FreeLibrary(reshade.value);reshade.value=nullptr;
    std::cout<<"ReShade released\n";
    return 0;
}
}
int wmain(int argc,wchar_t** argv) {
    std::cout.setf(std::ios::unitbuf);
    if(argc!=3)return 1;
    try {return run(argv[1],argv[2]);}
    catch(const std::exception& error) {
        std::cerr<<error.what()<<'\n';return 2;
    }
}
