// Exercises the exact game-device offscreen probe with captured Skyrim bytes
// in an isolated process. Never loads a reference host DLL or Skyrim.
#include "rk/OffscreenDlssProbe.hpp"
#include "rk/SrInput.hpp"
#include <Windows.h>
#include <d3d11.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

using Microsoft::WRL::ComPtr;
namespace fs=std::filesystem;
namespace {
constexpr UINT width=2560,height=1440;
[[noreturn]] void stop(const std::string& reason) {
    std::cerr<<"LIVE_PATH_REPLAY_STOP="<<reason<<std::endl;
    ExitProcess(10); // Uncertain NGX/GPU owners remain until child exit.
}
void checked(HRESULT result,const char* step) { if(FAILED(result))stop(step); }
std::vector<std::uint8_t> read(const fs::path& path,std::size_t count) {
    if(fs::file_size(path)!=count)stop("CAPTURE_EXTENT");
    std::vector<std::uint8_t> bytes(count);
    std::ifstream file(path,std::ios::binary);
    if(!file.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(count)))stop("CAPTURE_READ");
    return bytes;
}
ComPtr<ID3D11Texture2D> texture(ID3D11Device* device,DXGI_FORMAT format,
    UINT pixelBytes,UINT bind,const void* data) {
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width=width;desc.Height=height;desc.MipLevels=desc.ArraySize=1;
    desc.Format=format;desc.SampleDesc.Count=1;desc.BindFlags=bind;
    const D3D11_SUBRESOURCE_DATA initial{data,width*pixelBytes,0};
    ComPtr<ID3D11Texture2D> result;
    checked(device->CreateTexture2D(&desc,&initial,&result),"TEXTURE_CREATE");
    return result;
}
}
int wmain(int argc,wchar_t** argv) {
    if(argc!=2) { std::cerr<<"Usage: RazKolbasSrLivePathReplay <verified capture directory>\n";return 2; }
    try {
        const fs::path capture=fs::absolute(argv[1]);
        const auto colorBytes=read(capture/L"main-colour-candidate.raw",width*height*8);
        const auto motionBytes=read(capture/L"motion-candidate.raw",width*height*4);
        const auto depthBytes=read(capture/L"depth-candidate.raw",width*height*4);
        ComPtr<IDXGIFactory6> factory;
        checked(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory)),"FACTORY_CREATE");
        ComPtr<IDXGIAdapter1> adapter;
        for(UINT i=0;;++i) {
            ComPtr<IDXGIAdapter1> candidate;
            const auto result=factory->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                IID_PPV_ARGS(&candidate));
            if(result==DXGI_ERROR_NOT_FOUND)break;
            checked(result,"ADAPTER_ENUM");
            DXGI_ADAPTER_DESC1 desc{};
            checked(candidate->GetDesc1(&desc),"ADAPTER_DESC");
            if(desc.VendorId==0x10de&&!(desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)) {
                adapter=candidate;break;
            }
        }
        if(!adapter)stop("NVIDIA_ADAPTER_MISSING");
        ComPtr<ID3D11Device> device;
        ComPtr<ID3D11DeviceContext> context;
        checked(D3D11CreateDevice(adapter.Get(),D3D_DRIVER_TYPE_UNKNOWN,nullptr,0,
            nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context),"DEVICE_CREATE");
        const auto color=texture(device.Get(),DXGI_FORMAT_R16G16B16A16_FLOAT,8,
            D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET,colorBytes.data());
        const auto motion=texture(device.Get(),DXGI_FORMAT_R16G16_FLOAT,4,
            D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET,motionBytes.data());
        const auto depth=texture(device.Get(),DXGI_FORMAT_R24G8_TYPELESS,4,
            D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_DEPTH_STENCIL,depthBytes.data());
        const std::array<ID3D11Texture2D*,3> sources{color.Get(),motion.Get(),depth.Get()};
        auto prepared=rk::prepareSrInputs(context.Get(),sources);
        if(const auto error=std::get_if<rk::Error>(&prepared))stop(error->message);
        auto owned=std::move(std::get<rk::PreparedSrInputs>(prepared));
        rk::OffscreenDlssProbe probe;
        const auto begun=probe.begin(device.Get(),context.Get(),owned);
        if(const auto error=std::get_if<rk::Error>(&begun))stop(error->message);
        std::cout<<"LIVE_PATH_EVALUATE_SUBMITTED=1"<<std::endl;
        const auto start=GetTickCount64();
        for(;;) {
            const auto polled=probe.poll(device.Get(),context.Get());
            if(const auto error=std::get_if<rk::Error>(&polled))stop(error->message);
            if(const auto& hash=std::get<std::string>(polled);!hash.empty()) {
                std::cout<<"LIVE_PATH_OUTPUT_SHA256="<<hash<<std::endl;
                std::cout<<"LIVE_PATH_REPLAY=PASS"<<std::endl;
                return 0;
            }
            if(GetTickCount64()-start>20000)stop("GPU_TIMEOUT");
            Sleep(1);
        }
    } catch(const std::exception& error) { stop(error.what()); }
}
