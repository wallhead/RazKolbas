#include "rk/FgStreamlineTags.hpp"

namespace rk {
namespace {
constexpr auto kShaderRead=static_cast<D3D12_RESOURCE_STATES>(
    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE|
    D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
bool validTexture(ID3D12Resource* resource,Extent expected) noexcept {
    if(!resource||!expected.valid())return false;
    const auto desc=resource->GetDesc();
    return desc.Dimension==D3D12_RESOURCE_DIMENSION_TEXTURE2D&&
        desc.Width==expected.width&&desc.Height==expected.height&&
        desc.DepthOrArraySize==1&&desc.MipLevels==1&&
        desc.SampleDesc.Count==1&&desc.SampleDesc.Quality==0&&
        desc.Format!=DXGI_FORMAT_UNKNOWN;
}
}

FgStreamlineTagBundle::FgStreamlineTagBundle(
    const FgPreparedSubmission& submission,
    const FgStreamlineReadStates& states):
    presentToken_(submission.presentToken),
    owned_(submission.resources),
    renderExtent_{0,0,submission.render.width,submission.render.height},
    displayExtent_{0,0,submission.display.width,submission.display.height},
    slResources_{{
        {sl::ResourceType::eTex2d,owned_[1].Get(),
            static_cast<std::uint32_t>(states.actual[1])},
        {sl::ResourceType::eTex2d,owned_[2].Get(),
            static_cast<std::uint32_t>(states.actual[2])},
        {sl::ResourceType::eTex2d,owned_[3].Get(),
            static_cast<std::uint32_t>(states.actual[3])},
        {sl::ResourceType::eTex2d,owned_[4].Get(),
            static_cast<std::uint32_t>(states.actual[4])}}},
    tags_{{
        {&slResources_[0],sl::kBufferTypeDepth,
            sl::eValidUntilPresent,&renderExtent_},
        {&slResources_[1],sl::kBufferTypeMotionVectors,
            sl::eValidUntilPresent,&renderExtent_},
        {&slResources_[2],sl::kBufferTypeHUDLessColor,
            sl::eValidUntilPresent,&displayExtent_},
        {&slResources_[3],sl::kBufferTypeUIColorAndAlpha,
            sl::eValidUntilPresent,&displayExtent_},
        {nullptr,sl::kBufferTypeBackbuffer,
            sl::eValidUntilPresent,&displayExtent_}}} {}

Result<std::unique_ptr<FgStreamlineTagBundle>> makeFgStreamlineTags(
    const FgPreparedSubmission& submission,
    const FgStreamlineReadStates& states) {
    if(!submission.source||!submission.generation||!submission.presentToken||
       !submission.resetEpoch||!submission.render.valid()||
       !submission.display.valid()||!submission.swapBufferCount||
       submission.physicalOutputIndex>=submission.swapBufferCount||
       !submission.copyTicket.producer||!submission.copyTicket.copy||
       states.completedCopy.producer!=submission.copyTicket.producer||
       states.completedCopy.copy!=submission.copyTicket.copy)
        return Error{ErrorCode::Conflict,
            "FG Streamline tags require the current copied real frame"};
    for(std::size_t i=0;i<submission.resources.size();++i) {
        const auto expected=(i==1||i==2)?submission.render:
            submission.display;
        if(!validTexture(submission.resources[i].Get(),expected))
            return Error{ErrorCode::InvalidInput,
                "FG Streamline tag resource has an invalid descriptor"};
        if(states.actual[i]!=kShaderRead)
            return Error{ErrorCode::Conflict,
                "FG Streamline tag resource is not in the declared D3D12 shader-read state"};
    }
    return std::unique_ptr<FgStreamlineTagBundle>(
        new FgStreamlineTagBundle(submission,states));
}
}
