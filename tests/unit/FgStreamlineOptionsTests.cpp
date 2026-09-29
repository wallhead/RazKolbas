#include <catch2/catch_test_macros.hpp>
#include "rk/FgStreamlineOptions.hpp"
#include <dxgi1_6.h>
#include <variant>

namespace {
using Microsoft::WRL::ComPtr;
struct Warp {
    ComPtr<ID3D12Device> device;
    Warp() {
        ComPtr<IDXGIFactory4> factory;
        ComPtr<IDXGIAdapter> adapter;
        REQUIRE(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))));
        REQUIRE(SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter))));
        REQUIRE(SUCCEEDED(D3D12CreateDevice(adapter.Get(),
            D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device))));
    }
    ComPtr<ID3D12Resource> texture(rk::Extent size,DXGI_FORMAT format) {
        D3D12_RESOURCE_DESC desc{};
        desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width=size.width;desc.Height=size.height;
        desc.DepthOrArraySize=1;desc.MipLevels=1;
        desc.Format=format;desc.SampleDesc.Count=1;
        D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;
        ComPtr<ID3D12Resource> resource;
        REQUIRE(SUCCEEDED(device->CreateCommittedResource(&heap,
            D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_COMMON,
            nullptr,IID_PPV_ARGS(&resource))));
        return resource;
    }
};
rk::FgPreparedSubmission prepared(Warp& gpu) {
    rk::FgPreparedSubmission value{};
    value.source=27;value.generation=3;value.presentToken=1004;
    value.resetEpoch=2;value.render={32,20};value.display={64,40};
    value.swapBufferCount=2;value.physicalOutputIndex=1;
    value.copyTicket={1,2};
    value.resources[0]=gpu.texture(value.display,DXGI_FORMAT_R8G8B8A8_UNORM);
    value.resources[1]=gpu.texture(value.render,DXGI_FORMAT_R32_FLOAT);
    value.resources[2]=gpu.texture(value.render,DXGI_FORMAT_R16G16_FLOAT);
    value.resources[3]=gpu.texture(value.display,DXGI_FORMAT_R8G8B8A8_UNORM);
    value.resources[4]=gpu.texture(value.display,DXGI_FORMAT_R8G8B8A8_UNORM);
    return value;
}
}

TEST_CASE("FG Streamline options use actual render, display, formats and swap count",
    "[fg_streamline_options]") {
    Warp gpu;
    const auto frame=prepared(gpu);
    const auto result=rk::makeFgStreamlineOptionsOn(frame,
        DXGI_FORMAT_R8G8B8A8_UNORM);
    REQUIRE(std::holds_alternative<sl::DLSSGOptions>(result));
    const auto& options=std::get<sl::DLSSGOptions>(result);
    REQUIRE(options.mode==sl::DLSSGMode::eOn);
    REQUIRE(options.numFramesToGenerate==1);
    REQUIRE(options.numBackBuffers==2);
    REQUIRE(options.mvecDepthWidth==32);
    REQUIRE(options.mvecDepthHeight==20);
    REQUIRE(options.colorWidth==64);
    REQUIRE(options.colorHeight==40);
    REQUIRE(options.depthBufferFormat==DXGI_FORMAT_R32_FLOAT);
    REQUIRE(options.mvecBufferFormat==DXGI_FORMAT_R16G16_FLOAT);
    REQUIRE(options.colorBufferFormat==DXGI_FORMAT_R8G8B8A8_UNORM);
    REQUIRE(options.enableUserInterfaceRecomposition==sl::Boolean::eTrue);
}

TEST_CASE("FG Streamline options reject lower-format and guide-size drift",
    "[fg_streamline_options]") {
    Warp gpu;
    auto frame=prepared(gpu);
    REQUIRE(std::holds_alternative<rk::Error>(
        rk::makeFgStreamlineOptionsOn(frame,DXGI_FORMAT_B8G8R8A8_UNORM)));
    frame.resources[2]=gpu.texture(frame.display,DXGI_FORMAT_R16G16_FLOAT);
    REQUIRE(std::holds_alternative<rk::Error>(
        rk::makeFgStreamlineOptionsOn(frame,DXGI_FORMAT_R8G8B8A8_UNORM)));
}
