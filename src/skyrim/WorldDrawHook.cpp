#include "rk/WorldDrawHook.hpp"
#include "rk/CallSite.hpp"
#include "rk/FrameProbe.hpp"
#include "rk/PatchDescriptor.hpp"
#include "rk/PipelineBoundary.hpp"
#include "rk/RendererHook.hpp"
#include "rk/SwapObserver.hpp"
#include "rk/SrInput.hpp"
#include "rk/SdrSrPresentation.hpp"
#include "rk/StagePairCapture.hpp"
#include "rk/WorldDraw.hpp"
#include "rk/MenuDisplay.hpp"
#include "rk/DeferredUiFlush.hpp"
#include "rk/OwnedRouteProfile.hpp"
#include "rk/DiagnosticsMenu.hpp"
#include "rk/DrsHook.hpp"
#include "rk/DrsReadiness.hpp"
#include "rk/RenderSizePolicy.hpp"
#include "rk/RendererBootstrap.hpp"
#include "rk/NativeFlipTarget.hpp"
#include "rk/NativeUiRedirector.hpp"
#include "rk/HudMovieViewport.hpp"
#ifdef RK_WITH_NGX
#include "rk/OffscreenDlssProbe.hpp"
#include "rk/NrStage.hpp"
#include "rk/SdrDlssPresenter.hpp"
#endif
#include <spdlog/spdlog.h>
#include <RE/Skyrim.h>
#include <ShlObj.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstring>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <filesystem>
#include <wrl/client.h>

namespace rk {
namespace {
static_assert(sizeof(HudMovieViewport)==sizeof(RE::GViewport));
struct HudMovieViewportRestore {
    RE::IMenu* menu{};
    RE::GFxMovieView* movie{};
    HudMovieViewport original{};
    HudMovieViewport applied{};
};
struct WorldState {
    WorldDrawForwarder forwarder;
    MenuDisplayForwarder menuForwarder;
    DeferredUiFlushForwarder deferredUiFlushForwarder;
    std::atomic<std::uint64_t> forwarded{0};
    std::atomic<DisplayMode> displayedMode{DisplayMode::Native};
    std::atomic<std::uint32_t> statusWidth{0},statusHeight{0};
    std::atomic<std::uint64_t> statusDlssFrames{0},statusSkippedFrames{0};
    std::atomic<bool> statusDlssDisabled{false};
    std::uintptr_t expectedRenderer{};
    std::uintptr_t jitterCamera{};
    std::uintptr_t uiSingletonCell{};
    std::atomic_flag creationBound=ATOMIC_FLAG_INIT;
    std::atomic<std::uintptr_t> createdDevice{0},createdContext{0},createdSwap{0};
    enum class CopyStatus { NotAttempted, Pending, Complete, Failed };
    std::atomic<CopyStatus> copyStatus{CopyStatus::NotAttempted};
    std::uint64_t nextCopyFrame{1};
    unsigned copyAttempts{};
    Microsoft::WRL::ComPtr<ID3D11Query> copyEvent;
    std::optional<PreparedSrInputs> copiedFrame;
    std::atomic<bool> initialTargetMapDone{false};
    std::atomic<bool> presentTargetProbeDue{false};
    std::atomic<unsigned> ownedPrePresentProbes{0};
    std::atomic<unsigned> ownedPostEnbProbes{0};
    std::atomic<std::uintptr_t> worldColourIdentity{0};
    std::atomic<bool> drsSuppressed{false};
    std::mutex drsTupleMutex;
    StableDrsTupleGate drsTupleGate;
    std::atomic<bool> srSourceVerified{false};
    UINT srVerifiedWidth{},srVerifiedHeight{};
    std::uint64_t srGeneration{};
    std::mutex stagePairMutex;
    std::optional<StagePairCapture> stagePair;
    std::uint64_t stagePairFrame{};
#ifdef RK_WITH_NGX
    struct OwnedSrStageCapture {
        std::uint64_t frame{};
        std::vector<ProbeImage> images;
    };
    std::mutex ownedSrStageMutex;
    std::optional<OwnedSrStageCapture> ownedSrStages;
    bool ownedSrStageAttempted{};
    struct MenuUiSequence {
        std::uint64_t frame{};
        unsigned nextOrdinal{};
        std::vector<ProbeImage> images;
        std::vector<std::string> names;
        std::string lastDigest;
    };
    std::mutex menuUiSequenceMutex;
    std::optional<MenuUiSequence> menuUiSequence;
    bool menuUiSequenceAttempted{};
    struct MenuBoundaryCapture {
        std::uint64_t frame{};
        std::vector<ProbeImage> images;
        bool afterEndFrameCaptured{};
        bool magic{};
    };
    bool menuBoundaryCaptureAttempted{};
    bool magicBoundaryCaptureAttempted{};
    bool magicReplayCaptureAttempted{};
    std::optional<MenuBoundaryCapture> menuBoundaryCapture;
    struct CursorReplayCapture {
        std::uint64_t frame{};
        std::vector<ProbeImage> images;
        bool magic{};
    };
    bool cursorReplayCaptureAttempted{},cursorReplayMissingLogged{};
    std::optional<CursorReplayCapture> cursorReplayCapture;
    std::uint64_t inventoryMenuLastFrame{},inventoryTraceFrame{};
    unsigned inventoryMenuStableFrames{},inventoryTraceWindowFrames{};
    std::uint64_t magicMenuLastFrame{};
    unsigned magicMenuStableFrames{};
    struct CompletedOutput {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> image;
        UINT width{},height{};
        std::string sha256;
    };
    std::optional<CompletedOutput> completedOutput;
    OffscreenDlssProbe dlssProbe;
    bool probeFailed{};
    SdrDlssPresenter sdrPresenter;
    SdrDlssPresenter srPresenter;
    NrStage nrStage;
    bool nrFailureLogged{};
    std::array<std::optional<SdrSrFrameResult>,3> srFallbacks;
    std::vector<SdrSrFrameResult> ownedFallbacks;
    bool srRequested{};
    bool srDisabled{};
    OwnedSceneAdmissionGate ownedSceneGate;
    OwnedSceneAdmissionGate menuSceneGate;
    bool ownedInputCaptureOnly{};
    bool ownedSpatialBaseline{};
    bool captureFirstDlssFrame{};
    UpscaleQuality earlyQuality{UpscaleQuality::Quality};
    double manualRenderScale{};
    bool automaticMipBias{true};
    double manualMipBias{};
    std::atomic<float> srSharpness{};
    std::uint64_t ownedEvaluationLimit{};
    bool ownedInputCaptureAttempted{};
    std::uint64_t ownedNgxCreatedAt{};
    bool ownedNgxInitFailed{};
    bool nativeUiRouteActivated{},coldTitleUiRouteLogged{},coldLoadingUiRouteLogged{};
    std::uint64_t menuProviderAdmissionGeneration{};
    std::uint64_t deferredUiFlushRebinds{};
    std::uint64_t hudUiTraceFrame{};
    unsigned hudUiTraceCount{};
    bool hudMovieViewportsLogged{};
    std::array<HudMovieViewportRestore,32> hudViewportRestores;
    std::uint64_t hudViewportFrame{},hudViewportWindows{};
    unsigned hudViewportRestoreCount{};
    bool hudViewportConflictLogged{};
    std::uint64_t loadingUiTraceFrame{},loadingUiTraceLastFrame{};
    unsigned loadingUiEarlyTraceCount{},loadingUiWorldTraceCount{};
    std::uint64_t deferredMenuPublications{},preservedInventoryComposites{},
        preservedMagicComposites{},preservedMainMenuComposites{},
        magicMovieReplays{},cursorReplays{};
    bool deferredUiFlushWarningLogged{};
    bool deferredUiFlushMotionWarningLogged{};
    bool deferredMenuPublicationWarningLogged{};
    bool uiDepthViewContractLogged{};
    bool nativePresenterStoppedForSr{};
    UINT srWidth{},srHeight{};
    std::uint64_t srActiveGeneration{};
    std::uint64_t srSkipped{};
    bool sdrDisabled{},firstSdrCaptured{};
    std::uint64_t sdrSkipped{},sdrJitterSkipped{};
#endif
};
std::atomic<WorldState*> active{nullptr};
bool read(std::uintptr_t address,void* destination,std::size_t size);
#ifdef RK_WITH_NGX
Result<NgxJitter> readNgxJitter(std::uintptr_t camera,
    std::uint32_t targetWidth,std::uint32_t targetHeight) {
    if(!camera)return Error{ErrorCode::Unavailable,"Verified game camera is unavailable"};
    std::array<std::uint8_t,0x4c> bytes{};
    if(!read(camera,bytes.data(),bytes.size()))
        return Error{ErrorCode::Io,"Cannot read same-frame game camera jitter"};
    return ngxJitterFromGameCamera(bytes,targetWidth,targetHeight);
}
#endif
struct WorldNumbers {
    std::uintptr_t device{},context{},swap{},colour{},motion{},depth{},lockOwner{};
    std::int32_t lockRecursion{};
    bool valid{};
};
std::uintptr_t identity(IUnknown* object) noexcept {
    Microsoft::WRL::ComPtr<IUnknown> canonical;
    return object&&SUCCEEDED(object->QueryInterface(IID_PPV_ARGS(&canonical)))?
        reinterpret_cast<std::uintptr_t>(canonical.Get()):0;
}
void logTargetBoundary(const char* stage,ID3D11DeviceContext* context,
    IDXGISwapChain* swap,std::uintptr_t colourIdentity) {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> backbuffer;
    const auto backResult=swap->GetBuffer(0,IID_PPV_ARGS(&backbuffer));
    const auto backIdentity=identity(backbuffer.Get());
    D3D11_TEXTURE2D_DESC backDesc{};
    if(backbuffer)backbuffer->GetDesc(&backDesc);
    spdlog::info("Target map {}: colourIdentity=0x{:x}; backbuffer=0x{:x} {}x{} format={}; GetBuffer=0x{:08x}; colourIsBackbuffer={}; read-only",
        stage,colourIdentity,backIdentity,backDesc.Width,backDesc.Height,
        static_cast<unsigned>(backDesc.Format),static_cast<std::uint32_t>(backResult),
        colourIdentity&&colourIdentity==backIdentity);
    std::array<ID3D11RenderTargetView*,8> rawViews{};
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthView;
    context->OMGetRenderTargets(static_cast<UINT>(rawViews.size()),rawViews.data(),&depthView);
    for(unsigned i=0;i<rawViews.size();++i) {
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> view;
        view.Attach(rawViews[i]);
        if(!view)continue;
        Microsoft::WRL::ComPtr<ID3D11Resource> resource;
        view->GetResource(&resource);
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
        if(resource)resource.As(&texture);
        D3D11_TEXTURE2D_DESC desc{};
        if(texture)texture->GetDesc(&desc);
        const auto targetIdentity=identity(resource.Get());
        spdlog::info("Target map {} RTV{}: resource=0x{:x} {}x{} format={}; matchesColour={}; matchesBackbuffer={}; read-only",
            stage,i,targetIdentity,desc.Width,desc.Height,static_cast<unsigned>(desc.Format),
            colourIdentity&&targetIdentity==colourIdentity,
            backIdentity&&targetIdentity==backIdentity);
    }
    if(depthView) {
        Microsoft::WRL::ComPtr<ID3D11Resource> resource;
        depthView->GetResource(&resource);
        spdlog::info("Target map {} DSV: resource=0x{:x}; read-only",stage,identity(resource.Get()));
    }
}
WorldNumbers readWorldNumbers(void* world,std::uintptr_t expected) noexcept {
    WorldNumbers result{};
    if(!world||reinterpret_cast<std::uintptr_t>(world)!=expected)return result;
    std::array<std::uint8_t,renderer1170Size> bytes{};
    SIZE_T copied{};
    if(!ReadProcessMemory(GetCurrentProcess(),world,bytes.data(),bytes.size(),&copied)||copied!=bytes.size())
        return result;
    const auto pointer=[&](std::size_t offset) {
        std::uintptr_t value{};std::memcpy(&value,bytes.data()+offset,sizeof(value));return value;
    };
    result.device=pointer(0x48);result.context=pointer(0x50);result.swap=pointer(0x70);
    result.colour=pointer(0xa58+0x30);result.motion=pointer(0xa58+7*0x30);
    result.depth=pointer(0x2018);result.lockOwner=pointer(0x27f0+16);
    std::memcpy(&result.lockRecursion,bytes.data()+0x27f0+12,sizeof(result.lockRecursion));
    result.valid=true;
    return result;
}
void copyWorldInputsOnce(WorldState* state,const WorldNumbers& numbers) noexcept {
    const auto status=state->copyStatus.load(std::memory_order_acquire);
    if(status==WorldState::CopyStatus::Complete||status==WorldState::CopyStatus::Failed)return;
    if(status==WorldState::CopyStatus::NotAttempted&&
       state->forwarded.load(std::memory_order_relaxed)<state->nextCopyFrame)return;
    const auto thread=GetCurrentThreadId();
    const auto device=state->createdDevice.load(std::memory_order_acquire);
    const auto context=state->createdContext.load(std::memory_order_relaxed);
    const auto swap=state->createdSwap.load(std::memory_order_relaxed);
    if(!numbers.valid||numbers.lockOwner!=thread||numbers.lockRecursion<=0||
       !device||!context||!swap||numbers.device!=device||numbers.context!=context||
       numbers.swap!=swap||!numbers.colour||!numbers.motion||!numbers.depth)return;
    auto* immediate=reinterpret_cast<ID3D11DeviceContext*>(context);
    if(status==WorldState::CopyStatus::Pending) {
        const auto result=immediate->GetData(state->copyEvent.Get(),nullptr,0,
            D3D11_ASYNC_GETDATA_DONOTFLUSH);
        if(result==S_OK) {
            const auto removed=reinterpret_cast<ID3D11Device*>(device)->GetDeviceRemovedReason();
            if(FAILED(removed)) {
                state->copyStatus.store(WorldState::CopyStatus::Failed,std::memory_order_release);
                try { spdlog::warn("Owned SR input copy device removed: HRESULT=0x{:08x}; resources retained",static_cast<std::uint32_t>(removed)); } catch (...) {}
                return;
            }
            try {
                const std::array<ID3D11Texture2D*,1> depth{state->copiedFrame->depth()};
                const auto readback=readbackCandidates(immediate,depth,16*1024*1024);
                if(const auto error=std::get_if<Error>(&readback)) {
                    state->copyStatus.store(WorldState::CopyStatus::Failed,std::memory_order_release);
                    spdlog::warn("Owned depth readiness readback failed: {}; resources retained",error->message);
                    return;
                }
                const auto& image=std::get<std::vector<ProbeImage>>(readback).at(0);
                const auto sampled=sampleWorldDepth(image.pixels,image.descriptor.Width,
                    image.descriptor.Height,image.rowBytes);
                if(const auto error=std::get_if<Error>(&sampled)) {
                    state->copyStatus.store(WorldState::CopyStatus::Failed,std::memory_order_release);
                    spdlog::warn("Owned depth readiness sample failed: {}; resources retained",error->message);
                    return;
                }
                const auto stats=std::get<DepthSampleStats>(sampled);
                if(!stats.worldLike()) {
                    state->copiedFrame.reset();state->copyEvent.Reset();
                    state->nextCopyFrame=state->forwarded.load(std::memory_order_relaxed)+
                        worldDepthProbeRetryDelay(state->copyAttempts);
                    state->copyStatus.store(WorldState::CopyStatus::NotAttempted,std::memory_order_release);
                    if(state->copyAttempts<=3||state->copyAttempts%6==0)
                        spdlog::info("Offscreen DLAA waiting for world-like depth: attempt={}; sampled distinct={}; nonFar={}; next world call >= {}",
                            state->copyAttempts,stats.distinct,stats.nonFar,state->nextCopyFrame);
                    return;
                }
                const std::array<ID3D11Texture2D*,2> guides{
                    state->copiedFrame->color(),state->copiedFrame->motion()};
                const auto guidesReadback=readbackCandidates(immediate,guides,45*1024*1024);
                if(const auto error=std::get_if<Error>(&guidesReadback)) {
                    state->copyStatus.store(WorldState::CopyStatus::Failed,std::memory_order_release);
                    spdlog::warn("Owned colour/motion diagnostic readback failed: {}; resources retained",error->message);
                    return;
                }
                const auto& images=std::get<std::vector<ProbeImage>>(guidesReadback);
                spdlog::info("Offscreen DLAA input frame accepted: attempt={}; depthDistinct={}; depthNonFar={}; colourSHA256={}; motionSHA256={}; depthSHA256={}",
                    state->copyAttempts,stats.distinct,stats.nonFar,
                    sha256(images[0].pixels),sha256(images[1].pixels),sha256(image.pixels));
                try {
                    const auto colourIdentity=identity(reinterpret_cast<IUnknown*>(numbers.colour));
                    logTargetBoundary("post-world",immediate,
                        reinterpret_cast<IDXGISwapChain*>(swap),colourIdentity);
                    const auto pipeline=inspectPipelineBoundary(immediate,
                        reinterpret_cast<ID3D11Texture2D*>(numbers.colour));
                    if(const auto error=std::get_if<Error>(&pipeline))
                        spdlog::warn("Post-world pipeline map unavailable: {}",error->message);
                    else {
                        const auto& boundary=std::get<PipelineBoundary>(pipeline);
                        spdlog::info("Post-world pipeline: PS=0x{:x}; VS=0x{:x}; topology={}; viewports={}; textureSlots={}; read-only",
                            boundary.pixelShaderIdentity,boundary.vertexShaderIdentity,
                            static_cast<unsigned>(boundary.topology),boundary.viewportCount,
                            boundary.resources.size());
                        for(std::size_t i=0;i<boundary.resources.size()&&i<16;++i) {
                            const auto& resource=boundary.resources[i];
                            spdlog::info("Post-world PS SRV{}: resource=0x{:x} {}x{} format={}; matchesHDRScene={}; read-only",
                                resource.slot,resource.resourceIdentity,resource.width,
                                resource.height,static_cast<unsigned>(resource.format),
                                resource.matchesScene);
                        }
                        if(boundary.resources.size()>16)
                            spdlog::info("Post-world pipeline: {} further texture slots omitted",
                                boundary.resources.size()-16);
                    }
                    state->worldColourIdentity.store(colourIdentity,std::memory_order_relaxed);
                } catch(const std::exception& error) {
                    spdlog::warn("Post-world target map unavailable: {}; DLAA probe continues",error.what());
                } catch(...) {
                    spdlog::warn("Post-world target map unavailable; DLAA probe continues");
                }
                try {
                    Microsoft::WRL::ComPtr<ID3D11Texture2D> backbuffer;
                    const auto got=reinterpret_cast<IDXGISwapChain*>(swap)->GetBuffer(0,
                        IID_PPV_ARGS(&backbuffer));
                    if(FAILED(got)||!backbuffer)
                        spdlog::warn("Stage pair backbuffer unavailable: HRESULT=0x{:08x}",
                            static_cast<std::uint32_t>(got));
                    else {
                        auto captured=StagePairCapture::capturePostWorld(immediate,
                            reinterpret_cast<ID3D11Texture2D*>(numbers.colour),backbuffer.Get());
                        if(const auto error=std::get_if<Error>(&captured))
                            spdlog::warn("Stage pair post-world capture unavailable: {}",error->message);
                        else {
                            std::scoped_lock lock(state->stagePairMutex);
                            state->stagePair.emplace(std::move(std::get<StagePairCapture>(captured)));
                            state->stagePairFrame=state->forwarded.load(std::memory_order_relaxed);
                            spdlog::info("Stage pair post-world captured at world frame {}; awaiting same-frame pre-ENB-Present",state->stagePairFrame);
                        }
                    }
                } catch(const std::exception& error) {
                    spdlog::warn("Stage pair post-world capture exception: {}",error.what());
                } catch(...) {
                    spdlog::warn("Stage pair post-world capture exception");
                }
                state->presentTargetProbeDue.store(true,std::memory_order_release);
            } catch(const std::exception& error) {
                state->copyStatus.store(WorldState::CopyStatus::Failed,std::memory_order_release);
                try { spdlog::warn("Owned SR readiness diagnostic failed: {}; resources retained",error.what()); } catch(...) {}
                return;
            } catch(...) {
                state->copyStatus.store(WorldState::CopyStatus::Failed,std::memory_order_release);
                try { spdlog::warn("Owned SR readiness diagnostic failed; resources retained"); } catch(...) {}
                return;
            }
#ifdef RK_WITH_NGX
            try {
                const auto begun=state->dlssProbe.begin(reinterpret_cast<ID3D11Device*>(device),
                    immediate,*state->copiedFrame);
                if(const auto error=std::get_if<Error>(&begun)) {
                    state->probeFailed=true;
                    spdlog::warn("Offscreen DLSS probe rejected: {}; owned resources retained",error->message);
                } else {
                    spdlog::info("Offscreen DLAA evaluation submitted on game device; no display write");
                }
            } catch(const std::exception& error) {
                state->probeFailed=true;
                try { spdlog::warn("Offscreen DLSS probe exception: {}; owned resources retained",error.what()); } catch(...) {}
            } catch(...) {
                state->probeFailed=true;
                try { spdlog::warn("Offscreen DLSS probe exception; owned resources retained"); } catch(...) {}
            }
#else
            state->copiedFrame.reset();
#endif
            state->copyEvent.Reset();
            state->copyStatus.store(WorldState::CopyStatus::Complete,std::memory_order_release);
            try { spdlog::info("Owned SR input copies completed on game GPU: original colour/motion/native typeless depth; display unchanged"); } catch (...) {}
        } else if(FAILED(result)) {
            state->copyStatus.store(WorldState::CopyStatus::Failed,std::memory_order_release);
            // Completion is uncertain; retain resources until process exit.
            try { spdlog::warn("Owned SR input copy completion failed: HRESULT=0x{:08x}; resources retained",static_cast<std::uint32_t>(result)); } catch (...) {}
        }
        return;
    }
    if(state->copyAttempts==24)
        try { spdlog::warn("Offscreen DLAA initial depth window expired; startup scene is still clear, continuing low-frequency probes"); } catch(...) {}
    state->copyStatus.store(WorldState::CopyStatus::Failed,std::memory_order_release);
    ++state->copyAttempts;
    try {
        auto* actualDevice=reinterpret_cast<ID3D11Device*>(device);
        Microsoft::WRL::ComPtr<ID3D11Query> event;
        const D3D11_QUERY_DESC query{D3D11_QUERY_EVENT,0};
        const auto created=actualDevice->CreateQuery(&query,&event);
        if(FAILED(created)) {
            spdlog::warn("Owned SR input copy event unavailable: HRESULT=0x{:08x}",static_cast<std::uint32_t>(created));return;
        }
        const std::array sources{
            reinterpret_cast<ID3D11Texture2D*>(numbers.colour),
            reinterpret_cast<ID3D11Texture2D*>(numbers.motion),
            reinterpret_cast<ID3D11Texture2D*>(numbers.depth)};
        auto prepared=prepareSrInputs(immediate,sources);
        if(const auto error=std::get_if<Error>(&prepared)) {
            spdlog::warn("Owned SR input copy rejected: {}",error->message);return;
        }
        state->copiedFrame.emplace(std::move(std::get<PreparedSrInputs>(prepared)));
        state->copyEvent=std::move(event);
        immediate->End(state->copyEvent.Get());
        immediate->Flush();
        state->copyStatus.store(WorldState::CopyStatus::Pending,std::memory_order_release);
        try { spdlog::info("Owned SR input copies queued: {}x{} colour/motion/native typeless depth; original resources unchanged; no NGX evaluation",
            state->copiedFrame->width(),state->copiedFrame->height()); } catch (...) {}
    } catch(const std::exception& error) {
        state->copyStatus.store(WorldState::CopyStatus::Failed,std::memory_order_release);
        try { spdlog::warn("Owned SR input copy aborted: {}",error.what()); } catch (...) {}
    } catch(...) {
        state->copyStatus.store(WorldState::CopyStatus::Failed,std::memory_order_release);
        try { spdlog::warn("Owned SR input copy aborted by unknown exception"); } catch (...) {}
    }
}
void afterOriginal(void*,std::uint32_t) noexcept {
    active.load(std::memory_order_acquire)->forwarded.fetch_add(1,std::memory_order_relaxed);
}
#ifdef RK_WITH_NGX
void probeOwnedPixels(const char* stage,ID3D11DeviceContext* context,
    ID3D11Texture2D* scene,ID3D11Texture2D* display) {
    const std::array<ID3D11Texture2D*,2> textures{scene,display};
    const auto readback=readbackCandidates(context,textures,24*1024*1024);
    if(const auto error=std::get_if<Error>(&readback)) {
        spdlog::warn("Owned {} pixel probe unavailable: {}",stage,error->message);
        return;
    }
    const auto& images=std::get<std::vector<ProbeImage>>(readback);
    for(std::size_t i=0;i<images.size();++i) {
        const auto& image=images[i];
        if(image.descriptor.Format!=DXGI_FORMAT_R8G8B8A8_UNORM)continue;
        unsigned nonBlack=0,distinct=0;
        std::array<std::uint32_t,256> colours{};
        for(unsigned y=0;y<16;++y)for(unsigned x=0;x<16;++x) {
            const auto sx=static_cast<UINT>((2*x+1)*static_cast<std::uint64_t>(image.descriptor.Width)/32);
            const auto sy=static_cast<UINT>((2*y+1)*static_cast<std::uint64_t>(image.descriptor.Height)/32);
            const auto* pixel=image.pixels.data()+sy*image.rowBytes+4*sx;
            nonBlack+=pixel[0]>4||pixel[1]>4||pixel[2]>4;
            std::uint32_t colour{};std::memcpy(&colour,pixel,sizeof(colour));
            bool known=false;for(unsigned j=0;j<distinct;++j)known|=colours[j]==colour;
            if(!known)colours[distinct++]=colour;
        }
        spdlog::info("Owned {} {} pixels: extent={}x{} nonBlack={}/256 distinct={}/256 SHA256={}",
            stage,
            i==0?"reduced scene":"native buffer",
            image.descriptor.Width,image.descriptor.Height,nonBlack,distinct,
            sha256(image.pixels));
    }
}
struct OwnedSceneSample {
    ColorSampleStats color;
    DepthSampleStats depth;
};
Result<OwnedSceneSample> probeOwnedScene(ID3D11DeviceContext* context,
    ID3D11Texture2D* scene,ID3D11Texture2D* depth,Extent render) {
    const std::array<ID3D11Texture2D*,2> textures{scene,depth};
    const auto readback=readbackCandidates(context,textures,24*1024*1024);
    if(const auto error=std::get_if<Error>(&readback))return *error;
    const auto& images=std::get<std::vector<ProbeImage>>(readback);
    const auto& colorImage=images.at(0);
    const auto& depthImage=images.at(1);
    if(colorImage.descriptor.Width!=render.width||
       colorImage.descriptor.Height!=render.height||
       depthImage.descriptor.Width<render.width||
       depthImage.descriptor.Height<render.height)
        return Error{ErrorCode::InvalidInput,"Owned scene/depth extent differs from render extent"};
    const auto color=sampleWorldColor(colorImage.pixels,render.width,render.height,
        colorImage.rowBytes);
    if(const auto error=std::get_if<Error>(&color))return *error;
    const auto depthStats=sampleWorldDepth(depthImage.pixels,render.width,render.height,
        depthImage.rowBytes);
    if(const auto error=std::get_if<Error>(&depthStats))return *error;
    return OwnedSceneSample{std::get<ColorSampleStats>(color),
        std::get<DepthSampleStats>(depthStats)};
}
Result<std::filesystem::path> captureOwnedSrInputs(ID3D11DeviceContext* context,
    ID3D11Texture2D* scene,ID3D11Texture2D* motion,ID3D11Texture2D* depth,
    Extent display,std::uint64_t sequence) {
    const std::array<ID3D11Texture2D*,3> sources{scene,motion,depth};
    auto prepared=prepareSdrSrInputsFromOwnedScene(context,sources,
        display.width,display.height);
    if(const auto error=std::get_if<Error>(&prepared))return *error;
    const auto& frame=std::get<PreparedSrInputs>(prepared);
    const std::array<ID3D11Texture2D*,3> copied{
        frame.color(),frame.motion(),frame.depth()};
    auto readback=readbackCandidates(context,copied,24*1024*1024);
    if(const auto error=std::get_if<Error>(&readback))return *error;
    PWSTR documents=nullptr;
    const auto found=SHGetKnownFolderPath(FOLDERID_Documents,
        KF_FLAG_DEFAULT,nullptr,&documents);
    struct FreeDocuments { PWSTR value;~FreeDocuments(){CoTaskMemFree(value);} } free{documents};
    if(FAILED(found)||!documents)
        return Error{ErrorCode::Unavailable,"Owned SR input capture Documents directory unavailable"};
    try {
        const auto directory=std::filesystem::path(documents)/"My Games"/
            "Skyrim Special Edition"/"SKSE"/"RazKolbasCaptures"/
            ("owned-sr-inputs-"+std::to_string(GetCurrentProcessId())+"-"+
            std::to_string(sequence)+"-"+std::to_string(GetTickCount64()));
        const std::array<std::string_view,3> names{
            "color.raw","motion.raw","depth-r32.raw"};
        const auto saved=saveProbeBundle(directory,
            std::get<std::vector<ProbeImage>>(readback),names);
        if(const auto error=std::get_if<Error>(&saved))return *error;
        return directory;
    } catch(const std::exception& error) {
        return Error{ErrorCode::Io,std::string("Cannot locate owned SR capture directory: ")+error.what()};
    }
}
std::string safeMenuLabel(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for(const unsigned char c:value)
        result.push_back(std::isalnum(c)||c=='-'||c=='_'?static_cast<char>(c):'_');
    if(result.empty())result="unknown";
    if(result.size()>64)result.resize(64);
    return result;
}
std::pair<std::uintptr_t,std::string> menuAtOrdinal(
    const WorldState* state,unsigned ordinal) {
    RE::UI* ui{};
    if(!state||!state->uiSingletonCell||
       !read(state->uiSingletonCell,&ui,sizeof(ui)))return {0,"unresolved"};
    if(!ui||ordinal>=ui->menuStack.size())return {0,"unresolved"};
    auto* menu=ui->menuStack[ordinal].get();
    for(auto& [name,entry]:ui->menuMap)
        if(entry.menu.get()==menu)return {
            reinterpret_cast<std::uintptr_t>(menu),name.c_str()};
    return {reinterpret_cast<std::uintptr_t>(menu),"unregistered"};
}
RE::IMenu* menuInstanceOnStack(const WorldState* state,std::string_view menuName) {
    RE::UI* ui{};
    if(!state||!state->uiSingletonCell||
       !read(state->uiSingletonCell,&ui,sizeof(ui))||!ui)return nullptr;
    for(auto& [name,entry]:ui->menuMap) {
        if(std::string_view(name.c_str())!=menuName)continue;
        auto* menu=entry.menu.get();
        if(!menu)return nullptr;
        for(const auto& stacked:ui->menuStack)
            if(stacked.get()==menu)return menu;
        return nullptr;
    }
    return nullptr;
}
bool namedMenuOnStack(const WorldState* state,std::string_view menuName) {
    return menuInstanceOnStack(state,menuName)!=nullptr;
}
bool inventoryMenuOnStack(const WorldState* state) {
    return namedMenuOnStack(state,RE::InventoryMenu::MENU_NAME);
}
bool magicMenuOnStack(const WorldState* state) {
    return namedMenuOnStack(state,RE::MagicMenu::MENU_NAME);
}
bool titleMenuOnStack(const WorldState* state) {
    return namedMenuOnStack(state,RE::MainMenu::MENU_NAME)&&
        !namedMenuOnStack(state,RE::LoadingMenu::MENU_NAME);
}
RE::IMenu* cursorMenuOnStack(const WorldState* state) {
    RE::UI* ui{};
    if(!state||!state->uiSingletonCell||
       !read(state->uiSingletonCell,&ui,sizeof(ui))||!ui)return nullptr;
    for(auto& [name,entry]:ui->menuMap) {
        if(std::string_view(name.c_str())!=RE::CursorMenu::MENU_NAME)continue;
        auto* cursor=entry.menu.get();
        if(!cursor)return nullptr;
        for(const auto& stacked:ui->menuStack)
            if(stacked.get()==cursor)return cursor;
        return nullptr;
    }
    return nullptr;
}
struct BoundUiView {
    std::uintptr_t id{};
    UINT width{},height{};
    DXGI_FORMAT format{DXGI_FORMAT_UNKNOWN};
};
BoundUiView boundUiView(ID3D11View* view) noexcept {
    if(!view)return {};
    Microsoft::WRL::ComPtr<ID3D11Resource> resource;
    view->GetResource(&resource);
    if(!resource)return {};
    Microsoft::WRL::ComPtr<IUnknown> identity;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    if(FAILED(resource.As(&identity))||FAILED(resource.As(&texture)))return {};
    D3D11_TEXTURE2D_DESC desc{};
    texture->GetDesc(&desc);
    return {reinterpret_cast<std::uintptr_t>(identity.Get()),
        desc.Width,desc.Height,desc.Format};
}
void logInventoryBinding(WorldState* state,std::uint64_t frame,
    std::string_view phase) noexcept {
    if(!state||state->inventoryTraceFrame!=frame)return;
    auto* context=reinterpret_cast<ID3D11DeviceContext*>(
        state->createdContext.load(std::memory_order_relaxed));
    if(!context)return;
    std::array<ID3D11RenderTargetView*,2> raw{};
    ID3D11DepthStencilView* rawDepth{};
    context->OMGetRenderTargets(static_cast<UINT>(raw.size()),raw.data(),&rawDepth);
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> first,second;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depth;
    first.Attach(raw[0]);second.Attach(raw[1]);depth.Attach(rawDepth);
    const auto a=boundUiView(first.Get());
    const auto b=boundUiView(second.Get());
    const auto d=boundUiView(depth.Get());
    UINT viewCount=1;
    D3D11_VIEWPORT viewport{};
    context->RSGetViewports(&viewCount,&viewport);
    try {spdlog::info("Inventory frame {} {} binding: RTV0=0x{:x}/{} {}x{} RTV1=0x{:x}/{} {}x{} DSV=0x{:x}/{} {}x{} viewportCount={} viewport={}x{}",
        frame,phase,a.id,static_cast<unsigned>(a.format),a.width,a.height,
        b.id,static_cast<unsigned>(b.format),b.width,b.height,
        d.id,static_cast<unsigned>(d.format),d.width,d.height,
        viewCount,viewCount?viewport.Width:0,viewCount?viewport.Height:0);
    }catch(...) {}
}
void logHudUiBoundary(WorldState* state,std::uint64_t frame,
    std::string_view phase) noexcept {
    if(!state||state->hudUiTraceFrame!=frame)return;
    auto* context=reinterpret_cast<ID3D11DeviceContext*>(
        state->createdContext.load(std::memory_order_relaxed));
    if(!context)return;
    ID3D11RenderTargetView* rawColor{};
    ID3D11DepthStencilView* rawDepth{};
    context->OMGetRenderTargets(1,&rawColor,&rawDepth);
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> color;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depth;
    color.Attach(rawColor);depth.Attach(rawDepth);
    const auto target=boundUiView(color.Get());
    const auto stencil=boundUiView(depth.Get());
    UINT viewportCount=1,scissorCount=1;
    D3D11_VIEWPORT viewport{};
    D3D11_RECT scissor{};
    context->RSGetViewports(&viewportCount,&viewport);
    context->RSGetScissorRects(&scissorCount,&scissor);
    std::array<std::uint32_t,4> dimensions{};
    const bool haveDimensions=state->jitterCamera&&
        read(state->jitterCamera+0x24,dimensions.data(),sizeof(dimensions));
    try {spdlog::info("HUD native UI boundary frame {} {}: RTV0={}x{} format={} DSV={}x{} viewport={}x{} origin=({}, {}) scissor=({}, {}, {}, {}) graphicsPairs={}x{}/{}x{} valid={}",
        frame,phase,target.width,target.height,static_cast<unsigned>(target.format),
        stencil.width,stencil.height,
        viewportCount?viewport.Width:0,viewportCount?viewport.Height:0,
        viewportCount?viewport.TopLeftX:0,viewportCount?viewport.TopLeftY:0,
        scissorCount?scissor.left:0,scissorCount?scissor.top:0,
        scissorCount?scissor.right:0,scissorCount?scissor.bottom:0,
        dimensions[0],dimensions[1],dimensions[2],dimensions[3],haveDimensions);
    }catch(...) {}
}
HudMovieViewport hudViewportRecord(const RE::GViewport& viewport) noexcept {
    return {viewport.bufferWidth,viewport.bufferHeight,
        viewport.left,viewport.top,viewport.width,viewport.height,
        viewport.scissorLeft,viewport.scissorTop,
        viewport.scissorWidth,viewport.scissorHeight,
        viewport.scale,viewport.aspectRatio,
        viewport.flags.underlying(),viewport.pad34};
}
void setHudMovieViewport(RE::GFxMovieView* movie,
    const HudMovieViewport& record) noexcept {
    RE::GViewport viewport;
    viewport.bufferWidth=record.bufferWidth;
    viewport.bufferHeight=record.bufferHeight;
    viewport.left=record.left;
    viewport.top=record.top;
    viewport.width=record.width;
    viewport.height=record.height;
    viewport.scissorLeft=record.scissorLeft;
    viewport.scissorTop=record.scissorTop;
    viewport.scissorWidth=record.scissorWidth;
    viewport.scissorHeight=record.scissorHeight;
    viewport.scale=record.scale;
    viewport.aspectRatio=record.aspectRatio;
    viewport.flags=static_cast<RE::GViewport::Flag>(record.flags);
    viewport.pad34=record.padding;
    movie->SetViewport(viewport);
}
HudMovieViewport getHudMovieViewport(RE::GFxMovieView* movie) noexcept {
    RE::GViewport viewport;
    movie->GetViewport(&viewport);
    return hudViewportRecord(viewport);
}
void finishNativeHudMovieViewportWindow(WorldState* state) noexcept {
    if(!state||!state->hudViewportRestoreCount)return;
    RE::UI* ui{};
    if(state->uiSingletonCell)
        read(state->uiSingletonCell,&ui,sizeof(ui));
    unsigned restored{},conflicts{};
    for(unsigned index=0;index<state->hudViewportRestoreCount;++index) {
        auto& entry=state->hudViewportRestores[index];
        bool stillOwned=false;
        if(ui)for(const auto& stacked:ui->menuStack)
            if(stacked.get()==entry.menu&&entry.menu->uiMovie.get()==entry.movie) {
                stillOwned=true;
                break;
            }
        if(stillOwned) {
            auto* movie=entry.movie;
            if(sameHudMovieViewport(getHudMovieViewport(movie),entry.applied)) {
                setHudMovieViewport(movie,entry.original);
                ++restored;
            } else ++conflicts;
        } else ++conflicts;
        entry={};
    }
    const auto count=++state->hudViewportWindows;
    if(count==1||count%600==0||
       (conflicts&&!state->hudViewportConflictLogged)) {
        try {spdlog::info("Native HUD movie viewport window frame {}: adjusted={} restored={} conflicts={} windows={}",
            state->hudViewportFrame,state->hudViewportRestoreCount,
            restored,conflicts,count);}catch(...) {}
    }
    if(conflicts&&!state->hudViewportConflictLogged) {
        state->hudViewportConflictLogged=true;
        try {spdlog::warn("HUD movie viewport changed by another owner; skipped restoration of the changed movie");}catch(...) {}
    }
    state->hudViewportRestoreCount=0;
}
void beginNativeHudMovieViewportWindow(WorldState* state,
    std::uint64_t frame) noexcept {
    if(!state)return;
    if(state->hudViewportFrame==frame)return;
    finishNativeHudMovieViewportWindow(state);
    if(state->displayedMode.load(std::memory_order_relaxed)!=DisplayMode::DlssSr||
       !namedMenuOnStack(state,RE::HUDMenu::MENU_NAME)||
       inventoryMenuOnStack(state)||magicMenuOnStack(state)||
       titleMenuOnStack(state)||
       namedMenuOnStack(state,RE::LoadingMenu::MENU_NAME))return;
    auto* domain=activeOwnedSceneDomain();
    if(!domain||domain->phase()!=ScenePhase::NativeUi||
       domain->frame()!=frame||domain->renderThread()!=GetCurrentThreadId())return;
    RE::UI* ui{};
    if(!state->uiSingletonCell||
       !read(state->uiSingletonCell,&ui,sizeof(ui))||!ui)return;
    state->hudViewportFrame=frame;
    const auto render=domain->plan().render;
    const auto display=domain->plan().display;
    for(unsigned ordinal=0;ordinal<ui->menuStack.size()&&ordinal<32;++ordinal) {
        auto* item=ui->menuStack[ordinal].get();
        if(!item||!item->uiMovie)continue;
        auto* movie=item->uiMovie.get();
        const auto original=getHudMovieViewport(movie);
        const auto native=nativeHudMovieViewport(original,render,display);
        if(!native)continue;
        setHudMovieViewport(movie,*native);
        const auto applied=getHudMovieViewport(movie);
        if(applied.bufferWidth!=static_cast<std::int32_t>(display.width)||
           applied.bufferHeight!=static_cast<std::int32_t>(display.height)||
           applied.width!=static_cast<std::int32_t>(display.width)||
           applied.height!=static_cast<std::int32_t>(display.height)) {
            setHudMovieViewport(movie,original);
            continue;
        }
        auto& entry=state->hudViewportRestores[state->hudViewportRestoreCount++];
        entry.menu=item;
        entry.movie=movie;
        entry.original=original;
        entry.applied=applied;
        if(state->hudViewportWindows==0) {
            const auto [identity,name]=menuAtOrdinal(state,ordinal);
            try {spdlog::info("Native HUD movie viewport frame {} ordinal {} menu={} id=0x{:x} {}x{} -> {}x{}",
                frame,ordinal,name,identity,original.width,original.height,
                applied.width,applied.height);}catch(...) {}
        }
    }
}
void logHudMovieViewports(WorldState* state,std::uint64_t frame) noexcept {
    if(!state||state->hudMovieViewportsLogged||
       state->displayedMode.load(std::memory_order_relaxed)!=DisplayMode::DlssSr||
       !namedMenuOnStack(state,RE::HUDMenu::MENU_NAME)||
       inventoryMenuOnStack(state)||magicMenuOnStack(state)||
       titleMenuOnStack(state)||
       namedMenuOnStack(state,RE::LoadingMenu::MENU_NAME))return;
    RE::UI* ui{};
    if(!state->uiSingletonCell||
       !read(state->uiSingletonCell,&ui,sizeof(ui))||!ui)return;
    state->hudMovieViewportsLogged=true;
    unsigned logged{};
    for(unsigned ordinal=0;ordinal<ui->menuStack.size()&&ordinal<32;++ordinal) {
        auto* item=ui->menuStack[ordinal].get();
        if(!item||!item->uiMovie)continue;
        const auto viewport=getHudMovieViewport(item->uiMovie.get());
        const auto [identity,name]=menuAtOrdinal(state,ordinal);
        try {spdlog::info("HUD movie viewport frame {} ordinal {} menu={} id=0x{:x} buffer={}x{} rect=({}, {}, {}x{}) scissor=({}, {}, {}x{}) scale={} aspect={} flags=0x{:x}",
            frame,ordinal,name,identity,viewport.bufferWidth,viewport.bufferHeight,
            viewport.left,viewport.top,viewport.width,viewport.height,
            viewport.scissorLeft,viewport.scissorTop,
            viewport.scissorWidth,viewport.scissorHeight,
            viewport.scale,viewport.aspectRatio,
            viewport.flags);}catch(...) {}
        ++logged;
    }
    try {spdlog::info("HUD movie viewport snapshot complete: frame={} movies={}; read-only",
        frame,logged);}catch(...) {}
}
bool armLoadingUiTrace(WorldState* state,std::uint64_t frame) noexcept {
    if(!state||!frame||state->loadingUiTraceLastFrame==frame)return false;
    auto* loading=menuInstanceOnStack(state,RE::LoadingMenu::MENU_NAME);
    if(!loading||!loading->uiMovie)return false;
    auto& count=state->srPresenter.submittedFrames()>0?
        state->loadingUiWorldTraceCount:state->loadingUiEarlyTraceCount;
    if(count>=3)return false;
    state->loadingUiTraceLastFrame=frame;
    state->loadingUiTraceFrame=frame;
    ++count;
    return true;
}
void logLoadingUiBoundary(WorldState* state,std::uint64_t frame,
    std::string_view stage) noexcept {
    if(!state||!state->loadingUiTraceFrame||
       state->loadingUiTraceFrame!=frame)return;
    auto* domain=activeOwnedSceneDomain();
    const auto render=domain?domain->plan().render:Extent{};
    const auto display=domain?domain->plan().display:Extent{};
    auto* context=reinterpret_cast<ID3D11DeviceContext*>(
        state->createdContext.load(std::memory_order_relaxed));
    ID3D11RenderTargetView* rawColor{};
    ID3D11DepthStencilView* rawDepth{};
    if(context)context->OMGetRenderTargets(1,&rawColor,&rawDepth);
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> color;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depth;
    color.Attach(rawColor);depth.Attach(rawDepth);
    const auto target=boundUiView(color.Get());
    const auto stencil=boundUiView(depth.Get());
    UINT viewportCount=1,scissorCount=1;
    D3D11_VIEWPORT viewport{};
    D3D11_RECT scissor{};
    if(context) {
        context->RSGetViewports(&viewportCount,&viewport);
        context->RSGetScissorRects(&scissorCount,&scissor);
    }
    try {spdlog::info("Loading UI boundary frame {} {}: scene={}x{} display={}x{} domainPhase={} mode={} providerSubmissions={} RTV0={}x{} DSV={}x{} viewport={}x{} scissor=({}, {}, {}, {})",
        frame,stage,render.width,render.height,display.width,display.height,
        domain?static_cast<int>(domain->phase()):-1,
        static_cast<int>(state->displayedMode.load(std::memory_order_relaxed)),
        state->srPresenter.submittedFrames(),
        target.width,target.height,stencil.width,stencil.height,
        viewportCount?viewport.Width:0,viewportCount?viewport.Height:0,
        scissorCount?scissor.left:0,scissorCount?scissor.top:0,
        scissorCount?scissor.right:0,scissorCount?scissor.bottom:0);}catch(...) {}
    if(stage!="before-first-PostDisplay")return;
    RE::UI* ui{};
    if(!state->uiSingletonCell||
       !read(state->uiSingletonCell,&ui,sizeof(ui))||!ui)return;
    for(unsigned ordinal=0;ordinal<ui->menuStack.size()&&ordinal<32;++ordinal) {
        auto* item=ui->menuStack[ordinal].get();
        if(!item||!item->uiMovie)continue;
        const auto view=getHudMovieViewport(item->uiMovie.get());
        const auto [identity,name]=menuAtOrdinal(state,ordinal);
        try {spdlog::info("Loading UI movie frame {} ordinal {} menu={} id=0x{:x} buffer={}x{} rect=({}, {}, {}x{}) flags=0x{:x}",
            frame,ordinal,name,identity,view.bufferWidth,view.bufferHeight,
            view.left,view.top,view.width,view.height,view.flags);}catch(...) {}
    }
}
void captureMenuUiEntry(WorldState* state,std::uint64_t frame) {
    std::scoped_lock lock(state->menuUiSequenceMutex);
    if(!state->menuUiSequence||state->menuUiSequence->frame!=frame)return;
    auto& sequence=*state->menuUiSequence;
    if(sequence.nextOrdinal>=63)return;
    const auto ordinal=sequence.nextOrdinal++;
    auto* domain=activeOwnedSceneDomain();
    const auto device=state->createdDevice.load(std::memory_order_relaxed);
    const auto context=state->createdContext.load(std::memory_order_relaxed);
    const auto swap=state->createdSwap.load(std::memory_order_relaxed);
    if(!domain||!device||!context||!swap)return;
    const auto native=acquireNativeFlipTarget(reinterpret_cast<IDXGISwapChain*>(swap),
        reinterpret_cast<ID3D11Device*>(device),domain->plan().display);
    if(const auto error=std::get_if<Error>(&native)) {
        spdlog::warn("Owned UI menu-sequence frame {} ordinal {} unavailable: {}",
            frame,ordinal,error->message);
        return;
    }
    const auto display=domain->plan().display;
    auto image=readbackRegion(reinterpret_cast<ID3D11DeviceContext*>(context),
        std::get<NativeFlipTarget>(native).texture.Get(),0,0,
        display.width,display.height);
    if(const auto error=std::get_if<Error>(&image)) {
        spdlog::warn("Owned UI menu-sequence region {} unavailable: {}",
            ordinal,error->message);
        return;
    }
    const auto [menuPointer,menuName]=menuAtOrdinal(state,ordinal);
    auto pixels=std::move(std::get<ProbeImage>(image));
    const auto digest=sha256(pixels.pixels);
    const bool save=sequence.images.empty()||digest!=sequence.lastDigest;
    if(save) {
        sequence.names.emplace_back("before-"+std::to_string(ordinal)+"-"+
            safeMenuLabel(menuName)+".raw");
        sequence.images.emplace_back(std::move(pixels));
    }
    sequence.lastDigest=digest;
    spdlog::info("Owned UI menu-sequence frame {} before ordinal {}: menu={} pointer=0x{:x} fullFrameSHA256={} saved={}",
        frame,ordinal,menuName,menuPointer,digest,save);
}
void completeMenuUiSequence(WorldState* state,IDXGISwapChain* swap) {
    std::scoped_lock lock(state->menuUiSequenceMutex);
    if(!state->menuUiSequence)return;
    auto& sequence=*state->menuUiSequence;
    const auto frame=state->forwarded.load(std::memory_order_relaxed);
    if(sequence.frame!=frame) {
        spdlog::warn("Owned UI menu-sequence discarded: another world frame arrived before Present");
        state->menuUiSequence.reset();
        return;
    }
    auto* domain=activeOwnedSceneDomain();
    const auto device=state->createdDevice.load(std::memory_order_relaxed);
    const auto context=state->createdContext.load(std::memory_order_relaxed);
    if(!domain||!device||!context||!swap) {
        state->menuUiSequence.reset();
        return;
    }
    const auto native=acquireNativeFlipTarget(swap,
        reinterpret_cast<ID3D11Device*>(device),domain->plan().display);
    if(const auto error=std::get_if<Error>(&native)) {
        spdlog::warn("Owned UI menu-sequence final target unavailable: {}",error->message);
        state->menuUiSequence.reset();
        return;
    }
    const auto display=domain->plan().display;
    auto finalImage=readbackRegion(reinterpret_cast<ID3D11DeviceContext*>(context),
        std::get<NativeFlipTarget>(native).texture.Get(),0,0,
        display.width,display.height);
    if(const auto error=std::get_if<Error>(&finalImage)) {
        spdlog::warn("Owned UI menu-sequence final region unavailable: {}",error->message);
        state->menuUiSequence.reset();
        return;
    }
    auto finalPixels=std::move(std::get<ProbeImage>(finalImage));
    const auto finalDigest=sha256(finalPixels.pixels);
    sequence.images.emplace_back(std::move(finalPixels));
    sequence.names.emplace_back("after-all-menus.raw");
    PWSTR documents=nullptr;
    const auto found=SHGetKnownFolderPath(FOLDERID_Documents,
        KF_FLAG_DEFAULT,nullptr,&documents);
    struct FreeDocuments { PWSTR value;~FreeDocuments(){CoTaskMemFree(value);} } free{documents};
    if(FAILED(found)||!documents) {
        spdlog::warn("Owned UI menu-sequence Documents directory unavailable");
        state->menuUiSequence.reset();
        return;
    }
    const auto directory=std::filesystem::path(documents)/"My Games"/
        "Skyrim Special Edition"/"SKSE"/"RazKolbasCaptures"/
        ("owned-ui-sequence-"+std::to_string(GetCurrentProcessId())+"-"+
        std::to_string(frame)+"-"+std::to_string(GetTickCount64()));
    std::vector<std::string_view> names;
    names.reserve(sequence.names.size());
    for(const auto& name:sequence.names)names.emplace_back(name);
    const auto saved=saveProbeBundle(directory,sequence.images,names,
        "RazKolbas same-frame full native UI target before the first and each changed predicted menu-stack entry, then after the complete stack");
    if(const auto error=std::get_if<Error>(&saved))
        spdlog::warn("Owned UI menu-sequence save unavailable: {}",error->message);
    else
        spdlog::info("Owned UI menu-sequence complete for frame {} with {} changed-entry snapshots from {} entries; finalFullFrameSHA256={} at {}",
            frame,sequence.images.size()-1,sequence.nextOrdinal,
            finalDigest,directory.string());
    state->menuUiSequence.reset();
}
void applyPendingNrRuntimeSettings(WorldState* state) noexcept {
    try {
        const auto update=consumeDiagnosticsNrRuntimeUpdate();
        if(!update)return;
        const auto applied=state->nrStage.updateRuntime(*update);
        if(const auto error=std::get_if<Error>(&applied)) {
            spdlog::warn("Live Neural Rendering update rejected: {}",error->message);
            return;
        }
        if(!std::get<bool>(applied))return;
        state->nrFailureLogged=false;
        const auto skin=update->get<Text>("NeuralRendering.SkinStructureStrength").value;
        spdlog::info("Live Neural Rendering controls applied with history reset: enabled={}; style={}; intensity={}; tone={}; structure={}; skin={}; autoMask={}; uiCorrection=off (native UI after NR/SR)",
            update->get<bool>("NeuralRendering.Enabled"),
            update->get<std::int64_t>("NeuralRendering.Style"),
            update->get<double>("NeuralRendering.Intensity"),
            update->get<double>("NeuralRendering.LocalToneStrength"),
            update->get<double>("NeuralRendering.LocalStructureStrength"),skin,
            update->get<bool>("NeuralRendering.UseAutoMask"));
    } catch(const std::exception& error) {
        try { spdlog::warn("Live Neural Rendering update failed open: {}",error.what()); }
        catch(...) {}
    } catch(...) {
        try { spdlog::warn("Live Neural Rendering update failed open"); }
        catch(...) {}
    }
}
enum class OwnedPublicationBoundary { PrePresent, MenuDisplay };
bool processOwnedWorldFrame(WorldState* state,void* world,
    std::uint64_t sequence,OwnedPublicationBoundary boundary) noexcept {
    auto* domain=activeOwnedSceneDomain();
    if(!domain)return ownedScenePreviouslyActive();
    if(domain->phase()==ScenePhase::Suspended)return true;
    try {
        const auto numbers=readWorldNumbers(world,state->expectedRenderer);
        if(!numbers.valid||numbers.lockOwner!=GetCurrentThreadId()||
           numbers.lockRecursion<=0||
           numbers.device!=state->createdDevice.load(std::memory_order_acquire)||
           numbers.context!=state->createdContext.load(std::memory_order_acquire)||
           numbers.swap!=state->createdSwap.load(std::memory_order_acquire)||
           !numbers.motion||!numbers.depth||
           domain->phase()!=ScenePhase::World||domain->frame()!=sequence)
            throw std::runtime_error("Owned world frame, renderer lock or guide chain differs");
        auto* device=reinterpret_cast<ID3D11Device*>(numbers.device);
        auto* context=reinterpret_cast<ID3D11DeviceContext*>(numbers.context);
        auto* swap=reinterpret_cast<IDXGISwapChain*>(numbers.swap);
        if(const auto update=consumeDiagnosticsSharpeningUpdate()) {
            const auto configuredSr=state->srPresenter.configureSharpness(
                update->enabled,update->sharpness);
            if(const auto error=std::get_if<Error>(&configuredSr))
                throw std::runtime_error(error->message);
            const auto configuredDlaa=state->sdrPresenter.configureSharpness(
                update->enabled,update->sharpness);
            if(const auto error=std::get_if<Error>(&configuredDlaa))
                throw std::runtime_error(error->message);
            state->srSharpness.store(update->enabled?update->sharpness:0.0f,
                std::memory_order_release);
            spdlog::info("Live post-DLSS sharpening changed: enabled={}; sharpness={}",
                update->enabled,update->sharpness);
        }
        auto* ui=ownedUiRedirector();
        if(!ui||ui->compatibilityFault())
            throw std::runtime_error("Owned UI context layout changed");
        for(auto it=state->ownedFallbacks.begin();it!=state->ownedFallbacks.end();) {
            const auto completed=it->complete(context);
            if(const auto error=std::get_if<Error>(&completed))
                throw std::runtime_error(error->message);
            if(std::get<bool>(completed))it=state->ownedFallbacks.erase(it);
            else ++it;
        }
        if(!domain->startProcessing(sequence,domain->plan().generation))
            throw std::runtime_error("Owned world phase did not enter processing");
        auto native=acquireNativeFlipTarget(swap,device,domain->plan().display);
        if(const auto error=std::get_if<Error>(&native))
            throw std::runtime_error(error->message);
        auto* scene=activeOwnedSceneTexture();
        if(!ui||!scene||FAILED(ui->replaceNativeTarget(
            std::get<NativeFlipTarget>(native).view.Get())))
            throw std::runtime_error("Owned native UI target is unavailable");
        if(FAILED(ui->bindNativeForProcessing(sequence)))
            throw std::runtime_error("Owned native output could not be bound for processing");
        auto* display=std::get<NativeFlipTarget>(native).texture.Get();
        if(sequence==1&&state->jitterCamera) {
            std::array<std::uint8_t,0x2c> camera{};
            if(read(state->jitterCamera,camera.data(),camera.size())) {
                std::uint32_t width{},height{};
                std::memcpy(&width,camera.data()+0x24,sizeof(width));
                std::memcpy(&height,camera.data()+0x28,sizeof(height));
                spdlog::info("Owned game camera extent: {}x{}; planned render={}x{} display={}x{}",
                    width,height,domain->plan().render.width,domain->plan().render.height,
                    domain->plan().display.width,domain->plan().display.height);
            }
        }
        if(boundary==OwnedPublicationBoundary::PrePresent&&
           state->ownedSceneGate.needsSample(sequence,domain->plan().generation)) {
            const bool wasReady=state->ownedSceneGate.ready();
            const auto sample=probeOwnedScene(context,scene,
                reinterpret_cast<ID3D11Texture2D*>(numbers.depth),
                domain->plan().render);
            const auto* stats=std::get_if<OwnedSceneSample>(&sample);
            state->ownedSceneGate.record(sequence,
                stats?std::optional<ColorSampleStats>{stats->color}:std::nullopt,
                stats?std::optional<DepthSampleStats>{stats->depth}:std::nullopt);
            const bool ready=state->ownedSceneGate.ready();
            if(ready&&!wasReady&&state->captureFirstDlssFrame) {
                state->ownedPrePresentProbes.store(2,std::memory_order_release);
                state->ownedPostEnbProbes.store(2,std::memory_order_release);
            }
            if(sequence==1||sequence%600==0||ready!=wasReady) {
                if(stats)
                    spdlog::info("Owned scene admission frame {}: colorNonBlack={} colorDistinct={} sceneLike={} depthDistinct={} depthNonFar={} worldLike={} NGX-ready={}",
                        sequence,stats->color.nonBlack,stats->color.distinct,
                        stats->color.sceneLike(),stats->depth.distinct,
                        stats->depth.nonFar,stats->depth.worldLike(),ready);
                else
                    spdlog::warn("Owned scene admission frame {} unavailable: {}; NGX-ready=false",
                        sequence,std::get<Error>(sample).message);
            }
        }
        const auto presented=presentSdrSrFrame(context,scene,display,
            [&]()->Result<bool> {
                const auto sourceReady=boundary==OwnedPublicationBoundary::MenuDisplay?
                    shouldSubmitOwnedProvider(state->menuSceneGate.ready(),
                        state->menuProviderAdmissionGeneration,
                        domain->plan().generation):
                    state->ownedSceneGate.ready();
                if(!sourceReady)
                    return Error{ErrorCode::Unavailable,"World colour and depth have not passed the owned NGX admission gate"};
                if(state->ownedSpatialBaseline)
                    return Error{ErrorCode::Unavailable,"Configured spatial baseline; NGX not submitted"};
                if(state->ownedInputCaptureOnly) {
                    if(!state->ownedInputCaptureAttempted) {
                        state->ownedInputCaptureAttempted=true;
                        const auto capture=captureOwnedSrInputs(context,scene,
                            reinterpret_cast<ID3D11Texture2D*>(numbers.motion),
                            reinterpret_cast<ID3D11Texture2D*>(numbers.depth),
                            domain->plan().display,sequence);
                        if(const auto error=std::get_if<Error>(&capture))
                            spdlog::warn("Owned SR input capture frame {} failed: {}",
                                sequence,error->message);
                        else
                            spdlog::info("Owned SR input capture frame {} saved to {}; no NGX evaluation",
                                sequence,std::get<std::filesystem::path>(capture).string());
                    }
                    return Error{ErrorCode::Unavailable,"Owned SR input capture only; NGX not submitted"};
                }
                if(state->ownedEvaluationLimit&&
                   state->srPresenter.submittedFrames()>=state->ownedEvaluationLimit)
                    return Error{ErrorCode::Unavailable,"Owned bounded-evaluation diagnostic complete"};
                if(state->ownedNgxInitFailed)
                    return Error{ErrorCode::Unavailable,"Owned NGX feature creation previously failed"};
                if(!state->ownedNgxCreatedAt) {
                    spdlog::info("Owned NGX stage frame {}: before feature creation",sequence);
                    const auto started=std::chrono::steady_clock::now();
                    const auto initialized=state->srPresenter.createReducedFeature(device,context);
                    if(const auto error=std::get_if<Error>(&initialized)) {
                        state->ownedNgxInitFailed=true;
                        spdlog::warn("Owned NGX stage frame {}: feature creation failed: {}",
                            sequence,error->message);
                        return *error;
                    }
                    if(!std::get<bool>(initialized)) {
                        state->ownedNgxInitFailed=true;
                        return Error{ErrorCode::Unavailable,"Owned NGX feature was not created"};
                    }
                    state->ownedNgxCreatedAt=sequence;
                    const auto elapsed=std::chrono::duration<double,std::milli>(
                        std::chrono::steady_clock::now()-started).count();
                    spdlog::info("Owned NGX stage frame {}: feature created in {:.3f} ms; waiting 120 frames before evaluation",
                        sequence,elapsed);
                }
                if(sequence<state->ownedNgxCreatedAt||
                   sequence-state->ownedNgxCreatedAt<120)
                    return Error{ErrorCode::Unavailable,"Owned NGX feature startup observation interval"};
                const bool firstAttempt=state->srPresenter.submittedFrames()==0;
                if(firstAttempt)
                    spdlog::info("Owned NGX stage frame {}: preparing first evaluated inputs",sequence);
                const std::array<ID3D11Texture2D*,3> sources{
                    scene,reinterpret_cast<ID3D11Texture2D*>(numbers.motion),
                    reinterpret_cast<ID3D11Texture2D*>(numbers.depth)};
                state->srSourceVerified.store(true,std::memory_order_release);
                const auto renderJitter=readNgxJitter(state->jitterCamera,
                    domain->plan().render.width,domain->plan().render.height);
                if(const auto error=std::get_if<Error>(&renderJitter))
                    return Error{ErrorCode::Unavailable,error->message};
                if(firstAttempt)
                    spdlog::info("Owned NGX stage frame {}: before pooled evaluation",
                        sequence);
                const auto evaluationStarted=std::chrono::steady_clock::now();
                if(firstAttempt) {
                    const auto jitter=std::get<NgxJitter>(renderJitter);
                    spdlog::info("Owned NGX evaluation parameters: jitter=({},{}); MVScale={}x{}; ngxSharpness=0; postSharpness={}; autoExposure=true; reset=true",
                        jitter.x,jitter.y,domain->plan().render.width,
                        domain->plan().render.height,
                        state->srSharpness.load(std::memory_order_relaxed));
                }
                auto evaluated=state->srPresenter.evaluateOwnedScene(device,context,sources,
                    domain->plan().display.width,domain->plan().display.height,
                    SrFrameMetadata{sequence,domain->plan().generation,false,
                        boundary==OwnedPublicationBoundary::MenuDisplay?
                            SrSourcePhase::MenuDisplay:SrSourcePhase::PrePresent},
                    std::get<NgxJitter>(renderJitter));
                if(firstAttempt)
                    spdlog::info("Owned NGX stage frame {}: first evaluation returned in {:.3f} ms",
                        sequence,std::chrono::duration<double,std::milli>(
                            std::chrono::steady_clock::now()-evaluationStarted).count());
                if(const auto error=std::get_if<Error>(&evaluated)) {
                    if(error->code==ErrorCode::DeviceRemoved)return *error;
                    return Error{ErrorCode::Unavailable,error->message};
                }
                const auto token=std::get<std::optional<SrEvaluationToken>>(evaluated);
                if(!token)return false;
                if(state->captureFirstDlssFrame&&!state->ownedSrStageAttempted) {
                    state->ownedSrStageAttempted=true;
                    auto stages=state->srPresenter.captureEvaluated(context,*token);
                    if(const auto error=std::get_if<Error>(&stages))
                        spdlog::warn("Owned SR three-stage capture frame {} could not read prepared input/output: {}",
                            sequence,error->message);
                    else {
                        std::scoped_lock lock(state->ownedSrStageMutex);
                        state->ownedSrStages.emplace(WorldState::OwnedSrStageCapture{
                            sequence,std::move(std::get<std::vector<ProbeImage>>(stages))});
                        state->presentTargetProbeDue.store(true,std::memory_order_release);
                        spdlog::info("Owned SR three-stage capture frame {} recorded prepared input and raw DLSS output; awaiting final pre-Present composition",
                            sequence);
                    }
                }
                const auto published=state->srPresenter.publishEvaluated(context,*token,display);
                if(state->ownedEvaluationLimit&&
                   state->srPresenter.submittedFrames()==state->ownedEvaluationLimit&&
                   std::holds_alternative<bool>(published)&&std::get<bool>(published))
                    spdlog::info("Owned bounded-evaluation diagnostic reached {} DLSS frames at world frame {}; subsequent frames use spatial fallback",
                        state->ownedEvaluationLimit,sequence);
                return published;
            });
        if(const auto error=std::get_if<Error>(&presented))
            throw std::runtime_error(error->message);
        auto outcome=std::move(std::get<SdrSrFrameResult>(presented));
        if(sequence<=3&&outcome.providerFailure())
            spdlog::warn("Owned world DLSS frame {} unavailable: {}",
                sequence,outcome.providerFailure()->message);
        if(outcome.mode()==SdrSrFrameMode::Provider) {
            if(boundary==OwnedPublicationBoundary::MenuDisplay&&
               state->captureFirstDlssFrame&&!state->menuUiSequenceAttempted) {
                std::scoped_lock lock(state->menuUiSequenceMutex);
                state->menuUiSequence.emplace(WorldState::MenuUiSequence{
                    sequence,0,{},{}});
                state->menuUiSequenceAttempted=true;
                spdlog::info("Owned UI menu-sequence armed on first native-boundary DLSS frame {}",
                    sequence);
            }
            if(boundary==OwnedPublicationBoundary::MenuDisplay&&
               state->menuProviderAdmissionGeneration!=domain->plan().generation) {
                state->menuProviderAdmissionGeneration=domain->plan().generation;
                spdlog::info("Owned menu provider admission latched for generation {} at frame {}; later sparse probe rejection will not select spatial fallback",
                    domain->plan().generation,sequence);
            }
            state->displayedMode.store(DisplayMode::DlssSr,std::memory_order_release);
            state->statusDlssFrames.store(state->srPresenter.submittedFrames(),
                std::memory_order_relaxed);
        } else {
            state->displayedMode.store(DisplayMode::SpatialFallback,std::memory_order_release);
            state->srPresenter.requestReset();
            state->ownedFallbacks.emplace_back(std::move(outcome));
            ++state->srSkipped;
        }
        if(sequence==1&&state->captureFirstDlssFrame) {
            probeOwnedPixels("post-world",context,scene,display);
            state->ownedPrePresentProbes.store(2,std::memory_order_release);
        }
        if(FAILED(ui->commitPublishedUi(sequence))||ui->compatibilityFault())
            throw std::runtime_error("Owned native publication or context compatibility failed");
        if(boundary==OwnedPublicationBoundary::PrePresent&&
           !domain->closePublishedFrame(sequence,domain->plan().generation))
            throw std::runtime_error("Owned pre-Present frame did not close");
        state->statusWidth.store(domain->plan().render.width,std::memory_order_relaxed);
        state->statusHeight.store(domain->plan().render.height,std::memory_order_relaxed);
        state->statusSkippedFrames.store(state->srSkipped,std::memory_order_relaxed);
        if(sequence<=3||sequence%600==0)
            spdlog::info("Owned {} publication frame {}: source={}x{} native={}x{} flipIndex={} mode={} providerSubmissions={} fallbacksInFlight={}",
                boundary==OwnedPublicationBoundary::MenuDisplay?"menu-boundary":"pre-Present",
                sequence,domain->plan().render.width,domain->plan().render.height,
                domain->plan().display.width,domain->plan().display.height,
                std::get<NativeFlipTarget>(native).index,
                static_cast<unsigned>(state->displayedMode.load(std::memory_order_relaxed)),
                state->srPresenter.submittedFrames(),state->ownedFallbacks.size());
    } catch(const std::exception& error) {
        state->srDisabled=true;
        state->statusDlssDisabled.store(true,std::memory_order_release);
        domain->suspend();
        try {spdlog::warn("Owned world SR suspended after frame {}: {}",sequence,error.what());}
        catch(...) {}
    } catch(...) {
        state->srDisabled=true;
        state->statusDlssDisabled.store(true,std::memory_order_release);
        domain->suspend();
        try {spdlog::warn("Owned world SR suspended after frame {}",sequence);}catch(...) {}
    }
    return true;
}
#endif
void beforeMenuDisplay(void*,std::uint32_t,std::uint32_t,std::uint32_t) noexcept {
#ifdef RK_WITH_NGX
    auto* state=active.load(std::memory_order_acquire);
    auto* domain=activeOwnedSceneDomain();
    const auto frame=state?state->forwarded.load(std::memory_order_relaxed):0;
    if(state&&domain&&domain->phase()==ScenePhase::World) {
        if(auto* ui=ownedUiRedirector()) {
          try {
            if(inventoryMenuOnStack(state)) {
                if(frame!=state->inventoryMenuLastFrame) {
                    state->inventoryMenuStableFrames=
                        frame==state->inventoryMenuLastFrame+1?
                            std::min(state->inventoryMenuStableFrames+1,30u):1u;
                    state->inventoryMenuLastFrame=frame;
                }
                if(state->inventoryMenuStableFrames==30&&
                   !state->menuBoundaryCaptureAttempted&&
                   state->inventoryTraceWindowFrames<180&&
                   ui->beginInventoryTrace(frame)) {
                    state->inventoryTraceFrame=frame;
                    if(++state->inventoryTraceWindowFrames==1)
                        spdlog::info("Sustained InventoryMenu trace window began at frame {} after 30 consecutive menu frames",
                            frame);
                }
            } else {
                state->inventoryMenuStableFrames=0;
                state->inventoryMenuLastFrame=frame;
            }
            if(magicMenuOnStack(state)) {
                if(frame!=state->magicMenuLastFrame) {
                    state->magicMenuStableFrames=
                        frame==state->magicMenuLastFrame+1?
                            std::min(state->magicMenuStableFrames+1,30u):1u;
                    state->magicMenuLastFrame=frame;
                }
            } else {
                state->magicMenuStableFrames=0;
                state->magicMenuLastFrame=frame;
            }
            if(ui->resumeLatePassRouting(frame,state->ownedSceneGate.ready()))
                spdlog::info("Owned native UI late route resumed at frame {} after scene admission and 120-frame cooldown",
                    frame);
            if(ui->beginFaultTrace(frame))
                spdlog::info("Owned UI post-fault bind trace started at frame {}; observation only",
                    frame);
            if(frame<=12)ui->beginObservation(frame);
            if(frame>12&&state->ownedSceneGate.ready()&&
               ui->latePassRoutingAvailable()&&
               state->menuSceneGate.needsSample(frame,domain->plan().generation)) {
                const auto numbers=readWorldNumbers(
                    reinterpret_cast<void*>(state->expectedRenderer),
                    state->expectedRenderer);
                const auto sample=numbers.valid&&
                    numbers.lockOwner==GetCurrentThreadId()&&numbers.lockRecursion>0&&
                    numbers.device==state->createdDevice.load(std::memory_order_acquire)&&
                    numbers.context==state->createdContext.load(std::memory_order_acquire)&&
                    numbers.swap==state->createdSwap.load(std::memory_order_acquire)&&
                    numbers.depth&&activeOwnedSceneTexture()?
                    probeOwnedScene(reinterpret_cast<ID3D11DeviceContext*>(numbers.context),
                        activeOwnedSceneTexture(),
                        reinterpret_cast<ID3D11Texture2D*>(numbers.depth),
                        domain->plan().render):
                    Result<OwnedSceneSample>{Error{ErrorCode::Unavailable,
                        "Menu-boundary renderer ownership or guide chain differs"}};
                const auto* stats=std::get_if<OwnedSceneSample>(&sample);
                state->menuSceneGate.record(frame,
                    stats?std::optional<ColorSampleStats>{stats->color}:std::nullopt,
                    stats?std::optional<DepthSampleStats>{stats->depth}:std::nullopt);
                try {
                    if(stats)
                        spdlog::info("Owned menu-boundary admission frame {}: sceneLike={} worldLike={} ready={}",
                            frame,stats->color.sceneLike(),stats->depth.worldLike(),
                            state->menuSceneGate.ready());
                    else
                        spdlog::warn("Owned menu-boundary admission frame {} unavailable: {}",
                            frame,std::get<Error>(sample).message);
                } catch(...) {}
            }
            const bool coldTitle=titleMenuOnStack(state);
            const bool coldLoading=namedMenuOnStack(state,RE::LoadingMenu::MENU_NAME);
            bool coldMenuReady=false;
            if(frame>12&&!state->ownedSceneGate.ready()&&
               (coldTitle||coldLoading)&&ui->latePassRoutingAvailable()&&
               !ui->compatibilityFault()) {
                // Cold title/loading has no world-like depth, so NGX stays
                // gated. Publish its scene spatially before native Scaleform.
                const auto numbers=readWorldNumbers(
                    reinterpret_cast<void*>(state->expectedRenderer),
                    state->expectedRenderer);
                coldMenuReady=numbers.valid&&
                    numbers.lockOwner==GetCurrentThreadId()&&
                    numbers.lockRecursion>0&&
                    numbers.device==state->createdDevice.load(std::memory_order_acquire)&&
                    numbers.context==state->createdContext.load(std::memory_order_acquire)&&
                    numbers.swap==state->createdSwap.load(std::memory_order_acquire)&&
                    numbers.motion&&numbers.depth&&activeOwnedSceneTexture();
                if(coldMenuReady&&coldTitle&&!state->ownedNgxCreatedAt&&
                   !state->ownedNgxInitFailed&&!state->ownedSpatialBaseline&&
                   !state->ownedInputCaptureOnly) {
                    const auto started=std::chrono::steady_clock::now();
                    auto created=state->srPresenter.createReducedFeature(
                        reinterpret_cast<ID3D11Device*>(numbers.device),
                        reinterpret_cast<ID3D11DeviceContext*>(numbers.context));
                    if(const auto error=std::get_if<Error>(&created)) {
                        state->ownedNgxInitFailed=true;
                        spdlog::warn("Owned NGX cold-title prewarm failed at frame {}: {}",
                            frame,error->message);
                    } else if(std::get<bool>(created)) {
                        state->ownedNgxCreatedAt=frame;
                        spdlog::info("Owned NGX feature prewarmed on cold title at frame {} in {:.3f} ms; evaluation remains gated until world admission and 120 frames",
                            frame,std::chrono::duration<double,std::milli>(
                                std::chrono::steady_clock::now()-started).count());
                    } else state->ownedNgxInitFailed=true;
                }
            }
            if(frame>12&&(coldMenuReady||shouldUseMenuPublication(
                   state->nativeUiRouteActivated,state->menuSceneGate.ready(),
                   ui->latePassRoutingAvailable()))) {
                if(processOwnedWorldFrame(state,
                    reinterpret_cast<void*>(state->expectedRenderer),frame,
                    OwnedPublicationBoundary::MenuDisplay)&&
                   domain->phase()==ScenePhase::NativeUi) {
                    if(coldMenuReady&&coldTitle&&!state->coldTitleUiRouteLogged) {
                        state->coldTitleUiRouteLogged=true;
                        try {spdlog::info("Owned cold Main Menu spatial publication entered native UI at frame {}; provider remains gated until world admission",
                            frame);}catch(...) {}
                    } else if(coldMenuReady&&coldLoading&&
                              !state->coldLoadingUiRouteLogged) {
                        state->coldLoadingUiRouteLogged=true;
                        try {spdlog::info("Owned cold Loading Menu spatial publication entered native UI at frame {}; provider remains gated until world admission",
                            frame);}catch(...) {}
                    } else if(!coldMenuReady&&!state->nativeUiRouteActivated) {
                        state->nativeUiRouteActivated=true;
                        try {spdlog::info("Owned native UI resource route activated at frame {} after same-boundary colour/depth admission; DLSS submissions={}",
                            frame,state->srPresenter.submittedFrames());}catch(...) {}
                    }
                }
            }
          } catch(const std::exception& error) {
            state->menuSceneGate.record(frame,std::nullopt,std::nullopt);
            state->srPresenter.requestReset();
            try {spdlog::warn("Owned menu-boundary admission frame {} failed safely: {}",
                frame,error.what());}catch(...) {}
          } catch(...) {
            state->menuSceneGate.record(frame,std::nullopt,std::nullopt);
            state->srPresenter.requestReset();
            try {spdlog::warn("Owned menu-boundary admission frame {} failed safely",
                frame);}catch(...) {}
          }
          logInventoryBinding(state,frame,"before-menu-prep");
        }
    }
    if(state) {
        try {captureMenuUiEntry(state,frame);}
        catch(const std::exception& error) {
            try {spdlog::warn("Owned UI menu-sequence entry capture failed: {}",
                error.what());}catch(...) {}
        } catch(...) {}
    }
    if(armLoadingUiTrace(state,frame))
        logLoadingUiBoundary(state,frame,"before-first-PostDisplay");
    beginNativeHudMovieViewportWindow(state,frame);
#endif
}
void menuDisplayProxy(void* first,std::uint32_t second,std::uint32_t third,
    std::uint32_t fourth) noexcept {
    auto* state=active.load(std::memory_order_acquire);
    if(!state)std::terminate();
    state->menuForwarder.dispatch(first,second,third,fourth);
    logInventoryBinding(state,state->forwarded.load(std::memory_order_relaxed),
        "after-menu-prep-before-PostDisplay");
}
void beforeDeferredUiFlush(void*) noexcept {
#ifdef RK_WITH_NGX
    auto* state=active.load(std::memory_order_acquire);
    auto* domain=activeOwnedSceneDomain();
    const auto loadingFrame=state?state->forwarded.load(std::memory_order_relaxed):0;
    logLoadingUiBoundary(state,loadingFrame,"before-Scaleform-EndFrame");
    if(!state||!domain||domain->phase()!=ScenePhase::NativeUi)return;
    auto* ui=ownedUiRedirector();
    if(!ui)return;
    const auto frame=state->forwarded.load(std::memory_order_relaxed);
    bool preservedNativeMenuComposite=false;
    bool preservedInventoryComposite=false;
    bool preservedMagicComposite=false;
    bool preservedMainMenuComposite=false;
    if(ui->reducedMenuPassPending()) {
        preservedInventoryComposite=inventoryMenuOnStack(state);
        auto* magicMenu=preservedInventoryComposite?nullptr:
            menuInstanceOnStack(state,RE::MagicMenu::MENU_NAME);
        preservedMagicComposite=magicMenu&&magicMenu->uiMovie;
        preservedMainMenuComposite=!preservedInventoryComposite&&
            !preservedMagicComposite&&
            (titleMenuOnStack(state)||
             (namedMenuOnStack(state,RE::LoadingMenu::MENU_NAME)&&
              state->srPresenter.submittedFrames()==0&&
              !state->ownedSceneGate.ready()));
        if(preservedInventoryComposite||preservedMagicComposite||
           preservedMainMenuComposite) {
            // The MagicMenu capture showed its movie only in the reduced
            // image, while world/hero were already in native colour. Replay
            // that movie after rebinding rather than copying blurry UI over
            // the native scene.
            if(preservedInventoryComposite)
                state->menuBoundaryCaptureAttempted=true;
            preservedNativeMenuComposite=true;
            if(preservedInventoryComposite) {
                const auto count=++state->preservedInventoryComposites;
                if(count==1||count%600==0)
                    spdlog::info("Inventory frame {} retained native UI composite instead of overwriting it with the reduced menu scene; count={}",
                        frame,count);
            } else if(preservedMagicComposite) {
                const auto count=++state->preservedMagicComposites;
                if(count==1||count%600==0)
                    spdlog::info("Magic frame {} retained native world/hero for Scaleform movie replay; count={}",
                        frame,count);
            } else {
                const auto count=++state->preservedMainMenuComposites;
                if(count==1||count%600==0)
                    spdlog::info("Main Menu frame {} retained native UI composite instead of overwriting it with the reduced menu scene; count={}",
                        frame,count);
            }
        } else {
        try {
            std::optional<WorldState::MenuBoundaryCapture> capture;
            Microsoft::WRL::ComPtr<ID3D11Texture2D> captureTarget;
            const bool captureMagic=magicMenuOnStack(state)&&
                state->magicMenuStableFrames==30&&
                !state->magicBoundaryCaptureAttempted;
            const bool captureInventory=!state->menuBoundaryCaptureAttempted&&
                state->inventoryTraceFrame==frame;
            if(captureMagic||captureInventory) {
                if(captureMagic)state->magicBoundaryCaptureAttempted=true;
                else state->menuBoundaryCaptureAttempted=true;
                const char* captureName=captureMagic?"Magic":"Inventory";
                const auto device=state->createdDevice.load(std::memory_order_relaxed);
                const auto context=state->createdContext.load(std::memory_order_relaxed);
                const auto swap=state->createdSwap.load(std::memory_order_relaxed);
                auto* scene=activeOwnedSceneTexture();
                if(device&&context&&swap&&scene) {
                    auto native=acquireNativeFlipTarget(
                        reinterpret_cast<IDXGISwapChain*>(swap),
                        reinterpret_cast<ID3D11Device*>(device),domain->plan().display);
                    if(auto* target=std::get_if<NativeFlipTarget>(&native)) {
                        captureTarget=target->texture;
                        const std::array<ID3D11Texture2D*,2> sources{
                            scene,captureTarget.Get()};
                        auto before=readbackCandidates(
                            reinterpret_cast<ID3D11DeviceContext*>(context),
                            sources,24*1024*1024);
                        if(auto* images=std::get_if<std::vector<ProbeImage>>(&before))
                            capture.emplace(WorldState::MenuBoundaryCapture{
                                frame,std::move(*images),false,captureMagic});
                        else
                            spdlog::warn("{} boundary capture before copy unavailable: {}",captureName,
                                std::get<Error>(before).message);
                    } else
                        spdlog::warn("{} boundary capture target unavailable: {}",captureName,
                            std::get<Error>(native).message);
                }
            }
            auto late=ui->publishHeldMenuScene(frame);
            if(auto* fallback=std::get_if<SpatialFallbackFrame>(&late)) {
                if(capture&&captureTarget) {
                    const std::array<ID3D11Texture2D*,1> sources{
                        captureTarget.Get()};
                    auto after=readbackCandidates(
                        reinterpret_cast<ID3D11DeviceContext*>(
                            state->createdContext.load(std::memory_order_relaxed)),
                        sources,16*1024*1024);
                    if(auto* images=std::get_if<std::vector<ProbeImage>>(&after)) {
                        capture->images.emplace_back(std::move(images->front()));
                        const bool magic=capture->magic;
                        state->menuBoundaryCapture.emplace(std::move(*capture));
                        spdlog::info("{} boundary frame {} captured reduced source and native before/after late copy; awaiting pre-Present",
                            magic?"Magic":"Inventory",frame);
                    } else
                        spdlog::warn("{} boundary capture after copy unavailable: {}",
                            capture->magic?"Magic":"Inventory",
                            std::get<Error>(after).message);
                }
                state->ownedFallbacks.emplace_back(SdrSrFrameMode::SpatialFallback,
                    std::optional<SpatialFallbackFrame>{std::move(*fallback)},
                    std::nullopt);
                state->displayedMode.store(DisplayMode::SpatialFallback,
                    std::memory_order_release);
                const auto count=++state->deferredMenuPublications;
                if(count==1||count%600==0)
                    spdlog::info("Deferred reduced menu scene published before native Scaleform at frame {}; count={}",
                        frame,count);
            } else if(!state->deferredMenuPublicationWarningLogged) {
                state->deferredMenuPublicationWarningLogged=true;
                spdlog::warn("Deferred reduced menu scene could not publish at frame {}: {}",
                    frame,std::get<Error>(late).message);
            }
        } catch(...) {
            state->deferredMenuPublicationWarningLogged=true;
            try {spdlog::warn("Deferred reduced menu scene publication threw at frame {}",
                frame);}catch(...) {}
        }
        }
    }
    if(state->srPresenter.submittedFrames()>0&&
       state->hudUiTraceCount<3&&
       !inventoryMenuOnStack(state)&&!magicMenuOnStack(state)&&
       !titleMenuOnStack(state)) {
        state->hudUiTraceFrame=frame;
        ++state->hudUiTraceCount;
        logHudUiBoundary(state,frame,"before-native-rebind");
    }
    const auto rebound=ui->rebindForDeferredUiFlush(frame);
    if(SUCCEEDED(rebound))logHudMovieViewports(state,frame);
    logHudUiBoundary(state,frame,"before-Scaleform-EndFrame");
    if(SUCCEEDED(rebound)) {
        if(rebound==S_FALSE&&!state->deferredUiFlushMotionWarningLogged) {
            state->deferredUiFlushMotionWarningLogged=true;
            try {spdlog::info("Deferred Scaleform UI flush frame {} restored native colour after the reduced menu pass",
                frame);}catch(...) {}
        }
        const auto count=++state->deferredUiFlushRebinds;
        if(count==1||count%600==0) {
            try {spdlog::info("Deferred Scaleform UI flush frame {} reasserted native colour and full-size depth/stencil; count={}",
                frame,count);}catch(...) {}
        }
    } else if(!state->deferredUiFlushWarningLogged) {
        state->deferredUiFlushWarningLogged=true;
        try {spdlog::warn("Deferred Scaleform UI flush frame {} could not reassert native depth/stencil; HRESULT=0x{:08x}",
            frame,static_cast<std::uint32_t>(rebound));}catch(...) {}
    }
    if(preservedNativeMenuComposite&&SUCCEEDED(rebound)) {
        if(preservedMagicComposite) {
            if(auto* magic=menuInstanceOnStack(state,RE::MagicMenu::MENU_NAME);
               magic&&magic->uiMovie) {
                std::optional<WorldState::CursorReplayCapture> capture;
                Microsoft::WRL::ComPtr<ID3D11Texture2D> captureTarget;
                if(!state->magicReplayCaptureAttempted&&
                   state->magicMenuStableFrames==30) {
                    state->magicReplayCaptureAttempted=true;
                    const auto device=state->createdDevice.load(std::memory_order_relaxed);
                    const auto context=state->createdContext.load(std::memory_order_relaxed);
                    const auto swap=state->createdSwap.load(std::memory_order_relaxed);
                    if(device&&context&&swap) {
                        auto native=acquireNativeFlipTarget(
                            reinterpret_cast<IDXGISwapChain*>(swap),
                            reinterpret_cast<ID3D11Device*>(device),domain->plan().display);
                        if(auto* target=std::get_if<NativeFlipTarget>(&native)) {
                            captureTarget=target->texture;
                            const std::array<ID3D11Texture2D*,1> source{
                                captureTarget.Get()};
                            auto before=readbackCandidates(
                                reinterpret_cast<ID3D11DeviceContext*>(context),
                                source,16*1024*1024);
                            if(auto* images=std::get_if<std::vector<ProbeImage>>(&before))
                                capture.emplace(WorldState::CursorReplayCapture{
                                    frame,std::move(*images),true});
                        }
                    }
                }
                const auto count=++state->magicMovieReplays;
                if(count==1||count%600==0)
                    spdlog::info("Magic frame {} replaying Scaleform movie on native colour; count={}",
                        frame,count);
                try {magic->uiMovie->Display();}
                catch(...) {
                    try {spdlog::warn("Magic frame {} Scaleform movie replay threw",
                        frame);}catch(...) {}
                }
                if(capture&&captureTarget) {
                    const std::array<ID3D11Texture2D*,1> source{
                        captureTarget.Get()};
                    auto after=readbackCandidates(
                        reinterpret_cast<ID3D11DeviceContext*>(
                            state->createdContext.load(std::memory_order_relaxed)),
                        source,16*1024*1024);
                    if(auto* images=std::get_if<std::vector<ProbeImage>>(&after)) {
                        capture->images.emplace_back(std::move(images->front()));
                        state->cursorReplayCapture.emplace(std::move(*capture));
                    }
                }
            }
        }
        if(auto* cursor=cursorMenuOnStack(state);cursor&&cursor->uiMovie) {
            std::optional<WorldState::CursorReplayCapture> capture;
            Microsoft::WRL::ComPtr<ID3D11Texture2D> captureTarget;
            if(preservedInventoryComposite&&!state->cursorReplayCaptureAttempted&&
               state->inventoryMenuStableFrames==30) {
                state->cursorReplayCaptureAttempted=true;
                const auto device=state->createdDevice.load(std::memory_order_relaxed);
                const auto context=state->createdContext.load(std::memory_order_relaxed);
                const auto swap=state->createdSwap.load(std::memory_order_relaxed);
                if(device&&context&&swap) {
                    auto native=acquireNativeFlipTarget(
                        reinterpret_cast<IDXGISwapChain*>(swap),
                        reinterpret_cast<ID3D11Device*>(device),domain->plan().display);
                    if(auto* target=std::get_if<NativeFlipTarget>(&native)) {
                        captureTarget=target->texture;
                        const std::array<ID3D11Texture2D*,1> source{
                            captureTarget.Get()};
                        auto before=readbackCandidates(
                            reinterpret_cast<ID3D11DeviceContext*>(context),
                            source,16*1024*1024);
                        if(auto* images=std::get_if<std::vector<ProbeImage>>(&before))
                            capture.emplace(WorldState::CursorReplayCapture{
                                frame,std::move(*images)});
                        else spdlog::warn("Inventory frame {} cursor replay before-readback unavailable: {}",
                            frame,std::get<Error>(before).message);
                    }
                }
            }
            const auto count=++state->cursorReplays;
            if(count==1||count%600==0)
                spdlog::info("{} frame {} replaying Skyrim Cursor Menu after native UI rebind; count={}",
                    preservedInventoryComposite?"Inventory":
                        preservedMagicComposite?"Magic":"Main Menu",frame,count);
            try {cursor->PostDisplay();}
            catch(...) {
                try {spdlog::warn("{} frame {} Cursor Menu replay threw",
                    preservedInventoryComposite?"Inventory":
                        preservedMagicComposite?"Magic":"Main Menu",frame);}catch(...) {}
            }
            if(capture&&captureTarget) {
                const std::array<ID3D11Texture2D*,1> source{
                    captureTarget.Get()};
                auto after=readbackCandidates(
                    reinterpret_cast<ID3D11DeviceContext*>(
                        state->createdContext.load(std::memory_order_relaxed)),
                    source,16*1024*1024);
                if(auto* images=std::get_if<std::vector<ProbeImage>>(&after)) {
                    capture->images.emplace_back(std::move(images->front()));
                    state->cursorReplayCapture.emplace(std::move(*capture));
                } else spdlog::warn("Inventory frame {} cursor replay after-readback unavailable: {}",
                    frame,std::get<Error>(after).message);
            }
        } else if(!state->cursorReplayMissingLogged) {
            state->cursorReplayMissingLogged=true;
            try {spdlog::warn("{} frame {} Cursor Menu is not available for native replay",
                preservedInventoryComposite?"Inventory":
                    preservedMagicComposite?"Magic":"Main Menu",frame);}catch(...) {}
        }
    }
    logInventoryBinding(state,frame,"before-EndFrame");
#endif
}
void deferredUiFlushProxy(void* renderer) noexcept {
    auto* state=active.load(std::memory_order_acquire);
    if(!state)std::terminate();
    state->deferredUiFlushForwarder.dispatch(renderer);
#ifdef RK_WITH_NGX
    finishNativeHudMovieViewportWindow(state);
    const auto frame=state->forwarded.load(std::memory_order_relaxed);
    logLoadingUiBoundary(state,frame,"after-Scaleform-EndFrame");
    logHudUiBoundary(state,frame,"after-Scaleform-EndFrame");
    logInventoryBinding(state,frame,"after-EndFrame");
    if(state->cursorReplayCapture&&
       state->cursorReplayCapture->frame==frame&&
       state->cursorReplayCapture->images.size()==2) {
        auto capture=std::move(*state->cursorReplayCapture);
        state->cursorReplayCapture.reset();
        try {
            const auto device=state->createdDevice.load(std::memory_order_relaxed);
            const auto context=state->createdContext.load(std::memory_order_relaxed);
            const auto swap=state->createdSwap.load(std::memory_order_relaxed);
            auto* domain=activeOwnedSceneDomain();
            if(device&&context&&swap&&domain) {
                auto native=acquireNativeFlipTarget(
                    reinterpret_cast<IDXGISwapChain*>(swap),
                    reinterpret_cast<ID3D11Device*>(device),domain->plan().display);
                if(auto* target=std::get_if<NativeFlipTarget>(&native)) {
                    const std::array<ID3D11Texture2D*,1> source{target->texture.Get()};
                    auto after=readbackCandidates(
                        reinterpret_cast<ID3D11DeviceContext*>(context),
                        source,16*1024*1024);
                    if(auto* images=std::get_if<std::vector<ProbeImage>>(&after)) {
                        capture.images.emplace_back(std::move(images->front()));
                        PWSTR documents=nullptr;
                        const auto found=SHGetKnownFolderPath(FOLDERID_Documents,
                            KF_FLAG_DEFAULT,nullptr,&documents);
                        struct FreeDocuments {
                            PWSTR value;~FreeDocuments(){CoTaskMemFree(value);}
                        } free{documents};
                        if(SUCCEEDED(found)&&documents) {
                            const auto directory=std::filesystem::path(documents)/
                                "My Games"/"Skyrim Special Edition"/"SKSE"/
                                "RazKolbasCaptures"/
                                (std::string(capture.magic?"magic-movie-replay-":
                                    "cursor-replay-")+
                                std::to_string(GetCurrentProcessId())+"-"+
                                std::to_string(frame)+"-"+
                                std::to_string(GetTickCount64()));
                            const std::array<std::string_view,3> names=
                                capture.magic?
                                std::array<std::string_view,3>{
                                    "native-before-magic-replay.raw",
                                    "native-after-magic-replay.raw",
                                    "native-after-endframe.raw"}:
                                std::array<std::string_view,3>{
                                    "native-before-cursor-replay.raw",
                                    "native-after-cursor-replay.raw",
                                    "native-after-endframe.raw"};
                            const auto saved=saveProbeBundle(directory,
                                capture.images,names,
                                capture.magic?
                                    "One MagicMenu frame around native movie replay; diagnostic only":
                                    "One InventoryMenu frame around native Cursor Menu replay; diagnostic only");
                            if(const auto error=std::get_if<Error>(&saved))
                                spdlog::warn("{} replay capture save failed: {}",
                                    capture.magic?"Magic movie":"Inventory cursor",
                                    error->message);
                            else spdlog::info("{} replay frame {} capture complete at {}",
                                capture.magic?"Magic movie":"Inventory cursor",
                                frame,directory.string());
                        }
                    }
                }
            }
        }catch(...) {
            try {spdlog::warn("{} replay frame {} capture failed",
                capture.magic?"Magic movie":"Inventory cursor",
                frame);}catch(...) {}
        }
    }
    if(state->menuBoundaryCapture&&state->menuBoundaryCapture->frame==frame&&
       state->menuBoundaryCapture->images.size()==3) {
        try {
            const auto device=state->createdDevice.load(std::memory_order_relaxed);
            const auto context=state->createdContext.load(std::memory_order_relaxed);
            const auto swap=state->createdSwap.load(std::memory_order_relaxed);
            auto* domain=activeOwnedSceneDomain();
            if(device&&context&&swap&&domain) {
                auto native=acquireNativeFlipTarget(
                    reinterpret_cast<IDXGISwapChain*>(swap),
                    reinterpret_cast<ID3D11Device*>(device),domain->plan().display);
                if(auto* target=std::get_if<NativeFlipTarget>(&native)) {
                    const std::array<ID3D11Texture2D*,1> source{target->texture.Get()};
                    auto image=readbackCandidates(
                        reinterpret_cast<ID3D11DeviceContext*>(context),
                        source,16*1024*1024);
                    if(auto* images=std::get_if<std::vector<ProbeImage>>(&image)) {
                        state->menuBoundaryCapture->images.emplace_back(
                            std::move(images->front()));
                        state->menuBoundaryCapture->afterEndFrameCaptured=true;
                    } else spdlog::warn("{} frame {} after-EndFrame readback unavailable: {}",
                        state->menuBoundaryCapture->magic?"Magic":"Inventory",
                        frame,std::get<Error>(image).message);
                }
            }
        }catch(...) {
            try {spdlog::warn("{} frame {} after-EndFrame readback threw",
                state->menuBoundaryCapture->magic?"Magic":"Inventory",
                frame);}catch(...) {}
        }
    }
#endif
}
void worldDrawProxy(void* world,std::uint32_t flags) noexcept {
    auto* state=active.load(std::memory_order_acquire);
    const auto sequence=state->forwarded.load(std::memory_order_relaxed)+1;
    const bool sample=sequence<=3||sequence%600==0;
    const auto before=sample?readWorldNumbers(world,state->expectedRenderer):WorldNumbers{};
    state->forwarder.dispatch(world,flags);
    state->displayedMode.store(DisplayMode::Native,std::memory_order_release);
#ifdef RK_WITH_NGX
    applyPendingNrRuntimeSettings(state);
    if(!activeOwnedSceneDomain())if(const auto update=consumeDiagnosticsSharpeningUpdate()) {
        const auto configuredSr=state->srPresenter.configureSharpness(
            update->enabled,update->sharpness);
        const auto configuredDlaa=state->sdrPresenter.configureSharpness(
            update->enabled,update->sharpness);
        if(const auto srError=std::get_if<Error>(&configuredSr))
            try { spdlog::warn("Live SR sharpening update rejected: {}",srError->message); } catch(...) {}
        else if(const auto dlaaError=std::get_if<Error>(&configuredDlaa))
            try { spdlog::warn("Live DLAA sharpening update rejected: {}",dlaaError->message); } catch(...) {}
        else {
            state->srSharpness.store(update->enabled?update->sharpness:0.0f,
                std::memory_order_release);
            try { spdlog::info("Live post-DLSS sharpening changed: enabled={}; sharpness={}",
                update->enabled,update->sharpness); } catch(...) {}
        }
    }
    if(activeOwnedSceneDomain()||ownedScenePreviouslyActive())return;
#endif
    std::array<float,4> drsRatios{};
    const bool drsRead=state->jitterCamera&&
        read(state->jitterCamera+0x104,drsRatios.data(),sizeof(drsRatios));
    const bool nativeRatios=drsRead&&nativeDlaaRatiosReady(
        drsRatios[0],drsRatios[1],drsRatios[2],drsRatios[3]);
    std::optional<std::uint64_t> stableDrsGeneration;
    if(drsRead) {
        std::scoped_lock lock(state->drsTupleMutex);
        stableDrsGeneration=state->drsTupleGate.observe(drsRatios);
    }
    if(!nativeRatios) {
        if(!state->drsSuppressed.exchange(true,std::memory_order_acq_rel)) {
#ifdef RK_WITH_NGX
            state->sdrPresenter.requestReset();
#endif
            try { spdlog::warn("Native DLAA suspended: engine DRS ratios current=({},{}), previous=({},{}), read={}; measuring world-pass inputs",
                drsRatios[0],drsRatios[1],drsRatios[2],drsRatios[3],drsRead); } catch(...) {}
        }
    } else if(state->drsSuppressed.exchange(false,std::memory_order_acq_rel)) {
        state->srSourceVerified=false;
#ifdef RK_WITH_NGX
        state->sdrPresenter.requestReset();
#endif
        try { spdlog::info("Engine DRS ratios returned to native; DLAA history reset"); } catch(...) {}
    }
    if(stableDrsGeneration&&!nativeRatios) {
        state->srSourceVerified=false;
        const bool settled=std::abs(drsRatios[0]-drsRatios[2])<=0.0001f&&
            std::abs(drsRatios[1]-drsRatios[3])<=0.0001f;
        try { spdlog::info("Engine DRS tuple stable: generation={} worldFrame={} current=({},{}), previous=({},{}), settled={}; post-world diagnostic only",
            *stableDrsGeneration,sequence,drsRatios[0],drsRatios[1],
            drsRatios[2],drsRatios[3],settled); } catch(...) {}
        if(settled) {
            const auto numbers=readWorldNumbers(world,state->expectedRenderer);
            if(numbers.valid&&numbers.lockOwner==GetCurrentThreadId()&&
               numbers.lockRecursion>0&&numbers.colour&&numbers.motion&&numbers.depth&&
               numbers.device==state->createdDevice.load(std::memory_order_acquire)&&
               numbers.context==state->createdContext.load(std::memory_order_acquire)) {
                try {
                    D3D11_TEXTURE2D_DESC colour{},motion{},depth{};
                    auto* colourSource=reinterpret_cast<ID3D11Texture2D*>(numbers.colour);
                    auto* motionSource=reinterpret_cast<ID3D11Texture2D*>(numbers.motion);
                    auto* depthSource=reinterpret_cast<ID3D11Texture2D*>(numbers.depth);
                    colourSource->GetDesc(&colour);motionSource->GetDesc(&motion);
                    depthSource->GetDesc(&depth);
                    auto* immediate=reinterpret_cast<ID3D11DeviceContext*>(numbers.context);
                    D3D11_VIEWPORT viewport{};UINT count=1;
                    immediate->RSGetViewports(&count,&viewport);
                    spdlog::info("Engine DRS stable source descriptors: generation={} colour={}x{} format={} motion={}x{} format={} depth={}x{} format={} viewport={}x{} origin=({},{}); same world frame; no reduced SR submitted",
                        *stableDrsGeneration,colour.Width,colour.Height,static_cast<unsigned>(colour.Format),
                        motion.Width,motion.Height,static_cast<unsigned>(motion.Format),
                        depth.Width,depth.Height,static_cast<unsigned>(depth.Format),
                        viewport.Width,viewport.Height,viewport.TopLeftX,viewport.TopLeftY);
                    Microsoft::WRL::ComPtr<ID3D11Texture2D> backbuffer;
                    const auto got=numbers.swap?
                        reinterpret_cast<IDXGISwapChain*>(numbers.swap)->GetBuffer(0,
                            IID_PPV_ARGS(&backbuffer)):E_INVALIDARG;
                    if(FAILED(got)||!backbuffer)
                        throw std::runtime_error("SDR world source backbuffer unavailable");
                    D3D11_TEXTURE2D_DESC display{};backbuffer->GetDesc(&display);
                    const std::array<ID3D11Texture2D*,3> guides{
                        backbuffer.Get(),motionSource,depthSource};
                    const auto captured=readbackCandidates(immediate,guides,48*1024*1024);
                    if(const auto error=std::get_if<Error>(&captured))
                        spdlog::warn("Engine DRS same-frame guide readback unavailable: {}",error->message);
                    else {
                        const auto& images=std::get<std::vector<ProbeImage>>(captured);
                        const auto target=engineDrsTarget(display.Width,display.Height,
                            drsRatios[0],drsRatios[1]);
                        const bool matchingGuides=target&&
                            display.Format==DXGI_FORMAT_R8G8B8A8_UNORM&&
                            motion.Width==display.Width&&motion.Height==display.Height&&
                            depth.Width==display.Width&&depth.Height==display.Height;
                        const auto sampled=matchingGuides?
                            sampleWorldDepth(images[2].pixels,target->width,target->height,
                                images[2].rowBytes):Result<DepthSampleStats>{
                                    Error{ErrorCode::Conflict,"DRS guide extents differ"}};
                        const auto depthStats=std::get_if<DepthSampleStats>(&sampled);
                        const bool reducedColour=matchingGuides&&
                            reducedSdrRegionLooksUnscaled(images[0].pixels,
                                display.Width,display.Height,images[0].rowBytes,
                                target->width,target->height);
                        state->srSourceVerified=reducedColour&&depthStats&&
                            depthStats->worldLike();
                        if(state->srSourceVerified) {
                            state->srVerifiedWidth=target->width;
                            state->srVerifiedHeight=target->height;
                            state->srGeneration=*stableDrsGeneration;
                        }
                        spdlog::info("Engine DRS same-frame guides: generation={} worldFrame={} sdrColourSHA256={} motionSHA256={} depthSHA256={} depthDistinct={} depthNonFar={} reducedColour={} SRSourceAccepted={}; post-world pixels",
                            *stableDrsGeneration,sequence,sha256(images[0].pixels),
                            sha256(images[1].pixels),sha256(images[2].pixels),
                            depthStats?depthStats->distinct:0,depthStats?depthStats->nonFar:0,
                            reducedColour,state->srSourceVerified.load(std::memory_order_relaxed));
                    }
                } catch(const std::exception& error) {
                    try { spdlog::warn("Engine DRS stable guide diagnostic failed: {}",error.what()); } catch(...) {}
                } catch(...) {
                    try { spdlog::warn("Engine DRS stable guide diagnostic failed"); } catch(...) {}
                }
            } else try { spdlog::warn("Engine DRS stable guides unavailable at world boundary: generation={} renderer ownership changed",*stableDrsGeneration); } catch(...) {}
        }
    }
    if((sequence<=12||sequence%600==0)&&state->jitterCamera) {
        std::array<std::uint8_t,0x4c> camera{};
        if(read(state->jitterCamera,camera.data(),camera.size())) {
            std::uint32_t width{},height{};
            float jitterX{},jitterY{};
            std::memcpy(&width,camera.data()+0x24,sizeof(width));
            std::memcpy(&height,camera.data()+0x28,sizeof(height));
            std::memcpy(&jitterX,camera.data()+0x44,sizeof(jitterX));
            std::memcpy(&jitterY,camera.data()+0x48,sizeof(jitterY));
            if(width&&height&&width<=8192&&height<=8192&&
               std::isfinite(jitterX)&&std::isfinite(jitterY)&&
               std::abs(jitterX)<=2.0f&&std::abs(jitterY)<=2.0f)
                try { spdlog::info("World jitter observation: frame={} thread={} camera=0x{:x} extent={}x{} projection=({},{}); read-only camera sample",
                    sequence,GetCurrentThreadId(),state->jitterCamera,width,height,jitterX,jitterY); } catch(...) {}
            else try { spdlog::warn("World jitter observation rejected: invalid camera dimensions or offsets; frame={}",sequence); } catch(...) {}
        } else try { spdlog::warn("World jitter observation unavailable: camera read failed; frame={}",sequence); } catch(...) {}
    }
    const auto copyStatus=state->copyStatus.load(std::memory_order_acquire);
    if(copyStatus==WorldState::CopyStatus::Pending||
       (nativeRatios&&copyStatus==WorldState::CopyStatus::NotAttempted))
        copyWorldInputsOnce(state,readWorldNumbers(world,state->expectedRenderer));
    if(sequence>=600&&!state->initialTargetMapDone.load(std::memory_order_acquire)) {
        const auto numbers=readWorldNumbers(world,state->expectedRenderer);
        if(numbers.valid&&numbers.lockOwner==GetCurrentThreadId()&&numbers.lockRecursion>0&&
           numbers.device==state->createdDevice.load(std::memory_order_acquire)&&
           numbers.context==state->createdContext.load(std::memory_order_relaxed)&&
           numbers.swap==state->createdSwap.load(std::memory_order_relaxed)&&
           numbers.colour&&!state->initialTargetMapDone.exchange(true,std::memory_order_acq_rel)) {
            try {
                const auto colourIdentity=identity(reinterpret_cast<IUnknown*>(numbers.colour));
                logTargetBoundary("post-world-initial",reinterpret_cast<ID3D11DeviceContext*>(numbers.context),
                    reinterpret_cast<IDXGISwapChain*>(numbers.swap),colourIdentity);
                state->worldColourIdentity.store(colourIdentity,std::memory_order_relaxed);
                state->presentTargetProbeDue.store(true,std::memory_order_release);
            } catch(const std::exception& error) {
                try { spdlog::warn("Initial target map unavailable: {}",error.what()); } catch(...) {}
            } catch(...) {
                try { spdlog::warn("Initial target map unavailable"); } catch(...) {}
            }
        }
    }
    if(drsProbeHasRun()&&!drsProbeActive()) {
        const auto numbers=readWorldNumbers(world,state->expectedRenderer);
        if(numbers.valid&&numbers.lockOwner==GetCurrentThreadId()&&numbers.lockRecursion>0&&
           numbers.device==state->createdDevice.load(std::memory_order_acquire)&&
           numbers.context==state->createdContext.load(std::memory_order_relaxed)&&
           numbers.swap==state->createdSwap.load(std::memory_order_relaxed)&&
           numbers.colour&&numbers.motion&&numbers.depth) {
            D3D11_TEXTURE2D_DESC colour{},motion{},depth{},display{};
            reinterpret_cast<ID3D11Texture2D*>(numbers.colour)->GetDesc(&colour);
            reinterpret_cast<ID3D11Texture2D*>(numbers.motion)->GetDesc(&motion);
            reinterpret_cast<ID3D11Texture2D*>(numbers.depth)->GetDesc(&depth);
            Microsoft::WRL::ComPtr<ID3D11Texture2D> backbuffer;
            if(SUCCEEDED(reinterpret_cast<IDXGISwapChain*>(numbers.swap)->GetBuffer(0,
                IID_PPV_ARGS(&backbuffer)))&&backbuffer) {
                backbuffer->GetDesc(&display);
                drsProbeConfirmNativeRecovery({colour.Width,colour.Height},
                    {motion.Width,motion.Height},{depth.Width,depth.Height},
                    {display.Width,display.Height});
            }
        }
    }
#ifdef RK_WITH_NGX
    if(state->dlssProbe.pending()&&!state->probeFailed) {
        const auto numbers=readWorldNumbers(world,state->expectedRenderer);
        if(numbers.valid&&numbers.lockOwner==GetCurrentThreadId()&&numbers.lockRecursion>0&&
           numbers.device==state->createdDevice.load(std::memory_order_acquire)&&
           numbers.context==state->createdContext.load(std::memory_order_relaxed)&&
           numbers.swap==state->createdSwap.load(std::memory_order_relaxed)) {
            try {
                const auto polled=state->dlssProbe.poll(reinterpret_cast<ID3D11Device*>(numbers.device),
                    reinterpret_cast<ID3D11DeviceContext*>(numbers.context));
                if(const auto error=std::get_if<Error>(&polled)) {
                    state->probeFailed=true;
                    spdlog::warn("Offscreen DLSS probe failed: {}; owned resources retained",error->message);
                } else if(!std::get<std::string>(polled).empty()) {
                    if(!state->copiedFrame||!state->copiedFrame->output()) {
                        state->probeFailed=true;
                        spdlog::warn("Offscreen DLAA completed without an owned output frame");
                    } else {
                        state->completedOutput.emplace(WorldState::CompletedOutput{
                            state->copiedFrame->takeOutput(),state->dlssProbe.width(),
                            state->dlssProbe.height(),std::get<std::string>(polled)});
                        state->copiedFrame.reset();
                        spdlog::info("Offscreen DLAA output retained: {}x{} finite nonuniform RGB; SHA256={}; no display write",
                            state->completedOutput->width,state->completedOutput->height,
                            state->completedOutput->sha256);
                    }
                }
            } catch(const std::exception& error) {
                state->probeFailed=true;
                try { spdlog::warn("Offscreen DLSS probe exception: {}; owned resources retained",error.what()); } catch(...) {}
            } catch(...) {
                state->probeFailed=true;
                try { spdlog::warn("Offscreen DLSS probe exception; owned resources retained"); } catch(...) {}
            }
        }
    }
#endif
#ifdef RK_WITH_NGX
    {
        const auto numbers=readWorldNumbers(world,state->expectedRenderer);
        if(numbers.valid&&numbers.lockOwner==GetCurrentThreadId()&&
           numbers.lockRecursion>0&&
           numbers.context==state->createdContext.load(std::memory_order_acquire)) {
            auto* immediate=reinterpret_cast<ID3D11DeviceContext*>(numbers.context);
            for(auto& pending:state->srFallbacks)if(pending) {
                const auto retired=pending->complete(immediate);
                if(const auto error=std::get_if<Error>(&retired)) {
                    state->srDisabled=true;
                    try { spdlog::warn("Reduced SR fallback retirement failed: {}; resources retained",
                        error->message); } catch(...) {}
                    break;
                }
                if(std::get<bool>(retired))pending.reset();
            }
        }
    }
    bool fallbackCapacity=false;
    for(const auto& pending:state->srFallbacks)fallbackCapacity|=!pending;
    const bool reducedRatios=drsRead&&!nativeRatios&&
        std::abs(drsRatios[0]-drsRatios[2])<=0.0001f&&
        std::abs(drsRatios[1]-drsRatios[3])<=0.0001f;
    if(state->srRequested&&reducedRatios&&!fallbackCapacity) {
        state->srPresenter.requestReset();
        ++state->srSkipped;
    }
    if(state->srRequested&&!state->srDisabled&&fallbackCapacity&&
       !drsProbeHasRun()&&
       reducedRatios&&state->srSourceVerified) {
        const auto numbers=readWorldNumbers(world,state->expectedRenderer);
        if(numbers.valid&&numbers.lockOwner==GetCurrentThreadId()&&numbers.lockRecursion>0&&
           numbers.device==state->createdDevice.load(std::memory_order_acquire)&&
           numbers.context==state->createdContext.load(std::memory_order_relaxed)&&
           numbers.swap==state->createdSwap.load(std::memory_order_relaxed)&&
           numbers.motion&&numbers.depth) {
            try {
                auto* device=reinterpret_cast<ID3D11Device*>(numbers.device);
                auto* immediate=reinterpret_cast<ID3D11DeviceContext*>(numbers.context);
                Microsoft::WRL::ComPtr<ID3D11Texture2D> backbuffer;
                if(FAILED(reinterpret_cast<IDXGISwapChain*>(numbers.swap)->GetBuffer(0,
                    IID_PPV_ARGS(&backbuffer)))||!backbuffer)
                    throw std::runtime_error("Reduced SR display backbuffer unavailable");
                D3D11_TEXTURE2D_DESC display{};backbuffer->GetDesc(&display);
                const auto target=engineDrsTarget(display.Width,display.Height,
                    drsRatios[0],drsRatios[1]);
                if(target&&target->width==state->srVerifiedWidth&&
                   target->height==state->srVerifiedHeight&&
                   target->width<display.Width&&target->height<display.Height) {
                    if(!state->nativePresenterStoppedForSr) {
                        const auto stopped=state->sdrPresenter.stop(immediate);
                        if(const auto error=std::get_if<Error>(&stopped))
                            throw std::runtime_error(error->message);
                        state->nativePresenterStoppedForSr=std::get<bool>(stopped);
                    }
                    if(state->nativePresenterStoppedForSr) {
                    if(state->srActiveGeneration&&
                       (state->srActiveGeneration!=state->srGeneration||
                        state->srWidth!=target->width||state->srHeight!=target->height)) {
                        const auto stopped=state->srPresenter.stop(immediate);
                        if(const auto error=std::get_if<Error>(&stopped))
                            throw std::runtime_error(error->message);
                        if(std::get<bool>(stopped))state->srActiveGeneration=0;
                    }
                    if(!state->srActiveGeneration) {
                        state->srActiveGeneration=state->srGeneration;
                        state->srWidth=target->width;state->srHeight=target->height;
                    }
                    if(state->srActiveGeneration==state->srGeneration) {
                        const auto jitter=readNgxJitter(state->jitterCamera,
                            display.Width,display.Height);
                        if(const auto error=std::get_if<Error>(&jitter))
                            throw std::runtime_error(error->message);
                        const auto displayJitter=std::get<NgxJitter>(jitter);
                        const NgxJitter renderJitter{
                            displayJitter.x*target->width/display.Width,
                            displayJitter.y*target->height/display.Height};
                        const std::array<ID3D11Texture2D*,3> sources{
                            backbuffer.Get(),
                            reinterpret_cast<ID3D11Texture2D*>(numbers.motion),
                            reinterpret_cast<ID3D11Texture2D*>(numbers.depth)};
                        auto prepared=prepareSdrSrInputsFromRegion(immediate,sources,
                            SrSourceRegion{0,0,target->width,target->height},
                            display.Width,display.Height);
                        if(const auto error=std::get_if<Error>(&prepared))
                            throw std::runtime_error(error->message);
                        auto frame=std::move(std::get<PreparedSrInputs>(prepared));
                        Microsoft::WRL::ComPtr<ID3D11Texture2D> preserved=frame.color();
                        const auto presented=presentSdrSrFrame(immediate,preserved.Get(),
                            backbuffer.Get(),[&]()->Result<bool> {
                                const auto evaluated=state->srPresenter.evaluatePrepared(device,
                                    immediate,std::move(frame),
                                    SrFrameMetadata{sequence,state->srGeneration,false,
                                        SrSourcePhase::PrePresent},
                                    renderJitter);
                                if(const auto error=std::get_if<Error>(&evaluated))return *error;
                                const auto token=std::get<std::optional<SrEvaluationToken>>(evaluated);
                                if(!token)return false;
                                return state->srPresenter.publishEvaluated(immediate,*token,
                                    backbuffer.Get());
                            });
                        if(const auto error=std::get_if<Error>(&presented))
                            throw std::runtime_error(error->message);
                        auto outcome=std::move(std::get<SdrSrFrameResult>(presented));
                        if(outcome.mode()==SdrSrFrameMode::Provider) {
                            state->displayedMode.store(DisplayMode::DlssSr,
                                std::memory_order_release);
                            const auto count=state->srPresenter.submittedFrames();
                            state->statusDlssFrames.store(
                                state->sdrPresenter.submittedFrames()+count,
                                std::memory_order_relaxed);
                            if(count==1||count%600==0)
                                spdlog::info("Reduced DLSS SR displayed: worldFrame={} generation={} source={}x{} display={}x{} submissions={} jitter=({},{}); UI follows",
                                    sequence,state->srGeneration,target->width,target->height,
                                    display.Width,display.Height,count,renderJitter.x,renderJitter.y);
                        } else {
                            state->srPresenter.requestReset();
                            ++state->srSkipped;
                            for(auto& pending:state->srFallbacks)if(!pending) {
                                pending.emplace(std::move(outcome));
                                break;
                            }
                            if(state->srSkipped==1||state->srSkipped%600==0)
                                spdlog::warn("Reduced DLSS SR unavailable; current-frame spatial fallback displayed: worldFrame={} skipped={}",
                                    sequence,state->srSkipped);
                        }
                    }
                    }
                }
            } catch(const std::exception& error) {
                state->srDisabled=true;
                try { spdlog::warn("Reduced DLSS SR disabled; native frame retained: {}",
                    error.what()); } catch(...) {}
            } catch(...) {
                state->srDisabled=true;
                try { spdlog::warn("Reduced DLSS SR disabled; native frame retained"); } catch(...) {}
            }
        }
    }
    bool srRetiredForNative=true;
    if(nativeRatios&&state->nativePresenterStoppedForSr) {
        srRetiredForNative=false;
        const auto numbers=readWorldNumbers(world,state->expectedRenderer);
        if(numbers.valid&&numbers.lockOwner==GetCurrentThreadId()&&
           numbers.lockRecursion>0&&
           numbers.context==state->createdContext.load(std::memory_order_acquire)) {
            const auto stopped=state->srPresenter.stop(
                reinterpret_cast<ID3D11DeviceContext*>(numbers.context));
            if(const auto error=std::get_if<Error>(&stopped)) {
                state->srDisabled=true;
                try { spdlog::warn("Reduced SR teardown failed; native frame retained: {}",
                    error->message); } catch(...) {}
            } else if(std::get<bool>(stopped)) {
                state->srActiveGeneration=0;
                state->nativePresenterStoppedForSr=false;
                srRetiredForNative=true;
            }
        }
    }
    if(drsProbeHasRun())
        state->displayedMode.store(DisplayMode::Native,std::memory_order_release);
    if(state->completedOutput&&!state->sdrDisabled&&!drsProbeHasRun()&&
       nativeRatios&&srRetiredForNative) {
        const auto numbers=readWorldNumbers(world,state->expectedRenderer);
        if(numbers.valid&&numbers.lockOwner==GetCurrentThreadId()&&numbers.lockRecursion>0&&
           numbers.device==state->createdDevice.load(std::memory_order_acquire)&&
           numbers.context==state->createdContext.load(std::memory_order_relaxed)&&
           numbers.swap==state->createdSwap.load(std::memory_order_relaxed)) {
            try {
                auto* immediate=reinterpret_cast<ID3D11DeviceContext*>(numbers.context);
                Microsoft::WRL::ComPtr<ID3D11Texture2D> backbuffer;
                const auto got=reinterpret_cast<IDXGISwapChain*>(numbers.swap)->GetBuffer(0,
                    IID_PPV_ARGS(&backbuffer));
                if(FAILED(got)||!backbuffer) {
                    state->sdrDisabled=true;
                    spdlog::warn("Continuous SDR DLAA disabled: backbuffer unavailable 0x{:08x}",
                        static_cast<std::uint32_t>(got));
                } else {
                    D3D11_TEXTURE2D_DESC backDesc{};
                    backbuffer->GetDesc(&backDesc);
                    state->statusWidth.store(backDesc.Width,std::memory_order_relaxed);
                    state->statusHeight.store(backDesc.Height,std::memory_order_relaxed);
                    const auto jitter=readNgxJitter(state->jitterCamera,
                        backDesc.Width,backDesc.Height);
                    if(const auto jitterError=std::get_if<Error>(&jitter)) {
                        state->sdrPresenter.requestReset();
                        ++state->sdrJitterSkipped;
                        if(state->sdrJitterSkipped==1||state->sdrJitterSkipped%600==0)
                            spdlog::warn("SDR DLAA same-frame jitter unavailable; native frame retained: {}; skipped={}",
                                jitterError->message,state->sdrJitterSkipped);
                    } else {
                        const auto offsets=std::get<NgxJitter>(jitter);
                        const auto displayed=state->sdrPresenter.render(
                            reinterpret_cast<ID3D11Device*>(numbers.device),immediate,
                            backbuffer.Get(),reinterpret_cast<ID3D11Texture2D*>(numbers.motion),
                            reinterpret_cast<ID3D11Texture2D*>(numbers.depth),offsets);
                        if(const auto error=std::get_if<Error>(&displayed)) {
                            state->sdrDisabled=true;
                            spdlog::warn("Continuous SDR DLAA disabled; native frame retained: {}",
                                error->message);
                        } else if(std::get<bool>(displayed)) {
                            state->displayedMode.store(DisplayMode::Dlaa,
                                std::memory_order_release);
                            const auto count=state->sdrPresenter.submittedFrames();
                            state->statusDlssFrames.store(count,std::memory_order_relaxed);
                            if(count==1||count%600==0)
                                spdlog::info("Continuous SDR DLAA submitted for display: source frame {}; submitted={}; skipped={}; jitter=({},{}); UI follows; experimental SDR placement",
                                    state->forwarded.load(std::memory_order_relaxed),count,
                                    state->sdrSkipped,offsets.x,offsets.y);
                            if(!state->firstSdrCaptured) {
                                state->firstSdrCaptured=true;
                                auto captured=StagePairCapture::capturePostWorld(immediate,
                                    reinterpret_cast<ID3D11Texture2D*>(numbers.colour),
                                    backbuffer.Get());
                                if(const auto captureError=std::get_if<Error>(&captured))
                                    spdlog::warn("SDR display submission capture unavailable: {}",captureError->message);
                                else {
                                    std::scoped_lock lock(state->stagePairMutex);
                                    state->stagePair.emplace(std::move(std::get<StagePairCapture>(captured)));
                                    state->stagePairFrame=state->forwarded.load(std::memory_order_relaxed);
                                    state->presentTargetProbeDue.store(true,std::memory_order_release);
                                    spdlog::info("SDR display submission captured before HUD at world frame {}; awaiting same-frame Present",
                                        state->stagePairFrame);
                                }
                            }
                        } else {
                            state->sdrPresenter.requestReset();
                            ++state->sdrSkipped;
                            if(state->sdrSkipped==1||state->sdrSkipped%600==0)
                                spdlog::warn("SDR DLAA slot busy; native frame displayed; skipped={}",state->sdrSkipped);
                        }
                    }
                }
            } catch(const std::exception& error) {
                state->sdrDisabled=true;
                try { spdlog::warn("Continuous SDR DLAA exception; native frame retained: {}",error.what()); } catch(...) {}
            } catch(...) {
                state->sdrDisabled=true;
                try { spdlog::warn("Continuous SDR DLAA exception; native frame retained"); } catch(...) {}
            }
        }
    }
    state->statusSkippedFrames.store(state->sdrSkipped+state->sdrJitterSkipped+
        state->srSkipped,
        std::memory_order_relaxed);
    state->statusDlssDisabled.store(state->sdrDisabled||state->srDisabled,
        std::memory_order_relaxed);
#endif
    if(!sample)return;
    const auto after=readWorldNumbers(world,state->expectedRenderer);
    if(drsProbeHasRun()&&after.valid&&after.lockOwner==GetCurrentThreadId()&&
       after.lockRecursion>0&&after.colour&&after.motion&&after.depth&&after.swap) {
        D3D11_TEXTURE2D_DESC colour{},motion{},depth{},display{};
        reinterpret_cast<ID3D11Texture2D*>(after.colour)->GetDesc(&colour);
        reinterpret_cast<ID3D11Texture2D*>(after.motion)->GetDesc(&motion);
        reinterpret_cast<ID3D11Texture2D*>(after.depth)->GetDesc(&depth);
        Microsoft::WRL::ComPtr<ID3D11Texture2D> backbuffer;
        if(SUCCEEDED(reinterpret_cast<IDXGISwapChain*>(after.swap)->GetBuffer(0,
            IID_PPV_ARGS(&backbuffer)))&&backbuffer)backbuffer->GetDesc(&display);
        try { spdlog::info("Experimental DRS extent map: kMAIN={}x{} format={}; motion={}x{} format={}; depth={}x{} format={}; display={}x{} format={}; displayedMode={}",
            colour.Width,colour.Height,static_cast<unsigned>(colour.Format),
            motion.Width,motion.Height,static_cast<unsigned>(motion.Format),
            depth.Width,depth.Height,static_cast<unsigned>(depth.Format),
            display.Width,display.Height,static_cast<unsigned>(display.Format),
            static_cast<unsigned>(state->displayedMode.load(std::memory_order_relaxed))); } catch(...) {}
    }
    try {
        spdlog::info("World stage #{}: thread={}; flags=0x{:x}; rendererMatch={}; beforeRead={}; afterRead={}; "
            "beforeLock={}/{}; afterLock={}/{}; device=0x{:x}; context=0x{:x}; swap=0x{:x}; "
            "colour=0x{:x}->0x{:x}; motion=0x{:x}->0x{:x}; depth=0x{:x}->0x{:x}; world callback",
            sequence,GetCurrentThreadId(),flags,
            reinterpret_cast<std::uintptr_t>(world)==state->expectedRenderer,before.valid,after.valid,
            before.lockOwner,before.lockRecursion,after.lockOwner,after.lockRecursion,
            after.device,after.context,after.swap,
            before.colour,after.colour,before.motion,after.motion,before.depth,after.depth);
    } catch (...) {}
}
bool read(std::uintptr_t address,void* destination,std::size_t size) {
    SIZE_T copied{};
    return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(address),destination,size,&copied)&&copied==size;
}
}
std::uint64_t worldDrawForwardedCalls() noexcept {
    const auto* state=active.load(std::memory_order_acquire);
    return state?state->forwarded.load(std::memory_order_relaxed):0;
}
std::optional<DiagnosticsSnapshot> worldDiagnosticsSnapshot(IDXGISwapChain* swap) noexcept {
    auto* state=active.load(std::memory_order_acquire);
    if(!state||!swap||state->createdSwap.load(std::memory_order_acquire)!=
       reinterpret_cast<std::uintptr_t>(swap))return std::nullopt;
    DiagnosticsSnapshot snapshot{};
    snapshot.mode=state->displayedMode.load(std::memory_order_acquire);
    snapshot.displayWidth=state->statusWidth.load(std::memory_order_relaxed);
    snapshot.displayHeight=state->statusHeight.load(std::memory_order_relaxed);
    DXGI_SWAP_CHAIN_DESC swapDesc{};
    if(SUCCEEDED(swap->GetDesc(&swapDesc))&&swapDesc.BufferDesc.Width&&
       swapDesc.BufferDesc.Height) {
        snapshot.displayWidth=swapDesc.BufferDesc.Width;
        snapshot.displayHeight=swapDesc.BufferDesc.Height;
    }
    std::array<float,4> drsRatios{};
    if(state->jitterCamera&&snapshot.displayWidth&&snapshot.displayHeight&&
       read(state->jitterCamera+0x104,drsRatios.data(),sizeof(drsRatios))) {
        if(const auto target=engineDrsTarget(snapshot.displayWidth,
            snapshot.displayHeight,drsRatios[0],drsRatios[1])) {
            snapshot.renderWidth=target->width;
            snapshot.renderHeight=target->height;
            snapshot.engineDrsKnown=true;
        }
    }
    snapshot.dlaaSuspendedByDrs=state->drsSuppressed.load(std::memory_order_acquire);
#ifdef RK_WITH_NGX
    snapshot.srRequested=state->srRequested;
    snapshot.srSourceReady=state->srSourceVerified.load(std::memory_order_acquire);
    try {
        snapshot.nrRuntime=state->nrStage.runtimeStatus();
        snapshot.nrStatusAvailable=true;
    } catch(...) {}
#endif
    snapshot.worldFrames=state->forwarded.load(std::memory_order_relaxed);
    snapshot.dlssFrames=state->statusDlssFrames.load(std::memory_order_relaxed);
    snapshot.skippedFrames=state->statusSkippedFrames.load(std::memory_order_relaxed);
    snapshot.dlssDisabled=state->statusDlssDisabled.load(std::memory_order_relaxed);
    snapshot.postSharpness=state->srSharpness.load(std::memory_order_relaxed);
    if(auto* domain=activeOwnedSceneDomain()) {
        snapshot.ownedSceneActive=true;
        snapshot.renderWidth=domain->plan().render.width;
        snapshot.renderHeight=domain->plan().render.height;
        snapshot.engineDrsKnown=true;
    }
    return snapshot;
}
void probePresentationTargets(IDXGISwapChain* swap) noexcept {
    auto* state=active.load(std::memory_order_acquire);
    if(!state||!swap||state->createdSwap.load(std::memory_order_acquire)!=
       reinterpret_cast<std::uintptr_t>(swap))return;
#ifdef RK_WITH_NGX
    if(auto* domain=activeOwnedSceneDomain();domain&&domain->phase()==ScenePhase::World) {
        const auto frame=state->forwarded.load(std::memory_order_relaxed);
        if(auto* ui=ownedUiRedirector()) {
            if(auto observation=ui->finishObservation(frame)) {
                try {
                    spdlog::info("Owned menu-to-Present bind trace frame {}: events={} dropped={} sampledDepthReads={} firstSampledDepthSlot={} otherSingletonReads={}",
                        frame,observation->count,observation->dropped,
                        observation->sampledDepthReads,
                        observation->firstSampledDepthSlot==~0u?-1:
                            static_cast<int>(observation->firstSampledDepthSlot),
                        observation->otherSingletonReads);
                    for(std::uint32_t i=0;i<observation->count;++i) {
                        const auto& event=observation->events[i];
                        if(event.kind==UiObservationKind::Viewport) {
                            spdlog::info("Owned bind trace frame {} event {}: viewport={}x{}",
                                frame,i,event.viewport.width,event.viewport.height);
                        } else {
                            spdlog::info("Owned bind trace frame {} event {}: targets={} sceneSlot={} depth={} depthExtent={}x{} depthId=0x{:x}; target0={}x{} id=0x{:x}; target1={}x{} id=0x{:x}; target2={}x{} id=0x{:x}; target3={}x{} id=0x{:x}",
                                frame,i,event.targetCount,event.sceneSlot,event.hasDepth,
                                event.depth.width,event.depth.height,event.depthIdentity,
                                event.targets[0].width,event.targets[0].height,
                                event.targetIdentities[0],
                                event.targets[1].width,event.targets[1].height,
                                event.targetIdentities[1],
                                event.targets[2].width,event.targets[2].height,
                                event.targetIdentities[2],
                                event.targets[3].width,event.targets[3].height,
                                event.targetIdentities[3]);
                        }
                    }
                } catch(...) {}
                const auto prepared=ui->prepareObservedCompanions();
                if(FAILED(prepared)) {
                    try {spdlog::warn("Owned native UI companion preparation frame {} failed: HRESULT=0x{:08x}",
                        frame,static_cast<std::uint32_t>(prepared));} catch(...) {}
                } else if(ui->companionsReady()&&!state->uiDepthViewContractLogged) {
                    state->uiDepthViewContractLogged=true;
                    const auto contract=ui->depthViewContract();
                    try {
                        if(contract)spdlog::info("Owned native UI companions ready at frame {}: reduced MRT/depth roles learned and display-sized counterparts allocated; depthFormat={}; sourceDsvFlags=0x{:x}; writableDsvFlags=0x{:x}; sampledClearDsvFlags=0x{:x}",
                            frame,static_cast<unsigned>(contract->sourceFormat),
                            contract->sourceFlags,contract->writableFlags,
                            contract->sampledClearFlags);
                        else spdlog::warn("Owned native UI depth-view contract unavailable at frame {} after companion preparation",frame);
                    }catch(...) {}
                }
            }
            auto trace=ui->finishFaultTrace(frame);
            if(trace&&state->inventoryTraceFrame==frame&&
               !state->menuBoundaryCaptureAttempted&&
               state->inventoryTraceWindowFrames==180)
                spdlog::warn("Sustained InventoryMenu trace window ended at frame {} without a reduced menu pass",
                    frame);
            if(trace&&(state->inventoryTraceFrame!=frame||
                       state->menuBoundaryCaptureAttempted)) {
                try {
                    spdlog::info("Owned UI {} bind trace frame {}: events={} dropped={}; observation only",
                        state->inventoryTraceFrame==frame?"sustained-inventory":"post-fault",
                        frame,trace->count,trace->dropped);
                    for(std::uint32_t i=0;i<trace->count;++i) {
                        const auto& event=trace->events[i];
                        if(event.kind==UiFaultTraceKind::Viewport) {
                            spdlog::info("Owned UI bind trace {} event {}: viewport count={} extent={}x{}",
                                frame,i,event.count,event.viewport.width,event.viewport.height);
                        } else if(event.kind==UiFaultTraceKind::SampledTarget) {
                            const auto& target=event.targets[0];
                            spdlog::info("Owned UI bind trace {} event {}: PS slot={} callCount={} id=0x{:x} format={} extent={}x{}",
                                frame,i,event.slot,event.count,target.id,
                                static_cast<unsigned>(target.format),
                                target.extent.width,target.extent.height);
                        } else {
                            const auto& a=event.targets[0];
                            const auto& b=event.targets[1];
                            const auto& c=event.targets[2];
                            const auto& d=event.depth;
                            spdlog::info("Owned UI bind trace {} event {}: OM count={} target0=0x{:x}/{} {}x{} target1=0x{:x}/{} {}x{} target2=0x{:x}/{} {}x{} depth=0x{:x}/{} {}x{}",
                                frame,i,event.count,a.id,static_cast<unsigned>(a.format),
                                a.extent.width,a.extent.height,b.id,
                                static_cast<unsigned>(b.format),b.extent.width,b.extent.height,
                                c.id,static_cast<unsigned>(c.format),c.extent.width,c.extent.height,
                                d.id,static_cast<unsigned>(d.format),d.extent.width,d.extent.height);
                        }
                    }
                } catch(...) {}
            }
        }
        processOwnedWorldFrame(state,reinterpret_cast<void*>(state->expectedRenderer),
            frame,OwnedPublicationBoundary::PrePresent);
    } else if(auto* closingDomain=activeOwnedSceneDomain();
              closingDomain&&closingDomain->phase()==ScenePhase::NativeUi) {
        const auto frame=state->forwarded.load(std::memory_order_relaxed);
        auto* ui=ownedUiRedirector();
        if(ui&&state->inventoryTraceFrame==frame) {
            auto trace=ui->finishFaultTrace(frame);
            if(trace&&!state->menuBoundaryCaptureAttempted&&
               state->inventoryTraceWindowFrames==180)
                spdlog::warn("Sustained InventoryMenu trace window ended at frame {} without a reduced menu pass",
                    frame);
            if(trace&&state->menuBoundaryCaptureAttempted) {
                try {
                    spdlog::info("Owned sustained-inventory bind trace frame {}: events={} dropped={}; observation only",
                        frame,trace->count,trace->dropped);
                    for(std::uint32_t i=0;i<trace->count;++i) {
                        const auto& event=trace->events[i];
                        if(event.kind==UiFaultTraceKind::Viewport) {
                            spdlog::info("Inventory bind trace {} event {}: viewport count={} extent={}x{}",
                                frame,i,event.count,event.viewport.width,event.viewport.height);
                        } else if(event.kind==UiFaultTraceKind::SampledTarget) {
                            const auto& target=event.targets[0];
                            spdlog::info("Inventory bind trace {} event {}: PS slot={} callCount={} id=0x{:x} format={} extent={}x{}",
                                frame,i,event.slot,event.count,target.id,
                                static_cast<unsigned>(target.format),
                                target.extent.width,target.extent.height);
                        } else {
                            const auto& a=event.targets[0];
                            const auto& b=event.targets[1];
                            const auto& c=event.targets[2];
                            const auto& d=event.depth;
                            spdlog::info("Inventory bind trace {} event {}: OM count={} target0=0x{:x}/{} {}x{} target1=0x{:x}/{} {}x{} target2=0x{:x}/{} {}x{} depth=0x{:x}/{} {}x{}",
                                frame,i,event.count,a.id,static_cast<unsigned>(a.format),
                                a.extent.width,a.extent.height,b.id,
                                static_cast<unsigned>(b.format),b.extent.width,b.extent.height,
                                c.id,static_cast<unsigned>(c.format),c.extent.width,c.extent.height,
                                d.id,static_cast<unsigned>(d.format),d.extent.width,d.extent.height);
                        }
                    }
                }catch(...) {}
            }
        }
        if(!ui) {
            state->srDisabled=true;
            state->statusDlssDisabled.store(true,std::memory_order_release);
            closingDomain->suspend();
            try {spdlog::warn("Owned menu-boundary UI route suspended without its context hook at pre-Present frame {}",
                frame);}catch(...) {}
        } else if(ui->compatibilityFault()) {
            const auto fault=ui->compatibilityFaultInfo();
            try {spdlog::info("Owned UI fault frame {} source target PS reads={} firstSlot={}; observation only",
                frame,fault.unknownTargetSrvReads,
                fault.unknownTargetFirstSrvSlot==~0u?-1:
                    static_cast<int>(fault.unknownTargetFirstSrvSlot));}catch(...) {}
            ui->suspendLatePassRouting(frame);
            state->menuSceneGate.record(frame,std::nullopt,std::nullopt);
            if(!closingDomain->closePublishedFrame(frame,
                closingDomain->plan().generation)) {
                state->srDisabled=true;
                state->statusDlssDisabled.store(true,std::memory_order_release);
                closingDomain->suspend();
            }
            try {spdlog::warn("Owned native UI contract changed at frame {}; using pre-Present publication while a bounded late-route retry waits for scene admission",
                frame);}catch(...) {}
        } else if(!closingDomain->closePublishedFrame(frame,
                      closingDomain->plan().generation)) {
            state->srDisabled=true;
            state->statusDlssDisabled.store(true,std::memory_order_release);
            closingDomain->suspend();
            try {spdlog::warn("Owned menu-boundary UI frame {} could not close",
                frame);}catch(...) {}
        } else if(frame<=3||frame%600==0) {
            try {spdlog::info("Owned menu-boundary UI route closed frame {} at pre-Present",
                frame);}catch(...) {}
        }
    }
#endif
    const bool usualProbe=state->presentTargetProbeDue.exchange(false,std::memory_order_acq_rel);
    const auto ownedRemaining=state->ownedPrePresentProbes.load(std::memory_order_acquire);
    const bool ownedProbe=ownedRemaining!=0;
    bool ownedSrStageProbe=false;
    bool menuUiSequenceProbe=false;
    bool menuBoundaryProbe=false;
#ifdef RK_WITH_NGX
    {
        std::scoped_lock lock(state->ownedSrStageMutex);
        ownedSrStageProbe=state->ownedSrStages.has_value();
    }
    {
        std::scoped_lock lock(state->menuUiSequenceMutex);
        menuUiSequenceProbe=state->menuUiSequence.has_value();
    }
    menuBoundaryProbe=state->menuBoundaryCapture.has_value();
#endif
    if(!usualProbe&&!ownedProbe&&!ownedSrStageProbe&&
       !menuUiSequenceProbe&&!menuBoundaryProbe)return;
    if(ownedProbe)state->ownedPrePresentProbes.store(ownedRemaining-1,std::memory_order_release);
    try {
        const auto context=state->createdContext.load(std::memory_order_relaxed);
        if(!context)return;
#ifdef RK_WITH_NGX
        if(menuBoundaryProbe) {
            auto capture=std::move(*state->menuBoundaryCapture);
            state->menuBoundaryCapture.reset();
            const auto frame=state->forwarded.load(std::memory_order_relaxed);
            auto* domain=activeOwnedSceneDomain();
            auto* scene=activeOwnedSceneTexture();
            const auto device=state->createdDevice.load(std::memory_order_relaxed);
            if(capture.frame!=frame||!domain||!scene||!device)
                spdlog::warn("{} boundary capture frame {} could not match pre-Present frame {}",
                    capture.magic?"Magic":"Inventory",capture.frame,frame);
            else {
                auto native=acquireNativeFlipTarget(swap,
                    reinterpret_cast<ID3D11Device*>(device),domain->plan().display);
                if(auto* target=std::get_if<NativeFlipTarget>(&native)) {
                    const std::array<ID3D11Texture2D*,2> sources{
                        scene,target->texture.Get()};
                    auto final=readbackCandidates(
                        reinterpret_cast<ID3D11DeviceContext*>(context),
                        sources,24*1024*1024);
                    if(auto* images=std::get_if<std::vector<ProbeImage>>(&final)) {
                        for(auto& image:*images)
                            capture.images.emplace_back(std::move(image));
                        PWSTR documents=nullptr;
                        const auto found=SHGetKnownFolderPath(FOLDERID_Documents,
                            KF_FLAG_DEFAULT,nullptr,&documents);
                        struct FreeDocuments {
                            PWSTR value;~FreeDocuments(){CoTaskMemFree(value);}
                        } free{documents};
                        if(SUCCEEDED(found)&&documents) {
                            const auto directory=std::filesystem::path(documents)/
                                "My Games"/"Skyrim Special Edition"/"SKSE"/
                                "RazKolbasCaptures"/
                                (std::string(capture.magic?"magic-boundary-":"inventory-boundary-")+
                                std::to_string(GetCurrentProcessId())+"-"+
                                std::to_string(frame)+"-"+
                                std::to_string(GetTickCount64()));
                            std::vector<std::string_view> names{
                                "reduced-before-copy.raw","native-before-copy.raw",
                                "native-after-copy.raw"};
                            if(capture.afterEndFrameCaptured)
                                names.emplace_back("native-after-endframe.raw");
                            names.emplace_back("reduced-pre-present.raw");
                            names.emplace_back("native-pre-present.raw");
                            const auto saved=saveProbeBundle(directory,
                                capture.images,names,
                                capture.magic?
                                    "One sustained MagicMenu frame around deferred copy, EndFrame and Present; diagnostic only":
                                    "One sustained InventoryMenu frame around deferred copy, EndFrame and Present; diagnostic only");
                            if(const auto error=std::get_if<Error>(&saved))
                                spdlog::warn("{} boundary capture save failed: {}",
                                    capture.magic?"Magic":"Inventory",error->message);
                            else
                                spdlog::info("{} boundary frame {} capture complete at {}",
                                    capture.magic?"Magic":"Inventory",frame,directory.string());
                        } else
                            spdlog::warn("{} boundary capture Documents directory unavailable",
                                capture.magic?"Magic":"Inventory");
                    } else
                        spdlog::warn("{} boundary pre-Present readback unavailable: {}",
                            capture.magic?"Magic":"Inventory",std::get<Error>(final).message);
                } else
                    spdlog::warn("{} boundary pre-Present target unavailable: {}",
                        capture.magic?"Magic":"Inventory",
                        std::get<Error>(native).message);
            }
        }
#endif
        if(ownedProbe) {
#ifdef RK_WITH_NGX
            const auto numbers=readWorldNumbers(reinterpret_cast<void*>(state->expectedRenderer),
                state->expectedRenderer);
            if(!numbers.valid||numbers.lockOwner!=GetCurrentThreadId())
                spdlog::warn("Owned pre-Present pixel probe skipped: renderer lock is not held");
            else if(auto* scene=activeOwnedSceneTexture()) {
                auto* domain=activeOwnedSceneDomain();
                const auto native=domain?acquireNativeFlipTarget(swap,
                    reinterpret_cast<ID3D11Device*>(numbers.device),domain->plan().display):
                    Result<NativeFlipTarget>{Error{ErrorCode::Unavailable,"Owned route absent"}};
                if(const auto error=std::get_if<Error>(&native))
                    spdlog::warn("Owned pre-Present pixel probe unavailable: {}",error->message);
                else probeOwnedPixels(ownedRemaining==2?"pre-Present frame 1":"pre-Present frame 2",
                    reinterpret_cast<ID3D11DeviceContext*>(context),scene,
                    std::get<NativeFlipTarget>(native).texture.Get());
            }
#endif
        }
#ifdef RK_WITH_NGX
        if(ownedSrStageProbe) {
            std::scoped_lock lock(state->ownedSrStageMutex);
            if(state->ownedSrStages) {
                const auto frame=state->forwarded.load(std::memory_order_relaxed);
                if(state->ownedSrStages->frame!=frame)
                    spdlog::warn("Owned SR three-stage capture discarded: another world frame arrived before Present");
                else if(auto* domain=activeOwnedSceneDomain()) {
                    const auto device=state->createdDevice.load(std::memory_order_relaxed);
                    const auto native=device?acquireNativeFlipTarget(swap,
                        reinterpret_cast<ID3D11Device*>(device),domain->plan().display):
                        Result<NativeFlipTarget>{Error{ErrorCode::Unavailable,
                            "Owned capture device is absent"}};
                    if(const auto nativeError=std::get_if<Error>(&native))
                        spdlog::warn("Owned SR final composition capture unavailable: {}",
                            nativeError->message);
                    else {
                        const std::array<ID3D11Texture2D*,1> finalTexture{
                            std::get<NativeFlipTarget>(native).texture.Get()};
                        auto finalImage=readbackCandidates(
                            reinterpret_cast<ID3D11DeviceContext*>(context),finalTexture,
                            16*1024*1024);
                        if(const auto readbackError=std::get_if<Error>(&finalImage))
                            spdlog::warn("Owned SR final composition readback unavailable: {}",
                                readbackError->message);
                        else {
                            auto& images=state->ownedSrStages->images;
                            images.emplace_back(std::move(
                                std::get<std::vector<ProbeImage>>(finalImage).front()));
                            PWSTR documents=nullptr;
                            const auto found=SHGetKnownFolderPath(FOLDERID_Documents,
                                KF_FLAG_DEFAULT,nullptr,&documents);
                            struct FreeDocuments {
                                PWSTR value;~FreeDocuments(){CoTaskMemFree(value);}
                            } free{documents};
                            if(FAILED(found)||!documents)
                                spdlog::warn("Owned SR three-stage capture Documents directory unavailable");
                            else {
                                const auto directory=std::filesystem::path(documents)/
                                    "My Games"/"Skyrim Special Edition"/"SKSE"/
                                    "RazKolbasCaptures"/
                                    ("owned-sr-stages-"+
                                    std::to_string(GetCurrentProcessId())+"-"+
                                    std::to_string(frame)+"-"+
                                    std::to_string(GetTickCount64()));
                                const std::array<std::string_view,3> names{
                                    "prepared-color-input.raw",
                                    "dlss-output-pre-sharpen.raw",
                                    "final-composition.raw"};
                                const auto saved=saveProbeBundle(directory,images,names,
                                    "RazKolbas same-frame owned SR stages: exact prepared colour input, raw DLSS output before sharpening/UI, final pre-Present composition");
                                if(const auto saveError=std::get_if<Error>(&saved))
                                    spdlog::warn("Owned SR three-stage capture save unavailable: {}",
                                        saveError->message);
                                else
                                    spdlog::info("Owned SR three-stage capture complete for frame {} at {}",
                                        frame,directory.string());
                            }
                        }
                    }
                }
                state->ownedSrStages.reset();
            }
        }
        if(menuUiSequenceProbe)completeMenuUiSequence(state,swap);
#endif
        logTargetBoundary("pre-ENB-Present",reinterpret_cast<ID3D11DeviceContext*>(context),swap,
            state->worldColourIdentity.load(std::memory_order_relaxed));
        std::scoped_lock lock(state->stagePairMutex);
        if(state->stagePair) {
            if(state->stagePairFrame!=state->forwarded.load(std::memory_order_relaxed))
                spdlog::warn("Stage pair discarded: another world frame arrived before Present");
            else {
                Microsoft::WRL::ComPtr<ID3D11Texture2D> backbuffer;
                const auto got=swap->GetBuffer(0,IID_PPV_ARGS(&backbuffer));
                if(FAILED(got)||!backbuffer)
                    spdlog::warn("Stage pair pre-Present backbuffer unavailable: HRESULT=0x{:08x}",
                        static_cast<std::uint32_t>(got));
                else {
                    const auto captured=state->stagePair->captureBeforePresent(
                        reinterpret_cast<ID3D11DeviceContext*>(context),backbuffer.Get());
                    if(const auto error=std::get_if<Error>(&captured))
                        spdlog::warn("Stage pair pre-Present capture unavailable: {}",error->message);
                    else {
                        PWSTR documents=nullptr;
                        const auto found=SHGetKnownFolderPath(FOLDERID_Documents,
                            KF_FLAG_DEFAULT,nullptr,&documents);
                        struct FreeDocuments { PWSTR value;~FreeDocuments(){CoTaskMemFree(value);} } free{documents};
                        if(FAILED(found)||!documents)
                            spdlog::warn("Stage pair Documents directory unavailable");
                        else {
                            const auto directory=std::filesystem::path(documents)/"My Games"/
                                "Skyrim Special Edition"/"SKSE"/"RazKolbasCaptures"/
                                ("stage-pair-"+std::to_string(GetCurrentProcessId())+"-"+
                                std::to_string(state->stagePairFrame)+"-"+
                                std::to_string(GetTickCount64()));
                            const auto saved=state->stagePair->save(directory);
                            if(const auto saveError=std::get_if<Error>(&saved))
                                spdlog::warn("Stage pair save unavailable: {}",saveError->message);
                            else
                                spdlog::info("Stage pair complete: world frame {}; HDR, post-world backbuffer and pre-ENB-Present backbuffer saved to {}",
                                    state->stagePairFrame,directory.string());
                        }
                    }
                }
            }
            state->stagePair.reset();
        }
    } catch(const std::exception& error) {
        { std::scoped_lock lock(state->stagePairMutex);state->stagePair.reset(); }
        try { spdlog::warn("Pre-Present target map unavailable: {}",error.what()); } catch(...) {}
    } catch(...) {
        { std::scoped_lock lock(state->stagePairMutex);state->stagePair.reset(); }
        try { spdlog::warn("Pre-Present target map unavailable"); } catch(...) {}
    }
}
void bindWorldDrawRenderer(ID3D11Device* device,ID3D11DeviceContext* context,
    IDXGISwapChain* swap) noexcept {
    auto* state=active.load(std::memory_order_acquire);
    if(!state||!device||!context||!swap||state->creationBound.test_and_set(std::memory_order_acq_rel))return;
    state->createdContext.store(reinterpret_cast<std::uintptr_t>(context),std::memory_order_relaxed);
    state->createdSwap.store(reinterpret_cast<std::uintptr_t>(swap),std::memory_order_relaxed);
    state->createdDevice.store(reinterpret_cast<std::uintptr_t>(device),std::memory_order_release);
}
Result<Extent> planWorldOwnedScene(Extent display) noexcept {
#ifdef RK_WITH_NGX
    auto* state=active.load(std::memory_order_acquire);
    if(!state||!state->srRequested)
        return Error{ErrorCode::Unsupported,"Owned world SR is not requested"};
    const auto render=planEarlyOwnedScene(display,state->earlyQuality,
        state->manualRenderScale);
    if(!render.valid())
        return Error{ErrorCode::InvalidInput,"Early owned scene extent is invalid"};
    return render;
#else
    (void)display;
    return Error{ErrorCode::Unsupported,"NVIDIA SDR SR runtime is not built"};
#endif
}
Result<Extent> prepareWorldOwnedSrPlan(ID3D11Device* device,
    ID3D11DeviceContext* context,Extent display) {
#ifdef RK_WITH_NGX
    auto* state=active.load(std::memory_order_acquire);
    if(!state||!state->srRequested||!device||!context||!display.valid()||
       state->createdDevice.load(std::memory_order_acquire)!=
           reinterpret_cast<std::uintptr_t>(device)||
       state->createdContext.load(std::memory_order_acquire)!=
           reinterpret_cast<std::uintptr_t>(context))
        return Error{ErrorCode::Unsupported,"Owned world SR is not requested on this renderer"};
    return state->srPresenter.prepareReducedPlan(device,context,display);
#else
    (void)device;(void)context;(void)display;
    return Error{ErrorCode::Unsupported,"NVIDIA SDR SR runtime is not built"};
#endif
}
Result<float> worldOwnedMipBias(Extent render,Extent display) noexcept {
#ifdef RK_WITH_NGX
    auto* state=active.load(std::memory_order_acquire);
    if(!state||!state->srRequested)
        return Error{ErrorCode::Unsupported,"Owned world SR mip policy is unavailable"};
    return resolveMipLodBias(render,display,state->automaticMipBias,
        state->manualMipBias);
#else
    (void)render;(void)display;
    return Error{ErrorCode::Unsupported,"NVIDIA SDR SR runtime is not built"};
#endif
}
void useWorldOwnedSpatialFallback() noexcept {
#ifdef RK_WITH_NGX
    if(auto* state=active.load(std::memory_order_acquire)) {
        state->ownedNgxInitFailed=true;
        state->statusDlssDisabled.store(true,std::memory_order_release);
    }
#endif
}
Result<bool> abandonWorldOwnedSrPlan(ID3D11DeviceContext* context) {
#ifdef RK_WITH_NGX
    auto* state=active.load(std::memory_order_acquire);
    if(!state||!context||state->createdContext.load(std::memory_order_acquire)!=
        reinterpret_cast<std::uintptr_t>(context))
        return Error{ErrorCode::InvalidInput,"Owned world SR context differs"};
    return state->srPresenter.stop(context);
#else
    (void)context;
    return false;
#endif
}
Result<bool> installWorldDrawPassThrough(HMODULE game,std::string_view verifiedGameHash,
    const Settings& settings) {
    if(!settings.get<bool>("General.Enabled")||settings.get<bool>("General.SafeMode")||
       !settings.get<bool>("Patching.EnableVersionedPatches")||
       !settings.get<bool>("Patching.ExperimentalPatches")||
       patchDisabled(settings.get<Text>("Patching.DisabledPatchIds").value,worldDrawPatchId)||
       patchDisabled(settings.get<Text>("Patching.DisabledPatchIds").value,menuDisplayPatchId)||
       patchDisabled(settings.get<Text>("Patching.DisabledPatchIds").value,deferredUiFlushPatchId)) {
        spdlog::info("World-draw pass-through disabled by configuration");
        return false;
    }
    if(active.load(std::memory_order_acquire))return true;
    const auto& profile=skyrim1170CreationProfile();
    if(!game||verifiedGameHash!=profile.gameSha256)
        return Error{ErrorCode::Unsupported,"World-draw executable identity differs"};
    constexpr std::array<std::uint8_t,5> expected{0xe8,0xd1,0xf7,0xe9,0xff};
    const CallSiteDescriptor descriptor{std::string(worldDrawPatchId),std::string(profile.gameSha256),
        profile.imageSize,0xfa507a,0xe44850,expected};
    const auto base=reinterpret_cast<std::uintptr_t>(game);
    std::array<std::uint8_t,17> caller{};
    std::array<std::uint8_t,33> target{};
    if(!read(base+0xfa5071,caller.data(),caller.size())||
       !read(base+descriptor.originalTargetRva,target.data(),target.size()))
        return Error{ErrorCode::Io,"Cannot read decoded world-call ABI"};
    if(const auto abi=verifySkyrim1170WorldCallAbi(caller,target);
       const auto error=std::get_if<Error>(&abi))return *error;
    const auto planned=prepareCallSite(std::span(caller).subspan(9,5),verifiedGameHash,
        profile.imageSize,descriptor);
    if(const auto error=std::get_if<Error>(&planned))return *error;
    const auto& plan=std::get<CallSitePlan>(planned);
    constexpr std::array<std::uint8_t,5> menuExpected{0xe8,0xf0,0xef,0xe9,0xff};
    const CallSiteDescriptor menuDescriptor{std::string(menuDisplayPatchId),
        std::string(profile.gameSha256),profile.imageSize,0xfa51cb,0xe441c0,
        menuExpected};
    std::array<std::uint8_t,38> menuCaller{};
    std::array<std::uint8_t,66> menuTarget{};
    if(!read(base+0xfa51b4,menuCaller.data(),menuCaller.size())||
       !read(base+menuDescriptor.originalTargetRva,menuTarget.data(),menuTarget.size()))
        return Error{ErrorCode::Io,"Cannot read decoded menu-display ABI"};
    if(const auto abi=verifySkyrim1170MenuDisplayCallAbi(menuCaller,menuTarget);
       const auto error=std::get_if<Error>(&abi))return *error;
    const auto menuPlanned=prepareCallSite(std::span(menuCaller).subspan(23,5),
        verifiedGameHash,profile.imageSize,menuDescriptor);
    if(const auto error=std::get_if<Error>(&menuPlanned))return *error;
    const auto& menuPlan=std::get<CallSitePlan>(menuPlanned);
    constexpr std::array<std::uint8_t,5> flushExpected{0xe8,0x11,0xe1,0x01,0x00};
    const CallSiteDescriptor flushDescriptor{std::string(deferredUiFlushPatchId),
        std::string(profile.gameSha256),profile.imageSize,0xfa51ea,0xfc3300,
        flushExpected};
    std::array<std::uint8_t,16> flushCaller{};
    std::array<std::uint8_t,33> flushTarget{};
    if(!read(base+0xfa51df,flushCaller.data(),flushCaller.size())||
       !read(base+flushDescriptor.originalTargetRva,flushTarget.data(),flushTarget.size()))
        return Error{ErrorCode::Io,"Cannot read decoded deferred UI flush ABI"};
    if(const auto abi=verifySkyrim1170DeferredUiFlushCallAbi(flushCaller,flushTarget);
       const auto error=std::get_if<Error>(&abi))return *error;
    const auto flushPlanned=prepareCallSite(std::span(flushCaller).subspan(11,5),
        verifiedGameHash,profile.imageSize,flushDescriptor);
    if(const auto error=std::get_if<Error>(&flushPlanned))return *error;
    const auto& flushPlan=std::get<CallSitePlan>(flushPlanned);
    auto pending=std::make_unique<WorldState>();
    // Address Library AE 1.6.1170 ID 400327. The executable hash gate above
    // makes this UI singleton pointer-cell RVA version-specific.
    pending->uiSingletonCell=base+0x20f6a00;
#ifdef RK_WITH_NGX
    const auto provider=settings.get<Choice>("Upscaling.Provider").value;
    const auto quality=parseUpscaleQuality(
        settings.get<Choice>("Upscaling.Quality").value);
    if(!quality)return Error{ErrorCode::InvalidInput,"Unrecognized SR quality setting"};
    if(const auto configured=pending->srPresenter.configureQuality(*quality);
       const auto error=std::get_if<Error>(&configured))return *error;
    if(const auto configured=pending->srPresenter.configureModelPreset(
        settings.get<Choice>("Upscaling.ModelPreset").value);
       const auto error=std::get_if<Error>(&configured))return *error;
    if(const auto configured=pending->sdrPresenter.configureModelPreset(
        settings.get<Choice>("Upscaling.ModelPreset").value);
       const auto error=std::get_if<Error>(&configured))return *error;
    pending->srSharpness.store(settings.get<bool>("Upscaling.Sharpening")?
        static_cast<float>(settings.get<double>("Upscaling.Sharpness")):0.0f,
        std::memory_order_relaxed);
    if(const auto configured=pending->srPresenter.configureSharpness(
        settings.get<bool>("Upscaling.Sharpening"),
        pending->srSharpness.load(std::memory_order_relaxed));
       const auto error=std::get_if<Error>(&configured))return *error;
    if(const auto configured=pending->sdrPresenter.configureSharpness(
        settings.get<bool>("Upscaling.Sharpening"),
        pending->srSharpness.load(std::memory_order_relaxed));
       const auto error=std::get_if<Error>(&configured))return *error;
    const auto nrConfigured=pending->nrStage.configure(settings);
    if(const auto error=std::get_if<Error>(&nrConfigured))
        spdlog::warn("Neural Rendering request retained but inactive: {}",error->message);
    else {
        auto* stage=&pending->nrStage;
        auto* failureLogged=&pending->nrFailureLogged;
        const auto processor=[stage,failureLogged](ID3D11Device* device,
            ID3D11DeviceContext* context,PreparedSrInputs& frame,
            const SrFrameMetadata& metadata,NgxJitter,bool reset) {
            const auto processed=stage->process(device,context,frame,reset);
            if(const auto processError=std::get_if<Error>(&processed)) {
                if(!stage->enabled()) {
                    spdlog::warn("Neural Rendering disabled after frame {}: {}; original colour continues to DLSS/DLAA",
                        metadata.frameId,processError->message);
                } else if(!*failureLogged) {
                    *failureLogged=true;
                    spdlog::warn("Neural Rendering skipped frame {}: {}; original colour continues to DLSS/DLAA",
                        metadata.frameId,processError->message);
                }
                return false;
            }
            if(std::get<bool>(processed)) {
                *failureLogged=false;
                const auto count=stage->submittedFrames();
                if(count==1) {
                    const auto runtime=stage->runtimeStatus();
                    spdlog::info("Neural Rendering runtime: requested={} effective={} phase={} vendor=0x{:04x} device=0x{:04x} LUID={:08x}:{:08x} path={} sha256={} contract=direct-nr-310.8-v1",
                        runtime.requestedProfile,runtime.effectiveProfile,
                        static_cast<int>(runtime.phase),runtime.vendorId,
                        runtime.deviceId,static_cast<std::uint32_t>(runtime.luidHigh),
                        runtime.luidLow,runtime.path,runtime.sha256);
                }
                if(count<=3||count%600==0)
                    spdlog::info("Neural Rendering pre-SR submission {} completed for frame {}",
                        count,metadata.frameId);
                return true;
            }
            return false;
        };
        if(const auto configured=pending->srPresenter.configurePreSrProcessor(
            processor,{.requireR32Depth=true});
           const auto configureError=std::get_if<Error>(&configured))return *configureError;
        if(const auto configured=pending->sdrPresenter.configurePreSrProcessor(
            processor,{.requireR32Depth=true});
           const auto configureError=std::get_if<Error>(&configured))return *configureError;
        spdlog::info("Neural Rendering preprocessor armed before DLSS SR and DLAA: enabled={}; requested runtime={}; experimental={}; exact catalog identity required",
            pending->nrStage.enabled(),
            settings.get<Choice>("NeuralRendering.RuntimeProfile").value,
            settings.get<bool>("NeuralRendering.AllowExperimentalRuntime"));
    }
    pending->srRequested=(provider=="Auto"||provider=="DLSS")&&
        *quality!=UpscaleQuality::NativeAA;
    pending->earlyQuality=*quality;
    pending->manualRenderScale=settings.get<double>("Upscaling.ManualRenderScale");
    pending->automaticMipBias=
        settings.get<Choice>("Upscaling.MipBiasMode").value=="Auto";
    pending->manualMipBias=settings.get<double>("Upscaling.ManualMipBias");
    pending->ownedSpatialBaseline=
        settings.get<bool>("Diagnostics.SpatialBaselineOnly");
    pending->captureFirstDlssFrame=
        settings.get<bool>("Diagnostics.CaptureFirstDlssFrame");
    if(pending->ownedSpatialBaseline)
        spdlog::info("Owned reduced route configured for spatial baseline; NGX submissions disabled");
#endif
    pending->expectedRenderer=base+renderer1170Rva;
    constexpr std::array<std::uint8_t,7> cameraLoad{0x48,0x8d,0x0d,0xb5,0x85,0x44,0x02};
    constexpr std::array<std::uint8_t,5> jitterCall{0xe8,0x99,0x43,0x01,0x00};
    constexpr std::array<std::uint8_t,17> jitterEntry{
        0x48,0x8b,0x05,0x89,0x1c,0x4d,0x02,0x0f,0x57,0xc0,
        0x48,0x8b,0x90,0xf0,0x01,0x00,0x00};
    std::array<std::uint8_t,7> liveCameraLoad{};
    std::array<std::uint8_t,5> liveJitterCall{};
    std::array<std::uint8_t,17> liveJitterEntry{};
    if(read(base+0xe44664,liveCameraLoad.data(),liveCameraLoad.size())&&
       read(base+0xe44672,liveJitterCall.data(),liveJitterCall.size())&&
       read(base+0xe58a10,liveJitterEntry.data(),liveJitterEntry.size())&&
       liveCameraLoad==cameraLoad&&liveJitterCall==jitterCall&&
       liveJitterEntry==jitterEntry) {
        pending->jitterCamera=base+0x328cc20;
        spdlog::info("Skyrim camera jitter source armed: game RVA=0x328cc20; exact caller/CALL/target bytes verified; same-frame SR/DLAA jitter enabled");
    } else spdlog::warn("Skyrim camera jitter source unavailable: caller/CALL/target bytes differ; DLAA will retain native frames");
    const auto configured=pending->forwarder.configure(
        reinterpret_cast<WorldDrawFn>(base+plan.originalTargetRva),&afterOriginal);
    if(const auto error=std::get_if<Error>(&configured))return *error;
    const auto menuConfigured=pending->menuForwarder.configure(
        reinterpret_cast<MenuDisplayFn>(base+menuPlan.originalTargetRva),
        &beforeMenuDisplay);
    if(const auto error=std::get_if<Error>(&menuConfigured))return *error;
    const auto flushConfigured=pending->deferredUiFlushForwarder.configure(
        reinterpret_cast<DeferredUiFlushFn>(base+flushPlan.originalTargetRva),
        &beforeDeferredUiFlush);
    if(const auto error=std::get_if<Error>(&flushConfigured))return *error;
    auto relayResult=prepareNearCallRelay(plan,base,reinterpret_cast<std::uintptr_t>(&worldDrawProxy));
    if(const auto error=std::get_if<Error>(&relayResult))return *error;
    auto relay=std::make_unique<NearCallRelay>(std::move(std::get<NearCallRelay>(relayResult)));
    auto menuRelayResult=prepareNearCallRelay(menuPlan,base,
        reinterpret_cast<std::uintptr_t>(&menuDisplayProxy));
    if(const auto error=std::get_if<Error>(&menuRelayResult))return *error;
    auto menuRelay=std::make_unique<NearCallRelay>(
        std::move(std::get<NearCallRelay>(menuRelayResult)));
    auto flushRelayResult=prepareNearCallRelay(flushPlan,base,
        reinterpret_cast<std::uintptr_t>(&deferredUiFlushProxy));
    if(const auto error=std::get_if<Error>(&flushRelayResult))return *error;
    auto flushRelay=std::make_unique<NearCallRelay>(
        std::move(std::get<NearCallRelay>(flushRelayResult)));
    HMODULE pinnedSelf{};
    constexpr DWORD flags=GET_MODULE_HANDLE_EX_FLAG_PIN|GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS;
    if(!GetModuleHandleExW(flags,reinterpret_cast<LPCWSTR>(&worldDrawProxy),&pinnedSelf))
        return Error{ErrorCode::Unavailable,"Cannot pin world-draw callback DLL"};
    spdlog::info("Preparing {}: CALL RVA=0x{:x}, original RVA=0x{:x}, relay=0x{:x}; SKSEPlugin_Load startup boundary; thread={}",
        worldDrawPatchId,plan.siteRva,plan.originalTargetRva,
        reinterpret_cast<std::uintptr_t>(relay->entry()),GetCurrentThreadId());
    spdlog::info("Preparing {}: CALL RVA=0x{:x}, original RVA=0x{:x}, relay=0x{:x}; immediately before IMenu::PostDisplay",
        menuDisplayPatchId,menuPlan.siteRva,menuPlan.originalTargetRva,
        reinterpret_cast<std::uintptr_t>(menuRelay->entry()));
    spdlog::info("Preparing {}: CALL RVA=0x{:x}, original RVA=0x{:x}, relay=0x{:x}; common deferred Scaleform EndFrame boundary",
        deferredUiFlushPatchId,flushPlan.siteRva,flushPlan.originalTargetRva,
        reinterpret_cast<std::uintptr_t>(flushRelay->entry()));
    auto* published=pending.release();
    active.store(published,std::memory_order_release);
    const auto menuApplied=applyCallInstruction(menuPlan,base,*menuRelay,
        CallWriteBoundary::SkyrimStartupBeforeWorldThreads);
    if(const auto error=std::get_if<Error>(&menuApplied)) {
        active.store(nullptr,std::memory_order_release);
        delete published;
        return *error;
    }
    const auto flushApplied=applyCallInstruction(flushPlan,base,*flushRelay,
        CallWriteBoundary::SkyrimStartupBeforeWorldThreads);
    if(const auto error=std::get_if<Error>(&flushApplied)) {
        const auto restored=restoreCallInstruction(menuPlan,base,*menuRelay,
            CallWriteBoundary::SkyrimStartupBeforeWorldThreads);
        if(std::holds_alternative<Error>(restored))std::terminate();
        active.store(nullptr,std::memory_order_release);
        delete published;
        return *error;
    }
    const auto applied=applyCallInstruction(plan,base,*relay,
        CallWriteBoundary::SkyrimStartupBeforeWorldThreads);
    if(const auto error=std::get_if<Error>(&applied)) {
        const auto flushRestored=restoreCallInstruction(flushPlan,base,*flushRelay,
            CallWriteBoundary::SkyrimStartupBeforeWorldThreads);
        const auto restored=restoreCallInstruction(menuPlan,base,*menuRelay,
            CallWriteBoundary::SkyrimStartupBeforeWorldThreads);
        if(std::holds_alternative<Error>(flushRestored)||
           std::holds_alternative<Error>(restored))std::terminate();
        active.store(nullptr,std::memory_order_release);
        delete published;
        return *error;
    }
    relay.release(); // Reachable for process lifetime; never freed while CALL is installed.
    menuRelay.release();
    flushRelay.release();
#ifdef RK_WITH_NGX
    try { spdlog::info("Installed {}, {} and {}: exact five-byte CALLs; native UI depth/stencil is reasserted at the common deferred Scaleform flush; requested={}; configuredQuality={}; modelPreset={}",worldDrawPatchId,menuDisplayPatchId,deferredUiFlushPatchId,published->srRequested,settings.get<Choice>("Upscaling.Quality").value,settings.get<Choice>("Upscaling.ModelPreset").value); } catch (...) {}
#else
    try { spdlog::info("Installed {}: exact five-byte CALL, original-first pass-through; no SR work",worldDrawPatchId); } catch (...) {}
#endif
    return true;
}
void probePostEnbPresentationTarget(IDXGISwapChain* swap) noexcept {
#ifdef RK_WITH_NGX
    auto* state=active.load(std::memory_order_acquire);
    if(!state||!swap)return;
    auto remaining=state->ownedPostEnbProbes.load(std::memory_order_acquire);
    if(!remaining)return;
    if(!state->ownedPostEnbProbes.compare_exchange_strong(
        remaining,remaining-1,std::memory_order_acq_rel))return;
    try {
        const auto context=state->createdContext.load(std::memory_order_relaxed);
        auto* scene=activeOwnedSceneTexture();
        if(!context||!scene)return;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> display;
        const auto got=swap->GetBuffer(0,IID_PPV_ARGS(&display));
        if(FAILED(got)||!display) {
            spdlog::warn("Owned post-ENB pixel probe unavailable: GetBuffer HRESULT=0x{:08x}",
                static_cast<std::uint32_t>(got));
            return;
        }
        probeOwnedPixels(remaining==2?"post-ENB frame 1":"post-ENB frame 2",
            reinterpret_cast<ID3D11DeviceContext*>(context),scene,display.Get());
    } catch(const std::exception& error) {
        try {spdlog::warn("Owned post-ENB pixel probe unavailable: {}",error.what());}
        catch(...) {}
    } catch(...) {
        try {spdlog::warn("Owned post-ENB pixel probe unavailable");}catch(...) {}
    }
#else
    (void)swap;
#endif
}
}
