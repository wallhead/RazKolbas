#pragma once
#include "rk/Result.hpp"
#include "rk/SrInput.hpp"
#include <d3d11.h>
#include <wrl/client.h>
#include <Windows.h>
#include <cstdint>
#include <string>
#include <vector>

struct NVSDK_NGX_Handle;
struct NVSDK_NGX_Parameter;

namespace rk {
// Experimental one-frame DLAA evaluation on owned inputs. The caller retains
// this object, its frame, and the game device until poll completes or process
// exit. No game/render-target output is written.
class OffscreenDlssProbe final {
public:
    Result<bool> begin(ID3D11Device* device,ID3D11DeviceContext* context,
        PreparedSrInputs& inputs);
    Result<std::string> poll(ID3D11Device* device,ID3D11DeviceContext* context);
    bool pending() const noexcept { return pending_; }
    unsigned width() const noexcept { return width_; }
    unsigned height() const noexcept { return height_; }
private:
    HANDLE runtimeFile_{INVALID_HANDLE_VALUE};
    NVSDK_NGX_Parameter* parameters_{};
    NVSDK_NGX_Handle* feature_{};
    Microsoft::WRL::ComPtr<ID3D11Texture2D> readback_;
    Microsoft::WRL::ComPtr<ID3D11Query> completion_;
    unsigned width_{},height_{},pixelBytes_{};
    bool initialized_{},pending_{};
};
}
