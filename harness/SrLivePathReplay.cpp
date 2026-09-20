// Exercises the exact game-device offscreen probe with captured Skyrim bytes
// in an isolated process. Never loads a reference host DLL or Skyrim.
#include "rk/OffscreenDlssProbe.hpp"
#include "rk/SrInput.hpp"
#include <Windows.h>
#include <d3d11.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>
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
    const bool sdr=(argc==3||argc==4)&&std::wstring_view(argv[1])==L"--sdr";
    if(argc!=2&&!sdr) {
        std::cerr<<"Usage: RazKolbasSrLivePathReplay [--sdr] <verified capture directory> [output.raw]\n";
        return 2;
    }
    try {
        const fs::path capture=fs::absolute(argv[sdr?2:1]);
        const auto colorBytes=read(capture/(sdr?L"post-world-backbuffer.raw":
            L"main-colour-candidate.raw"),width*height*(sdr?4:8));
        auto motionBytes=sdr?std::vector<std::uint8_t>(width*height*4):
            read(capture/L"motion-candidate.raw",width*height*4);
        auto depthBytes=sdr?std::vector<std::uint8_t>(width*height*4):
            read(capture/L"depth-candidate.raw",width*height*4);
        if(sdr)for(std::size_t i=0;i<static_cast<std::size_t>(width)*height;++i) {
            const auto depth=static_cast<std::uint32_t>(0x00400000+(i%4096));
            std::memcpy(depthBytes.data()+i*4,&depth,4);
        }
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
        const auto color=texture(device.Get(),sdr?DXGI_FORMAT_R8G8B8A8_UNORM:
            DXGI_FORMAT_R16G16B16A16_FLOAT,sdr?4:8,
            sdr?D3D11_BIND_RENDER_TARGET:
                D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET,colorBytes.data());
        const auto motion=texture(device.Get(),DXGI_FORMAT_R16G16_FLOAT,4,
            D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET,motionBytes.data());
        const auto depth=texture(device.Get(),DXGI_FORMAT_R24G8_TYPELESS,4,
            D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_DEPTH_STENCIL,depthBytes.data());
        const std::array<ID3D11Texture2D*,3> sources{color.Get(),motion.Get(),depth.Get()};
        auto prepared=sdr?rk::prepareSdrSrInputs(context.Get(),sources):
            rk::prepareSrInputs(context.Get(),sources);
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
                if(sdr&&argc==4) {
                    D3D11_TEXTURE2D_DESC output{};owned.output()->GetDesc(&output);
                    output.Usage=D3D11_USAGE_STAGING;output.BindFlags=0;
                    output.CPUAccessFlags=D3D11_CPU_ACCESS_READ;output.MiscFlags=0;
                    ComPtr<ID3D11Texture2D> staging;
                    checked(device->CreateTexture2D(&output,nullptr,&staging),"OUTPUT_STAGING");
                    context->CopyResource(staging.Get(),owned.output());
                    D3D11_MAPPED_SUBRESOURCE mapped{};
                    checked(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped),"OUTPUT_MAP");
                    std::ofstream file(argv[3],std::ios::binary);
                    if(!file)stop("OUTPUT_FILE");
                    for(UINT y=0;y<height;++y)
                        file.write(reinterpret_cast<const char*>(mapped.pData)+
                            static_cast<std::size_t>(y)*mapped.RowPitch,width*4);
                    context->Unmap(staging.Get(),0);
                    if(!file)stop("OUTPUT_WRITE");
                }
                std::cout<<"LIVE_PATH_OUTPUT_SHA256="<<hash<<std::endl;
                std::cout<<"LIVE_PATH_REPLAY=PASS"<<std::endl;
                return 0;
            }
            if(GetTickCount64()-start>20000)stop("GPU_TIMEOUT");
            Sleep(1);
        }
    } catch(const std::exception& error) { stop(error.what()); }
}
