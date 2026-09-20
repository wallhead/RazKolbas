#include "rk/SdrDlssPresenter.hpp"
#include "rk/FrameProbe.hpp"
#include "rk/PatchDescriptor.hpp"
#include <Windows.h>
#include <d3d11.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>
#include <vector>
#include <array>
#include <cstring>

using Microsoft::WRL::ComPtr;
namespace fs=std::filesystem;
namespace {
constexpr UINT width=2560,height=1440;
[[noreturn]] void stop(const std::string& reason) {
    std::cerr<<"SDR_PRESENT_STOP="<<reason<<std::endl;
    ExitProcess(10);
}
void checked(HRESULT result,const char* step) { if(FAILED(result))stop(step); }
}
int wmain(int argc,wchar_t** argv) {
    const bool sr=argc==3&&std::wstring_view(argv[1])==L"--sr";
    if(argc!=2&&!sr) {
        std::cerr<<"Usage: RazKolbasSdrLivePresentation [--sr] <stage-pair directory>\n";
        return 2;
    }
    try {
        const fs::path capture=fs::absolute(argv[sr?2:1]);
        const auto source=capture/L"post-world-backbuffer.raw";
        if(fs::file_size(source)!=static_cast<std::uint64_t>(width)*height*4)stop("SOURCE_SIZE");
        std::vector<std::uint8_t> scene(width*height*4);
        std::ifstream input(source,std::ios::binary);
        if(!input.read(reinterpret_cast<char*>(scene.data()),scene.size()))stop("SOURCE_READ");
        const UINT inputWidth=sr?width/2:width,inputHeight=sr?height/2:height;
        std::vector<std::uint8_t> reduced;
        if(sr) {
            reduced.resize(static_cast<std::size_t>(inputWidth)*inputHeight*4);
            for(UINT y=0;y<inputHeight;++y)for(UINT x=0;x<inputWidth;++x)
                std::memcpy(reduced.data()+(static_cast<std::size_t>(y)*inputWidth+x)*4,
                    scene.data()+(static_cast<std::size_t>(y*2)*width+x*2)*4,4);
        }
        ComPtr<IDXGIFactory6> factory;
        checked(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory)),"FACTORY");
        ComPtr<IDXGIAdapter1> adapter;
        for(UINT i=0;;++i) {
            ComPtr<IDXGIAdapter1> candidate;
            const auto result=factory->EnumAdapterByGpuPreference(i,
                DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&candidate));
            if(result==DXGI_ERROR_NOT_FOUND)break;
            checked(result,"ADAPTER_ENUM");
            DXGI_ADAPTER_DESC1 desc{};checked(candidate->GetDesc1(&desc),"ADAPTER_DESC");
            if(desc.VendorId==0x10de&&!(desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)) {
                adapter=candidate;break;
            }
        }
        if(!adapter)stop("NVIDIA_MISSING");
        ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
        checked(D3D11CreateDevice(adapter.Get(),D3D_DRIVER_TYPE_UNKNOWN,nullptr,0,
            nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context),"DEVICE");
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width=width;desc.Height=height;desc.MipLevels=desc.ArraySize=1;
        desc.SampleDesc.Count=1;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.BindFlags=D3D11_BIND_RENDER_TARGET;
        ComPtr<ID3D11Texture2D> backbuffer;
        const D3D11_SUBRESOURCE_DATA pixels{scene.data(),width*4,0};
        checked(device->CreateTexture2D(&desc,&pixels,&backbuffer),"BACKBUFFER");
        ComPtr<ID3D11RenderTargetView> view;
        checked(device->CreateRenderTargetView(backbuffer.Get(),nullptr,&view),"RTV");
        context->OMSetRenderTargets(1,view.GetAddressOf(),nullptr);
        ComPtr<ID3D11Texture2D> sceneInput;
        if(sr) {
            auto sourceDesc=desc;sourceDesc.Width=inputWidth;sourceDesc.Height=inputHeight;
            const D3D11_SUBRESOURCE_DATA reducedPixels{reduced.data(),inputWidth*4,0};
            checked(device->CreateTexture2D(&sourceDesc,&reducedPixels,&sceneInput),"SCENE_INPUT");
        }
        std::vector<std::uint8_t> zero(static_cast<std::size_t>(inputWidth)*inputHeight*4);
        auto motionDesc=desc;motionDesc.Format=DXGI_FORMAT_R16G16_FLOAT;
        motionDesc.Width=inputWidth;motionDesc.Height=inputHeight;
        motionDesc.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
        const D3D11_SUBRESOURCE_DATA zeros{zero.data(),inputWidth*4,0};
        ComPtr<ID3D11Texture2D> motion;
        checked(device->CreateTexture2D(&motionDesc,&zeros,&motion),"MOTION");
        auto depthDesc=desc;depthDesc.Format=DXGI_FORMAT_R24G8_TYPELESS;
        depthDesc.Width=inputWidth;depthDesc.Height=inputHeight;
        depthDesc.BindFlags=D3D11_BIND_DEPTH_STENCIL|D3D11_BIND_SHADER_RESOURCE;
        for(std::size_t i=0;i<static_cast<std::size_t>(inputWidth)*inputHeight;++i) {
            const std::uint32_t value=0x00400000+static_cast<std::uint32_t>(i%4096);
            std::memcpy(zero.data()+i*4,&value,4);
        }
        ComPtr<ID3D11Texture2D> depth;
        checked(device->CreateTexture2D(&depthDesc,&zeros,&depth),"DEPTH");
        rk::SdrDlssPresenter presenter;
        constexpr std::array<rk::NgxJitter,8> observedGameCycle{{
            {-0.25f,-1.0f/6.0f},{0.25f,7.0f/18.0f},
            {-0.375f,1.0f/18.0f},{0.125f,-5.0f/18.0f},
            {-0.125f,5.0f/18.0f},{0.375f,-1.0f/18.0f},
            {-0.4375f,-7.0f/18.0f},{0.0f,1.0f/6.0f}}};
        unsigned rendered=0;
        const auto started=GetTickCount64();
        while(rendered<30) {
            context->UpdateSubresource(backbuffer.Get(),0,nullptr,scene.data(),width*4,0);
            if(sr)context->UpdateSubresource(sceneInput.Get(),0,nullptr,reduced.data(),inputWidth*4,0);
            const auto result=sr?
                presenter.renderSr(device.Get(),context.Get(),sceneInput.Get(),motion.Get(),
                    depth.Get(),backbuffer.Get(),observedGameCycle[rendered%observedGameCycle.size()]):
                presenter.render(device.Get(),context.Get(),backbuffer.Get(),motion.Get(),
                    depth.Get(),observedGameCycle[rendered%observedGameCycle.size()]);
            if(const auto error=std::get_if<rk::Error>(&result))stop(error->message);
            if(std::get<bool>(result))++rendered;
            context->Flush();
            if(GetTickCount64()-started>20000)stop("TIMEOUT");
            Sleep(1);
        }
        const std::array<ID3D11Texture2D*,1> target{backbuffer.Get()};
        const auto readback=rk::readbackCandidates(context.Get(),target);
        if(const auto error=std::get_if<rk::Error>(&readback))stop(error->message);
        const auto hash=rk::sha256(std::get<std::vector<rk::ProbeImage>>(readback)[0].pixels);
        const auto inputHash=rk::sha256(scene);
        if(hash==inputHash)stop("OUTPUT_UNCHANGED");
        context->Flush();
        const auto deadline=GetTickCount64()+20000;
        for(;;) {
            const auto stopped=presenter.stop(context.Get());
            if(const auto error=std::get_if<rk::Error>(&stopped))stop(error->message);
            if(std::get<bool>(stopped))break;
            if(GetTickCount64()>deadline)stop("SHUTDOWN_TIMEOUT");
            Sleep(1);
        }
        std::cout<<"SDR_PRESENT_MODE="<<(sr?"SR":"DLAA")
            <<"\nSDR_PRESENT_RENDER="<<inputWidth<<"x"<<inputHeight
            <<"\nSDR_PRESENT_DISPLAY="<<width<<"x"<<height
            <<"\nSDR_PRESENT_FRAMES="<<rendered<<"\nSDR_PRESENT_SHA256="<<hash
            <<"\nSDR_PRESENT_REPLAY=PASS"<<std::endl;
        return 0;
    } catch(const std::exception& error) { stop(error.what()); }
}
