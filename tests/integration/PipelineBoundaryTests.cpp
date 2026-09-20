#include <catch2/catch_test_macros.hpp>
#include "rk/PipelineBoundary.hpp"
#include <d3dcompiler.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

TEST_CASE("Pixel shader boundary reports HDR source slot without changing state", "[pipeline_boundary]") {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    REQUIRE(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,
        nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)));
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width=4;desc.Height=3;desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;
    desc.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;
    desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    ComPtr<ID3D11Texture2D> hdr,other;
    REQUIRE(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&hdr)));
    REQUIRE(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&other)));
    ComPtr<ID3D11ShaderResourceView> hdrView,otherView;
    REQUIRE(SUCCEEDED(device->CreateShaderResourceView(hdr.Get(),nullptr,&hdrView)));
    REQUIRE(SUCCEEDED(device->CreateShaderResourceView(other.Get(),nullptr,&otherView)));
    ID3D11ShaderResourceView* bound[]={otherView.Get(),hdrView.Get()};
    context->PSSetShaderResources(3,2,bound);
    constexpr char source[]="Texture2D scene : register(t4); float4 main(float4 p:SV_POSITION):SV_TARGET { return scene.Load(int3(0,0,0)); }";
    ComPtr<ID3DBlob> bytecode,errors;
    const auto compiled=D3DCompile(source,sizeof(source)-1,nullptr,nullptr,nullptr,
        "main","ps_5_0",0,0,&bytecode,&errors);
    const char* compileMessage=errors?static_cast<const char*>(errors->GetBufferPointer()):"no shader errors";
    INFO(compileMessage);
    REQUIRE(SUCCEEDED(compiled));
    ComPtr<ID3D11PixelShader> shader;
    REQUIRE(SUCCEEDED(device->CreatePixelShader(bytecode->GetBufferPointer(),
        bytecode->GetBufferSize(),nullptr,&shader)));
    context->PSSetShader(shader.Get(),nullptr,0);

    const auto inspected=rk::inspectPipelineBoundary(context.Get(),hdr.Get());
    REQUIRE(std::holds_alternative<rk::PipelineBoundary>(inspected));
    const auto& boundary=std::get<rk::PipelineBoundary>(inspected);
    REQUIRE(boundary.pixelShaderBound);
    REQUIRE(boundary.resources.size()==2);
    REQUIRE(boundary.resources[0].slot==3);
    REQUIRE_FALSE(boundary.resources[0].matchesScene);
    REQUIRE(boundary.resources[1].slot==4);
    REQUIRE(boundary.resources[1].matchesScene);
    REQUIRE(boundary.resources[1].format==DXGI_FORMAT_R16G16B16A16_FLOAT);
    REQUIRE(boundary.resources[1].width==4);
    REQUIRE(boundary.resources[1].height==3);
    ComPtr<ID3D11PixelShader> stillBound;
    context->PSGetShader(&stillBound,nullptr,nullptr);
    REQUIRE(stillBound.Get()==shader.Get());
    ComPtr<ID3D11ShaderResourceView> stillSource;
    ID3D11ShaderResourceView* raw{};
    context->PSGetShaderResources(4,1,&raw);stillSource.Attach(raw);
    REQUIRE(stillSource.Get()==hdrView.Get());
}
