#include "rk/NativeUiRedirector.hpp"

namespace rk {
namespace {
using Microsoft::WRL::ComPtr;
ComPtr<IUnknown> canonical(IUnknown* object) noexcept {
    ComPtr<IUnknown> id;
    if(object)object->QueryInterface(IID_PPV_ARGS(id.GetAddressOf()));
    return id;
}
ComPtr<ID3D11Resource> resource(ID3D11View* view) noexcept {
    ComPtr<ID3D11Resource> value;
    if(view)view->GetResource(value.GetAddressOf());
    return value;
}
bool sameObject(IUnknown* first,IUnknown* second) noexcept {
    auto a=canonical(first),b=canonical(second);
    return a&&b&&a.Get()==b.Get();
}
bool extent(ID3D11Resource* value,Extent expected) noexcept {
    ComPtr<ID3D11Texture2D> texture;
    if(!value||FAILED(value->QueryInterface(IID_PPV_ARGS(texture.GetAddressOf()))))return false;
    D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);
    return desc.Width==expected.width&&desc.Height==expected.height&&
        desc.ArraySize==1&&desc.MipLevels==1&&desc.SampleDesc.Count==1;
}
}
HRESULT NativeUiRedirector::configure(ID3D11DeviceContext* context,DWORD renderThread,
    UiContextNext next,ID3D11Texture2D* reducedScene,
    ID3D11RenderTargetView* nativeRtv) noexcept {
    if(context_||!context||!renderThread||!next.om||!next.viewport||
       !reducedScene||!nativeRtv||!route_.plan().valid()||
       context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return E_INVALIDARG;
    auto nativeColor=resource(nativeRtv);
    if(!extent(reducedScene,route_.plan().render)||
       !extent(nativeColor.Get(),route_.plan().display)||
       sameObject(reducedScene,nativeColor.Get()))return E_INVALIDARG;
    D3D11_RENDER_TARGET_VIEW_DESC viewDesc{};nativeRtv->GetDesc(&viewDesc);
    if(viewDesc.ViewDimension!=D3D11_RTV_DIMENSION_TEXTURE2D||
       viewDesc.Texture2D.MipSlice!=0)return E_INVALIDARG;
    ComPtr<ID3D11Device> device,sceneOwner,nativeOwner;
    context->GetDevice(device.GetAddressOf());
    reducedScene->GetDevice(sceneOwner.GetAddressOf());
    nativeColor->GetDevice(nativeOwner.GetAddressOf());
    if(!sameObject(device.Get(),sceneOwner.Get())||
       !sameObject(device.Get(),nativeOwner.Get()))return E_INVALIDARG;
    context_=context;thread_=renderThread;next_=next;
    scene_=reducedScene;sceneId_=canonical(reducedScene);
    nativeId_=canonical(nativeColor.Get());nativeRtv_=nativeRtv;
    generation_=route_.plan().generation;compatibilityFault_=false;
    return S_OK;
}
HRESULT NativeUiRedirector::replaceNativeTarget(ID3D11RenderTargetView* nativeRtv) noexcept {
    const auto owner=route_.renderThread()?route_.renderThread():thread_;
    if(!context_||!nativeRtv||generation_!=route_.plan().generation||
       route_.phase()!=ScenePhase::Processing||GetCurrentThreadId()!=owner)
        return E_UNEXPECTED;
    auto nativeColor=resource(nativeRtv);
    if(!extent(nativeColor.Get(),route_.plan().display)||
       sameObject(scene_.Get(),nativeColor.Get()))return E_INVALIDARG;
    D3D11_RENDER_TARGET_VIEW_DESC viewDesc{};nativeRtv->GetDesc(&viewDesc);
    if(viewDesc.ViewDimension!=D3D11_RTV_DIMENSION_TEXTURE2D||
       viewDesc.Texture2D.MipSlice!=0)return E_INVALIDARG;
    ComPtr<ID3D11Device> device,nativeOwner;
    context_->GetDevice(device.GetAddressOf());
    nativeColor->GetDevice(nativeOwner.GetAddressOf());
    if(!sameObject(device.Get(),nativeOwner.Get()))return E_INVALIDARG;
    nativeId_=canonical(nativeColor.Get());nativeRtv_=nativeRtv;
    return S_OK;
}
void NativeUiRedirector::bindNativeTarget() noexcept {
    auto* view=nativeRtv_.Get();next_.om(context_.Get(),1,&view,nullptr);
    const auto display=route_.plan().display;
    const D3D11_VIEWPORT viewport{0,0,static_cast<float>(display.width),
        static_cast<float>(display.height),0,1};
    next_.viewport(context_.Get(),1,&viewport);
    const D3D11_RECT scissor{0,0,static_cast<LONG>(display.width),
        static_cast<LONG>(display.height)};
    context_->RSSetScissorRects(1,&scissor);
}
HRESULT NativeUiRedirector::bindNativeForProcessing(std::uint64_t frame) noexcept {
    const auto owner=route_.renderThread()?route_.renderThread():thread_;
    if(!context_||!nativeRtv_||generation_!=route_.plan().generation||
       route_.phase()!=ScenePhase::Processing||route_.frame()!=frame||
       GetCurrentThreadId()!=owner)return E_UNEXPECTED;
    bindNativeTarget();
    return S_OK;
}
HRESULT NativeUiRedirector::commitPublishedUi(std::uint64_t frame) noexcept {
    const auto owner=route_.renderThread()?route_.renderThread():thread_;
    if(!context_||GetCurrentThreadId()!=owner||generation_!=route_.plan().generation||
       !route_.enterUi(frame,generation_,true))return E_UNEXPECTED;
    bindNativeTarget();
    return S_OK;
}
bool NativeUiRedirector::eligible(ID3D11DeviceContext* context) const noexcept {
    const auto owner=route_.renderThread()?route_.renderThread():thread_;
    return context==context_.Get()&&GetCurrentThreadId()==owner&&
       route_.phase()==ScenePhase::NativeUi&&generation_==route_.plan().generation;
}
bool NativeUiRedirector::nativeBound() const noexcept {
    ComPtr<ID3D11RenderTargetView> bound;
    context_->OMGetRenderTargets(1,bound.GetAddressOf(),nullptr);
    auto value=resource(bound.Get());auto id=canonical(value.Get());
    return id&&id.Get()==nativeId_.Get();
}
void NativeUiRedirector::onOMSetRenderTargets(ID3D11DeviceContext* context,
    UINT count,ID3D11RenderTargetView* const* views,ID3D11DepthStencilView* depth) noexcept {
    if(eligible(context)&&views&&count) {
        bool sceneIncoming=false;
        for(UINT i=0;i<count&&i<D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;++i) {
            auto value=resource(views[i]);auto id=canonical(value.Get());
            sceneIncoming|=id&&id.Get()==sceneId_.Get();
        }
        if(sceneIncoming) {
            if(count==1&&!depth) {
                auto* replacement=nativeRtv_.Get();
                next_.om(context,1,&replacement,nullptr);return;
            }
            compatibilityFault_=true; // Unknown MRT/depth semantics.
        }
    }
    next_.om(context,count,views,depth);
}
void NativeUiRedirector::onRSSetViewports(ID3D11DeviceContext* context,
    UINT count,const D3D11_VIEWPORT* views) noexcept {
    if(eligible(context)&&count==1&&views&&nativeBound()) {
        auto width=views[0].Width,height=views[0].Height;
        if(route_.remapFullUiViewport(width,height,views[0].TopLeftX,
            views[0].TopLeftY,true)) {
            auto mapped=views[0];mapped.Width=width;mapped.Height=height;
            next_.viewport(context,1,&mapped);return;
        }
    }
    next_.viewport(context,count,views);
}
void NativeUiRedirector::releaseAfterRetirement(bool unbindNative) noexcept {
    if(unbindNative&&context_&&nativeRtv_&&next_.om&&nativeBound())
        next_.om(context_.Get(),0,nullptr,nullptr);
    scene_.Reset();sceneId_.Reset();nativeId_.Reset();nativeRtv_.Reset();context_.Reset();
    thread_=0;generation_=0;next_={};compatibilityFault_=false;
}
}
