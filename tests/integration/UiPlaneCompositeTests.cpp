#include <catch2/catch_test_macros.hpp>
#include "rk/UiPlaneComposite.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <wrl/client.h>

namespace {
using Microsoft::WRL::ComPtr;
std::array<std::uint8_t,4> firstPixel(ID3D11Device* device,
    ID3D11DeviceContext* context,ID3D11Texture2D* source) {
    D3D11_TEXTURE2D_DESC desc{};source->GetDesc(&desc);
    desc.BindFlags=0;desc.Usage=D3D11_USAGE_STAGING;
    desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;
    ComPtr<ID3D11Texture2D> staging;
    REQUIRE(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&staging)));
    context->CopyResource(staging.Get(),source);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    REQUIRE(SUCCEEDED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)));
    std::array<std::uint8_t,4> pixel{};
    std::memcpy(pixel.data(),mapped.pData,pixel.size());
    context->Unmap(staging.Get(),0);
    return pixel;
}
}

TEST_CASE("WARP premultiplied UI keeps the scene and its alpha",
    "[ui_plane_composite]") {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL level{};
    REQUIRE(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,
        nullptr,0,D3D11_SDK_VERSION,&device,&level,&context)));
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width=4;desc.Height=4;desc.MipLevels=1;desc.ArraySize=1;
    desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;
    desc.Usage=D3D11_USAGE_DEFAULT;
    desc.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
    ComPtr<ID3D11Texture2D> finalColor,uiColor;
    REQUIRE(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&finalColor)));
    REQUIRE(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&uiColor)));
    ComPtr<ID3D11RenderTargetView> finalView,uiView;
    REQUIRE(SUCCEEDED(device->CreateRenderTargetView(finalColor.Get(),nullptr,&finalView)));
    REQUIRE(SUCCEEDED(device->CreateRenderTargetView(uiColor.Get(),nullptr,&uiView)));
    const float green[4]{0,1,0,1},halfRed[4]{0.5f,0,0,0.5f};
    context->ClearRenderTargetView(finalView.Get(),green);
    context->ClearRenderTargetView(uiView.Get(),halfRed);
    auto* active=uiView.Get();
    context->OMSetRenderTargets(1,&active,nullptr);
    auto result=rk::compositePremultipliedUi(context.Get(),uiColor.Get(),finalView.Get());
    REQUIRE(std::holds_alternative<bool>(result));
    REQUIRE(std::get<bool>(result));
    ComPtr<ID3D11RenderTargetView> restored;
    context->OMGetRenderTargets(1,&restored,nullptr);
    REQUIRE(restored.Get()==uiView.Get());
    const auto pixel=firstPixel(device.Get(),context.Get(),finalColor.Get());
    REQUIRE(pixel[0]>=126);REQUIRE(pixel[0]<=129);
    REQUIRE(pixel[1]>=126);REQUIRE(pixel[1]<=129);
    REQUIRE(pixel[2]==0);REQUIRE(pixel[3]==255);
    REQUIRE(std::holds_alternative<rk::Error>(
        rk::compositePremultipliedUi(context.Get(),finalColor.Get(),finalView.Get())));
}
