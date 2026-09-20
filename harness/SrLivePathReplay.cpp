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
    UINT textureWidth,UINT textureHeight,UINT pixelBytes,UINT bind,const void* data) {
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width=textureWidth;desc.Height=textureHeight;desc.MipLevels=desc.ArraySize=1;
    desc.Format=format;desc.SampleDesc.Count=1;desc.BindFlags=bind;
    const D3D11_SUBRESOURCE_DATA initial{data,textureWidth*pixelBytes,0};
    ComPtr<ID3D11Texture2D> result;
    checked(device->CreateTexture2D(&desc,&initial,&result),"TEXTURE_CREATE");
    return result;
}
std::vector<std::uint8_t> halfExtentSdr(const std::vector<std::uint8_t>& full) {
    std::vector<std::uint8_t> reduced(static_cast<std::size_t>(width/2)*(height/2)*4);
    for(UINT y=0;y<height/2;++y)for(UINT x=0;x<width/2;++x)
        std::memcpy(reduced.data()+(static_cast<std::size_t>(y)*(width/2)+x)*4,
            full.data()+(static_cast<std::size_t>(y*2)*width+x*2)*4,4);
    return reduced;
}
}
int wmain(int argc,wchar_t** argv) {
    const bool sdr=(argc==3||argc==4)&&std::wstring_view(argv[1])==L"--sdr";
    const bool sdrSr=(argc==3||argc==4)&&std::wstring_view(argv[1])==L"--sdr-sr";
    if(argc!=2&&!sdr&&!sdrSr) {
        std::cerr<<"Usage: RazKolbasSrLivePathReplay [--sdr|--sdr-sr] <verified capture directory> [output.raw]\n";
        return 2;
    }
    try {
        const bool useSdr=sdr||sdrSr;
        const fs::path capture=fs::absolute(argv[useSdr?2:1]);
        auto colorBytes=read(capture/(useSdr?L"post-world-backbuffer.raw":
            L"main-colour-candidate.raw"),width*height*(useSdr?4:8));
        if(sdrSr)colorBytes=halfExtentSdr(colorBytes);
        const UINT renderWidth=sdrSr?width/2:width;
        const UINT renderHeight=sdrSr?height/2:height;
        auto motionBytes=useSdr?std::vector<std::uint8_t>(renderWidth*renderHeight*4):
            read(capture/L"motion-candidate.raw",width*height*4);
        auto depthBytes=useSdr?std::vector<std::uint8_t>(renderWidth*renderHeight*4):
            read(capture/L"depth-candidate.raw",width*height*4);
        if(useSdr)for(std::size_t i=0;i<static_cast<std::size_t>(renderWidth)*renderHeight;++i) {
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
        const auto color=texture(device.Get(),useSdr?DXGI_FORMAT_R8G8B8A8_UNORM:
            DXGI_FORMAT_R16G16B16A16_FLOAT,renderWidth,renderHeight,useSdr?4:8,
            useSdr?D3D11_BIND_RENDER_TARGET:
                D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET,colorBytes.data());
        const auto motion=texture(device.Get(),DXGI_FORMAT_R16G16_FLOAT,renderWidth,renderHeight,4,
            D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET,motionBytes.data());
        const auto depth=texture(device.Get(),DXGI_FORMAT_R24G8_TYPELESS,renderWidth,renderHeight,4,
            D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_DEPTH_STENCIL,depthBytes.data());
        const std::array<ID3D11Texture2D*,3> sources{color.Get(),motion.Get(),depth.Get()};
        auto prepared=sdrSr?rk::prepareSdrSrInputsForDisplay(context.Get(),sources,width,height):
            sdr?rk::prepareSdrSrInputs(context.Get(),sources):
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
                if(useSdr&&argc==4) {
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
                std::cout<<"LIVE_PATH_RENDER_EXTENT="<<renderWidth<<"x"<<renderHeight
                    <<"; DISPLAY_EXTENT="<<width<<"x"<<height<<std::endl;
                std::cout<<"LIVE_PATH_REPLAY=PASS"<<std::endl;
                return 0;
            }
            if(GetTickCount64()-start>20000)stop("GPU_TIMEOUT");
            Sleep(1);
        }
    } catch(const std::exception& error) { stop(error.what()); }
}
