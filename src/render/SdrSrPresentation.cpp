#include "rk/SdrSrPresentation.hpp"
#include <utility>

namespace rk {
SdrSrFrameResult::SdrSrFrameResult(SdrSrFrameMode mode,
    std::optional<SpatialFallbackFrame> fallback,
    std::optional<Error> providerFailure) noexcept:
    mode_(mode),fallback_(std::move(fallback)),
    providerFailure_(std::move(providerFailure)) {}

Result<bool> SdrSrFrameResult::complete(ID3D11DeviceContext* context) const {
    if(mode_==SdrSrFrameMode::SpatialFallback) {
        if(!fallback_)return Error{ErrorCode::Conflict,"Fallback result lost its retained frame"};
        return fallback_->complete(context);
    }
    return true; // The successful provider owns its own slot retirement.
}

Result<SdrSrFrameResult> presentSdrSrFrame(ID3D11DeviceContext* context,
    ID3D11Texture2D* scene,ID3D11Texture2D* display,
    const std::function<Result<bool>()>& submitProvider) {
    if(!context||!scene||!display||!submitProvider)
        return Error{ErrorCode::InvalidInput,"SR presentation needs a scene, display and provider"};
    const auto provider=submitProvider();
    if(const auto submitted=std::get_if<bool>(&provider);submitted&&*submitted)
        return SdrSrFrameResult{SdrSrFrameMode::Provider,std::nullopt,std::nullopt};
    std::optional<Error> failure;
    if(const auto error=std::get_if<Error>(&provider)) {
        if(error->code!=ErrorCode::Unavailable&&error->code!=ErrorCode::Unsupported)
            return *error;
        failure=*error;
    } else failure=Error{ErrorCode::Unavailable,"SR provider slot is busy"};
    auto fallback=produceSdrSpatialFallbackToDisplay(context,scene,display);
    if(const auto error=std::get_if<Error>(&fallback))return *error;
    return SdrSrFrameResult{SdrSrFrameMode::SpatialFallback,
        std::move(std::get<SpatialFallbackFrame>(fallback)),std::move(failure)};
}
}
