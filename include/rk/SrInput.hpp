#pragma once
#include "rk/Result.hpp"
#include <d3d11.h>
#include <wrl/client.h>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>

namespace rk {
struct SrSourceRegion {
    UINT left{},top{},width{},height{};
    bool operator==(const SrSourceRegion&) const noexcept=default;
};
// Owns one source frame's NGX-compatible resources. The caller must keep this
// object alive until all queued GPU work that reads or writes it has retired.
class PreparedSrInputs {
public:
    PreparedSrInputs(Microsoft::WRL::ComPtr<ID3D11Texture2D> color,
        Microsoft::WRL::ComPtr<ID3D11Texture2D> motion,
        Microsoft::WRL::ComPtr<ID3D11Texture2D> depth,
        Microsoft::WRL::ComPtr<ID3D11Texture2D> output,UINT width,UINT height,
        UINT outputWidth,UINT outputHeight,SrSourceRegion sourceRegion,
        Microsoft::WRL::ComPtr<ID3D11ComputeShader> depthCropShader={},
        Microsoft::WRL::ComPtr<ID3D11Texture2D> depthSnapshot={},
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> depthView={},
        Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> depthTarget={},
        Microsoft::WRL::ComPtr<ID3D11Buffer> cropConstants={}) noexcept;
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
    SrSourceRegion sourceRegion() const noexcept { return sourceRegion_; }
    // Refreshes a slot whose R24G8 source depth was normalized to R32_FLOAT.
    // The three source identities may vary, but their device, formats and
    // extents must remain compatible with the prepared slot.
    Result<bool> refreshConvertedDepth(ID3D11DeviceContext* context,
        std::span<ID3D11Texture2D* const> sources);
private:
    Microsoft::WRL::ComPtr<ID3D11Texture2D> color_,motion_,depth_,output_;
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> depthCropShader_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> depthSnapshot_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> depthView_;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> depthTarget_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> cropConstants_;
    UINT width_{},height_{},outputWidth_{},outputHeight_{};
    SrSourceRegion sourceRegion_{};
};

struct DepthSampleStats {
    unsigned distinct{},nonFar{};
    bool worldLike() const noexcept { return distinct>=16&&nonFar>=16; }
};
// Main-menu and loading scenes can expose only cleared depth for several
// minutes. Keep the first probe window responsive, then reduce the diagnostic
// readback rate without permanently disabling DLAA before a save is loaded.
inline constexpr std::uint64_t worldDepthProbeRetryDelay(unsigned attempts) noexcept {
    return attempts<24?600:1800;
}
struct ColorSampleStats {
    unsigned nonBlack{},distinct{};
    bool sceneLike() const noexcept { return nonBlack>=16&&distinct>=16; }
};
// Requires two recent samples where both the current colour and depth look
// populated. This avoids treating a black loading/fade frame with stale or
// already-populated depth as a valid temporal-SR source.
class OwnedSceneAdmissionGate {
public:
    bool needsSample(std::uint64_t frame,std::uint64_t generation) noexcept {
        if(generation_!=generation||frame<lastSample_) {
            generation_=generation;
            lastSample_=0;
            consecutiveSceneSamples_=0;
        }
        if(lastSample_&&frame==lastSample_)return false;
        return !lastSample_||frame-lastSample_>=30;
    }
    void record(std::uint64_t frame,std::optional<ColorSampleStats> color,
        std::optional<DepthSampleStats> depth) noexcept {
        lastSample_=frame;
        if(color&&depth&&color->sceneLike()&&depth->worldLike()) {
            if(consecutiveSceneSamples_<2)++consecutiveSceneSamples_;
        } else consecutiveSceneSamples_=0;
    }
    bool ready() const noexcept { return consecutiveSceneSamples_>=2; }
private:
    std::uint64_t generation_{},lastSample_{};
    unsigned consecutiveSceneSamples_{};
};
// A conservative admission gate for the owned reduced-scene route. Depth
// readiness is sampled periodically and reset on a new resource generation.
class WorldDepthGate {
public:
    bool needsSample(std::uint64_t frame,std::uint64_t generation) noexcept {
        if(generation_!=generation||frame<lastSample_) {
            generation_=generation;
            lastSample_=0;
            consecutiveWorldSamples_=0;
        }
        if(lastSample_&&frame==lastSample_)return false;
        return !lastSample_||frame-lastSample_>=30;
    }
    void record(std::uint64_t frame,std::optional<DepthSampleStats> sample) noexcept {
        lastSample_=frame;
        if(sample&&sample->worldLike()) {
            if(consecutiveWorldSamples_<2)++consecutiveWorldSamples_;
        } else consecutiveWorldSamples_=0;
    }
    bool ready() const noexcept { return consecutiveWorldSamples_>=2; }
private:
    std::uint64_t generation_{},lastSample_{};
    unsigned consecutiveWorldSamples_{};
};
// Samples a fixed 10x10 grid of raw Skyrim R24G8 depth words. This is only a
// bounded scene-readiness gate: it does not infer linearization or guide units.
Result<DepthSampleStats> sampleWorldDepth(std::span<const std::uint8_t> pixels,
    UINT width,UINT height,std::size_t rowBytes);
// Samples a 16x16 grid from an RGBA8 scene. This is a bounded transition gate,
// not an assertion that the source is HUD-free or at the final SR boundary.
Result<ColorSampleStats> sampleWorldColor(std::span<const std::uint8_t> pixels,
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
// Extracts a caller-verified active world rectangle from matching full-size
// SDR colour, motion and depth textures. Depth is converted to normalized
// R32_FLOAT because D3D11 forbids partial depth-stencil copies. This does not
// establish that Skyrim rendered only that rectangle or that NGX accepts this
// converted depth; both must be verified before SR submission.
Result<PreparedSrInputs> prepareSdrSrInputsFromRegion(ID3D11DeviceContext* context,
    std::span<ID3D11Texture2D* const> sources,UINT renderWidth,UINT renderHeight,
    UINT outputWidth,UINT outputHeight);
// Retains the original rectangle's origin in PreparedSrInputs metadata while
// the owned cropped textures use (0,0) as their NGX input origin.
Result<PreparedSrInputs> prepareSdrSrInputsFromRegion(ID3D11DeviceContext* context,
    std::span<ID3D11Texture2D* const> sources,SrSourceRegion region,
    UINT outputWidth,UINT outputHeight);
// The scene is already a genuinely reduced owned texture. Motion and depth
// may retain display-sized allocations with valid data in the top-left render
// rectangle, or both may share the reduced extent.
Result<PreparedSrInputs> prepareSdrSrInputsFromOwnedScene(ID3D11DeviceContext* context,
    std::span<ID3D11Texture2D* const> sources,UINT outputWidth,UINT outputHeight);
}
