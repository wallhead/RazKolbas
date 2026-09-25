#include <catch2/catch_test_macros.hpp>
#include "rk/FrameProbe.hpp"
#include "rk/PatchDescriptor.hpp"
#include <wrl/client.h>
#include <cstring>
#include <filesystem>
#include <fstream>
using Microsoft::WRL::ComPtr;
TEST_CASE("Prepared input capture writes verified raw bytes and refuses a repeated destination", "[frame_probe]") {
    namespace fs=std::filesystem;
    rk::ProbeImage image;
    image.descriptor.Width=2;
    image.descriptor.Height=1;
    image.descriptor.Format=DXGI_FORMAT_R32_FLOAT;
    image.rowBytes=8;
    image.pixels={1,2,3,4,5,6,7,8};
    const auto directory=fs::temp_directory_path()/
        ("rk-probe-bundle-"+std::to_string(GetCurrentProcessId())+"-"+
        std::to_string(GetTickCount64()));
    const std::array images{image};
    const std::array<std::string_view,1> names{"depth.raw"};
    const auto saved=rk::saveProbeBundle(directory,images,names);
    REQUIRE(std::holds_alternative<bool>(saved));
    REQUIRE(std::get<bool>(saved));
    std::ifstream raw(directory/"depth.raw",std::ios::binary);
    const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>{raw},
        std::istreambuf_iterator<char>{}};
    raw.close();
    REQUIRE(bytes==image.pixels);
    std::ifstream manifest(directory/"manifest.txt");
    const std::string contents{std::istreambuf_iterator<char>{manifest},
        std::istreambuf_iterator<char>{}};
    manifest.close();
    REQUIRE(contents.find(rk::sha256(image.pixels))!=std::string::npos);
    REQUIRE(contents.find("complete=true")!=std::string::npos);
    REQUIRE(std::holds_alternative<rk::Error>(rk::saveProbeBundle(directory,images,names)));
    REQUIRE(fs::remove(directory/"depth.raw"));
    REQUIRE(fs::remove(directory/"manifest.txt"));
    REQUIRE(fs::remove(directory));
}
TEST_CASE("Frame probe is limited to the verified ENB base Present boundary", "[frame_probe]") {
    REQUIRE(rk::frameProbeBoundary("enb20260508.swapchain-observe-v1",rk::SwapCall::Present));
    REQUIRE(rk::frameProbeBoundary("enb0505.swapchain-observe-v1",rk::SwapCall::Present));
    REQUIRE_FALSE(rk::frameProbeBoundary("reshade673.swapchain-observe-v1",rk::SwapCall::Present));
    REQUIRE_FALSE(rk::frameProbeBoundary("enb20260508.swapchain-observe-v1",rk::SwapCall::Present1));
    REQUIRE_FALSE(rk::frameProbeBoundary("unknown",rk::SwapCall::Present));
}
TEST_CASE("Renderer candidate pointers require owned lock and matching renderer identities", "[frame_probe]") {
    std::vector<std::uint8_t> bytes(rk::renderer1170Size);
    const auto put=[&](std::size_t offset,auto value){std::memcpy(bytes.data()+offset,&value,sizeof(value));};
    put(0x48,std::uintptr_t{1});put(0x50,std::uintptr_t{2});put(0x70,std::uintptr_t{3});
    put(0x27f0+12,LONG{1});put(0x27f0+16,std::uintptr_t{45});
    put(0xa58+0x30,std::uintptr_t{10});put(0xa58+7*0x30,std::uintptr_t{11});put(0x2018,std::uintptr_t{12});
    const auto result=rk::rendererCandidatePointers(bytes,45,1,2,3);
    REQUIRE(std::holds_alternative<std::array<std::uintptr_t,3>>(result));
    REQUIRE((std::get<std::array<std::uintptr_t,3>>(result)==std::array<std::uintptr_t,3>{10,11,12}));
    REQUIRE(std::holds_alternative<rk::Error>(rk::rendererCandidatePointers(bytes,46,1,2,3)));
    REQUIRE(std::holds_alternative<rk::Error>(rk::rendererCandidatePointers(bytes,45,1,2,4)));
    REQUIRE(std::holds_alternative<rk::Error>(rk::rendererCandidatePointers(bytes,45,2,2,3)));
    put(0x27f0+12,LONG{0});REQUIRE(std::holds_alternative<rk::Error>(rk::rendererCandidatePointers(bytes,45,1,2,3)));
    bytes.resize(32);REQUIRE(std::holds_alternative<rk::Error>(rk::rendererCandidatePointers(bytes,45,1,2,3)));
}
TEST_CASE("Candidate capture packs odd-sized GPU textures without changing pixels or bindings", "[frame_probe]") {
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    REQUIRE(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)));
    D3D11_TEXTURE2D_DESC desc{};desc.Width=17;desc.Height=13;desc.MipLevels=1;desc.ArraySize=1;
    desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_DEFAULT;desc.BindFlags=D3D11_BIND_RENDER_TARGET;
    std::vector<std::uint8_t> known(17*13*4);for(std::size_t i=0;i<known.size();++i)known[i]=static_cast<std::uint8_t>(i%251);
    D3D11_SUBRESOURCE_DATA init{known.data(),17*4,0};ComPtr<ID3D11Texture2D> texture;
    REQUIRE(SUCCEEDED(device->CreateTexture2D(&desc,&init,&texture)));
    ComPtr<ID3D11RenderTargetView> rtv;REQUIRE(SUCCEEDED(device->CreateRenderTargetView(texture.Get(),nullptr,&rtv)));
    ID3D11RenderTargetView* bound=rtv.Get();context->OMSetRenderTargets(1,&bound,nullptr);
    std::array<ID3D11Texture2D*,1> inputs{texture.Get()};
    const auto captured=rk::readbackCandidates(context.Get(),inputs);
    REQUIRE(std::holds_alternative<std::vector<rk::ProbeImage>>(captured));
    const auto& images=std::get<std::vector<rk::ProbeImage>>(captured);
    REQUIRE(images.size()==1);REQUIRE(images[0].rowBytes==68);REQUIRE(images[0].pixels==known);
    REQUIRE(std::get<std::vector<rk::ProbeImage>>(rk::readbackCandidates(context.Get(),inputs))[0].pixels==known);
    ComPtr<ID3D11RenderTargetView> after;context->OMGetRenderTargets(1,&after,nullptr);REQUIRE(after.Get()==bound);
    REQUIRE(std::holds_alternative<rk::Error>(rk::readbackCandidates(context.Get(),inputs,known.size()-1)));
    ComPtr<ID3D11Device> other;REQUIRE(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&other,nullptr,nullptr)));
    ComPtr<ID3D11Texture2D> foreign;REQUIRE(SUCCEEDED(other->CreateTexture2D(&desc,&init,&foreign)));
    inputs[0]=foreign.Get();REQUIRE(std::holds_alternative<rk::Error>(rk::readbackCandidates(context.Get(),inputs)));
    inputs[0]=nullptr;REQUIRE(std::holds_alternative<rk::Error>(rk::readbackCandidates(context.Get(),inputs)));
    context->ClearState();
}
TEST_CASE("Frame probe captures an exact bounded texture region", "[frame_probe]") {
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    REQUIRE(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,
        nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)));
    D3D11_TEXTURE2D_DESC desc{};desc.Width=4;desc.Height=3;
    desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;
    desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    std::array<std::uint32_t,12> pixels{};
    for(std::uint32_t i=0;i<pixels.size();++i)pixels[i]=0xff000000u+i;
    const D3D11_SUBRESOURCE_DATA initial{pixels.data(),4*4,0};
    ComPtr<ID3D11Texture2D> texture;
    REQUIRE(SUCCEEDED(device->CreateTexture2D(&desc,&initial,&texture)));
    const auto captured=rk::readbackRegion(context.Get(),texture.Get(),1,1,2,2,16);
    REQUIRE(std::holds_alternative<rk::ProbeImage>(captured));
    const auto& image=std::get<rk::ProbeImage>(captured);
    REQUIRE(image.descriptor.Width==2);
    REQUIRE(image.descriptor.Height==2);
    REQUIRE(image.rowBytes==8);
    std::array<std::uint32_t,4> actual{};
    std::memcpy(actual.data(),image.pixels.data(),image.pixels.size());
    REQUIRE((actual==std::array<std::uint32_t,4>{pixels[5],pixels[6],pixels[9],pixels[10]}));
    REQUIRE(std::holds_alternative<rk::Error>(
        rk::readbackRegion(context.Get(),texture.Get(),3,2,2,2,16)));
}
TEST_CASE("Candidate capture preserves raw motion and typeless depth bytes", "[frame_probe]") {
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    REQUIRE(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)));
    for(const auto format:{DXGI_FORMAT_R16G16_FLOAT,DXGI_FORMAT_R24G8_TYPELESS,DXGI_FORMAT_R32G8X24_TYPELESS}) {
        D3D11_TEXTURE2D_DESC d{};d.Width=17;d.Height=13;d.MipLevels=1;d.ArraySize=1;
        d.Format=format;d.SampleDesc.Count=1;d.Usage=D3D11_USAGE_DEFAULT;
        d.BindFlags=format==DXGI_FORMAT_R16G16_FLOAT?D3D11_BIND_RENDER_TARGET:D3D11_BIND_DEPTH_STENCIL;
        const UINT row=17*(format==DXGI_FORMAT_R32G8X24_TYPELESS?8U:4U);
        std::vector<std::uint8_t> bytes(row*13);for(std::size_t i=0;i<bytes.size();++i)bytes[i]=static_cast<std::uint8_t>(i%251);
        D3D11_SUBRESOURCE_DATA init{bytes.data(),row,0};ComPtr<ID3D11Texture2D> texture;
        REQUIRE(SUCCEEDED(device->CreateTexture2D(&d,&init,&texture)));
        std::array<ID3D11Texture2D*,1> input{texture.Get()};
        const auto captured=rk::readbackCandidates(context.Get(),input);
        REQUIRE(std::holds_alternative<std::vector<rk::ProbeImage>>(captured));
        const auto& image=std::get<std::vector<rk::ProbeImage>>(captured)[0];
        REQUIRE(image.rowBytes==row);REQUIRE(image.pixels==bytes);
    }
}
