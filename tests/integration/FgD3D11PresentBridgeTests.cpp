#include <catch2/catch_test_macros.hpp>
#include "rk/FgD3D11PresentBridge.hpp"
#include <d3d11_4.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <array>
#include <variant>

using Microsoft::WRL::ComPtr;

namespace {
struct Devices {
    HWND window{};
    ComPtr<IDXGIAdapter> adapter;
    ComPtr<ID3D11Device> d11;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<ID3D12Device> d12;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<IDXGISwapChain1> swap;
    Devices() {
        window=CreateWindowExW(0,L"STATIC",L"FG bridge WARP",WS_POPUP,
            0,0,64,48,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        REQUIRE(window!=nullptr);
        ComPtr<IDXGIFactory4> factory;
        REQUIRE(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))));
        REQUIRE(SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter))));
        D3D_FEATURE_LEVEL level{};
        REQUIRE(SUCCEEDED(D3D11CreateDevice(adapter.Get(),
            D3D_DRIVER_TYPE_UNKNOWN,nullptr,0,nullptr,0,D3D11_SDK_VERSION,
            &d11,&level,&context)));
        REQUIRE(SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&d12))));
        D3D12_COMMAND_QUEUE_DESC qdesc{};
        REQUIRE(SUCCEEDED(d12->CreateCommandQueue(&qdesc,IID_PPV_ARGS(&queue))));
        DXGI_SWAP_CHAIN_DESC1 desc{};
        desc.Width=64;desc.Height=48;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount=2;desc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
        REQUIRE(SUCCEEDED(factory->CreateSwapChainForHwnd(queue.Get(),window,
            &desc,nullptr,nullptr,&swap)));
    }
    ~Devices() { swap.Reset();if(window)DestroyWindow(window); }
};
std::array<std::uint8_t,4> readPixel(Devices& gpu,unsigned index) {
    ComPtr<ID3D12Resource> back;
    REQUIRE(SUCCEEDED(gpu.swap->GetBuffer(index,IID_PPV_ARGS(&back))));
    const auto desc=back->GetDesc();
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT64 bytes{};
    gpu.d12->GetCopyableFootprints(&desc,0,1,0,&footprint,nullptr,nullptr,
        &bytes);
    D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_READBACK;
    D3D12_RESOURCE_DESC buffer{};
    buffer.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
    buffer.Width=bytes;buffer.Height=1;buffer.DepthOrArraySize=1;
    buffer.MipLevels=1;buffer.SampleDesc.Count=1;
    buffer.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ComPtr<ID3D12Resource> readback;
    REQUIRE(SUCCEEDED(gpu.d12->CreateCommittedResource(&heap,
        D3D12_HEAP_FLAG_NONE,&buffer,D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,IID_PPV_ARGS(&readback))));
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> commands;
    REQUIRE(SUCCEEDED(gpu.d12->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator))));
    REQUIRE(SUCCEEDED(gpu.d12->CreateCommandList(0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,
        IID_PPV_ARGS(&commands))));
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
    REQUIRE(SUCCEEDED(commands->Close()));
    ID3D12CommandList* lists[]{commands.Get()};
    gpu.queue->ExecuteCommandLists(1,lists);
    ComPtr<ID3D12Fence> done;
    REQUIRE(SUCCEEDED(gpu.d12->CreateFence(0,D3D12_FENCE_FLAG_NONE,
        IID_PPV_ARGS(&done))));
    REQUIRE(SUCCEEDED(gpu.queue->Signal(done.Get(),1)));
    const auto event=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    REQUIRE(event!=nullptr);
    REQUIRE(SUCCEEDED(done->SetEventOnCompletion(1,event)));
    const auto waited=WaitForSingleObject(event,5000);
    CloseHandle(event);
    REQUIRE(waited==WAIT_OBJECT_0);
    void* data{};
    REQUIRE(SUCCEEDED(readback->Map(0,nullptr,&data)));
    const auto* p=static_cast<const std::uint8_t*>(data);
    const std::array<std::uint8_t,4> result{p[0],p[1],p[2],p[3]};
    readback->Unmap(0,nullptr);
    return result;
}
}

TEST_CASE("FG D3D11 colour reaches one D3D12 lower Present",
    "[fg_d3d11_present_bridge]") {
    Devices gpu;
    auto made=rk::FgD3D11PresentBridge::create(gpu.d11.Get(),
        gpu.context.Get(),gpu.d12.Get(),gpu.queue.Get(),gpu.swap.Get());
    REQUIRE(std::holds_alternative<std::unique_ptr<rk::FgD3D11PresentBridge>>(made));
    auto bridge=std::move(std::get<std::unique_ptr<rk::FgD3D11PresentBridge>>(made));
    const auto index=bridge->currentIndex();
    REQUIRE(index<2);
    auto* source=bridge->renderBuffer(index);
    REQUIRE(source!=nullptr);
    ComPtr<ID3D11Device> owner;
    source->GetDevice(&owner);
    REQUIRE(owner.Get()==gpu.d11.Get());
    ComPtr<ID3D11RenderTargetView> view;
    REQUIRE(SUCCEEDED(gpu.d11->CreateRenderTargetView(source,nullptr,&view)));
    const float colour[]{0.25f,0.5f,0.75f,1.0f};
    gpu.context->ClearRenderTargetView(view.Get(),colour);
    REQUIRE(SUCCEEDED(bridge->copyToCurrent()));
    const auto pixel=readPixel(gpu,index);
    REQUIRE(pixel[0]>=63);REQUIRE(pixel[0]<=65);
    REQUIRE(pixel[1]>=127);REQUIRE(pixel[1]<=129);
    REQUIRE(pixel[2]>=190);REQUIRE(pixel[2]<=192);
    REQUIRE(pixel[3]==255);
    REQUIRE(SUCCEEDED(bridge->presentPrepared({rk::FgPresentMethod::Present,
        0,DXGI_PRESENT_TEST})));
    REQUIRE(bridge->prepared());
    REQUIRE(SUCCEEDED(bridge->presentPrepared({rk::FgPresentMethod::Present,0,0})));
    REQUIRE_FALSE(bridge->prepared());
    REQUIRE(bridge->presentPrepared({rk::FgPresentMethod::Present,0,0})==
        DXGI_ERROR_INVALID_CALL);
    const auto next=bridge->currentIndex();
    REQUIRE(next<2);
    REQUIRE(next!=index);
    auto* second=bridge->renderBuffer(next);
    REQUIRE(second!=nullptr);
    view.Reset();
    REQUIRE(SUCCEEDED(gpu.d11->CreateRenderTargetView(second,nullptr,&view)));
    const float secondColour[]{1.0f,0.0f,0.0f,1.0f};
    gpu.context->ClearRenderTargetView(view.Get(),secondColour);
    REQUIRE(SUCCEEDED(bridge->copyToCurrent()));
    const auto secondPixel=readPixel(gpu,next);
    REQUIRE((secondPixel==std::array<std::uint8_t,4>{255,0,0,255}));
    REQUIRE(SUCCEEDED(bridge->presentPrepared({rk::FgPresentMethod::Present,0,0})));
}
