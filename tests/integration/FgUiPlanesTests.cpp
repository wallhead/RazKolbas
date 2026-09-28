#include <catch2/catch_test_macros.hpp>
#include "rk/FgUiPlanes.hpp"
#include <array>
#include <cstdint>
#include <variant>

using Microsoft::WRL::ComPtr;

namespace {
struct Warp {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    Warp() {
        D3D_FEATURE_LEVEL level{};
        REQUIRE(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,
            nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context)));
    }
    ComPtr<ID3D11Texture2D> colour(std::uint32_t width=32,
        std::uint32_t height=20,
        DXGI_FORMAT format=DXGI_FORMAT_R8G8B8A8_UNORM) {
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width=width;desc.Height=height;desc.MipLevels=1;desc.ArraySize=1;
        desc.Format=format;desc.SampleDesc.Count=1;
        desc.Usage=D3D11_USAGE_DEFAULT;
        desc.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
        ComPtr<ID3D11Texture2D> texture;
        REQUIRE(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&texture)));
        return texture;
    }
    void clear(ID3D11Texture2D* texture,const std::array<float,4>& rgba) {
        ComPtr<ID3D11RenderTargetView> view;
        REQUIRE(SUCCEEDED(device->CreateRenderTargetView(texture,nullptr,&view)));
        context->ClearRenderTargetView(view.Get(),rgba.data());
    }
    std::array<std::uint8_t,4> pixel(ID3D11Texture2D* texture) {
        D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);
        desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;
        desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;
        ComPtr<ID3D11Texture2D> staging;
        REQUIRE(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&staging)));
        context->CopyResource(staging.Get(),texture);
        D3D11_MAPPED_SUBRESOURCE mapped{};
        REQUIRE(SUCCEEDED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)));
        const auto* data=static_cast<const std::uint8_t*>(mapped.pData);
        const std::array<std::uint8_t,4> result{data[0],data[1],data[2],data[3]};
        context->Unmap(staging.Get(),0);
        return result;
    }
};
rk::FgSourceFrame frame(std::uint64_t source=1,
    std::uint64_t generation=7) {
    rk::FgSourceFrame result{};
    result.source=source;result.generation=generation;
    result.presentToken=9000+source;result.resetEpoch=3;
    result.display={32,20};result.render={20,12};
    return result;
}
}

TEST_CASE("FG UI planes preserve pre-UI, separate UI and final pixels",
    "[fg_ui_planes]") {
    Warp gpu;
    rk::FgUiPlanes planes(gpu.device.Get(),7);
    const auto source=frame();
    auto final=gpu.colour(),ui=gpu.colour();
    gpu.clear(final.Get(),{1,0,0,1});
    REQUIRE(std::holds_alternative<bool>(planes.captureBeforeUi(source,
        gpu.context.Get(),final.Get())));
    gpu.clear(final.Get(),{0.5f,0,0.5f,1});
    // Premultiplied blue UI at half alpha over the red HUD-less scene.
    gpu.clear(ui.Get(),{0,0,0.5f,0.5f});
    auto captured=planes.finish(source,gpu.context.Get(),ui.Get(),
        final.Get(),{0,0,32,20});
    REQUIRE(std::holds_alternative<rk::FgUiPlaneFrame>(captured));
    const auto& result=std::get<rk::FgUiPlaneFrame>(captured);
    REQUIRE(gpu.pixel(result.hudless.Get())==
        std::array<std::uint8_t,4>{255,0,0,255});
    REQUIRE(gpu.pixel(result.uiColorAlpha.Get())==
        std::array<std::uint8_t,4>{0,0,128,128});
    REQUIRE(gpu.pixel(result.finalColor.Get())==
        std::array<std::uint8_t,4>{128,0,128,255});
    REQUIRE(result.hudlessStamp.source==1);
    REQUIRE(result.uiStamp.source==1);
    REQUIRE(result.presentToken==source.presentToken);
}

TEST_CASE("FG UI capture rejects absent, aliased and stale UI planes",
    "[fg_ui_planes]") {
    Warp gpu;
    rk::FgUiPlanes planes(gpu.device.Get(),7);
    const auto source=frame();
    auto final=gpu.colour(),ui=gpu.colour(),other=gpu.colour();
    auto reduced=gpu.colour(20,12);
    gpu.clear(final.Get(),{1,0,0,1});
    REQUIRE(std::holds_alternative<bool>(planes.captureBeforeUi(source,
        gpu.context.Get(),final.Get())));
    REQUIRE(std::holds_alternative<rk::Error>(planes.finish(source,
        gpu.context.Get(),nullptr,final.Get(),{0,0,32,20})));
    REQUIRE(std::holds_alternative<bool>(planes.captureBeforeUi(source,
        gpu.context.Get(),final.Get())));
    REQUIRE(std::holds_alternative<rk::Error>(planes.finish(source,
        gpu.context.Get(),final.Get(),final.Get(),{0,0,32,20})));
    REQUIRE(std::holds_alternative<bool>(planes.captureBeforeUi(source,
        gpu.context.Get(),final.Get())));
    REQUIRE(std::holds_alternative<rk::Error>(planes.finish(source,
        gpu.context.Get(),final.Get(),other.Get(),{0,0,32,20})));
    REQUIRE(std::holds_alternative<bool>(planes.captureBeforeUi(source,
        gpu.context.Get(),final.Get())));
    REQUIRE(std::holds_alternative<rk::Error>(planes.finish(frame(2),
        gpu.context.Get(),ui.Get(),final.Get(),{0,0,32,20})));
    REQUIRE(std::holds_alternative<bool>(planes.captureBeforeUi(source,
        gpu.context.Get(),final.Get())));
    REQUIRE(std::holds_alternative<rk::Error>(planes.finish(source,
        gpu.context.Get(),reduced.Get(),final.Get(),{0,0,32,20})));
    auto lowAlpha=gpu.colour(32,20,DXGI_FORMAT_R10G10B10A2_UNORM);
    REQUIRE(std::holds_alternative<bool>(planes.captureBeforeUi(source,
        gpu.context.Get(),final.Get())));
    REQUIRE(std::holds_alternative<rk::Error>(planes.finish(source,
        gpu.context.Get(),lowAlpha.Get(),final.Get(),{0,0,32,20})));
    REQUIRE(std::holds_alternative<bool>(planes.captureBeforeUi(source,
        gpu.context.Get(),final.Get())));
    REQUIRE_FALSE(planes.advanceGeneration(8));
    planes.discard();
    REQUIRE(planes.advanceGeneration(8));
    REQUIRE(std::holds_alternative<rk::Error>(planes.captureBeforeUi(source,
        gpu.context.Get(),final.Get())));
    REQUIRE(gpu.pixel(final.Get())==
        std::array<std::uint8_t,4>{255,0,0,255});
}
