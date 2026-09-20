#include <catch2/catch_test_macros.hpp>
#include "rk/ReducedSdrSurface.hpp"
#include "rk/SdrSrPresentation.hpp"
#include "rk/SrInput.hpp"
#include "rk/FrameProbe.hpp"
#include <wrl/client.h>
#include <array>
#include <chrono>
#include <thread>
#include <utility>
#include <vector>

using Microsoft::WRL::ComPtr;

TEST_CASE("Owned reduced SDR scene reaches a display-sized fallback", "[reduced_sdr_surface]") {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    REQUIRE(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,
        nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)));
    auto created=rk::createReducedSdrSurface(device.Get(),{8,8},{4,4});
    REQUIRE(std::holds_alternative<rk::ReducedSdrSurface>(created));
    auto scene=std::move(std::get<rk::ReducedSdrSurface>(created));
    REQUIRE(scene.texture()!=nullptr);
    REQUIRE(scene.renderTarget()!=nullptr);
    ComPtr<ID3D11Texture2D> queried;
    REQUIRE(SUCCEEDED(scene.queryBuffer(IID_PPV_ARGS(&queried))));
    REQUIRE(queried.Get()==scene.texture());
    queried.Reset();
    void* invalid=reinterpret_cast<void*>(1);
    REQUIRE(scene.queryBuffer(__uuidof(IDXGISwapChain),&invalid)==E_NOINTERFACE);
    REQUIRE(invalid==nullptr);
    D3D11_TEXTURE2D_DESC desc{};
    scene.texture()->GetDesc(&desc);
    REQUIRE(desc.Width==4);
    REQUIRE(desc.Height==4);
    REQUIRE(desc.Format==DXGI_FORMAT_R8G8B8A8_UNORM);
    REQUIRE((desc.BindFlags&(D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE))==
        (D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE));
    constexpr float red[4]{1,0,0,1};
    context->ClearRenderTargetView(scene.renderTarget(),red);

    auto displayDesc=desc;
    displayDesc.Width=displayDesc.Height=8;
    displayDesc.BindFlags=D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> display;
    REQUIRE(SUCCEEDED(device->CreateTexture2D(&displayDesc,nullptr,&display)));
    ComPtr<ID3D11RenderTargetView> displayView;
    REQUIRE(SUCCEEDED(device->CreateRenderTargetView(display.Get(),nullptr,&displayView)));
    context->OMSetRenderTargets(1,displayView.GetAddressOf(),nullptr);

    D3D11_TEXTURE2D_DESC motionDesc=desc;
    motionDesc.Format=DXGI_FORMAT_R16G16_FLOAT;
    D3D11_TEXTURE2D_DESC depthDesc=desc;
    depthDesc.Format=DXGI_FORMAT_R24G8_TYPELESS;
    depthDesc.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_DEPTH_STENCIL;
    ComPtr<ID3D11Texture2D> motion,depth;
    REQUIRE(SUCCEEDED(device->CreateTexture2D(&motionDesc,nullptr,&motion)));
    REQUIRE(SUCCEEDED(device->CreateTexture2D(&depthDesc,nullptr,&depth)));
    const std::array<ID3D11Texture2D*,3> inputs{scene.texture(),motion.Get(),depth.Get()};
    auto prepared=rk::prepareSdrSrInputsForDisplay(context.Get(),inputs,8,8);
    REQUIRE(std::holds_alternative<rk::PreparedSrInputs>(prepared));
    auto& copied=std::get<rk::PreparedSrInputs>(prepared);
    REQUIRE(copied.width()==4);
    REQUIRE(copied.outputWidth()==8);

    auto result=rk::presentSdrSrFrame(context.Get(),copied.color(),display.Get(),
        []()->rk::Result<bool> {return rk::Error{rk::ErrorCode::Unavailable,"forced SR failure"};});
    REQUIRE(std::holds_alternative<rk::SdrSrFrameResult>(result));
    auto frame=std::move(std::get<rk::SdrSrFrameResult>(result));
    REQUIRE(frame.mode()==rk::SdrSrFrameMode::SpatialFallback);
    prepared=rk::Error{rk::ErrorCode::Unavailable,"retire copied input"};
    created=rk::Error{rk::ErrorCode::Unavailable,"retire surface"};
    scene={};
    context->Flush();
    bool complete=false;
    for(unsigned i=0;i<500&&!complete;++i) {
        auto checked=frame.complete(context.Get());
        REQUIRE(std::holds_alternative<bool>(checked));
        complete=std::get<bool>(checked);
        if(!complete)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    REQUIRE(complete);
    const std::array<ID3D11Texture2D*,1> targets{display.Get()};
    auto captured=rk::readbackCandidates(context.Get(),targets);
    REQUIRE(std::holds_alternative<std::vector<rk::ProbeImage>>(captured));
    const auto& pixels=std::get<std::vector<rk::ProbeImage>>(captured)[0].pixels;
    REQUIRE(pixels.size()==8*8*4);
    for(std::size_t i=0;i<pixels.size();i+=4) {
        REQUIRE(pixels[i]==255);
        REQUIRE(pixels[i+1]==0);
        REQUIRE(pixels[i+2]==0);
        REQUIRE(pixels[i+3]==255);
    }
}

TEST_CASE("Reduced SDR surface rejects invalid geometry and can be recreated", "[reduced_sdr_surface]") {
    ComPtr<ID3D11Device> device;
    REQUIRE(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,
        nullptr,0,D3D11_SDK_VERSION,&device,nullptr,nullptr)));
    REQUIRE(std::holds_alternative<rk::Error>(rk::createReducedSdrSurface(nullptr,{8,8},{4,4})));
    REQUIRE(std::holds_alternative<rk::Error>(rk::createReducedSdrSurface(device.Get(),{8,8},{0,4})));
    REQUIRE(std::holds_alternative<rk::Error>(rk::createReducedSdrSurface(device.Get(),{8,8},{9,4})));
    REQUIRE(std::holds_alternative<rk::Error>(rk::createReducedSdrSurface(device.Get(),{8,8},{8,8})));
    REQUIRE(std::holds_alternative<rk::Error>(rk::createReducedSdrSurface(device.Get(),{9000,8},{4,4})));
    auto old=rk::createReducedSdrSurface(device.Get(),{8,8},{4,4});
    auto next=rk::createReducedSdrSurface(device.Get(),{10,10},{5,5});
    REQUIRE(std::holds_alternative<rk::ReducedSdrSurface>(old));
    REQUIRE(std::holds_alternative<rk::ReducedSdrSurface>(next));
    REQUIRE(std::get<rk::ReducedSdrSurface>(old).texture()!=
        std::get<rk::ReducedSdrSurface>(next).texture());
    D3D11_TEXTURE2D_DESC oldDesc{},nextDesc{};
    std::get<rk::ReducedSdrSurface>(old).texture()->GetDesc(&oldDesc);
    std::get<rk::ReducedSdrSurface>(next).texture()->GetDesc(&nextDesc);
    REQUIRE(oldDesc.Width==4);
    REQUIRE(nextDesc.Width==5);
}
