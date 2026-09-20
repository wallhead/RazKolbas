#pragma once
#include "rk/Result.hpp"
#include "rk/SpatialFallback.hpp"
#include <d3d11.h>
#include <functional>
#include <optional>

namespace rk {
enum class SdrSrFrameMode { Provider, SpatialFallback };

class SdrSrFrameResult final {
public:
    SdrSrFrameResult(SdrSrFrameMode mode,
        std::optional<SpatialFallbackFrame> fallback,
        std::optional<Error> providerFailure) noexcept;
    SdrSrFrameMode mode() const noexcept { return mode_; }
    const std::optional<Error>& providerFailure() const noexcept { return providerFailure_; }
    Result<bool> complete(ID3D11DeviceContext* context) const;
private:
    SdrSrFrameMode mode_{};
    std::optional<SpatialFallbackFrame> fallback_;
    std::optional<Error> providerFailure_;
};

// The provider callback returns true only after it has copied a valid
// display-sized result to the destination. Busy or recoverable failure uses
// the preserved reduced scene for a real spatial output. The returned frame
// must be retained until complete() reports true.
Result<SdrSrFrameResult> presentSdrSrFrame(ID3D11DeviceContext* context,
    ID3D11Texture2D* scene,ID3D11Texture2D* display,
    const std::function<Result<bool>()>& submitProvider);
}
