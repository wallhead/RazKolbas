#include <catch2/catch_test_macros.hpp>
#include "rk/StagePairCapture.hpp"
#include <wrl/client.h>
#include <array>
#include <fstream>
#include <iterator>
#include <string>

using Microsoft::WRL::ComPtr;

TEST_CASE("Stage pair preserves same-frame HDR and backbuffer before later callbacks", "[stage_pair]") {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    REQUIRE(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,
        nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)));
    D3D11_TEXTURE2D_DESC sceneDesc{};
    sceneDesc.Width=3;sceneDesc.Height=2;
    sceneDesc.MipLevels=sceneDesc.ArraySize=sceneDesc.SampleDesc.Count=1;
    sceneDesc.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;
    sceneDesc.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;
    const std::array<std::uint16_t,24> hdr{
        0x3c00,0,0,0x3c00,0x4000,0,0,0x3c00,0x4400,0,0,0x3c00,
        0,0x3c00,0,0x3c00,0,0x4000,0,0x3c00,0,0x4400,0,0x3c00};
    D3D11_SUBRESOURCE_DATA sourceData{hdr.data(),sceneDesc.Width*8,0};
    ComPtr<ID3D11Texture2D> scene;
    REQUIRE(SUCCEEDED(device->CreateTexture2D(&sceneDesc,&sourceData,&scene)));
    auto backDesc=sceneDesc;backDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    constexpr std::array<std::uint8_t,24> post{
        10,20,30,255,11,21,31,255,12,22,32,255,
        13,23,33,255,14,24,34,255,15,25,35,255};
    const D3D11_SUBRESOURCE_DATA postData{post.data(),backDesc.Width*4,0};
    ComPtr<ID3D11Texture2D> backbuffer;
    REQUIRE(SUCCEEDED(device->CreateTexture2D(&backDesc,&postData,&backbuffer)));
    auto captured=rk::StagePairCapture::capturePostWorld(context.Get(),scene.Get(),backbuffer.Get());
    const auto reason=std::holds_alternative<rk::Error>(captured)?
        std::get<rk::Error>(captured).message:"success";
    INFO(reason);
    REQUIRE(std::holds_alternative<rk::StagePairCapture>(captured));
    auto& pair=std::get<rk::StagePairCapture>(captured);
    REQUIRE(pair.postWorld().pixels==std::vector<std::uint8_t>(post.begin(),post.end()));
    constexpr std::array<std::uint8_t,24> before{
        40,50,60,255,41,51,61,255,42,52,62,255,
        43,53,63,255,44,54,64,255,45,55,65,255};
    context->UpdateSubresource(backbuffer.Get(),0,nullptr,before.data(),backDesc.Width*4,0);
    const auto done=pair.captureBeforePresent(context.Get(),backbuffer.Get());
    REQUIRE(std::holds_alternative<bool>(done));
    REQUIRE(std::get<bool>(done));
    REQUIRE(pair.complete());
    REQUIRE(pair.beforePresent().pixels==std::vector<std::uint8_t>(before.begin(),before.end()));
    REQUIRE(pair.scene().pixels.size()==hdr.size()*sizeof(std::uint16_t));
    const auto directory=std::filesystem::temp_directory_path()/
        ("RazKolbasStagePairTest-"+std::to_string(GetCurrentProcessId()));
    const auto saved=pair.save(directory);
    REQUIRE(std::holds_alternative<bool>(saved));
    REQUIRE(std::get<bool>(saved));
    const auto manifest=directory/"manifest.txt";
    std::ifstream file(manifest);
    const std::string text{std::istreambuf_iterator<char>{file},std::istreambuf_iterator<char>{}};
    REQUIRE(text.find("complete=true")!=std::string::npos);
    file.close();
    REQUIRE(std::filesystem::file_size(directory/"scene-hdr.raw")==hdr.size()*2);
    REQUIRE(std::filesystem::file_size(directory/"post-world-backbuffer.raw")==post.size());
    REQUIRE(std::filesystem::file_size(directory/"before-present-backbuffer.raw")==before.size());
    std::filesystem::remove(directory/"scene-hdr.raw");
    std::filesystem::remove(directory/"post-world-backbuffer.raw");
    std::filesystem::remove(directory/"before-present-backbuffer.raw");
    std::filesystem::remove(manifest);
    std::filesystem::remove(directory);
}
