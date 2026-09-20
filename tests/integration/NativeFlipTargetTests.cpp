#include <catch2/catch_test_macros.hpp>
#include "rk/NativeFlipTarget.hpp"
#include <wrl/client.h>

namespace {
using Microsoft::WRL::ComPtr;
struct Window {
    HWND handle{CreateWindowW(L"STATIC",L"RazKolbas flip target fixture",
        WS_OVERLAPPED,0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr)};
    ~Window(){if(handle)DestroyWindow(handle);}
};
ComPtr<IUnknown> identity(IUnknown* object) {
    ComPtr<IUnknown> value;
    if(object)object->QueryInterface(IID_PPV_ARGS(value.GetAddressOf()));
    return value;
}
}

TEST_CASE("Current native flip target matches IDXGISwapChain3 buffer index", "[native_flip]") {
    Window window;
    REQUIRE(window.handle!=nullptr);
    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferDesc.Width=64;desc.BufferDesc.Height=32;
    desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount=3;desc.OutputWindow=window.handle;desc.Windowed=TRUE;
    desc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGISwapChain> swap;
    REQUIRE(SUCCEEDED(D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_WARP,
        nullptr,0,nullptr,0,D3D11_SDK_VERSION,&desc,&swap,&device,nullptr,&context)));
    ComPtr<IDXGISwapChain3> flip;
    REQUIRE(SUCCEEDED(swap.As(&flip)));
    {
        auto target=rk::acquireNativeFlipTarget(swap.Get(),device.Get(),{64,32});
        REQUIRE(std::holds_alternative<rk::NativeFlipTarget>(target));
        auto& current=std::get<rk::NativeFlipTarget>(target);
        REQUIRE(current.index==flip->GetCurrentBackBufferIndex());
        REQUIRE(current.texture!=nullptr);
        REQUIRE(current.view!=nullptr);
        ComPtr<ID3D11Texture2D> direct;
        REQUIRE(SUCCEEDED(swap->GetBuffer(current.index,IID_PPV_ARGS(&direct))));
        REQUIRE(identity(direct.Get()).Get()==identity(current.texture.Get()).Get());
        ComPtr<ID3D11Resource> viewResource;
        current.view->GetResource(&viewResource);
        REQUIRE(identity(viewResource.Get()).Get()==identity(current.texture.Get()).Get());
    }
    for(unsigned frame=0;frame<3;++frame) {
        REQUIRE(SUCCEEDED(swap->Present(0,0)));
        auto current=rk::acquireNativeFlipTarget(swap.Get(),device.Get(),{64,32});
        REQUIRE(std::holds_alternative<rk::NativeFlipTarget>(current));
        REQUIRE(std::get<rk::NativeFlipTarget>(current).index==
            flip->GetCurrentBackBufferIndex());
    }
    REQUIRE(std::holds_alternative<rk::Error>(
        rk::acquireNativeFlipTarget(swap.Get(),device.Get(),{63,32})));
    REQUIRE(std::holds_alternative<rk::Error>(
        rk::acquireNativeFlipTarget(nullptr,device.Get(),{64,32})));
}
