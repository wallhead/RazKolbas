#include <catch2/catch_test_macros.hpp>
#include "rk/FgLowerSwap.hpp"
#include "rk/FgNativePresentBackend.hpp"
#include "rk/FgSwapFacadeProbe.hpp"
#include <d3d11.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace {
struct HiddenWindow {
    HWND handle{};
    HiddenWindow() {
        handle=CreateWindowExW(0,L"STATIC",L"RazKolbas FG DXGI harness",
            WS_POPUP,0,0,96,72,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        REQUIRE(handle!=nullptr);
    }
    ~HiddenWindow() { if(handle)DestroyWindow(handle); }
};
struct WarpSwap {
    HiddenWindow window;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGISwapChain1> swap;
    WarpSwap() {
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
        REQUIRE(SUCCEEDED(factory->CreateSwapChainForHwnd(device.Get(),
            window.handle,&desc,nullptr,nullptr,&swap)));
    }
};
}

TEST_CASE("FG lower swap retains COM identity and forwards both Present methods",
    "[fg_lower_swap]") {
    WarpSwap warp;
    auto made=rk::FgLowerSwap::create(warp.swap.Get());
    REQUIRE(std::holds_alternative<rk::FgLowerSwap>(made));
    auto lower=std::get<rk::FgLowerSwap>(std::move(made));
    ComPtr<IUnknown> originalIdentity;
    ComPtr<IUnknown> ownedIdentity;
    REQUIRE(SUCCEEDED(warp.swap.As(&originalIdentity)));
    REQUIRE(SUCCEEDED(lower.queryInterface(IID_PPV_ARGS(&ownedIdentity))));
    REQUIRE(originalIdentity.Get()==ownedIdentity.Get());
    const rk::FgPresentCall test{rk::FgPresentMethod::Present,0,DXGI_PRESENT_TEST};
    REQUIRE(lower.present(test)==warp.swap->Present(0,DXGI_PRESENT_TEST));
    DXGI_PRESENT_PARAMETERS parameters{};
    const rk::FgPresentCall test1{rk::FgPresentMethod::Present1,0,
        DXGI_PRESENT_TEST,&parameters};
    REQUIRE(lower.present(test1)==warp.swap->Present1(0,DXGI_PRESENT_TEST,
        &parameters));
    REQUIRE(lower.present({rk::FgPresentMethod::Present1,0,0,nullptr})==E_INVALIDARG);
}

TEST_CASE("FG lower resize returns the real DXGI error for a held backbuffer",
    "[fg_lower_swap]") {
    WarpSwap warp;
    auto made=rk::FgLowerSwap::create(warp.swap.Get());
    REQUIRE(std::holds_alternative<rk::FgLowerSwap>(made));
    auto lower=std::get<rk::FgLowerSwap>(std::move(made));
    ComPtr<ID3D11Texture2D> held;
    REQUIRE(SUCCEEDED(lower.getBuffer(0,IID_PPV_ARGS(&held))));
    const rk::FgResizeCall resize{rk::FgResizeMethod::ResizeBuffers,
        2,80,52,DXGI_FORMAT_B8G8R8A8_UNORM,0};
    REQUIRE(lower.resize(resize)==DXGI_ERROR_INVALID_CALL);
    DXGI_SWAP_CHAIN_DESC desc{};
    REQUIRE(SUCCEEDED(lower.getDesc(&desc)));
    REQUIRE(desc.BufferDesc.Width==64);
    held.Reset();
    REQUIRE(SUCCEEDED(lower.resize(resize)));
    REQUIRE(SUCCEEDED(lower.getDesc(&desc)));
    REQUIRE(desc.BufferDesc.Width==80);
    REQUIRE(desc.BufferDesc.Height==52);
    for(unsigned i=0;i<100;++i) {
        const rk::FgResizeCall cycle{rk::FgResizeMethod::ResizeBuffers,
            2,(i&1)?80u:64u,(i&1)?52u:48u,
            DXGI_FORMAT_B8G8R8A8_UNORM,0};
        REQUIRE(SUCCEEDED(lower.resize(cycle)));
    }
    REQUIRE(SUCCEEDED(lower.getDesc(&desc)));
    REQUIRE(desc.BufferDesc.Width==80);
}

TEST_CASE("FG native owner presents one real source and preserves DXGI result",
    "[fg_lower_swap]") {
    WarpSwap warp;
    auto made=rk::FgLowerSwap::create(warp.swap.Get());
    REQUIRE(std::holds_alternative<rk::FgLowerSwap>(made));
    auto lower=std::get<rk::FgLowerSwap>(std::move(made));
    rk::FgNativePresentBackend backend(lower);
    const rk::FgProviderSession session({false,rk::FgProvider::Off,1},
        rk::FgSrProvider::Native,0x10de,{});
    rk::FgPresentationCoordinator coordinator(session,backend);
    rk::FgSourceFrame source{};
    source.source=1;source.generation=7;source.presentToken=11;
    const rk::FgPresentCall test{rk::FgPresentMethod::Present,0,DXGI_PRESENT_TEST};
    REQUIRE(std::holds_alternative<rk::FgPresentOutcome>(
        coordinator.present(source,false,test)));
    REQUIRE(backend.realPresentCalls()==0);
    const rk::FgPresentCall real{rk::FgPresentMethod::Present,0,0};
    const auto presented=coordinator.present(source,false,real);
    REQUIRE(std::holds_alternative<rk::FgPresentOutcome>(presented));
    REQUIRE(std::get<rk::FgPresentOutcome>(presented).resultCode==
        backend.lastResult());
    REQUIRE(backend.realPresentCalls()==1);
    REQUIRE(std::holds_alternative<rk::Error>(
        coordinator.present(source,false,real)));
    REQUIRE(backend.realPresentCalls()==1);
}

TEST_CASE("FG facade probe identifies the DXGI and D3D11 ownership boundary",
    "[fg_lower_swap]") {
    WarpSwap warp;
    const auto facts=rk::inspectFgSwapFacade(warp.swap.Get(),warp.device.Get());
    REQUIRE(SUCCEEDED(facts.getDesc));
    REQUIRE(facts.desc.BufferDesc.Width==64);
    REQUIRE(SUCCEEDED(facts.swap1));
    REQUIRE(SUCCEEDED(facts.swap3));
    REQUIRE(SUCCEEDED(facts.getD3D11Device));
    REQUIRE(facts.expectedDeviceIdentity);
    REQUIRE(FAILED(facts.getD3D12Device));
    const auto missing=rk::inspectFgSwapFacade(nullptr,warp.device.Get());
    REQUIRE(missing.getDesc==E_POINTER);
}
