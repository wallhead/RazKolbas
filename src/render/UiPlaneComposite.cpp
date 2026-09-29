#include "rk/UiPlaneComposite.hpp"
#include "rk/D3D11StateScope.hpp"
#include <d3dcompiler.h>
#include <memory>

namespace rk {
namespace {
using Microsoft::WRL::ComPtr;
constexpr char shader[] = R"(
Texture2D<float4> Ui : register(t0);
struct Vertex { float4 position : SV_Position; };
Vertex vertex(uint id : SV_VertexID) {
    Vertex result;
    float2 uv=float2((id << 1) & 2, id & 2);
    result.position=float4(uv*float2(2,-2)+float2(-1,1),0,1);
    return result;
}
float4 pixel(Vertex input) : SV_Target {
    return Ui.Load(int3(int2(input.position.xy),0));
}
)";

ComPtr<IUnknown> identity(IUnknown* object) noexcept {
    ComPtr<IUnknown> value;
    if(object)object->QueryInterface(IID_PPV_ARGS(&value));
    return value;
}
bool sameObject(IUnknown* first,IUnknown* second) noexcept {
    const auto a=identity(first),b=identity(second);
    return a&&b&&a.Get()==b.Get();
}
Result<ComPtr<ID3DBlob>> compile(const char* entry,const char* profile) {
    ComPtr<ID3DBlob> code,diagnostics;
    if(FAILED(D3DCompile(shader,sizeof(shader)-1,nullptr,nullptr,nullptr,
        entry,profile,D3DCOMPILE_ENABLE_STRICTNESS,0,&code,&diagnostics))||!code)
        return Error{ErrorCode::Unavailable,"Cannot compile UI composition shader"};
    return code;
}
}

Result<bool> compositePremultipliedUi(ID3D11DeviceContext* context,
    ID3D11Texture2D* uiPlane,ID3D11RenderTargetView* finalView) {
    if(!context||!uiPlane||!finalView||
       context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)
        return Error{ErrorCode::InvalidInput,"UI composition requires an immediate context and two targets"};
    ComPtr<ID3D11Resource> finalResource;
    finalView->GetResource(&finalResource);
    ComPtr<ID3D11Texture2D> finalTexture;
    if(!finalResource||FAILED(finalResource.As(&finalTexture))||
       sameObject(finalTexture.Get(),uiPlane))
        return Error{ErrorCode::InvalidInput,"UI plane aliases or lacks final colour"};
    D3D11_TEXTURE2D_DESC uiDesc{},finalDesc{};
    uiPlane->GetDesc(&uiDesc);finalTexture->GetDesc(&finalDesc);
    D3D11_RENDER_TARGET_VIEW_DESC viewDesc{};finalView->GetDesc(&viewDesc);
    if(!uiDesc.Width||!uiDesc.Height||uiDesc.Width>8192||uiDesc.Height>8192||
       uiDesc.Width!=finalDesc.Width||uiDesc.Height!=finalDesc.Height||
       uiDesc.Format!=DXGI_FORMAT_R8G8B8A8_UNORM||
       finalDesc.Format!=uiDesc.Format||uiDesc.MipLevels!=1||
       finalDesc.MipLevels!=1||uiDesc.ArraySize!=1||finalDesc.ArraySize!=1||
       uiDesc.SampleDesc.Count!=1||finalDesc.SampleDesc.Count!=1||
       uiDesc.SampleDesc.Quality!=0||finalDesc.SampleDesc.Quality!=0||
       uiDesc.Usage!=D3D11_USAGE_DEFAULT||
       finalDesc.Usage!=D3D11_USAGE_DEFAULT||
       !(uiDesc.BindFlags&D3D11_BIND_SHADER_RESOURCE)||
       !(finalDesc.BindFlags&D3D11_BIND_RENDER_TARGET)||
       viewDesc.ViewDimension!=D3D11_RTV_DIMENSION_TEXTURE2D||
       viewDesc.Texture2D.MipSlice!=0)
        return Error{ErrorCode::Unsupported,"UI composition format or geometry differs"};
    ComPtr<ID3D11Device> device,uiOwner,finalOwner;
    context->GetDevice(&device);uiPlane->GetDevice(&uiOwner);
    finalTexture->GetDevice(&finalOwner);
    if(!sameObject(device.Get(),uiOwner.Get())||
       !sameObject(device.Get(),finalOwner.Get()))
        return Error{ErrorCode::Conflict,"UI plane and final target belong to different devices"};
    auto vsCode=compile("vertex","vs_5_0");
    if(const auto error=std::get_if<Error>(&vsCode))return *error;
    auto psCode=compile("pixel","ps_5_0");
    if(const auto error=std::get_if<Error>(&psCode))return *error;
    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader> ps;
    auto& vertex=std::get<ComPtr<ID3DBlob>>(vsCode);
    auto& pixel=std::get<ComPtr<ID3DBlob>>(psCode);
    if(FAILED(device->CreateVertexShader(vertex->GetBufferPointer(),
            vertex->GetBufferSize(),nullptr,&vs))||
       FAILED(device->CreatePixelShader(pixel->GetBufferPointer(),
            pixel->GetBufferSize(),nullptr,&ps)))
        return Error{ErrorCode::Unavailable,"Cannot create UI composition shaders"};
    ComPtr<ID3D11ShaderResourceView> uiView;
    if(FAILED(device->CreateShaderResourceView(uiPlane,nullptr,&uiView)))
        return Error{ErrorCode::Unavailable,"Cannot sample the UI plane"};
    D3D11_BLEND_DESC blendDesc{};
    blendDesc.RenderTarget[0].BlendEnable=TRUE;
    blendDesc.RenderTarget[0].SrcBlend=D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlend=D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp=D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha=D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].DestBlendAlpha=D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].BlendOpAlpha=D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
    ComPtr<ID3D11BlendState> blend;
    if(FAILED(device->CreateBlendState(&blendDesc,&blend)))
        return Error{ErrorCode::Unavailable,"Cannot create UI composition blend state"};
    D3D11_RASTERIZER_DESC raster{};
    raster.FillMode=D3D11_FILL_SOLID;raster.CullMode=D3D11_CULL_NONE;
    raster.DepthClipEnable=TRUE;
    ComPtr<ID3D11RasterizerState> rasterState;
    if(FAILED(device->CreateRasterizerState(&raster,&rasterState)))
        return Error{ErrorCode::Unavailable,"Cannot create UI composition rasterizer"};
    D3D11_DEPTH_STENCIL_DESC depth{};
    depth.DepthEnable=FALSE;depth.StencilEnable=FALSE;
    ComPtr<ID3D11DepthStencilState> depthState;
    if(FAILED(device->CreateDepthStencilState(&depth,&depthState)))
        return Error{ErrorCode::Unavailable,"Cannot create UI composition depth state"};
    auto isolated=D3D11StateScope::begin(context);
    if(const auto error=std::get_if<Error>(&isolated))return *error;
    auto scope=std::move(std::get<std::unique_ptr<D3D11StateScope>>(isolated));
    auto* target=finalView;
    context->OMSetRenderTargets(1,&target,nullptr);
    context->OMSetBlendState(blend.Get(),nullptr,0xffffffff);
    context->OMSetDepthStencilState(depthState.Get(),0);
    context->RSSetState(rasterState.Get());
    const D3D11_VIEWPORT viewport{0,0,static_cast<float>(uiDesc.Width),
        static_cast<float>(uiDesc.Height),0,1};
    context->RSSetViewports(1,&viewport);
    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(vs.Get(),nullptr,0);
    context->PSSetShader(ps.Get(),nullptr,0);
    context->PSSetShaderResources(0,1,uiView.GetAddressOf());
    context->Draw(3,0);
    return true;
}
}
