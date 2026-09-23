#pragma once
#include "rk/Result.hpp"
#include <d3d11.h>
#include <wrl/client.h>
#include <array>

namespace rk {
// Applies a native-resolution, contrast-limited five-tap detail correction to
// an SDR provider result. The caller supplies the active destination RTV so
// native UI can be composed after this pass.
class SdrPostSharpenPass final {
public:
    Result<bool> apply(ID3D11DeviceContext* context,ID3D11Texture2D* destination,
        ID3D11Texture2D* source,float strength);
    void reset() noexcept;
private:
    struct CachedView {
        Microsoft::WRL::ComPtr<IUnknown> identity;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
    };
    Result<bool> ensureResources(ID3D11Device* device);
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> constants_;
    std::array<CachedView,3> sourceViews_{};
    unsigned nextView_{};
};
}
