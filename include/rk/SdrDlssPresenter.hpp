#pragma once
#include "rk/Result.hpp"
#include "rk/SrInput.hpp"
#include "rk/JitterContract.hpp"
#include "rk/RenderSizePolicy.hpp"
#include <d3d11.h>
#include <wrl/client.h>
#include <Windows.h>
#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

struct NVSDK_NGX_Handle;
struct NVSDK_NGX_Parameter;

namespace rk {
struct SrFrameMetadata {
    std::uint64_t frameId{},generation{};
    bool resetHistory{};
};
struct SrEvaluationToken {
    std::uint64_t frameId{},generation{};
    unsigned slot{};
};
// Experimental SDR DLAA presenter. Caller holds the verified Skyrim renderer
// lock and supplies one real HUD-free source frame per call. A busy resource
// slot skips a frame, leaving Skyrim's native image intact.
class SdrDlssPresenter final {
public:
    using PreparedEvaluator=std::function<Result<bool>(ID3D11DeviceContext*,
        const PreparedSrInputs&,const SrFrameMetadata&,NgxJitter,bool)>;
    explicit SdrDlssPresenter(PreparedEvaluator evaluator={}):
        preparedEvaluator_(std::move(evaluator)) {}
    Result<bool> configureQuality(UpscaleQuality quality) noexcept;
    // Query from the same NGX capability session that later creates/evaluates
    // the feature. Intended for the early factory boundary before a reduced
    // scene buffer is exposed to ENB/Skyrim.
    Result<Extent> prepareReducedPlan(ID3D11Device* device,
        ID3D11DeviceContext* context,Extent display);
    // Separates feature creation from first-frame evaluation for a staged
    // live diagnostic; uses only the dimensions of the verified NGX plan.
    Result<bool> createReducedFeature(ID3D11Device* device,
        ID3D11DeviceContext* context);
    Result<bool> render(ID3D11Device* device,ID3D11DeviceContext* context,
        ID3D11Texture2D* backbuffer,ID3D11Texture2D* motion,ID3D11Texture2D* depth,
        NgxJitter jitter);
    // The scene and guides have the reduced render extent; the destination
    // keeps the display extent. An error leaves destination publication to
    // the caller's display-sized fallback path.
    Result<bool> renderSr(ID3D11Device* device,ID3D11DeviceContext* context,
        ID3D11Texture2D* scene,ID3D11Texture2D* motion,ID3D11Texture2D* depth,
        ID3D11Texture2D* backbuffer,NgxJitter jitter);
    // Consumes owned, already-cropped resources without re-entering the R24
    // preparer. Success returns a same-frame token; publication is separate.
    Result<std::optional<SrEvaluationToken>> evaluatePrepared(ID3D11Device* device,
        ID3D11DeviceContext* context,PreparedSrInputs prepared,
        SrFrameMetadata metadata,NgxJitter jitter);
    Result<bool> publishEvaluated(ID3D11DeviceContext* context,
        SrEvaluationToken token,ID3D11Texture2D* destination);
    std::size_t retainedPreparedFrames() const noexcept;
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
    struct PreparedSlot {
        std::optional<PreparedSrInputs> frame;
        Microsoft::WRL::ComPtr<ID3D11Query> completion;
        SrFrameMetadata metadata{};
        bool inFlight{},evaluated{},published{};
    };
    struct RetiredPrepared {
        PreparedSrInputs frame;
        Microsoft::WRL::ComPtr<ID3D11Query> completion;
    };
    Result<bool> renderFrame(ID3D11Device* device,ID3D11DeviceContext* context,
        ID3D11Texture2D* scene,ID3D11Texture2D* motion,ID3D11Texture2D* depth,
        ID3D11Texture2D* backbuffer,NgxJitter jitter,bool reduced);
    Result<bool> initialize(ID3D11Device* device,ID3D11DeviceContext* context,
        UINT width,UINT height,UINT displayWidth,UINT displayHeight,bool reduced);
    Result<bool> beginSession(ID3D11Device* device,ID3D11DeviceContext* context,bool reduced);
    Result<bool> submitPreparedNgx(ID3D11DeviceContext* context,
        const PreparedSrInputs& prepared,NgxJitter jitter,bool reset);
    void retainUnsubmitted(ID3D11DeviceContext* context,PreparedSrInputs frame);
    Result<bool> retirePrepared(ID3D11DeviceContext* context);
    std::array<Slot,3> slots_;
    std::array<PreparedSlot,3> preparedSlots_;
    std::vector<RetiredPrepared> retiredPrepared_;
    std::vector<PreparedSrInputs> unfencedPrepared_;
    PreparedEvaluator preparedEvaluator_;
    std::uint64_t preparedGeneration_{},lastPreparedFrameId_{};
    unsigned nextPreparedSlot_{};
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    HANDLE runtimeFile_{INVALID_HANDLE_VALUE};
    NVSDK_NGX_Parameter* parameters_{};
    NVSDK_NGX_Handle* feature_{};
    UINT width_{},height_{},displayWidth_{},displayHeight_{};
    std::uint64_t submittedFrames_{};
    unsigned nextSlot_{};
    bool initialized_{};
    bool ngxStartAttempted_{},ngxInitSucceeded_{};
    bool reduced_{};
    bool resetPending_{};
    UpscaleQuality quality_{UpscaleQuality::Quality};
    std::optional<RenderSizePlan> preparedPlan_;
};
}
