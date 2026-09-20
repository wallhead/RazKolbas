#include "rk/SdrDisplayCopy.hpp"
#include <wrl/client.h>

namespace rk {
namespace {
Microsoft::WRL::ComPtr<IUnknown> identity(IUnknown* object) {
    Microsoft::WRL::ComPtr<IUnknown> canonical;
    if(object)object->QueryInterface(IID_PPV_ARGS(&canonical));
    return canonical;
}
}
Result<bool> copySdrDisplayFrame(ID3D11DeviceContext* context,
    ID3D11Texture2D* backbuffer,ID3D11Texture2D* output) {
    if(!context||!backbuffer||!output||context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)
        return Error{ErrorCode::InvalidInput,"SDR display copy needs an immediate context and two textures"};
    if(identity(backbuffer).Get()==identity(output).Get())
        return Error{ErrorCode::InvalidInput,"SDR display source aliases backbuffer"};
    Microsoft::WRL::ComPtr<ID3D11Device> contextDevice,backDevice,outputDevice;
    context->GetDevice(&contextDevice);
    backbuffer->GetDevice(&backDevice);output->GetDevice(&outputDevice);
    if(!contextDevice||!backDevice||!outputDevice||
       identity(contextDevice.Get()).Get()!=identity(backDevice.Get()).Get()||
       identity(contextDevice.Get()).Get()!=identity(outputDevice.Get()).Get())
        return Error{ErrorCode::Conflict,"SDR display resources have different devices"};
    D3D11_TEXTURE2D_DESC back{},result{};
    backbuffer->GetDesc(&back);output->GetDesc(&result);
    if(back.Format!=DXGI_FORMAT_R8G8B8A8_UNORM||result.Format!=back.Format||
       !back.Width||!back.Height||back.Width!=result.Width||back.Height!=result.Height||
       back.MipLevels!=1||result.MipLevels!=1||back.ArraySize!=1||result.ArraySize!=1||
       back.SampleDesc.Count!=1||result.SampleDesc.Count!=1||
       back.SampleDesc.Quality!=result.SampleDesc.Quality||
       back.Usage!=D3D11_USAGE_DEFAULT||result.Usage!=D3D11_USAGE_DEFAULT||
       !(back.BindFlags&D3D11_BIND_RENDER_TARGET)||
       !(result.BindFlags&D3D11_BIND_UNORDERED_ACCESS))
        return Error{ErrorCode::Unsupported,"SDR display target and output do not match"};
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> currentView;
    context->OMGetRenderTargets(1,currentView.GetAddressOf(),nullptr);
    Microsoft::WRL::ComPtr<ID3D11Resource> currentResource;
    if(currentView)currentView->GetResource(&currentResource);
    if(!currentResource||identity(currentResource.Get()).Get()!=identity(backbuffer).Get())
        return Error{ErrorCode::Conflict,"SDR backbuffer is not active RTV0"};
    if(FAILED(contextDevice->GetDeviceRemovedReason()))
        return Error{ErrorCode::DeviceRemoved,"SDR display device was removed"};
    context->CopyResource(backbuffer,output);
    return true;
}
}
