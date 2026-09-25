#include "rk/FrameProbe.hpp"
#include "rk/NrStage.hpp"
#include "rk/PatchDescriptor.hpp"
#include "rk/Settings.hpp"
#include "rk/SrInput.hpp"
#include <Windows.h>
#include <d3d11.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <filesystem>
#include <iostream>
#include <string_view>
#include <vector>

using Microsoft::WRL::ComPtr;
namespace fs=std::filesystem;
namespace {
[[noreturn]] void stop(const std::string& reason) {
    std::cerr<<"NR_LIVE_BRIDGE_STOP="<<reason<<std::endl;
    ExitProcess(10);
}
void checked(HRESULT result,const char* step) {if(FAILED(result))stop(step);}
template<class T> T value(rk::Result<T> result) {
    if(const auto error=std::get_if<rk::Error>(&result))stop(error->message);
    return std::move(std::get<T>(result));
}
}

int wmain(int argc,wchar_t** argv) {
    if(argc!=2) {
        std::cerr<<"Usage: RazKolbasNrLiveBridge <exact _nvngx.dll>\n";
        return 2;
    }
    const auto corePath=fs::absolute(argv[1]);
    const auto core=LoadLibraryExW(corePath.c_str(),nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!core)stop("CORE_LOAD");

    ComPtr<IDXGIFactory6> factory;
    checked(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory)),"FACTORY");
    ComPtr<IDXGIAdapter1> adapter;
    DXGI_ADAPTER_DESC1 adapterDesc{};
    for(UINT i=0;;++i) {
        ComPtr<IDXGIAdapter1> candidate;
        const auto result=factory->EnumAdapterByGpuPreference(i,
            DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&candidate));
        if(result==DXGI_ERROR_NOT_FOUND)break;
        checked(result,"ADAPTER_ENUM");
        checked(candidate->GetDesc1(&adapterDesc),"ADAPTER_DESC");
        if(adapterDesc.VendorId==0x10de&&
           !(adapterDesc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)) {
            adapter=std::move(candidate);break;
        }
    }
    if(!adapter)stop("NVIDIA_MISSING");
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    checked(D3D11CreateDevice(adapter.Get(),D3D_DRIVER_TYPE_UNKNOWN,nullptr,0,
        nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context),"DEVICE");

    constexpr UINT width=640,height=360;
    const auto makeTexture=[&](DXGI_FORMAT format,UINT bind,
        const void* pixels,UINT rowPitch,const char* step) {
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width=width;desc.Height=height;desc.MipLevels=desc.ArraySize=1;
        desc.Format=format;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_DEFAULT;
        desc.BindFlags=bind;
        const D3D11_SUBRESOURCE_DATA data{pixels,rowPitch,0};
        ComPtr<ID3D11Texture2D> texture;
        checked(device->CreateTexture2D(&desc,pixels?&data:nullptr,&texture),step);
        return texture;
    };
    std::vector<std::uint8_t> colors(width*height*4);
    for(UINT y=0;y<height;++y)for(UINT x=0;x<width;++x) {
        const auto at=static_cast<std::size_t>(y*width+x)*4;
        colors[at]=static_cast<std::uint8_t>((x*255)/(width-1));
        colors[at+1]=static_cast<std::uint8_t>((y*255)/(height-1));
        colors[at+2]=static_cast<std::uint8_t>((x+y)&0xff);
        colors[at+3]=255;
    }
    std::vector<std::uint16_t> motion(width*height*2,0);
    std::vector<float> depth(width*height,0.5f);
    auto color=makeTexture(DXGI_FORMAT_R8G8B8A8_UNORM,
        D3D11_BIND_SHADER_RESOURCE,colors.data(),width*4,"COLOR");
    auto motionTexture=makeTexture(DXGI_FORMAT_R16G16_FLOAT,
        D3D11_BIND_SHADER_RESOURCE,motion.data(),width*4,"MOTION");
    auto depthTexture=makeTexture(DXGI_FORMAT_R32_FLOAT,
        D3D11_BIND_SHADER_RESOURCE,depth.data(),width*4,"DEPTH");
    auto output=makeTexture(DXGI_FORMAT_R8G8B8A8_UNORM,
        D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_UNORDERED_ACCESS,
        nullptr,0,"OUTPUT");
    const std::array<ID3D11Texture2D*,1> beforeTarget{color.Get()};
    const auto before=value(rk::readbackCandidates(context.Get(),beforeTarget));
    const auto beforeHash=rk::sha256(before[0].pixels);

    rk::PreparedSrInputs frame{std::move(color),std::move(motionTexture),
        std::move(depthTexture),std::move(output),width,height,width,height,
        {0,0,width,height}};
    auto settings=rk::defaultSettings();
    settings.values["NeuralRendering.Enabled"]=true;
    settings.values["NeuralRendering.Preset"]=rk::Choice{"Shipping"};
    {
        rk::NrStage stage;
        if(!value(stage.configure(settings)))stop("CONFIGURE_FALSE");
        constexpr unsigned frames=30;
        for(unsigned i=0;i<frames;++i) {
            if(i==10) {
                settings.values["NeuralRendering.Style"]=std::int64_t{1};
                settings.values["NeuralRendering.Intensity"]=0.75;
                if(!value(stage.updateRuntime(settings)))stop("LIVE_UPDATE_FALSE");
                if(value(stage.updateRuntime(settings)))stop("LIVE_UPDATE_NOT_IDEMPOTENT");
            }
            context->UpdateSubresource(frame.color(),0,nullptr,colors.data(),width*4,0);
            if(!value(stage.process(device.Get(),context.Get(),frame,i==0)))
                stop("PROCESS_FALSE");
        }
        const std::array<ID3D11Texture2D*,1> afterTarget{frame.color()};
        const auto after=value(rk::readbackCandidates(context.Get(),afterTarget));
        const auto afterHash=rk::sha256(after[0].pixels);
        if(beforeHash==afterHash||
           std::all_of(after[0].pixels.begin(),after[0].pixels.end(),
               [&](std::uint8_t byte){return byte==after[0].pixels.front();}))
            stop("OUTPUT_NOT_CHANGED");
        if(stage.submittedFrames()!=frames)stop("SUBMISSION_COUNT");
        if(!value(stage.stop()))stop("STOP_FALSE");
        std::wcout<<L"NR_LIVE_BRIDGE_ADAPTER="<<adapterDesc.Description<<L'\n';
        std::cout<<"NR_LIVE_BRIDGE_INPUT_SHA256="<<beforeHash
            <<"\nNR_LIVE_BRIDGE_OUTPUT_SHA256="<<afterHash
            <<"\nNR_LIVE_BRIDGE_FRAMES="<<frames
            <<"\nNR_LIVE_BRIDGE=PASS"<<std::endl;
    }
    FreeLibrary(core);
    return 0;
}
