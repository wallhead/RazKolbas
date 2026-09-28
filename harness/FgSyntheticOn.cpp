#include <sl.h>
#include <sl_dlss_g.h>
#include <sl_reflex.h>
#include <sl_pcl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>

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

sl::float4x4 identity() {
    sl::float4x4 matrix{};
    for(unsigned row=0;row<4;++row)
        for(unsigned column=0;column<4;++column)
            (&matrix[row].x)[column]=row==column?1.0f:0.0f;
    return matrix;
}

sl::Constants constants(unsigned frame) {
    sl::Constants values{};
    values.cameraViewToClip=identity();
    values.clipToCameraView=identity();
    values.clipToPrevClip=identity();
    values.prevClipToClip=identity();
    values.jitterOffset={0.0f,0.0f};
    values.mvecScale={1.0f,1.0f};
    values.cameraPinholeOffset={0.0f,0.0f};
    values.cameraPos={0.0f,0.0f,0.0f};
    values.cameraUp={0.0f,1.0f,0.0f};
    values.cameraRight={1.0f,0.0f,0.0f};
    values.cameraFwd={0.0f,0.0f,1.0f};
    values.cameraNear=0.1f;
    values.cameraFar=100.0f;
    values.cameraFOV=1.04719755f;
    values.cameraAspectRatio=static_cast<float>(kWidth)/kHeight;
    values.depthInverted=sl::Boolean::eFalse;
    values.cameraMotionIncluded=sl::Boolean::eTrue;
    values.motionVectors3D=sl::Boolean::eFalse;
    values.reset=frame==0?sl::Boolean::eTrue:sl::Boolean::eFalse;
    return values;
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
    IDXGISwapChain1* swap) {
    ComPtr<IDXGISwapChain3> swap3;
    if(FAILED(swap->QueryInterface(IID_PPV_ARGS(&swap3))))return 40;
    ComPtr<ID3D12DescriptorHeap> heap;
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
    heapDesc.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heapDesc.NumDescriptors=6;
    if(FAILED(device->CreateDescriptorHeap(&heapDesc,IID_PPV_ARGS(&heap))))
        return 41;
    std::array<Plane,4> planes{};
    const std::array<DXGI_FORMAT,4> formats{
        DXGI_FORMAT_R32_FLOAT,DXGI_FORMAT_R16G16_FLOAT,
        DXGI_FORMAT_R8G8B8A8_UNORM,DXGI_FORMAT_R8G8B8A8_UNORM};
    for(UINT i=0;i<planes.size();++i)
        if(!makePlane(device,heap.Get(),i,formats[i],planes[i]))return 42;
    std::array<ComPtr<ID3D12Resource>,2> back{};
    std::array<D3D12_CPU_DESCRIPTOR_HANDLE,2> backRtv{};
    for(UINT i=0;i<back.size();++i) {
        if(FAILED(swap->GetBuffer(i,IID_PPV_ARGS(&back[i]))))return 43;
        backRtv[i]=heap->GetCPUDescriptorHandleForHeapStart();
        backRtv[i].ptr+=static_cast<SIZE_T>(i+4)*
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
        transition(list.Get(),back[physical].Get(),
            D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_RENDER_TARGET);
        const float final[]{0.25f,0.25f,0.0f,1.0f};
        list->ClearRenderTargetView(backRtv[physical],final,0,nullptr);
        transition(list.Get(),back[physical].Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PRESENT);
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
        const auto camera=constants(frame);
        if(!ok("slSetConstants",slSetConstants(camera,*token,viewport))) {
            error=54;break;
        }
        sl::Resource depth{sl::ResourceType::eTex2d,
            planes[0].resource.Get(),static_cast<UINT>(kReadState)};
        sl::Resource motion{sl::ResourceType::eTex2d,
            planes[1].resource.Get(),static_cast<UINT>(kReadState)};
        sl::Resource hudless{sl::ResourceType::eTex2d,
            planes[2].resource.Get(),static_cast<UINT>(kReadState)};
        sl::Resource ui{sl::ResourceType::eTex2d,
            planes[3].resource.Get(),static_cast<UINT>(kReadState)};
        sl::Extent extent{0,0,kWidth,kHeight};
        sl::ResourceTag tags[]{
            {&depth,sl::kBufferTypeDepth,sl::eValidUntilPresent,&extent},
            {&motion,sl::kBufferTypeMotionVectors,sl::eValidUntilPresent,&extent},
            {&hudless,sl::kBufferTypeHUDLessColor,sl::eValidUntilPresent,&extent},
            {&ui,sl::kBufferTypeUIColorAndAlpha,sl::eValidUntilPresent,&extent},
            {nullptr,sl::kBufferTypeBackbuffer,sl::eValidUntilPresent,&extent}};
        if(!ok("slSetTagForFrame",slSetTagForFrame(*token,viewport,tags,
                static_cast<UINT>(std::size(tags)),nullptr))||
           !ok("presentStart",slPCLSetMarker(
                sl::PCLMarker::ePresentStart,*token))) {
            error=55;break;
        }
        const auto present=swap->Present(0,0);
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
        Sleep(16);
    }
    options.mode=sl::DLSSGMode::eOff;
    std::cout<<"slDLSSGSetOptions(Off)="<<
        static_cast<int>(slDLSSGSetOptions(viewport,options))<<'\n';
    const auto drain=swap->Present(0,0);
    std::cout<<"FG-On drain Present=0x"<<std::hex<<
        static_cast<UINT>(drain)<<std::dec<<'\n';
    std::cout<<"FG-On generatedObserved="<<generated<<'\n';
    return error?error:generated?0:58;
}
