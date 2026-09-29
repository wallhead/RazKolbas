#include <catch2/catch_test_macros.hpp>
#include "rk/FgSubmission.hpp"
#include <dxgi1_6.h>
#include <limits>
#include <variant>

using Microsoft::WRL::ComPtr;

namespace {
struct Warp {
    ComPtr<IDXGIAdapter> adapter;
    ComPtr<ID3D11Device> d11;
    ComPtr<ID3D12Device> d12;
    Warp() {
        ComPtr<IDXGIFactory4> factory;
        REQUIRE(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))));
        REQUIRE(SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter))));
        D3D_FEATURE_LEVEL level{};
        REQUIRE(SUCCEEDED(D3D11CreateDevice(adapter.Get(),D3D_DRIVER_TYPE_UNKNOWN,
            nullptr,0,nullptr,0,D3D11_SDK_VERSION,&d11,&level,nullptr)));
        REQUIRE(SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&d12))));
    }
    ComPtr<ID3D12Resource> resource(rk::Extent size) {
        D3D12_RESOURCE_DESC desc{};
        desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width=size.width;desc.Height=size.height;
        desc.DepthOrArraySize=1;desc.MipLevels=1;
        desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count=1;
        D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;
        ComPtr<ID3D12Resource> result;
        REQUIRE(SUCCEEDED(d12->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,
            &desc,D3D12_RESOURCE_STATE_COMMON,nullptr,IID_PPV_ARGS(&result))));
        return result;
    }
    ComPtr<ID3D11Texture2D> uiTexture() {
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width=64;desc.Height=40;desc.MipLevels=1;desc.ArraySize=1;
        desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;
        ComPtr<ID3D11Texture2D> result;
        REQUIRE(SUCCEEDED(d11->CreateTexture2D(&desc,nullptr,&result)));
        return result;
    }
};
rk::FgSourceFrame frame() {
    rk::FgSourceFrame result{};
    result.source=17;result.generation=3;result.presentToken=41;
    result.resetEpoch=9;result.render={32,20};result.display={64,40};
    result.cameraValid=true;
    result.color={17,3,result.display,true,9};
    result.depth={17,3,result.render,true,9};
    result.motion={17,3,result.render,true,9};
    result.hudless={17,3,result.display,true,9};
    result.uiColorAlpha={17,3,result.display,true,9};
    return result;
}
rk::FgCameraData camera() {
    rk::FgCameraData result{};
    result.source=17;result.generation=3;result.presentToken=41;
    result.resetEpoch=9;result.sampleRevision=72;
    for(auto* matrix:{&result.viewToClip,&result.clipToView,
        &result.clipToPrevClip,&result.prevClipToClip})
        for(unsigned i=0;i<4;++i)(*matrix)[i*4+i]=1.0f;
    result.up={0,1,0};result.right={1,0,0};
    result.forward={0,0,1};result.mvecScale={1.0f/32,1.0f/20};
    result.nearPlane=0.1f;result.farPlane=100.0f;
    result.fovRadians=1.0f;result.aspectRatio=1.6f;
    return result;
}
rk::FgInputLease lease(Warp& gpu,const rk::FgSourceFrame& source,
    const rk::FgUiPlaneFrame& planes) {
    rk::FgInputLease result{};
    result.source=source.source;result.generation=source.generation;
    result.presentToken=source.presentToken;result.resetEpoch=source.resetEpoch;
    result.serial=1;result.lastCopy={2,3};
    for(std::size_t i=0;i<result.resources.size();++i)
        result.resources[i]=gpu.resource((i==1||i==2)?source.render:
            source.display);
    result.sourceTextures[0]=planes.finalColor;
    result.sourceTextures[3]=planes.hudless;
    result.sourceTextures[4]=planes.uiColorAlpha;
    return result;
}
rk::FgUiPlaneFrame ui(Warp& gpu,const rk::FgSourceFrame& source) {
    rk::FgUiPlaneFrame result{};
    result.source=source.source;result.generation=source.generation;
    result.presentToken=source.presentToken;result.resetEpoch=source.resetEpoch;
    result.display=source.display;result.uiRegion={0,0,64,40};
    result.hudlessStamp=source.hudless;
    result.uiStamp=source.uiColorAlpha;
    result.finalStamp=source.color;
    result.hudless=gpu.uiTexture();
    result.uiColorAlpha=gpu.uiTexture();
    result.finalColor=gpu.uiTexture();
    return result;
}
}

TEST_CASE("FG submission binds the exact same-frame resources and camera",
    "[fg_submission]") {
    Warp gpu;
    const auto source=frame();
    const auto planes=ui(gpu,source);
    const auto input=lease(gpu,source,planes);
    const auto view=camera();
    const auto prepared=rk::prepareFgSubmission(source,input,planes,view,1,2);
    REQUIRE(std::holds_alternative<rk::FgPreparedSubmission>(prepared));
    const auto& record=std::get<rk::FgPreparedSubmission>(prepared);
    REQUIRE(record.source==17);
    REQUIRE(record.presentToken==41);
    REQUIRE(record.resetEpoch==9);
    REQUIRE(record.copyTicket.copy==3);
    REQUIRE(record.physicalOutputIndex==1);
    REQUIRE(record.resources[4].Get()==input.resources[4].Get());
    REQUIRE(record.camera.mvecScale==view.mvecScale);
    REQUIRE(record.camera.sampleRevision==72);
}

TEST_CASE("FG submission accepts a noncommuting temporal transform and inverse",
    "[fg_submission]") {
    Warp gpu;
    const auto source=frame();
    const auto planes=ui(gpu,source);
    const auto input=lease(gpu,source,planes);
    auto view=camera();
    view.clipToPrevClip={2,0,0,0, 0,3,0,0, 0,0,1,0, 4,5,0,1};
    view.prevClipToClip={0.5f,0,0,0, 0,1.0f/3,0,0,
        0,0,1,0, -2,-5.0f/3,0,1};
    REQUIRE(std::holds_alternative<rk::FgPreparedSubmission>(
        rk::prepareFgSubmission(source,input,planes,view,1,2)));
}

TEST_CASE("FG submission rejects altered token, UI, guide and camera",
    "[fg_submission]") {
    Warp gpu;
    const auto source=frame();
    const auto planes=ui(gpu,source);
    const auto input=lease(gpu,source,planes);
    const auto view=camera();
    unsigned caseIndex=0;
    auto rejected=[&](const rk::FgInputLease& l,const rk::FgUiPlaneFrame& u,
        const rk::FgCameraData& c,unsigned index=0,unsigned count=2) {
        INFO("negative case "<<++caseIndex);
        REQUIRE(std::holds_alternative<rk::Error>(
            rk::prepareFgSubmission(source,l,u,c,index,count)));
    };
    auto stale=input;stale.presentToken++;
    rejected(stale,planes,view);
    stale=input;stale.resources[2].Reset();
    rejected(stale,planes,view);
    stale=input;stale.resources[1]=gpu.resource(source.display);
    rejected(stale,planes,view);
    stale=input;stale.lastCopy.copy=0;
    rejected(stale,planes,view);
    stale=input;stale.sourceTextures[4]=planes.finalColor;
    rejected(stale,planes,view);
    auto oldUi=planes;oldUi.resetEpoch++;
    rejected(input,oldUi,view);
    oldUi=planes;oldUi.uiStamp.source++;
    rejected(input,oldUi,view);
    oldUi=planes;oldUi.uiColorAlpha.Reset();
    rejected(input,oldUi,view);
    auto badCamera=view;badCamera.mvecScale[0]=0;
    rejected(input,planes,badCamera);
    badCamera=view;badCamera.viewToClip[0]=std::numeric_limits<float>::quiet_NaN();
    rejected(input,planes,badCamera);
    badCamera=view;badCamera.clipToPrevClip[12]=1.0f;
    rejected(input,planes,badCamera);
    badCamera=view;badCamera.source++;
    rejected(input,planes,badCamera);
    badCamera=view;badCamera.generation++;
    rejected(input,planes,badCamera);
    badCamera=view;badCamera.presentToken++;
    rejected(input,planes,badCamera);
    badCamera=view;badCamera.resetEpoch++;
    rejected(input,planes,badCamera);
    badCamera=view;badCamera.sampleRevision=0;
    rejected(input,planes,badCamera);
    rejected(input,planes,view,2,2);
}
