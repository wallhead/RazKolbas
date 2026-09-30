#pragma once
#include "rk/FgCameraWriteCapture.hpp"
#include "rk/Result.hpp"
#include <d3d11.h>
#include <string_view>

namespace rk {
bool isFgCameraWriteBuffer(const D3D11_BUFFER_DESC& desc) noexcept;
struct FgCameraWriter {std::uint64_t mapCaller{},unmapCaller{};};
struct FgCameraWriteObservation {
    std::optional<FgCameraWrite> latest;
    std::uint64_t completed{},rejected{},writerOverflow{};
    std::array<FgCameraWriter,16> writers{};
    unsigned writerCount{};
};
// Startup only, after the caller verifies Skyrim's loaded file and exact
// renderer context. Both ENB slots and their loaded code are validated before
// either pointer changes. Modules, context and callback state stay pinned.
Result<bool> installFgCameraWriteHooks(ID3D11DeviceContext* context,
    std::uintptr_t verifiedGameBase,std::string_view verifiedGameHash,
    std::string_view disabledPatchIds);
FgCameraWriteObservation snapshotFgCameraWrites() noexcept;
}
