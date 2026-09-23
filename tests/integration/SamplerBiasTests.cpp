#include <catch2/catch_test_macros.hpp>
#include "rk/SamplerBiasCache.hpp"
#include <wrl/client.h>
#include <array>

using Microsoft::WRL::ComPtr;

namespace {
ComPtr<ID3D11SamplerState> sampler(ID3D11Device* device,
    D3D11_FILTER filter,float bias,UINT anisotropy) {
    D3D11_SAMPLER_DESC desc{};
    desc.Filter=filter;
    desc.AddressU=desc.AddressV=desc.AddressW=D3D11_TEXTURE_ADDRESS_WRAP;
    desc.MipLODBias=bias;
    desc.MaxAnisotropy=anisotropy;
    desc.ComparisonFunc=D3D11_COMPARISON_NEVER;
    desc.MinLOD=0;desc.MaxLOD=D3D11_FLOAT32_MAX;
    ComPtr<ID3D11SamplerState> result;
    REQUIRE(SUCCEEDED(device->CreateSamplerState(&desc,&result)));
    return result;
}
}

TEST_CASE("Reduced rendering biases only zero-bias anisotropic samplers",
    "[sampler_bias]") {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    REQUIRE(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,
        nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)));
    auto anisotropic=sampler(device.Get(),D3D11_FILTER_ANISOTROPIC,0.0f,16);
    auto linear=sampler(device.Get(),D3D11_FILTER_MIN_MAG_MIP_LINEAR,0.0f,1);
    auto authored=sampler(device.Get(),D3D11_FILTER_ANISOTROPIC,-0.25f,16);
    rk::SamplerBiasCache cache;
    REQUIRE(SUCCEEDED(cache.configure(context.Get(),-0.584962f)));
    std::array<ID3D11SamplerState*,3> input{
        anisotropic.Get(),linear.Get(),authored.Get()};
    std::array<ID3D11SamplerState*,3> output{};
    REQUIRE(cache.remap(context.Get(),input,output));
    REQUIRE(output[0]!=input[0]);
    REQUIRE(output[1]==input[1]);
    REQUIRE(output[2]==input[2]);
    D3D11_SAMPLER_DESC changed{};output[0]->GetDesc(&changed);
    REQUIRE(changed.MipLODBias==-0.584962f);
    REQUIRE(changed.MaxAnisotropy==16);
    std::array<ID3D11SamplerState*,3> again{};
    REQUIRE(cache.remap(context.Get(),input,again));
    REQUIRE(again[0]==output[0]);
    REQUIRE(cache.replacementCount()==1);
}

TEST_CASE("Sampler bias rejects unsafe configuration and foreign contexts",
    "[sampler_bias]") {
    ComPtr<ID3D11Device> firstDevice,secondDevice;
    ComPtr<ID3D11DeviceContext> firstContext,secondContext;
    REQUIRE(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,
        nullptr,0,D3D11_SDK_VERSION,&firstDevice,nullptr,&firstContext)));
    REQUIRE(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,
        nullptr,0,D3D11_SDK_VERSION,&secondDevice,nullptr,&secondContext)));
    rk::SamplerBiasCache cache;
    REQUIRE(FAILED(cache.configure(firstContext.Get(),-4.0f)));
    REQUIRE(SUCCEEDED(cache.configure(firstContext.Get(),-0.5f)));
    auto source=sampler(firstDevice.Get(),D3D11_FILTER_ANISOTROPIC,0.0f,8);
    std::array<ID3D11SamplerState*,1> input{source.Get()},output{};
    REQUIRE_FALSE(cache.remap(secondContext.Get(),input,output));
    REQUIRE(output[0]==source.Get());
}
