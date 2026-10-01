#include <catch2/catch_test_macros.hpp>
#include "rk/FgGameInputCandidate.hpp"
#include "rk/FgGameInputProbe.hpp"
#include "rk/FgSubmission.hpp"
#include <dxgi1_6.h>
#include <array>
#include <chrono>
#include <thread>
#include <variant>

using Microsoft::WRL::ComPtr;

namespace {
ComPtr<ID3D11Texture2D> texture(ID3D11Device* device,rk::Extent size,
    DXGI_FORMAT format) {
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width=size.width;desc.Height=size.height;
    desc.MipLevels=1;desc.ArraySize=1;desc.SampleDesc.Count=1;
    desc.Format=format;desc.Usage=D3D11_USAGE_DEFAULT;
    desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    ComPtr<ID3D11Texture2D> result;
    REQUIRE(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&result)));
    return result;
}
struct Fixture {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    rk::FgWorldGuideFrame world{};
    rk::FgUiPlaneFrame ui{};
    Fixture() {
        D3D_FEATURE_LEVEL level{};
        REQUIRE(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,
            nullptr,0,nullptr,0,D3D11_SDK_VERSION,
            &device,&level,&context)));
        world.frame.source=33;
        world.frame.generation=7;
        world.frame.presentToken=9000;
        world.frame.resetEpoch=4;
        world.frame.render={19,11};
        world.frame.display={37,19};
        world.frame.worldActive=true;
        world.depth=texture(device.Get(),world.frame.render,DXGI_FORMAT_R32_FLOAT);
        world.motion=texture(device.Get(),world.frame.render,DXGI_FORMAT_R16G16_FLOAT);
        world.hudless=texture(device.Get(),world.frame.display,
            DXGI_FORMAT_R8G8B8A8_UNORM);
        ui.source=33;ui.generation=7;ui.presentToken=9000;ui.resetEpoch=4;
        ui.display=world.frame.display;
        ui.uiRegion={0,0,37,19};
        const auto stamp=rk::FgResourceStamp{33,7,ui.display,true,4};
        ui.hudlessStamp=stamp;ui.uiStamp=stamp;ui.finalStamp=stamp;
        ui.hudless=texture(device.Get(),ui.display,DXGI_FORMAT_R8G8B8A8_UNORM);
        ui.uiColorAlpha=texture(device.Get(),ui.display,
            DXGI_FORMAT_R8G8B8A8_UNORM);
        ui.finalColor=texture(device.Get(),ui.display,
            DXGI_FORMAT_R8G8B8A8_UNORM);
    }
};
rk::FgCameraData camera(const rk::FgSourceFrame& frame) {
    rk::FgCameraData result{};
    result.source=frame.source;
    result.generation=frame.generation;
    result.presentToken=frame.presentToken;
    result.resetEpoch=frame.resetEpoch;
    result.sampleRevision=1;
    for(auto* matrix:{&result.viewToClip,&result.clipToView,
        &result.clipToPrevClip,&result.prevClipToClip})
        for(unsigned n=0;n<4;++n)(*matrix)[n*4+n]=1.0f;
    result.right={1,0,0};result.up={0,1,0};
    result.forward={0,0,1};result.mvecScale={1,1};
    result.nearPlane=0.1f;result.farPlane=100.0f;
    result.fovRadians=1.0f;
    result.aspectRatio=static_cast<float>(frame.display.width)/
        static_cast<float>(frame.display.height);
    return result;
}
}

TEST_CASE("FG game inputs pair five owned textures from one real frame",
    "[fg_game_inputs]") {
    Fixture f;
    const auto result=rk::pairFgGameInputs(f.world,f.ui);
    REQUIRE(std::holds_alternative<rk::FgGameInputCandidate>(result));
    const auto& packet=std::get<rk::FgGameInputCandidate>(result);
    REQUIRE(packet.frame.source==33);
    REQUIRE(packet.frame.presentToken==9000);
    REQUIRE(packet.frame.depth.ready);
    REQUIRE(packet.frame.motion.ready);
    REQUIRE(packet.frame.hudless.ready);
    REQUIRE(packet.frame.uiColorAlpha.ready);
    const auto sources=packet.sources();
    REQUIRE(sources.textures[0]==f.ui.finalColor.Get());
    REQUIRE(sources.textures[1]==f.world.depth.Get());
    REQUIRE(sources.textures[2]==f.world.motion.Get());
    REQUIRE(sources.textures[3]==f.ui.hudless.Get());
    REQUIRE(sources.textures[4]==f.ui.uiColorAlpha.Get());
}

TEST_CASE("FG game input pairing rejects stale identity and wrong guide formats",
    "[fg_game_inputs]") {
    Fixture f;
    f.ui.presentToken++;
    REQUIRE(std::holds_alternative<rk::Error>(
        rk::pairFgGameInputs(f.world,f.ui)));
    f.ui.presentToken=f.world.frame.presentToken;
    f.world.depth=texture(f.device.Get(),f.world.frame.render,
        DXGI_FORMAT_R24G8_TYPELESS);
    REQUIRE(std::holds_alternative<rk::Error>(
        rk::pairFgGameInputs(f.world,f.ui)));
    f.world.depth=texture(f.device.Get(),f.world.frame.render,
        DXGI_FORMAT_R32_FLOAT);
    f.world.hudless.Reset();
    REQUIRE(std::holds_alternative<rk::Error>(
        rk::pairFgGameInputs(f.world,f.ui)));
    f.world.hudless=texture(f.device.Get(),f.world.frame.display,
        DXGI_FORMAT_R8G8B8A8_UNORM);
    f.ui.hudless=f.ui.uiColorAlpha;
    REQUIRE(std::holds_alternative<rk::Error>(
        rk::pairFgGameInputs(f.world,f.ui)));
}

TEST_CASE("FG paired game inputs enter and retire a five-surface WARP lease",
    "[fg_game_inputs]") {
    Fixture f;
    ComPtr<IDXGIFactory4> factory;
    REQUIRE(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))));
    ComPtr<IDXGIAdapter> adapter;
    REQUIRE(SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter))));
    ComPtr<ID3D12Device> d12;
    REQUIRE(SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&d12))));
    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    ComPtr<ID3D12CommandQueue> queue;
    REQUIRE(SUCCEEDED(d12->CreateCommandQueue(&queueDesc,IID_PPV_ARGS(&queue))));
    auto bridgeResult=rk::FgSharedInputs::create(f.device.Get(),
        d12.Get(),queue.Get());
    REQUIRE(std::holds_alternative<std::unique_ptr<rk::FgSharedInputs>>(
        bridgeResult));
    auto bridge=std::move(std::get<std::unique_ptr<rk::FgSharedInputs>>(
        bridgeResult));
    auto paired=rk::pairFgGameInputs(f.world,f.ui);
    REQUIRE(std::holds_alternative<rk::FgGameInputCandidate>(paired));
    auto packet=std::get<rk::FgGameInputCandidate>(std::move(paired));
    rk::FgInputLeaseRing ring(*bridge,packet.frame.generation);
    auto prepared=ring.prepare(packet.frame,packet.sources(),f.context.Get(),
        {packet.frame.generation,0,0,0,0,0});
    REQUIRE(std::holds_alternative<rk::FgInputLease>(prepared));
    auto lease=std::get<rk::FgInputLease>(std::move(prepared));
    REQUIRE(lease.source==packet.frame.source);
    REQUIRE(lease.presentToken==packet.frame.presentToken);
    for(const auto& resource:lease.resources)REQUIRE(resource!=nullptr);
    auto providerFrame=packet.frame;
    providerFrame.cameraValid=true;
    REQUIRE(std::holds_alternative<rk::FgPreparedSubmission>(
        rk::prepareFgSubmission(providerFrame,lease,f.ui,
            camera(providerFrame),0,2)));
    REQUIRE(bridge->waitCopy(lease.lastCopy.copy));
    REQUIRE(ring.discard(lease));
    REQUIRE(ring.stop({packet.frame.generation,0,0,0,0,0}));
}

TEST_CASE("FG game probe completes the sampled five-input copy without provider use",
    "[fg_game_inputs]") {
    Fixture f;
    auto paired=rk::pairFgGameInputs(f.world,f.ui);
    REQUIRE(std::holds_alternative<rk::FgGameInputCandidate>(paired));
    auto packet=std::get<rk::FgGameInputCandidate>(std::move(paired));
    REQUIRE(std::holds_alternative<rk::Error>(rk::FgGameInputProbe::begin(
        nullptr,f.context.Get(),packet)));
    auto started=rk::FgGameInputProbe::begin(f.device.Get(),
        f.context.Get(),packet);
    REQUIRE(std::holds_alternative<std::unique_ptr<rk::FgGameInputProbe>>(
        started));
    auto probe=std::move(std::get<std::unique_ptr<rk::FgGameInputProbe>>(
        started));
    REQUIRE(probe->source()==packet.frame.source);
    REQUIRE(probe->presentToken()==packet.frame.presentToken);
    REQUIRE(std::holds_alternative<rk::FgPreparedSubmission>(
        probe->inspectPrepared(packet,f.ui,camera(packet.frame),0,2)));
    auto altered=packet;
    altered.textures[3]=f.world.hudless;
    REQUIRE(std::holds_alternative<rk::Error>(
        probe->inspectPrepared(altered,f.ui,camera(packet.frame),0,2)));
    auto staleCamera=camera(packet.frame);
    staleCamera.source++;
    REQUIRE(std::holds_alternative<rk::Error>(
        probe->inspectPrepared(packet,f.ui,staleCamera,0,2)));
    REQUIRE(std::holds_alternative<rk::Error>(
        probe->enqueue(f.context.Get(),packet)));
    const auto deadline=std::chrono::steady_clock::now()+
        std::chrono::seconds(5);
    auto status=probe->poll();
    while(status==rk::FgGameCopyState::Pending&&
          std::chrono::steady_clock::now()<deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        status=probe->poll();
    }
    REQUIRE(status==rk::FgGameCopyState::Complete);
    REQUIRE(std::holds_alternative<rk::Error>(
        probe->inspectPrepared(packet,f.ui,camera(packet.frame),0,2)));
    REQUIRE(probe->poll()==rk::FgGameCopyState::Complete);
    f.world.frame.source=34;
    f.world.frame.presentToken=9001;
    f.ui.source=34;
    f.ui.presentToken=9001;
    f.ui.hudlessStamp.source=34;
    f.ui.uiStamp.source=34;
    f.ui.finalStamp.source=34;
    auto nextPair=rk::pairFgGameInputs(f.world,f.ui);
    REQUIRE(std::holds_alternative<rk::FgGameInputCandidate>(nextPair));
    const auto& next=std::get<rk::FgGameInputCandidate>(nextPair);
    const auto enqueued=probe->enqueue(f.context.Get(),next);
    REQUIRE(std::holds_alternative<rk::FgCopyTicket>(enqueued));
    REQUIRE(std::get<rk::FgCopyTicket>(enqueued).producer==2);
    REQUIRE(std::get<rk::FgCopyTicket>(enqueued).copy==2);
    status=probe->poll();
    while(status==rk::FgGameCopyState::Pending&&
          std::chrono::steady_clock::now()<deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        status=probe->poll();
    }
    REQUIRE(status==rk::FgGameCopyState::Complete);
    REQUIRE(probe->source()==34);
    REQUIRE(probe->presentToken()==9001);
    REQUIRE(probe->close());
    REQUIRE(std::holds_alternative<rk::Error>(
        probe->enqueue(f.context.Get(),next)));
}

TEST_CASE("FG game copy binds the exact presentation device and direct queue",
    "[fg_game_inputs]") {
    Fixture f;
    ComPtr<IDXGIDevice> dxgi;
    ComPtr<IDXGIAdapter> adapter;
    REQUIRE(SUCCEEDED(f.device.As(&dxgi)));
    REQUIRE(SUCCEEDED(dxgi->GetAdapter(&adapter)));
    ComPtr<ID3D12Device> d12;
    REQUIRE(SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&d12))));
    D3D12_COMMAND_QUEUE_DESC desc{};
    desc.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
    ComPtr<ID3D12CommandQueue> queue;
    REQUIRE(SUCCEEDED(d12->CreateCommandQueue(&desc,IID_PPV_ARGS(&queue))));
    const auto paired=rk::pairFgGameInputs(f.world,f.ui);
    REQUIRE(std::holds_alternative<rk::FgGameInputCandidate>(paired));
    const auto& packet=std::get<rk::FgGameInputCandidate>(paired);
    auto lifetime=std::make_shared<int>(1);
    std::weak_ptr<int> weak=lifetime;
    REQUIRE(std::holds_alternative<rk::Error>(
        rk::FgGameInputProbe::beginOnOwner(f.device.Get(),f.context.Get(),
            nullptr,queue.Get(),lifetime,packet)));
    desc.Type=D3D12_COMMAND_LIST_TYPE_COPY;
    ComPtr<ID3D12CommandQueue> copyQueue;
    REQUIRE(SUCCEEDED(d12->CreateCommandQueue(&desc,IID_PPV_ARGS(&copyQueue))));
    REQUIRE(std::holds_alternative<rk::Error>(
        rk::FgGameInputProbe::beginOnOwner(f.device.Get(),f.context.Get(),
            d12.Get(),copyQueue.Get(),lifetime,packet)));
    auto started=rk::FgGameInputProbe::beginOnOwner(f.device.Get(),
        f.context.Get(),d12.Get(),queue.Get(),lifetime,packet);
    REQUIRE(std::holds_alternative<std::unique_ptr<rk::FgGameInputProbe>>(started));
    auto probe=std::move(std::get<std::unique_ptr<rk::FgGameInputProbe>>(started));
    lifetime.reset();
    REQUIRE_FALSE(weak.expired());
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    auto status=probe->poll();
    while(status==rk::FgGameCopyState::Pending&&
          std::chrono::steady_clock::now()<deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        status=probe->poll();
    }
    REQUIRE(status==rk::FgGameCopyState::Complete);
    REQUIRE(probe->close());
    probe.reset();
    REQUIRE(weak.expired());

    // A resize/destructor may abandon a copy whose exact queue is still
    // blocked. The ring must quarantine the runtime with its GPU owners.
    ComPtr<ID3D12Fence> gate;
    REQUIRE(SUCCEEDED(d12->CreateFence(0,D3D12_FENCE_FLAG_NONE,
        IID_PPV_ARGS(&gate))));
    REQUIRE(SUCCEEDED(queue->Wait(gate.Get(),1)));
    struct Unblock { ID3D12Fence* fence;~Unblock(){fence->Signal(1);} } unblock{gate.Get()};
    lifetime=std::make_shared<int>(2);
    weak=lifetime;
    const auto prior=rk::FgInputLeaseRing::quarantinedOwners();
    started=rk::FgGameInputProbe::beginOnOwner(f.device.Get(),f.context.Get(),
        d12.Get(),queue.Get(),lifetime,packet);
    REQUIRE(std::holds_alternative<std::unique_ptr<rk::FgGameInputProbe>>(started));
    probe=std::move(std::get<std::unique_ptr<rk::FgGameInputProbe>>(started));
    REQUIRE(probe->poll()==rk::FgGameCopyState::Pending);
    REQUIRE_FALSE(probe->close());
    lifetime.reset();
    probe.reset();
    REQUIRE(rk::FgInputLeaseRing::quarantinedOwners()==prior+1);
    REQUIRE_FALSE(weak.expired());
    REQUIRE(SUCCEEDED(gate->Signal(1)));
}
