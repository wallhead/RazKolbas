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
Extent extentOf(ID3D11Resource* value) noexcept {
    ComPtr<ID3D11Texture2D> texture;
    if(!value||FAILED(value->QueryInterface(IID_PPV_ARGS(texture.GetAddressOf()))))return {};
    D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);
    return {desc.Width,desc.Height};
}
HRESULT createNativeAuxiliary(ID3D11Device* device,ID3D11RenderTargetView* source,
    Extent display,ComPtr<ID3D11RenderTargetView>& result) noexcept {
    if(!device||!source||!display.valid())return E_INVALIDARG;
    auto sourceResource=resource(source);
    ComPtr<ID3D11Texture2D> sourceTexture;
    if(!sourceResource||FAILED(sourceResource.As(&sourceTexture)))return E_INVALIDARG;
    D3D11_TEXTURE2D_DESC textureDesc{};sourceTexture->GetDesc(&textureDesc);
    D3D11_RENDER_TARGET_VIEW_DESC viewDesc{};source->GetDesc(&viewDesc);
    if(textureDesc.ArraySize!=1||textureDesc.MipLevels!=1||
       textureDesc.SampleDesc.Count!=1||
       viewDesc.ViewDimension!=D3D11_RTV_DIMENSION_TEXTURE2D||
       viewDesc.Texture2D.MipSlice!=0)return E_NOTIMPL;
    textureDesc.Width=display.width;textureDesc.Height=display.height;
    textureDesc.Usage=D3D11_USAGE_DEFAULT;textureDesc.CPUAccessFlags=0;
    textureDesc.BindFlags=D3D11_BIND_RENDER_TARGET;
    textureDesc.MiscFlags=0;
    ComPtr<ID3D11Texture2D> texture;
    auto hr=device->CreateTexture2D(&textureDesc,nullptr,&texture);
    if(FAILED(hr))return hr;
    return device->CreateRenderTargetView(texture.Get(),&viewDesc,&result);
}
bool canPrepareNativeSampledAuxiliary(ID3D11RenderTargetView* source,
    Extent render) noexcept {
    if(!source||!render.valid())return false;
    auto value=resource(source);
    ComPtr<ID3D11Texture2D> texture;
    if(!value||FAILED(value.As(&texture)))return false;
    D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);
    D3D11_RENDER_TARGET_VIEW_DESC viewDesc{};source->GetDesc(&viewDesc);
    return desc.Width==render.width&&desc.Height==render.height&&
        desc.ArraySize==1&&desc.MipLevels==1&&desc.SampleDesc.Count==1&&
        desc.Usage==D3D11_USAGE_DEFAULT&&
        desc.Format==DXGI_FORMAT_R16G16_FLOAT&&
        (desc.BindFlags&(D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE))==
            (D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE)&&
        viewDesc.ViewDimension==D3D11_RTV_DIMENSION_TEXTURE2D&&
        viewDesc.Texture2D.MipSlice==0;
}
bool supportedSampledAuxiliaryView(ID3D11RenderTargetView* target,
    ID3D11ShaderResourceView* sample) noexcept {
    if(!target||!sample||
       !sameObject(resource(target).Get(),resource(sample).Get()))return false;
    D3D11_SHADER_RESOURCE_VIEW_DESC desc{};sample->GetDesc(&desc);
    return desc.Format==DXGI_FORMAT_R16G16_FLOAT&&
        desc.ViewDimension==D3D11_SRV_DIMENSION_TEXTURE2D&&
        desc.Texture2D.MostDetailedMip==0&&desc.Texture2D.MipLevels==1;
}
HRESULT createNativeSampledAuxiliary(ID3D11Device* device,
    ID3D11RenderTargetView* source,ID3D11ShaderResourceView* sample,
    Extent render,Extent display,ComPtr<ID3D11RenderTargetView>& target,
    ComPtr<ID3D11ShaderResourceView>& sampled) noexcept {
    if(!device||!display.valid()||
       !canPrepareNativeSampledAuxiliary(source,render)||
       !supportedSampledAuxiliaryView(source,sample))return E_INVALIDARG;
    auto sourceResource=resource(source);
    ComPtr<ID3D11Texture2D> sourceTexture;
    if(FAILED(sourceResource.As(&sourceTexture)))return E_INVALIDARG;
    D3D11_TEXTURE2D_DESC textureDesc{};sourceTexture->GetDesc(&textureDesc);
    D3D11_RENDER_TARGET_VIEW_DESC targetDesc{};source->GetDesc(&targetDesc);
    D3D11_SHADER_RESOURCE_VIEW_DESC sampleDesc{};sample->GetDesc(&sampleDesc);
    textureDesc.Width=display.width;textureDesc.Height=display.height;
    textureDesc.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
    textureDesc.CPUAccessFlags=0;textureDesc.MiscFlags=0;
    ComPtr<ID3D11Texture2D> texture;
    auto hr=device->CreateTexture2D(&textureDesc,nullptr,&texture);
    if(FAILED(hr))return hr;
    ComPtr<ID3D11RenderTargetView> targetView;
    hr=device->CreateRenderTargetView(texture.Get(),&targetDesc,&targetView);
    if(FAILED(hr))return hr;
    ComPtr<ID3D11ShaderResourceView> sampledView;
    hr=device->CreateShaderResourceView(texture.Get(),&sampleDesc,&sampledView);
    if(FAILED(hr))return hr;
    target=std::move(targetView);sampled=std::move(sampledView);
    return S_OK;
}
HRESULT createNativeDepth(ID3D11Device* device,ID3D11DepthStencilView* source,
    Extent display,ComPtr<ID3D11DepthStencilView>& result) noexcept {
    if(!device||!source||!display.valid())return E_INVALIDARG;
    auto sourceResource=resource(source);
    ComPtr<ID3D11Texture2D> sourceTexture;
    if(!sourceResource||FAILED(sourceResource.As(&sourceTexture)))return E_INVALIDARG;
    D3D11_TEXTURE2D_DESC textureDesc{};sourceTexture->GetDesc(&textureDesc);
    D3D11_DEPTH_STENCIL_VIEW_DESC viewDesc{};source->GetDesc(&viewDesc);
    if(textureDesc.ArraySize!=1||textureDesc.MipLevels!=1||
       textureDesc.SampleDesc.Count!=1||
       viewDesc.ViewDimension!=D3D11_DSV_DIMENSION_TEXTURE2D||
       viewDesc.Texture2D.MipSlice!=0)return E_NOTIMPL;
    textureDesc.Width=display.width;textureDesc.Height=display.height;
    textureDesc.Usage=D3D11_USAGE_DEFAULT;textureDesc.CPUAccessFlags=0;
    textureDesc.BindFlags=D3D11_BIND_DEPTH_STENCIL;
    textureDesc.MiscFlags=0;
    // The owned late-UI attachment is cleared and written independently of
    // the observed source view. Do not inherit READ_ONLY_DEPTH/STENCIL.
    viewDesc.Flags=0;
    ComPtr<ID3D11Texture2D> texture;
    auto hr=device->CreateTexture2D(&textureDesc,nullptr,&texture);
    if(FAILED(hr))return hr;
    return device->CreateDepthStencilView(texture.Get(),&viewDesc,&result);
}
HRESULT createNativeSampledDepth(ID3D11Device* device,
    ID3D11DepthStencilView* depthSource,ID3D11ShaderResourceView* shaderSource,
    Extent display,ComPtr<ID3D11DepthStencilView>& clearView,
    ComPtr<ID3D11ShaderResourceView>& sampledView) noexcept {
    if(!device||!depthSource||!shaderSource||!display.valid())return E_INVALIDARG;
    auto depthResource=resource(depthSource);
    auto shaderResource=resource(shaderSource);
    if(!sameObject(depthResource.Get(),shaderResource.Get()))return E_INVALIDARG;
    ComPtr<ID3D11Texture2D> sourceTexture;
    if(!depthResource||FAILED(depthResource.As(&sourceTexture)))return E_INVALIDARG;
    D3D11_TEXTURE2D_DESC textureDesc{};sourceTexture->GetDesc(&textureDesc);
    D3D11_DEPTH_STENCIL_VIEW_DESC depthDesc{};depthSource->GetDesc(&depthDesc);
    D3D11_SHADER_RESOURCE_VIEW_DESC shaderDesc{};shaderSource->GetDesc(&shaderDesc);
    if(textureDesc.ArraySize!=1||textureDesc.MipLevels!=1||
       textureDesc.SampleDesc.Count!=1||
       depthDesc.ViewDimension!=D3D11_DSV_DIMENSION_TEXTURE2D||
       depthDesc.Texture2D.MipSlice!=0||
       shaderDesc.ViewDimension!=D3D11_SRV_DIMENSION_TEXTURE2D||
       shaderDesc.Texture2D.MostDetailedMip!=0||
       shaderDesc.Texture2D.MipLevels!=1)return E_NOTIMPL;
    textureDesc.Width=display.width;textureDesc.Height=display.height;
    textureDesc.Usage=D3D11_USAGE_DEFAULT;textureDesc.CPUAccessFlags=0;
    textureDesc.BindFlags=D3D11_BIND_DEPTH_STENCIL|D3D11_BIND_SHADER_RESOURCE;
    textureDesc.MiscFlags=0;
    // This view exists only to initialize the separate sampled depth texture;
    // it must remain writable even when the observed source DSV is read-only.
    depthDesc.Flags=0;
    ComPtr<ID3D11Texture2D> texture;
    auto hr=device->CreateTexture2D(&textureDesc,nullptr,&texture);
    if(FAILED(hr))return hr;
    hr=device->CreateDepthStencilView(texture.Get(),&depthDesc,&clearView);
    if(FAILED(hr))return hr;
    return device->CreateShaderResourceView(texture.Get(),&shaderDesc,&sampledView);
}
bool hasStencil(DXGI_FORMAT format) noexcept {
    return format==DXGI_FORMAT_D24_UNORM_S8_UINT||
        format==DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
}
}
bool matchesNativeUiObservation(const UiFrameObservation& observation,
    Extent render,std::uintptr_t sceneId,UiObservationLayout layout) noexcept {
    const auto count=layout==UiObservationLayout::FourPairsThenSceneBind?9u:8u;
    if(!render.valid()||!sceneId||observation.dropped||
       observation.count!=count)return false;
    const auto isRender=[render](Extent value) {
        return value.width==render.width&&value.height==render.height;
    };
    std::uintptr_t depth{};
    for(std::uint32_t i=0;i<8;++i) {
        const auto& event=observation.events[i];
        if(i%2) {
            if(event.kind!=UiObservationKind::Viewport||
               !isRender(event.viewport))return false;
            continue;
        }
        if(event.kind!=UiObservationKind::RenderTargets||event.sceneSlot!=0||
           !event.hasDepth||!isRender(event.depth)||!isRender(event.targets[0])||
           event.targetIdentities[0]!=sceneId)return false;
        if(!depth)depth=event.depthIdentity;
        if(!depth||event.depthIdentity!=depth)return false;
        if(i==0) {
            if(event.targetCount!=2||!isRender(event.targets[1])||
               !event.targetIdentities[1]||
               event.targetIdentities[1]==event.targetIdentities[0])return false;
        } else if(event.targetCount!=1)return false;
    }
    if(layout==UiObservationLayout::FourPairsThenSceneBind) {
        const auto& final=observation.events[8];
        if(final.kind!=UiObservationKind::RenderTargets||final.targetCount!=1||
           final.sceneSlot!=0||!final.hasDepth||!isRender(final.depth)||
           !isRender(final.targets[0])||final.targetIdentities[0]!=sceneId||
           final.depthIdentity!=depth)return false;
    }
    return true;
}
HRESULT NativeUiRedirector::configure(ID3D11DeviceContext* context,DWORD renderThread,
    UiContextNext next,ID3D11Texture2D* reducedScene,
    ID3D11RenderTargetView* nativeRtv,UiObservationLayout layout) noexcept {
    if(context_||!context||!renderThread||!next.om||!next.viewport||
       !next.scissor||!next.ps||
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
    generation_=route_.plan().generation;compatibilityFault_=false;faultInfo_={};
    latePassRoutingDisabled_=latePassPermanentlyDisabled_=false;
    latePassFaults_=0;latePassFaultFrame_=0;learnedSampledAuxiliaryOnFault_=false;
    observationLayout_=layout;
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
void NativeUiRedirector::bindNativeTarget(bool bindUiDepth) noexcept {
    auto* view=nativeRtv_.Get();
    auto* depth=bindUiDepth&&companionsReady()?nativeDepthView_.Get():nullptr;
    next_.om(context_.Get(),1,&view,depth);
    const auto display=route_.plan().display;
    const D3D11_VIEWPORT viewport{0,0,static_cast<float>(display.width),
        static_cast<float>(display.height),0,1};
    next_.viewport(context_.Get(),1,&viewport);
    const D3D11_RECT scissor{0,0,static_cast<LONG>(display.width),
        static_cast<LONG>(display.height)};
    next_.scissor(context_.Get(),1,&scissor);
}
HRESULT NativeUiRedirector::bindNativeForProcessing(std::uint64_t frame) noexcept {
    const auto owner=route_.renderThread()?route_.renderThread():thread_;
    if(!context_||!nativeRtv_||generation_!=route_.plan().generation||
       route_.phase()!=ScenePhase::Processing||route_.frame()!=frame||
       GetCurrentThreadId()!=owner)return E_UNEXPECTED;
    bindNativeTarget(false);
    return S_OK;
}
HRESULT NativeUiRedirector::commitPublishedUi(std::uint64_t frame) noexcept {
    const auto owner=route_.renderThread()?route_.renderThread():thread_;
    if(!context_||GetCurrentThreadId()!=owner||generation_!=route_.plan().generation||
       !route_.enterUi(frame,generation_,true))return E_UNEXPECTED;
    if(companionsReady()) {
        constexpr float clear[4]{};
        for(const auto& auxiliary:auxiliaries_)
            if(auxiliary.nativeView)
                context_->ClearRenderTargetView(auxiliary.nativeView.Get(),clear);
        D3D11_DEPTH_STENCIL_VIEW_DESC desc{};nativeDepthView_->GetDesc(&desc);
        const auto flags=D3D11_CLEAR_DEPTH|
            (hasStencil(desc.Format)?D3D11_CLEAR_STENCIL:0u);
        context_->ClearDepthStencilView(nativeDepthView_.Get(),flags,1.0f,0);
    }
    // Scaleform records display commands in each IMenu::PostDisplay call and
    // submits them later from GRenderer::EndFrame. Its vector masks require a
    // stencil attachment at that deferred boundary, so leave the prepared
    // display-sized depth/stencil view bound with the native colour target.
    bindNativeTarget(true);
    return S_OK;
}
HRESULT NativeUiRedirector::rebindForDeferredUiFlush(std::uint64_t frame) noexcept {
    const auto owner=route_.renderThread()?route_.renderThread():thread_;
    if(!context_||!nativeRtv_||!companionsReady()||
       GetCurrentThreadId()!=owner||generation_!=route_.plan().generation||
       route_.phase()!=ScenePhase::NativeUi||route_.frame()!=frame)return E_UNEXPECTED;
    std::array<ID3D11RenderTargetView*,D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT>
        rawTargets{};
    context_->OMGetRenderTargets(static_cast<UINT>(rawTargets.size()),
        rawTargets.data(),nullptr);
    std::array<ComPtr<ID3D11RenderTargetView>,D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT>
        targets;
    UINT count{};
    for(UINT i=0;i<rawTargets.size();++i) {
        targets[i].Attach(rawTargets[i]);
        if(rawTargets[i])count=i+1;
    }
    if(!count)return E_UNEXPECTED;
    auto boundResource=resource(targets[0].Get());
    auto boundId=canonical(boundResource.Get());
    if(!boundId||boundId.Get()!=nativeId_.Get())return E_UNEXPECTED;
    next_.om(context_.Get(),count,rawTargets.data(),nativeDepthView_.Get());
    return S_OK;
}
bool NativeUiRedirector::beginObservation(std::uint64_t frame) noexcept {
    const auto owner=route_.renderThread()?route_.renderThread():thread_;
    if(!context_||!frame||GetCurrentThreadId()!=owner||
       route_.phase()!=ScenePhase::World||route_.frame()!=frame)return false;
    if(observing_)return observation_.frame==frame;
    observation_={};observation_.frame=frame;
    observing_=true;observeViewport_=false;
    return true;
}
std::optional<UiFrameObservation> NativeUiRedirector::finishObservation(
    std::uint64_t frame) noexcept {
    const auto owner=route_.renderThread()?route_.renderThread():thread_;
    if(!observing_||observation_.frame!=frame||GetCurrentThreadId()!=owner)
        return std::nullopt;
    observing_=false;observeViewport_=false;++completedObservations_;
    if(observationMatchesRoute())++validRouteObservations_;
    else observationContractFault_=true;
    return observation_;
}
bool NativeUiRedirector::observationMatchesRoute() const noexcept {
    return matchesNativeUiObservation(observation_,route_.plan().render,
        reinterpret_cast<std::uintptr_t>(sceneId_.Get()),observationLayout_);
}
void NativeUiRedirector::rememberObservedCompanions(UINT count,
    ID3D11RenderTargetView* const* views,ID3D11DepthStencilView* depth,
    int sceneSlot) noexcept {
    if(depth) {
        auto value=resource(depth);auto id=canonical(value.Get());
        if(id&&extentOf(value.Get()).width==route_.plan().render.width&&
           extentOf(value.Get()).height==route_.plan().render.height) {
            if(!depthSourceId_) {depthSourceId_=id;depthSourceView_=depth;}
            if(depthSourceId_.Get()==id.Get()) {
                if(count>1)observedMrtDepth_=true;
                if(count==1)observedSingleDepth_=true;
            }
        }
    }
    for(UINT i=0;i<count&&i<D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;++i) {
        if(static_cast<int>(i)==sceneSlot||!views[i])continue;
        auto value=resource(views[i]);auto id=canonical(value.Get());
        if(!id||extentOf(value.Get()).width!=route_.plan().render.width||
           extentOf(value.Get()).height!=route_.plan().render.height)continue;
        for(auto& auxiliary:auxiliaries_) {
            if(auxiliary.sourceId&&auxiliary.sourceId.Get()!=id.Get())continue;
            if(!auxiliary.sourceId) {
                auxiliary.sourceId=id;auxiliary.sourceView=views[i];
            }
            break;
        }
    }
}
HRESULT NativeUiRedirector::prepareObservedCompanions() noexcept {
    const auto owner=route_.renderThread()?route_.renderThread():thread_;
    if(!context_||GetCurrentThreadId()!=owner||
       generation_!=route_.plan().generation)return E_UNEXPECTED;
    ComPtr<ID3D11Device> device;context_->GetDevice(&device);
    if(!device)return E_UNEXPECTED;
    for(auto& auxiliary:auxiliaries_) {
        if(!auxiliary.sourceView||auxiliary.nativeView)continue;
        if(auxiliary.requiresSampledView&&!auxiliary.sourceShaderView)continue;
        const auto hr=auxiliary.requiresSampledView?
            createNativeSampledAuxiliary(device.Get(),auxiliary.sourceView.Get(),
                auxiliary.sourceShaderView.Get(),route_.plan().render,
                route_.plan().display,auxiliary.nativeView,
                auxiliary.nativeShaderView):
            createNativeAuxiliary(device.Get(),auxiliary.sourceView.Get(),
                route_.plan().display,auxiliary.nativeView);
        if(FAILED(hr))return hr;
    }
    if(depthSourceView_&&!nativeDepthView_) {
        const auto hr=createNativeDepth(device.Get(),depthSourceView_.Get(),
            route_.plan().display,nativeDepthView_);
        if(FAILED(hr))return hr;
    }
    if(depthSourceView_&&depthSourceShaderView_&&!nativeSampledDepthView_) {
        const auto hr=createNativeSampledDepth(device.Get(),depthSourceView_.Get(),
            depthSourceShaderView_.Get(),route_.plan().display,
            nativeSampledDepthClearView_,nativeSampledDepthView_);
        if(FAILED(hr))return hr;
        D3D11_DEPTH_STENCIL_VIEW_DESC desc{};
        nativeSampledDepthClearView_->GetDesc(&desc);
        const auto flags=D3D11_CLEAR_DEPTH|
            (hasStencil(desc.Format)?D3D11_CLEAR_STENCIL:0u);
        context_->ClearDepthStencilView(nativeSampledDepthClearView_.Get(),flags,1.0f,0);
    }
    return companionsReady()?S_OK:S_FALSE;
}
bool NativeUiRedirector::hasUnpreparedAuxiliary() const noexcept {
    for(const auto& auxiliary:auxiliaries_)
        if(auxiliary.sourceView&&(!auxiliary.nativeView||
           (auxiliary.requiresSampledView&&!auxiliary.nativeShaderView))&&
           (!auxiliary.requiresSampledView||auxiliary.sourceShaderView))return true;
    return false;
}
bool NativeUiRedirector::companionsReady() const noexcept {
    if(observationContractFault_||validRouteObservations_<2||
       !observedMrtDepth_||!observedSingleDepth_||
       !depthSourceId_||!depthSourceShaderView_||!nativeDepthView_||
       !nativeSampledDepthView_||!nativeSampledDepthClearView_)return false;
    unsigned auxiliaryCount{};
    for(const auto& item:auxiliaries_) {
        if(item.sourceId&&(!item.nativeView||
           (item.requiresSampledView&&
            (!item.sourceShaderView||!item.nativeShaderView))))return false;
        if(item.sourceId)++auxiliaryCount;
    }
    return auxiliaryCount>=2&&auxiliaryCount<=3;
}
void NativeUiRedirector::suspendLatePassRouting(std::uint64_t frame) noexcept {
    latePassRoutingDisabled_=true;
    learnedSampledAuxiliaryOnFault_=false;
    for(const auto& auxiliary:auxiliaries_)
        if(auxiliary.requiresSampledView&&auxiliary.sourceShaderView&&
           reinterpret_cast<std::uintptr_t>(auxiliary.sourceId.Get())==
               faultInfo_.unknownTargetId)
            learnedSampledAuxiliaryOnFault_=true;
    compatibilityFault_=false;faultInfo_={};
    latePassFaultFrame_=frame;
    if(++latePassFaults_>1)latePassPermanentlyDisabled_=true;
}
bool NativeUiRedirector::resumeLatePassRouting(std::uint64_t frame,
    bool sceneReady) noexcept {
    if(!latePassRoutingDisabled_||latePassPermanentlyDisabled_||
       route_.phase()!=ScenePhase::World||route_.frame()!=frame||
       generation_!=route_.plan().generation||
       frame<latePassFaultFrame_||
       (learnedSampledAuxiliaryOnFault_?
           frame-latePassFaultFrame_<2:
           (!sceneReady||frame-latePassFaultFrame_<120))||
       !companionsReady())return false;
    latePassRoutingDisabled_=false;
    learnedSampledAuxiliaryOnFault_=false;
    return true;
}
std::optional<UiDepthViewContract> NativeUiRedirector::depthViewContract() const noexcept {
    if(!depthSourceView_||!nativeDepthView_||!nativeSampledDepthClearView_)
        return std::nullopt;
    D3D11_DEPTH_STENCIL_VIEW_DESC source{},writable{},sampled{};
    depthSourceView_->GetDesc(&source);
    nativeDepthView_->GetDesc(&writable);
    nativeSampledDepthClearView_->GetDesc(&sampled);
    return UiDepthViewContract{source.Format,source.Flags,writable.Flags,sampled.Flags};
}
ID3D11RenderTargetView* NativeUiRedirector::auxiliaryReplacement(
    IUnknown* sourceId) const noexcept {
    if(!sourceId)return nullptr;
    for(const auto& auxiliary:auxiliaries_)
        if(auxiliary.sourceId.Get()==sourceId)return auxiliary.nativeView.Get();
    return nullptr;
}
bool NativeUiRedirector::eligible(ID3D11DeviceContext* context) const noexcept {
    const auto owner=route_.renderThread()?route_.renderThread():thread_;
    return context==context_.Get()&&GetCurrentThreadId()==owner&&
       route_.phase()==ScenePhase::NativeUi&&generation_==route_.plan().generation&&
       !latePassRoutingDisabled_;
}
bool NativeUiRedirector::nativeBound() const noexcept {
    ComPtr<ID3D11RenderTargetView> bound;
    context_->OMGetRenderTargets(1,bound.GetAddressOf(),nullptr);
    auto value=resource(bound.Get());auto id=canonical(value.Get());
    return id&&id.Get()==nativeId_.Get();
}
void NativeUiRedirector::onOMSetRenderTargets(ID3D11DeviceContext* context,
    UINT count,ID3D11RenderTargetView* const* views,ID3D11DepthStencilView* depth) noexcept {
    bool observedScene=false;
    int observedSlot=-1;
    std::array<ComPtr<IUnknown>,4> observedIds;
    std::array<Extent,4> observedExtents{};
    if(observing_&&context==context_.Get()&&views&&count&&
       GetCurrentThreadId()==(route_.renderThread()?route_.renderThread():thread_)) {
        for(UINT i=0;i<count&&i<observedIds.size();++i) {
            auto value=resource(views[i]);
            observedIds[i]=canonical(value.Get());
            observedExtents[i]=extentOf(value.Get());
            if(observedIds[i]&&observedIds[i].Get()==sceneId_.Get()) {
                observedScene=true;observedSlot=static_cast<int>(i);
            }
        }
        if(observedScene) {
            rememberObservedCompanions(count,views,depth,observedSlot);
            if(observation_.count<observation_.events.size()) {
                auto& event=observation_.events[observation_.count++];
                event.kind=UiObservationKind::RenderTargets;
                event.targetCount=count;event.sceneSlot=observedSlot;
                event.hasDepth=depth!=nullptr;event.targets=observedExtents;
                for(std::size_t i=0;i<observedIds.size();++i)
                    event.targetIdentities[i]=reinterpret_cast<std::uintptr_t>(
                        observedIds[i].Get());
                if(depth) {
                    auto value=resource(depth);auto id=canonical(value.Get());
                    event.depth=extentOf(value.Get());
                    event.depthIdentity=reinterpret_cast<std::uintptr_t>(id.Get());
                }
            } else ++observation_.dropped;
            observeViewport_=true;
        } else observeViewport_=false;
    }
    if(eligible(context)&&views&&count) {
        bool sceneIncoming=false;
        UINT sceneSlot=0;
        for(UINT i=0;i<count&&i<D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;++i) {
            auto value=resource(views[i]);auto id=canonical(value.Get());
            if(id&&id.Get()==sceneId_.Get()) {sceneIncoming=true;sceneSlot=i;}
        }
        if(sceneIncoming) {
            std::array<ID3D11RenderTargetView*,D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT>
                replacements{};
            bool compatible=count<=replacements.size();
            for(UINT i=0;compatible&&i<count;++i) {
                auto value=resource(views[i]);auto id=canonical(value.Get());
                if(id&&id.Get()==sceneId_.Get())replacements[i]=nativeRtv_.Get();
                else if(auto* auxiliary=auxiliaryReplacement(id.Get()))
                    replacements[i]=auxiliary;
                else if(!views[i]||extentOf(value.Get()).width!=route_.plan().render.width||
                        extentOf(value.Get()).height!=route_.plan().render.height)
                    replacements[i]=views[i];
                else compatible=false;
            }
            ID3D11DepthStencilView* replacementDepth=depth;
            if(depth) {
                auto value=resource(depth);auto id=canonical(value.Get());
                if(id&&id.Get()==depthSourceId_.Get()&&nativeDepthView_)
                    replacementDepth=nativeDepthView_.Get();
                else if(extentOf(value.Get()).width==route_.plan().render.width&&
                        extentOf(value.Get()).height==route_.plan().render.height)
                    compatible=false;
            }
            if(compatible) {
                next_.om(context,count,replacements.data(),replacementDepth);
                // A menu may set its reduced viewport while an offscreen target
                // is bound, then restore the cached scene target without setting
                // the viewport again. Keep the translated native target and its
                // effective viewport in the same coordinate space.
                UINT viewportCount=1;
                D3D11_VIEWPORT viewport{};
                context->RSGetViewports(&viewportCount,&viewport);
                if(viewportCount==1) {
                    auto width=viewport.Width,height=viewport.Height;
                    if(route_.remapFullUiViewport(width,height,
                        viewport.TopLeftX,viewport.TopLeftY,true)) {
                        viewport.Width=width;viewport.Height=height;
                        next_.viewport(context,1,&viewport);
                    }
                }
                return;
            }
            if(!compatibilityFault_) {
                faultInfo_={count,sceneSlot,depth!=nullptr,0,0};
                if(depth) {
                    auto value=resource(depth);auto id=canonical(value.Get());
                    faultInfo_.depthId=reinterpret_cast<std::uintptr_t>(id.Get());
                    faultInfo_.expectedDepthId=reinterpret_cast<std::uintptr_t>(
                        depthSourceId_.Get());
                    ComPtr<ID3D11Texture2D> texture;
                    if(value&&SUCCEEDED(value.As(&texture))) {
                        D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);
                        faultInfo_.depthWidth=desc.Width;
                        faultInfo_.depthHeight=desc.Height;
                    }
                }
                for(UINT i=0;i<count&&i<D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;++i) {
                    if(i==sceneSlot||!views[i])continue;
                    auto value=resource(views[i]);auto id=canonical(value.Get());
                    if(!id||auxiliaryReplacement(id.Get()))continue;
                    ComPtr<ID3D11Texture2D> texture;
                    if(!value||FAILED(value.As(&texture)))continue;
                    D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);
                    if(desc.Width!=route_.plan().render.width||
                       desc.Height!=route_.plan().render.height)continue;
                    faultInfo_.unknownTargetId=reinterpret_cast<std::uintptr_t>(id.Get());
                    faultInfo_.unknownTargetSlot=i;
                    faultInfo_.unknownTargetMips=desc.MipLevels;
                    faultInfo_.unknownTargetFormat=desc.Format;
                    faultInfo_.unknownTargetBindFlags=desc.BindFlags;
                    break;
                }
                // The inventory target is sampled later in the same frame.
                // Learn both its RTV and exact SRV before admitting a native
                // companion; redirecting only its writes hid the inventory.
                if(count==2&&sceneSlot==0&&
                   faultInfo_.unknownTargetSlot==1&&
                   faultInfo_.depthId&&
                   faultInfo_.depthId==faultInfo_.expectedDepthId&&
                   canPrepareNativeSampledAuxiliary(views[1],route_.plan().render)) {
                    rememberObservedCompanions(count,views,depth,sceneSlot);
                    for(auto& auxiliary:auxiliaries_)
                        if(reinterpret_cast<std::uintptr_t>(auxiliary.sourceId.Get())==
                           faultInfo_.unknownTargetId&&!auxiliary.nativeView)
                            auxiliary.requiresSampledView=true;
                }
            }
            compatibilityFault_=true; // Unknown MRT/depth semantics.
        }
    }
    next_.om(context,count,views,depth);
}
void NativeUiRedirector::onRSSetViewports(ID3D11DeviceContext* context,
    UINT count,const D3D11_VIEWPORT* views) noexcept {
    if(observing_&&observeViewport_&&context==context_.Get()&&count==1&&views&&
       GetCurrentThreadId()==(route_.renderThread()?route_.renderThread():thread_)) {
        if(observation_.count<observation_.events.size()) {
            auto& event=observation_.events[observation_.count++];
            event.kind=UiObservationKind::Viewport;
            event.viewport={static_cast<std::uint32_t>(views[0].Width),
                static_cast<std::uint32_t>(views[0].Height)};
        } else ++observation_.dropped;
        observeViewport_=false;
    }
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
void NativeUiRedirector::onRSSetScissorRects(ID3D11DeviceContext* context,
    UINT count,const D3D11_RECT* rects) noexcept {
    next_.scissor(context,count,rects);
}
void NativeUiRedirector::onPSSetShaderResources(ID3D11DeviceContext* context,
    UINT start,UINT count,ID3D11ShaderResourceView* const* views) noexcept {
    if(compatibilityFault_&&faultInfo_.unknownTargetId&&
       context==context_.Get()&&views) {
        for(UINT i=0;i<count&&i<D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT;++i) {
            if(!views[i])continue;
            auto value=resource(views[i]);auto id=canonical(value.Get());
            if(reinterpret_cast<std::uintptr_t>(id.Get())!=
               faultInfo_.unknownTargetId)continue;
            if(faultInfo_.unknownTargetSrvReads!=~0u)
                ++faultInfo_.unknownTargetSrvReads;
            if(faultInfo_.unknownTargetFirstSrvSlot==~0u)
                faultInfo_.unknownTargetFirstSrvSlot=start+i;
            for(auto& auxiliary:auxiliaries_)
                if(auxiliary.requiresSampledView&&
                   reinterpret_cast<std::uintptr_t>(auxiliary.sourceId.Get())==
                       faultInfo_.unknownTargetId&&
                   supportedSampledAuxiliaryView(auxiliary.sourceView.Get(),views[i])&&
                   !auxiliary.sourceShaderView)
                    auxiliary.sourceShaderView=views[i];
        }
    }
    if(observing_&&context==context_.Get()&&count==1&&views&&views[0]&&
       GetCurrentThreadId()==(route_.renderThread()?route_.renderThread():thread_)) {
        auto value=resource(views[0]);
        auto id=canonical(value.Get());
        if(id&&depthSourceId_&&id.Get()==depthSourceId_.Get()) {
            if(!depthSourceShaderView_)depthSourceShaderView_=views[0];
            ++observation_.sampledDepthReads;
            if(observation_.firstSampledDepthSlot==~0u)
                observation_.firstSampledDepthSlot=start;
        } else ++observation_.otherSingletonReads;
    }
    if(eligible(context)&&views&&count&&
       start<=D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT&&
       count<=D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT-start) {
        std::array<ID3D11ShaderResourceView*,
            D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT> replacements{};
        bool changed=false;
        for(UINT i=0;i<count;++i) {
            replacements[i]=views[i];
            if(!views[i])continue;
            auto value=resource(views[i]);auto id=canonical(value.Get());
            if(!id)continue;
            if(count==1&&depthSourceId_&&id.Get()==depthSourceId_.Get()&&
               nativeSampledDepthView_) {
                replacements[i]=nativeSampledDepthView_.Get();changed=true;
                continue;
            }
            for(const auto& auxiliary:auxiliaries_)
                if(auxiliary.requiresSampledView&&auxiliary.nativeShaderView&&
                   id.Get()==auxiliary.sourceId.Get()&&
                   supportedSampledAuxiliaryView(auxiliary.sourceView.Get(),views[i])) {
                    replacements[i]=auxiliary.nativeShaderView.Get();
                    changed=true;break;
                }
        }
        if(changed) {next_.ps(context,start,count,replacements.data());return;}
    }
    next_.ps(context,start,count,views);
}
void NativeUiRedirector::releaseAfterRetirement(bool unbindNative) noexcept {
    if(unbindNative&&context_&&nativeRtv_&&next_.om&&nativeBound())
        next_.om(context_.Get(),0,nullptr,nullptr);
    scene_.Reset();sceneId_.Reset();nativeId_.Reset();nativeRtv_.Reset();context_.Reset();
    thread_=0;generation_=0;next_={};compatibilityFault_=false;faultInfo_={};
    latePassRoutingDisabled_=latePassPermanentlyDisabled_=false;
    latePassFaults_=0;latePassFaultFrame_=0;learnedSampledAuxiliaryOnFault_=false;
    observationLayout_=UiObservationLayout::FourPairs;
    observation_={};observing_=observeViewport_=false;completedObservations_=0;
    validRouteObservations_=0;observationContractFault_=false;
    observedMrtDepth_=observedSingleDepth_=false;
    for(auto& auxiliary:auxiliaries_)auxiliary={};
    depthSourceId_.Reset();depthSourceView_.Reset();nativeDepthView_.Reset();
    depthSourceShaderView_.Reset();nativeSampledDepthView_.Reset();
    nativeSampledDepthClearView_.Reset();
}
}
