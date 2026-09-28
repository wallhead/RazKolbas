#include "rk/FgUiPlanes.hpp"
#include <utility>

namespace rk {
namespace {
using Microsoft::WRL::ComPtr;

bool sameObject(IUnknown* first,IUnknown* second) noexcept {
    ComPtr<IUnknown> a,b;
    return first&&second&&
        SUCCEEDED(first->QueryInterface(IID_PPV_ARGS(&a)))&&
        SUCCEEDED(second->QueryInterface(IID_PPV_ARGS(&b)))&&
        a.Get()==b.Get();
}
ComPtr<IUnknown> identity(IUnknown* object) noexcept {
    ComPtr<IUnknown> result;
    if(object)object->QueryInterface(IID_PPV_ARGS(&result));
    return result;
}
bool alphaFormat(DXGI_FORMAT format) noexcept {
    switch(format) {
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
    case DXGI_FORMAT_R10G10B10A2_UNORM:
        return true;
    default:return false;
    }
}
bool frameMatches(const FgUiPlaneFrame& stored,
    const FgSourceFrame& frame) noexcept {
    return stored.source==frame.source&&
        stored.generation==frame.generation&&
        stored.presentToken==frame.presentToken&&
        stored.resetEpoch==frame.resetEpoch&&
        stored.display.width==frame.display.width&&
        stored.display.height==frame.display.height;
}
bool validSource(ID3D11Device* device,ID3D11Texture2D* source,
    Extent expected,bool requireAlpha) noexcept {
    if(!device||!source||!expected.valid())return false;
    ComPtr<ID3D11Device> owner;
    source->GetDevice(&owner);
    if(!sameObject(device,owner.Get()))return false;
    D3D11_TEXTURE2D_DESC desc{};
    source->GetDesc(&desc);
    return desc.Width==expected.width&&desc.Height==expected.height&&
        desc.MipLevels==1&&desc.ArraySize==1&&
        desc.SampleDesc.Count==1&&desc.SampleDesc.Quality==0&&
        desc.Usage==D3D11_USAGE_DEFAULT&&
        desc.Format!=DXGI_FORMAT_UNKNOWN&&
        (!requireAlpha||alphaFormat(desc.Format));
}
Result<ComPtr<ID3D11Texture2D>> snapshot(ID3D11Device* device,
    ID3D11DeviceContext* context,ID3D11Texture2D* source) {
    D3D11_TEXTURE2D_DESC desc{};
    source->GetDesc(&desc);
    desc.Usage=D3D11_USAGE_DEFAULT;
    desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags=0;desc.MiscFlags=0;
    ComPtr<ID3D11Texture2D> copy;
    const auto created=device->CreateTexture2D(&desc,nullptr,&copy);
    if(FAILED(created))return Error{ErrorCode::Unavailable,
        "FG display-plane snapshot allocation failed"};
    context->CopyResource(copy.Get(),source);
    return copy;
}
}

FgUiPlanes::FgUiPlanes(ID3D11Device* device,
    std::uint64_t generation) noexcept : device_(device),generation_(generation) {}

Result<bool> FgUiPlanes::captureBeforeUi(const FgSourceFrame& frame,
    ID3D11DeviceContext* context,ID3D11Texture2D* displayBeforeUi) {
    if(pendingValid_)return Error{ErrorCode::Conflict,
        "Previous FG pre-UI snapshot has not finished"};
    ComPtr<ID3D11Device> contextOwner;
    if(context)context->GetDevice(&contextOwner);
    if(!frame.source||!frame.presentToken||!frame.resetEpoch||
       frame.generation!=generation_||!frame.display.valid()||
       !context||context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE||
       !sameObject(device_.Get(),contextOwner.Get())||
       !validSource(device_.Get(),displayBeforeUi,frame.display,false))
        return Error{ErrorCode::InvalidInput,
            "FG pre-UI frame or display source is unavailable"};
    auto copy=snapshot(device_.Get(),context,displayBeforeUi);
    if(const auto* error=std::get_if<Error>(&copy))return *error;
    pending_={};
    pending_.source=frame.source;
    pending_.generation=frame.generation;
    pending_.presentToken=frame.presentToken;
    pending_.resetEpoch=frame.resetEpoch;
    pending_.display=frame.display;
    pending_.hudlessStamp={frame.source,frame.generation,
        frame.display,true,frame.resetEpoch};
    pending_.hudless=std::move(std::get<ComPtr<ID3D11Texture2D>>(copy));
    preUiSource_=identity(displayBeforeUi);
    pendingValid_=true;
    return true;
}

Result<FgUiPlaneFrame> FgUiPlanes::finish(const FgSourceFrame& frame,
    ID3D11DeviceContext* context,ID3D11Texture2D* uiColorAlpha,
    ID3D11Texture2D* finalColor,D3D11_RECT uiRegion) {
    if(!pendingValid_)return Error{ErrorCode::Conflict,
        "No FG pre-UI snapshot exists for this frame"};
    auto prepared=std::move(pending_);
    auto preUiSource=std::move(preUiSource_);
    pending_={};pendingValid_=false;
    ComPtr<ID3D11Device> contextOwner;
    if(context)context->GetDevice(&contextOwner);
    if(!frameMatches(prepared,frame)||!context||
       context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE||
       !sameObject(device_.Get(),contextOwner.Get())||
       !validSource(device_.Get(),uiColorAlpha,frame.display,true)||
       !validSource(device_.Get(),finalColor,frame.display,false)||
       sameObject(uiColorAlpha,preUiSource.Get())||
       sameObject(uiColorAlpha,finalColor)||
       uiRegion.left<0||uiRegion.top<0||
       uiRegion.left>=uiRegion.right||uiRegion.top>=uiRegion.bottom||
       uiRegion.right>static_cast<LONG>(frame.display.width)||
       uiRegion.bottom>static_cast<LONG>(frame.display.height))
        return Error{ErrorCode::InvalidInput,
            "FG UI plane is absent, aliased, stale or outside display"};
    auto uiCopy=snapshot(device_.Get(),context,uiColorAlpha);
    if(const auto* error=std::get_if<Error>(&uiCopy))return *error;
    auto finalCopy=snapshot(device_.Get(),context,finalColor);
    if(const auto* error=std::get_if<Error>(&finalCopy))return *error;
    prepared.uiColorAlpha=std::move(std::get<ComPtr<ID3D11Texture2D>>(uiCopy));
    prepared.finalColor=std::move(std::get<ComPtr<ID3D11Texture2D>>(finalCopy));
    prepared.uiRegion=uiRegion;
    prepared.uiStamp={frame.source,frame.generation,
        frame.display,true,frame.resetEpoch};
    prepared.finalStamp={frame.source,frame.generation,
        frame.display,true,frame.resetEpoch};
    return prepared;
}

void FgUiPlanes::discard() noexcept {
    pending_={};preUiSource_.Reset();pendingValid_=false;
}
bool FgUiPlanes::advanceGeneration(std::uint64_t next) noexcept {
    if(pendingValid_||!next||next<=generation_)return false;
    generation_=next;
    return true;
}
}
