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
ComPtr<IUnknown> identity(IUnknown* object) {
    ComPtr<IUnknown> result;
    if(object)object->QueryInterface(IID_PPV_ARGS(result.GetAddressOf()));
    return result;
}
ComPtr<ID3D11Resource> viewResource(ID3D11RenderTargetView* view) {
    ComPtr<ID3D11Resource> result;
    if(view)view->GetResource(result.GetAddressOf());
    return result;
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
        {&forwardOm,&forwardVp},scene.texture(),nativeView.Get())));
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
