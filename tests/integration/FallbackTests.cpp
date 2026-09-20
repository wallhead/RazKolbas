#include <catch2/catch_test_macros.hpp>
#include "rk/SpatialFallback.hpp"
#include <array>
#include <chrono>
#include <thread>
#include <vector>

using Microsoft::WRL::ComPtr;

TEST_CASE("Spatial fallback retains a scene frame and produces display-sized HDR", "[fallback]") {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    REQUIRE(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,
        nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)));
    D3D11_TEXTURE2D_DESC description{};
    description.Width=description.Height=2;
    description.MipLevels=description.ArraySize=description.SampleDesc.Count=1;
    description.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;
    description.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    constexpr std::array<std::uint16_t,16> pixels{
        0x3c00,0,0,0x3c00, 0,0x3c00,0,0x3c00,
        0,0,0x3c00,0x3c00, 0x3c00,0x3c00,0x3c00,0x3c00};
    const D3D11_SUBRESOURCE_DATA initial{pixels.data(),16,0};
    ComPtr<ID3D11Texture2D> source;
    REQUIRE(SUCCEEDED(device->CreateTexture2D(&description,&initial,&source)));
    auto bindingDescription=description;
    bindingDescription.Width=8;bindingDescription.Height=8;
    bindingDescription.BindFlags=D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> bindingTexture;
    ComPtr<ID3D11RenderTargetView> priorTarget;
    REQUIRE(SUCCEEDED(device->CreateTexture2D(&bindingDescription,nullptr,&bindingTexture)));
    REQUIRE(SUCCEEDED(device->CreateRenderTargetView(bindingTexture.Get(),nullptr,&priorTarget)));
    context->OMSetRenderTargets(1,priorTarget.GetAddressOf(),nullptr);
    const D3D11_VIEWPORT priorViewport{2,1,6,7,0,1};
    context->RSSetViewports(1,&priorViewport);
    auto produced=rk::produceSpatialFallback(context.Get(),source.Get(),4,4);
    const auto reason=std::holds_alternative<rk::Error>(produced)?
        std::get<rk::Error>(produced).message:"success";
    INFO(reason);
    REQUIRE(std::holds_alternative<rk::SpatialFallbackFrame>(produced));
    auto& frame=std::get<rk::SpatialFallbackFrame>(produced);
    REQUIRE(frame.output()!=source.Get());
    REQUIRE(frame.width()==4);
    REQUIRE(frame.height()==4);
    D3D11_TEXTURE2D_DESC output{};frame.output()->GetDesc(&output);
    REQUIRE(output.Width==4);
    REQUIRE(output.Height==4);
    REQUIRE(output.Format==DXGI_FORMAT_R16G16B16A16_FLOAT);
    ComPtr<ID3D11RenderTargetView> restoredTarget;
    context->OMGetRenderTargets(1,&restoredTarget,nullptr);
    REQUIRE(restoredTarget.Get()==priorTarget.Get());
    UINT viewportCount=1;D3D11_VIEWPORT restoredViewport{};
    context->RSGetViewports(&viewportCount,&restoredViewport);
    REQUIRE(viewportCount==1);
    REQUIRE(restoredViewport.TopLeftX==priorViewport.TopLeftX);
    REQUIRE(restoredViewport.Width==priorViewport.Width);
    source.Reset(); // The submitted frame must retain its input until completion.
    context->Flush();
    bool ready=false;
    for(unsigned attempt=0;attempt<500&&!ready;++attempt) {
        auto status=frame.complete(context.Get());
        REQUIRE(std::holds_alternative<bool>(status));
        ready=std::get<bool>(status);
        if(!ready)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    REQUIRE(ready);
    auto staging=output;
    staging.Usage=D3D11_USAGE_STAGING;
    staging.BindFlags=0;
    staging.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> readback;
    REQUIRE(SUCCEEDED(device->CreateTexture2D(&staging,nullptr,&readback)));
    context->CopyResource(readback.Get(),frame.output());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    REQUIRE(SUCCEEDED(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&mapped)));
    auto channel=[&](unsigned x,unsigned y,unsigned c) {
        return *reinterpret_cast<const std::uint16_t*>(
            static_cast<const std::uint8_t*>(mapped.pData)+y*mapped.RowPitch+x*8+c*2);
    };
    REQUIRE(channel(0,0,0)==0x3c00);
    REQUIRE(channel(0,0,1)==0);
    REQUIRE(channel(3,0,1)==0x3c00);
    REQUIRE(channel(0,3,2)==0x3c00);
    REQUIRE(channel(3,3,0)==0x3c00);
    REQUIRE(channel(1,1,0)==0x3900);
    REQUIRE(channel(1,1,1)==0x3400);
    REQUIRE(channel(1,1,2)==0x3400);
    context->Unmap(readback.Get(),0);
    REQUIRE(std::holds_alternative<rk::Error>(rk::produceSpatialFallback(context.Get(),nullptr,4,4)));
    REQUIRE(std::holds_alternative<rk::Error>(rk::produceSpatialFallback(context.Get(),frame.output(),0,4)));
    ComPtr<ID3D11Device> anotherDevice;
    REQUIRE(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,
        nullptr,0,D3D11_SDK_VERSION,&anotherDevice,nullptr,nullptr)));
    ComPtr<ID3D11Texture2D> foreign;
    REQUIRE(SUCCEEDED(anotherDevice->CreateTexture2D(&description,&initial,&foreign)));
    REQUIRE(std::holds_alternative<rk::Error>(
        rk::produceSpatialFallback(context.Get(),foreign.Get(),4,4)));
}

TEST_CASE("SDR fallback produces display-sized colour from a reduced frame", "[fallback]") {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    REQUIRE(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,
        nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)));
    D3D11_TEXTURE2D_DESC sourceDesc{};
    sourceDesc.Width=sourceDesc.Height=2;
    sourceDesc.MipLevels=sourceDesc.ArraySize=sourceDesc.SampleDesc.Count=1;
    sourceDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    sourceDesc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    constexpr std::array<std::uint8_t,16> pixels{
        255,0,0,255, 0,255,0,255,
        0,0,255,255, 255,255,255,255};
    const D3D11_SUBRESOURCE_DATA initial{pixels.data(),8,0};
    ComPtr<ID3D11Texture2D> source;
    REQUIRE(SUCCEEDED(device->CreateTexture2D(&sourceDesc,&initial,&source)));
    auto produced=rk::produceSdrSpatialFallback(context.Get(),source.Get(),4,4);
    REQUIRE(std::holds_alternative<rk::SpatialFallbackFrame>(produced));
    auto& frame=std::get<rk::SpatialFallbackFrame>(produced);
    REQUIRE(frame.width()==4);
    REQUIRE(frame.height()==4);
    D3D11_TEXTURE2D_DESC output{};frame.output()->GetDesc(&output);
    REQUIRE(output.Format==DXGI_FORMAT_R8G8B8A8_UNORM);
    REQUIRE(output.Width==4);
    REQUIRE(output.Height==4);
    source.Reset();
    context->Flush();
    bool ready=false;
    for(unsigned attempt=0;attempt<500&&!ready;++attempt) {
        const auto completed=frame.complete(context.Get());
        REQUIRE(std::holds_alternative<bool>(completed));
        ready=std::get<bool>(completed);
        if(!ready)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    REQUIRE(ready);
    auto staging=output;
    staging.Usage=D3D11_USAGE_STAGING;
    staging.BindFlags=0;
    staging.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> readback;
    REQUIRE(SUCCEEDED(device->CreateTexture2D(&staging,nullptr,&readback)));
    context->CopyResource(readback.Get(),frame.output());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    REQUIRE(SUCCEEDED(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&mapped)));
    const auto* top=static_cast<const std::uint8_t*>(mapped.pData);
    const auto* bottom=top+3*mapped.RowPitch;
    REQUIRE(top[0]==255);
    REQUIRE(top[1]==0);
    REQUIRE(top[3*4+1]==255);
    REQUIRE(bottom[2]==255);
    REQUIRE(bottom[3*4]==255);
    REQUIRE(top[4+0]>0);
    REQUIRE(top[4+1]>0);
    context->Unmap(readback.Get(),0);
    REQUIRE(std::holds_alternative<rk::Error>(
        rk::produceSdrSpatialFallback(context.Get(),frame.output(),0,4)));
}
