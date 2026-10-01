#include <catch2/catch_test_macros.hpp>
#include "rk/FgSharedInputs.hpp"
#include "rk/PointerPatch.hpp"
#include <d3d11_4.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <array>
#include <chrono>
#include <thread>
#include <variant>

using Microsoft::WRL::ComPtr;

namespace {
using GetDeviceFn=void(STDMETHODCALLTYPE*)(ID3D11DeviceChild*,ID3D11Device**);
GetDeviceFn originalGetDevice{};
ID3D11DeviceChild* aliasedResource{};
ID3D11Device* reportedAlias{};
void STDMETHODCALLTYPE aliasGetDevice(ID3D11DeviceChild* resource,ID3D11Device** device) {
    if(resource==aliasedResource) {
        *device=reportedAlias;
        reportedAlias->AddRef();
    } else originalGetDevice(resource,device);
}
}

TEST_CASE("FG shared colour reaches a same-adapter D3D12 lease", "[fg_interop]") {
    ComPtr<IDXGIFactory4> factory;
    REQUIRE(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))));
    ComPtr<IDXGIAdapter> adapter;
    REQUIRE(SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter))));
    ComPtr<ID3D11Device> d11;
    ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL level{};
    REQUIRE(SUCCEEDED(D3D11CreateDevice(adapter.Get(),D3D_DRIVER_TYPE_UNKNOWN,
        nullptr,0,nullptr,0,D3D11_SDK_VERSION,&d11,&level,&context)));
    ComPtr<ID3D12Device> d12;
    REQUIRE(SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&d12))));
    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    ComPtr<ID3D12CommandQueue> queue;
    REQUIRE(SUCCEEDED(d12->CreateCommandQueue(&queueDesc,IID_PPV_ARGS(&queue))));
    auto made=rk::FgSharedInputs::create(d11.Get(),d12.Get(),queue.Get());
    REQUIRE(std::holds_alternative<std::unique_ptr<rk::FgSharedInputs>>(made));
    auto bridge=std::move(std::get<std::unique_ptr<rk::FgSharedInputs>>(made));

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width=37;desc.Height=19;desc.MipLevels=1;desc.ArraySize=1;
    desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;
    desc.Usage=D3D11_USAGE_DEFAULT;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    std::array<std::uint32_t,37*19> pixels{};
    pixels.fill(0xff1973c5);
    D3D11_SUBRESOURCE_DATA data{pixels.data(),37*4,0};
    ComPtr<ID3D11Texture2D> source;
    REQUIRE(SUCCEEDED(d11->CreateTexture2D(&desc,&data,&source)));
    auto surfaceResult=bridge->makeSurface(desc);
    REQUIRE(std::holds_alternative<rk::FgSharedSurface>(surfaceResult));
    auto surface=std::move(std::get<rk::FgSharedSurface>(surfaceResult));
    REQUIRE(surface.d12()!=nullptr);
    REQUIRE(surface.d12()->GetDesc().Width==37);
    auto secondBridgeResult=rk::FgSharedInputs::create(d11.Get(),d12.Get(),queue.Get());
    REQUIRE(std::holds_alternative<std::unique_ptr<rk::FgSharedInputs>>(secondBridgeResult));
    auto secondBridge=std::move(std::get<std::unique_ptr<rk::FgSharedInputs>>(
        secondBridgeResult));
    REQUIRE(std::holds_alternative<rk::Error>(
        secondBridge->copy(context.Get(),source.Get(),surface)));
    auto wrongDesc=desc;
    wrongDesc.Width=36;
    ComPtr<ID3D11Texture2D> wrongSource;
    REQUIRE(SUCCEEDED(d11->CreateTexture2D(&wrongDesc,nullptr,&wrongSource)));
    REQUIRE(std::holds_alternative<rk::Error>(
        bridge->copy(context.Get(),wrongSource.Get(),surface)));
    auto sourceResult=bridge->captureSource(source.Get());
    REQUIRE(std::holds_alternative<rk::FgSourceLease>(sourceResult));
    auto sourceLease=std::move(std::get<rk::FgSourceLease>(sourceResult));
    auto secondSurfaceResult=secondBridge->makeSurface(desc);
    REQUIRE(std::holds_alternative<rk::FgSharedSurface>(secondSurfaceResult));
    auto secondSurface=std::move(std::get<rk::FgSharedSurface>(secondSurfaceResult));
    REQUIRE(std::holds_alternative<rk::Error>(
        secondBridge->copy(context.Get(),sourceLease,secondSurface)));
    REQUIRE(std::holds_alternative<rk::Error>(
        bridge->copy(context.Get(),rk::FgSourceLease{},surface)));
    ComPtr<ID3D11Device> foreignDevice;
    REQUIRE(SUCCEEDED(D3D11CreateDevice(adapter.Get(),D3D_DRIVER_TYPE_UNKNOWN,
        nullptr,0,nullptr,0,D3D11_SDK_VERSION,&foreignDevice,nullptr,nullptr)));
    ComPtr<ID3D11DeviceContext> foreignContext;
    foreignDevice->GetImmediateContext(&foreignContext);
    REQUIRE(std::holds_alternative<rk::Error>(
        bridge->copy(foreignContext.Get(),sourceLease,surface)));
    auto** sourceTable=*reinterpret_cast<void***>(source.Get());
    originalGetDevice=reinterpret_cast<GetDeviceFn>(sourceTable[3]);
    aliasedResource=source.Get();reportedAlias=foreignDevice.Get();
    rk::PointerPatch aliasPatch;
    REQUIRE(std::holds_alternative<bool>(aliasPatch.apply(sourceTable+3,
        reinterpret_cast<void*>(originalGetDevice),reinterpret_cast<void*>(&aliasGetDevice))));
    // Simulate the later wrapper's changed GetDevice report. Raw resources
    // still reject; the lease retains the exact resource validated earlier.
    REQUIRE(std::holds_alternative<rk::Error>(
        bridge->copy(context.Get(),source.Get(),surface)));
    REQUIRE(std::holds_alternative<rk::Error>(bridge->captureSource(source.Get())));
    const auto copied=bridge->copy(context.Get(),sourceLease,surface);
    REQUIRE(std::holds_alternative<rk::FgCopyTicket>(copied));
    const auto ticket=std::get<rk::FgCopyTicket>(copied);
    REQUIRE(ticket.producer!=0);
    REQUIRE(ticket.copy!=0);
    REQUIRE(bridge->waitCopy(ticket.copy));
    REQUIRE(std::holds_alternative<bool>(aliasPatch.restore()));
    aliasedResource=nullptr;reportedAlias=nullptr;
    REQUIRE_FALSE(bridge->copyComplete(ticket.copy+2));

    const auto textureDesc=surface.d12()->GetDesc();
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT64 bytes{};
    d12->GetCopyableFootprints(&textureDesc,0,1,0,&footprint,nullptr,nullptr,&bytes);
    REQUIRE(bytes>=37*19*4);
    D3D12_HEAP_PROPERTIES readbackHeap{};
    readbackHeap.Type=D3D12_HEAP_TYPE_READBACK;
    D3D12_RESOURCE_DESC bufferDesc{};
    bufferDesc.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width=bytes;
    bufferDesc.Height=1;
    bufferDesc.DepthOrArraySize=1;
    bufferDesc.MipLevels=1;
    bufferDesc.SampleDesc.Count=1;
    bufferDesc.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ComPtr<ID3D12Resource> readback;
    REQUIRE(SUCCEEDED(d12->CreateCommittedResource(&readbackHeap,D3D12_HEAP_FLAG_NONE,
        &bufferDesc,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&readback))));
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> commands;
    REQUIRE(SUCCEEDED(d12->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&allocator))));
    REQUIRE(SUCCEEDED(d12->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,
        allocator.Get(),nullptr,IID_PPV_ARGS(&commands))));
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition={surface.d12(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
        D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_COPY_SOURCE};
    commands->ResourceBarrier(1,&barrier);
    D3D12_TEXTURE_COPY_LOCATION from{};
    from.pResource=surface.d12();
    from.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    D3D12_TEXTURE_COPY_LOCATION to{};
    to.pResource=readback.Get();
    to.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    to.PlacedFootprint=footprint;
    commands->CopyTextureRegion(&to,0,0,0,&from,nullptr);
    barrier.Transition.StateBefore=D3D12_RESOURCE_STATE_COPY_SOURCE;
    barrier.Transition.StateAfter=D3D12_RESOURCE_STATE_COMMON;
    commands->ResourceBarrier(1,&barrier);
    REQUIRE(SUCCEEDED(commands->Close()));
    ID3D12CommandList* lists[]{commands.Get()};
    queue->ExecuteCommandLists(1,lists);
    ComPtr<ID3D12Fence> readbackFence;
    REQUIRE(SUCCEEDED(d12->CreateFence(0,D3D12_FENCE_FLAG_NONE,
        IID_PPV_ARGS(&readbackFence))));
    REQUIRE(SUCCEEDED(queue->Signal(readbackFence.Get(),1)));
    const auto event=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    REQUIRE(event!=nullptr);
    REQUIRE(SUCCEEDED(readbackFence->SetEventOnCompletion(1,event)));
    const auto waited=WaitForSingleObject(event,5000);
    CloseHandle(event);
    REQUIRE(waited==WAIT_OBJECT_0);
    void* mapped{};
    REQUIRE(SUCCEEDED(readback->Map(0,nullptr,&mapped)));
    REQUIRE(*static_cast<const std::uint32_t*>(mapped)==pixels.front());
    readback->Unmap(0,nullptr);
}

TEST_CASE("FG copy progress rejects a removed-device fence sentinel",
    "[fg_interop]") {
    using rk::FgCopyStatus;
    using rk::classifyFgCopyStatus;
    REQUIRE(classifyFgCopyStatus(4,5,S_OK)==FgCopyStatus::Pending);
    REQUIRE(classifyFgCopyStatus(5,5,S_OK)==FgCopyStatus::Complete);
    REQUIRE(classifyFgCopyStatus(UINT64_MAX,5,S_OK)==
        FgCopyStatus::DeviceRemoved);
    REQUIRE(classifyFgCopyStatus(5,5,DXGI_ERROR_DEVICE_REMOVED)==
        FgCopyStatus::DeviceRemoved);
    REQUIRE(classifyFgCopyStatus(5,UINT64_MAX,S_OK)==FgCopyStatus::Unknown);
}

TEST_CASE("FG batched inputs validate before copying and publish all five pixels",
    "[fg_interop]") {
    ComPtr<IDXGIFactory4> factory;
    REQUIRE(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))));
    ComPtr<IDXGIAdapter> adapter;
    REQUIRE(SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter))));
    ComPtr<ID3D11Device> d11;
    ComPtr<ID3D11DeviceContext> context;
    REQUIRE(SUCCEEDED(D3D11CreateDevice(adapter.Get(),D3D_DRIVER_TYPE_UNKNOWN,
        nullptr,0,nullptr,0,D3D11_SDK_VERSION,&d11,nullptr,&context)));
    ComPtr<ID3D12Device> d12;
    REQUIRE(SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&d12))));
    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    ComPtr<ID3D12CommandQueue> queue;
    REQUIRE(SUCCEEDED(d12->CreateCommandQueue(&queueDesc,IID_PPV_ARGS(&queue))));
    auto made=rk::FgSharedInputs::create(d11.Get(),d12.Get(),queue.Get());
    REQUIRE(std::holds_alternative<std::unique_ptr<rk::FgSharedInputs>>(made));
    auto bridge=std::move(std::get<std::unique_ptr<rk::FgSharedInputs>>(made));

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width=8;desc.Height=8;desc.MipLevels=1;desc.ArraySize=1;
    desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;
    desc.Usage=D3D11_USAGE_DEFAULT;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    std::array<ComPtr<ID3D11Texture2D>,5> sources{};
    std::array<rk::FgSharedSurface,5> targets{};
    std::array<rk::FgCopyPair,5> pairs{};
    std::array<std::uint32_t,8*8> pixels{},sentinel{};
    sentinel.fill(0xff112233);
    for(std::size_t i=0;i<pairs.size();++i) {
        pixels.fill(0xff000000u|static_cast<std::uint32_t>(i+1));
        const D3D11_SUBRESOURCE_DATA data{pixels.data(),8*4,0};
        REQUIRE(SUCCEEDED(d11->CreateTexture2D(&desc,&data,&sources[i])));
        auto target=bridge->makeSurface(desc);
        REQUIRE(std::holds_alternative<rk::FgSharedSurface>(target));
        targets[i]=std::move(std::get<rk::FgSharedSurface>(target));
        context->UpdateSubresource(targets[i].d11(),0,nullptr,
            sentinel.data(),8*4,0);
        pairs[i]={sources[i].Get(),&targets[i]};
    }
    auto wrongDesc=desc;
    wrongDesc.Width=7;
    ComPtr<ID3D11Texture2D> wrong;
    REQUIRE(SUCCEEDED(d11->CreateTexture2D(&wrongDesc,nullptr,&wrong)));
    pairs[4].source=wrong.Get();
    REQUIRE(std::holds_alternative<rk::Error>(
        bridge->copyBatch(context.Get(),pairs)));

    auto stagingDesc=desc;
    stagingDesc.Usage=D3D11_USAGE_STAGING;
    stagingDesc.BindFlags=0;
    stagingDesc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging;
    REQUIRE(SUCCEEDED(d11->CreateTexture2D(&stagingDesc,nullptr,&staging)));
    context->CopyResource(staging.Get(),targets[0].d11());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    REQUIRE(SUCCEEDED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)));
    REQUIRE(*static_cast<const std::uint32_t*>(mapped.pData)==sentinel.front());
    context->Unmap(staging.Get(),0);

    pairs[4].source=sources[4].Get();
    const auto copied=bridge->copyBatch(context.Get(),pairs);
    REQUIRE(std::holds_alternative<rk::FgCopyTicket>(copied));
    const auto ticket=std::get<rk::FgCopyTicket>(copied);
    REQUIRE(ticket.producer==1);
    REQUIRE(ticket.copy==1);
    REQUIRE(bridge->waitCopy(ticket.copy));
    for(std::size_t i=0;i<targets.size();++i) {
        context->CopyResource(staging.Get(),targets[i].d11());
        REQUIRE(SUCCEEDED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,
            &mapped)));
        REQUIRE(*static_cast<const std::uint32_t*>(mapped.pData)==
            (0xff000000u|static_cast<std::uint32_t>(i+1)));
        context->Unmap(staging.Get(),0);
    }
}

TEST_CASE("FG producer cannot certify a delayed consumer queue",
    "[fg_interop]") {
    ComPtr<IDXGIFactory4> factory;
    REQUIRE(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))));
    ComPtr<IDXGIAdapter> adapter;
    REQUIRE(SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter))));
    ComPtr<ID3D11Device> d11;
    ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL level{};
    REQUIRE(SUCCEEDED(D3D11CreateDevice(adapter.Get(),D3D_DRIVER_TYPE_UNKNOWN,
        nullptr,0,nullptr,0,D3D11_SDK_VERSION,&d11,&level,&context)));
    ComPtr<ID3D12Device> d12;
    REQUIRE(SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&d12))));
    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    ComPtr<ID3D12CommandQueue> queue;
    REQUIRE(SUCCEEDED(d12->CreateCommandQueue(&queueDesc,IID_PPV_ARGS(&queue))));
    auto made=rk::FgSharedInputs::create(d11.Get(),d12.Get(),queue.Get());
    REQUIRE(std::holds_alternative<std::unique_ptr<rk::FgSharedInputs>>(made));
    auto bridge=std::move(std::get<std::unique_ptr<rk::FgSharedInputs>>(made));
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width=8;desc.Height=8;desc.MipLevels=1;desc.ArraySize=1;
    desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;
    desc.Usage=D3D11_USAGE_DEFAULT;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    ComPtr<ID3D11Texture2D> source;
    REQUIRE(SUCCEEDED(d11->CreateTexture2D(&desc,nullptr,&source)));
    auto surfaceResult=bridge->makeSurface(desc);
    REQUIRE(std::holds_alternative<rk::FgSharedSurface>(surfaceResult));
    auto surface=std::move(std::get<rk::FgSharedSurface>(surfaceResult));
    ComPtr<ID3D12Fence> gate;
    REQUIRE(SUCCEEDED(d12->CreateFence(0,D3D12_FENCE_FLAG_NONE,
        IID_PPV_ARGS(&gate))));
    REQUIRE(SUCCEEDED(queue->Wait(gate.Get(),1)));
    struct GateRelease {
        ID3D12Fence* fence;
        ~GateRelease() { if(fence)fence->Signal(1); }
    } releaseOnFailure{gate.Get()};
    const auto first=bridge->copy(context.Get(),source.Get(),surface);
    const auto second=bridge->copy(context.Get(),source.Get(),surface);
    REQUIRE(std::holds_alternative<rk::FgCopyTicket>(first));
    REQUIRE(std::holds_alternative<rk::FgCopyTicket>(second));
    const auto a=std::get<rk::FgCopyTicket>(first);
    const auto b=std::get<rk::FgCopyTicket>(second);
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(!bridge->producerComplete(b.producer)&&
        std::chrono::steady_clock::now()<deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    const bool producerRan=bridge->producerComplete(b.producer);
    const bool consumerStillPending=!bridge->copyComplete(a.copy);
    REQUIRE(SUCCEEDED(gate->Signal(1)));
    releaseOnFailure.fence=nullptr;
    REQUIRE(producerRan);
    REQUIRE(consumerStillPending);
    REQUIRE(bridge->waitCopy(a.copy));
    REQUIRE(bridge->waitCopy(b.copy));
}
