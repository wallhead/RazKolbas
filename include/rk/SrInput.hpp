#pragma once
#include "rk/Result.hpp"
#include <d3d11.h>
#include <wrl/client.h>
#include <cstdint>
#include <span>

namespace rk {
// Owns one source frame's NGX-compatible resources. The caller must keep this
// object alive until all queued GPU work that reads or writes it has retired.
class PreparedSrInputs {
public:
    PreparedSrInputs(Microsoft::WRL::ComPtr<ID3D11Texture2D> color,
        Microsoft::WRL::ComPtr<ID3D11Texture2D> motion,
        Microsoft::WRL::ComPtr<ID3D11Texture2D> depth,
        Microsoft::WRL::ComPtr<ID3D11Texture2D> output,UINT width,UINT height) noexcept;
    PreparedSrInputs(const PreparedSrInputs&)=delete;
    PreparedSrInputs& operator=(const PreparedSrInputs&)=delete;
    PreparedSrInputs(PreparedSrInputs&&) noexcept=default;
    PreparedSrInputs& operator=(PreparedSrInputs&&) noexcept=default;
    ID3D11Texture2D* color() const noexcept { return color_.Get(); }
    ID3D11Texture2D* motion() const noexcept { return motion_.Get(); }
    ID3D11Texture2D* depth() const noexcept { return depth_.Get(); }
    ID3D11Texture2D* output() const noexcept { return output_.Get(); }
    UINT width() const noexcept { return width_; }
    UINT height() const noexcept { return height_; }
private:
    Microsoft::WRL::ComPtr<ID3D11Texture2D> color_,motion_,depth_,output_;
    UINT width_{},height_{};
};

// The caller proves exclusive access to the actual immediate context and
// renderer-owned source textures. All validation/allocation precedes CopyResource.
// Copies are ordered with subsequent NGX work on the same context; this call
// does not wait for GPU completion or alter Skyrim's source textures.
Result<PreparedSrInputs> prepareSrInputs(ID3D11DeviceContext* context,
    std::span<ID3D11Texture2D* const> sources);
}
