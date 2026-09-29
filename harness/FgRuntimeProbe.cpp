#include "rk/FgStreamlineRuntime.hpp"
#include "rk/FactoryCreateTrace.hpp"
#include "rk/OwnedRouteProfile.hpp"
#include "rk/PatchDescriptor.hpp"
#include <sl_dlss_g.h>
#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <iostream>
#include <variant>

int wmain(int argc,wchar_t** argv) {
    if(argc!=2&&argc!=3&&
       !(argc==4&&(std::wcscmp(argv[2],L"--early-reshade")==0||
           std::wcscmp(argv[2],L"--native-first")==0)))return 1;
    HMODULE reshade{};
    Microsoft::WRL::ComPtr<IDXGIFactory1> wrapper;
    const auto reshadePath=argc==3?argv[2]:argc==4?argv[3]:nullptr;
    const auto createWrapper=[&]() -> int {
        const auto& site=rk::reshade680FactoryCreateSite();
        const auto digest=rk::sha256File(reshadePath);
        if(!std::holds_alternative<std::string>(digest)||
           std::get<std::string>(digest)!=site.moduleSha256)return 5;
        reshade=LoadLibraryW(reshadePath);
        if(!reshade)return 6;
        using Create=HRESULT(WINAPI*)(REFIID,void**);
        const auto create=reinterpret_cast<Create>(
            GetProcAddress(reshade,"CreateDXGIFactory1"));
        if(!create||FAILED(create(IID_PPV_ARGS(&wrapper))))return 7;
        const auto before=rk::inspectReshadeFactoryDelegate(wrapper.Get());
        if(!before.methodExecutable)return 8;
        std::cout<<"created factory method=0x"<<std::hex<<
            before.createMethod<<std::dec<<'\n';
        return 0;
    };
    if(argc==3)if(const auto result=createWrapper())return result;
    auto loaded=rk::FgStreamlineRuntime::initialize(argv[1]);
    if(const auto error=std::get_if<rk::Error>(&loaded)) {
        std::cerr<<"private-sl-init: "<<error->message<<'\n';
        return 2;
    }
    auto runtime=std::move(std::get<std::unique_ptr<rk::FgStreamlineRuntime>>(
        loaded));
    std::wcout<<L"private-sl-init=ok directory="<<runtime->directory()<<L'\n';
    if(runtime->setD3DDevice(nullptr)!=sl::Result::eErrorInvalidParameter||
       runtime->upgradeInterface(nullptr)!=sl::Result::eErrorInvalidParameter)
        return 3;
    Microsoft::WRL::ComPtr<ID3D12Device> d12;
    if(reshadePath) {
        Microsoft::WRL::ComPtr<IDXGIFactory1> nativeFactory;
        if(FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&nativeFactory))))return 13;
        for(UINT index=0;;++index) {
            Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
            if(nativeFactory->EnumAdapters1(index,&adapter)==DXGI_ERROR_NOT_FOUND)
                break;
            DXGI_ADAPTER_DESC1 description{};
            if(SUCCEEDED(adapter->GetDesc1(&description))&&
               description.VendorId==0x10de&&
               SUCCEEDED(D3D12CreateDevice(adapter.Get(),
                   D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&d12))))break;
        }
        if(!d12)return 14;
        const auto set=runtime->setD3DDevice(d12.Get());
        std::cout<<"slSetD3DDevice="<<static_cast<int>(set)<<'\n';
        if(set!=sl::Result::eOk)return 15;
    }
    Microsoft::WRL::ComPtr<IDXGIFactory6> nativeFactory6;
    Microsoft::WRL::ComPtr<IDXGIFactory6> nativeProxy6;
    if(argc==4&&std::wcscmp(argv[2],L"--native-first")==0) {
        if(FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&nativeFactory6))))return 16;
        auto* upgraded=nativeFactory6.Get();
        void* nativeCheck{};
        const auto nativeFactoryResult=runtime->getNativeInterface(
            nativeFactory6.Get(),&nativeCheck);
        std::cout<<"native6 get-native="<<
            static_cast<int>(nativeFactoryResult)<<" distinct="<<
            (nativeCheck&&nativeCheck!=nativeFactory6.Get())<<'\n';
        if(nativeCheck)static_cast<IUnknown*>(nativeCheck)->Release();
        const auto result=runtime->upgradeInterface(
            reinterpret_cast<void**>(&upgraded));
        std::cout<<"native6 upgrade="<<static_cast<int>(result)<<
            " distinct="<<(upgraded!=nativeFactory6.Get())<<'\n';
        if(result!=sl::Result::eOk)return 17;
        if(upgraded==nativeFactory6.Get())nativeProxy6=upgraded;
        else nativeProxy6.Attach(upgraded);
        auto* upgradedDevice=d12.Get();
        nativeCheck=nullptr;
        const auto nativeDeviceResult=runtime->getNativeInterface(
            d12.Get(),&nativeCheck);
        std::cout<<"native device get-native="<<
            static_cast<int>(nativeDeviceResult)<<" distinct="<<
            (nativeCheck&&nativeCheck!=d12.Get())<<'\n';
        if(nativeCheck)static_cast<IUnknown*>(nativeCheck)->Release();
        const auto deviceUpgrade=runtime->upgradeInterface(
            reinterpret_cast<void**>(&upgradedDevice));
        std::cout<<"native device upgrade="<<
            static_cast<int>(deviceUpgrade)<<" distinct="<<
            (upgradedDevice!=d12.Get())<<'\n';
        if(deviceUpgrade!=sl::Result::eOk)return 18;
        Microsoft::WRL::ComPtr<ID3D12Device> proxyDevice;
        if(upgradedDevice==d12.Get())proxyDevice=upgradedDevice;
        else proxyDevice.Attach(upgradedDevice);
        D3D12_COMMAND_QUEUE_DESC queueDesc{};
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue;
        if(FAILED(proxyDevice->CreateCommandQueue(&queueDesc,
            IID_PPV_ARGS(&queue))))return 19;
        const auto hwnd=CreateWindowExW(0,L"STATIC",L"RazKolbas private FG probe",
            WS_POPUP,0,0,96,72,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        if(!hwnd)return 20;
        DXGI_SWAP_CHAIN_DESC1 desc{};
        desc.Width=96;desc.Height=72;
        desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count=1;
        desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount=2;
        desc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
        Microsoft::WRL::ComPtr<IDXGISwapChain1> lower;
        const auto made=nativeProxy6->CreateSwapChainForHwnd(queue.Get(),
            hwnd,&desc,nullptr,nullptr,&lower);
        std::cout<<"private proxy lower swap=0x"<<std::hex<<
            static_cast<unsigned>(made)<<std::dec<<'\n';
        if(SUCCEEDED(made)) {
            nativeCheck=nullptr;
            const auto nativeSwapResult=runtime->getNativeInterface(
                lower.Get(),&nativeCheck);
            std::cout<<"lower swap get-native="<<
                static_cast<int>(nativeSwapResult)<<" distinct="<<
                (nativeCheck&&nativeCheck!=lower.Get())<<'\n';
            if(nativeCheck)static_cast<IUnknown*>(nativeCheck)->Release();
            void* function{};
            const auto resolved=runtime->getFeatureFunction(
                sl::kFeatureDLSS_G,"slDLSSGSetOptions",function);
            std::cout<<"FG options function="<<
                static_cast<int>(resolved)<<" present="<<(function!=nullptr)<<'\n';
            if(resolved==sl::Result::eOk&&function) {
                const auto set=reinterpret_cast<PFun_slDLSSGSetOptions*>(
                    function);
                sl::DLSSGOptions options{};
                options.mode=sl::DLSSGMode::eOff;
                const auto applied=set(sl::ViewportHandle{0u},options);
                std::cout<<"FG Off options="<<static_cast<int>(applied)<<'\n';
            }
        }
        lower.Reset();
        DestroyWindow(hwnd);
        if(FAILED(made))return 21;
        Microsoft::WRL::ComPtr<ID3D11Device> d11;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
        D3D_FEATURE_LEVEL level{};
        const auto d11Result=D3D11CreateDevice(nullptr,
            D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            nullptr,0,D3D11_SDK_VERSION,&d11,&level,&context);
        std::cout<<"private probe D3D11=0x"<<std::hex<<
            static_cast<unsigned>(d11Result)<<std::dec<<'\n';
        if(FAILED(d11Result))return 22;
    }
    if(argc==4)if(const auto result=createWrapper())return result;
    if(wrapper) {
        const auto before=rk::inspectReshadeFactoryDelegate(wrapper.Get());
        auto* delegate=reinterpret_cast<IDXGIFactory*>(before.delegate);
        if(!delegate)return 9;
        void* upgraded=delegate;
        const auto upgradeResult=runtime->upgradeInterface(&upgraded);
        std::cout<<"post-init factory upgrade="<<
            static_cast<int>(upgradeResult)<<" distinct="<<
            (upgraded!=delegate)<<'\n';
        if(upgradeResult!=sl::Result::eOk)return 10;
        const auto upgradedMethod=reinterpret_cast<std::uintptr_t>(
            (*reinterpret_cast<void***>(upgraded))[10]);
        HMODULE upgradedOwner{};
        if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
            reinterpret_cast<LPCWSTR>(upgradedMethod),&upgradedOwner))return 11;
        wchar_t upgradedPath[32768]{};
        GetModuleFileNameW(upgradedOwner,upgradedPath,32768);
        std::wcout<<L"upgraded method owner="<<upgradedPath<<L'\n';
        std::cout<<"upgraded method RVA=0x"<<std::hex<<
            (upgradedMethod-reinterpret_cast<std::uintptr_t>(upgradedOwner))
            <<std::dec<<'\n';
        const auto upgradedHash=rk::sha256File(upgradedPath);
        const bool upgradedVerified=upgraded!=delegate&&
            std::holds_alternative<std::string>(upgradedHash)&&
            std::get<std::string>(upgradedHash)==
                rk::streamline2141FactoryCreateSite().moduleSha256;
        FreeLibrary(upgradedOwner);
        if(upgraded!=delegate)static_cast<IUnknown*>(upgraded)->Release();
        if(!upgradedVerified)return 12;
        const auto after=rk::inspectReshadeFactoryDelegate(wrapper.Get());
        HMODULE owner{};
        if(!after.methodExecutable||
           !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
               reinterpret_cast<LPCWSTR>(after.createMethod),&owner))return 11;
        const auto ownerBase=reinterpret_cast<std::uintptr_t>(owner);
        wchar_t path[32768]{};
        const auto count=GetModuleFileNameW(owner,path,32768);
        const auto digest=count&&count<32768?
            rk::sha256File(path):rk::Result<std::string>{rk::Error{
                rk::ErrorCode::Unavailable,"path unavailable"}};
        std::wcout<<L"post-init method owner="<<path<<L'\n';
        std::cout<<"post-init method RVA=0x"<<std::hex<<
            (after.createMethod-ownerBase)<<std::dec<<'\n';
        const bool nativeVerified=std::holds_alternative<std::string>(digest)&&
            std::get<std::string>(digest)==rk::win11DxgiFactoryCreateSite().moduleSha256&&
            after.createMethod==ownerBase+
                rk::win11DxgiFactoryCreateSite().methodRva;
        const bool streamlineVerified=std::holds_alternative<std::string>(digest)&&
            std::get<std::string>(digest)==rk::streamline2141FactoryCreateSite().moduleSha256&&
            after.createMethod==ownerBase+
                rk::streamline2141FactoryCreateSite().methodRva;
        FreeLibrary(owner);
        if(!nativeVerified&&!streamlineVerified)return 12;
        wrapper.Reset();
    }
    d12.Reset();
    const auto stopped=runtime->shutdown();
    if(const auto error=std::get_if<rk::Error>(&stopped)) {
        std::cerr<<"private-sl-shutdown: "<<error->message<<'\n';
        return 4;
    }
    std::cout<<"private-sl-shutdown=ok\n";
    if(reshade)FreeLibrary(reshade);
    return 0;
}
