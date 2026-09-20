#include "rk/WorldDrawHook.hpp"
#include "rk/CallSite.hpp"
#include "rk/FrameProbe.hpp"
#include "rk/PatchDescriptor.hpp"
#include "rk/PipelineBoundary.hpp"
#include "rk/RendererHook.hpp"
#include "rk/SwapObserver.hpp"
#include "rk/SrInput.hpp"
#include "rk/StagePairCapture.hpp"
#include "rk/WorldDraw.hpp"
#include "rk/DiagnosticsMenu.hpp"
#include "rk/DrsHook.hpp"
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
#include <string>
#include <filesystem>
#include <wrl/client.h>

namespace rk {
namespace {
struct WorldState {
    WorldDrawForwarder forwarder;
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
    std::atomic<std::uintptr_t> worldColourIdentity{0};
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
void worldDrawProxy(void* world,std::uint32_t flags) noexcept {
    auto* state=active.load(std::memory_order_acquire);
    const auto sequence=state->forwarded.load(std::memory_order_relaxed)+1;
    const bool sample=sequence<=3||sequence%600==0;
    const auto before=sample?readWorldNumbers(world,state->expectedRenderer):WorldNumbers{};
    state->forwarder.dispatch(world,flags);
    state->displayedMode.store(DisplayMode::Native,std::memory_order_release);
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
    if(const auto status=state->copyStatus.load(std::memory_order_acquire);
       status==WorldState::CopyStatus::NotAttempted||status==WorldState::CopyStatus::Pending)
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
    if(drsProbeHasRun())
        state->displayedMode.store(DisplayMode::Native,std::memory_order_release);
    if(state->completedOutput&&!state->sdrDisabled&&!drsProbeHasRun()) {
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
    state->statusSkippedFrames.store(state->sdrSkipped+state->sdrJitterSkipped,
        std::memory_order_relaxed);
    state->statusDlssDisabled.store(state->sdrDisabled,std::memory_order_relaxed);
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
        try { spdlog::info("Experimental DRS extent map: kMAIN={}x{} format={}; motion={}x{} format={}; depth={}x{} format={}; display={}x{} format={}; DLSS SR not submitted",
            colour.Width,colour.Height,static_cast<unsigned>(colour.Format),
            motion.Width,motion.Height,static_cast<unsigned>(motion.Format),
            depth.Width,depth.Height,static_cast<unsigned>(depth.Format),
            display.Width,display.Height,static_cast<unsigned>(display.Format)); } catch(...) {}
    }
    try {
        spdlog::info("World stage #{}: thread={}; flags=0x{:x}; rendererMatch={}; beforeRead={}; afterRead={}; "
            "beforeLock={}/{}; afterLock={}/{}; device=0x{:x}; context=0x{:x}; swap=0x{:x}; "
            "colour=0x{:x}->0x{:x}; motion=0x{:x}->0x{:x}; depth=0x{:x}->0x{:x}; read-only",
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
    if(const auto reduced=drsProbeRenderExtent()) {
        snapshot.renderWidth=reduced->width;
        snapshot.renderHeight=reduced->height;
    } else {
        snapshot.renderWidth=snapshot.displayWidth;
        snapshot.renderHeight=snapshot.displayHeight;
    }
    snapshot.worldFrames=state->forwarded.load(std::memory_order_relaxed);
    snapshot.dlssFrames=state->statusDlssFrames.load(std::memory_order_relaxed);
    snapshot.skippedFrames=state->statusSkippedFrames.load(std::memory_order_relaxed);
    snapshot.dlssDisabled=state->statusDlssDisabled.load(std::memory_order_relaxed);
    return snapshot;
}
void probePresentationTargets(IDXGISwapChain* swap) noexcept {
    auto* state=active.load(std::memory_order_acquire);
    if(!state||!swap||state->createdSwap.load(std::memory_order_acquire)!=
       reinterpret_cast<std::uintptr_t>(swap)||
       !state->presentTargetProbeDue.exchange(false,std::memory_order_acq_rel))return;
    try {
        const auto context=state->createdContext.load(std::memory_order_relaxed);
        if(!context)return;
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
Result<bool> installWorldDrawPassThrough(HMODULE game,std::string_view verifiedGameHash,
    const Settings& settings) {
    if(!settings.get<bool>("General.Enabled")||settings.get<bool>("General.SafeMode")||
       !settings.get<bool>("Patching.EnableVersionedPatches")||
       !settings.get<bool>("Patching.ExperimentalPatches")||
       patchDisabled(settings.get<Text>("Patching.DisabledPatchIds").value,worldDrawPatchId)) {
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
    auto pending=std::make_unique<WorldState>();
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
        spdlog::info("Skyrim camera jitter source armed: game RVA=0x328cc20; exact caller/CALL/target bytes verified; same-frame DLAA jitter enabled");
    } else spdlog::warn("Skyrim camera jitter source unavailable: caller/CALL/target bytes differ; DLAA will retain native frames");
    const auto configured=pending->forwarder.configure(
        reinterpret_cast<WorldDrawFn>(base+plan.originalTargetRva),&afterOriginal);
    if(const auto error=std::get_if<Error>(&configured))return *error;
    auto relayResult=prepareNearCallRelay(plan,base,reinterpret_cast<std::uintptr_t>(&worldDrawProxy));
    if(const auto error=std::get_if<Error>(&relayResult))return *error;
    auto relay=std::make_unique<NearCallRelay>(std::move(std::get<NearCallRelay>(relayResult)));
    HMODULE pinnedSelf{};
    constexpr DWORD flags=GET_MODULE_HANDLE_EX_FLAG_PIN|GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS;
    if(!GetModuleHandleExW(flags,reinterpret_cast<LPCWSTR>(&worldDrawProxy),&pinnedSelf))
        return Error{ErrorCode::Unavailable,"Cannot pin world-draw callback DLL"};
    spdlog::info("Preparing {}: CALL RVA=0x{:x}, original RVA=0x{:x}, relay=0x{:x}; SKSEPlugin_Load startup boundary; thread={}",
        worldDrawPatchId,plan.siteRva,plan.originalTargetRva,
        reinterpret_cast<std::uintptr_t>(relay->entry()),GetCurrentThreadId());
    auto* published=pending.release();
    active.store(published,std::memory_order_release);
    const auto applied=applyCallInstruction(plan,base,*relay,
        CallWriteBoundary::SkyrimStartupBeforeWorldThreads);
    if(const auto error=std::get_if<Error>(&applied)) {
        active.store(nullptr,std::memory_order_release);
        delete published;
        return *error;
    }
    relay.release(); // Reachable for process lifetime; never freed while CALL is installed.
#ifdef RK_WITH_NGX
    try { spdlog::info("Installed {}: exact five-byte CALL, original-first pass-through; experimental SDR DLAA armed",worldDrawPatchId); } catch (...) {}
#else
    try { spdlog::info("Installed {}: exact five-byte CALL, original-first pass-through; no SR work",worldDrawPatchId); } catch (...) {}
#endif
    return true;
}
}
