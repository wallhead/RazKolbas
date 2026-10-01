#include <catch2/catch_test_macros.hpp>
#include "rk/FgWorldGuideLatch.hpp"
#include <array>
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
    ComPtr<ID3D11Texture2D> texture(UINT width,UINT height) {
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width=width;desc.Height=height;desc.MipLevels=1;desc.ArraySize=1;
        desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;
        desc.Usage=D3D11_USAGE_DEFAULT;
        desc.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
        ComPtr<ID3D11Texture2D> result;
        REQUIRE(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&result)));
        return result;
    }
    void clear(ID3D11Texture2D* texture,float red) {
        ComPtr<ID3D11RenderTargetView> view;
        REQUIRE(SUCCEEDED(device->CreateRenderTargetView(texture,nullptr,&view)));
        const float color[]{red,0,0,1};
        context->ClearRenderTargetView(view.Get(),color);
    }
    std::uint8_t red(ID3D11Texture2D* texture) {
        D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);
        desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;
        desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;
        ComPtr<ID3D11Texture2D> staging;
        REQUIRE(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&staging)));
        context->CopyResource(staging.Get(),texture);
        D3D11_MAPPED_SUBRESOURCE mapped{};
        REQUIRE(SUCCEEDED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)));
        const auto result=*static_cast<std::uint8_t*>(mapped.pData);
        context->Unmap(staging.Get(),0);
        return result;
    }
};
rk::FgBoundarySample boundary(std::uint64_t source=7) {
    rk::FgBoundarySample result{};
    result.kind=rk::FgBoundaryKind::Ready;result.phaseReady=true;
    result.world=source;result.epoch=3;result.realPresent=21;
    result.worldThread=GetCurrentThreadId();
    result.presentThread=result.worldThread;
    return result;
}
}

TEST_CASE("FG world guides retain raw render inputs and immutable pre-UI pixels",
    "[fg_world_guides]") {
    Warp gpu;
    rk::FgWorldGuideLatch latch;
    auto depth=gpu.texture(20,12),motion=gpu.texture(20,12);
    auto display=gpu.texture(32,20);
    gpu.clear(display.Get(),1.0f);
    auto captured=latch.capture(7,5,{20,12},{32,20},gpu.context.Get(),
        depth.Get(),motion.Get(),display.Get());
    REQUIRE(std::holds_alternative<bool>(captured));
    gpu.clear(display.Get(),0.0f);
    auto published=latch.take(boundary());
    REQUIRE(published);
    REQUIRE(published->frame.source==7);
    REQUIRE(published->frame.generation==5);
    REQUIRE(published->frame.presentToken==21);
    REQUIRE(published->frame.resetEpoch==3);
    REQUIRE_FALSE(published->frame.depth.ready);
    REQUIRE_FALSE(published->frame.motion.ready);
    REQUIRE_FALSE(published->frame.hudless.ready);
    REQUIRE_FALSE(published->frame.color.ready);
    REQUIRE_FALSE(published->frame.uiColorAlpha.ready);
    REQUIRE(published->depth.Get()==depth.Get());
    REQUIRE(published->motion.Get()==motion.Get());
    REQUIRE(gpu.red(published->hudless.Get())==255);
    REQUIRE_FALSE(latch.take(boundary()));
}

TEST_CASE("FG world guides reject stale, cross-thread and wrong-size inputs",
    "[fg_world_guides]") {
    Warp gpu;
    rk::FgWorldGuideLatch latch;
    auto depth=gpu.texture(20,12),motion=gpu.texture(20,12);
    auto display=gpu.texture(32,20),wrong=gpu.texture(16,10);
    REQUIRE(std::holds_alternative<rk::Error>(latch.capture(7,5,
        {20,12},{32,20},gpu.context.Get(),depth.Get(),motion.Get(),wrong.Get())));
    REQUIRE(std::holds_alternative<bool>(latch.capture(7,5,
        {20,12},{32,20},gpu.context.Get(),depth.Get(),motion.Get(),display.Get())));
    REQUIRE(std::holds_alternative<rk::Error>(latch.capture(7,5,
        {20,12},{32,20},gpu.context.Get(),depth.Get(),motion.Get(),display.Get())));
    auto test=boundary();test.kind=rk::FgBoundaryKind::Test;
    REQUIRE_FALSE(latch.take(test));
    auto wrongThread=boundary();++wrongThread.presentThread;
    REQUIRE_FALSE(latch.take(wrongThread));
    REQUIRE_FALSE(latch.take(boundary()));
    REQUIRE(std::holds_alternative<bool>(latch.capture(8,5,
        {20,12},{32,20},gpu.context.Get(),depth.Get(),motion.Get(),display.Get())));
    auto stale=boundary(7);
    REQUIRE_FALSE(latch.take(stale));
    REQUIRE_FALSE(latch.take(boundary(8)));
}
