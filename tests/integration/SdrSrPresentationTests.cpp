#include <catch2/catch_test_macros.hpp>
#include "rk/SdrSrPresentation.hpp"
#include "rk/FrameProbe.hpp"
#include <wrl/client.h>
#include <array>
#include <chrono>
#include <cstdint>
#include <thread>

using Microsoft::WRL::ComPtr;

TEST_CASE("Failed reduced SR publishes a display-sized SDR image", "[sdr_sr_presentation]") {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    REQUIRE(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,
        nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)));
    D3D11_TEXTURE2D_DESC sourceDesc{};
    sourceDesc.Width=sourceDesc.Height=2;
    sourceDesc.MipLevels=sourceDesc.ArraySize=sourceDesc.SampleDesc.Count=1;
    sourceDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    sourceDesc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    constexpr std::array<std::uint8_t,16> colours{
        255,0,0,255, 0,255,0,255,
        0,0,255,255, 255,255,255,255};
    const D3D11_SUBRESOURCE_DATA pixels{colours.data(),8,0};
    ComPtr<ID3D11Texture2D> source;
    REQUIRE(SUCCEEDED(device->CreateTexture2D(&sourceDesc,&pixels,&source)));
    auto displayDesc=sourceDesc;
    displayDesc.Width=displayDesc.Height=4;
    displayDesc.BindFlags=D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> display;
    REQUIRE(SUCCEEDED(device->CreateTexture2D(&displayDesc,nullptr,&display)));
    ComPtr<ID3D11RenderTargetView> view;
    REQUIRE(SUCCEEDED(device->CreateRenderTargetView(display.Get(),nullptr,&view)));
    context->OMSetRenderTargets(1,view.GetAddressOf(),nullptr);
    unsigned calls{};
    auto submitted=rk::presentSdrSrFrame(context.Get(),source.Get(),display.Get(),[&]()->rk::Result<bool> {
        ++calls;
        return rk::Error{rk::ErrorCode::Unavailable,"Injected SR evaluation failure"};
    });
    const auto error=std::get_if<rk::Error>(&submitted);
    INFO((error?error->message:"No provider error"));
    REQUIRE(std::holds_alternative<rk::SdrSrFrameResult>(submitted));
    auto frame=std::move(std::get<rk::SdrSrFrameResult>(submitted));
    REQUIRE(calls==1);
    REQUIRE(frame.mode()==rk::SdrSrFrameMode::SpatialFallback);
    REQUIRE(frame.providerFailure().has_value());
    const auto removed=rk::presentSdrSrFrame(context.Get(),source.Get(),display.Get(),
        []()->rk::Result<bool> {
            return rk::Error{rk::ErrorCode::DeviceRemoved,"Injected device removal"};
        });
    REQUIRE(std::holds_alternative<rk::Error>(removed));
    REQUIRE(std::get<rk::Error>(removed).code==rk::ErrorCode::DeviceRemoved);
    source.Reset();
    context->Flush();
    bool ready=false;
    for(unsigned attempt=0;attempt<500&&!ready;++attempt) {
        const auto complete=frame.complete(context.Get());
        REQUIRE(std::holds_alternative<bool>(complete));
        ready=std::get<bool>(complete);
        if(!ready)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    REQUIRE(ready);
    const std::array<ID3D11Texture2D*,1> targets{display.Get()};
    const auto readback=rk::readbackCandidates(context.Get(),targets);
    REQUIRE(std::holds_alternative<std::vector<rk::ProbeImage>>(readback));
    const auto& bytes=std::get<std::vector<rk::ProbeImage>>(readback)[0].pixels;
    REQUIRE(bytes.size()==4*4*4);
    REQUIRE(bytes[0]==255);
    REQUIRE(bytes[1]==0);
    REQUIRE(bytes[3*4+1]==255);
    REQUIRE(bytes[3*4*4+2]==255);
    ID3D11RenderTargetView* stillBound{};
    context->OMGetRenderTargets(1,&stillBound,nullptr);
    ComPtr<ID3D11RenderTargetView> retained;retained.Attach(stillBound);
    REQUIRE(retained.Get()==view.Get());
}
