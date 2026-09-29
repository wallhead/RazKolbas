#include <catch2/catch_test_macros.hpp>
#include "rk/FgStreamlineTags.hpp"
#include "rk/FgStreamlineFrameInputs.hpp"
#include "rk/FgStreamlineSubmit.hpp"
#include "rk/FgStreamlineInputLease.hpp"
#include <dxgi1_6.h>
#include <variant>

using Microsoft::WRL::ComPtr;

namespace {
struct Warp {
    ComPtr<IDXGIAdapter> adapter;
    ComPtr<ID3D12Device> device;
    Warp() {
        ComPtr<IDXGIFactory4> factory;
        REQUIRE(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))));
        REQUIRE(SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter))));
        REQUIRE(SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&device))));
    }
    ComPtr<ID3D12Resource> texture(rk::Extent size,DXGI_FORMAT format) {
        D3D12_RESOURCE_DESC desc{};
        desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width=size.width;desc.Height=size.height;
        desc.DepthOrArraySize=1;desc.MipLevels=1;
        desc.Format=format;desc.SampleDesc.Count=1;
        D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;
        ComPtr<ID3D12Resource> result;
        const auto shaderRead=static_cast<D3D12_RESOURCE_STATES>(
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE|
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        REQUIRE(SUCCEEDED(device->CreateCommittedResource(&heap,
            D3D12_HEAP_FLAG_NONE,&desc,shaderRead,
            nullptr,IID_PPV_ARGS(&result))));
        return result;
    }
};
rk::FgPreparedSubmission submission(Warp& gpu) {
    rk::FgPreparedSubmission frame{};
    frame.source=17;frame.generation=3;frame.presentToken=41;
    frame.resetEpoch=9;frame.render={32,20};frame.display={64,40};
    frame.swapBufferCount=2;
    frame.copyTicket={2,3};
    frame.resources[0]=gpu.texture(frame.display,DXGI_FORMAT_R8G8B8A8_UNORM);
    frame.resources[1]=gpu.texture(frame.render,DXGI_FORMAT_R32_FLOAT);
    frame.resources[2]=gpu.texture(frame.render,DXGI_FORMAT_R16G16_FLOAT);
    frame.resources[3]=gpu.texture(frame.display,DXGI_FORMAT_R8G8B8A8_UNORM);
    frame.resources[4]=gpu.texture(frame.display,DXGI_FORMAT_R8G8B8A8_UNORM);
    return frame;
}
rk::FgStreamlineReadStates states(const rk::FgPreparedSubmission& frame) {
    rk::FgStreamlineReadStates result{};
    result.completedCopy=frame.copyTicket;
    result.actual.fill(static_cast<D3D12_RESOURCE_STATES>(
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE|
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
    return result;
}
rk::FgSourceFrame source(const rk::FgPreparedSubmission& prepared) {
    rk::FgSourceFrame frame{};
    frame.source=prepared.source;frame.generation=prepared.generation;
    frame.presentToken=prepared.presentToken;
    frame.resetEpoch=prepared.resetEpoch;
    frame.render=prepared.render;frame.display=prepared.display;
    frame.ownerReady=true;frame.worldActive=true;frame.cameraValid=true;
    return frame;
}
void setCamera(rk::FgPreparedSubmission& prepared) {
    auto& camera=prepared.camera;
    camera.source=prepared.source;camera.generation=prepared.generation;
    camera.presentToken=prepared.presentToken;
    camera.resetEpoch=prepared.resetEpoch;camera.sampleRevision=1;
    for(auto* matrix:{&camera.viewToClip,&camera.clipToView,
        &camera.clipToPrevClip,&camera.prevClipToClip})
        for(unsigned i=0;i<4;++i)(*matrix)[i*4+i]=1.0f;
    camera.right={1,0,0};camera.up={0,1,0};
    camera.forward={0,0,1};camera.mvecScale={1,1};
    camera.nearPlane=0.1f;camera.farPlane=100.0f;
    camera.fovRadians=1.0f;camera.aspectRatio=1.6f;
    camera.cameraMotionIncluded=true;
}
struct TestToken final : sl::FrameToken {
    operator std::uint32_t() const override { return 41; }
};
}

TEST_CASE("FG Streamline tags retain five current textures with domain-correct extents",
    "[fg_streamline_tags]") {
    Warp gpu;
    auto frame=submission(gpu);
    auto ready=states(frame);
    auto built=rk::makeFgStreamlineTags(frame,ready);
    REQUIRE(std::holds_alternative<std::unique_ptr<rk::FgStreamlineTagBundle>>(built));
    auto bundle=std::move(std::get<std::unique_ptr<rk::FgStreamlineTagBundle>>(built));
    REQUIRE(bundle->presentToken()==frame.presentToken);
    const auto* tags=bundle->tags();
    const std::array<sl::BufferType,5> roles{
        sl::kBufferTypeDepth,sl::kBufferTypeMotionVectors,
        sl::kBufferTypeHUDLessColor,sl::kBufferTypeUIColorAndAlpha,
        sl::kBufferTypeBackbuffer};
    const std::array<std::size_t,4> resourceIndex{1,2,3,4};
    for(std::size_t i=0;i<rk::FgStreamlineTagBundle::tagCount;++i) {
        CHECK(tags[i].type==roles[i]);
        CHECK(tags[i].lifecycle==sl::eValidUntilPresent);
        CHECK(tags[i].extent.left==0);
        CHECK(tags[i].extent.top==0);
        CHECK(tags[i].extent.width==(i<2?frame.render.width:frame.display.width));
        CHECK(tags[i].extent.height==(i<2?frame.render.height:frame.display.height));
        if(i<4) {
            CHECK(tags[i].resource->native==frame.resources[resourceIndex[i]].Get());
            CHECK(tags[i].resource->state==static_cast<std::uint32_t>(ready.actual[resourceIndex[i]]));
        } else CHECK(tags[i].resource==nullptr);
    }
    std::array<ComPtr<ID3D12Resource>,5> retained=frame.resources;
    frame={};
    CHECK(bundle->sourceColor()==retained[0].Get());
    for(std::size_t i=0;i<4;++i)
        CHECK(tags[i].resource->native==retained[resourceIndex[i]].Get());
}

TEST_CASE("FG Streamline tags reject stale or non-readable input states",
    "[fg_streamline_tags]") {
    Warp gpu;
    auto frame=submission(gpu);
    auto ready=states(frame);
    ready.actual[2]=D3D12_RESOURCE_STATE_COMMON;
    CHECK(std::holds_alternative<rk::Error>(rk::makeFgStreamlineTags(frame,ready)));
    ready=states(frame);
    ready.completedCopy.copy++;
    CHECK(std::holds_alternative<rk::Error>(rk::makeFgStreamlineTags(frame,ready)));
    ready=states(frame);
    frame.resources[4].Reset();
    CHECK(std::holds_alternative<rk::Error>(rk::makeFgStreamlineTags(frame,ready)));
    frame=submission(gpu);
    frame.resources[1]=gpu.texture(frame.display,DXGI_FORMAT_R32_FLOAT);
    CHECK(std::holds_alternative<rk::Error>(rk::makeFgStreamlineTags(frame,
        states(frame))));
}

TEST_CASE("FG Streamline frame packet keeps one source token across constants options and tags",
    "[fg_streamline_tags]") {
    Warp gpu;
    auto prepared=submission(gpu);
    setCamera(prepared);
    const auto frame=source(prepared);
    auto result=rk::prepareFgStreamlineFrameInputs(frame,prepared,
        states(prepared),DXGI_FORMAT_R8G8B8A8_UNORM);
    REQUIRE(std::holds_alternative<rk::FgStreamlineFrameInputs>(result));
    auto& packet=std::get<rk::FgStreamlineFrameInputs>(result);
    REQUIRE(packet.source==frame.source);
    REQUIRE(packet.presentToken==frame.presentToken);
    REQUIRE(packet.tags->presentToken()==frame.presentToken);
    REQUIRE(packet.options.mvecDepthWidth==frame.render.width);
    REQUIRE(packet.options.colorWidth==frame.display.width);
    REQUIRE(packet.constants.mvecScale.x==1.0f);
    auto stale=frame;stale.presentToken++;
    REQUIRE(std::holds_alternative<rk::Error>(
        rk::prepareFgStreamlineFrameInputs(stale,prepared,
            states(prepared),DXGI_FORMAT_R8G8B8A8_UNORM)));
}

TEST_CASE("FG Streamline submission binds one SDK token and propagates tag failure",
    "[fg_streamline_tags]") {
    Warp gpu;
    auto prepared=submission(gpu);setCamera(prepared);
    const auto frame=source(prepared);
    auto result=rk::prepareFgStreamlineFrameInputs(frame,prepared,
        states(prepared),DXGI_FORMAT_R8G8B8A8_UNORM);
    REQUIRE(std::holds_alternative<rk::FgStreamlineFrameInputs>(result));
    auto& packet=std::get<rk::FgStreamlineFrameInputs>(result);
    TestToken token;
    const rk::FgStreamlineTokenBinding binding{
        frame.source,frame.generation,frame.presentToken,frame.resetEpoch,&token};
    unsigned calls{};
    rk::FgStreamlineCalls sdk{};
    sdk.setConstants=[&](const sl::Constants& values,
        const sl::FrameToken& supplied,const sl::ViewportHandle&) {
        CHECK(&supplied==&token);
        CHECK(values.mvecScale.x==1.0f);
        CHECK(calls++==0);
        return sl::Result::eOk;
    };
    sdk.setTags=[&](const sl::FrameToken& supplied,
        const sl::ViewportHandle&,const sl::ResourceTag* tags,
        std::uint32_t count,sl::CommandBuffer*) {
        CHECK(&supplied==&token);
        CHECK(calls++==1);
        CHECK(count==5);
        CHECK(tags[0].resource->native==prepared.resources[1].Get());
        return sl::Result::eOk;
    };
    const sl::ViewportHandle viewport{0u};
    REQUIRE(std::holds_alternative<bool>(
        rk::submitFgStreamlineInputs(packet,binding,viewport,sdk)));
    REQUIRE(calls==2);
    auto stale=binding;stale.presentToken++;
    REQUIRE(std::holds_alternative<rk::Error>(
        rk::submitFgStreamlineInputs(packet,stale,viewport,sdk)));
    REQUIRE(calls==2);
    calls=0;
    sdk.setTags=[&](const sl::FrameToken&,
        const sl::ViewportHandle&,const sl::ResourceTag*,
        std::uint32_t,sl::CommandBuffer*) {
        ++calls;
        return static_cast<sl::Result>(-1);
    };
    REQUIRE(std::holds_alternative<rk::Error>(
        rk::submitFgStreamlineInputs(packet,binding,viewport,sdk)));
    REQUIRE(calls==2);
}

TEST_CASE("FG Streamline input lease waits for the reported provider fence",
    "[fg_streamline_tags]") {
    Warp gpu;
    auto prepared=submission(gpu);setCamera(prepared);
    auto built=rk::prepareFgStreamlineFrameInputs(source(prepared),prepared,
        states(prepared),DXGI_FORMAT_R8G8B8A8_UNORM);
    REQUIRE(std::holds_alternative<rk::FgStreamlineFrameInputs>(built));
    rk::FgStreamlineInputLease lease(
        std::move(std::get<rk::FgStreamlineFrameInputs>(built)));
    ComPtr<ID3D12Fence> fence;
    REQUIRE(SUCCEEDED(gpu.device->CreateFence(0,D3D12_FENCE_FLAG_NONE,
        IID_PPV_ARGS(&fence))));
    sl::DLSSGState reported{};
    reported.inputsProcessingCompletionFence=fence.Get();
    reported.lastPresentInputsProcessingCompletionFenceValue=4;
    REQUIRE(std::holds_alternative<bool>(lease.observeCompletion(reported)));
    REQUIRE_FALSE(lease.retired());
    REQUIRE_FALSE(lease.releaseIfRetired());
    REQUIRE(SUCCEEDED(fence->Signal(4)));
    REQUIRE(lease.retired());
    REQUIRE(lease.releaseIfRetired());
}

TEST_CASE("FG Streamline lease retains resources when completion is unknown",
    "[fg_streamline_tags]") {
    Warp gpu;
    auto prepared=submission(gpu);setCamera(prepared);
    auto built=rk::prepareFgStreamlineFrameInputs(source(prepared),prepared,
        states(prepared),DXGI_FORMAT_R8G8B8A8_UNORM);
    REQUIRE(std::holds_alternative<rk::FgStreamlineFrameInputs>(built));
    const auto before=rk::FgStreamlineInputLease::quarantinedCount();
    {
        rk::FgStreamlineInputLease lease(
            std::move(std::get<rk::FgStreamlineFrameInputs>(built)));
        sl::DLSSGState missing{};
        REQUIRE(std::holds_alternative<rk::Error>(
            lease.observeCompletion(missing)));
        REQUIRE_FALSE(lease.releaseIfRetired());
    }
    REQUIRE(rk::FgStreamlineInputLease::quarantinedCount()==before+1);
}
