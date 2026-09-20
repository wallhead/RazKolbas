#pragma once
#include "rk/Result.hpp"
#include <d3d11_1.h>
#include <wrl/client.h>
#include <memory>

namespace rk {
// Switches the immediate context to an isolated D3D11.1 state object for
// injected work, then restores the exact previous state object on destruction.
// GPU commands are still ordered on the original immediate context.
class D3D11StateScope final {
public:
    static Result<std::unique_ptr<D3D11StateScope>> begin(ID3D11DeviceContext* context);
    ~D3D11StateScope();
    D3D11StateScope(const D3D11StateScope&)=delete;
    D3D11StateScope& operator=(const D3D11StateScope&)=delete;
private:
    explicit D3D11StateScope(Microsoft::WRL::ComPtr<ID3D11DeviceContext1> context,
        Microsoft::WRL::ComPtr<ID3DDeviceContextState> previous) noexcept;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext1> context_;
    Microsoft::WRL::ComPtr<ID3DDeviceContextState> previous_;
};
}
