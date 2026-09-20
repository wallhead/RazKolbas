#include "rk/SpatialFallback.hpp"
#include "rk/D3D11StateScope.hpp"
#include <d3dcompiler.h>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

namespace rk {
namespace {
using Microsoft::WRL::ComPtr;
constexpr char shader[] = R"(
Texture2D<float4> Scene : register(t0);
SamplerState LinearClamp : register(s0);
struct Vertex { float4 position : SV_Position; float2 uv : TEXCOORD0; };
Vertex vertex(uint id : SV_VertexID) {
    Vertex result;
    result.uv=float2((id << 1) & 2, id & 2);
    result.position=float4(result.uv*float2(2,-2)+float2(-1,1),0,1);
    return result;
}
float4 pixel(Vertex input) : SV_Target {
    return Scene.SampleLevel(LinearClamp,input.uv,0);
}
)";

Result<ComPtr<ID3DBlob>> compile(const char* entry,const char* profile) {
    ComPtr<ID3DBlob> code,diagnostics;
    const auto status=D3DCompile(shader,sizeof(shader)-1,nullptr,nullptr,nullptr,
        entry,profile,D3DCOMPILE_ENABLE_STRICTNESS,0,&code,&diagnostics);
    if(FAILED(status)||!code)
        return Error{ErrorCode::Unavailable,"Cannot compile spatial fallback shader"};
    return code;
}

Result<bool> draw(ID3D11Device* device,ID3D11DeviceContext* context,
    ID3D11Texture2D* source,ID3D11Texture2D* output,UINT width,UINT height) {
    auto vertexCode=compile("vertex","vs_5_0");
    if(const auto error=std::get_if<Error>(&vertexCode))return *error;
    auto pixelCode=compile("pixel","ps_5_0");
    if(const auto error=std::get_if<Error>(&pixelCode))return *error;
    ComPtr<ID3D11VertexShader> vertexShader;
    ComPtr<ID3D11PixelShader> pixelShader;
    auto& vs=std::get<ComPtr<ID3DBlob>>(vertexCode);
    auto& ps=std::get<ComPtr<ID3DBlob>>(pixelCode);
    if(FAILED(device->CreateVertexShader(vs->GetBufferPointer(),vs->GetBufferSize(),
        nullptr,&vertexShader))||
       FAILED(device->CreatePixelShader(ps->GetBufferPointer(),ps->GetBufferSize(),
        nullptr,&pixelShader)))
        return Error{ErrorCode::Unavailable,"Cannot create spatial fallback shaders"};
    ComPtr<ID3D11ShaderResourceView> sourceView;
    ComPtr<ID3D11RenderTargetView> targetView;
    if(FAILED(device->CreateShaderResourceView(source,nullptr,&sourceView))||
       FAILED(device->CreateRenderTargetView(output,nullptr,&targetView)))
        return Error{ErrorCode::Unavailable,"Cannot create spatial fallback views"};
    D3D11_SAMPLER_DESC samplerDescription{};
    samplerDescription.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDescription.AddressU=samplerDescription.AddressV=samplerDescription.AddressW=
        D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDescription.MaxLOD=D3D11_FLOAT32_MAX;
    ComPtr<ID3D11SamplerState> sampler;
    if(FAILED(device->CreateSamplerState(&samplerDescription,&sampler)))
        return Error{ErrorCode::Unavailable,"Cannot create spatial fallback sampler"};
    auto isolated=D3D11StateScope::begin(context);
    if(const auto error=std::get_if<Error>(&isolated))return *error;
    auto scope=std::move(std::get<std::unique_ptr<D3D11StateScope>>(isolated));
    const D3D11_VIEWPORT viewport{0,0,static_cast<float>(width),
        static_cast<float>(height),0,1};
    context->OMSetRenderTargets(1,targetView.GetAddressOf(),nullptr);
    context->RSSetViewports(1,&viewport);
    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(vertexShader.Get(),nullptr,0);
    context->PSSetShader(pixelShader.Get(),nullptr,0);
    context->PSSetShaderResources(0,1,sourceView.GetAddressOf());
    context->PSSetSamplers(0,1,sampler.GetAddressOf());
    context->Draw(3,0);
    return true;
}
}

SpatialFallbackFrame::SpatialFallbackFrame(Microsoft::WRL::ComPtr<ID3D11Texture2D> source,
    Microsoft::WRL::ComPtr<ID3D11Texture2D> output,
    Microsoft::WRL::ComPtr<ID3D11Query> completion,UINT width,UINT height) noexcept:
    source_(std::move(source)),output_(std::move(output)),
    completion_(std::move(completion)),width_(width),height_(height) {}
Result<bool> SpatialFallbackFrame::complete(ID3D11DeviceContext* context) const {
    if(!context||!completion_||!source_)
        return Error{ErrorCode::InvalidInput,"Fallback frame or context is incomplete"};
    ComPtr<ID3D11Device> contextDevice,sourceDevice;
    context->GetDevice(&contextDevice);
    source_->GetDevice(&sourceDevice);
    ComPtr<IUnknown> contextIdentity,sourceIdentity;
    if(!contextDevice||!sourceDevice||FAILED(contextDevice.As(&contextIdentity))||
       FAILED(sourceDevice.As(&sourceIdentity))||contextIdentity.Get()!=sourceIdentity.Get())
        return Error{ErrorCode::Conflict,"Fallback completion uses another device"};
    const auto status=context->GetData(completion_.Get(),nullptr,0,D3D11_ASYNC_GETDATA_DONOTFLUSH);
    if(status==S_FALSE)return false;
    if(FAILED(status)||FAILED(contextDevice->GetDeviceRemovedReason()))
        return Error{ErrorCode::DeviceRemoved,"Fallback GPU completion failed"};
    return true;
}
static Result<SpatialFallbackFrame> produceFallback(ID3D11DeviceContext* context,
    ID3D11Texture2D* source,UINT displayWidth,UINT displayHeight,
    DXGI_FORMAT expectedFormat) {
    if(!context||context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE||!source||
       !displayWidth||!displayHeight||displayWidth>8192||displayHeight>8192)
        return Error{ErrorCode::InvalidInput,"Fallback requires source, immediate context and valid display extent"};
    ComPtr<ID3D11Device> device,sourceDevice;
    context->GetDevice(&device);
    source->GetDevice(&sourceDevice);
    ComPtr<IUnknown> identity,sourceIdentity;
    if(!device||!sourceDevice||FAILED(device.As(&identity))||
       FAILED(sourceDevice.As(&sourceIdentity))||identity.Get()!=sourceIdentity.Get())
        return Error{ErrorCode::Conflict,"Fallback source belongs to another device"};
    D3D11_TEXTURE2D_DESC description{};
    source->GetDesc(&description);
    if(description.Format!=expectedFormat||!description.Width||
       !description.Height||description.Width>8192||description.Height>8192||
       description.MipLevels!=1||description.ArraySize!=1||
       description.SampleDesc.Count!=1||description.Usage!=D3D11_USAGE_DEFAULT||
       !(description.BindFlags&D3D11_BIND_SHADER_RESOURCE))
        return Error{ErrorCode::Unsupported,"Fallback source format or geometry differs"};
    auto targetDescription=description;
    targetDescription.Width=displayWidth;targetDescription.Height=displayHeight;
    targetDescription.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
    targetDescription.CPUAccessFlags=0;targetDescription.MiscFlags=0;
    ComPtr<ID3D11Texture2D> target;
    if(FAILED(device->CreateTexture2D(&targetDescription,nullptr,&target)))
        return Error{ErrorCode::Unavailable,"Cannot allocate display-sized spatial fallback"};
    const D3D11_QUERY_DESC queryDescription{D3D11_QUERY_EVENT,0};
    ComPtr<ID3D11Query> completion;
    if(FAILED(device->CreateQuery(&queryDescription,&completion)))
        return Error{ErrorCode::Unavailable,"Cannot allocate fallback completion event"};
    if(description.Width==displayWidth&&description.Height==displayHeight)
        context->CopyResource(target.Get(),source);
    else if(const auto drawn=draw(device.Get(),context,source,target.Get(),displayWidth,displayHeight);
            const auto error=std::get_if<Error>(&drawn))return *error;
    context->End(completion.Get());
    return SpatialFallbackFrame{ComPtr<ID3D11Texture2D>(source),std::move(target),
        std::move(completion),displayWidth,displayHeight};
}
Result<SpatialFallbackFrame> produceSpatialFallback(ID3D11DeviceContext* context,
    ID3D11Texture2D* source,UINT displayWidth,UINT displayHeight) {
    return produceFallback(context,source,displayWidth,displayHeight,
        DXGI_FORMAT_R16G16B16A16_FLOAT);
}
Result<SpatialFallbackFrame> produceSdrSpatialFallback(ID3D11DeviceContext* context,
    ID3D11Texture2D* source,UINT displayWidth,UINT displayHeight) {
    return produceFallback(context,source,displayWidth,displayHeight,
        DXGI_FORMAT_R8G8B8A8_UNORM);
}
Result<SpatialFallbackFrame> produceSdrSpatialFallbackToDisplay(
    ID3D11DeviceContext* context,ID3D11Texture2D* source,
    ID3D11Texture2D* display) {
    if(!context||context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE||
       !source||!display||source==display)
        return Error{ErrorCode::InvalidInput,"SDR fallback needs distinct scene and active display"};
    ComPtr<ID3D11Device> device,sourceDevice,displayDevice;
    context->GetDevice(&device);
    source->GetDevice(&sourceDevice);
    display->GetDevice(&displayDevice);
    ComPtr<IUnknown> deviceId,sourceId,displayId;
    if(!device||!sourceDevice||!displayDevice||FAILED(device.As(&deviceId))||
       FAILED(sourceDevice.As(&sourceId))||FAILED(displayDevice.As(&displayId))||
       deviceId.Get()!=sourceId.Get()||deviceId.Get()!=displayId.Get())
        return Error{ErrorCode::Conflict,"SDR fallback resources belong to different devices"};
    ComPtr<IUnknown> displayResourceId,sourceResourceId;
    if(FAILED(display->QueryInterface(IID_PPV_ARGS(&displayResourceId)))||
       FAILED(source->QueryInterface(IID_PPV_ARGS(&sourceResourceId)))||
       displayResourceId.Get()==sourceResourceId.Get())
        return Error{ErrorCode::Conflict,"SDR fallback source aliases display"};
    D3D11_TEXTURE2D_DESC input{},output{};
    source->GetDesc(&input);display->GetDesc(&output);
    if(input.Format!=DXGI_FORMAT_R8G8B8A8_UNORM||output.Format!=input.Format||
       !input.Width||!input.Height||!output.Width||!output.Height||
       output.Width<input.Width||output.Height<input.Height||
       output.Width>8192||output.Height>8192||
       input.MipLevels!=1||output.MipLevels!=1||
       input.ArraySize!=1||output.ArraySize!=1||
       input.SampleDesc.Count!=1||output.SampleDesc.Count!=1||
       input.Usage!=D3D11_USAGE_DEFAULT||output.Usage!=D3D11_USAGE_DEFAULT||
       !(input.BindFlags&D3D11_BIND_SHADER_RESOURCE)||
       !(output.BindFlags&D3D11_BIND_RENDER_TARGET))
        return Error{ErrorCode::Unsupported,"SDR fallback source/display format or extent differs"};
    ComPtr<ID3D11RenderTargetView> currentView;
    ComPtr<ID3D11Resource> currentResource;
    ComPtr<IUnknown> currentId;
    context->OMGetRenderTargets(1,currentView.GetAddressOf(),nullptr);
    if(currentView)currentView->GetResource(&currentResource);
    if(!currentResource||FAILED(currentResource.As(&currentId))||
       currentId.Get()!=displayResourceId.Get())
        return Error{ErrorCode::Conflict,"SDR fallback display is not active RTV0"};
    const D3D11_QUERY_DESC queryDescription{D3D11_QUERY_EVENT,0};
    ComPtr<ID3D11Query> completion;
    if(FAILED(device->CreateQuery(&queryDescription,&completion)))
        return Error{ErrorCode::Unavailable,"Cannot allocate SDR fallback completion event"};
    if(const auto rendered=draw(device.Get(),context,source,display,
            output.Width,output.Height);const auto error=std::get_if<Error>(&rendered))
        return *error;
    context->End(completion.Get());
    // D3D11 retains the draw target for submitted commands; the result must
    // not hold the swap buffer across ResizeBuffers.
    return SpatialFallbackFrame{ComPtr<ID3D11Texture2D>(source),
        {},std::move(completion),
        output.Width,output.Height};
}
}
