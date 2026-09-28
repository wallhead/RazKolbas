#include "rk/FgD3D11PresentBridge.hpp"
#include <Windows.h>
#include <exception>
#include <mutex>
#include <new>
#include <variant>

namespace rk {
namespace {
struct QuarantinedBridge {
    FgInteropLifetime interop;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    Microsoft::WRL::ComPtr<IDXGISwapChain3> swap;
    std::vector<Microsoft::WRL::ComPtr<ID3D11Texture2D>> render;
    std::vector<FgSharedSurface> shared;
    Microsoft::WRL::ComPtr<ID3D12Resource> back;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commands;
    Microsoft::WRL::ComPtr<ID3D12Fence> fence;
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
D3D12_RESOURCE_BARRIER transition(ID3D12Resource* resource,
    D3D12_RESOURCE_STATES before,D3D12_RESOURCE_STATES after) noexcept {
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition={resource,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,before,after};
    return barrier;
}
}
FgD3D11PresentBridge::FgD3D11PresentBridge(FgLowerSwap lower,
    std::unique_ptr<FgSharedInputs> interop) noexcept:
    lower_(std::move(lower)),interop_(std::move(interop)) {}
FgD3D11PresentBridge::~FgD3D11PresentBridge() noexcept {
    if(!poisoned_)return;
    try {
        std::scoped_lock lock(quarantineMutex());
        quarantine().push_back({interop_->retainLifetime(),std::move(context_),
            std::move(swap3_),std::move(render_),std::move(shared_),
            std::move(inFlightBack_),std::move(inFlightAllocator_),
            std::move(inFlightCommands_),std::move(inFlightFence_)});
    } catch(...) {
        std::terminate();
    }
}

Result<std::unique_ptr<FgD3D11PresentBridge>> FgD3D11PresentBridge::create(
    ID3D11Device* d11,ID3D11DeviceContext* context,ID3D12Device* d12,
    ID3D12CommandQueue* queue,IDXGISwapChain* lower,
    ID3D12Device* verifiedLowerNative) {
    if(!d11||!context||!d12||!queue||!lower)
        return Error{ErrorCode::InvalidInput,"FG D3D11 bridge requires all devices, context, queue and lower swap"};
    auto lowerResult=FgLowerSwap::create(lower);
    if(!std::holds_alternative<FgLowerSwap>(lowerResult))
        return std::get<Error>(std::move(lowerResult));
    Microsoft::WRL::ComPtr<ID3D11Device> contextDevice;
    context->GetDevice(&contextDevice);
    if(!sameIdentity(contextDevice.Get(),d11))
        return Error{ErrorCode::Conflict,"FG bridge context belongs to a different D3D11 device"};
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
    D3D11_TEXTURE2D_DESC source{};
    source.Width=desc.BufferDesc.Width;
    source.Height=desc.BufferDesc.Height;
    source.MipLevels=1;
    source.ArraySize=1;
    source.Format=desc.BufferDesc.Format;
    source.SampleDesc.Count=1;
    source.Usage=D3D11_USAGE_DEFAULT;
    source.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
    for(UINT i=0;i<desc.BufferCount;++i) {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> render;
        if(FAILED(d11->CreateTexture2D(&source,nullptr,&render)))
            return Error{ErrorCode::Unavailable,"Cannot create FG D3D11 render buffer"};
        auto shared=bridge->interop_->makeSurface(source);
        if(!std::holds_alternative<FgSharedSurface>(shared))
            return std::get<Error>(std::move(shared));
        bridge->render_.push_back(std::move(render));
        bridge->shared_.push_back(std::move(std::get<FgSharedSurface>(shared)));
    }
    return bridge;
}
UINT FgD3D11PresentBridge::currentIndex() const noexcept {
    return swap3_->GetCurrentBackBufferIndex();
}
ID3D11Texture2D* FgD3D11PresentBridge::renderBuffer(UINT index) const noexcept {
    return index<render_.size()?render_[index].Get():nullptr;
}
HRESULT FgD3D11PresentBridge::copyToCurrent() noexcept {
    if(prepared_||poisoned_)return DXGI_ERROR_INVALID_CALL;
    const auto index=currentIndex();
    if(index>=render_.size())return DXGI_ERROR_INVALID_CALL;
    auto ticket=interop_->copy(context_.Get(),render_[index].Get(),shared_[index]);
    if(!std::holds_alternative<FgCopyTicket>(ticket)) {
        poisoned_=true;
        return E_FAIL;
    }
    if(!interop_->waitCopy(std::get<FgCopyTicket>(ticket).copy)) {
        poisoned_=true;
        return FAILED(d12_->GetDeviceRemovedReason())?
            d12_->GetDeviceRemovedReason():HRESULT_FROM_WIN32(ERROR_TIMEOUT);
    }
    Microsoft::WRL::ComPtr<ID3D12Resource> back;
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
    if(FAILED(hr=commands->Close()))return hr;
    inFlightBack_=back;
    inFlightAllocator_=allocator;
    inFlightCommands_=commands;
    inFlightFence_=done;
    ID3D12CommandList* lists[]{commands.Get()};
    queue_->ExecuteCommandLists(1,lists);
    poisoned_=true;
    if(FAILED(hr=queue_->Signal(done.Get(),1))) {
        return hr;
    }
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
    return S_OK;
}
HRESULT FgD3D11PresentBridge::presentPrepared(const FgPresentCall& call) noexcept {
    if(poisoned_)return DXGI_ERROR_DEVICE_REMOVED;
    if(call.flags&DXGI_PRESENT_TEST)return lower_.present(call);
    if(!prepared_||currentIndex()!=preparedIndex_)
        return DXGI_ERROR_INVALID_CALL;
    prepared_=false;
    return lower_.present(call);
}
HRESULT FgD3D11PresentBridge::resize(const FgResizeCall& call) noexcept {
    try {
    if(prepared_||poisoned_)return DXGI_ERROR_INVALID_CALL;
    if(!call.width||!call.height)
        return DXGI_ERROR_UNSUPPORTED;
    DXGI_SWAP_CHAIN_DESC old{};
    auto hr=lower_.getDesc(&old);
    if(FAILED(hr))return hr;
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
        // The bridge owns one reference. A caller-held buffer or view prevents
        // replacing this generation, just as DXGI rejects held backbuffers.
        const auto refs=texture->AddRef();
        texture->Release();
        if(refs>2)return DXGI_ERROR_INVALID_CALL;
    }
    D3D11_TEXTURE2D_DESC source{};
    render_.front()->GetDesc(&source);
    source.Width=call.width;
    source.Height=call.height;
    source.Format=format;
    std::vector<Microsoft::WRL::ComPtr<ID3D11Texture2D>> nextRender;
    std::vector<FgSharedSurface> nextShared;
    nextRender.reserve(count);
    nextShared.reserve(count);
    for(UINT i=0;i<count;++i) {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
        if(FAILED(hr=d11_->CreateTexture2D(&source,nullptr,&texture)))return hr;
        auto shared=interop_->makeSurface(source);
        if(!std::holds_alternative<FgSharedSurface>(shared))return E_FAIL;
        nextRender.push_back(std::move(texture));
        nextShared.push_back(std::move(std::get<FgSharedSurface>(shared)));
    }
    hr=lower_.resize(call);
    if(FAILED(hr))return hr;
    DXGI_SWAP_CHAIN_DESC actual{};
    if(FAILED(lower_.getDesc(&actual))||actual.BufferCount!=count||
       actual.BufferDesc.Width!=call.width||
       actual.BufferDesc.Height!=call.height||
       actual.BufferDesc.Format!=format) {
        poisoned_=true;
        return E_FAIL;
    }
    render_.swap(nextRender);
    shared_.swap(nextShared);
    return S_OK;
    } catch(const std::bad_alloc&) {
        return E_OUTOFMEMORY;
    }
}
}
