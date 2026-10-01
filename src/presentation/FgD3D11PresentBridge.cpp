#include "rk/FgD3D11PresentBridge.hpp"
#include <Windows.h>
#include <algorithm>
#include <exception>
#include <mutex>
#include <new>
#include <variant>

namespace rk {
namespace {
struct QuarantinedBridge {
    std::shared_ptr<void> providerLifetime;
    FgInteropLifetime interop;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    Microsoft::WRL::ComPtr<IDXGISwapChain3> swap;
    std::vector<Microsoft::WRL::ComPtr<ID3D11Texture2D>> render;
    std::unique_ptr<FgD3D11AuxSwapSource> auxiliary;
    std::vector<FgSharedSurface> shared;
    Microsoft::WRL::ComPtr<ID3D12Resource> back;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commands;
    Microsoft::WRL::ComPtr<ID3D12Fence> fence;
    std::vector<FgD3D11CopySlot> copySlots;
    Microsoft::WRL::ComPtr<ID3D12Fence> copyFence;
};
std::vector<QuarantinedBridge>& quarantine() {
    static auto* owners=new std::vector<QuarantinedBridge>;
    return *owners;
}
std::mutex& quarantineMutex() {
    static auto* mutex=new std::mutex;
    return *mutex;
}
bool sameIdentity(IUnknown* a,IUnknown* b) noexcept {
    Microsoft::WRL::ComPtr<IUnknown> ia,ib;
    return a&&b&&SUCCEEDED(a->QueryInterface(IID_PPV_ARGS(&ia)))&&
        SUCCEEDED(b->QueryInterface(IID_PPV_ARGS(&ib)))&&ia.Get()==ib.Get();
}
bool boundAsOutput(ID3D11DeviceContext* context,ID3D11Texture2D* texture) noexcept {
    ID3D11RenderTargetView* views[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT]{};
    ID3D11DepthStencilView* depth{};
    context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT,
        views,&depth);
    bool found=false;
    for(auto* view:views) {
        if(!view)continue;
        Microsoft::WRL::ComPtr<ID3D11Resource> resource;
        view->GetResource(&resource);
        found|=sameIdentity(resource.Get(),texture);
        view->Release();
    }
    if(depth) {
        Microsoft::WRL::ComPtr<ID3D11Resource> resource;
        depth->GetResource(&resource);
        found|=sameIdentity(resource.Get(),texture);
        depth->Release();
    }
    return found;
}
D3D12_RESOURCE_BARRIER transition(ID3D12Resource* resource,
    D3D12_RESOURCE_STATES before,D3D12_RESOURCE_STATES after) noexcept {
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition={resource,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,before,after};
    return barrier;
}
std::uint64_t elapsedNs(std::chrono::steady_clock::time_point start) noexcept {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<
        std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count());
}
}
void FgD3D11PresentBridge::beginCpuSample(std::uint64_t realPresent) noexcept {
    cpuSample_={};
    cpuSample_.realPresent=realPresent;
    cpuSample_.queued=queueOwnsLowerSwap_;
    cpuSampleStart_=std::chrono::steady_clock::now();
    cpuSampleActive_=true;
}
FgBridgeCpuSample FgD3D11PresentBridge::endCpuSample(HRESULT result) noexcept {
    if(cpuSampleActive_)cpuSample_.totalNs=elapsedNs(cpuSampleStart_);
    cpuSample_.result=result;
    cpuSampleActive_=false;
    return cpuSample_;
}
FgD3D11PresentBridge::FgD3D11PresentBridge(FgLowerSwap lower,
    std::unique_ptr<FgSharedInputs> interop) noexcept:
    lower_(std::move(lower)),interop_(std::move(interop)) {}
FgD3D11PresentBridge::~FgD3D11PresentBridge() noexcept {
    if(queueOwnsLowerSwap_&&!poisoned_&&!drainCopySlots())poisoned_=true;
    if(!poisoned_) {
        render_.clear();
        sourceLease_=FgSourceLease{};
        auxiliary_.reset();
        return;
    }
    try {
        std::scoped_lock lock(quarantineMutex());
        quarantine().push_back({std::move(providerLifetime_),
            interop_->retainLifetime(),std::move(context_),
            std::move(swap3_),std::move(render_),std::move(auxiliary_),
            std::move(shared_),
            std::move(inFlightBack_),std::move(inFlightAllocator_),
            std::move(inFlightCommands_),std::move(inFlightFence_),
            std::move(copySlots_),std::move(copyFence_)});
    } catch(...) {
        std::terminate();
    }
}

Result<std::unique_ptr<FgD3D11PresentBridge>> FgD3D11PresentBridge::create(
    ID3D11Device* d11,ID3D11DeviceContext* context,ID3D12Device* d12,
    ID3D12CommandQueue* queue,IDXGISwapChain* lower,
    ID3D12Device* verifiedLowerNative,
    std::unique_ptr<FgD3D11AuxSwapSource> auxiliary,
    std::shared_ptr<void> providerLifetime,bool queueOwnsLowerSwap) {
    if(!d11||!context||!d12||!queue||!lower)
        return Error{ErrorCode::InvalidInput,"FG D3D11 bridge requires all devices, context, queue and lower swap"};
    auto lowerResult=FgLowerSwap::create(lower);
    if(!std::holds_alternative<FgLowerSwap>(lowerResult))
        return std::get<Error>(std::move(lowerResult));
    Microsoft::WRL::ComPtr<ID3D11Device> contextDevice;
    context->GetDevice(&contextDevice);
    if(!sameIdentity(contextDevice.Get(),d11))
        return Error{ErrorCode::Conflict,"FG bridge context belongs to a different D3D11 device"};
    if(context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)
        return Error{ErrorCode::Unsupported,"FG bridge requires an immediate D3D11 context"};
    Microsoft::WRL::ComPtr<ID3D12Device> lowerDevice;
    if(FAILED(lower->GetDevice(IID_PPV_ARGS(&lowerDevice)))||
       !sameIdentity(lowerDevice.Get(),
           verifiedLowerNative?verifiedLowerNative:d12)||
       (verifiedLowerNative&&
           (verifiedLowerNative->GetAdapterLuid().LowPart!=
                d12->GetAdapterLuid().LowPart||
            verifiedLowerNative->GetAdapterLuid().HighPart!=
                d12->GetAdapterLuid().HighPart)))
        return Error{ErrorCode::Conflict,"FG lower swap belongs to a different D3D12 device"};
    Microsoft::WRL::ComPtr<ID3D12Device> queueDevice;
    if(FAILED(queue->GetDevice(IID_PPV_ARGS(&queueDevice)))||
       !sameIdentity(queueDevice.Get(),d12))
        return Error{ErrorCode::Conflict,"FG copy queue belongs to a different D3D12 device"};
    if(queue->GetDesc().Type!=D3D12_COMMAND_LIST_TYPE_DIRECT)
        return Error{ErrorCode::Unsupported,"FG bridge requires a DIRECT D3D12 queue"};
    Microsoft::WRL::ComPtr<IDXGISwapChain3> swap3;
    if(FAILED(lower->QueryInterface(IID_PPV_ARGS(&swap3))))
        return Error{ErrorCode::Unsupported,"FG lower swap needs IDXGISwapChain3 backbuffer indexing"};
    DXGI_SWAP_CHAIN_DESC desc{};
    if(FAILED(lower->GetDesc(&desc))||!desc.BufferDesc.Width||
       !desc.BufferDesc.Height||desc.BufferCount<2||desc.BufferCount>4||
       desc.SampleDesc.Count!=1||desc.SampleDesc.Quality!=0||
       desc.BufferDesc.Format==DXGI_FORMAT_UNKNOWN)
        return Error{ErrorCode::Unsupported,"FG lower swap descriptor is not a supported flip chain"};
    if(desc.SwapEffect!=DXGI_SWAP_EFFECT_FLIP_DISCARD&&
       desc.SwapEffect!=DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL)
        return Error{ErrorCode::Unsupported,"FG lower swap must use flip presentation"};
    auto interopResult=FgSharedInputs::create(d11,d12,queue);
    if(!std::holds_alternative<std::unique_ptr<FgSharedInputs>>(interopResult))
        return std::get<Error>(std::move(interopResult));
    auto bridge=std::unique_ptr<FgD3D11PresentBridge>(new FgD3D11PresentBridge(
        std::move(std::get<FgLowerSwap>(lowerResult)),
        std::move(std::get<std::unique_ptr<FgSharedInputs>>(interopResult))));
    bridge->d11_=d11;
    bridge->context_=context;
    bridge->d12_=d12;
    bridge->queue_=queue;
    bridge->swap3_=std::move(swap3);
    bridge->providerLifetime_=std::move(providerLifetime);
    bridge->queueOwnsLowerSwap_=queueOwnsLowerSwap;
    if(queueOwnsLowerSwap) {
        if(FAILED(d12->CreateFence(0,D3D12_FENCE_FLAG_NONE,
            IID_PPV_ARGS(&bridge->copyFence_))))
            return Error{ErrorCode::Unavailable,"Cannot create owned FG copy fence"};
        bridge->copySlots_.resize(desc.BufferCount);
    }
    D3D11_TEXTURE2D_DESC source{};
    source.Width=desc.BufferDesc.Width;
    source.Height=desc.BufferDesc.Height;
    source.MipLevels=1;
    source.ArraySize=1;
    source.Format=desc.BufferDesc.Format;
    source.SampleDesc.Count=1;
    source.Usage=D3D11_USAGE_DEFAULT;
    source.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> render;
    if(auxiliary) {
        auto* externalRenderBuffer=auxiliary->buffer();
        D3D11_TEXTURE2D_DESC actual{};
        externalRenderBuffer->GetDesc(&actual);
        if(!sameIdentity(auxiliary->verifiedDevice(),d11)||
           actual.Width!=source.Width||actual.Height!=source.Height||
           actual.Format!=source.Format||actual.MipLevels!=1||
           actual.ArraySize!=1||actual.SampleDesc.Count!=1||
           actual.SampleDesc.Quality!=0||
           !(actual.BindFlags&D3D11_BIND_RENDER_TARGET))
            return Error{ErrorCode::Conflict,"External FG render buffer does not match the D3D11 device or lower swap"};
        render=externalRenderBuffer;
        bridge->auxiliary_=std::move(auxiliary);
    } else if(FAILED(d11->CreateTexture2D(&source,nullptr,&render)))
        return Error{ErrorCode::Unavailable,"Cannot create FG D3D11 render buffer"};
    bridge->render_.push_back(std::move(render));
    auto captured=bridge->auxiliary_?
        bridge->interop_->captureSource(*bridge->auxiliary_):
        bridge->interop_->captureSource(bridge->render_[0].Get());
    if(!std::holds_alternative<FgSourceLease>(captured))
        return std::get<Error>(std::move(captured));
    bridge->sourceLease_=std::move(std::get<FgSourceLease>(captured));
    for(UINT i=0;i<desc.BufferCount;++i) {
        auto shared=bridge->interop_->makeSurface(source);
        if(!std::holds_alternative<FgSharedSurface>(shared))
            return std::get<Error>(std::move(shared));
        bridge->shared_.push_back(std::move(std::get<FgSharedSurface>(shared)));
    }
    return bridge;
}
UINT FgD3D11PresentBridge::currentIndex() const noexcept {
    return swap3_->GetCurrentBackBufferIndex();
}
ID3D11Texture2D* FgD3D11PresentBridge::renderBuffer(UINT index) const noexcept {
    return index<shared_.size()?render_[0].Get():nullptr;
}
bool FgD3D11PresentBridge::waitCopyFence(std::uint64_t value) const noexcept {
    if(!value)return true;
    if(!copyFence_||!d12_||FAILED(d12_->GetDeviceRemovedReason()))return false;
    const auto completed=copyFence_->GetCompletedValue();
    if(completed==UINT64_MAX)return false;
    if(completed>=value)return true;
    const auto event=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    if(!event)return false;
    const auto armed=copyFence_->SetEventOnCompletion(value,event);
    const auto waited=SUCCEEDED(armed)?WaitForSingleObject(event,5000):WAIT_FAILED;
    CloseHandle(event);
    return waited==WAIT_OBJECT_0&&
        copyFence_->GetCompletedValue()>=value&&
        SUCCEEDED(d12_->GetDeviceRemovedReason());
}
bool FgD3D11PresentBridge::drainCopySlots() const noexcept {
    std::uint64_t latest{};
    for(const auto& slot:copySlots_)latest=std::max(latest,slot.fenceValue);
    return waitCopyFence(latest);
}
HRESULT FgD3D11PresentBridge::copyToCurrent() noexcept {
    copyResult_=copyToCurrentImpl();
    if(FAILED(copyResult_)&&SUCCEEDED(firstCopyFailure_)) {
        firstCopyFailure_=copyResult_;
        firstCopyFailurePhase_=copyPhase_;
    }
    return copyResult_;
}
HRESULT FgD3D11PresentBridge::copyToCurrentImpl() noexcept {
    copyPhase_="copy-admission";
    if(prepared_||poisoned_)return DXGI_ERROR_INVALID_CALL;
    const auto index=currentIndex();
    if(index>=shared_.size())return DXGI_ERROR_INVALID_CALL;
    if(queueOwnsLowerSwap_)return copyToCurrentQueued(index);
    // D3D11 exposes one stable logical back buffer; DXGI rotates physical
    // D3D12 destinations after Present.
    copyPhase_="shared-copy-submit";
    auto ticket=interop_->copy(context_.Get(),sourceLease_,shared_[index]);
    if(!std::holds_alternative<FgCopyTicket>(ticket)) {
        copyDetail_=std::get<Error>(std::move(ticket)).message;
        poisoned_=true;
        return E_FAIL;
    }
    copyPhase_="shared-copy-wait";
    if(!interop_->waitCopy(std::get<FgCopyTicket>(ticket).copy)) {
        poisoned_=true;
        return FAILED(d12_->GetDeviceRemovedReason())?
            d12_->GetDeviceRemovedReason():HRESULT_FROM_WIN32(ERROR_TIMEOUT);
    }
    Microsoft::WRL::ComPtr<ID3D12Resource> back;
    copyPhase_="lower-get-buffer";
    auto hr=lower_.getBuffer(index,__uuidof(ID3D12Resource),
        reinterpret_cast<void**>(back.GetAddressOf()));
    if(FAILED(hr))return hr;
    const auto a=shared_[index].d12()->GetDesc();
    const auto b=back->GetDesc();
    if(a.Width!=b.Width||a.Height!=b.Height||a.Format!=b.Format||
       a.SampleDesc.Count!=1||b.SampleDesc.Count!=1)return E_INVALIDARG;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commands;
    Microsoft::WRL::ComPtr<ID3D12Fence> done;
    copyPhase_="d3d12-command-create";
    if(FAILED(hr=d12_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&allocator)))||
       FAILED(hr=d12_->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,
        allocator.Get(),nullptr,IID_PPV_ARGS(&commands)))||
       FAILED(hr=d12_->CreateFence(0,D3D12_FENCE_FLAG_NONE,
        IID_PPV_ARGS(&done))))return hr;
    const auto sharedIn=transition(shared_[index].d12(),
        D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_COPY_SOURCE);
    const auto backIn=transition(back.Get(),
        D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_COPY_DEST);
    const D3D12_RESOURCE_BARRIER before[]{sharedIn,backIn};
    commands->ResourceBarrier(2,before);
    commands->CopyResource(back.Get(),shared_[index].d12());
    const auto sharedOut=transition(shared_[index].d12(),
        D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_COMMON);
    const auto backOut=transition(back.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PRESENT);
    const D3D12_RESOURCE_BARRIER after[]{sharedOut,backOut};
    commands->ResourceBarrier(2,after);
    copyPhase_="d3d12-command-close";
    if(FAILED(hr=commands->Close()))return hr;
    inFlightBack_=back;
    inFlightAllocator_=allocator;
    inFlightCommands_=commands;
    inFlightFence_=done;
    ID3D12CommandList* lists[]{commands.Get()};
    queue_->ExecuteCommandLists(1,lists);
    poisoned_=true;
    copyPhase_="d3d12-copy-signal";
    if(FAILED(hr=queue_->Signal(done.Get(),1))) {
        return hr;
    }
    copyPhase_="d3d12-copy-wait";
    const auto event=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    if(!event) {
        return HRESULT_FROM_WIN32(GetLastError());
    }
    hr=done->SetEventOnCompletion(1,event);
    const auto waited=SUCCEEDED(hr)?WaitForSingleObject(event,5000):WAIT_FAILED;
    CloseHandle(event);
    if(FAILED(hr)||waited!=WAIT_OBJECT_0||
       done->GetCompletedValue()!=1||
       FAILED(d12_->GetDeviceRemovedReason())) {
        return FAILED(d12_->GetDeviceRemovedReason())?
            d12_->GetDeviceRemovedReason():
            FAILED(hr)?hr:HRESULT_FROM_WIN32(ERROR_TIMEOUT);
    }
    inFlightBack_.Reset();
    inFlightAllocator_.Reset();
    inFlightCommands_.Reset();
    inFlightFence_.Reset();
    poisoned_=false;
    prepared_=true;
    preparedIndex_=index;
    copyPhase_="copy-prepared";
    return S_OK;
}
HRESULT FgD3D11PresentBridge::copyToCurrentQueued(UINT index) noexcept {
    if(index>=copySlots_.size()||!copyFence_||
       nextCopyFenceValue_==UINT64_MAX)return DXGI_ERROR_INVALID_CALL;
    auto& slot=copySlots_[index];
    copyPhase_="slot-retirement";
    const auto slotStart=cpuSampleActive_?
        std::chrono::steady_clock::now():std::chrono::steady_clock::time_point{};
    const bool slotReady=waitCopyFence(slot.fenceValue);
    if(cpuSampleActive_)cpuSample_.slotWaitNs=elapsedNs(slotStart);
    if(!slotReady) {
        poisoned_=true;
        return FAILED(d12_->GetDeviceRemovedReason())?
            d12_->GetDeviceRemovedReason():HRESULT_FROM_WIN32(ERROR_TIMEOUT);
    }
    copyPhase_="shared-copy-submit";
    const auto sharedStart=cpuSampleActive_?
        std::chrono::steady_clock::now():std::chrono::steady_clock::time_point{};
    auto ticket=interop_->copy(context_.Get(),sourceLease_,shared_[index]);
    if(cpuSampleActive_)cpuSample_.sharedCopyNs=elapsedNs(sharedStart);
    if(!std::holds_alternative<FgCopyTicket>(ticket)) {
        copyDetail_=std::get<Error>(std::move(ticket)).message;
        poisoned_=true;
        return E_FAIL;
    }
    // The private route created the lower swap on this exact queue. The
    // interop copy enqueued Wait(producerGate) on it; these commands and the
    // later lower Present therefore follow the D3D11 producer in queue order.
    poisoned_=true;
    HRESULT hr{};
    const auto d12Start=cpuSampleActive_?
        std::chrono::steady_clock::now():std::chrono::steady_clock::time_point{};
    copyPhase_="lower-get-buffer";
    if(!slot.back&&FAILED(hr=lower_.getBuffer(index,
        __uuidof(ID3D12Resource),
        reinterpret_cast<void**>(slot.back.GetAddressOf()))))return hr;
    if(!slot.back)return E_FAIL;
    const auto a=shared_[index].d12()->GetDesc();
    const auto b=slot.back->GetDesc();
    if(a.Width!=b.Width||a.Height!=b.Height||a.Format!=b.Format||
       a.SampleDesc.Count!=1||b.SampleDesc.Count!=1)return E_INVALIDARG;
    copyPhase_="d3d12-command-prepare";
    if(!slot.allocator) {
        if(FAILED(hr=d12_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&slot.allocator)))||
           FAILED(hr=d12_->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,
            slot.allocator.Get(),nullptr,IID_PPV_ARGS(&slot.commands))))return hr;
        ++copyCommandAllocations_;
    } else if(FAILED(hr=slot.allocator->Reset())||
        FAILED(hr=slot.commands->Reset(slot.allocator.Get(),nullptr)))return hr;
    const D3D12_RESOURCE_BARRIER before[]{
        transition(shared_[index].d12(),D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_STATE_COPY_SOURCE),
        transition(slot.back.Get(),D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_COPY_DEST)};
    slot.commands->ResourceBarrier(2,before);
    slot.commands->CopyResource(slot.back.Get(),shared_[index].d12());
    const D3D12_RESOURCE_BARRIER after[]{
        transition(shared_[index].d12(),D3D12_RESOURCE_STATE_COPY_SOURCE,
            D3D12_RESOURCE_STATE_COMMON),
        transition(slot.back.Get(),D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_PRESENT)};
    slot.commands->ResourceBarrier(2,after);
    copyPhase_="d3d12-command-close";
    if(FAILED(hr=slot.commands->Close()))return hr;
    ID3D12CommandList* lists[]{slot.commands.Get()};
    queue_->ExecuteCommandLists(1,lists);
    copyPhase_="d3d12-copy-signal";
    const auto value=++nextCopyFenceValue_;
    if(FAILED(hr=queue_->Signal(copyFence_.Get(),value)))return hr;
    if(cpuSampleActive_)cpuSample_.d3d12CopyNs=elapsedNs(d12Start);
    slot.fenceValue=value;
    poisoned_=false;
    prepared_=true;
    preparedIndex_=index;
    copyPhase_="copy-prepared-queued";
    return S_OK;
}
HRESULT FgD3D11PresentBridge::presentPrepared(const FgPresentCall& call) noexcept {
    presentPhase_="present-admission";
    if(poisoned_)return DXGI_ERROR_DEVICE_REMOVED;
    if((call.method==FgPresentMethod::Present&&call.parameters)||
       (call.method==FgPresentMethod::Present1&&!call.parameters)||
       (call.method!=FgPresentMethod::Present&&
        call.method!=FgPresentMethod::Present1))return E_INVALIDARG;
    if(call.flags&DXGI_PRESENT_TEST) {
        presentPhase_="lower-test";
        return lower_.present(call);
    }
    if(!prepared_||currentIndex()!=preparedIndex_)
        return DXGI_ERROR_INVALID_CALL;
    prepared_=false;
    presentPhase_="lower-present";
    const auto presentStart=cpuSampleActive_?
        std::chrono::steady_clock::now():std::chrono::steady_clock::time_point{};
    const auto result=lower_.present(call);
    if(cpuSampleActive_)cpuSample_.lowerPresentNs=elapsedNs(presentStart);
    return result;
}
HRESULT FgD3D11PresentBridge::resize(const FgResizeCall& call) noexcept {
    try {
    if(prepared_||poisoned_)return DXGI_ERROR_INVALID_CALL;
    DXGI_SWAP_CHAIN_DESC old{};
    auto hr=lower_.getDesc(&old);
    if(FAILED(hr))return hr;
    // Reject reserved bits before a proxy can alter the lower swap's Present
    // state on a failed ResizeBuffers call. 0x1fff covers the DXGI flags in
    // the pinned Windows SDK used by this build.
    constexpr UINT knownSwapFlags=0x1fffu;
    if(call.flags&~knownSwapFlags)return DXGI_ERROR_INVALID_CALL;
    auto width=call.width;
    auto height=call.height;
    if(!width||!height) {
        HWND window{};
        RECT client{};
        if(FAILED(swap3_->GetHwnd(&window))||!window||
           !GetClientRect(window,&client)||
           client.right<=client.left||client.bottom<=client.top)
            return DXGI_ERROR_UNSUPPORTED;
        if(!width)width=static_cast<UINT>(client.right-client.left);
        if(!height)height=static_cast<UINT>(client.bottom-client.top);
    }
    const auto count=call.buffers?call.buffers:old.BufferCount;
    const auto format=call.format==DXGI_FORMAT_UNKNOWN?
        old.BufferDesc.Format:call.format;
    if(count<2||count>4||format==DXGI_FORMAT_UNKNOWN)
        return DXGI_ERROR_INVALID_CALL;
    if(call.method==FgResizeMethod::ResizeBuffers) {
        if(call.creationNodeMask||call.presentQueue)return E_INVALIDARG;
    } else if(call.method==FgResizeMethod::ResizeBuffers1) {
        if(call.presentQueue)
            for(UINT i=0;i<count;++i)
                if(!sameIdentity(call.presentQueue[i],queue_.Get()))
                    return DXGI_ERROR_INVALID_CALL;
    } else return E_INVALIDARG;
    for(const auto& texture:render_) {
        if(boundAsOutput(context_.Get(),texture.Get()))
            return DXGI_ERROR_INVALID_CALL;
        if(!auxiliary_) {
            // The bridge and source lease own two references. A caller-held texture adds
            // a reference; an unbound view may not and remains a production gap.
            const auto refs=texture->AddRef();
            texture->Release();
            if(refs>3)return DXGI_ERROR_INVALID_CALL;
        }
    }
    D3D11_TEXTURE2D_DESC source{};
    render_.front()->GetDesc(&source);
    source.Width=width;
    source.Height=height;
    source.Format=format;
    std::unique_ptr<FgD3D11AuxSwapSource> nextAuxiliary;
    std::vector<Microsoft::WRL::ComPtr<ID3D11Texture2D>> nextRender;
    std::vector<FgSharedSurface> nextShared;
    std::vector<FgD3D11CopySlot> nextCopySlots;
    nextRender.reserve(1);
    nextShared.reserve(count);
    if(queueOwnsLowerSwap_)nextCopySlots.resize(count);
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    if(auxiliary_) {
        auto prepared=auxiliary_->prepare(width,height,format);
        if(!std::holds_alternative<
           std::unique_ptr<FgD3D11AuxSwapSource>>(prepared))return E_FAIL;
        nextAuxiliary=std::move(std::get<
            std::unique_ptr<FgD3D11AuxSwapSource>>(prepared));
        texture=nextAuxiliary->buffer();
        D3D11_TEXTURE2D_DESC actualTexture{};
        texture->GetDesc(&actualTexture);
        if(actualTexture.Width!=width||actualTexture.Height!=height||
           actualTexture.Format!=format)return E_FAIL;
    } else if(FAILED(hr=d11_->CreateTexture2D(&source,nullptr,&texture)))
        return hr;
    nextRender.push_back(std::move(texture));
    auto captured=nextAuxiliary?interop_->captureSource(*nextAuxiliary):
        interop_->captureSource(nextRender[0].Get());
    if(!std::holds_alternative<FgSourceLease>(captured))return E_FAIL;
    auto nextSourceLease=std::move(std::get<FgSourceLease>(captured));
    for(UINT i=0;i<count;++i) {
        auto shared=interop_->makeSurface(source);
        if(!std::holds_alternative<FgSharedSurface>(shared))return E_FAIL;
        nextShared.push_back(std::move(std::get<FgSharedSurface>(shared)));
    }
    if(queueOwnsLowerSwap_) {
        if(!drainCopySlots()) {
            poisoned_=true;
            return FAILED(d12_->GetDeviceRemovedReason())?
                d12_->GetDeviceRemovedReason():HRESULT_FROM_WIN32(ERROR_TIMEOUT);
        }
        for(auto& slot:copySlots_)slot.back.Reset();
    }
    hr=lower_.resize(call);
    if(FAILED(hr))return hr;
    DXGI_SWAP_CHAIN_DESC actual{};
    if(FAILED(lower_.getDesc(&actual))||actual.BufferCount!=count||
       actual.BufferDesc.Width!=width||
       actual.BufferDesc.Height!=height||
       actual.BufferDesc.Format!=format) {
        poisoned_=true;
        return E_FAIL;
    }
    render_.swap(nextRender);
    shared_.swap(nextShared);
    if(queueOwnsLowerSwap_)copySlots_.swap(nextCopySlots);
    auxiliary_.swap(nextAuxiliary);
    sourceLease_=std::move(nextSourceLease);
    nextRender.clear();
    nextAuxiliary.reset();
    return S_OK;
    } catch(const std::bad_alloc&) {
        return E_OUTOFMEMORY;
    }
}
}
