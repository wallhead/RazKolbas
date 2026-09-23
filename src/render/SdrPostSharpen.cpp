#include "rk/SdrPostSharpen.hpp"
#include "rk/D3D11StateScope.hpp"
#include <d3dcompiler.h>
#include <cmath>
#include <cstdint>
#include <memory>

namespace rk {
namespace {
using Microsoft::WRL::ComPtr;
constexpr char shader[] = R"(
Texture2D<float4> Scene : register(t0);
cbuffer Settings : register(b0) { float Gain; float3 Padding; };
struct Vertex { float4 position : SV_Position; };
Vertex vertex(uint id : SV_VertexID) {
    Vertex result;
    float2 uv=float2((id << 1) & 2, id & 2);
    result.position=float4(uv*float2(2,-2)+float2(-1,1),0,1);
    return result;
}
float4 pixel(Vertex input) : SV_Target {
    uint width,height;
    Scene.GetDimensions(width,height);
    int2 limit=int2(width,height)-1;
    int2 p=clamp(int2(input.position.xy),int2(0,0),limit);
    float4 center=Scene.Load(int3(p,0));
    float3 north=Scene.Load(int3(clamp(p+int2(0,-1),int2(0,0),limit),0)).rgb;
    float3 west =Scene.Load(int3(clamp(p+int2(-1,0),int2(0,0),limit),0)).rgb;
    float3 east =Scene.Load(int3(clamp(p+int2( 1,0),int2(0,0),limit),0)).rgb;
    float3 south=Scene.Load(int3(clamp(p+int2(0, 1),int2(0,0),limit),0)).rgb;
    float3 lo=min(min(north,west),min(east,south));
    float3 hi=max(max(north,west),max(east,south));
    const float epsilon=1.0/65536.0;
    float3 lower=lo/(4.0*max(hi,epsilon));
    float3 upper=(1.0-hi)/min(4.0*lo-4.0,-epsilon);
    float3 allowed=max(upper,-lower);
    float lobe=clamp(max(allowed.r,max(allowed.g,allowed.b)),-0.1875,0.0)*Gain;
    float normalization=max(1.0+4.0*lobe,epsilon);
    float3 result=(center.rgb+lobe*(north+west+east+south))/normalization;
    return float4(saturate(result),center.a);
}
)";

ComPtr<IUnknown> identity(IUnknown* object) {
    ComPtr<IUnknown> canonical;
    if(object)object->QueryInterface(IID_PPV_ARGS(&canonical));
    return canonical;
}
Result<ComPtr<ID3DBlob>> compile(const char* entry,const char* profile) {
    ComPtr<ID3DBlob> code,diagnostics;
    const auto result=D3DCompile(shader,sizeof(shader)-1,nullptr,nullptr,nullptr,
        entry,profile,D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_OPTIMIZATION_LEVEL3,
        0,&code,&diagnostics);
    if(FAILED(result)||!code)
        return Error{ErrorCode::Unavailable,"Cannot compile SDR post-sharpen shader"};
    return code;
}
}

Result<bool> SdrPostSharpenPass::ensureResources(ID3D11Device* device) {
    if(!device)return Error{ErrorCode::InvalidInput,"Post-sharpen device is missing"};
    if(device_&&identity(device_.Get()).Get()==identity(device).Get()&&
       vertexShader_&&pixelShader_&&constants_)return true;
    reset();
    auto vertexCode=compile("vertex","vs_5_0");
    if(const auto error=std::get_if<Error>(&vertexCode))return *error;
    auto pixelCode=compile("pixel","ps_5_0");
    if(const auto error=std::get_if<Error>(&pixelCode))return *error;
    const auto& vs=std::get<ComPtr<ID3DBlob>>(vertexCode);
    const auto& ps=std::get<ComPtr<ID3DBlob>>(pixelCode);
    if(FAILED(device->CreateVertexShader(vs->GetBufferPointer(),vs->GetBufferSize(),
        nullptr,&vertexShader_))||
       FAILED(device->CreatePixelShader(ps->GetBufferPointer(),ps->GetBufferSize(),
        nullptr,&pixelShader_))) {
        reset();
        return Error{ErrorCode::Unavailable,"Cannot create SDR post-sharpen shaders"};
    }
    D3D11_BUFFER_DESC buffer{};
    buffer.ByteWidth=16;
    buffer.Usage=D3D11_USAGE_DEFAULT;
    buffer.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    if(FAILED(device->CreateBuffer(&buffer,nullptr,&constants_))) {
        reset();
        return Error{ErrorCode::Unavailable,"Cannot create SDR post-sharpen constants"};
    }
    device_=device;
    return true;
}

Result<bool> SdrPostSharpenPass::apply(ID3D11DeviceContext* context,
    ID3D11Texture2D* destination,ID3D11Texture2D* source,float strength) {
    if(!context||context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE||
       !destination||!source||identity(destination).Get()==identity(source).Get()||
       !std::isfinite(strength)||strength<=0.0f||strength>1.0f)
        return Error{ErrorCode::InvalidInput,"Post-sharpen arguments are invalid"};
    ComPtr<ID3D11Device> device,destinationDevice,sourceDevice;
    context->GetDevice(&device);
    destination->GetDevice(&destinationDevice);
    source->GetDevice(&sourceDevice);
    if(!device||!destinationDevice||!sourceDevice||
       identity(device.Get()).Get()!=identity(destinationDevice.Get()).Get()||
       identity(device.Get()).Get()!=identity(sourceDevice.Get()).Get())
        return Error{ErrorCode::Conflict,"Post-sharpen resources use different devices"};
    D3D11_TEXTURE2D_DESC target{},input{};
    destination->GetDesc(&target);source->GetDesc(&input);
    if(!target.Width||!target.Height||target.Width!=input.Width||
       target.Height!=input.Height||target.Format!=DXGI_FORMAT_R8G8B8A8_UNORM||
       input.Format!=target.Format||target.MipLevels!=1||input.MipLevels!=1||
       target.ArraySize!=1||input.ArraySize!=1||target.SampleDesc.Count!=1||
       input.SampleDesc.Count!=1||target.Usage!=D3D11_USAGE_DEFAULT||
       input.Usage!=D3D11_USAGE_DEFAULT||
       !(target.BindFlags&D3D11_BIND_RENDER_TARGET)||
       !(input.BindFlags&D3D11_BIND_SHADER_RESOURCE))
        return Error{ErrorCode::Unsupported,"Post-sharpen texture contract differs"};
    ComPtr<ID3D11RenderTargetView> targetView;
    ComPtr<ID3D11Resource> activeTarget;
    context->OMGetRenderTargets(1,targetView.GetAddressOf(),nullptr);
    if(targetView)targetView->GetResource(&activeTarget);
    if(!activeTarget||identity(activeTarget.Get()).Get()!=identity(destination).Get())
        return Error{ErrorCode::Conflict,"Post-sharpen destination is not active RTV0"};
    if(FAILED(device->GetDeviceRemovedReason()))
        return Error{ErrorCode::DeviceRemoved,"Post-sharpen device was removed"};
    if(const auto ready=ensureResources(device.Get());
       const auto error=std::get_if<Error>(&ready))return *error;

    const auto sourceIdentity=identity(source);
    ID3D11ShaderResourceView* sourceView{};
    for(auto& cached:sourceViews_)if(cached.identity.Get()==sourceIdentity.Get()) {
        sourceView=cached.view.Get();
        break;
    }
    if(!sourceView) {
        auto& cached=sourceViews_[nextView_++%sourceViews_.size()];
        cached={};
        if(FAILED(device->CreateShaderResourceView(source,nullptr,&cached.view)))
            return Error{ErrorCode::Unavailable,"Cannot create SDR post-sharpen source view"};
        cached.identity=sourceIdentity;
        sourceView=cached.view.Get();
    }

    const std::array<float,4> constants{std::exp2(2.0f*strength-2.0f),0,0,0};
    context->UpdateSubresource(constants_.Get(),0,nullptr,constants.data(),0,0);
    auto isolated=D3D11StateScope::begin(context);
    if(const auto error=std::get_if<Error>(&isolated))return *error;
    auto scope=std::move(std::get<std::unique_ptr<D3D11StateScope>>(isolated));
    const D3D11_VIEWPORT viewport{0,0,static_cast<float>(target.Width),
        static_cast<float>(target.Height),0,1};
    context->OMSetRenderTargets(1,targetView.GetAddressOf(),nullptr);
    context->RSSetViewports(1,&viewport);
    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(vertexShader_.Get(),nullptr,0);
    context->PSSetShader(pixelShader_.Get(),nullptr,0);
    context->PSSetShaderResources(0,1,&sourceView);
    context->PSSetConstantBuffers(0,1,constants_.GetAddressOf());
    context->Draw(3,0);
    return true;
}

void SdrPostSharpenPass::reset() noexcept {
    for(auto& cached:sourceViews_)cached={};
    constants_.Reset();pixelShader_.Reset();vertexShader_.Reset();device_.Reset();
    nextView_=0;
}
}
