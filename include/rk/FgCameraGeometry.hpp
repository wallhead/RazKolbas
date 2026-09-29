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

// Skyrim 1.6.1170 world-route geometry only. Guide conventions, same-frame
// jitter and a real-frame token are separate admission requirements.
struct FgCameraCalibration {
    FgCameraTransforms transforms{};
    std::array<float,3> position{},right{},up{},forward{};
    float nearPlane{},farPlane{},verticalFovRadians{},
        horizontalFovRadians{},aspectRatio{};
};
Result<FgCameraCalibration> deriveFgCameraCalibration(
    const FgGameCameraSample& camera);
}
