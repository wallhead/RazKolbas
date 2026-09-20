#include "rk/D3D11StateScope.hpp"
#include <cstdint>

namespace rk {
using Microsoft::WRL::ComPtr;
D3D11StateScope::D3D11StateScope(ComPtr<ID3D11DeviceContext1> context,
    ComPtr<ID3DDeviceContextState> previous) noexcept:
    context_(std::move(context)),previous_(std::move(previous)) {}

Result<std::unique_ptr<D3D11StateScope>> D3D11StateScope::begin(ID3D11DeviceContext* context) {
    if(!context||context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)
        return Error{ErrorCode::InvalidInput,"State isolation requires an immediate D3D11 context"};
    ComPtr<ID3D11DeviceContext1> context1;
    if(FAILED(context->QueryInterface(IID_PPV_ARGS(&context1))))
        return Error{ErrorCode::Unsupported,"D3D11.1 context state switching is unavailable"};
    ComPtr<ID3D11Device> device;
    context->GetDevice(&device);
    ComPtr<ID3D11Device1> device1;
    if(!device||FAILED(device.As(&device1)))
        return Error{ErrorCode::Unsupported,"D3D11.1 device state switching is unavailable"};
    ComPtr<ID3DDeviceContextState> isolated;
    const auto feature=device->GetFeatureLevel();
    const auto created=device1->CreateDeviceContextState(0,&feature,1,D3D11_SDK_VERSION,
        __uuidof(ID3D11Device1),nullptr,&isolated);
    if(FAILED(created)||!isolated)
        return Error{ErrorCode::Unavailable,"Cannot create isolated D3D11 context state, HRESULT="+
            std::to_string(static_cast<std::uint32_t>(created))};
    ComPtr<ID3DDeviceContextState> previous;
    context1->SwapDeviceContextState(isolated.Get(),&previous);
    return std::unique_ptr<D3D11StateScope>(new D3D11StateScope(std::move(context1),std::move(previous)));
}

D3D11StateScope::~D3D11StateScope() {
    if(context_)context_->SwapDeviceContextState(previous_.Get(),nullptr);
}
}
