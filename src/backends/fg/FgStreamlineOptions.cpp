#include "rk/FgStreamlineOptions.hpp"

namespace rk {
Result<sl::DLSSGOptions> makeFgStreamlineOptionsOn(
    const FgPreparedSubmission& frame,DXGI_FORMAT lowerBackbufferFormat) {
    if(!frame.source||!frame.generation||!frame.presentToken||
       !frame.resetEpoch||!frame.render.valid()||!frame.display.valid()||
       !frame.copyTicket.producer||!frame.copyTicket.copy||
       frame.swapBufferCount<2||frame.swapBufferCount>4||
       frame.physicalOutputIndex>=frame.swapBufferCount)
        return Error{ErrorCode::InvalidInput,
            "FG Streamline options require a prepared real frame and flip swap"};
    for(std::size_t i=0;i<frame.resources.size();++i) {
        if(!frame.resources[i])
            return Error{ErrorCode::Conflict,
                "FG Streamline options require five retained input resources"};
        const auto desc=frame.resources[i]->GetDesc();
        const auto expected=(i==1||i==2)?frame.render:frame.display;
        if(desc.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D||
           desc.Width!=expected.width||desc.Height!=expected.height||
           desc.SampleDesc.Count!=1||desc.Format==DXGI_FORMAT_UNKNOWN)
            return Error{ErrorCode::Conflict,
                "FG Streamline input format or extent changed after preparation"};
    }
    const auto color=frame.resources[0]->GetDesc().Format;
    if(lowerBackbufferFormat==DXGI_FORMAT_UNKNOWN||
       color!=lowerBackbufferFormat)
        return Error{ErrorCode::Conflict,
            "FG Streamline final colour differs from the lower backbuffer format"};
    sl::DLSSGOptions out{};
    out.mode=sl::DLSSGMode::eOn;
    out.numFramesToGenerate=1;
    out.numBackBuffers=frame.swapBufferCount;
    out.mvecDepthWidth=frame.render.width;
    out.mvecDepthHeight=frame.render.height;
    out.colorWidth=frame.display.width;
    out.colorHeight=frame.display.height;
    out.colorBufferFormat=static_cast<std::uint32_t>(color);
    out.mvecBufferFormat=static_cast<std::uint32_t>(
        frame.resources[2]->GetDesc().Format);
    out.depthBufferFormat=static_cast<std::uint32_t>(
        frame.resources[1]->GetDesc().Format);
    out.hudLessBufferFormat=static_cast<std::uint32_t>(
        frame.resources[3]->GetDesc().Format);
    out.uiBufferFormat=static_cast<std::uint32_t>(
        frame.resources[4]->GetDesc().Format);
    out.enableUserInterfaceRecomposition=sl::Boolean::eTrue;
    return out;
}
}
