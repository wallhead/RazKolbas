#include <sl.h>
#include <sl_dlss_g.h>
#include "rk/FgD3D11SwapFacade.hpp"
#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <array>
#include <cstdint>
#include <cwchar>
#include <iostream>
#include <variant>

using Microsoft::WRL::ComPtr;

namespace {
int code(sl::Result result) { return static_cast<int>(result); }
bool sameIdentity(IUnknown* a,IUnknown* b) {
    ComPtr<IUnknown> left,right;
    return a&&b&&SUCCEEDED(a->QueryInterface(IID_PPV_ARGS(&left)))&&
        SUCCEEDED(b->QueryInterface(IID_PPV_ARGS(&right)))&&
        left.Get()==right.Get();
}
bool readLowerPixel(ID3D12Device* device,ID3D12CommandQueue* queue,
    IDXGISwapChain1* swap,UINT index,std::array<std::uint8_t,4>& pixel) {
    ComPtr<ID3D12Resource> back;
    if(FAILED(swap->GetBuffer(index,IID_PPV_ARGS(&back))))return false;
    const auto desc=back->GetDesc();
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT64 bytes{};
    device->GetCopyableFootprints(&desc,0,1,0,&footprint,nullptr,nullptr,&bytes);
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type=D3D12_HEAP_TYPE_READBACK;
    D3D12_RESOURCE_DESC buffer{};
    buffer.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
    buffer.Width=bytes;
    buffer.Height=1;
    buffer.DepthOrArraySize=1;
    buffer.MipLevels=1;
    buffer.SampleDesc.Count=1;
    buffer.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ComPtr<ID3D12Resource> readback;
    if(FAILED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,
        &buffer,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,
        IID_PPV_ARGS(&readback))))return false;
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> commands;
    if(FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&allocator)))||
       FAILED(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,
        allocator.Get(),nullptr,IID_PPV_ARGS(&commands))))return false;
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition={back.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
        D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_COPY_SOURCE};
    commands->ResourceBarrier(1,&barrier);
    D3D12_TEXTURE_COPY_LOCATION from{};
    from.pResource=back.Get();
    from.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    D3D12_TEXTURE_COPY_LOCATION to{};
    to.pResource=readback.Get();
    to.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    to.PlacedFootprint=footprint;
    commands->CopyTextureRegion(&to,0,0,0,&from,nullptr);
    barrier.Transition.StateBefore=D3D12_RESOURCE_STATE_COPY_SOURCE;
    barrier.Transition.StateAfter=D3D12_RESOURCE_STATE_PRESENT;
    commands->ResourceBarrier(1,&barrier);
    if(FAILED(commands->Close()))return false;
    ID3D12CommandList* lists[]{commands.Get()};
    queue->ExecuteCommandLists(1,lists);
    ComPtr<ID3D12Fence> done;
    if(FAILED(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,
        IID_PPV_ARGS(&done)))||FAILED(queue->Signal(done.Get(),1)))
        return false;
    const auto event=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    if(!event)return false;
    const auto armed=done->SetEventOnCompletion(1,event);
    const auto waited=SUCCEEDED(armed)?WaitForSingleObject(event,5000):WAIT_FAILED;
    CloseHandle(event);
    if(waited!=WAIT_OBJECT_0||done->GetCompletedValue()!=1||
       FAILED(device->GetDeviceRemovedReason()))return false;
    void* data{};
    if(FAILED(readback->Map(0,nullptr,&data)))return false;
    const auto* p=static_cast<const std::uint8_t*>(data);
    pixel={p[0],p[1],p[2],p[3]};
    readback->Unmap(0,nullptr);
    return true;
}
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
int probeFacade(IDXGIAdapter1* adapter,ID3D12Device* device,
    ID3D12CommandQueue* queue,IDXGISwapChain1* swap) {
    ComPtr<ID3D12Device> lowerDevice,queueDevice;
    const auto lowerHr=swap->GetDevice(IID_PPV_ARGS(&lowerDevice));
    const auto queueHr=queue->GetDevice(IID_PPV_ARGS(&queueDevice));
    std::cout<<"Facade D3D12 identities lower=0x"<<std::hex<<
        static_cast<std::uint32_t>(lowerHr)<<" queue=0x"<<
        static_cast<std::uint32_t>(queueHr)<<std::dec<<
        " lowerProxy="<<sameIdentity(lowerDevice.Get(),device)<<
        " queueProxy="<<sameIdentity(queueDevice.Get(),device)<<
        " lowerQueue="<<sameIdentity(lowerDevice.Get(),queueDevice.Get())<<'\n';
    void* native{};
    const auto nativeResult=slGetNativeInterface(device,&native);
    ComPtr<IUnknown> nativeOwner;
    if(nativeResult==sl::Result::eOk&&native)
        nativeOwner.Attach(static_cast<IUnknown*>(native));
    ComPtr<ID3D12Device> verifiedNative;
    if(nativeOwner)nativeOwner.As(&verifiedNative);
    std::cout<<"Facade verified SL native="<<code(nativeResult)<<
        " lowerNative="<<sameIdentity(lowerDevice.Get(),
            verifiedNative.Get())<<'\n';
    if(nativeResult!=sl::Result::eOk||
       !sameIdentity(lowerDevice.Get(),verifiedNative.Get()))return 27;
    ComPtr<ID3D11Device> d11;
    ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL level{};
    const auto created=D3D11CreateDevice(adapter,D3D_DRIVER_TYPE_UNKNOWN,
        nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,
        &d11,&level,&context);
    std::cout<<"Facade D3D11CreateDevice=0x"<<std::hex<<
        static_cast<std::uint32_t>(created)<<std::dec<<'\n';
    if(FAILED(created))return 20;
    auto made=rk::FgD3D11SwapFacade::create(d11.Get(),context.Get(),
        device,queue,swap,verifiedNative.Get());
    if(const auto* error=std::get_if<rk::Error>(&made)) {
        std::cout<<"Facade create error="<<static_cast<int>(error->code)<<
            " "<<error->message<<'\n';
        return 21;
    }
    auto facade=std::move(std::get<ComPtr<IDXGISwapChain4>>(made));
    ComPtr<ID3D11Device> owner;
    ComPtr<ID3D12Device> hidden;
    const auto gotD11=facade->GetDevice(IID_PPV_ARGS(&owner));
    const auto gotD12=facade->GetDevice(IID_PPV_ARGS(&hidden));
    std::cout<<"Facade GetDevice D3D11=0x"<<std::hex<<
        static_cast<std::uint32_t>(gotD11)<<" D3D12=0x"<<
        static_cast<std::uint32_t>(gotD12)<<std::dec<<
        " original="<<(owner.Get()==d11.Get())<<'\n';
    if(FAILED(gotD11)||gotD12!=E_NOINTERFACE||owner.Get()!=d11.Get())
        return 22;
    const auto index=facade->GetCurrentBackBufferIndex();
    ComPtr<ID3D11Texture2D> buffer;
    const auto gotBuffer=facade->GetBuffer(index,IID_PPV_ARGS(&buffer));
    std::cout<<"Facade GetBuffer=0x"<<std::hex<<
        static_cast<std::uint32_t>(gotBuffer)<<std::dec<<
        " index="<<index<<'\n';
    if(FAILED(gotBuffer))return 23;
    ComPtr<ID3D11RenderTargetView> view;
    const auto madeView=d11->CreateRenderTargetView(buffer.Get(),nullptr,&view);
    if(FAILED(madeView))return 24;
    const float color[]{0.25f,0.5f,0.75f,1.0f};
    context->ClearRenderTargetView(view.Get(),color);
    const auto test=facade->Present(0,DXGI_PRESENT_TEST);
    const auto present=facade->Present(0,0);
    std::cout<<"Facade Present(TEST)=0x"<<std::hex<<
        static_cast<std::uint32_t>(test)<<" Present=0x"<<
        static_cast<std::uint32_t>(present)<<std::dec<<'\n';
    if(FAILED(test)||FAILED(present))return 25;
    std::array<std::uint8_t,4> pixel{};
    const auto read=readLowerPixel(device,queue,swap,index,pixel);
    std::cout<<"Facade lower pixel="<<read<<" rgba="<<
        static_cast<unsigned>(pixel[0])<<','<<
        static_cast<unsigned>(pixel[1])<<','<<
        static_cast<unsigned>(pixel[2])<<','<<
        static_cast<unsigned>(pixel[3])<<'\n';
    if(!read||pixel[0]<63||pixel[0]>65||
       pixel[1]<127||pixel[1]>129||
       pixel[2]<190||pixel[2]>192||pixel[3]!=255)return 28;
    const sl::ViewportHandle viewport{0u};
    sl::DLSSGState state{};
    const auto stateResult=slDLSSGGetState(viewport,state,nullptr);
    std::cout<<"Facade slDLSSGGetState="<<code(stateResult);
    if(stateResult==sl::Result::eOk)
        std::cout<<" actualPresented="<<
            state.numFramesActuallyPresented<<" maxExtra="<<
            state.numFramesToGenerateMax;
    std::cout<<'\n';
    if(stateResult!=sl::Result::eOk)return 29;
    view.Reset();
    buffer.Reset();
    context->ClearState();
    context->Flush();
    const auto resized=facade->ResizeBuffers(2,128,80,
        DXGI_FORMAT_R8G8B8A8_UNORM,0);
    std::cout<<"Facade ResizeBuffers=0x"<<std::hex<<
        static_cast<std::uint32_t>(resized)<<std::dec<<'\n';
    return SUCCEEDED(resized)?0:26;
}
int probeSwap(IDXGIFactory6* factory,IDXGIAdapter1* adapter,
    ID3D12Device* device,bool facadeMode) {
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
        if(facadeMode) {
            if(!result)result=probeFacade(adapter,proxyDevice.Get(),
                queue.Get(),swap.Get());
        } else {
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
    if((argc!=2&&argc!=3)||(argc==3&&
       std::wcscmp(argv[2],L"--swap")&&
       std::wcscmp(argv[2],L"--facade"))) {
        std::wcerr<<L"Usage: RazKolbasFgStreamlineProbe <absolute SDK bin/x64> [--swap|--facade]\n";
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
                else if(argc==3)exitCode=probeSwap(factory.Get(),adapter.Get(),
                    device.Get(),!std::wcscmp(argv[2],L"--facade"));
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
