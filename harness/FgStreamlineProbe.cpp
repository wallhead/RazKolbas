#include <sl.h>
#include <sl_dlss_g.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <cstdint>
#include <cwchar>
#include <iostream>

using Microsoft::WRL::ComPtr;

namespace {
int code(sl::Result result) { return static_cast<int>(result); }
template<class T> void attachUpgrade(Microsoft::WRL::ComPtr<T>& proxy,
    T* original,T* upgraded) {
    if(upgraded==original)proxy=original;
    else proxy.Attach(upgraded);
}
template<class T> sl::Result useProxy(T* original,
    Microsoft::WRL::ComPtr<T>& proxy,const char* label) {
    void* native{};
    const auto unwrapped=slGetNativeInterface(original,&native);
    ComPtr<IUnknown> nativeOwner;
    if(unwrapped==sl::Result::eOk&&native)
        nativeOwner.Attach(static_cast<IUnknown*>(native));
    std::cout<<"slGetNativeInterface("<<label<<")="<<code(unwrapped)<<
        " distinct="<<(native&&native!=original)<<'\n';
    if(unwrapped==sl::Result::eOk&&native&&native!=original) {
        proxy=original;
        return sl::Result::eOk;
    }
    auto* upgraded=original;
    const auto result=slUpgradeInterface(reinterpret_cast<void**>(&upgraded));
    std::cout<<"slUpgradeInterface("<<label<<")="<<code(result)<<'\n';
    if(result==sl::Result::eOk)attachUpgrade(proxy,original,upgraded);
    return result;
}
int probeSwap(IDXGIFactory6* factory,ID3D12Device* device) {
    ComPtr<ID3D12Device> proxyDevice;
    const auto deviceUpgrade=useProxy(device,proxyDevice,"device");
    if(deviceUpgrade!=sl::Result::eOk)return 9;
    ComPtr<IDXGIFactory6> proxyFactory;
    const auto factoryUpgrade=useProxy(factory,proxyFactory,"factory");
    if(factoryUpgrade!=sl::Result::eOk)return 10;
    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    ComPtr<ID3D12CommandQueue> queue;
    const auto madeQueue=proxyDevice->CreateCommandQueue(&queueDesc,
        IID_PPV_ARGS(&queue));
    std::cout<<"CreateCommandQueue=0x"<<std::hex<<
        static_cast<std::uint32_t>(madeQueue)<<std::dec<<'\n';
    if(FAILED(madeQueue))return 11;
    const auto window=CreateWindowExW(0,L"STATIC",L"RazKolbas FG SL probe",
        WS_POPUP,0,0,96,72,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    if(!window)return 12;
    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width=96;desc.Height=72;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount=2;desc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
    ComPtr<IDXGISwapChain1> swap;
    const auto madeSwap=proxyFactory->CreateSwapChainForHwnd(queue.Get(),
        window,&desc,nullptr,nullptr,&swap);
    std::cout<<"CreateSwapChainForHwnd=0x"<<std::hex<<
        static_cast<std::uint32_t>(madeSwap)<<std::dec<<'\n';
    int result=0;
    if(FAILED(madeSwap))result=13;
    else {
        void* nativeSwap{};
        const auto native=slGetNativeInterface(swap.Get(),&nativeSwap);
        ComPtr<IUnknown> nativeSwapOwner;
        if(native==sl::Result::eOk&&nativeSwap)
            nativeSwapOwner.Attach(static_cast<IUnknown*>(nativeSwap));
        std::cout<<"slGetNativeInterface(swap)="<<code(native)<<
            " distinct="<<(nativeSwap&&nativeSwap!=swap.Get())<<'\n';
        if(native!=sl::Result::eOk)result=14;
        const sl::ViewportHandle viewport{0u};
        sl::DLSSGOptions options{};
        options.mode=sl::DLSSGMode::eOff;
        const auto mode=slDLSSGSetOptions(viewport,options);
        std::cout<<"slDLSSGSetOptions(Off)="<<code(mode)<<'\n';
        if(mode!=sl::Result::eOk&&!result)result=15;
        ComPtr<IDXGISwapChain3> swap3;
        const auto qi=swap.As(&swap3);
        std::cout<<"QueryInterface(IDXGISwapChain3)=0x"<<std::hex<<
            static_cast<std::uint32_t>(qi)<<std::dec<<'\n';
        if(FAILED(qi)&&!result)result=16;
        if(swap3) {
            std::cout<<"GetCurrentBackBufferIndex="<<
                swap3->GetCurrentBackBufferIndex()<<'\n';
        }
        const auto test=swap->Present(0,DXGI_PRESENT_TEST);
        const auto presented=swap->Present(0,0);
        std::cout<<"Present(TEST)=0x"<<std::hex<<
            static_cast<std::uint32_t>(test)<<" Present=0x"<<
            static_cast<std::uint32_t>(presented)<<std::dec<<'\n';
        if(FAILED(test)||FAILED(presented))if(!result)result=17;
        sl::DLSSGState state{};
        const auto stateResult=slDLSSGGetState(viewport,state,nullptr);
        std::cout<<"slDLSSGGetState="<<code(stateResult);
        if(stateResult==sl::Result::eOk)
            std::cout<<" status=0x"<<std::hex<<
                static_cast<std::uint32_t>(state.status)<<std::dec<<
                " actualPresented="<<state.numFramesActuallyPresented<<
                " maxExtra="<<state.numFramesToGenerateMax;
        std::cout<<'\n';
        if(stateResult!=sl::Result::eOk&&!result)result=18;
        const auto resized=swap->ResizeBuffers(2,128,80,
            DXGI_FORMAT_R8G8B8A8_UNORM,0);
        std::cout<<"ResizeBuffers=0x"<<std::hex<<
            static_cast<std::uint32_t>(resized)<<std::dec<<'\n';
        if(FAILED(resized)&&!result)result=19;
    }
    swap.Reset();
    queue.Reset();
    proxyFactory.Reset();
    proxyDevice.Reset();
    DestroyWindow(window);
    return result;
}
}

int wmain(int argc,wchar_t** argv) {
    if((argc!=2&&argc!=3)||(argc==3&&std::wcscmp(argv[2],L"--swap"))) {
        std::wcerr<<L"Usage: RazKolbasFgStreamlineProbe <absolute SDK bin/x64> [--swap]\n";
        return 1;
    }
    const wchar_t* pluginPaths[]{argv[1]};
    const sl::Feature features[]{sl::kFeatureDLSS_G,sl::kFeatureReflex};
    sl::Preferences preferences{};
    preferences.pathsToPlugins=pluginPaths;
    preferences.numPathsToPlugins=1;
    preferences.featuresToLoad=features;
    preferences.numFeaturesToLoad=2;
    preferences.flags=sl::PreferenceFlags::eDisableCLStateTracking|
        sl::PreferenceFlags::eUseManualHooking|
        sl::PreferenceFlags::eUseFrameBasedResourceTagging;
    preferences.engine=sl::EngineType::eCustom;
    preferences.engineVersion="0.1.115";
    preferences.projectId="b3340e44-a57e-4b98-9318-d7150829d110";
    preferences.renderAPI=sl::RenderAPI::eD3D12;
    const auto initialized=slInit(preferences);
    std::cout<<"slInit="<<code(initialized)<<'\n';
    if(initialized!=sl::Result::eOk)return 2;

    int exitCode=0;
    // SL shutdown precedes destruction of every DXGI/D3D12 interface.
    ComPtr<IDXGIFactory6> factory;
    ComPtr<IDXGIAdapter1> adapter;
    ComPtr<ID3D12Device> device;
    {
        if(FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))exitCode=3;
        DXGI_ADAPTER_DESC1 desc{};
        if(!exitCode) {
            for(UINT index=0;factory->EnumAdapterByGpuPreference(index,
                DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                IID_PPV_ARGS(&adapter))!=DXGI_ERROR_NOT_FOUND;++index) {
                if(SUCCEEDED(adapter->GetDesc1(&desc))&&desc.VendorId==0x10de)break;
                adapter.Reset();
            }
            if(!adapter)exitCode=4;
        }
        if(!exitCode) {
            std::cout<<"adapterVendor=0x"<<std::hex<<desc.VendorId
                <<" device=0x"<<desc.DeviceId<<std::dec<<'\n';
            sl::AdapterInfo info{};
            info.deviceLUID=reinterpret_cast<std::uint8_t*>(&desc.AdapterLuid);
            info.deviceLUIDSizeInBytes=sizeof(desc.AdapterLuid);
            const auto support=slIsFeatureSupported(sl::kFeatureDLSS_G,info);
            std::cout<<"slIsFeatureSupported(DLSS-G)="<<code(support)<<'\n';
            sl::FeatureRequirements requirements{};
            const auto queried=slGetFeatureRequirements(sl::kFeatureDLSS_G,
                requirements);
            std::cout<<"slGetFeatureRequirements(DLSS-G)="<<code(queried);
            if(queried==sl::Result::eOk)
                std::cout<<" flags=0x"<<std::hex<<
                    static_cast<std::uint32_t>(requirements.flags)<<std::dec;
            std::cout<<'\n';
            const auto created=D3D12CreateDevice(adapter.Get(),
                D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device));
            std::cout<<"D3D12CreateDevice=0x"<<std::hex<<
                static_cast<std::uint32_t>(created)<<std::dec<<'\n';
            if(FAILED(created))exitCode=5;
            else {
                const auto bound=slSetD3DDevice(device.Get());
                std::cout<<"slSetD3DDevice="<<code(bound)<<'\n';
                if(bound!=sl::Result::eOk)exitCode=6;
                else if(argc==3)exitCode=probeSwap(factory.Get(),device.Get());
            }
            if(support!=sl::Result::eOk||queried!=sl::Result::eOk)
                if(!exitCode)exitCode=7;
        }
    }
    const auto stopped=slShutdown();
    std::cout<<"slShutdown="<<code(stopped)<<'\n';
    if(stopped!=sl::Result::eOk&&exitCode==0)exitCode=8;
    return exitCode;
}
