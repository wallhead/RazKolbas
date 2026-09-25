#include <catch2/catch_test_macros.hpp>
#include "rk/NativeFlipTarget.hpp"
#include "rk/NativeUiRedirector.hpp"
#include "rk/ReducedSdrSurface.hpp"
#include "rk/SdrSrPresentation.hpp"
#include "rk/SrInput.hpp"
#include <array>
#include <wrl/client.h>

namespace {
using Microsoft::WRL::ComPtr;
struct Window {
    HWND handle{CreateWindowW(L"STATIC",L"RazKolbas owned frame fixture",
        WS_OVERLAPPED,0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr)};
    ~Window(){if(handle)DestroyWindow(handle);}
};
void STDMETHODCALLTYPE nextOm(ID3D11DeviceContext* context,UINT count,
    ID3D11RenderTargetView* const* views,ID3D11DepthStencilView* depth) {
    context->OMSetRenderTargets(count,views,depth);
}
void STDMETHODCALLTYPE nextVp(ID3D11DeviceContext* context,UINT count,
    const D3D11_VIEWPORT* views) {context->RSSetViewports(count,views);}
void STDMETHODCALLTYPE nextScissor(ID3D11DeviceContext* context,UINT count,
    const D3D11_RECT* rects) {context->RSSetScissorRects(count,rects);}
void STDMETHODCALLTYPE nextPs(ID3D11DeviceContext* context,UINT start,UINT count,
    ID3D11ShaderResourceView* const* views) {
    context->PSSetShaderResources(start,count,views);
}
}

TEST_CASE("Owned reduced SDR scene publishes spatial fallback before native UI",
    "[owned_pipeline]") {
    Window window;
    REQUIRE(window.handle!=nullptr);
    constexpr rk::Extent display{64,32},render{32,16};
    DXGI_SWAP_CHAIN_DESC chain{};
    chain.BufferDesc.Width=display.width;chain.BufferDesc.Height=display.height;
    chain.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    chain.SampleDesc.Count=1;chain.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
    chain.BufferCount=3;chain.OutputWindow=window.handle;chain.Windowed=TRUE;
    chain.SwapEffect=DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGISwapChain> swap;
    REQUIRE(SUCCEEDED(D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_WARP,
        nullptr,0,nullptr,0,D3D11_SDK_VERSION,&chain,&swap,&device,nullptr,&context)));
    auto sceneResult=rk::createReducedSdrSurface(device.Get(),display,render);
    REQUIRE(std::holds_alternative<rk::ReducedSdrSurface>(sceneResult));
    auto& scene=std::get<rk::ReducedSdrSurface>(sceneResult);
    auto nativeResult=rk::acquireNativeFlipTarget(swap.Get(),device.Get(),display);
    REQUIRE(std::holds_alternative<rk::NativeFlipTarget>(nativeResult));
    auto& native=std::get<rk::NativeFlipTarget>(nativeResult);
    rk::OwnedSceneDomain domain;
    REQUIRE(domain.configure({render,display,1}));
    rk::NativeUiRedirector ui(domain);
    REQUIRE(SUCCEEDED(ui.configure(context.Get(),GetCurrentThreadId(),
        {&nextOm,&nextVp,&nextScissor,&nextPs},scene.texture(),native.view.Get())));
    const std::array formats{DXGI_FORMAT_R16G16_FLOAT,DXGI_FORMAT_R24G8_TYPELESS};
    std::array<ComPtr<ID3D11Texture2D>,2> guides;
    for(std::size_t i=0;i<guides.size();++i) {
        D3D11_TEXTURE2D_DESC d{};
        d.Width=display.width;d.Height=display.height;
        d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;d.Format=formats[i];
        d.BindFlags=D3D11_BIND_SHADER_RESOURCE|
            (i?D3D11_BIND_DEPTH_STENCIL:D3D11_BIND_RENDER_TARGET);
        REQUIRE(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&guides[i])));
    }
    REQUIRE(domain.begin(1,1,GetCurrentThreadId()));
    auto* sceneView=scene.renderTarget();
    ui.onOMSetRenderTargets(context.Get(),1,&sceneView,nullptr);
    const D3D11_VIEWPORT reducedViewport{0,0,32,16,0,1};
    ui.onRSSetViewports(context.Get(),1,&reducedViewport);
    const float blue[4]{0,0,1,1};
    context->ClearRenderTargetView(sceneView,blue);
    const std::array<ID3D11Texture2D*,3> sources{
        scene.texture(),guides[0].Get(),guides[1].Get()};
    auto prepared=rk::prepareSdrSrInputsFromOwnedScene(context.Get(),sources,
        display.width,display.height);
    REQUIRE(std::holds_alternative<rk::PreparedSrInputs>(prepared));
    REQUIRE(domain.startProcessing(1,1));
    REQUIRE(SUCCEEDED(ui.replaceNativeTarget(native.view.Get())));
    REQUIRE(SUCCEEDED(ui.bindNativeForProcessing(1)));
    auto published=rk::presentSdrSrFrame(context.Get(),scene.texture(),
        native.texture.Get(),[]()->rk::Result<bool> {
            return rk::Error{rk::ErrorCode::Unavailable,"fixture provider busy"};
        });
    REQUIRE(std::holds_alternative<rk::SdrSrFrameResult>(published));
    auto& outcome=std::get<rk::SdrSrFrameResult>(published);
    REQUIRE(outcome.mode()==rk::SdrSrFrameMode::SpatialFallback);
    REQUIRE(SUCCEEDED(ui.commitPublishedUi(1)));
    ui.onOMSetRenderTargets(context.Get(),1,&sceneView,nullptr);
    ui.onRSSetViewports(context.Get(),1,&reducedViewport);
    ComPtr<ID3D11RenderTargetView> bound;
    context->OMGetRenderTargets(1,&bound,nullptr);
    ComPtr<ID3D11Resource> boundResource;
    bound->GetResource(&boundResource);
    REQUIRE(boundResource.Get()==native.texture.Get());
    UINT viewportCount=1;D3D11_VIEWPORT viewport{};
    context->RSGetViewports(&viewportCount,&viewport);
    REQUIRE(viewportCount==1);
    REQUIRE(viewport.Width==display.width);
    REQUIRE(viewport.Height==display.height);
    REQUIRE_FALSE(ui.compatibilityFault());
    context->Flush();
    bool retired=false;
    for(unsigned attempt=0;attempt<100&&!retired;++attempt) {
        const auto done=outcome.complete(context.Get());
        REQUIRE(std::holds_alternative<bool>(done));
        retired=std::get<bool>(done);
        if(!retired)Sleep(1);
    }
    REQUIRE(retired);
    REQUIRE(domain.closePublishedFrame(1,1));
    REQUIRE(domain.begin(2,1,GetCurrentThreadId()));
    ui.onOMSetRenderTargets(context.Get(),1,&sceneView,nullptr);
    const float red[4]{1,0,0,1};
    context->ClearRenderTargetView(sceneView,red);
    REQUIRE(domain.startProcessing(2,1));
    REQUIRE(SUCCEEDED(ui.replaceNativeTarget(native.view.Get())));
    REQUIRE(SUCCEEDED(ui.bindNativeForProcessing(2)));
    auto nextFrame=rk::presentSdrSrFrame(context.Get(),scene.texture(),
        native.texture.Get(),[]()->rk::Result<bool> {
            return rk::Error{rk::ErrorCode::Unavailable,"spatial-only fixture"};
        });
    REQUIRE(std::holds_alternative<rk::SdrSrFrameResult>(nextFrame));
    REQUIRE(std::get<rk::SdrSrFrameResult>(nextFrame).mode()==
        rk::SdrSrFrameMode::SpatialFallback);
    REQUIRE(SUCCEEDED(ui.commitPublishedUi(2)));
    REQUIRE(domain.closePublishedFrame(2,1));
    REQUIRE_FALSE(ui.compatibilityFault());
    context->Flush();
    retired=false;
    for(unsigned attempt=0;attempt<100&&!retired;++attempt) {
        const auto done=std::get<rk::SdrSrFrameResult>(nextFrame).complete(context.Get());
        REQUIRE(std::holds_alternative<bool>(done));
        retired=std::get<bool>(done);
        if(!retired)Sleep(1);
    }
    REQUIRE(retired);
    context->OMSetRenderTargets(0,nullptr,nullptr);
    bound.Reset();boundResource.Reset();
    domain.suspend();ui.releaseAfterRetirement();
}
