#include <catch2/catch_test_macros.hpp>
#include "rk/FgInputLeaseRing.hpp"
#include <dxgi1_6.h>
#include <array>
#include <variant>

using Microsoft::WRL::ComPtr;

namespace {
struct WarpDevices {
    ComPtr<IDXGIAdapter> adapter;
    ComPtr<ID3D11Device> d11;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<ID3D12Device> d12;
    ComPtr<ID3D12CommandQueue> queue;
    std::unique_ptr<rk::FgSharedInputs> bridge;
    WarpDevices() {
        ComPtr<IDXGIFactory4> factory;
        REQUIRE(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))));
        REQUIRE(SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter))));
        D3D_FEATURE_LEVEL level{};
        REQUIRE(SUCCEEDED(D3D11CreateDevice(adapter.Get(),D3D_DRIVER_TYPE_UNKNOWN,
            nullptr,0,nullptr,0,D3D11_SDK_VERSION,&d11,&level,&context)));
        REQUIRE(SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&d12))));
        D3D12_COMMAND_QUEUE_DESC desc{};
        REQUIRE(SUCCEEDED(d12->CreateCommandQueue(&desc,IID_PPV_ARGS(&queue))));
        auto created=rk::FgSharedInputs::create(d11.Get(),d12.Get(),queue.Get());
        REQUIRE(std::holds_alternative<std::unique_ptr<rk::FgSharedInputs>>(created));
        bridge=std::move(std::get<std::unique_ptr<rk::FgSharedInputs>>(created));
    }
};
rk::FgSourceFrame frame(std::uint64_t source,std::uint64_t generation=7) {
    rk::FgSourceFrame value{};
    value.source=source;value.generation=generation;value.presentToken=9000+source;
    value.resetEpoch=3;value.render={19,11};value.display={37,19};
    value.color={source,generation,value.display,true,3};
    value.depth={source,generation,value.render,true,3};
    value.motion={source,generation,value.render,true,3};
    value.hudless={source,generation,value.display,true,3};
    value.uiColorAlpha={source,generation,value.display,true,3};
    return value;
}
ComPtr<ID3D11Texture2D> texture(ID3D11Device* device,rk::Extent size) {
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width=size.width;desc.Height=size.height;desc.MipLevels=1;
    desc.ArraySize=1;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_DEFAULT;
    desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    ComPtr<ID3D11Texture2D> made;
    REQUIRE(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&made)));
    return made;
}
struct SourceTextures {
    std::array<ComPtr<ID3D11Texture2D>,5> owned;
    rk::FgInputSources inputs{};
    SourceTextures(ID3D11Device* device,const rk::FgSourceFrame& f) {
        for(std::size_t i=0;i<owned.size();++i) {
            owned[i]=texture(device,(i==1||i==2)?f.render:f.display);
            inputs.textures[i]=owned[i].Get();
        }
    }
};
}

TEST_CASE("FG five-input GPU lease waits for every consumer before reuse",
    "[fg_input_lease]") {
    WarpDevices gpu;
    rk::FgInputLeaseRing ring(*gpu.bridge,7);
    const auto firstFrame=frame(1);
    SourceTextures sources(gpu.d11.Get(),firstFrame);
    auto prepared=ring.prepare(firstFrame,sources.inputs,gpu.context.Get(),
        {7,0,0,0,0,0});
    REQUIRE(std::holds_alternative<rk::FgInputLease>(prepared));
    auto first=std::get<rk::FgInputLease>(std::move(prepared));
    for(const auto& input:first.resources)REQUIRE(input!=nullptr);
    // All five inputs from one real frame must share one producer/consumer
    // synchronization pair before this path can run every frame.
    REQUIRE(first.lastCopy.producer==1);
    REQUIRE(first.lastCopy.copy==1);
    REQUIRE(gpu.bridge->waitCopy(first.lastCopy.copy));
    REQUIRE_FALSE(ring.submit(first,{7,first.lastCopy.producer,
        first.lastCopy.copy,0,2,3}));
    REQUIRE(ring.submit(first,{7,first.lastCopy.producer,
        first.lastCopy.copy,1,2,3}));
    REQUIRE_FALSE(ring.submit(first,{7,first.lastCopy.producer,
        first.lastCopy.copy,1,2,3}));
    for(std::uint64_t id=2;id<=3;++id) {
        auto next=ring.prepare(frame(id),sources.inputs,gpu.context.Get(),
            {7,first.lastCopy.producer,first.lastCopy.copy,0,0,0});
        REQUIRE(std::holds_alternative<rk::FgInputLease>(next));
        const auto lease=std::get<rk::FgInputLease>(std::move(next));
        REQUIRE(ring.submit(lease,{7,lease.lastCopy.producer,
            lease.lastCopy.copy,1,2,3}));
    }
    REQUIRE(std::holds_alternative<rk::Error>(ring.prepare(frame(4),
        sources.inputs,gpu.context.Get(),{7,100,100,0,2,3})));
    REQUIRE_FALSE(ring.advanceGeneration(8,{7,100,100,0,2,3}));
    REQUIRE(ring.advanceGeneration(8,{7,100,100,1,2,3}));
    auto resized=frame(4,8);
    SourceTextures newSources(gpu.d11.Get(),resized);
    auto next=ring.prepare(resized,newSources.inputs,gpu.context.Get(),
        {8,0,0,0,0,0});
    REQUIRE(std::holds_alternative<rk::FgInputLease>(next));
    const auto newLease=std::get<rk::FgInputLease>(std::move(next));
    REQUIRE(newLease.generation==8);
    REQUIRE_FALSE(ring.submit(first,{8,100,100,1,2,3}));
    REQUIRE(ring.discard(newLease));
}

TEST_CASE("FG input lease rejects stale stamps and mismatched guide sizes",
    "[fg_input_lease]") {
    WarpDevices gpu;
    rk::FgInputLeaseRing ring(*gpu.bridge,7);
    auto input=frame(1);
    SourceTextures sources(gpu.d11.Get(),input);
    input.motion.source=2;
    REQUIRE(std::holds_alternative<rk::Error>(ring.prepare(input,sources.inputs,
        gpu.context.Get(),{7,0,0,0,0,0})));
    input=frame(1);
    sources.owned[2]=texture(gpu.d11.Get(),input.display);
    sources.inputs.textures[2]=sources.owned[2].Get();
    REQUIRE(std::holds_alternative<rk::Error>(ring.prepare(input,sources.inputs,
        gpu.context.Get(),{7,0,0,0,0,0})));
    REQUIRE(ring.advanceGeneration(8,{7,0,0,0,0,0}));
}

TEST_CASE("FG copied inputs remain bound to their exact real frame",
    "[fg_input_lease]") {
    WarpDevices gpu;
    rk::FgInputLeaseRing ring(*gpu.bridge,7);
    auto input=frame(1);
    SourceTextures sources(gpu.d11.Get(),input);
    input.presentToken=0;
    REQUIRE(std::holds_alternative<rk::Error>(ring.prepare(input,
        sources.inputs,gpu.context.Get(),{7,0,0,0,0,0})));
    input=frame(1);
    auto result=ring.prepare(input,sources.inputs,gpu.context.Get(),
        {7,0,0,0,0,0});
    REQUIRE(std::holds_alternative<rk::FgInputLease>(result));
    const auto lease=std::get<rk::FgInputLease>(std::move(result));
    REQUIRE(lease.presentToken==input.presentToken);
    REQUIRE(lease.resetEpoch==input.resetEpoch);
    const rk::FgRetirementSet retirement{7,lease.lastCopy.producer,
        lease.lastCopy.copy,1,2,3};
    auto changed=lease;
    changed.source++;
    REQUIRE_FALSE(ring.submit(changed,retirement));
    changed=lease;
    changed.presentToken++;
    REQUIRE_FALSE(ring.submit(changed,retirement));
    changed=lease;
    changed.resetEpoch++;
    REQUIRE_FALSE(ring.submit(changed,retirement));
    changed=lease;
    changed.lastCopy.copy++;
    REQUIRE_FALSE(ring.submit(changed,retirement));
    changed=lease;
    changed.resources[1]=lease.resources[0];
    REQUIRE_FALSE(ring.submit(changed,retirement));
    changed=lease;
    changed.sourceTextures[4]=lease.sourceTextures[0];
    REQUIRE_FALSE(ring.submit(changed,retirement));
    REQUIRE(gpu.bridge->waitCopy(lease.lastCopy.copy));
    REQUIRE(ring.submit(lease,retirement));
    REQUIRE(ring.stop({7,retirement.producer,retirement.copy,
        retirement.providerInput,retirement.present,retirement.allocator}));
}

TEST_CASE("FG lease shutdown drains completed copies and quarantines partial copies",
    "[fg_input_lease]") {
    WarpDevices gpu;
    const auto before=rk::FgInputLeaseRing::quarantinedOwners();
    const auto input=frame(1);
    SourceTextures sources(gpu.d11.Get(),input);
    {
        auto ring=std::make_unique<rk::FgInputLeaseRing>(*gpu.bridge,7);
        const auto prepared=ring->prepare(input,sources.inputs,gpu.context.Get(),
            {7,0,0,0,0,0});
        REQUIRE(std::holds_alternative<rk::FgInputLease>(prepared));
        const auto lease=std::get<rk::FgInputLease>(prepared);
        REQUIRE(ring->discard(lease));
        REQUIRE(ring->stop({7,0,0,0,0,0}));
    }
    REQUIRE(rk::FgInputLeaseRing::quarantinedOwners()==before);

    ComPtr<ID3D11Device> foreign;
    ComPtr<ID3D11DeviceContext> foreignContext;
    D3D_FEATURE_LEVEL level{};
    REQUIRE(SUCCEEDED(D3D11CreateDevice(gpu.adapter.Get(),
        D3D_DRIVER_TYPE_UNKNOWN,nullptr,0,nullptr,0,D3D11_SDK_VERSION,
        &foreign,&level,&foreignContext)));
    sources.owned[1]=texture(foreign.Get(),input.render);
    sources.inputs.textures[1]=sources.owned[1].Get();
    {
        auto ring=std::make_unique<rk::FgInputLeaseRing>(*gpu.bridge,7);
        REQUIRE(std::holds_alternative<rk::Error>(ring->prepare(input,
            sources.inputs,gpu.context.Get(),{7,0,0,0,0,0})));
        REQUIRE(ring->failed());
        REQUIRE_FALSE(ring->stop({7,UINT64_MAX,UINT64_MAX,0,0,0}));
        REQUIRE_FALSE(ring->advanceGeneration(8,{7,100,100,100,100,100}));
    }
    REQUIRE(rk::FgInputLeaseRing::quarantinedOwners()==before+1);

    SourceTextures validSources(gpu.d11.Get(),input);
    {
        auto ring=std::make_unique<rk::FgInputLeaseRing>(*gpu.bridge,7);
        const auto prepared=ring->prepare(input,validSources.inputs,
            gpu.context.Get(),{7,0,0,0,0,0});
        REQUIRE(std::holds_alternative<rk::FgInputLease>(prepared));
        const auto lease=std::get<rk::FgInputLease>(prepared);
        REQUIRE(gpu.bridge->waitCopy(lease.lastCopy.copy));
        REQUIRE(ring->submit(lease,{7,lease.lastCopy.producer,
            lease.lastCopy.copy,9,10,11}));
        REQUIRE_FALSE(ring->stop({7,lease.lastCopy.producer,
            lease.lastCopy.copy,8,10,11}));
    }
    REQUIRE(rk::FgInputLeaseRing::quarantinedOwners()==before+2);
}
