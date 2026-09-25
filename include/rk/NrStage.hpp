#pragma once
#include "rk/Result.hpp"
#include "rk/Settings.hpp"
#include "rk/SrInput.hpp"
#include <d3d11.h>
#include <memory>

namespace rk {
// Exact-runtime experimental DLSS Neural Rendering stage. A successful call
// replaces PreparedSrInputs::color in place before DLSS SR consumes it. Every
// failure leaves the original prepared colour untouched.
class NrStage final {
public:
    NrStage();
    ~NrStage();
    NrStage(const NrStage&)=delete;
    NrStage& operator=(const NrStage&)=delete;
    Result<bool> configure(const Settings& settings);
    Result<bool> process(ID3D11Device* device,ID3D11DeviceContext* context,
        PreparedSrInputs& frame,bool reset);
    Result<bool> stop();
    bool enabled() const noexcept;
    std::uint64_t submittedFrames() const noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
