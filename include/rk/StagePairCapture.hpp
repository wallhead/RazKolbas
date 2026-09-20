#pragma once
#include "rk/FrameProbe.hpp"
#include <filesystem>

namespace rk {
// CPU-only evidence from one world frame, captured on the immediate context.
// The paired backbuffer observations bracket game callbacks before ENB Present.
class StagePairCapture final {
public:
    static Result<StagePairCapture> capturePostWorld(ID3D11DeviceContext* context,
        ID3D11Texture2D* scene,ID3D11Texture2D* backbuffer);
    Result<bool> captureBeforePresent(ID3D11DeviceContext* context,
        ID3D11Texture2D* backbuffer);
    Result<bool> save(const std::filesystem::path& directory) const;
    bool complete() const noexcept { return complete_; }
    const ProbeImage& scene() const noexcept { return scene_; }
    const ProbeImage& postWorld() const noexcept { return postWorld_; }
    const ProbeImage& beforePresent() const noexcept { return beforePresent_; }
private:
    ProbeImage scene_,postWorld_,beforePresent_;
    std::uintptr_t backbufferIdentity_{};
    bool complete_{};
};
}
