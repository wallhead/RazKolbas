#include <catch2/catch_test_macros.hpp>
#include "rk/D3D11StateScope.hpp"
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

TEST_CASE("Injected D3D11 work restores Skyrim render bindings and viewport", "[d3d11_state]") {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    REQUIRE(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,
        nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)));
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width=64;desc.Height=32;desc.MipLevels=desc.ArraySize=1;
    desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;
    desc.BindFlags=D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11RenderTargetView> target;
    REQUIRE(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&texture)));
    REQUIRE(SUCCEEDED(device->CreateRenderTargetView(texture.Get(),nullptr,&target)));
    context->OMSetRenderTargets(1,target.GetAddressOf(),nullptr);
    const D3D11_VIEWPORT viewport{3.0f,4.0f,48.0f,20.0f,0.0f,1.0f};
    context->RSSetViewports(1,&viewport);
    {
        auto isolated=rk::D3D11StateScope::begin(context.Get());
        const auto reason=std::holds_alternative<rk::Error>(isolated)?std::get<rk::Error>(isolated).message:"success";
        INFO(reason);
        REQUIRE(std::holds_alternative<std::unique_ptr<rk::D3D11StateScope>>(isolated));
        auto& scope=std::get<std::unique_ptr<rk::D3D11StateScope>>(isolated);
        ComPtr<ID3D11RenderTargetView> during;
        context->OMGetRenderTargets(1,&during,nullptr);
        REQUIRE(during.Get()!=target.Get());
        context->OMSetRenderTargets(1,target.GetAddressOf(),nullptr);
        const D3D11_VIEWPORT changed{0,0,64,32,0,1};
        context->RSSetViewports(1,&changed);
        scope.reset();
    }
    ComPtr<ID3D11RenderTargetView> restored;
    context->OMGetRenderTargets(1,&restored,nullptr);
    REQUIRE(restored.Get()==target.Get());
    UINT count=1;D3D11_VIEWPORT actual{};
    context->RSGetViewports(&count,&actual);
    REQUIRE(count==1);
    REQUIRE(actual.TopLeftX==viewport.TopLeftX);
    REQUIRE(actual.TopLeftY==viewport.TopLeftY);
    REQUIRE(actual.Width==viewport.Width);
    REQUIRE(actual.Height==viewport.Height);
    REQUIRE(std::holds_alternative<rk::Error>(rk::D3D11StateScope::begin(nullptr)));
}
