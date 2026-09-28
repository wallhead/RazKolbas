#include <catch2/catch_test_macros.hpp>
#include "rk/FgD3D11PresentBridge.hpp"
#include "rk/FgD3D11SwapFacade.hpp"
#include "../support/FgObservedSwap.hpp"
#include <d3d11_4.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <array>
#include <exception>
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
    explicit Devices(UINT count=2) {
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
        desc.BufferCount=count;desc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
        REQUIRE(SUCCEEDED(factory->CreateSwapChainForHwnd(queue.Get(),window,
            &desc,nullptr,nullptr,&swap)));
    }
    ~Devices() { swap.Reset();if(window)DestroyWindow(window); }
};
bool tryReadPixel(Devices& gpu,unsigned index,
    std::array<std::uint8_t,4>& result) noexcept {
    ComPtr<ID3D12Resource> back;
    if(FAILED(gpu.swap->GetBuffer(index,IID_PPV_ARGS(&back))))return false;
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
    if(FAILED(gpu.d12->CreateCommittedResource(&heap,
        D3D12_HEAP_FLAG_NONE,&buffer,D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,IID_PPV_ARGS(&readback))))return false;
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> commands;
    if(FAILED(gpu.d12->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator))))return false;
    if(FAILED(gpu.d12->CreateCommandList(0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,
        IID_PPV_ARGS(&commands))))return false;
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
    ComPtr<ID3D12Fence> done;
    if(FAILED(gpu.d12->CreateFence(0,D3D12_FENCE_FLAG_NONE,
        IID_PPV_ARGS(&done))))return false;
    const auto event=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    if(!event)return false;
    ID3D12CommandList* lists[]{commands.Get()};
    gpu.queue->ExecuteCommandLists(1,lists);
    if(FAILED(gpu.queue->Signal(done.Get(),1)))std::terminate();
    const auto armed=done->SetEventOnCompletion(1,event);
    const auto waited=SUCCEEDED(armed)?WaitForSingleObject(event,5000):WAIT_FAILED;
    CloseHandle(event);
    if(waited!=WAIT_OBJECT_0||done->GetCompletedValue()!=1||
       FAILED(gpu.d12->GetDeviceRemovedReason()))std::terminate();
    void* data{};
    if(FAILED(readback->Map(0,nullptr,&data)))return false;
    const auto* p=static_cast<const std::uint8_t*>(data);
    result={p[0],p[1],p[2],p[3]};
    readback->Unmap(0,nullptr);
    return true;
}
std::array<std::uint8_t,4> readPixel(Devices& gpu,unsigned index) {
    std::array<std::uint8_t,4> result{};
    REQUIRE(tryReadPixel(gpu,index,result));
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
    auto* source=bridge->renderBuffer(0);
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
    DXGI_PRESENT_PARAMETERS parameters{};
    REQUIRE(bridge->presentPrepared({rk::FgPresentMethod::Present,0,0,
        &parameters})==E_INVALIDARG);
    REQUIRE(bridge->prepared());
    REQUIRE(bridge->presentPrepared({rk::FgPresentMethod::Present1,0,0,
        nullptr})==E_INVALIDARG);
    REQUIRE(bridge->prepared());
    REQUIRE(SUCCEEDED(bridge->presentPrepared({rk::FgPresentMethod::Present,0,0})));
    REQUIRE_FALSE(bridge->prepared());
    REQUIRE(bridge->presentPrepared({rk::FgPresentMethod::Present,0,0})==
        DXGI_ERROR_INVALID_CALL);
    const auto next=bridge->currentIndex();
    REQUIRE(next<2);
    REQUIRE(next!=index);
    auto* second=bridge->renderBuffer(0);
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

TEST_CASE("FG bridge presents a cached D3D11 buffer zero across physical rotation",
    "[fg_d3d11_present_bridge]") {
    for(const auto buffers:{2u,3u}) {
    Devices gpu(buffers);
    auto made=rk::FgD3D11PresentBridge::create(gpu.d11.Get(),
        gpu.context.Get(),gpu.d12.Get(),gpu.queue.Get(),gpu.swap.Get());
    REQUIRE(std::holds_alternative<std::unique_ptr<rk::FgD3D11PresentBridge>>(made));
    auto bridge=std::move(std::get<std::unique_ptr<rk::FgD3D11PresentBridge>>(made));
    auto* cached=bridge->renderBuffer(0);
    REQUIRE(cached!=nullptr);
    ComPtr<ID3D11RenderTargetView> view;
    REQUIRE(SUCCEEDED(gpu.d11->CreateRenderTargetView(cached,nullptr,&view)));
    for(unsigned frame=0;frame<8;++frame) {
        const float colour[]{frame%2?1.0f:0.0f,
            frame%3?0.0f:1.0f,frame%4?0.0f:1.0f,1.0f};
        gpu.context->ClearRenderTargetView(view.Get(),colour);
        const auto physical=bridge->currentIndex();
        REQUIRE(bridge->renderBuffer(physical)==cached);
        REQUIRE(SUCCEEDED(bridge->copyToCurrent()));
        const auto pixel=readPixel(gpu,physical); // Before Present: flip-discard is undefined afterwards.
        REQUIRE(pixel[0]==(frame%2?255:0));
        REQUIRE(pixel[1]==(frame%3?0:255));
        REQUIRE(pixel[2]==(frame%4?0:255));
        REQUIRE(pixel[3]==255);
        REQUIRE(SUCCEEDED(bridge->presentPrepared({rk::FgPresentMethod::Present,0,0})));
    }
    }
}

TEST_CASE("FG bridge rejects deferred contexts and non-direct queues",
    "[fg_d3d11_present_bridge]") {
    Devices gpu;
    ComPtr<ID3D11DeviceContext> deferred;
    REQUIRE(SUCCEEDED(gpu.d11->CreateDeferredContext(0,&deferred)));
    auto wrongContext=rk::FgD3D11PresentBridge::create(gpu.d11.Get(),
        deferred.Get(),gpu.d12.Get(),gpu.queue.Get(),gpu.swap.Get());
    REQUIRE(std::holds_alternative<rk::Error>(wrongContext));
    D3D12_COMMAND_QUEUE_DESC desc{};
    desc.Type=D3D12_COMMAND_LIST_TYPE_COPY;
    ComPtr<ID3D12CommandQueue> copyQueue;
    REQUIRE(SUCCEEDED(gpu.d12->CreateCommandQueue(&desc,
        IID_PPV_ARGS(&copyQueue))));
    auto wrongQueue=rk::FgD3D11PresentBridge::create(gpu.d11.Get(),
        gpu.context.Get(),gpu.d12.Get(),copyQueue.Get(),gpu.swap.Get());
    REQUIRE(std::holds_alternative<rk::Error>(wrongQueue));
}

TEST_CASE("FG facade submits cached D3D11 colour before each lower Present",
    "[fg_d3d11_present_bridge]") {
    Devices gpu;
    ComPtr<IDXGISwapChain4> native;
    REQUIRE(SUCCEEDED(gpu.swap.As(&native)));
    struct ObservedFrame {
        UINT index{};
        std::array<std::uint8_t,4> pixel{};
        bool read{};
    };
    std::array<ObservedFrame,4> submitted{};
    std::size_t submittedCount{};
    ComPtr<IDXGISwapChain4> observed;
    observed.Attach(new rk_test::FgObservedSwap(native.Get(),[&](UINT index) noexcept {
        if(submittedCount>=submitted.size())return E_FAIL;
        auto& frame=submitted[submittedCount++];
        frame.index=index;
        frame.read=tryReadPixel(gpu,index,frame.pixel);
        return frame.read?S_OK:E_FAIL;
    }));
    auto made=rk::FgD3D11SwapFacade::create(gpu.d11.Get(),gpu.context.Get(),
        gpu.d12.Get(),gpu.queue.Get(),observed.Get());
    REQUIRE(std::holds_alternative<ComPtr<IDXGISwapChain4>>(made));
    auto facade=std::move(std::get<ComPtr<IDXGISwapChain4>>(made));
    ComPtr<ID3D11Texture2D> cached;
    REQUIRE(SUCCEEDED(facade->GetBuffer(0,IID_PPV_ARGS(&cached))));
    ComPtr<ID3D11RenderTargetView> view;
    REQUIRE(SUCCEEDED(gpu.d11->CreateRenderTargetView(cached.Get(),nullptr,&view)));
    const std::array<std::array<std::uint8_t,4>,4> colours{{
        {{255,0,0,255}},{{0,255,0,255}},{{0,0,255,255}},{{255,255,0,255}}
    }};
    for(std::size_t frame=0;frame<colours.size();++frame) {
        const auto& expected=colours[frame];
        const float clear[]{expected[0]/255.0f,expected[1]/255.0f,
            expected[2]/255.0f,1.0f};
        gpu.context->ClearRenderTargetView(view.Get(),clear);
        REQUIRE(SUCCEEDED(facade->Present(0,DXGI_PRESENT_TEST)));
        REQUIRE(submittedCount==frame);
        if(frame==2) {
            DXGI_PRESENT_PARAMETERS params{};
            REQUIRE(SUCCEEDED(facade->Present1(0,0,&params)));
        } else REQUIRE(SUCCEEDED(facade->Present(0,0)));
        REQUIRE(submittedCount==frame+1);
        REQUIRE(submitted[frame].read);
        REQUIRE(submitted[frame].pixel==expected);
        REQUIRE(submitted[frame].index<2);
    }
    const auto beforeRejected=facade->GetCurrentBackBufferIndex();
    REQUIRE(facade->Present(0,0)==E_FAIL); // Observer capacity rejects the call.
    REQUIRE(native->GetCurrentBackBufferIndex()==beforeRejected);
}

TEST_CASE("FG swap facade exposes only the D3D11 game-facing device and buffers",
    "[fg_d3d11_present_bridge]") {
    Devices gpu;
    auto made=rk::FgD3D11SwapFacade::create(gpu.d11.Get(),gpu.context.Get(),
        gpu.d12.Get(),gpu.queue.Get(),gpu.swap.Get());
    REQUIRE(std::holds_alternative<ComPtr<IDXGISwapChain4>>(made));
    auto facade=std::move(std::get<ComPtr<IDXGISwapChain4>>(made));
    ComPtr<IDXGISwapChain1> one;
    ComPtr<IDXGISwapChain3> three;
    REQUIRE(SUCCEEDED(facade.As(&one)));
    REQUIRE(SUCCEEDED(facade.As(&three)));
    ComPtr<IUnknown> identityOne,identityThree;
    REQUIRE(SUCCEEDED(one.As(&identityOne)));
    REQUIRE(SUCCEEDED(three.As(&identityThree)));
    REQUIRE(identityOne.Get()==identityThree.Get());
    ComPtr<ID3D11Device> gameDevice;
    REQUIRE(SUCCEEDED(facade->GetDevice(IID_PPV_ARGS(&gameDevice))));
    REQUIRE(gameDevice.Get()==gpu.d11.Get());
    ComPtr<ID3D12Device> hiddenDevice;
    REQUIRE(facade->GetDevice(IID_PPV_ARGS(&hiddenDevice))==E_NOINTERFACE);
    const auto index=0u; // D3D11 callers cache the logical back buffer.
    ComPtr<ID3D11Texture2D> gameBuffer;
    REQUIRE(SUCCEEDED(facade->GetBuffer(index,IID_PPV_ARGS(&gameBuffer))));
    ComPtr<ID3D11Device> bufferDevice;
    gameBuffer->GetDevice(&bufferDevice);
    REQUIRE(bufferDevice.Get()==gpu.d11.Get());
    ComPtr<ID3D12Resource> hiddenBuffer;
    REQUIRE(facade->GetBuffer(index,IID_PPV_ARGS(&hiddenBuffer))==E_NOINTERFACE);
    ComPtr<ID3D11RenderTargetView> view;
    REQUIRE(SUCCEEDED(gpu.d11->CreateRenderTargetView(gameBuffer.Get(),nullptr,&view)));
    const float green[]{0.0f,1.0f,0.0f,1.0f};
    gpu.context->ClearRenderTargetView(view.Get(),green);
    REQUIRE(SUCCEEDED(facade->Present(0,DXGI_PRESENT_TEST)));
    REQUIRE(SUCCEEDED(facade->Present(0,0)));
    ComPtr<ID3D11Texture2D> indexedBuffer;
    REQUIRE(SUCCEEDED(facade->GetBuffer(facade->GetCurrentBackBufferIndex(),
        IID_PPV_ARGS(&indexedBuffer))));
    REQUIRE(indexedBuffer.Get()==gameBuffer.Get());
    indexedBuffer.Reset();
    REQUIRE(SUCCEEDED(facade->Present(0,0)));
    REQUIRE(facade->ResizeBuffers(2,80,60,DXGI_FORMAT_R8G8B8A8_UNORM,0)==
        DXGI_ERROR_INVALID_CALL);
    ID3D11RenderTargetView* bound[]{view.Get()};
    gpu.context->OMSetRenderTargets(1,bound,nullptr);
    gameBuffer.Reset();
    bufferDevice.Reset();
    view.Reset();
    REQUIRE(facade->ResizeBuffers(2,80,60,DXGI_FORMAT_R8G8B8A8_UNORM,0)==
        DXGI_ERROR_INVALID_CALL); // Context binding retains the view.
    gpu.context->ClearState();
    gpu.context->Flush();
    REQUIRE(facade->ResizeBuffers(7,80,60,
        DXGI_FORMAT_R8G8B8A8_UNORM,0)==DXGI_ERROR_INVALID_CALL);
    DXGI_SWAP_CHAIN_DESC unchanged{};
    REQUIRE(SUCCEEDED(facade->GetDesc(&unchanged)));
    REQUIRE(unchanged.BufferDesc.Width==64);
    REQUIRE(unchanged.BufferDesc.Height==48);
    REQUIRE(SUCCEEDED(facade->ResizeBuffers(2,80,60,
        DXGI_FORMAT_R8G8B8A8_UNORM,0)));
    DXGI_SWAP_CHAIN_DESC resized{};
    REQUIRE(SUCCEEDED(facade->GetDesc(&resized)));
    REQUIRE(resized.BufferDesc.Width==80);
    REQUIRE(resized.BufferDesc.Height==60);
    const auto resizedIndex=0u;
    REQUIRE(SUCCEEDED(facade->GetBuffer(resizedIndex,
        IID_PPV_ARGS(&gameBuffer))));
    D3D11_TEXTURE2D_DESC resizedTexture{};
    gameBuffer->GetDesc(&resizedTexture);
    REQUIRE(resizedTexture.Width==80);
    REQUIRE(resizedTexture.Height==60);
    REQUIRE(SUCCEEDED(gpu.d11->CreateRenderTargetView(gameBuffer.Get(),
        nullptr,&view)));
    const float blue[]{0.0f,0.0f,1.0f,1.0f};
    gpu.context->ClearRenderTargetView(view.Get(),blue);
    REQUIRE(SUCCEEDED(facade->Present(0,0)));
    view.Reset();
    gameBuffer.Reset();
    gpu.context->ClearState();
    gpu.context->Flush();
    const UINT nodes[]{0,0};
    IUnknown* queues[]{gpu.queue.Get(),gpu.queue.Get()};
    REQUIRE(SUCCEEDED(facade->ResizeBuffers1(2,96,64,
        DXGI_FORMAT_R8G8B8A8_UNORM,0,nodes,queues)));
    REQUIRE(SUCCEEDED(facade->GetDesc(&resized)));
    REQUIRE(resized.BufferDesc.Width==96);
    REQUIRE(resized.BufferDesc.Height==64);
    const auto third=0u;
    REQUIRE(SUCCEEDED(facade->GetBuffer(third,IID_PPV_ARGS(&gameBuffer))));
    REQUIRE(SUCCEEDED(gpu.d11->CreateRenderTargetView(gameBuffer.Get(),
        nullptr,&view)));
    const float yellow[]{1.0f,1.0f,0.0f,1.0f};
    gpu.context->ClearRenderTargetView(view.Get(),yellow);
    DXGI_PRESENT_PARAMETERS parameters{};
    REQUIRE(SUCCEEDED(facade->Present1(0,0,&parameters)));
    for(UINT n=0;n<100;++n) {
        view.Reset();
        gameBuffer.Reset();
        gpu.context->ClearState();
        gpu.context->Flush();
        REQUIRE(SUCCEEDED(facade->ResizeBuffers(2,100+n,70+n,
            DXGI_FORMAT_R8G8B8A8_UNORM,0)));
        const auto current=0u;
        REQUIRE(SUCCEEDED(facade->GetBuffer(current,
            IID_PPV_ARGS(&gameBuffer))));
        D3D11_TEXTURE2D_DESC extent{};
        gameBuffer->GetDesc(&extent);
        REQUIRE(extent.Width==100+n);
        REQUIRE(extent.Height==70+n);
        REQUIRE(SUCCEEDED(gpu.d11->CreateRenderTargetView(gameBuffer.Get(),
            nullptr,&view)));
        gpu.context->ClearRenderTargetView(view.Get(),yellow);
        REQUIRE(SUCCEEDED(facade->Present(0,0)));
    }
    view.Reset();
    gameBuffer.Reset();
    gpu.context->ClearState();
    gpu.context->Flush();
    REQUIRE(SetWindowPos(gpu.window,nullptr,0,0,120,84,
        SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE));
    REQUIRE(SUCCEEDED(facade->ResizeBuffers(2,0,0,
        DXGI_FORMAT_UNKNOWN,0)));
    REQUIRE(SUCCEEDED(facade->GetDesc(&resized)));
    REQUIRE(resized.BufferDesc.Width==120);
    REQUIRE(resized.BufferDesc.Height==84);
    const auto windowIndex=0u;
    REQUIRE(SUCCEEDED(facade->GetBuffer(windowIndex,
        IID_PPV_ARGS(&gameBuffer))));
    D3D11_TEXTURE2D_DESC windowTexture{};
    gameBuffer->GetDesc(&windowTexture);
    REQUIRE(windowTexture.Width==120);
    REQUIRE(windowTexture.Height==84);
}
