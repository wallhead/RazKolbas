#include "rk/PipelineBoundary.hpp"
#include <wrl/client.h>
#include <array>

namespace rk {
namespace {
std::uintptr_t identity(IUnknown* object) noexcept {
    Microsoft::WRL::ComPtr<IUnknown> canonical;
    return object&&SUCCEEDED(object->QueryInterface(IID_PPV_ARGS(&canonical)))?
        reinterpret_cast<std::uintptr_t>(canonical.Get()):0;
}
}
Result<PipelineBoundary> inspectPipelineBoundary(ID3D11DeviceContext* context,
    ID3D11Texture2D* scene) {
    if(!context||!scene)return Error{ErrorCode::InvalidInput,"Pipeline boundary needs context and scene"};
    Microsoft::WRL::ComPtr<ID3D11Device> contextDevice,sceneDevice;
    context->GetDevice(&contextDevice);scene->GetDevice(&sceneDevice);
    if(!contextDevice||!sceneDevice||identity(contextDevice.Get())!=identity(sceneDevice.Get()))
        return Error{ErrorCode::InvalidInput,"Pipeline boundary scene belongs to another device"};
    PipelineBoundary result;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixel;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertex;
    context->PSGetShader(&pixel,nullptr,nullptr);
    context->VSGetShader(&vertex,nullptr,nullptr);
    result.pixelShaderBound=!!pixel;result.vertexShaderBound=!!vertex;
    result.pixelShaderIdentity=identity(pixel.Get());
    result.vertexShaderIdentity=identity(vertex.Get());
    context->IAGetPrimitiveTopology(&result.topology);
    context->RSGetViewports(&result.viewportCount,nullptr);
    std::array<ID3D11ShaderResourceView*,D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT> raw{};
    context->PSGetShaderResources(0,static_cast<UINT>(raw.size()),raw.data());
    const auto sceneIdentity=identity(scene);
    for(unsigned slot=0;slot<raw.size();++slot) {
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
        view.Attach(raw[slot]);
        if(!view)continue;
        Microsoft::WRL::ComPtr<ID3D11Resource> resource;
        view->GetResource(&resource);
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
        if(!resource||FAILED(resource.As(&texture)))continue;
        D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);
        const auto targetIdentity=identity(resource.Get());
        result.resources.push_back(PipelineResourceSlot{slot,targetIdentity,desc.Format,
            desc.Width,desc.Height,targetIdentity&&targetIdentity==sceneIdentity});
    }
    return result;
}
}
