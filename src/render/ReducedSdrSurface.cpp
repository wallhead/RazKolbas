#include "rk/ReducedSdrSurface.hpp"
#include <utility>

namespace rk {
using Microsoft::WRL::ComPtr;
ReducedSdrSurface::ReducedSdrSurface(ComPtr<ID3D11Texture2D> texture,
    ComPtr<ID3D11RenderTargetView> view,
    Extent render,Extent display) noexcept:
    texture_(std::move(texture)),view_(std::move(view)),render_(render),display_(display) {}
HRESULT ReducedSdrSurface::queryBuffer(REFIID iid,void** output) const noexcept {
    if(!output)return E_POINTER;
    *output=nullptr;
    if(!texture_)return DXGI_ERROR_INVALID_CALL;
    return texture_->QueryInterface(iid,output);
}

Result<ReducedSdrSurface> createReducedSdrSurface(ID3D11Device* device,
    Extent display,Extent render) {
    if(!device||!display.valid()||!render.valid()||
       display.width>8192||display.height>8192||
       render.width>display.width||render.height>display.height||
       (render.width==display.width&&render.height==display.height))
        return Error{ErrorCode::InvalidInput,"Invalid reduced SDR surface geometry"};
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width=render.width;
    desc.Height=render.height;
    desc.MipLevels=1;
    desc.ArraySize=1;
    desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count=1;
    desc.Usage=D3D11_USAGE_DEFAULT;
    desc.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
    ComPtr<ID3D11Texture2D> texture;
    auto hr=device->CreateTexture2D(&desc,nullptr,&texture);
    if(FAILED(hr))return Error{hr==DXGI_ERROR_DEVICE_REMOVED||hr==DXGI_ERROR_DEVICE_RESET?
        ErrorCode::DeviceRemoved:ErrorCode::Unavailable,"Cannot allocate reduced SDR texture"};
    ComPtr<ID3D11RenderTargetView> view;
    hr=device->CreateRenderTargetView(texture.Get(),nullptr,&view);
    if(FAILED(hr))return Error{hr==DXGI_ERROR_DEVICE_REMOVED||hr==DXGI_ERROR_DEVICE_RESET?
        ErrorCode::DeviceRemoved:ErrorCode::Unavailable,"Cannot create reduced SDR render target"};
    return ReducedSdrSurface{std::move(texture),std::move(view),render,display};
}
}
