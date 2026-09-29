#include <sl.h>
#include <sl_dlss_g.h>
#include <sl_reflex.h>
#include <sl_pcl.h>
#include "rk/FgD3D11SwapFacade.hpp"
#include "rk/FgStreamlineFrameInputs.hpp"
#include "rk/FgStreamlineSubmit.hpp"
#include "rk/FgStreamlineInputLease.hpp"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <variant>

using Microsoft::WRL::ComPtr;

namespace {
constexpr UINT kWidth=1280;
constexpr UINT kHeight=720;
constexpr D3D12_RESOURCE_STATES kReadState=static_cast<D3D12_RESOURCE_STATES>(
    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE|
    D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

struct Plane {
    ComPtr<ID3D12Resource> resource;
    DXGI_FORMAT format{};
    D3D12_CPU_DESCRIPTOR_HANDLE rtv{};
};

bool makePlane(ID3D12Device* device,ID3D12DescriptorHeap* heap,
    UINT slot,DXGI_FORMAT format,Plane& plane) {
    plane.format=format;
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width=kWidth;
    desc.Height=kHeight;
    desc.DepthOrArraySize=1;
    desc.MipLevels=1;
    desc.Format=format;
    desc.SampleDesc.Count=1;
    desc.Layout=D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type=D3D12_HEAP_TYPE_DEFAULT;
    D3D12_CLEAR_VALUE clear{};
    clear.Format=format;
    if(FAILED(device->CreateCommittedResource(&heapProperties,D3D12_HEAP_FLAG_NONE,
        &desc,D3D12_RESOURCE_STATE_RENDER_TARGET,&clear,
        IID_PPV_ARGS(&plane.resource))))return false;
    plane.rtv=heap->GetCPUDescriptorHandleForHeapStart();
    plane.rtv.ptr+=static_cast<SIZE_T>(slot)*
        device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    device->CreateRenderTargetView(plane.resource.Get(),nullptr,plane.rtv);
    return true;
}

void transition(ID3D12GraphicsCommandList* list,ID3D12Resource* resource,
    D3D12_RESOURCE_STATES before,D3D12_RESOURCE_STATES after) {
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition={resource,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
        before,after};
    list->ResourceBarrier(1,&barrier);
}

bool waitFor(ID3D12CommandQueue* queue,ID3D12Fence* fence,
    std::uint64_t value,HANDLE event) {
    if(FAILED(queue->Signal(fence,value))||
       FAILED(fence->SetEventOnCompletion(value,event)))return false;
    return WaitForSingleObject(event,10000)==WAIT_OBJECT_0&&
        fence->GetCompletedValue()>=value;
}
bool waitCompletion(ID3D12Fence* fence,std::uint64_t value,HANDLE event) {
    if(!fence||!value)return true;
    if(fence->GetCompletedValue()>=value)return true;
    if(FAILED(fence->SetEventOnCompletion(value,event)))return false;
    return WaitForSingleObject(event,10000)==WAIT_OBJECT_0&&
        fence->GetCompletedValue()>=value;
}

rk::FgSourceFrame sourceFrame(unsigned frame) {
    rk::FgSourceFrame source{};
    source.source=frame+1;source.generation=1;
    source.presentToken=frame+1;source.resetEpoch=1;
    source.cameraValid=true;source.worldActive=true;source.ownerReady=true;
    source.render={kWidth,kHeight};source.display=source.render;
    return source;
}
rk::FgCameraData cameraData(unsigned frame) {
    const auto source=sourceFrame(frame);
    rk::FgCameraData camera{};
    camera.source=source.source;camera.generation=source.generation;
    camera.presentToken=source.presentToken;
    camera.resetEpoch=source.resetEpoch;camera.sampleRevision=frame+1;
    for(unsigned i=0;i<4;++i) {
        camera.viewToClip[i*4+i]=1.0f;
        camera.clipToView[i*4+i]=1.0f;
        camera.clipToPrevClip[i*4+i]=1.0f;
        camera.prevClipToClip[i*4+i]=1.0f;
    }
    camera.up={0.0f,1.0f,0.0f};
    camera.right={1.0f,0.0f,0.0f};
    camera.forward={0.0f,0.0f,1.0f};
    camera.mvecScale={1.0f,1.0f};
    camera.nearPlane=0.1f;camera.farPlane=100.0f;
    camera.fovRadians=1.04719755f;
    camera.aspectRatio=static_cast<float>(kWidth)/kHeight;
    camera.cameraMotionIncluded=true;camera.reset=frame==0;
    return camera;
}

bool ok(const char* label,sl::Result result) {
    std::cout<<label<<'='<<static_cast<int>(result)<<'\n';
    return result==sl::Result::eOk;
}

struct EventOwner {
    HANDLE handle{CreateEventW(nullptr,FALSE,FALSE,nullptr)};
    ~EventOwner(){if(handle)CloseHandle(handle);}
};
}

int probeSyntheticOn(ID3D12Device* device,ID3D12CommandQueue* queue,
    IDXGISwapChain1* swap,IDXGIAdapter1* adapter,bool facadeMode) {
    ComPtr<IDXGISwapChain3> swap3;
    if(FAILED(swap->QueryInterface(IID_PPV_ARGS(&swap3))))return 40;
    ComPtr<ID3D11Device> d11;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGISwapChain4> facade;
    ComPtr<ID3D11Texture2D> renderBuffer;
    ComPtr<ID3D11RenderTargetView> renderView;
    if(facadeMode) {
        D3D_FEATURE_LEVEL level{};
        if(FAILED(D3D11CreateDevice(adapter,D3D_DRIVER_TYPE_UNKNOWN,nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,
            &d11,&level,&context)))return 60;
        void* native{};
        if(slGetNativeInterface(device,&native)!=sl::Result::eOk||!native)
            return 61;
        ComPtr<IUnknown> nativeOwner;
        nativeOwner.Attach(static_cast<IUnknown*>(native));
        ComPtr<ID3D12Device> verifiedNative;
        if(FAILED(nativeOwner.As(&verifiedNative)))return 61;
        auto made=rk::FgD3D11SwapFacade::create(d11.Get(),context.Get(),
            device,queue,swap,verifiedNative.Get());
        if(!std::holds_alternative<ComPtr<IDXGISwapChain4>>(made)) {
            const auto& failure=std::get<rk::Error>(made);
            std::cout<<"FG-D3D11 facade error: "<<failure.message<<'\n';
            return 62;
        }
        facade=std::move(std::get<ComPtr<IDXGISwapChain4>>(made));
        if(FAILED(facade->GetBuffer(0,IID_PPV_ARGS(&renderBuffer)))||
           FAILED(d11->CreateRenderTargetView(renderBuffer.Get(),nullptr,
               &renderView)))return 63;
        std::cout<<"FG-D3D11 facade=1\n";
    }
    ComPtr<ID3D12DescriptorHeap> heap;
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
    heapDesc.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heapDesc.NumDescriptors=7;
    if(FAILED(device->CreateDescriptorHeap(&heapDesc,IID_PPV_ARGS(&heap))))
        return 41;
    std::array<Plane,5> planes{};
    const std::array<DXGI_FORMAT,5> formats{
        DXGI_FORMAT_R32_FLOAT,DXGI_FORMAT_R16G16_FLOAT,
        DXGI_FORMAT_R8G8B8A8_UNORM,DXGI_FORMAT_R8G8B8A8_UNORM,
        DXGI_FORMAT_R8G8B8A8_UNORM};
    for(UINT i=0;i<planes.size();++i)
        if(!makePlane(device,heap.Get(),i,formats[i],planes[i]))return 42;
    std::array<ComPtr<ID3D12Resource>,2> back{};
    std::array<D3D12_CPU_DESCRIPTOR_HANDLE,2> backRtv{};
    for(UINT i=0;i<back.size();++i) {
        if(FAILED(swap->GetBuffer(i,IID_PPV_ARGS(&back[i]))))return 43;
        backRtv[i]=heap->GetCPUDescriptorHandleForHeapStart();
        backRtv[i].ptr+=static_cast<SIZE_T>(i+5)*
            device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        device->CreateRenderTargetView(back[i].Get(),nullptr,backRtv[i]);
    }
    ComPtr<ID3D12Fence> fence;
    if(FAILED(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,
        IID_PPV_ARGS(&fence))))return 44;
    EventOwner event;
    if(!event.handle)return 45;
    sl::ReflexOptions reflex{};
    reflex.mode=sl::ReflexMode::eLowLatency;
    if(!ok("slReflexSetOptions",slReflexSetOptions(reflex)))return 46;
    const sl::ViewportHandle viewport{0u};
    sl::DLSSGOptions options{};
    options.mode=sl::DLSSGMode::eOn;
    options.numFramesToGenerate=1;
    options.numBackBuffers=2;
    options.mvecDepthWidth=kWidth;
    options.mvecDepthHeight=kHeight;
    options.colorWidth=kWidth;
    options.colorHeight=kHeight;
    options.colorBufferFormat=DXGI_FORMAT_R8G8B8A8_UNORM;
    options.mvecBufferFormat=formats[1];
    options.depthBufferFormat=formats[0];
    options.hudLessBufferFormat=formats[2];
    options.uiBufferFormat=formats[3];
    options.enableUserInterfaceRecomposition=sl::Boolean::eTrue;
    if(!ok("slDLSSGSetOptions(On)",slDLSSGSetOptions(viewport,options)))
        return 47;
    bool generated=false;
    int error=0;
    for(unsigned frame=0;frame<8;++frame) {
        sl::FrameToken* token{};
        if(!ok("slGetNewFrameToken",slGetNewFrameToken(token))||!token||
           !ok("simulationStart",slPCLSetMarker(
                sl::PCLMarker::eSimulationStart,*token))||
           !ok("slReflexSleep",slReflexSleep(*token))||
           !ok("simulationEnd",slPCLSetMarker(
                sl::PCLMarker::eSimulationEnd,*token))||
           !ok("renderSubmitStart",slPCLSetMarker(
                sl::PCLMarker::eRenderSubmitStart,*token))) {
            error=48;break;
        }
        const auto physical=swap3->GetCurrentBackBufferIndex();
        if(physical>=back.size()){error=49;break;}
        ComPtr<ID3D12CommandAllocator> allocator;
        ComPtr<ID3D12GraphicsCommandList> list;
        if(FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                IID_PPV_ARGS(&allocator)))||
           FAILED(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,
                allocator.Get(),nullptr,IID_PPV_ARGS(&list)))) {
            error=50;break;
        }
        for(UINT i=0;i<planes.size();++i) {
            if(frame)transition(list.Get(),planes[i].resource.Get(),
                kReadState,D3D12_RESOURCE_STATE_RENDER_TARGET);
            const float clear[]{
                i==0?0.5f:i==2?0.25f:0.0f,
                i==2?0.25f:0.0f,0.0f,0.0f};
            list->ClearRenderTargetView(planes[i].rtv,clear,0,nullptr);
            transition(list.Get(),planes[i].resource.Get(),
                D3D12_RESOURCE_STATE_RENDER_TARGET,kReadState);
        }
        const float final[]{0.25f,0.25f,0.0f,1.0f};
        if(facadeMode) {
            context->ClearRenderTargetView(renderView.Get(),final);
        } else {
            transition(list.Get(),back[physical].Get(),
                D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_RENDER_TARGET);
            list->ClearRenderTargetView(backRtv[physical],final,0,nullptr);
            transition(list.Get(),back[physical].Get(),
                D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PRESENT);
        }
        if(FAILED(list->Close())){error=51;break;}
        ID3D12CommandList* lists[]{list.Get()};
        queue->ExecuteCommandLists(1,lists);
        if(!waitFor(queue,fence.Get(),frame+1,event.handle)){
            error=52;break;
        }
        if(!ok("renderSubmitEnd",slPCLSetMarker(
                sl::PCLMarker::eRenderSubmitEnd,*token))) {
            error=53;break;
        }
        const auto source=sourceFrame(frame);
        rk::FgPreparedSubmission prepared{};
        prepared.source=source.source;prepared.generation=source.generation;
        prepared.presentToken=source.presentToken;
        prepared.resetEpoch=source.resetEpoch;
        prepared.render=source.render;prepared.display=source.display;
        prepared.physicalOutputIndex=physical;
        prepared.swapBufferCount=static_cast<std::uint32_t>(back.size());
        // Synthetic single-queue producer: both recorded completion values
        // name the real fence signal just waited above. No D3D11 copy occurs.
        prepared.copyTicket={frame+1,frame+1};
        prepared.camera=cameraData(frame);
        prepared.resources={planes[4].resource,planes[0].resource,
            planes[1].resource,planes[2].resource,planes[3].resource};
        rk::FgStreamlineReadStates readable{};
        readable.completedCopy=prepared.copyTicket;
        readable.actual.fill(kReadState);
        auto inputs=rk::prepareFgStreamlineFrameInputs(source,prepared,
            readable,DXGI_FORMAT_R8G8B8A8_UNORM);
        if(const auto failure=std::get_if<rk::Error>(&inputs)) {
            std::cout<<"FG-On prepared inputs failed: "<<failure->message<<'\n';
            error=54;break;
        }
        rk::FgStreamlineInputLease retained(
            std::move(std::get<rk::FgStreamlineFrameInputs>(inputs)));
        rk::FgStreamlineCalls sdk{};
        sdk.setConstants=[](const sl::Constants& values,
            const sl::FrameToken& frameToken,const sl::ViewportHandle& view) {
            return slSetConstants(values,frameToken,view);
        };
        sdk.setTags=[](const sl::FrameToken& frameToken,
            const sl::ViewportHandle& view,const sl::ResourceTag* tags,
            std::uint32_t count,sl::CommandBuffer* commands) {
            return slSetTagForFrame(frameToken,view,tags,count,commands);
        };
        const rk::FgStreamlineTokenBinding binding{
            source.source,source.generation,source.presentToken,
            source.resetEpoch,token};
        const auto submitted=rk::submitFgStreamlineInputs(
            retained.inputs(),binding,viewport,sdk);
        if(const auto failure=std::get_if<rk::Error>(&submitted)) {
            std::cout<<"FG-On input submission failed: "<<failure->message<<'\n';
            error=54;break;
        }
        if(!ok("presentStart",slPCLSetMarker(
                sl::PCLMarker::ePresentStart,*token))) {
            error=55;break;
        }
        const auto present=facadeMode?facade->Present(0,0):swap->Present(0,0);
        std::cout<<"FG-On Present frame="<<frame<<" hr=0x"<<std::hex<<
            static_cast<UINT>(present)<<std::dec<<'\n';
        if(FAILED(present)||!ok("presentEnd",slPCLSetMarker(
                sl::PCLMarker::ePresentEnd,*token))) {
            error=56;break;
        }
        sl::DLSSGState state{};
        const auto stateResult=slDLSSGGetState(viewport,state,nullptr);
        std::cout<<"FG-On state result="<<static_cast<int>(stateResult)<<
            " status=0x"<<std::hex<<static_cast<UINT>(state.status)<<std::dec<<
            " actualPresented="<<state.numFramesActuallyPresented<<
            " maxExtra="<<state.numFramesToGenerateMax<<
            " minDimension="<<state.minWidthOrHeight<<'\n';
        if(stateResult!=sl::Result::eOk){error=57;break;}
        generated|=state.numFramesActuallyPresented>1;
        std::cout<<"FG-On input retirement frame="<<frame<<
            " fencePresent="<<(state.inputsProcessingCompletionFence!=nullptr)<<
            " value="<<state.lastPresentInputsProcessingCompletionFenceValue<<'\n';
        const auto observed=retained.observeCompletion(state);
        if(!std::holds_alternative<bool>(observed)||
           !waitCompletion(retained.completionFence(),
                retained.completionValue(),event.handle)||
           !retained.releaseIfRetired()) {
            std::cout<<"FG-On provider input fence did not retire\n";
            error=59;break;
        }
        Sleep(16);
    }
    options.mode=sl::DLSSGMode::eOff;
    std::cout<<"slDLSSGSetOptions(Off)="<<
        static_cast<int>(slDLSSGSetOptions(viewport,options))<<'\n';
    const auto drain=facadeMode?facade->Present(0,0):swap->Present(0,0);
    std::cout<<"FG-On drain Present=0x"<<std::hex<<
        static_cast<UINT>(drain)<<std::dec<<'\n';
    std::cout<<"FG-On generatedObserved="<<generated<<'\n';
    return error?error:generated?0:58;
}
