#include "rk/FgStreamlineSubmit.hpp"

namespace rk {
Result<bool> submitFgStreamlineInputs(
    const FgStreamlineFrameInputs& packet,
    const FgStreamlineTokenBinding& binding,
    const sl::ViewportHandle& viewport,const FgStreamlineCalls& calls) {
    if(!packet.source||!packet.generation||!packet.presentToken||
       !packet.resetEpoch||!packet.tags||!binding.token||
       binding.source!=packet.source||
       binding.generation!=packet.generation||
       binding.presentToken!=packet.presentToken||
       binding.resetEpoch!=packet.resetEpoch||
       packet.tags->presentToken()!=packet.presentToken)
        return Error{ErrorCode::Conflict,
            "FG Streamline token does not match the prepared real frame"};
    if(!calls.setConstants||!calls.setTags)
        return Error{ErrorCode::Unavailable,
            "FG Streamline submission functions are unavailable"};
    if(calls.setConstants(packet.constants,*binding.token,viewport)!=
       sl::Result::eOk)
        return Error{ErrorCode::Unavailable,
            "FG Streamline rejected camera constants"};
    if(calls.setTags(*binding.token,viewport,packet.tags->tags(),
           static_cast<std::uint32_t>(FgStreamlineTagBundle::tagCount),
           nullptr)!=sl::Result::eOk)
        return Error{ErrorCode::Unavailable,
            "FG Streamline rejected frame input tags"};
    return true;
}
}
