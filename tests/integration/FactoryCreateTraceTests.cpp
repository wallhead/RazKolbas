#include <catch2/catch_test_macros.hpp>
#include "rk/FactoryCreateTrace.hpp"
#include <wrl/client.h>

namespace {
using Microsoft::WRL::ComPtr;
struct Observation {
    unsigned calls{};
    IDXGIFactory* factory{};
    IDXGISwapChain* swap{};
    HRESULT result{E_FAIL};
};
void observed(IDXGIFactory* factory,IUnknown*,const DXGI_SWAP_CHAIN_DESC*,
    IDXGISwapChain* swap,HRESULT result,void* value) noexcept {
    auto& state=*static_cast<Observation*>(value);
    ++state.calls;state.factory=factory;state.swap=swap;state.result=result;
}
}

TEST_CASE("Factory CreateSwapChain trace preserves the real WARP COM result",
    "[factory_create_trace]") {
    const auto window=CreateWindowW(L"STATIC",L"RazKolbas factory trace fixture",
        WS_OVERLAPPED,0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    REQUIRE(window!=nullptr);
    ComPtr<ID3D11Device> device;
    REQUIRE(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,
        nullptr,0,D3D11_SDK_VERSION,&device,nullptr,nullptr)));
    ComPtr<IDXGIDevice> dxgiDevice;
    REQUIRE(SUCCEEDED(device.As(&dxgiDevice)));
    ComPtr<IDXGIAdapter> adapter;
    REQUIRE(SUCCEEDED(dxgiDevice->GetAdapter(&adapter)));
    ComPtr<IDXGIFactory> factory;
    REQUIRE(SUCCEEDED(adapter->GetParent(IID_PPV_ARGS(&factory))));
    auto** table=*reinterpret_cast<void***>(factory.Get());
    auto next=reinterpret_cast<rk::FactoryCreateFn>(table[10]);
    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferDesc.Width=64;desc.BufferDesc.Height=32;
    desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount=2;desc.OutputWindow=window;
    desc.Windowed=TRUE;desc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
    Observation state;
    ComPtr<IDXGISwapChain> swap;
    const auto result=rk::observeFactoryCreate(next,factory.Get(),device.Get(),
        &desc,swap.GetAddressOf(),&observed,&state);
    REQUIRE(SUCCEEDED(result));
    REQUIRE(swap!=nullptr);
    REQUIRE(state.calls==1);
    REQUIRE(state.factory==factory.Get());
    REQUIRE(state.swap==swap.Get());
    REQUIRE(state.result==result);
    DXGI_SWAP_CHAIN_DESC actual{};
    REQUIRE(SUCCEEDED(swap->GetDesc(&actual)));
    REQUIRE(actual.BufferDesc.Width==64);
    REQUIRE(actual.BufferDesc.Height==32);
    swap.Reset();
    DestroyWindow(window);
}
