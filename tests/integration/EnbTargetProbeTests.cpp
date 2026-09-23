#include <catch2/catch_test_macros.hpp>
#include "rk/EnbTargetProbe.hpp"
#include <wrl/client.h>
#include <array>
#include <cstring>
#include <vector>

TEST_CASE("ENB probe bounds failed candidate attempts as well as successful samples", "[enb_target_probe]") {
    rk::EnbTargetProbeBudget budget;
    for(std::uint64_t window=0;window<64;++window) {
        const auto frame=window*600;
        for(unsigned attempt=0;attempt<8;++attempt)REQUIRE(budget.admit(frame));
        REQUIRE_FALSE(budget.admit(frame));
        REQUIRE_FALSE(budget.admit(frame+599));
    }
    REQUIRE_FALSE(budget.admit(64*600));
}

TEST_CASE("An accepted HDR sample ends its observation window", "[enb_target_probe]") {
    rk::EnbTargetProbeBudget budget;
    REQUIRE(budget.admit(700));
    budget.accepted();
    REQUIRE_FALSE(budget.admit(700));
    REQUIRE_FALSE(budget.admit(699));
    REQUIRE_FALSE(budget.admit(1299));
    REQUIRE(budget.admit(1300));
}

TEST_CASE("ENB target probe rejects another image or changed comparison", "[enb_target_probe]") {
    std::vector<std::uint8_t> image(0xaae000);
    // Independently decoded comparison block from installed ENB 47ff... .
    const std::array<std::uint8_t,27> code{0x3b,0x2d,0x3f,0x23,0x1e,0x00,
        0x75,0x48,0x44,0x3b,0x25,0x3a,0x23,0x1e,0x00,0x75,0x38,
        0x83,0xfb,0x0a,0x74,0x05,0x83,0xfb,0x1a,0x75,0x3c};
    std::memcpy(image.data()+0x691cb,code.data(),code.size());
    constexpr auto hash="47ff220dd26a44520d4cec2d515d89effe87b632c1885c32388c93e8d0ceda58";
    REQUIRE(rk::validateEnbTargetProbeImage(image,hash));
    REQUIRE_FALSE(rk::validateEnbTargetProbeImage(image,"other"));
    REQUIRE_FALSE(rk::validateEnbTargetProbeImage(std::span(image).first(0x1000),hash));
    image[0x691d1]^=1;
    REQUIRE_FALSE(rk::validateEnbTargetProbeImage(image,hash));
}

TEST_CASE("ENB dimensions read fails closed on unreadable memory", "[enb_target_probe]") {
    REQUIRE_FALSE(rk::readEnbTargetProbeExtent(0).has_value());
    // Reserve, without committing, the bounded region: the real RPM must fail.
    auto* inaccessible=VirtualAlloc(nullptr,0x24b518,MEM_RESERVE,PAGE_NOACCESS);
    REQUIRE(inaccessible!=nullptr);
    const auto missing=rk::readEnbTargetProbeExtent(reinterpret_cast<std::uintptr_t>(inaccessible));
    REQUIRE(VirtualFree(inaccessible,0,MEM_RELEASE));
    REQUIRE_FALSE(missing.has_value());
    std::vector<std::uint8_t> image(0x24b518);
    const std::array<std::uint32_t,2> expected{2560,1440};
    std::memcpy(image.data()+0x24b510,expected.data(),sizeof(expected));
    const auto actual=rk::readEnbTargetProbeExtent(reinterpret_cast<std::uintptr_t>(image.data()));
    REQUIRE(actual.has_value());
    REQUIRE(actual->width==2560);
    REQUIRE(actual->height==1440);
}

TEST_CASE("Target probe distinguishes actual extent from ENB metadata without changing bindings", "[enb_target_probe]") {
    using Microsoft::WRL::ComPtr;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    REQUIRE(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,
        nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)));
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width=32;desc.Height=16;desc.MipLevels=1;desc.ArraySize=1;
    desc.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;desc.SampleDesc.Count=1;
    desc.BindFlags=D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> texture;
    REQUIRE(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&texture)));
    ComPtr<ID3D11RenderTargetView> view;
    REQUIRE(SUCCEEDED(device->CreateRenderTargetView(texture.Get(),nullptr,&view)));
    std::array<ID3D11RenderTargetView*,7> targets{};
    targets[0]=view.Get();
    context->OMSetRenderTargets(7,targets.data(),nullptr);
    auto sample=rk::inspectEnbTargets(context.Get(),view.Get());
    REQUIRE(sample.actual.width==32);
    REQUIRE(sample.actual.height==16);
    REQUIRE(sample.format==10);
    REQUIRE_FALSE(sample.metadataValid);
    REQUIRE(sample.boundMask==1);
    // A deliberately different metadata extent must not be conflated with GetDesc.
    std::array<std::uint32_t,12> metadata{2,64,32,0,10,1,0,1,1,0,0,0};
    constexpr GUID guid{0xb272d61a,0xacbe,0x4117,{0x8e,0xde,0xe2,0x4c,0x4e,0xe8,0x87,0x21}};
    REQUIRE(SUCCEEDED(view->SetPrivateData(guid,sizeof(metadata),metadata.data())));
    sample=rk::inspectEnbTargets(context.Get(),view.Get());
    REQUIRE(sample.metadataValid);
    REQUIRE(sample.metadata.width==64);
    REQUIRE(sample.metadata.height==32);
    REQUIRE(sample.actual.width==32);
    REQUIRE(sample.boundMask==1);
    ComPtr<ID3D11RenderTargetView> stillBound;
    context->OMGetRenderTargets(1,&stillBound,nullptr);
    REQUIRE(stillBound.Get()==view.Get());
    // A short private-data payload is unavailable, not a partially valid record.
    REQUIRE(SUCCEEDED(view->SetPrivateData(guid,4,metadata.data())));
    REQUIRE_FALSE(rk::inspectEnbTargets(context.Get(),view.Get()).metadataValid);
    ComPtr<ID3D11Texture2D> auxiliaryA,auxiliaryB;
    ComPtr<ID3D11RenderTargetView> viewA,viewB;
    REQUIRE(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&auxiliaryA)));
    REQUIRE(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&auxiliaryB)));
    REQUIRE(SUCCEEDED(device->CreateRenderTargetView(auxiliaryA.Get(),nullptr,&viewA)));
    REQUIRE(SUCCEEDED(device->CreateRenderTargetView(auxiliaryB.Get(),nullptr,&viewB)));
    targets[5]=viewA.Get();targets[6]=viewB.Get();
    context->OMSetRenderTargets(7,targets.data(),nullptr);
    REQUIRE(rk::inspectEnbTargets(context.Get(),view.Get()).boundMask==0x61);
    context->ClearState();
}
