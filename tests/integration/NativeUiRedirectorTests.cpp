#include <catch2/catch_test_macros.hpp>
#include "rk/NativeUiRedirector.hpp"
#include "rk/ReducedSdrSurface.hpp"
#include <wrl/client.h>

namespace {
using Microsoft::WRL::ComPtr;
void STDMETHODCALLTYPE forwardOm(ID3D11DeviceContext* context,UINT count,
    ID3D11RenderTargetView* const* views,ID3D11DepthStencilView* depth) {
    context->OMSetRenderTargets(count,views,depth);
}
void STDMETHODCALLTYPE forwardVp(ID3D11DeviceContext* context,UINT count,
    const D3D11_VIEWPORT* views) { context->RSSetViewports(count,views); }
void STDMETHODCALLTYPE forwardScissor(ID3D11DeviceContext* context,UINT count,
    const D3D11_RECT* rects) { context->RSSetScissorRects(count,rects); }
void STDMETHODCALLTYPE forwardPs(ID3D11DeviceContext* context,UINT start,UINT count,
    ID3D11ShaderResourceView* const* views) {
    context->PSSetShaderResources(start,count,views);
}
ComPtr<IUnknown> identity(IUnknown* object) {
    ComPtr<IUnknown> result;
    if(object)object->QueryInterface(IID_PPV_ARGS(result.GetAddressOf()));
    return result;
}
ComPtr<ID3D11Resource> viewResource(ID3D11View* view) {
    ComPtr<ID3D11Resource> result;
    if(view)view->GetResource(result.GetAddressOf());
    return result;
}
rk::Extent viewExtent(ID3D11View* view) {
    ComPtr<ID3D11Texture2D> texture;
    if(auto value=viewResource(view);value)value.As(&texture);
    D3D11_TEXTURE2D_DESC desc{};
    if(texture)texture->GetDesc(&desc);
    return {desc.Width,desc.Height};
}
}

TEST_CASE("WARP cached reduced RTV and viewport bind routes native UI after publication", "[native_ui]") {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL level{};
    REQUIRE(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,
        nullptr,0,D3D11_SDK_VERSION,&device,&level,&context)));
    constexpr rk::Extent render{32,16},display{64,32};
    auto sceneResult=rk::createReducedSdrSurface(device.Get(),display,render);
    REQUIRE(std::holds_alternative<rk::ReducedSdrSurface>(sceneResult));
    auto& scene=std::get<rk::ReducedSdrSurface>(sceneResult);
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width=display.width;desc.Height=display.height;desc.MipLevels=1;
    desc.ArraySize=1;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_DEFAULT;
    desc.BindFlags=D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> native;
    REQUIRE(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&native)));
    ComPtr<ID3D11RenderTargetView> nativeView;
    REQUIRE(SUCCEEDED(device->CreateRenderTargetView(native.Get(),nullptr,&nativeView)));
    rk::OwnedSceneDomain route;
    REQUIRE(route.configure({render,display,1}));
    rk::NativeUiRedirector redirect(route);
    REQUIRE(SUCCEEDED(redirect.configure(context.Get(),GetCurrentThreadId(),
        {&forwardOm,&forwardVp,&forwardScissor,&forwardPs},scene.texture(),nativeView.Get())));
    REQUIRE(route.begin(1,1));
    auto* sceneView=scene.renderTarget();
    redirect.onOMSetRenderTargets(context.Get(),1,&sceneView,nullptr);
    const D3D11_VIEWPORT reducedViewport{0,0,32,16,0,1};
    redirect.onRSSetViewports(context.Get(),1,&reducedViewport);
    const float red[4]{1,0,0,1};
    context->ClearRenderTargetView(sceneView,red);
    REQUIRE(route.startProcessing(1,1));
    // Mock provider publishes a current-frame display image; this validates
    // binding, not NGX or Skyrim's scene producer.
    const float green[4]{0,1,0,1};
    context->ClearRenderTargetView(nativeView.Get(),green);
    REQUIRE(SUCCEEDED(redirect.commitPublishedUi(1)));

    // Some custom menus set their reduced viewport while an offscreen target
    // is active, then restore the cached scene RTV without setting the
    // viewport again. Once the scene RTV is translated to the native target,
    // the effective viewport must be native too.
    D3D11_TEXTURE2D_DESC offscreenDesc=desc;
    offscreenDesc.Width=render.width;
    offscreenDesc.Height=render.height;
    ComPtr<ID3D11Texture2D> offscreen;
    REQUIRE(SUCCEEDED(device->CreateTexture2D(&offscreenDesc,nullptr,&offscreen)));
    ComPtr<ID3D11RenderTargetView> offscreenView;
    REQUIRE(SUCCEEDED(device->CreateRenderTargetView(offscreen.Get(),nullptr,
        &offscreenView)));
    auto* offscreenRaw=offscreenView.Get();
    redirect.onOMSetRenderTargets(context.Get(),1,&offscreenRaw,nullptr);
    redirect.onRSSetViewports(context.Get(),1,&reducedViewport);
    redirect.onOMSetRenderTargets(context.Get(),1,&sceneView,nullptr);
    UINT reorderedCount=1;
    D3D11_VIEWPORT reorderedViewport{};
    context->RSGetViewports(&reorderedCount,&reorderedViewport);
    REQUIRE(reorderedCount==1);
    REQUIRE(reorderedViewport.Width==display.width);
    REQUIRE(reorderedViewport.Height==display.height);
    // The 0.1.74 reduced-to-native scissor hypothesis failed in Skyrim.
    // Forward application-owned rectangles unchanged even when a cached
    // reduced viewport was translated for the native target.
    const D3D11_RECT reducedScissor{4,2,20,10};
    redirect.onRSSetScissorRects(context.Get(),1,&reducedScissor);
    UINT scissorCount=1;D3D11_RECT mappedScissor{};
    context->RSGetScissorRects(&scissorCount,&mappedScissor);
    REQUIRE(scissorCount==1);
    REQUIRE(mappedScissor.left==reducedScissor.left);
    REQUIRE(mappedScissor.top==reducedScissor.top);
    REQUIRE(mappedScissor.right==reducedScissor.right);
    REQUIRE(mappedScissor.bottom==reducedScissor.bottom);
    const D3D11_VIEWPORT nativeViewport{0,0,64,32,0,1};
    redirect.onRSSetViewports(context.Get(),1,&nativeViewport);
    redirect.onRSSetScissorRects(context.Get(),1,&reducedScissor);
    scissorCount=1;mappedScissor={};
    context->RSGetScissorRects(&scissorCount,&mappedScissor);
    REQUIRE(mappedScissor.left==reducedScissor.left);
    REQUIRE(mappedScissor.top==reducedScissor.top);
    REQUIRE(mappedScissor.right==reducedScissor.right);
    REQUIRE(mappedScissor.bottom==reducedScissor.bottom);

    redirect.onOMSetRenderTargets(context.Get(),1,&sceneView,nullptr);
    redirect.onRSSetViewports(context.Get(),1,&reducedViewport);
    ComPtr<ID3D11RenderTargetView> bound;
    context->OMGetRenderTargets(1,bound.GetAddressOf(),nullptr);
    auto boundResource=viewResource(bound.Get());
    REQUIRE(identity(boundResource.Get()).Get()==identity(native.Get()).Get());
    UINT count=1;D3D11_VIEWPORT active{};
    context->RSGetViewports(&count,&active);
    REQUIRE(count==1);
    REQUIRE(active.Width==64);
    REQUIRE(active.Height==32);
    const float blue[4]{0,0,1,1};
    context->ClearRenderTargetView(bound.Get(),blue);
    desc.BindFlags=0;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging;
    REQUIRE(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&staging)));
    context->CopyResource(staging.Get(),native.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    REQUIRE(SUCCEEDED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)));
    auto* farPixel=static_cast<const unsigned char*>(mapped.pData)+31*mapped.RowPitch+63*4;
    REQUIRE(farPixel[0]==0);
    REQUIRE(farPixel[1]==0);
    REQUIRE(farPixel[2]==255);
    context->Unmap(staging.Get(),0);
    REQUIRE_FALSE(redirect.compatibilityFault());
    REQUIRE(route.closePublishedFrame(1,1));
    REQUIRE(route.begin(2,1));
    redirect.onOMSetRenderTargets(context.Get(),1,&sceneView,nullptr);
    bound.Reset();context->OMGetRenderTargets(1,bound.GetAddressOf(),nullptr);
    boundResource=viewResource(bound.Get());
    REQUIRE(identity(boundResource.Get()).Get()==identity(scene.texture()).Get());
    REQUIRE(route.startProcessing(2,1));
    native->GetDesc(&desc);
    ComPtr<ID3D11Texture2D> nextNative;
    REQUIRE(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&nextNative)));
    ComPtr<ID3D11RenderTargetView> nextNativeView;
    REQUIRE(SUCCEEDED(device->CreateRenderTargetView(nextNative.Get(),nullptr,&nextNativeView)));
    REQUIRE(SUCCEEDED(redirect.replaceNativeTarget(nextNativeView.Get())));
    REQUIRE(SUCCEEDED(redirect.commitPublishedUi(2)));
    bound.Reset();boundResource.Reset();
    context->OMGetRenderTargets(1,bound.GetAddressOf(),nullptr);
    boundResource=viewResource(bound.Get());
    REQUIRE(identity(boundResource.Get()).Get()==identity(nextNative.Get()).Get());
    bound.Reset();boundResource.Reset();
    route.suspend();redirect.releaseAfterRetirement(true);
    context->OMGetRenderTargets(1,bound.GetAddressOf(),nullptr);
    REQUIRE(bound==nullptr);
}

TEST_CASE("WARP menu marker observes reduced scene binds without changing them", "[native_ui]") {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL level{};
    REQUIRE(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,
        nullptr,0,D3D11_SDK_VERSION,&device,&level,&context)));
    constexpr rk::Extent render{32,16},display{64,32};
    auto sceneResult=rk::createReducedSdrSurface(device.Get(),display,render);
    REQUIRE(std::holds_alternative<rk::ReducedSdrSurface>(sceneResult));
    auto& scene=std::get<rk::ReducedSdrSurface>(sceneResult);
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width=display.width;desc.Height=display.height;desc.MipLevels=1;
    desc.ArraySize=1;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_DEFAULT;
    desc.BindFlags=D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> native;
    REQUIRE(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&native)));
    ComPtr<ID3D11RenderTargetView> nativeView;
    REQUIRE(SUCCEEDED(device->CreateRenderTargetView(native.Get(),nullptr,&nativeView)));
    desc.Width=render.width;desc.Height=render.height;
    desc.Format=DXGI_FORMAT_R24G8_TYPELESS;
    desc.BindFlags=D3D11_BIND_DEPTH_STENCIL|D3D11_BIND_SHADER_RESOURCE;
    ComPtr<ID3D11Texture2D> depth;
    REQUIRE(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&depth)));
    D3D11_DEPTH_STENCIL_VIEW_DESC depthViewDesc{};
    depthViewDesc.Format=DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthViewDesc.ViewDimension=D3D11_DSV_DIMENSION_TEXTURE2D;
    // The observed source view may be read-only. Owned full-size views have
    // independent writable/clear roles and must not inherit those flags.
    depthViewDesc.Flags=D3D11_DSV_READ_ONLY_DEPTH|
        D3D11_DSV_READ_ONLY_STENCIL;
    ComPtr<ID3D11DepthStencilView> depthView;
    REQUIRE(SUCCEEDED(device->CreateDepthStencilView(depth.Get(),&depthViewDesc,&depthView)));
    D3D11_SHADER_RESOURCE_VIEW_DESC depthSrvDesc{};
    depthSrvDesc.Format=DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    depthSrvDesc.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;
    depthSrvDesc.Texture2D.MipLevels=1;
    ComPtr<ID3D11ShaderResourceView> depthSrv;
    REQUIRE(SUCCEEDED(device->CreateShaderResourceView(depth.Get(),&depthSrvDesc,&depthSrv)));
    desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BindFlags=D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> auxiliary;
    REQUIRE(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&auxiliary)));
    ComPtr<ID3D11RenderTargetView> auxiliaryView;
    REQUIRE(SUCCEEDED(device->CreateRenderTargetView(auxiliary.Get(),nullptr,
        &auxiliaryView)));
    ComPtr<ID3D11Texture2D> secondAuxiliary;
    REQUIRE(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&secondAuxiliary)));
    ComPtr<ID3D11RenderTargetView> secondAuxiliaryView;
    REQUIRE(SUCCEEDED(device->CreateRenderTargetView(secondAuxiliary.Get(),nullptr,
        &secondAuxiliaryView)));
    rk::OwnedSceneDomain route;
    REQUIRE(route.configure({render,display,1}));
    REQUIRE(route.begin(7,1,GetCurrentThreadId()));
    rk::NativeUiRedirector redirect(route);
    REQUIRE(SUCCEEDED(redirect.configure(context.Get(),GetCurrentThreadId(),
        {&forwardOm,&forwardVp,&forwardScissor,&forwardPs},scene.texture(),nativeView.Get())));
    auto* sceneView=scene.renderTarget();
    const D3D11_VIEWPORT reducedViewport{0,0,32,16,0,1};
    std::array<ID3D11RenderTargetView*,2> reducedTargets{
        sceneView,auxiliaryView.Get()};
    REQUIRE(redirect.beginObservation(7));
    redirect.onOMSetRenderTargets(context.Get(),2,reducedTargets.data(),depthView.Get());
    redirect.onRSSetViewports(context.Get(),1,&reducedViewport);
    forwardOm(context.Get(),0,nullptr,nullptr);
    auto* sampledDepth=depthSrv.Get();
    redirect.onPSSetShaderResources(context.Get(),3,1,&sampledDepth);
    ComPtr<ID3D11ShaderResourceView> boundDepthSrv;
    context->PSGetShaderResources(3,1,&boundDepthSrv);
    REQUIRE(identity(viewResource(boundDepthSrv.Get()).Get()).Get()==
        identity(depth.Get()).Get());
    for(unsigned pass=0;pass<3;++pass) {
        redirect.onOMSetRenderTargets(context.Get(),1,&sceneView,depthView.Get());
        redirect.onRSSetViewports(context.Get(),1,&reducedViewport);
    }
    auto observed=redirect.finishObservation(7);
    REQUIRE(observed.has_value());
    REQUIRE(observed->frame==7);
    REQUIRE(observed->count==8);
    REQUIRE(observed->sampledDepthReads==1);
    REQUIRE(observed->firstSampledDepthSlot==3);
    REQUIRE(observed->events[0].kind==rk::UiObservationKind::RenderTargets);
    REQUIRE(observed->events[0].sceneSlot==0);
    REQUIRE(observed->events[0].hasDepth);
    REQUIRE(observed->events[0].targetCount==2);
    REQUIRE(observed->events[1].kind==rk::UiObservationKind::Viewport);
    REQUIRE(observed->events[1].viewport.width==render.width);
    REQUIRE(observed->events[1].viewport.height==render.height);
    REQUIRE(observed->events[2].kind==rk::UiObservationKind::RenderTargets);
    REQUIRE(observed->events[2].hasDepth);
    REQUIRE(observed->events[2].targetCount==1);
    REQUIRE(observed->events[2].depth.width==render.width);
    REQUIRE(observed->events[2].depth.height==render.height);
    REQUIRE(observed->events[4].kind==rk::UiObservationKind::RenderTargets);
    REQUIRE(observed->events[4].targetCount==1);
    REQUIRE(observed->events[4].hasDepth);
    REQUIRE(observed->events[6].targetCount==1);
    ComPtr<ID3D11RenderTargetView> bound;
    ComPtr<ID3D11DepthStencilView> boundDepth;
    context->OMGetRenderTargets(1,&bound,&boundDepth);
    REQUIRE(identity(viewResource(bound.Get()).Get()).Get()==
        identity(scene.texture()).Get());
    REQUIRE(identity(viewResource(boundDepth.Get()).Get()).Get()==
        identity(depth.Get()).Get());
    REQUIRE_FALSE(redirect.compatibilityFault());
    REQUIRE(SUCCEEDED(redirect.prepareObservedCompanions()));
    REQUIRE_FALSE(redirect.companionsReady());
    reducedTargets[1]=secondAuxiliaryView.Get();
    REQUIRE(redirect.beginObservation(7));
    redirect.onOMSetRenderTargets(context.Get(),2,reducedTargets.data(),depthView.Get());
    redirect.onRSSetViewports(context.Get(),1,&reducedViewport);
    redirect.onPSSetShaderResources(context.Get(),3,1,&sampledDepth);
    for(unsigned pass=0;pass<3;++pass) {
        redirect.onOMSetRenderTargets(context.Get(),1,&sceneView,depthView.Get());
        redirect.onRSSetViewports(context.Get(),1,&reducedViewport);
    }
    observed=redirect.finishObservation(7);
    REQUIRE(observed.has_value());
    REQUIRE(observed->count==8);
    REQUIRE(observed->events[0].targetIdentities[1]==
        reinterpret_cast<std::uintptr_t>(identity(secondAuxiliary.Get()).Get()));
    REQUIRE(SUCCEEDED(redirect.prepareObservedCompanions()));
    REQUIRE(redirect.companionsReady());
    const auto depthContract=redirect.depthViewContract();
    REQUIRE(depthContract.has_value());
    REQUIRE(depthContract->sourceFormat==DXGI_FORMAT_D24_UNORM_S8_UINT);
    REQUIRE(depthContract->sourceFlags==
        (D3D11_DSV_READ_ONLY_DEPTH|D3D11_DSV_READ_ONLY_STENCIL));
    REQUIRE(depthContract->writableFlags==0);
    REQUIRE(depthContract->sampledClearFlags==0);
    REQUIRE(route.startProcessing(7,1));
    REQUIRE(SUCCEEDED(redirect.commitPublishedUi(7)));
    // Scaleform defers its masked widget draws until GRenderer::EndFrame.
    // The native UI commit must therefore leave the matching display-sized
    // stencil attachment bound even before a menu explicitly rebinds the
    // reduced scene target.
    bound.Reset();boundDepth.Reset();
    context->OMGetRenderTargets(1,bound.GetAddressOf(),boundDepth.GetAddressOf());
    REQUIRE(identity(viewResource(bound.Get()).Get()).Get()==
        identity(native.Get()).Get());
    REQUIRE(boundDepth!=nullptr);
    REQUIRE(viewExtent(boundDepth.Get()).width==display.width);
    REQUIRE(viewExtent(boundDepth.Get()).height==display.height);
    D3D11_DEPTH_STENCIL_VIEW_DESC boundDepthDesc{};
    boundDepth->GetDesc(&boundDepthDesc);
    REQUIRE(boundDepthDesc.Flags==0);
    redirect.onOMSetRenderTargets(context.Get(),2,reducedTargets.data(),depthView.Get());
    redirect.onRSSetViewports(context.Get(),1,&reducedViewport);
    std::array<ComPtr<ID3D11RenderTargetView>,2> nativeTargets;
    boundDepth.Reset();
    std::array<ID3D11RenderTargetView*,2> rawTargets{};
    context->OMGetRenderTargets(2,rawTargets.data(),boundDepth.GetAddressOf());
    for(std::size_t i=0;i<nativeTargets.size();++i)nativeTargets[i].Attach(rawTargets[i]);
    REQUIRE(identity(viewResource(nativeTargets[0].Get()).Get()).Get()==
        identity(native.Get()).Get());
    REQUIRE(identity(viewResource(nativeTargets[1].Get()).Get()).Get()!=
        identity(secondAuxiliary.Get()).Get());
    // A later menu may retain native colour and auxiliary targets while
    // dropping the shared DSV. The exact common EndFrame boundary restores
    // the existing full-size stencil attachment without clearing it again or
    // changing the current MRT set.
    std::array<ID3D11RenderTargetView*,2> deferredTargets{
        nativeTargets[0].Get(),nativeTargets[1].Get()};
    context->OMSetRenderTargets(2,deferredTargets.data(),nullptr);
    REQUIRE(SUCCEEDED(redirect.rebindForDeferredUiFlush(7)));
    std::array<ComPtr<ID3D11RenderTargetView>,2> reboundTargets;
    std::array<ID3D11RenderTargetView*,2> reboundRaw{};
    boundDepth.Reset();
    context->OMGetRenderTargets(2,reboundRaw.data(),boundDepth.GetAddressOf());
    for(std::size_t i=0;i<reboundTargets.size();++i)
        reboundTargets[i].Attach(reboundRaw[i]);
    REQUIRE(identity(viewResource(reboundTargets[0].Get()).Get()).Get()==
        identity(viewResource(nativeTargets[0].Get()).Get()).Get());
    REQUIRE(identity(viewResource(reboundTargets[1].Get()).Get()).Get()==
        identity(viewResource(nativeTargets[1].Get()).Get()).Get());
    REQUIRE(boundDepth!=nullptr);
    REQUIRE(viewExtent(boundDepth.Get()).width==display.width);
    REQUIRE(viewExtent(boundDepth.Get()).height==display.height);
    const auto auxiliaryExtent=viewExtent(nativeTargets[1].Get());
    REQUIRE(auxiliaryExtent.width==display.width);
    REQUIRE(auxiliaryExtent.height==display.height);
    reducedTargets[1]=auxiliaryView.Get();
    redirect.onOMSetRenderTargets(context.Get(),2,reducedTargets.data(),depthView.Get());
    for(auto& target:nativeTargets)target.Reset();
    boundDepth.Reset();rawTargets={};
    context->OMGetRenderTargets(2,rawTargets.data(),boundDepth.GetAddressOf());
    for(std::size_t i=0;i<nativeTargets.size();++i)nativeTargets[i].Attach(rawTargets[i]);
    REQUIRE(identity(viewResource(nativeTargets[1].Get()).Get()).Get()!=
        identity(auxiliary.Get()).Get());
    REQUIRE(viewExtent(nativeTargets[1].Get()).width==display.width);
    ComPtr<ID3D11Texture2D> nativeDepth;
    REQUIRE(SUCCEEDED(viewResource(boundDepth.Get()).As(&nativeDepth)));
    D3D11_TEXTURE2D_DESC nativeDepthDesc{};nativeDepth->GetDesc(&nativeDepthDesc);
    REQUIRE(nativeDepthDesc.Width==display.width);
    REQUIRE(nativeDepthDesc.Height==display.height);
    redirect.onPSSetShaderResources(context.Get(),3,1,&sampledDepth);
    boundDepthSrv.Reset();
    context->PSGetShaderResources(3,1,&boundDepthSrv);
    REQUIRE(boundDepthSrv!=nullptr);
    auto sampledResource=viewResource(boundDepthSrv.Get());
    REQUIRE(identity(sampledResource.Get()).Get()!=identity(depth.Get()).Get());
    REQUIRE(identity(sampledResource.Get()).Get()!=identity(nativeDepth.Get()).Get());
    REQUIRE(viewExtent(boundDepthSrv.Get()).width==display.width);
    REQUIRE(viewExtent(boundDepthSrv.Get()).height==display.height);
    UINT viewportCount=1;D3D11_VIEWPORT nativeViewport{};
    context->RSGetViewports(&viewportCount,&nativeViewport);
    REQUIRE(nativeViewport.Width==display.width);
    REQUIRE(nativeViewport.Height==display.height);
    REQUIRE_FALSE(redirect.compatibilityFault());
    desc.MipLevels=2;
    ComPtr<ID3D11Texture2D> unknownReduced;
    REQUIRE(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&unknownReduced)));
    D3D11_RENDER_TARGET_VIEW_DESC unknownViewDesc{};
    unknownViewDesc.Format=desc.Format;
    unknownViewDesc.ViewDimension=D3D11_RTV_DIMENSION_TEXTURE2D;
    ComPtr<ID3D11RenderTargetView> unknownReducedView;
    REQUIRE(SUCCEEDED(device->CreateRenderTargetView(unknownReduced.Get(),
        &unknownViewDesc,&unknownReducedView)));
    reducedTargets[1]=unknownReducedView.Get();
    redirect.onOMSetRenderTargets(context.Get(),2,reducedTargets.data(),depthView.Get());
    REQUIRE(redirect.compatibilityFault());
    redirect.disableLatePassRouting();
    REQUIRE_FALSE(redirect.compatibilityFault());
    REQUIRE_FALSE(redirect.latePassRoutingAvailable());
    REQUIRE(route.closePublishedFrame(7,1));
    route.suspend();
    redirect.releaseAfterRetirement();
}
