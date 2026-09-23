#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <cstddef>
#include <span>
#include <vector>

namespace rk {
// Replaces only zero-bias samplers whose descriptor requests anisotropy.
// Original objects are never modified; replacements are retained for the
// process-lifetime context hook and reused by canonical COM identity.
class SamplerBiasCache final {
public:
    HRESULT configure(ID3D11DeviceContext* context,float bias) noexcept;
    bool remap(ID3D11DeviceContext* context,
        std::span<ID3D11SamplerState* const> input,
        std::span<ID3D11SamplerState*> output) noexcept;
    std::size_t replacementCount() const noexcept { return entries_.size(); }
    float bias() const noexcept { return bias_; }
private:
    struct Entry {
        Microsoft::WRL::ComPtr<IUnknown> source;
        Microsoft::WRL::ComPtr<ID3D11SamplerState> replacement;
    };
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<IUnknown> deviceIdentity_;
    std::vector<Entry> entries_;
    float bias_{};
};
}
