#include "rk/FgStreamlineFrameInputs.hpp"
#include <utility>

namespace rk {
Result<FgStreamlineFrameInputs> prepareFgStreamlineFrameInputs(
    const FgSourceFrame& frame,const FgPreparedSubmission& prepared,
    const FgStreamlineReadStates& states,
    DXGI_FORMAT lowerBackbufferFormat) {
    if(!frame.worldActive||frame.loading||frame.paused||
       !frame.ownerReady||!frame.cameraValid||
       frame.source!=prepared.source||
       frame.generation!=prepared.generation||
       frame.presentToken!=prepared.presentToken||
       frame.resetEpoch!=prepared.resetEpoch||
       frame.render.width!=prepared.render.width||
       frame.render.height!=prepared.render.height||
       frame.display.width!=prepared.display.width||
       frame.display.height!=prepared.display.height)
        return Error{ErrorCode::Conflict,
            "FG Streamline inputs and real world frame do not match"};
    const auto constants=makeFgStreamlineConstants(frame,prepared.camera);
    if(const auto error=std::get_if<Error>(&constants))return *error;
    const auto options=makeFgStreamlineOptionsOn(prepared,
        lowerBackbufferFormat);
    if(const auto error=std::get_if<Error>(&options))return *error;
    auto tags=makeFgStreamlineTags(prepared,states);
    if(const auto error=std::get_if<Error>(&tags))return *error;
    FgStreamlineFrameInputs out{};
    out.source=frame.source;out.generation=frame.generation;
    out.presentToken=frame.presentToken;
    out.resetEpoch=frame.resetEpoch;
    out.constants=std::get<sl::Constants>(constants);
    out.options=std::get<sl::DLSSGOptions>(options);
    out.tags=std::move(std::get<std::unique_ptr<FgStreamlineTagBundle>>(tags));
    return out;
}
}
