#include <catch2/catch_test_macros.hpp>
#include "rk/SdrDisplayCopy.hpp"
#include "rk/FrameProbe.hpp"
#include <wrl/client.h>
#include <array>

using Microsoft::WRL::ComPtr;

TEST_CASE("SDR result copies to the bound display target and preserves binding", "[sdr_display_copy]") {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    REQUIRE(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,
        nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)));
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width=4;desc.Height=3;desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;
    desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BindFlags=D3D11_BIND_RENDER_TARGET;
    const std::array<std::uint8_t,48> original{};
    const D3D11_SUBRESOURCE_DATA originalData{original.data(),desc.Width*4,0};
    ComPtr<ID3D11Texture2D> backbuffer;
    REQUIRE(SUCCEEDED(device->CreateTexture2D(&desc,&originalData,&backbuffer)));
    ComPtr<ID3D11RenderTargetView> view;
    REQUIRE(SUCCEEDED(device->CreateRenderTargetView(backbuffer.Get(),nullptr,&view)));
    context->OMSetRenderTargets(1,view.GetAddressOf(),nullptr);
    auto outputDesc=desc;
    outputDesc.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_UNORDERED_ACCESS;
    std::array<std::uint8_t,48> expected{};
    for(std::size_t i=0;i<expected.size();++i)expected[i]=static_cast<std::uint8_t>(i+4);
    const D3D11_SUBRESOURCE_DATA outputData{expected.data(),desc.Width*4,0};
    ComPtr<ID3D11Texture2D> output;
    REQUIRE(SUCCEEDED(device->CreateTexture2D(&outputDesc,&outputData,&output)));
    const auto copied=rk::copySdrDisplayFrame(context.Get(),backbuffer.Get(),output.Get());
    REQUIRE(std::holds_alternative<bool>(copied));
    REQUIRE(std::get<bool>(copied));
    ID3D11RenderTargetView* bound{};
    context->OMGetRenderTargets(1,&bound,nullptr);
    ComPtr<ID3D11RenderTargetView> retained;retained.Attach(bound);
    REQUIRE(retained.Get()==view.Get());
    const std::array<ID3D11Texture2D*,1> back{backbuffer.Get()};
    const auto readback=rk::readbackCandidates(context.Get(),back);
    REQUIRE(std::holds_alternative<std::vector<rk::ProbeImage>>(readback));
    REQUIRE(std::get<std::vector<rk::ProbeImage>>(readback)[0].pixels==
        std::vector<std::uint8_t>(expected.begin(),expected.end()));
    context->OMSetRenderTargets(0,nullptr,nullptr);
    REQUIRE(std::holds_alternative<rk::Error>(
        rk::copySdrDisplayFrame(context.Get(),backbuffer.Get(),output.Get())));
    context->OMSetRenderTargets(1,view.GetAddressOf(),nullptr);
    auto wrongDesc=outputDesc;wrongDesc.Width=5;
    ComPtr<ID3D11Texture2D> wrong;
    REQUIRE(SUCCEEDED(device->CreateTexture2D(&wrongDesc,nullptr,&wrong)));
    REQUIRE(std::holds_alternative<rk::Error>(
        rk::copySdrDisplayFrame(context.Get(),backbuffer.Get(),wrong.Get())));
}
