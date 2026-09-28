#include <catch2/catch_test_macros.hpp>
#include "rk/FgResizeTransaction.hpp"
#include <d3d11.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace {
struct Backend final : rk::IFgPresentBackend {
    bool on{},failOff{};
    unsigned offCalls{};
    rk::FgCapability capability() const noexcept override {
        return {rk::FgProvider::Dlss,true,true,1,
            rk::fgSrMask(rk::FgSrProvider::Dlss),true};
    }
    rk::Result<bool> setMode(bool enabled) override {
        if(!enabled) { ++offCalls;if(failOff)return false; }
        on=enabled;return true;
    }
    rk::Result<rk::FgBackendPresent> presentReal(const rk::FgSourceFrame&,
        bool enabled,const rk::FgPresentCall&) override {
        return rk::FgBackendPresent{S_OK,enabled?1u:0u};
    }
};
rk::FgSourceFrame frame(std::uint64_t source) {
    rk::FgSourceFrame f{};
    f.source=source;f.generation=7;f.presentToken=9000+source;
    f.resetEpoch=3;f.render={48,36};f.display={64,48};
    f.sr=rk::FgSrProvider::Dlss;f.renderVendor=0x10de;
    f.ownerReady=true;f.worldActive=true;f.cameraValid=true;
    f.color={source,7,f.display,true,3};f.depth={source,7,f.render,true,3};
    f.motion={source,7,f.render,true,3};
    f.hudless={source,7,f.display,true,3};
    f.uiColorAlpha={source,7,f.display,true,3};
    return f;
}
}

TEST_CASE("FG resize drains before DXGI and advances only after actual success",
    "[fg_resize]") {
    const auto window=CreateWindowExW(0,L"STATIC",L"FG resize test",WS_POPUP,
        0,0,64,48,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    REQUIRE(window!=nullptr);
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL level{};
    REQUIRE(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,
        nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context)));
    ComPtr<IDXGIDevice> dxgi;
    ComPtr<IDXGIAdapter> adapter;
    ComPtr<IDXGIFactory2> factory;
    REQUIRE(SUCCEEDED(device.As(&dxgi)));
    REQUIRE(SUCCEEDED(dxgi->GetAdapter(&adapter)));
    REQUIRE(SUCCEEDED(adapter->GetParent(IID_PPV_ARGS(&factory))));
    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width=64;desc.Height=48;desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount=2;desc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
    ComPtr<IDXGISwapChain1> swap;
    REQUIRE(SUCCEEDED(factory->CreateSwapChainForHwnd(device.Get(),window,
        &desc,nullptr,nullptr,&swap)));
    auto made=rk::FgLowerSwap::create(swap.Get());
    REQUIRE(std::holds_alternative<rk::FgLowerSwap>(made));
    auto lower=std::get<rk::FgLowerSwap>(std::move(made));
    ComPtr<ID3D12Device> d12;
    REQUIRE(SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&d12))));
    D3D12_COMMAND_QUEUE_DESC qdesc{};
    ComPtr<ID3D12CommandQueue> queue;
    REQUIRE(SUCCEEDED(d12->CreateCommandQueue(&qdesc,IID_PPV_ARGS(&queue))));
    auto bridgeResult=rk::FgSharedInputs::create(device.Get(),d12.Get(),queue.Get());
    REQUIRE(std::holds_alternative<std::unique_ptr<rk::FgSharedInputs>>(bridgeResult));
    auto bridge=std::move(std::get<std::unique_ptr<rk::FgSharedInputs>>(bridgeResult));
    rk::FgInputLeaseRing ring(*bridge,7);
    Backend backend;
    rk::FgProviderSession session({false,rk::FgProvider::Dlss,1},
        rk::FgSrProvider::Dlss,0x10de,{backend.capability()});
    rk::FgPresentationCoordinator coordinator(session,backend);
    REQUIRE(std::holds_alternative<rk::FgPresentOutcome>(
        coordinator.present(frame(1),true)));
    rk::FgResizeTransaction transaction(coordinator,ring,lower);
    const rk::FgResizeCall call{rk::FgResizeMethod::ResizeBuffers,
        2,80,52,DXGI_FORMAT_B8G8R8A8_UNORM,0};
    std::array<ComPtr<ID3D11Texture2D>,5> sourceTextures;
    rk::FgInputSources inputs{};
    for(std::size_t i=0;i<sourceTextures.size();++i) {
        D3D11_TEXTURE2D_DESC inputDesc{};
        inputDesc.Width=(i==1||i==2)?48:64;
        inputDesc.Height=(i==1||i==2)?36:48;
        inputDesc.MipLevels=1;inputDesc.ArraySize=1;
        inputDesc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;
        inputDesc.SampleDesc.Count=1;inputDesc.Usage=D3D11_USAGE_DEFAULT;
        inputDesc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        REQUIRE(SUCCEEDED(device->CreateTexture2D(&inputDesc,nullptr,
            &sourceTextures[i])));
        inputs.textures[i]=sourceTextures[i].Get();
    }
    auto prepared=ring.prepare(frame(1),inputs,context.Get(),{7,0,0,0,0,0});
    REQUIRE(std::holds_alternative<rk::FgInputLease>(prepared));
    const auto lease=std::get<rk::FgInputLease>(std::move(prepared));
    REQUIRE(ring.submit(lease,{7,lease.lastCopy.producer,
        lease.lastCopy.copy,1,2,3}));
    const auto pending=rk::FgFenceProgress{7,lease.lastCopy.producer,
        lease.lastCopy.copy,0,2,3};
    REQUIRE(std::holds_alternative<rk::Error>(transaction.resize(call,8,pending)));
    REQUIRE(coordinator.enabled());
    REQUIRE(backend.offCalls==0);
    DXGI_SWAP_CHAIN_DESC current{};
    REQUIRE(SUCCEEDED(lower.getDesc(&current)));
    REQUIRE(current.BufferDesc.Width==64);
    const auto retired=rk::FgFenceProgress{7,lease.lastCopy.producer,
        lease.lastCopy.copy,1,2,3};
    ComPtr<ID3D11Texture2D> held;
    REQUIRE(SUCCEEDED(lower.getBuffer(0,IID_PPV_ARGS(&held))));
    const auto failed=transaction.resize(call,8,retired);
    REQUIRE(std::holds_alternative<HRESULT>(failed));
    REQUIRE(std::get<HRESULT>(failed)==DXGI_ERROR_INVALID_CALL);
    REQUIRE(backend.offCalls==1);
    REQUIRE(ring.generation()==7);
    held.Reset();
    const auto succeeded=transaction.resize(call,8,retired);
    REQUIRE(std::holds_alternative<HRESULT>(succeeded));
    REQUIRE(SUCCEEDED(std::get<HRESULT>(succeeded)));
    REQUIRE(ring.generation()==8);
    REQUIRE(backend.offCalls==1);
    swap.Reset();
    DestroyWindow(window);
}
