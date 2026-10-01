#pragma once
#include "rk/FgCameraWriteCapture.hpp"
#include "rk/FgGameCameraBuffer.hpp"
#include <optional>

namespace rk {
struct FgCameraProducerSample {
    std::uint64_t source{},revision{},writeGeneration{};
    FgGameCameraSample camera{};
};

// A camera revision is only a producer write. Pairing additionally requires
// an unchanged pre-UI/pre-Present sample and two fresh adjacent world frames.
class FgCameraFramePairer {
public:
    void beforeUi(std::uint64_t source,std::optional<FgCameraWrite> write);
    std::optional<FgCameraProducerSample> beforePresent(
        std::uint64_t source,std::optional<FgCameraWrite> write);
    void clear() noexcept;
private:
    std::uint64_t beforeFrame_{},priorFrame_{};
    std::optional<FgCameraWrite> beforeWrite_,priorWrite_;
    std::optional<FgGameCameraSample> priorCamera_;
    bool priorFresh_{};
};
}
