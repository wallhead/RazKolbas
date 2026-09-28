#include <sl.h>
#include <sl_dlss_g.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <cstdint>
#include <iostream>

using Microsoft::WRL::ComPtr;

namespace {
int code(sl::Result result) { return static_cast<int>(result); }
}

int wmain(int argc,wchar_t** argv) {
    if(argc!=2) {
        std::wcerr<<L"Usage: RazKolbasFgStreamlineProbe <absolute SDK bin/x64>\n";
        return 1;
    }
    const wchar_t* pluginPaths[]{argv[1]};
    const sl::Feature features[]{sl::kFeatureDLSS_G,sl::kFeatureReflex};
    sl::Preferences preferences{};
    preferences.pathsToPlugins=pluginPaths;
    preferences.numPathsToPlugins=1;
    preferences.featuresToLoad=features;
    preferences.numFeaturesToLoad=2;
    preferences.flags=sl::PreferenceFlags::eDisableCLStateTracking|
        sl::PreferenceFlags::eUseManualHooking|
        sl::PreferenceFlags::eUseFrameBasedResourceTagging;
    preferences.engine=sl::EngineType::eCustom;
    preferences.engineVersion="0.1.115";
    preferences.projectId="b3340e44-a57e-4b98-9318-d7150829d110";
    preferences.renderAPI=sl::RenderAPI::eD3D12;
    const auto initialized=slInit(preferences);
    std::cout<<"slInit="<<code(initialized)<<'\n';
    if(initialized!=sl::Result::eOk)return 2;

    int exitCode=0;
    // SL shutdown precedes destruction of every DXGI/D3D12 interface.
    ComPtr<IDXGIFactory6> factory;
    ComPtr<IDXGIAdapter1> adapter;
    ComPtr<ID3D12Device> device;
    {
        if(FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))exitCode=3;
        DXGI_ADAPTER_DESC1 desc{};
        if(!exitCode) {
            for(UINT index=0;factory->EnumAdapterByGpuPreference(index,
                DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                IID_PPV_ARGS(&adapter))!=DXGI_ERROR_NOT_FOUND;++index) {
                if(SUCCEEDED(adapter->GetDesc1(&desc))&&desc.VendorId==0x10de)break;
                adapter.Reset();
            }
            if(!adapter)exitCode=4;
        }
        if(!exitCode) {
            std::cout<<"adapterVendor=0x"<<std::hex<<desc.VendorId
                <<" device=0x"<<desc.DeviceId<<std::dec<<'\n';
            sl::AdapterInfo info{};
            info.deviceLUID=reinterpret_cast<std::uint8_t*>(&desc.AdapterLuid);
            info.deviceLUIDSizeInBytes=sizeof(desc.AdapterLuid);
            const auto support=slIsFeatureSupported(sl::kFeatureDLSS_G,info);
            std::cout<<"slIsFeatureSupported(DLSS-G)="<<code(support)<<'\n';
            sl::FeatureRequirements requirements{};
            const auto queried=slGetFeatureRequirements(sl::kFeatureDLSS_G,
                requirements);
            std::cout<<"slGetFeatureRequirements(DLSS-G)="<<code(queried);
            if(queried==sl::Result::eOk)
                std::cout<<" flags=0x"<<std::hex<<
                    static_cast<std::uint32_t>(requirements.flags)<<std::dec;
            std::cout<<'\n';
            const auto created=D3D12CreateDevice(adapter.Get(),
                D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device));
            std::cout<<"D3D12CreateDevice=0x"<<std::hex<<
                static_cast<std::uint32_t>(created)<<std::dec<<'\n';
            if(FAILED(created))exitCode=5;
            else {
                const auto bound=slSetD3DDevice(device.Get());
                std::cout<<"slSetD3DDevice="<<code(bound)<<'\n';
                if(bound!=sl::Result::eOk)exitCode=6;
            }
            if(support!=sl::Result::eOk||queried!=sl::Result::eOk)
                if(!exitCode)exitCode=7;
        }
    }
    const auto stopped=slShutdown();
    std::cout<<"slShutdown="<<code(stopped)<<'\n';
    if(stopped!=sl::Result::eOk&&exitCode==0)exitCode=8;
    return exitCode;
}
