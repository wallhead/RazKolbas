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
#include "rk/DiagnosticsMenu.hpp"
#include "rk/DrsHook.hpp"
#include "rk/DrsReadiness.hpp"
#include "rk/RenderSizePolicy.hpp"
#include "rk/RendererBootstrap.hpp"
#include "rk/NativeFlipTarget.hpp"
#include "rk/NativeUiRedirector.hpp"
#ifdef RK_WITH_NGX
#include "rk/OffscreenDlssProbe.hpp"
#include "rk/SdrDlssPresenter.hpp"
#endif
#include <spdlog/spdlog.h>
#include <ShlObj.h>
#include <array>
#include <atomic>
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
struct WorldState {
    WorldDrawForwarder forwarder;
    MenuDisplayForwarder menuForwarder;
    std::atomic<std::uint64_t> forwarded{0};
    std::atomic<DisplayMode> displayedMode{DisplayMode::Native};
    std::atomic<std::uint32_t> statusWidth{0},statusHeight{0};
    std::atomic<std::uint64_t> statusDlssFrames{0},statusSkippedFrames{0};
    std::atomic<bool> statusDlssDisabled{false};
    std::uintptr_t expectedRenderer{};
    std::uintptr_t jitterCamera{};
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
    std::array<std::optional<SdrSrFrameResult>,3> srFallbacks;
    std::vector<SdrSrFrameResult> ownedFallbacks;
    bool srRequested{};
    bool srDisabled{};
    OwnedSceneAdmissionGate ownedSceneGate;
    OwnedSceneAdmissionGate menuSceneGate;
    bool ownedInputCaptureOnly{};
    bool ownedSpatialBaseline{};
    UpscaleQuality earlyQuality{UpscaleQuality::Quality};
    double manualRenderScale{};
    bool automaticMipBias{true};
    double manualMipBias{};
    float srSharpness{};
    std::uint64_t ownedEvaluationLimit{};
    bool ownedInputCaptureAttempted{};
    std::uint64_t ownedNgxCreatedAt{};
    bool ownedNgxInitFailed{};
    bool nativeUiRouteActivated{};
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
                    state->nextCopyFrame=state->forwarded.load(std::memory_order_relaxed)+600;
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
    if(state->copyAttempts>=24) {
        state->copyStatus.store(WorldState::CopyStatus::Failed,std::memory_order_release);
        try { spdlog::warn("Offscreen DLAA world-depth readiness not reached in 24 attempts; no NGX evaluation"); } catch(...) {}
        return;
    }
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
            if(ready&&!wasReady) {
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
                    state->menuSceneGate.ready():state->ownedSceneGate.ready();
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
                    spdlog::info("Owned NGX stage frame {}: feature created; waiting 120 frames before evaluation",
                        sequence);
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
                if(firstAttempt) {
                    const auto jitter=std::get<NgxJitter>(renderJitter);
                    spdlog::info("Owned NGX evaluation parameters: jitter=({},{}); MVScale={}x{}; ngxSharpness=0; postSharpness={}; autoExposure=true; reset=true",
                        jitter.x,jitter.y,domain->plan().render.width,
                        domain->plan().render.height,state->srSharpness);
                }
                auto evaluated=state->srPresenter.evaluateOwnedScene(device,context,sources,
                    domain->plan().display.width,domain->plan().display.height,
                    SrFrameMetadata{sequence,domain->plan().generation,false,
                        boundary==OwnedPublicationBoundary::MenuDisplay?
                            SrSourcePhase::MenuDisplay:SrSourcePhase::PrePresent},
                    std::get<NgxJitter>(renderJitter));
                if(firstAttempt)
                    spdlog::info("Owned NGX stage frame {}: first evaluation returned",sequence);
                if(const auto error=std::get_if<Error>(&evaluated)) {
                    if(error->code==ErrorCode::DeviceRemoved)return *error;
                    return Error{ErrorCode::Unavailable,error->message};
                }
                const auto token=std::get<std::optional<SrEvaluationToken>>(evaluated);
                if(!token)return false;
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
            state->displayedMode.store(DisplayMode::DlssSr,std::memory_order_release);
            state->statusDlssFrames.store(state->srPresenter.submittedFrames(),
                std::memory_order_relaxed);
        } else {
            state->displayedMode.store(DisplayMode::SpatialFallback,std::memory_order_release);
            state->srPresenter.requestReset();
            state->ownedFallbacks.emplace_back(std::move(outcome));
            ++state->srSkipped;
        }
        if(sequence==1) {
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
            if(frame>12&&state->menuSceneGate.ready()&&
               ui->latePassRoutingAvailable()) {
                if(processOwnedWorldFrame(state,
                    reinterpret_cast<void*>(state->expectedRenderer),frame,
                    OwnedPublicationBoundary::MenuDisplay)&&
                   domain->phase()==ScenePhase::NativeUi&&
                   !state->nativeUiRouteActivated) {
                    state->nativeUiRouteActivated=true;
                    try {spdlog::info("Owned native UI resource route activated at frame {} after same-boundary colour/depth admission; DLSS submissions={}",
                        frame,state->srPresenter.submittedFrames());}catch(...) {}
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
        }
    }
#endif
}
void menuDisplayProxy(void* first,std::uint32_t second,std::uint32_t third,
    std::uint32_t fourth) noexcept {
    auto* state=active.load(std::memory_order_acquire);
    if(!state)std::terminate();
    state->menuForwarder.dispatch(first,second,third,fourth);
}
void worldDrawProxy(void* world,std::uint32_t flags) noexcept {
    auto* state=active.load(std::memory_order_acquire);
    const auto sequence=state->forwarded.load(std::memory_order_relaxed)+1;
    const bool sample=sequence<=3||sequence%600==0;
    const auto before=sample?readWorldNumbers(world,state->expectedRenderer):WorldNumbers{};
    state->forwarder.dispatch(world,flags);
    state->displayedMode.store(DisplayMode::Native,std::memory_order_release);
#ifdef RK_WITH_NGX
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
#endif
    snapshot.worldFrames=state->forwarded.load(std::memory_order_relaxed);
    snapshot.dlssFrames=state->statusDlssFrames.load(std::memory_order_relaxed);
    snapshot.skippedFrames=state->statusSkippedFrames.load(std::memory_order_relaxed);
    snapshot.dlssDisabled=state->statusDlssDisabled.load(std::memory_order_relaxed);
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
        if(auto* ui=ownedUiRedirector())
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
                } else if(ui->companionsReady()&&frame<=2) {
                    try {spdlog::info("Owned native UI companions ready at frame {}: reduced MRT/depth roles learned and display-sized counterparts allocated",
                        frame);}catch(...) {}
                }
            }
        processOwnedWorldFrame(state,reinterpret_cast<void*>(state->expectedRenderer),
            frame,OwnedPublicationBoundary::PrePresent);
    } else if(auto* closingDomain=activeOwnedSceneDomain();
              closingDomain&&closingDomain->phase()==ScenePhase::NativeUi) {
        const auto frame=state->forwarded.load(std::memory_order_relaxed);
        auto* ui=ownedUiRedirector();
        if(!ui) {
            state->srDisabled=true;
            state->statusDlssDisabled.store(true,std::memory_order_release);
            closingDomain->suspend();
            try {spdlog::warn("Owned menu-boundary UI route suspended without its context hook at pre-Present frame {}",
                frame);}catch(...) {}
        } else if(ui->compatibilityFault()) {
            ui->disableLatePassRouting();
            if(!closingDomain->closePublishedFrame(frame,
                closingDomain->plan().generation)) {
                state->srDisabled=true;
                state->statusDlssDisabled.store(true,std::memory_order_release);
                closingDomain->suspend();
            }
            try {spdlog::warn("Owned native UI contract changed at frame {}; continuing with stable pre-Present publication",
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
    if(!usualProbe&&!ownedProbe)return;
    if(ownedProbe)state->ownedPrePresentProbes.store(ownedRemaining-1,std::memory_order_release);
    try {
        const auto context=state->createdContext.load(std::memory_order_relaxed);
        if(!context)return;
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
       patchDisabled(settings.get<Text>("Patching.DisabledPatchIds").value,menuDisplayPatchId)) {
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
    auto pending=std::make_unique<WorldState>();
#ifdef RK_WITH_NGX
    const auto provider=settings.get<Choice>("Upscaling.Provider").value;
    const auto quality=parseUpscaleQuality(
        settings.get<Choice>("Upscaling.Quality").value);
    if(!quality)return Error{ErrorCode::InvalidInput,"Unrecognized SR quality setting"};
    if(const auto configured=pending->srPresenter.configureQuality(*quality);
       const auto error=std::get_if<Error>(&configured))return *error;
    pending->srSharpness=settings.get<bool>("Upscaling.Sharpening")?
        static_cast<float>(settings.get<double>("Upscaling.Sharpness")):0.0f;
    if(const auto configured=pending->srPresenter.configureSharpness(
        settings.get<bool>("Upscaling.Sharpening"),pending->srSharpness);
       const auto error=std::get_if<Error>(&configured))return *error;
    pending->srRequested=(provider=="Auto"||provider=="DLSS")&&
        *quality!=UpscaleQuality::NativeAA;
    pending->earlyQuality=*quality;
    pending->manualRenderScale=settings.get<double>("Upscaling.ManualRenderScale");
    pending->automaticMipBias=
        settings.get<Choice>("Upscaling.MipBiasMode").value=="Auto";
    pending->manualMipBias=settings.get<double>("Upscaling.ManualMipBias");
    pending->ownedSpatialBaseline=
        settings.get<bool>("Diagnostics.SpatialBaselineOnly");
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
    auto relayResult=prepareNearCallRelay(plan,base,reinterpret_cast<std::uintptr_t>(&worldDrawProxy));
    if(const auto error=std::get_if<Error>(&relayResult))return *error;
    auto relay=std::make_unique<NearCallRelay>(std::move(std::get<NearCallRelay>(relayResult)));
    auto menuRelayResult=prepareNearCallRelay(menuPlan,base,
        reinterpret_cast<std::uintptr_t>(&menuDisplayProxy));
    if(const auto error=std::get_if<Error>(&menuRelayResult))return *error;
    auto menuRelay=std::make_unique<NearCallRelay>(
        std::move(std::get<NearCallRelay>(menuRelayResult)));
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
    auto* published=pending.release();
    active.store(published,std::memory_order_release);
    const auto menuApplied=applyCallInstruction(menuPlan,base,*menuRelay,
        CallWriteBoundary::SkyrimStartupBeforeWorldThreads);
    if(const auto error=std::get_if<Error>(&menuApplied)) {
        active.store(nullptr,std::memory_order_release);
        delete published;
        return *error;
    }
    const auto applied=applyCallInstruction(plan,base,*relay,
        CallWriteBoundary::SkyrimStartupBeforeWorldThreads);
    if(const auto error=std::get_if<Error>(&applied)) {
        const auto restored=restoreCallInstruction(menuPlan,base,*menuRelay,
            CallWriteBoundary::SkyrimStartupBeforeWorldThreads);
        if(std::holds_alternative<Error>(restored))std::terminate();
        active.store(nullptr,std::memory_order_release);
        delete published;
        return *error;
    }
    relay.release(); // Reachable for process lifetime; never freed while CALL is installed.
    menuRelay.release();
#ifdef RK_WITH_NGX
    try { spdlog::info("Installed {} and {}: exact five-byte CALLs; menu marker is read-only for bounded resource tracing; owned SR publishes at pre-Present; requested={}; configuredQuality={}",worldDrawPatchId,menuDisplayPatchId,published->srRequested,settings.get<Choice>("Upscaling.Quality").value); } catch (...) {}
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
