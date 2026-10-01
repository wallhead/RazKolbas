#include "rk/FgCameraFramePairer.hpp"
#include <utility>

namespace rk {
void FgCameraFramePairer::beforeUi(std::uint64_t source,
    std::optional<FgCameraWrite> write) {
    if(!source) {clear();return;}
    beforeFrame_=source;
    beforeWrite_=std::move(write);
}
std::optional<FgCameraProducerSample> FgCameraFramePairer::beforePresent(
    std::uint64_t source,std::optional<FgCameraWrite> write) {
    if(!source||beforeFrame_!=source) {clear();return std::nullopt;}
    std::optional<FgGameCameraSample> decoded;
    if(write) {
        const auto parsed=decodeFgGameCameraBuffer(write->bytes);
        if(const auto* camera=std::get_if<FgGameCameraSample>(&parsed))decoded=*camera;
    }
    const bool stable=beforeWrite_&&write&&
        sameFgCameraWrite(*beforeWrite_,*write);
    const bool fresh=priorWrite_&&write&&
        canCompareFgCameraWriteHistory(*priorWrite_,*write);
    const bool consecutive=priorFrame_&&source>priorFrame_&&
        source-priorFrame_==1&&priorFresh_&&priorCamera_&&decoded&&
        fgGameCameraConsecutive(*priorCamera_,*decoded);
    std::optional<FgCameraProducerSample> paired;
    if(stable&&fresh&&consecutive)
        paired=FgCameraProducerSample{source,write->revision,
            write->generation,*decoded};
    priorFrame_=source;
    priorWrite_=std::move(write);
    priorCamera_=std::move(decoded);
    priorFresh_=fresh&&stable;
    beforeFrame_=0;beforeWrite_.reset();
    return paired;
}
void FgCameraFramePairer::clear() noexcept {
    beforeFrame_=0;priorFrame_=0;
    beforeWrite_.reset();priorWrite_.reset();priorCamera_.reset();
    priorFresh_=false;
}
}
