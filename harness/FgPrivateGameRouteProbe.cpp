#include "rk/FgPrivateSwapRoute.hpp"
#include "rk/FgD3D11SwapFacade.hpp"
#include "rk/FactoryCreateTrace.hpp"
#include "rk/OwnedRouteProfile.hpp"
#include "rk/PatchDescriptor.hpp"
#include "rk/PointerPatch.hpp"
#include <d3d11.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <iostream>
#include <array>
#include <cstring>
#include <optional>
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
bool sameIdentity(IUnknown* first,IUnknown* second) noexcept {
    ComPtr<IUnknown> a,b;
    return first&&second&&SUCCEEDED(first->QueryInterface(IID_PPV_ARGS(&a)))&&
        SUCCEEDED(second->QueryInterface(IID_PPV_ARGS(&b)))&&a.Get()==b.Get();
}
std::optional<std::array<unsigned char,4>> firstPixel(
    ID3D11Device* device,ID3D11DeviceContext* context,
    ID3D11Texture2D* source) {
    if(!device||!context||!source)return std::nullopt;
    D3D11_TEXTURE2D_DESC desc{};
    source->GetDesc(&desc);
    if(!desc.Width||!desc.Height||desc.SampleDesc.Count!=1||
       desc.Format!=DXGI_FORMAT_R8G8B8A8_UNORM)return std::nullopt;
    desc.Usage=D3D11_USAGE_STAGING;
    desc.BindFlags=0;
    desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    desc.MiscFlags=0;
    ComPtr<ID3D11Texture2D> staging;
    if(FAILED(device->CreateTexture2D(&desc,nullptr,&staging)))return std::nullopt;
    context->CopyResource(staging.Get(),source);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if(FAILED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)))
        return std::nullopt;
    const auto* data=static_cast<const unsigned char*>(mapped.pData);
    std::array<unsigned char,4> pixel{data[0],data[1],data[2],data[3]};
    context->Unmap(staging.Get(),0);
    return pixel;
}
void reportPixel(const char* stage,
    const std::optional<std::array<unsigned char,4>>& pixel) {
    std::cout<<stage<<" pixel=";
    if(!pixel)std::cout<<"unavailable";
    else for(const auto channel:*pixel)std::cout<<static_cast<unsigned>(channel)<<',';
    std::cout<<'\n';
}
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
int run(const wchar_t* runtimeDirectory,const wchar_t* reshadePath,
    const wchar_t* enbPath,bool failAfterSrv) {
    const auto hash=rk::sha256File(reshadePath);
    if(!std::holds_alternative<std::string>(hash)||
       std::get<std::string>(hash)!=rk::reshade680FactoryCreateSite().moduleSha256)
        return 13;
    Module reshade{LoadLibraryW(reshadePath)};
    if(!reshade.value)return 14;
    Module enb;
    if(enbPath) {
        const auto enbHash=rk::sha256File(enbPath);
        if(!std::holds_alternative<std::string>(enbHash)||
           std::get<std::string>(enbHash)!=
               "35ff1543c8aaa5435a9002dc58d5459c29557ce8e5e5f91b25dfe4645be7bae3")return 35;
        enb.value=LoadLibraryW(enbPath);
        if(!enb.value)return 36;
        std::cout<<"Exact ENB 0.505 preloaded for the outer Present probe\n";
    }
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
    Window window{CreateWindowExW(0,L"STATIC",L"RazKolbas game FG route",
        WS_POPUP,0,0,160,96,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr)};
    if(!window.value)return 16;
    DXGI_SWAP_CHAIN_DESC game{};
    game.BufferDesc.Width=160;game.BufferDesc.Height=96;
    game.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    game.SampleDesc.Count=1;game.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT|DXGI_USAGE_SHADER_INPUT;
    game.BufferCount=3;game.OutputWindow=window.value;game.Windowed=TRUE;
    game.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
    game.Flags=DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    auto prepared=rk::FgPrivateSwapRoute::prepare(adapter.Get(),game,
        runtimeDirectory);
    if(const auto* error=std::get_if<rk::Error>(&prepared)) {
        std::cerr<<"game-route preparation: "<<error->message<<'\n';
        return 17;
    }
    auto route=std::move(std::get<std::unique_ptr<rk::FgPrivateSwapRoute>>(
        prepared));
    // Reproduce the live chain: a downstream owner enables tearing after
    // the early private lower was prepared, before native game creation.
    game.Flags|=DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    ComPtr<ID3D11Device> d11;
    ComPtr<ID3D11DeviceContext> context;
    if(!enb.value&&FAILED(D3D11CreateDevice(adapter.Get(),D3D_DRIVER_TYPE_UNKNOWN,
        nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,
        &d11,nullptr,&context)))return 15;
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
    std::vector<std::uint8_t> nativeImage(site.imageSize);
    const auto nativeBase=reinterpret_cast<std::uintptr_t>(owner);
    const auto codeFacts=rk::inspectFactoryMethodCode(facts.createMethod);
    std::cout<<"Native factory code: protection=0x"<<std::hex<<
        codeFacts.protection<<"; entry=";
    for(const auto byte:std::span(codeFacts.bytes).first(16))
        std::cout<<static_cast<unsigned>(byte)<<' ';
    std::cout<<std::dec<<'\n';
    if(!codeFacts.readable)return 31;
    std::memcpy(nativeImage.data()+site.tableRva+site.slot*sizeof(void*),
        reinterpret_cast<const void*>(facts.vtable+site.slot*sizeof(void*)),
        sizeof(void*));
    std::memcpy(nativeImage.data()+site.methodRva,
        codeFacts.bytes.data(),site.prologue.size());
    auto nativeValidated=rk::validateOwnedRouteSite(nativeImage,nativeBase,
        std::get<std::string>(nativeHash),std::filesystem::file_size(nativePath),
        site.tableRva,site);
    if(std::holds_alternative<rk::Error>(nativeValidated)) {
        const auto compatible=rk::inspectAndPinSteamFactoryInline(facts.createMethod);
        if(std::holds_alternative<bool>(compatible)&&std::get<bool>(compatible)) {
            std::copy_n(site.prologue.begin(),5,nativeImage.begin()+site.methodRva);
            nativeValidated=rk::validateOwnedRouteSite(nativeImage,nativeBase,
                std::get<std::string>(nativeHash),std::filesystem::file_size(nativePath),
                site.tableRva,site);
            std::cout<<"Exact Steam native factory chain accepted; inline hook preserved\n";
        } else if(const auto* error=std::get_if<rk::Error>(&compatible)) {
            std::cerr<<"Steam compatibility: "<<error->message<<'\n';
        }
    }
    if(const auto* error=std::get_if<rk::Error>(&nativeValidated)) {
        std::cerr<<"production native validation: "<<error->message<<"; live16=";
        for(const auto byte:std::span(nativeImage).subspan(site.methodRva,16))
            std::cerr<<std::hex<<static_cast<unsigned>(byte)<<' ';
        std::cerr<<std::dec<<'\n';
        return 31;
    }
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
    // Early probe failures need the same ownership order as successful exit.
    // A returned ENB/ReShade object must be released before slShutdown.
    struct OrderedExit {
        ComPtr<IDXGISwapChain>& upper;
        Callback& callback;
        ComPtr<IDXGIFactory1>& wrapper;
        std::unique_ptr<rk::FgPrivateSwapRoute>& route;
        ComPtr<ID3D11DeviceContext>& context;
        ComPtr<ID3D11Device>& d11;
        ComPtr<IDXGIAdapter1>& adapter;
        rk::PointerPatch& wrapperPatch;
        rk::PointerPatch& nativePatch;
        ~OrderedExit() {
            if(std::holds_alternative<rk::Error>(wrapperPatch.restore())||
               std::holds_alternative<rk::Error>(nativePatch.restore()))
                std::terminate();
            active=nullptr;wrapperNext=nullptr;
            if(context) {context->ClearState();context->Flush();}
            upper.Reset();callback.facade.Reset();wrapper.Reset();route.reset();
            context.Reset();d11.Reset();adapter.Reset();
        }
    } orderedExit{upper,callback,wrapper,route,context,d11,adapter,wrapperPatch,patch};
    HRESULT created{};
    if(enb.value) {
        auto* createDevice=reinterpret_cast<PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN>(
            GetProcAddress(enb.value,"D3D11CreateDeviceAndSwapChain"));
        if(!createDevice)return 37;
        created=createDevice(adapter.Get(),D3D_DRIVER_TYPE_UNKNOWN,nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,
            &game,&upper,&d11,nullptr,&context);
        std::cout<<"ENB device/swap creation=0x"<<std::hex<<
            static_cast<unsigned>(created)<<std::dec<<'\n';
    } else created=wrapper->CreateSwapChain(d11.Get(),&game,&upper);
    const auto wrapperRestored=wrapperPatch.restore();
    const auto restored=patch.restore();
    active=nullptr;
    if(!std::holds_alternative<bool>(restored)||
       !std::holds_alternative<bool>(wrapperRestored)||FAILED(created)||!upper||
       callback.substitutions!=1||wrapperCalls!=1||!route->issued())return 22;
    const auto& diagnostics=static_cast<rk::FgD3D11SwapFacade*>(callback.facade.Get())->diagnostics();
    const auto report=[&] {
        std::cout<<"Bridge copy="<<diagnostics.copyPhase()<<" hr=0x"<<std::hex<<
            static_cast<unsigned>(diagnostics.copyResult())<<std::dec<<
            "; present="<<diagnostics.presentPhase()<<
            "; detail="<<diagnostics.copyDetail()<<
            "; first-failure="<<diagnostics.firstCopyFailurePhase()<<"/0x"<<
            std::hex<<static_cast<unsigned>(diagnostics.firstCopyFailure())<<std::dec<<'\n';
    };
    report();
    const auto directTest=callback.facade->Present(0,DXGI_PRESENT_TEST);
    const auto upperTest=upper->Present(0,DXGI_PRESENT_TEST);
    std::cout<<"direct/upper TEST=0x"<<std::hex<<
        static_cast<unsigned>(directTest)<<"/0x"<<
        static_cast<unsigned>(upperTest)<<std::dec<<'\n';
    ComPtr<ID3D11Texture2D> colour;
    ComPtr<ID3D11RenderTargetView> view;
    if(FAILED(upper->GetBuffer(0,IID_PPV_ARGS(&colour)))||
       FAILED(d11->CreateRenderTargetView(colour.Get(),nullptr,&view)))return 23;
    ComPtr<ID3D11Texture2D> innerColour;
    if(FAILED(callback.facade->GetBuffer(0,IID_PPV_ARGS(&innerColour))))return 43;
    D3D11_TEXTURE2D_DESC innerColourDesc{};
    innerColour->GetDesc(&innerColourDesc);
    ComPtr<ID3D11ShaderResourceView> sampleView;
    D3D11_TEXTURE2D_DESC colourDesc{};
    colour->GetDesc(&colourDesc);
    std::cout<<"Colour handoff upper/inner sameIdentity="<<
        sameIdentity(colour.Get(),innerColour.Get())<<
        "; upper format="<<static_cast<unsigned>(colourDesc.Format)<<
        " bind=0x"<<std::hex<<colourDesc.BindFlags<<
        " misc=0x"<<colourDesc.MiscFlags<<
        "; inner format="<<std::dec<<static_cast<unsigned>(innerColourDesc.Format)<<
        " bind=0x"<<std::hex<<innerColourDesc.BindFlags<<
        " misc=0x"<<innerColourDesc.MiscFlags<<std::dec<<'\n';
    const auto sampled=d11->CreateShaderResourceView(colour.Get(),nullptr,&sampleView);
    std::cout<<"Game shader-input contract: bind=0x"<<std::hex<<colourDesc.BindFlags<<
        "; SRV=0x"<<static_cast<unsigned>(sampled)<<std::dec<<'\n';
    if(FAILED(sampled))return 41;
    sampleView.Reset();
    if(failAfterSrv) {
        std::cout<<"Intentional early failure after SRV; ordered cleanup required\n";
        return 42;
    }
    const float red[4]{1.f,0.f,0.f,1.f};
    context->ClearRenderTargetView(view.Get(),red);
    reportPixel("Colour upper before Present",firstPixel(d11.Get(),context.Get(),colour.Get()));
    reportPixel("Colour inner before Present",firstPixel(d11.Get(),context.Get(),innerColour.Get()));
    DXGI_SWAP_CHAIN_DESC current{};
    if(FAILED(upper->GetDesc(&current)))return 34;
    std::cout<<"Game requested/lower flags=0x"<<std::hex<<game.Flags<<"/0x"<<current.Flags<<std::dec<<'\n';
    const auto presented=upper->Present(0,DXGI_PRESENT_ALLOW_TEARING);
    std::cout<<"Game route first Present=0x"<<std::hex<<
        static_cast<unsigned>(presented)<<std::dec<<'\n';
    report();
    if(FAILED(presented))return 24;
    reportPixel("Colour upper after Present",firstPixel(d11.Get(),context.Get(),colour.Get()));
    reportPixel("Colour inner after Present",firstPixel(d11.Get(),context.Get(),innerColour.Get()));
    innerColour.Reset();
    for(unsigned frame=1;frame<120;++frame) {
        const float next[4]{frame%2?1.f:0.f,frame%3?0.f:1.f,0.f,1.f};
        context->ClearRenderTargetView(view.Get(),next);
        if(FAILED(upper->Present(0,DXGI_PRESENT_ALLOW_TEARING)))return 38;
    }
    view.Reset();colour.Reset();context->ClearState();context->Flush();
    if(FAILED(upper->ResizeBuffers(3,192,108,
        DXGI_FORMAT_R8G8B8A8_UNORM,current.Flags)))return 25;
    DXGI_SWAP_CHAIN_DESC after{};
    if(FAILED(upper->GetDesc(&after))||after.BufferDesc.Width!=192||
       after.BufferDesc.Height!=108)return 26;
    if(FAILED(upper->GetBuffer(0,IID_PPV_ARGS(&colour)))||
       FAILED(d11->CreateRenderTargetView(colour.Get(),nullptr,&view)))return 39;
    for(unsigned frame=0;frame<120;++frame) {
        context->ClearRenderTargetView(view.Get(),red);
        if(FAILED(upper->Present(0,DXGI_PRESENT_ALLOW_TEARING)))return 40;
    }
    view.Reset();colour.Reset();context->ClearState();context->Flush();
    upper.Reset();context->ClearState();context->Flush();
    std::cout<<"Game route FG-Off: substitution=1, Present=240 frames ok, resize=192x108\n";
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
    if(argc<3||argc>6)return 1;
    const bool failAfterSrv=argc==6&&std::wcscmp(argv[5],L"fail-after-srv")==0;
    if(argc==6&&!failAfterSrv)return 1;
    if(argc>=4&&std::wcscmp(argv[3],L"-")!=0) {
        const auto hash=rk::sha256File(argv[3]);
        const auto& profile=rk::steamFactoryInlineProfile();
        if(!std::holds_alternative<std::string>(hash)||
           std::get<std::string>(hash)!=profile.moduleHash||
           std::filesystem::file_size(argv[3])!=profile.fileSize)return 32;
        // Keep this explicit research preload resident until process exit.
        // Steam may install callbacks even if a later probe stage rejects.
        if(!LoadLibraryW(argv[3]))return 33;
        std::cout<<"Exact Steam overlay preloaded for the factory-chain probe\n";
    } else std::cout<<"No Steam overlay preload; pristine native factory required\n";
    try {return run(argv[1],argv[2],argc>=5?argv[4]:nullptr,failAfterSrv);}
    catch(const std::exception& error) {
        std::cerr<<error.what()<<'\n';return 2;
    }
}
