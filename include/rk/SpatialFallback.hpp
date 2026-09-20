#pragma once
#include "rk/Result.hpp"
#include <d3d11.h>
#include <wrl/client.h>

namespace rk {
// Owns the source and, for offscreen fallback, the output until the GPU event
// completes. A direct display fallback does not retain the swap buffer.
class SpatialFallbackFrame final {
public:
    SpatialFallbackFrame(Microsoft::WRL::ComPtr<ID3D11Texture2D> source,
        Microsoft::WRL::ComPtr<ID3D11Texture2D> output,
        Microsoft::WRL::ComPtr<ID3D11Query> completion,UINT width,UINT height) noexcept;
    ID3D11Texture2D* output() const noexcept { return output_.Get(); }
    UINT width() const noexcept { return width_; }
    UINT height() const noexcept { return height_; }
    Result<bool> complete(ID3D11DeviceContext* context) const;
private:
    Microsoft::WRL::ComPtr<ID3D11Texture2D> source_,output_;
    Microsoft::WRL::ComPtr<ID3D11Query> completion_;
    UINT width_{},height_{};
};

Result<SpatialFallbackFrame> produceSpatialFallback(ID3D11DeviceContext* context,
    ID3D11Texture2D* source,UINT displayWidth,UINT displayHeight);
// Preserved HUD-free SDR colour receives a correctly sized RGBA8 fallback if
// a reduced-render SR evaluation cannot be published.
Result<SpatialFallbackFrame> produceSdrSpatialFallback(ID3D11DeviceContext* context,
    ID3D11Texture2D* source,UINT displayWidth,UINT displayHeight);
// Draws a reduced SDR scene directly into the currently bound display target,
// retaining the source until the draw has completed.
Result<SpatialFallbackFrame> produceSdrSpatialFallbackToDisplay(
    ID3D11DeviceContext* context,ID3D11Texture2D* source,
    ID3D11Texture2D* display);
}
