#include "rk/FgSharedInputs.hpp"
#include <dxgi1_2.h>
#include <Windows.h>
#include <atomic>

namespace rk {
namespace {
std::atomic<std::uint64_t> nextBridgeId{1};
bool sameLuid(LUID a,LUID b) noexcept {
    return a.LowPart==b.LowPart&&a.HighPart==b.HighPart;
}
bool sameDesc(const D3D11_TEXTURE2D_DESC& a,
    const D3D11_TEXTURE2D_DESC& b) noexcept {
    return a.Width==b.Width&&a.Height==b.Height&&a.Format==b.Format&&
        a.MipLevels==b.MipLevels&&a.ArraySize==b.ArraySize&&
        a.SampleDesc.Count==b.SampleDesc.Count&&
        a.SampleDesc.Quality==b.SampleDesc.Quality;
}
bool sameDevice(ID3D11Device* a,ID3D11Device* b) noexcept {
    Microsoft::WRL::ComPtr<IUnknown> identityA,identityB;
    return a&&b&&SUCCEEDED(a->QueryInterface(IID_PPV_ARGS(&identityA)))&&
        SUCCEEDED(b->QueryInterface(IID_PPV_ARGS(&identityB)))&&
        identityA.Get()==identityB.Get();
}
}
Result<std::unique_ptr<FgSharedInputs>> FgSharedInputs::create(
    ID3D11Device* d11,ID3D12Device* d12,ID3D12CommandQueue* queue) {
    if(!d11||!d12||!queue)
        return Error{ErrorCode::InvalidInput,"FG interop requires both devices and a D3D12 queue"};
    auto bridge=std::unique_ptr<FgSharedInputs>(new FgSharedInputs);
    bridge->id_=nextBridgeId.fetch_add(1,std::memory_order_relaxed);
    if(FAILED(d11->QueryInterface(IID_PPV_ARGS(&bridge->d11_))))
        return Error{ErrorCode::Unsupported,"D3D11 fence interface is unavailable"};
    bridge->d12_=d12;
    bridge->queue_=queue;
    Microsoft::WRL::ComPtr<IDXGIDevice> dxgi;
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    DXGI_ADAPTER_DESC desc{};
    if(FAILED(d11->QueryInterface(IID_PPV_ARGS(&dxgi)))||
       FAILED(dxgi->GetAdapter(&adapter))||FAILED(adapter->GetDesc(&desc)))
        return Error{ErrorCode::Unavailable,"Cannot identify FG D3D11 adapter"};
    if(!sameLuid(desc.AdapterLuid,d12->GetAdapterLuid()))
        return Error{ErrorCode::Conflict,"FG D3D11 and D3D12 adapter LUIDs differ"};
    Microsoft::WRL::ComPtr<ID3D12Device> queueDevice;
    if(FAILED(queue->GetDevice(IID_PPV_ARGS(&queueDevice)))||
       !sameLuid(queueDevice->GetAdapterLuid(),desc.AdapterLuid))
        return Error{ErrorCode::Conflict,"FG command queue is on a different adapter"};
    if(FAILED(bridge->d11_->CreateFence(0,D3D11_FENCE_FLAG_SHARED,
        IID_PPV_ARGS(&bridge->producerFence_))))
        return Error{ErrorCode::Unavailable,"Cannot create FG shared producer fence"};
    HANDLE handle{};
    const auto shared=bridge->producerFence_->CreateSharedHandle(nullptr,
        GENERIC_ALL,nullptr,&handle);
    if(FAILED(shared)||!handle)
        return Error{ErrorCode::Unavailable,"Cannot share FG producer fence"};
    const auto opened=d12->OpenSharedHandle(handle,
        IID_PPV_ARGS(&bridge->consumerFence_));
    CloseHandle(handle);
    if(FAILED(opened))
        return Error{ErrorCode::Unavailable,"Cannot open FG fence on D3D12"};
    return bridge;
}
Result<FgSharedSurface> FgSharedInputs::makeSurface(
    const D3D11_TEXTURE2D_DESC& input) const {
    if(!input.Width||!input.Height||input.MipLevels!=1||input.ArraySize!=1||
       input.SampleDesc.Count!=1||input.SampleDesc.Quality!=0||
       input.Format==DXGI_FORMAT_UNKNOWN)
        return Error{ErrorCode::InvalidInput,"FG shared surface requires a single-sample 2D input"};
    auto desc=input;
    desc.Usage=D3D11_USAGE_DEFAULT;
    desc.CPUAccessFlags=0;
    desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    desc.MiscFlags=D3D11_RESOURCE_MISC_SHARED|D3D11_RESOURCE_MISC_SHARED_NTHANDLE;
    FgSharedSurface surface;
    if(FAILED(d11_->CreateTexture2D(&desc,nullptr,&surface.d11_)))
        return Error{ErrorCode::Unavailable,"Cannot create FG shared D3D11 surface"};
    Microsoft::WRL::ComPtr<IDXGIResource1> shared;
    if(FAILED(surface.d11_.As(&shared)))
        return Error{ErrorCode::Unavailable,"FG surface lacks NT sharing interface"};
    HANDLE handle{};
    const auto created=shared->CreateSharedHandle(nullptr,
        DXGI_SHARED_RESOURCE_READ|DXGI_SHARED_RESOURCE_WRITE,nullptr,&handle);
    if(FAILED(created)||!handle)
        return Error{ErrorCode::Unavailable,"Cannot share FG surface"};
    const auto opened=d12_->OpenSharedHandle(handle,IID_PPV_ARGS(&surface.d12_));
    CloseHandle(handle);
    if(FAILED(opened)||!surface.d12_)
        return Error{ErrorCode::Unavailable,"Cannot open FG surface on D3D12"};
    surface.ownerId_=id_;
    return surface;
}
Result<FgCopyTicket> FgSharedInputs::copy(ID3D11DeviceContext* context,
    ID3D11Texture2D* source,const FgSharedSurface& target) {
    if(!context||!source||!target.d11_||!target.d12_||target.ownerId_!=id_)
        return Error{ErrorCode::InvalidInput,"FG copy is missing a resource or context"};
    D3D11_TEXTURE2D_DESC sourceDesc{},targetDesc{};
    source->GetDesc(&sourceDesc);
    target.d11_->GetDesc(&targetDesc);
    if(!sameDesc(sourceDesc,targetDesc))
        return Error{ErrorCode::InvalidInput,"FG source and shared surface descriptors differ"};
    Microsoft::WRL::ComPtr<ID3D11DeviceContext4> context4;
    Microsoft::WRL::ComPtr<ID3D11Device> contextDevice;
    Microsoft::WRL::ComPtr<ID3D11Device> sourceDevice;
    context->GetDevice(&contextDevice);
    source->GetDevice(&sourceDevice);
    if(!sameDevice(contextDevice.Get(),d11_.Get())||
       !sameDevice(sourceDevice.Get(),d11_.Get())||
       FAILED(context->QueryInterface(IID_PPV_ARGS(&context4))))
        return Error{ErrorCode::Conflict,"FG copy requires the matching D3D11 immediate context"};
    std::scoped_lock lock(mutex_);
    if(failed_||nextValue_>UINT64_MAX-2)
        return Error{ErrorCode::Unavailable,"FG interop fence timeline is unavailable"};
    const FgCopyTicket ticket{nextValue_+1,nextValue_+2};
    nextValue_=ticket.copy;
    context->CopyResource(target.d11_.Get(),source);
    if(FAILED(context4->Signal(producerFence_.Get(),ticket.producer))) {
        failed_=true;
        return Error{ErrorCode::DeviceRemoved,"Cannot signal FG producer completion"};
    }
    context->Flush();
    if(FAILED(queue_->Wait(consumerFence_.Get(),ticket.producer))||
       FAILED(queue_->Signal(consumerFence_.Get(),ticket.copy))) {
        failed_=true;
        return Error{ErrorCode::DeviceRemoved,"Cannot signal FG shared-copy completion"};
    }
    return ticket;
}
bool FgSharedInputs::copyComplete(std::uint64_t value) const noexcept {
    return value!=0&&consumerFence_&&consumerFence_->GetCompletedValue()>=value;
}
bool FgSharedInputs::waitCopy(std::uint64_t value) const noexcept {
    if(!value||!consumerFence_)return false;
    if(copyComplete(value))return true;
    const auto event=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    if(!event)return false;
    const auto armed=consumerFence_->SetEventOnCompletion(value,event);
    const auto waited=SUCCEEDED(armed)?WaitForSingleObject(event,5000):WAIT_FAILED;
    CloseHandle(event);
    return waited==WAIT_OBJECT_0&&SUCCEEDED(d12_->GetDeviceRemovedReason());
}
}
