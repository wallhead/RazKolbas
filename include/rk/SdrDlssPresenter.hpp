#pragma once
#include "rk/Result.hpp"
#include "rk/SrInput.hpp"
#include <d3d11.h>
#include <wrl/client.h>
#include <Windows.h>
#include <array>
#include <cstdint>
#include <optional>

struct NVSDK_NGX_Handle;
struct NVSDK_NGX_Parameter;

namespace rk {
// Experimental SDR DLAA presenter. Caller holds the verified Skyrim renderer
// lock and supplies one real HUD-free source frame per call. A busy resource
// slot skips a frame, leaving Skyrim's native image intact.
class SdrDlssPresenter final {
public:
    Result<bool> render(ID3D11Device* device,ID3D11DeviceContext* context,
        ID3D11Texture2D* backbuffer,ID3D11Texture2D* motion,ID3D11Texture2D* depth);
    // Returns false while GPU work still owns a slot. Call again after a
    // present/flush, then release NGX before destroying the D3D11 device.
    Result<bool> stop(ID3D11DeviceContext* context);
    std::uint64_t submittedFrames() const noexcept { return submittedFrames_; }
private:
    struct Slot {
        std::optional<PreparedSrInputs> frame;
        Microsoft::WRL::ComPtr<ID3D11Query> completion;
        bool inFlight{};
    };
    Result<bool> initialize(ID3D11Device* device,ID3D11DeviceContext* context,
        UINT width,UINT height);
    std::array<Slot,3> slots_;
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    HANDLE runtimeFile_{INVALID_HANDLE_VALUE};
    NVSDK_NGX_Parameter* parameters_{};
    NVSDK_NGX_Handle* feature_{};
    UINT width_{},height_{};
    std::uint64_t submittedFrames_{};
    unsigned nextSlot_{};
    bool initialized_{};
};
}
