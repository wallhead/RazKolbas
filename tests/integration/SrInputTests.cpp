#include <catch2/catch_test_macros.hpp>
#include "rk/SrInput.hpp"
#include "rk/FrameProbe.hpp"
#include <wrl/client.h>
#include <array>
#include <vector>
#include <utility>

using Microsoft::WRL::ComPtr;

TEST_CASE("Owned D3D11 SR inputs copy Skyrim colour, motion and native typeless depth", "[sr_input]") {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    REQUIRE(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,
        D3D11_SDK_VERSION,&device,nullptr,&context)));
    constexpr UINT width=17,height=13;
    const std::array formats{DXGI_FORMAT_R16G16B16A16_FLOAT,DXGI_FORMAT_R16G16_FLOAT,DXGI_FORMAT_R24G8_TYPELESS};
    const std::array sizes{8U,4U,4U};
    std::array<ComPtr<ID3D11Texture2D>,3> sources;
    std::array<std::vector<std::uint8_t>,3> pixels;
    for(std::size_t i=0;i<sources.size();++i) {
        pixels[i].resize(width*height*sizes[i]);
        for(std::size_t j=0;j<pixels[i].size();++j)pixels[i][j]=static_cast<std::uint8_t>((j+i*37)%251);
        D3D11_TEXTURE2D_DESC d{};d.Width=width;d.Height=height;d.MipLevels=d.ArraySize=1;
        d.SampleDesc.Count=1;d.Format=formats[i];
        d.BindFlags=D3D11_BIND_SHADER_RESOURCE|(i==2?D3D11_BIND_DEPTH_STENCIL:D3D11_BIND_RENDER_TARGET);
        D3D11_SUBRESOURCE_DATA initial{pixels[i].data(),width*sizes[i],0};
        REQUIRE(SUCCEEDED(device->CreateTexture2D(&d,&initial,&sources[i])));
    }
    std::array<ID3D11Texture2D*,3> raw{sources[0].Get(),sources[1].Get(),sources[2].Get()};
    auto prepared=rk::prepareSrInputs(context.Get(),raw);
    REQUIRE(std::holds_alternative<rk::PreparedSrInputs>(prepared));
    auto& owned=std::get<rk::PreparedSrInputs>(prepared);
    REQUIRE(owned.width()==width);
    REQUIRE(owned.height()==height);
    REQUIRE(owned.color()!=raw[0]);
    REQUIRE(owned.motion()!=raw[1]);
    REQUIRE(owned.depth()!=raw[2]);
    D3D11_TEXTURE2D_DESC output{};owned.output()->GetDesc(&output);
    REQUIRE(output.Format==DXGI_FORMAT_R16G16B16A16_FLOAT);
    REQUIRE((output.BindFlags&D3D11_BIND_UNORDERED_ACCESS)!=0);
    const std::array<ID3D11Texture2D*,3> copies{owned.color(),owned.motion(),owned.depth()};
    const auto readback=rk::readbackCandidates(context.Get(),copies);
    REQUIRE(std::holds_alternative<std::vector<rk::ProbeImage>>(readback));
    const auto& result=std::get<std::vector<rk::ProbeImage>>(readback);
    for(std::size_t i=0;i<3;++i)REQUIRE(result[i].pixels==pixels[i]);
    raw[1]=nullptr;
    REQUIRE(std::holds_alternative<rk::Error>(rk::prepareSrInputs(context.Get(),raw)));
    raw[1]=sources[1].Get();
    REQUIRE(std::holds_alternative<rk::Error>(rk::prepareSrInputs(nullptr,raw)));
    std::swap(raw[0],raw[1]);
    REQUIRE(std::holds_alternative<rk::Error>(rk::prepareSrInputs(context.Get(),raw)));
    std::swap(raw[0],raw[1]);
    ComPtr<ID3D11Device> otherDevice;
    REQUIRE(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,
        D3D11_SDK_VERSION,&otherDevice,nullptr,nullptr)));
    D3D11_TEXTURE2D_DESC foreignDesc{};sources[0]->GetDesc(&foreignDesc);
    ComPtr<ID3D11Texture2D> foreign;
    REQUIRE(SUCCEEDED(otherDevice->CreateTexture2D(&foreignDesc,nullptr,&foreign)));
    raw[0]=foreign.Get();
    REQUIRE(std::holds_alternative<rk::Error>(rk::prepareSrInputs(context.Get(),raw)));
}
