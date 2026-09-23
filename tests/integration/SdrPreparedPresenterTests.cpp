#include <catch2/catch_test_macros.hpp>
#include "rk/SdrDlssPresenter.hpp"
#include "rk/SdrSrPresentation.hpp"
#include "rk/FrameProbe.hpp"
#include <array>
#include <chrono>
#include <cstring>
#include <set>
#include <thread>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace {
struct Scene {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    std::array<ComPtr<ID3D11Texture2D>,3> source;
    ComPtr<ID3D11Texture2D> display;
    ComPtr<ID3D11RenderTargetView> displayView;
    Scene() {
        REQUIRE(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,
            nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)));
        const std::array formats{DXGI_FORMAT_R8G8B8A8_UNORM,
            DXGI_FORMAT_R16G16_FLOAT,DXGI_FORMAT_R24G8_TYPELESS};
        std::array<std::uint32_t,48> pixels{};
        for(std::size_t i=0;i<pixels.size();++i)pixels[i]=
            static_cast<std::uint32_t>(0x00400000+i*113);
        for(std::size_t i=0;i<source.size();++i) {
            D3D11_TEXTURE2D_DESC d{};d.Width=8;d.Height=6;
            d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;
            d.Format=formats[i];d.BindFlags=D3D11_BIND_SHADER_RESOURCE|
                (i==2?D3D11_BIND_DEPTH_STENCIL:D3D11_BIND_RENDER_TARGET);
            const D3D11_SUBRESOURCE_DATA initial{pixels.data(),8*4,0};
            REQUIRE(SUCCEEDED(device->CreateTexture2D(&d,&initial,&source[i])));
        }
        D3D11_TEXTURE2D_DESC displayDesc{};displayDesc.Width=8;displayDesc.Height=6;
        displayDesc.MipLevels=displayDesc.ArraySize=displayDesc.SampleDesc.Count=1;
        displayDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
        displayDesc.BindFlags=D3D11_BIND_RENDER_TARGET;
        REQUIRE(SUCCEEDED(device->CreateTexture2D(&displayDesc,nullptr,&display)));
        REQUIRE(SUCCEEDED(device->CreateRenderTargetView(display.Get(),nullptr,&displayView)));
    }
    rk::PreparedSrInputs cropped() {
        const std::array<ID3D11Texture2D*,3> raw{
            source[0].Get(),source[1].Get(),source[2].Get()};
        auto result=rk::prepareSdrSrInputsFromRegion(context.Get(),raw,
            rk::SrSourceRegion{2,1,4,3},8,6);
        REQUIRE(std::holds_alternative<rk::PreparedSrInputs>(result));
        return std::move(std::get<rk::PreparedSrInputs>(result));
    }
};
}

TEST_CASE("Prepared R32 depth reaches offscreen presenter before controlled publication",
    "[sdr_prepared_presenter]") {
    Scene scene;
    unsigned evaluations{};
    rk::SdrDlssPresenter presenter{[&](ID3D11DeviceContext* context,
        const rk::PreparedSrInputs& frame,const rk::SrFrameMetadata& metadata,
        rk::NgxJitter,float sharpness,bool reset)->rk::Result<bool> {
        ++evaluations;
        REQUIRE(metadata.frameId==10);
        REQUIRE(metadata.generation==3);
        REQUIRE(reset);
        REQUIRE(sharpness==0.6f);
        REQUIRE((frame.sourceRegion()==rk::SrSourceRegion{2,1,4,3}));
        D3D11_TEXTURE2D_DESC depth{};frame.depth()->GetDesc(&depth);
        REQUIRE(depth.Format==DXGI_FORMAT_R32_FLOAT);
        ComPtr<ID3D11Device> device;context->GetDevice(&device);
        ComPtr<ID3D11UnorderedAccessView> view;
        REQUIRE(SUCCEEDED(device->CreateUnorderedAccessView(frame.output(),nullptr,&view)));
        constexpr float colour[4]{0.25f,0.5f,0.75f,1.0f};
        context->ClearUnorderedAccessViewFloat(view.Get(),colour);
        return true;
    }};
    REQUIRE(std::holds_alternative<rk::Error>(presenter.configureQuality(
        static_cast<rk::UpscaleQuality>(99))));
    REQUIRE(std::holds_alternative<rk::Error>(presenter.configureSharpness(true,1.1f)));
    REQUIRE(std::get<bool>(presenter.configureQuality(rk::UpscaleQuality::Balanced)));
    REQUIRE(std::get<bool>(presenter.configureSharpness(true,0.6f)));
    auto evaluated=presenter.evaluatePrepared(scene.device.Get(),scene.context.Get(),
        scene.cropped(),{10,3,true},{0.125f,-0.25f});
    REQUIRE(std::holds_alternative<rk::Error>(
        presenter.configureQuality(rk::UpscaleQuality::Performance)));
    REQUIRE(std::holds_alternative<rk::Error>(presenter.configureSharpness(false,0.0f)));
    REQUIRE(std::holds_alternative<std::optional<rk::SrEvaluationToken>>(evaluated));
    const auto token=std::get<std::optional<rk::SrEvaluationToken>>(evaluated);
    REQUIRE(token.has_value());
    REQUIRE(evaluations==1);
    // A completed evaluation remains publishable until this frame is handed off.
    const std::array<ID3D11Texture2D*,1> evaluatedTarget{scene.display.Get()};
    const auto completed=rk::readbackCandidates(scene.context.Get(),evaluatedTarget);
    REQUIRE(std::holds_alternative<std::vector<rk::ProbeImage>>(completed));
    scene.context->OMSetRenderTargets(1,scene.displayView.GetAddressOf(),nullptr);
    const auto published=presenter.publishEvaluated(scene.context.Get(),*token,
        scene.display.Get());
    REQUIRE(std::holds_alternative<bool>(published));
    REQUIRE(std::get<bool>(published));
    REQUIRE(std::holds_alternative<rk::Error>(presenter.publishEvaluated(
        scene.context.Get(),{10,4,token->slot},scene.display.Get())));
    const std::array<ID3D11Texture2D*,1> target{scene.display.Get()};
    const auto captured=rk::readbackCandidates(scene.context.Get(),target);
    REQUIRE(std::holds_alternative<std::vector<rk::ProbeImage>>(captured));
    const auto& pixels=std::get<std::vector<rk::ProbeImage>>(captured)[0].pixels;
    REQUIRE(pixels[0]==64);
    REQUIRE(pixels[1]==128);
    REQUIRE(pixels[2]==191);
    scene.context->Flush();
    for(unsigned i=0;i<500;++i) {
        const auto stopped=presenter.stop(scene.context.Get());
        REQUIRE_FALSE(std::holds_alternative<rk::Error>(stopped));
        if(std::get<bool>(stopped))return;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    FAIL("Prepared frame did not retire");
}

TEST_CASE("Partial prepared evaluation failure retains work and uses current scene fallback",
    "[sdr_prepared_presenter]") {
    Scene scene;
    rk::SdrDlssPresenter presenter{[](ID3D11DeviceContext* context,
        const rk::PreparedSrInputs& frame,const rk::SrFrameMetadata&,
        rk::NgxJitter,float,bool)->rk::Result<bool> {
        ComPtr<ID3D11Device> device;context->GetDevice(&device);
        ComPtr<ID3D11UnorderedAccessView> view;
        if(FAILED(device->CreateUnorderedAccessView(frame.output(),nullptr,&view)))
            return rk::Error{rk::ErrorCode::Unavailable,"UAV unavailable"};
        constexpr float colour[4]{1,0,1,1};
        context->ClearUnorderedAccessViewFloat(view.Get(),colour);
        return rk::Error{rk::ErrorCode::Unavailable,"Injected after GPU recording"};
    }};
    auto prepared=scene.cropped();
    ComPtr<ID3D11Texture2D> currentColour=prepared.color();
    scene.context->OMSetRenderTargets(1,scene.displayView.GetAddressOf(),nullptr);
    auto result=rk::presentSdrSrFrame(scene.context.Get(),currentColour.Get(),
        scene.display.Get(),[&]()->rk::Result<bool> {
            const auto evaluated=presenter.evaluatePrepared(scene.device.Get(),
                scene.context.Get(),std::move(prepared),{20,7,true},{0,0});
            if(const auto error=std::get_if<rk::Error>(&evaluated))return *error;
            return std::get<std::optional<rk::SrEvaluationToken>>(evaluated).has_value();
        });
    REQUIRE(std::holds_alternative<rk::SdrSrFrameResult>(result));
    auto fallback=std::move(std::get<rk::SdrSrFrameResult>(result));
    REQUIRE(fallback.mode()==rk::SdrSrFrameMode::SpatialFallback);
    REQUIRE(presenter.retainedPreparedFrames()>=1);
    scene.context->Flush();
    for(unsigned i=0;i<500;++i) {
        const auto ready=fallback.complete(scene.context.Get());
        REQUIRE_FALSE(std::holds_alternative<rk::Error>(ready));
        if(std::get<bool>(ready))break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    const std::array<ID3D11Texture2D*,1> target{scene.display.Get()};
    const auto captured=rk::readbackCandidates(scene.context.Get(),target);
    REQUIRE(std::holds_alternative<std::vector<rk::ProbeImage>>(captured));
    const auto& pixels=std::get<std::vector<rk::ProbeImage>>(captured)[0].pixels;
    REQUIRE(pixels[0]!=255); // The injected magenta output was not published.
    for(unsigned i=0;i<500;++i) {
        const auto stopped=presenter.stop(scene.context.Get());
        REQUIRE_FALSE(std::holds_alternative<rk::Error>(stopped));
        if(std::get<bool>(stopped))return;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    FAIL("Failed evaluation resources did not retire");
}

TEST_CASE("A newer prepared frame invalidates the previous publication token",
    "[sdr_prepared_presenter]") {
    Scene scene;
    rk::SdrDlssPresenter presenter{[](ID3D11DeviceContext*,
        const rk::PreparedSrInputs&,const rk::SrFrameMetadata&,
        rk::NgxJitter,float,bool)->rk::Result<bool> { return true; }};
    const auto first=presenter.evaluatePrepared(scene.device.Get(),scene.context.Get(),
        scene.cropped(),{30,8,true},{0,0});
    REQUIRE(std::holds_alternative<std::optional<rk::SrEvaluationToken>>(first));
    const auto oldToken=std::get<std::optional<rk::SrEvaluationToken>>(first);
    REQUIRE(oldToken.has_value());
    const auto second=presenter.evaluatePrepared(scene.device.Get(),scene.context.Get(),
        scene.cropped(),{31,8,false},{0,0});
    REQUIRE(std::holds_alternative<std::optional<rk::SrEvaluationToken>>(second));
    REQUIRE(std::get<std::optional<rk::SrEvaluationToken>>(second).has_value());
    scene.context->OMSetRenderTargets(1,scene.displayView.GetAddressOf(),nullptr);
    REQUIRE(std::holds_alternative<rk::Error>(presenter.publishEvaluated(
        scene.context.Get(),*oldToken,scene.display.Get())));
    scene.context->Flush();
    for(unsigned i=0;i<500;++i) {
        const auto stopped=presenter.stop(scene.context.Get());
        REQUIRE_FALSE(std::holds_alternative<rk::Error>(stopped));
        if(std::get<bool>(stopped))return;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    FAIL("Superseded frames did not retire");
}

TEST_CASE("Owned scene evaluation reuses three prepared resource slots",
    "[sdr_prepared_presenter]") {
    Scene scene;
    ComPtr<ID3D11Texture2D> reducedColour;
    D3D11_TEXTURE2D_DESC reduced{};
    reduced.Width=4;reduced.Height=3;
    reduced.MipLevels=reduced.ArraySize=reduced.SampleDesc.Count=1;
    reduced.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    reduced.Usage=D3D11_USAGE_DEFAULT;
    reduced.BindFlags=D3D11_BIND_RENDER_TARGET;
    REQUIRE(SUCCEEDED(scene.device->CreateTexture2D(&reduced,nullptr,&reducedColour)));
    const std::array<ID3D11Texture2D*,3> sources{
        reducedColour.Get(),scene.source[1].Get(),scene.source[2].Get()};
    std::set<std::array<std::uintptr_t,4>> resourceSets;
    rk::SdrDlssPresenter presenter{[&](ID3D11DeviceContext*,
        const rk::PreparedSrInputs& frame,const rk::SrFrameMetadata&,
        rk::NgxJitter,float,bool)->rk::Result<bool> {
        resourceSets.insert({reinterpret_cast<std::uintptr_t>(frame.color()),
            reinterpret_cast<std::uintptr_t>(frame.motion()),
            reinterpret_cast<std::uintptr_t>(frame.depth()),
            reinterpret_cast<std::uintptr_t>(frame.output())});
        return true;
    }};
    for(std::uint64_t frame=1;frame<=360;++frame) {
        auto evaluated=presenter.evaluateOwnedScene(scene.device.Get(),scene.context.Get(),
            sources,8,6,{frame,42,frame==1},{0,0});
        REQUIRE(std::holds_alternative<std::optional<rk::SrEvaluationToken>>(evaluated));
        const auto token=std::get<std::optional<rk::SrEvaluationToken>>(evaluated);
        REQUIRE(token.has_value());
        scene.context->OMSetRenderTargets(1,scene.displayView.GetAddressOf(),nullptr);
        const auto published=presenter.publishEvaluated(scene.context.Get(),*token,
            scene.display.Get());
        REQUIRE(std::holds_alternative<bool>(published));
        REQUIRE(std::get<bool>(published));
        const std::array<ID3D11Texture2D*,1> target{scene.display.Get()};
        REQUIRE(std::holds_alternative<std::vector<rk::ProbeImage>>(
            rk::readbackCandidates(scene.context.Get(),target)));
    }
    REQUIRE(presenter.submittedFrames()==360);
    REQUIRE(resourceSets.size()==3);
    REQUIRE(presenter.retainedPreparedFrames()==3);
}

TEST_CASE("Prepared history resets after failure gap and source phase change",
    "[sdr_prepared_presenter]") {
    Scene scene;
    std::vector<bool> resets;
    unsigned calls{};
    rk::SdrDlssPresenter presenter{[&](ID3D11DeviceContext*,
        const rk::PreparedSrInputs&,const rk::SrFrameMetadata&,
        rk::NgxJitter,float,bool reset)->rk::Result<bool> {
        resets.push_back(reset);
        if(++calls==1)
            return rk::Error{rk::ErrorCode::Unavailable,"injected first failure"};
        return true;
    }};
    const auto failed=presenter.evaluatePrepared(scene.device.Get(),scene.context.Get(),
        scene.cropped(),{100,11,false,rk::SrSourcePhase::PrePresent},{0,0});
    REQUIRE(std::holds_alternative<rk::Error>(failed));
    const auto recovered=presenter.evaluatePrepared(scene.device.Get(),scene.context.Get(),
        scene.cropped(),{101,11,false,rk::SrSourcePhase::PrePresent},{0,0});
    REQUIRE(std::holds_alternative<std::optional<rk::SrEvaluationToken>>(recovered));
    REQUIRE(std::get<std::optional<rk::SrEvaluationToken>>(recovered).has_value());
    const auto gap=presenter.evaluatePrepared(scene.device.Get(),scene.context.Get(),
        scene.cropped(),{103,11,false,rk::SrSourcePhase::PrePresent},{0,0});
    REQUIRE(std::holds_alternative<std::optional<rk::SrEvaluationToken>>(gap));
    REQUIRE(std::get<std::optional<rk::SrEvaluationToken>>(gap).has_value());
    const auto phase=presenter.evaluatePrepared(scene.device.Get(),scene.context.Get(),
        scene.cropped(),{104,11,false,rk::SrSourcePhase::MenuDisplay},{0,0});
    REQUIRE(std::holds_alternative<std::optional<rk::SrEvaluationToken>>(phase));
    REQUIRE(std::get<std::optional<rk::SrEvaluationToken>>(phase).has_value());
    REQUIRE(resets==std::vector<bool>{true,true,true,true});
    scene.context->Flush();
    for(unsigned i=0;i<500;++i) {
        const auto stopped=presenter.stop(scene.context.Get());
        REQUIRE_FALSE(std::holds_alternative<rk::Error>(stopped));
        if(std::get<bool>(stopped))return;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    FAIL("History-reset fixture did not retire");
}
