#include <catch2/catch_test_macros.hpp>
#include "rk/SrInput.hpp"
#include "rk/FrameProbe.hpp"
#include <wrl/client.h>
#include <array>
#include <vector>
#include <utility>
#include <cstring>

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
    auto* originalOutput=owned.output();
    auto retainedOutput=owned.takeOutput();
    REQUIRE(retainedOutput.Get()==originalOutput);
    REQUIRE(owned.output()==nullptr);
    prepared=rk::Error{rk::ErrorCode::Unavailable,"retire source frame"};
    D3D11_TEXTURE2D_DESC retainedDescription{};
    retainedOutput->GetDesc(&retainedDescription);
    REQUIRE(retainedDescription.Width==width);
    REQUIRE(retainedDescription.Height==height);
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

TEST_CASE("Raw depth sample gate distinguishes a menu clear from world geometry", "[sr_input]") {
    constexpr UINT width=100,height=100;
    std::vector<std::uint8_t> bytes(width*height*4,0xff);
    auto menu=rk::sampleWorldDepth(bytes,width,height,width*4);
    REQUIRE(std::holds_alternative<rk::DepthSampleStats>(menu));
    REQUIRE(std::get<rk::DepthSampleStats>(menu).distinct==1);
    REQUIRE_FALSE(std::get<rk::DepthSampleStats>(menu).worldLike());
    for(UINT y=0;y<10;++y)for(UINT x=0;x<10;++x) {
        const auto sx=(2*x+1)*width/20,sy=(2*y+1)*height/20;
        const std::uint32_t depth=1000+y*10+x;
        std::memcpy(bytes.data()+(sy*width+sx)*4,&depth,4);
    }
    auto world=rk::sampleWorldDepth(bytes,width,height,width*4);
    REQUIRE(std::holds_alternative<rk::DepthSampleStats>(world));
    const auto stats=std::get<rk::DepthSampleStats>(world);
    REQUIRE(stats.distinct==100);
    REQUIRE(stats.nonFar==100);
    REQUIRE(stats.worldLike());
    REQUIRE(std::holds_alternative<rk::Error>(rk::sampleWorldDepth(bytes,width,height,width*4-1)));
}

TEST_CASE("SDR scene preparation accepts an RTV-only backbuffer and preserves its pixels", "[sr_input]") {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    REQUIRE(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,
        D3D11_SDK_VERSION,&device,nullptr,&context)));
    constexpr UINT width=8,height=6;
    const std::array formats{DXGI_FORMAT_R8G8B8A8_UNORM,DXGI_FORMAT_R16G16_FLOAT,
        DXGI_FORMAT_R24G8_TYPELESS};
    const std::array<UINT,3> binds{D3D11_BIND_RENDER_TARGET,
        D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE,
        D3D11_BIND_DEPTH_STENCIL|D3D11_BIND_SHADER_RESOURCE};
    std::array<ComPtr<ID3D11Texture2D>,3> sources;
    std::array<std::vector<std::uint8_t>,3> bytes;
    for(std::size_t i=0;i<sources.size();++i) {
        bytes[i].resize(width*height*4);
        for(std::size_t p=0;p<bytes[i].size();++p)
            bytes[i][p]=static_cast<std::uint8_t>((p+i*19)%251);
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width=width;desc.Height=height;desc.MipLevels=desc.ArraySize=1;
        desc.SampleDesc.Count=1;desc.Format=formats[i];desc.BindFlags=binds[i];
        const D3D11_SUBRESOURCE_DATA initial{bytes[i].data(),width*4,0};
        REQUIRE(SUCCEEDED(device->CreateTexture2D(&desc,&initial,&sources[i])));
    }
    const std::array<ID3D11Texture2D*,3> raw{
        sources[0].Get(),sources[1].Get(),sources[2].Get()};
    auto prepared=rk::prepareSdrSrInputs(context.Get(),raw);
    REQUIRE(std::holds_alternative<rk::PreparedSrInputs>(prepared));
    auto& owned=std::get<rk::PreparedSrInputs>(prepared);
    D3D11_TEXTURE2D_DESC colorDesc{},outputDesc{};
    owned.color()->GetDesc(&colorDesc);owned.output()->GetDesc(&outputDesc);
    REQUIRE(colorDesc.Format==DXGI_FORMAT_R8G8B8A8_UNORM);
    REQUIRE((colorDesc.BindFlags&D3D11_BIND_SHADER_RESOURCE)!=0);
    REQUIRE(outputDesc.Format==DXGI_FORMAT_R8G8B8A8_UNORM);
    REQUIRE((outputDesc.BindFlags&D3D11_BIND_UNORDERED_ACCESS)!=0);
    const std::array<ID3D11Texture2D*,3> copied{
        owned.color(),owned.motion(),owned.depth()};
    const auto readback=rk::readbackCandidates(context.Get(),copied);
    REQUIRE(std::holds_alternative<std::vector<rk::ProbeImage>>(readback));
    const auto& images=std::get<std::vector<rk::ProbeImage>>(readback);
    for(std::size_t i=0;i<3;++i)REQUIRE(images[i].pixels==bytes[i]);
    auto invalid=raw;
    invalid[0]=sources[1].Get();
    REQUIRE(std::holds_alternative<rk::Error>(rk::prepareSdrSrInputs(context.Get(),invalid)));
}
