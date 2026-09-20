#pragma once
#include "rk/Result.hpp"
#include "rk/SrInput.hpp"
#include "rk/JitterContract.hpp"
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
        ID3D11Texture2D* backbuffer,ID3D11Texture2D* motion,ID3D11Texture2D* depth,
        NgxJitter jitter);
    // The scene and guides have the reduced render extent; the destination
    // keeps the display extent. An error leaves destination publication to
    // the caller's display-sized fallback path.
    Result<bool> renderSr(ID3D11Device* device,ID3D11DeviceContext* context,
        ID3D11Texture2D* scene,ID3D11Texture2D* motion,ID3D11Texture2D* depth,
        ID3D11Texture2D* backbuffer,NgxJitter jitter);
    void requestReset() noexcept { resetPending_=true; }
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
    Result<bool> renderFrame(ID3D11Device* device,ID3D11DeviceContext* context,
        ID3D11Texture2D* scene,ID3D11Texture2D* motion,ID3D11Texture2D* depth,
        ID3D11Texture2D* backbuffer,NgxJitter jitter,bool reduced);
    Result<bool> initialize(ID3D11Device* device,ID3D11DeviceContext* context,
        UINT width,UINT height,UINT displayWidth,UINT displayHeight,bool reduced);
    std::array<Slot,3> slots_;
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    HANDLE runtimeFile_{INVALID_HANDLE_VALUE};
    NVSDK_NGX_Parameter* parameters_{};
    NVSDK_NGX_Handle* feature_{};
    UINT width_{},height_{},displayWidth_{},displayHeight_{};
    std::uint64_t submittedFrames_{};
    unsigned nextSlot_{};
    bool initialized_{};
    bool reduced_{};
    bool resetPending_{};
};
}
