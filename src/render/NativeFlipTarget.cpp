#include "rk/NativeFlipTarget.hpp"
#include <utility>

namespace rk {
namespace {
using Microsoft::WRL::ComPtr;
ComPtr<IUnknown> identity(IUnknown* object) noexcept {
    ComPtr<IUnknown> value;
    if(object)object->QueryInterface(IID_PPV_ARGS(value.GetAddressOf()));
    return value;
}
}
Result<NativeFlipTarget> acquireNativeFlipTarget(IDXGISwapChain* swap,
    ID3D11Device* device,Extent display) {
    if(!swap||!device||!display.valid()||
       display.width>8192||display.height>8192)
        return Error{ErrorCode::InvalidInput,"Native flip target arguments are invalid"};
    DXGI_SWAP_CHAIN_DESC chain{};
    const auto described=swap->GetDesc(&chain);
    if(FAILED(described))
        return Error{ErrorCode::Unavailable,"Native swap descriptor unavailable"};
    if((chain.SwapEffect!=DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL&&
        chain.SwapEffect!=DXGI_SWAP_EFFECT_FLIP_DISCARD)||
       chain.BufferCount<2||chain.BufferCount>16||
       chain.BufferDesc.Width!=display.width||
       chain.BufferDesc.Height!=display.height||
       chain.BufferDesc.Format!=DXGI_FORMAT_R8G8B8A8_UNORM||
       chain.SampleDesc.Count!=1)
        return Error{ErrorCode::Unsupported,"Native swap is not the expected SDR flip chain"};
    ComPtr<IDXGISwapChain3> flip;
    if(FAILED(swap->QueryInterface(IID_PPV_ARGS(flip.GetAddressOf())))||!flip)
        return Error{ErrorCode::Unsupported,"Native swap has no current-buffer interface"};
    const auto index=flip->GetCurrentBackBufferIndex();
    if(index>=chain.BufferCount)
        return Error{ErrorCode::Conflict,"Current native flip-buffer index is out of range"};
    ComPtr<ID3D11Texture2D> buffer;
    if(FAILED(swap->GetBuffer(index,IID_PPV_ARGS(buffer.GetAddressOf())))||!buffer)
        return Error{ErrorCode::Unavailable,"Current native flip buffer is unavailable"};
    D3D11_TEXTURE2D_DESC texture{};
    buffer->GetDesc(&texture);
    if(texture.Width!=display.width||texture.Height!=display.height||
       texture.Format!=chain.BufferDesc.Format||texture.ArraySize!=1||
       texture.MipLevels!=1||texture.SampleDesc.Count!=1||
       !(texture.BindFlags&D3D11_BIND_RENDER_TARGET))
        return Error{ErrorCode::Conflict,"Current native flip buffer descriptor differs"};
    ComPtr<ID3D11Device> owner;
    buffer->GetDevice(owner.GetAddressOf());
    auto ownerId=identity(owner.Get()),deviceId=identity(device);
    if(!ownerId||!deviceId||ownerId.Get()!=deviceId.Get())
        return Error{ErrorCode::Conflict,"Current native flip buffer belongs to another device"};
    ComPtr<ID3D11RenderTargetView> view;
    if(FAILED(device->CreateRenderTargetView(buffer.Get(),nullptr,view.GetAddressOf()))||!view)
        return Error{ErrorCode::Unavailable,"Cannot create current native flip RTV"};
    return NativeFlipTarget{std::move(buffer),std::move(view),index};
}
}
