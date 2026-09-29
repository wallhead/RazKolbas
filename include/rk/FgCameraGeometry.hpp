#pragma once
#include "rk/FgGameCameraBuffer.hpp"

namespace rk {
// Row-vector, row-major candidate transforms for Streamline common constants.
// Callers must still validate the game's matrix/jitter phase and all guide
// semantics before submitting these matrices to a provider.
struct FgCameraTransforms {
    std::array<float,16> cameraViewToClip{},clipToCameraView{},
        clipToPrevClip{},prevClipToClip{};
};
Result<FgCameraTransforms> deriveFgCameraTransforms(
    const FgGameCameraSample& camera);
}
