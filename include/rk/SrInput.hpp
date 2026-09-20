#pragma once
#include "rk/Result.hpp"
#include <d3d11.h>
#include <wrl/client.h>
#include <cstdint>
#include <span>
#include <utility>

namespace rk {
// Owns one source frame's NGX-compatible resources. The caller must keep this
// object alive until all queued GPU work that reads or writes it has retired.
class PreparedSrInputs {
public:
    PreparedSrInputs(Microsoft::WRL::ComPtr<ID3D11Texture2D> color,
        Microsoft::WRL::ComPtr<ID3D11Texture2D> motion,
        Microsoft::WRL::ComPtr<ID3D11Texture2D> depth,
        Microsoft::WRL::ComPtr<ID3D11Texture2D> output,UINT width,UINT height,
        UINT outputWidth,UINT outputHeight) noexcept;
    PreparedSrInputs(const PreparedSrInputs&)=delete;
    PreparedSrInputs& operator=(const PreparedSrInputs&)=delete;
    PreparedSrInputs(PreparedSrInputs&&) noexcept=default;
    PreparedSrInputs& operator=(PreparedSrInputs&&) noexcept=default;
    ID3D11Texture2D* color() const noexcept { return color_.Get(); }
    ID3D11Texture2D* motion() const noexcept { return motion_.Get(); }
    ID3D11Texture2D* depth() const noexcept { return depth_.Get(); }
    ID3D11Texture2D* output() const noexcept { return output_.Get(); }
    Microsoft::WRL::ComPtr<ID3D11Texture2D> takeOutput() noexcept { return std::move(output_); }
    UINT width() const noexcept { return width_; }
    UINT height() const noexcept { return height_; }
    UINT outputWidth() const noexcept { return outputWidth_; }
    UINT outputHeight() const noexcept { return outputHeight_; }
private:
    Microsoft::WRL::ComPtr<ID3D11Texture2D> color_,motion_,depth_,output_;
    UINT width_{},height_{},outputWidth_{},outputHeight_{};
};

struct DepthSampleStats {
    unsigned distinct{},nonFar{};
    bool worldLike() const noexcept { return distinct>=16&&nonFar>=16; }
};
// Samples a fixed 10x10 grid of raw Skyrim R24G8 depth words. This is only a
// bounded scene-readiness gate: it does not infer linearization or guide units.
Result<DepthSampleStats> sampleWorldDepth(std::span<const std::uint8_t> pixels,
    UINT width,UINT height,std::size_t rowBytes);

// The caller proves exclusive access to the actual immediate context and
// renderer-owned source textures. All validation/allocation precedes CopyResource.
// Copies are ordered with subsequent NGX work on the same context; this call
// does not wait for GPU completion or alter Skyrim's source textures.
Result<PreparedSrInputs> prepareSrInputs(ID3D11DeviceContext* context,
    std::span<ID3D11Texture2D* const> sources);
// The three source textures share the actual world render extent. Output is
// allocated at the separate display extent; this does not resize game targets.
Result<PreparedSrInputs> prepareSrInputsForDisplay(ID3D11DeviceContext* context,
    std::span<ID3D11Texture2D* const> sources,UINT outputWidth,UINT outputHeight);
// Uses the already tone-mapped, HUD-free SDR scene in an RTV-only backbuffer.
// The owned copy is shader-readable; the original backbuffer is never bound
// to NGX and remains unchanged until an explicit presentation decision.
Result<PreparedSrInputs> prepareSdrSrInputs(ID3D11DeviceContext* context,
    std::span<ID3D11Texture2D* const> sources);
Result<PreparedSrInputs> prepareSdrSrInputsForDisplay(ID3D11DeviceContext* context,
    std::span<ID3D11Texture2D* const> sources,UINT outputWidth,UINT outputHeight);
}
