#include "rk/SdrDlssPresenter.hpp"
#include "rk/SdrSrPresentation.hpp"
#include "rk/FrameProbe.hpp"
#include "rk/PatchDescriptor.hpp"
#include <Windows.h>
#include <d3d11.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
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
std::vector<std::uint8_t> rawCapture(const fs::path& file,
    std::string_view manifest,UINT renderWidth,UINT renderHeight,DXGI_FORMAT format) {
    const auto expected=static_cast<std::uint64_t>(renderWidth)*renderHeight*4;
    if(fs::file_size(file)!=expected)stop("CAPTURE_SIZE");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(expected));
    std::ifstream stream(file,std::ios::binary);
    if(!stream.read(reinterpret_cast<char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size())))stop("CAPTURE_READ");
    const auto record=file.filename().string()+" width="+std::to_string(renderWidth)+
        " height="+std::to_string(renderHeight)+" format="+
        std::to_string(static_cast<unsigned>(format))+" rowBytes="+
        std::to_string(renderWidth*4)+" bytes="+std::to_string(expected)+
        " sha256="+rk::sha256(bytes);
    if(manifest.find(record)==std::string_view::npos)stop("CAPTURE_MANIFEST_MISMATCH");
    return bytes;
}
int replayCapturedInputs(const fs::path& capture,bool publish) {
    constexpr UINT renderWidth=1707,renderHeight=960;
    std::ifstream manifestStream(capture/L"manifest.txt");
    if(!manifestStream)stop("CAPTURE_MANIFEST_MISSING");
    const std::string manifest{std::istreambuf_iterator<char>{manifestStream},
        std::istreambuf_iterator<char>{}};
    if(manifest.find("complete=true")==std::string::npos)
        stop("CAPTURE_MANIFEST_INCOMPLETE");
    const auto colorBytes=rawCapture(capture/L"color.raw",manifest,
        renderWidth,renderHeight,DXGI_FORMAT_R8G8B8A8_UNORM);
    const auto motionBytes=rawCapture(capture/L"motion.raw",manifest,
        renderWidth,renderHeight,DXGI_FORMAT_R16G16_FLOAT);
    const auto depthBytes=rawCapture(capture/L"depth-r32.raw",manifest,
        renderWidth,renderHeight,DXGI_FORMAT_R32_FLOAT);
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
    rk::SdrDlssPresenter presenter;
    const auto planned=presenter.prepareReducedPlan(device.Get(),context.Get(),{width,height});
    if(const auto error=std::get_if<rk::Error>(&planned))stop(error->message);
    if(const auto extent=std::get<rk::Extent>(planned);
       extent.width!=renderWidth||extent.height!=renderHeight)
        stop("CAPTURE_PLAN_EXTENT");
    const auto created=presenter.createReducedFeature(device.Get(),context.Get());
    if(const auto error=std::get_if<rk::Error>(&created))stop(error->message);
    if(!std::get<bool>(created))stop("CAPTURE_FEATURE_NOT_CREATED");
    const auto makeTexture=[&](DXGI_FORMAT format,UINT bind,
        UINT textureWidth,UINT textureHeight,const std::vector<std::uint8_t>* bytes,
        const char* step) {
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width=textureWidth;desc.Height=textureHeight;
        desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;
        desc.Format=format;desc.BindFlags=bind;
        const D3D11_SUBRESOURCE_DATA data{
            bytes?bytes->data():nullptr,textureWidth*4,0};
        ComPtr<ID3D11Texture2D> texture;
        checked(device->CreateTexture2D(&desc,bytes?&data:nullptr,&texture),step);
        return texture;
    };
    auto color=makeTexture(DXGI_FORMAT_R8G8B8A8_UNORM,
        D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE,
        renderWidth,renderHeight,&colorBytes,"CAPTURE_COLOR");
    auto motion=makeTexture(DXGI_FORMAT_R16G16_FLOAT,
        D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE,
        renderWidth,renderHeight,&motionBytes,"CAPTURE_MOTION");
    auto depth=makeTexture(DXGI_FORMAT_R32_FLOAT,
        D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_UNORDERED_ACCESS,
        renderWidth,renderHeight,&depthBytes,"CAPTURE_DEPTH");
    auto output=makeTexture(DXGI_FORMAT_R8G8B8A8_UNORM,
        D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_UNORDERED_ACCESS,
        width,height,nullptr,"CAPTURE_OUTPUT");
    auto display=makeTexture(DXGI_FORMAT_R8G8B8A8_UNORM,
        D3D11_BIND_RENDER_TARGET,width,height,nullptr,"CAPTURE_DISPLAY");
    ComPtr<ID3D11RenderTargetView> displayView;
    checked(device->CreateRenderTargetView(display.Get(),nullptr,&displayView),
        "CAPTURE_DISPLAY_RTV");
    context->OMSetRenderTargets(1,displayView.GetAddressOf(),nullptr);
    rk::PreparedSrInputs prepared{std::move(color),std::move(motion),
        std::move(depth),output,renderWidth,renderHeight,width,height,
        {0,0,renderWidth,renderHeight}};
    const auto evaluated=presenter.evaluatePrepared(device.Get(),context.Get(),
        std::move(prepared),{1,1,true},{-0.25f,-1.0f/6.0f});
    if(const auto error=std::get_if<rk::Error>(&evaluated))stop(error->message);
    const auto token=std::get<std::optional<rk::SrEvaluationToken>>(evaluated);
    if(!token)stop("CAPTURE_EVALUATION_SKIPPED");
    std::cout<<"CAPTURE_EVALUATION_RETURNED=1"<<std::endl;
    const std::array<ID3D11Texture2D*,1> evaluatedTarget{output.Get()};
    const auto evaluatedImage=rk::readbackCandidates(context.Get(),evaluatedTarget);
    if(const auto error=std::get_if<rk::Error>(&evaluatedImage))stop(error->message);
    const auto outputHash=rk::sha256(
        std::get<std::vector<rk::ProbeImage>>(evaluatedImage)[0].pixels);
    std::string displayHash;
    if(publish) {
        const auto published=presenter.publishEvaluated(context.Get(),*token,display.Get());
        if(const auto error=std::get_if<rk::Error>(&published))stop(error->message);
        if(!std::get<bool>(published))stop("CAPTURE_PUBLICATION_SKIPPED");
        const std::array<ID3D11Texture2D*,1> target{display.Get()};
        const auto image=rk::readbackCandidates(context.Get(),target);
        if(const auto error=std::get_if<rk::Error>(&image))stop(error->message);
        displayHash=rk::sha256(std::get<std::vector<rk::ProbeImage>>(image)[0].pixels);
        if(displayHash!=outputHash)stop("CAPTURE_PUBLICATION_DIFFERS");
    }
    context->Flush();
    const auto deadline=GetTickCount64()+20000;
    for(;;) {
        const auto stopped=presenter.stop(context.Get());
        if(const auto error=std::get_if<rk::Error>(&stopped))stop(error->message);
        if(std::get<bool>(stopped))break;
        if(GetTickCount64()>deadline)stop("CAPTURE_RETIRE_TIMEOUT");
        Sleep(1);
    }
    std::cout<<"CAPTURE_MODE="<<(publish?"EVAL_AND_PUBLISH":"EVAL_ONLY")
        <<"\nCAPTURE_RENDER="<<renderWidth<<"x"<<renderHeight
        <<"\nCAPTURE_DISPLAY="<<width<<"x"<<height
        <<"\nCAPTURE_NGX_FRAMES="<<presenter.submittedFrames()
        <<"\nCAPTURE_OUTPUT_SHA256="<<outputHash
        <<"\nCAPTURE_DISPLAY_SHA256="<<displayHash
        <<"\nCAPTURE_REPLAY=PASS"<<std::endl;
    return 0;
}
}
int wmain(int argc,wchar_t** argv) {
    if(argc==3&&(std::wstring_view(argv[1])==L"--captured-eval-only"||
       std::wstring_view(argv[1])==L"--captured-eval-publish")) {
        try { return replayCapturedInputs(fs::absolute(argv[2]),
            std::wstring_view(argv[1])==L"--captured-eval-publish"); }
        catch(const std::exception& error) { stop(error.what()); }
    }
    const bool injectFailure=argc==3&&std::wstring_view(argv[1])==L"--sr-fallback";
    const bool preparedFailure=argc==3&&
        std::wstring_view(argv[1])==L"--prepared-r32-fallback";
    const bool preparedR32=preparedFailure||
        (argc==3&&std::wstring_view(argv[1])==L"--prepared-r32");
    const bool ngxPlanOwned=argc==3&&
        std::wstring_view(argv[1])==L"--ngx-plan-owned-r32";
    const bool ngxPlan=ngxPlanOwned||
        (argc==3&&std::wstring_view(argv[1])==L"--ngx-plan");
    const bool usePreparedR32=preparedR32||ngxPlanOwned;
    const bool sr=injectFailure||usePreparedR32||ngxPlan||
        (argc==3&&std::wstring_view(argv[1])==L"--sr");
    if(argc!=2&&!sr) {
        std::cerr<<"Usage: RazKolbasSdrLivePresentation [--sr|--sr-fallback|--prepared-r32|--prepared-r32-fallback|--ngx-plan|--ngx-plan-owned-r32|--captured-eval-only|--captured-eval-publish] <capture directory>\n";
        return 2;
    }
    try {
        const fs::path capture=fs::absolute(argv[sr?2:1]);
        const auto source=capture/L"post-world-backbuffer.raw";
        if(fs::file_size(source)!=static_cast<std::uint64_t>(width)*height*4)stop("SOURCE_SIZE");
        std::vector<std::uint8_t> scene(width*height*4);
        std::ifstream input(source,std::ios::binary);
        if(!input.read(reinterpret_cast<char*>(scene.data()),scene.size()))stop("SOURCE_READ");
        UINT inputWidth=sr?width/2:width,inputHeight=sr?height/2:height;
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
        rk::SdrDlssPresenter presenter;
        if(ngxPlan) {
            const auto planned=presenter.prepareReducedPlan(device.Get(),context.Get(),{width,height});
            if(const auto error=std::get_if<rk::Error>(&planned))stop(error->message);
            inputWidth=std::get<rk::Extent>(planned).width;
            inputHeight=std::get<rk::Extent>(planned).height;
            if(ngxPlanOwned) {
                const auto created=presenter.createReducedFeature(device.Get(),context.Get());
                if(const auto error=std::get_if<rk::Error>(&created))stop(error->message);
                if(!std::get<bool>(created))stop("PREPARED_FEATURE_NOT_CREATED");
            }
        }
        std::vector<std::uint8_t> reduced;
        if(sr) {
            reduced.resize(static_cast<std::size_t>(inputWidth)*inputHeight*4);
            for(UINT y=0;y<inputHeight;++y)for(UINT x=0;x<inputWidth;++x) {
                const auto fromY=static_cast<UINT>(static_cast<std::uint64_t>(y)*height/inputHeight);
                const auto fromX=static_cast<UINT>(static_cast<std::uint64_t>(x)*width/inputWidth);
                std::memcpy(reduced.data()+(static_cast<std::size_t>(y)*inputWidth+x)*4,
                    scene.data()+(static_cast<std::size_t>(fromY)*width+fromX)*4,4);
            }
        }
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
            sourceDesc.BindFlags|=D3D11_BIND_SHADER_RESOURCE;
            const D3D11_SUBRESOURCE_DATA reducedPixels{reduced.data(),inputWidth*4,0};
            checked(device->CreateTexture2D(&sourceDesc,&reducedPixels,&sceneInput),"SCENE_INPUT");
        }
        const UINT guideWidth=ngxPlanOwned?width:inputWidth;
        const UINT guideHeight=ngxPlanOwned?height:inputHeight;
        std::vector<std::uint8_t> zero(static_cast<std::size_t>(guideWidth)*guideHeight*4);
        auto motionDesc=desc;motionDesc.Format=DXGI_FORMAT_R16G16_FLOAT;
        motionDesc.Width=guideWidth;motionDesc.Height=guideHeight;
        motionDesc.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
        const D3D11_SUBRESOURCE_DATA zeros{zero.data(),guideWidth*4,0};
        ComPtr<ID3D11Texture2D> motion;
        checked(device->CreateTexture2D(&motionDesc,&zeros,&motion),"MOTION");
        auto depthDesc=desc;depthDesc.Format=DXGI_FORMAT_R24G8_TYPELESS;
        depthDesc.Width=guideWidth;depthDesc.Height=guideHeight;
        depthDesc.BindFlags=D3D11_BIND_DEPTH_STENCIL|D3D11_BIND_SHADER_RESOURCE;
        for(std::size_t i=0;i<static_cast<std::size_t>(guideWidth)*guideHeight;++i) {
            const std::uint32_t value=0x00400000+static_cast<std::uint32_t>(i%4096);
            std::memcpy(zero.data()+i*4,&value,4);
        }
        ComPtr<ID3D11Texture2D> depth;
        checked(device->CreateTexture2D(&depthDesc,&zeros,&depth),"DEPTH");
        ComPtr<ID3D11DepthStencilView> ownedDepthView;
        if(ngxPlanOwned) {
            D3D11_DEPTH_STENCIL_VIEW_DESC depthViewDesc{};
            depthViewDesc.Format=DXGI_FORMAT_D24_UNORM_S8_UINT;
            depthViewDesc.ViewDimension=D3D11_DSV_DIMENSION_TEXTURE2D;
            checked(device->CreateDepthStencilView(depth.Get(),&depthViewDesc,
                &ownedDepthView),"OWNED_DEPTH_DSV");
            context->OMSetRenderTargets(1,view.GetAddressOf(),ownedDepthView.Get());
        }
        constexpr std::array<rk::NgxJitter,8> observedGameCycle{{
            {-0.25f,-1.0f/6.0f},{0.25f,7.0f/18.0f},
            {-0.375f,1.0f/18.0f},{0.125f,-5.0f/18.0f},
            {-0.125f,5.0f/18.0f},{0.375f,-1.0f/18.0f},
            {-0.4375f,-7.0f/18.0f},{0.0f,1.0f/6.0f}}};
        unsigned rendered=0,fallbackFrames=0;
        bool injected=false;
        std::string fallbackHash;
        std::vector<rk::SdrSrFrameResult> retainedFallbacks;
        const auto started=GetTickCount64();
        while(rendered<30) {
            context->UpdateSubresource(backbuffer.Get(),0,nullptr,scene.data(),width*4,0);
            if(sr)context->UpdateSubresource(sceneInput.Get(),0,nullptr,reduced.data(),inputWidth*4,0);
            if(sr) {
                auto presented=rk::presentSdrSrFrame(context.Get(),sceneInput.Get(),
                    backbuffer.Get(),[&]()->rk::Result<bool> {
                        if(injectFailure&&rendered==10&&!injected) {
                            injected=true;
                            return rk::Error{rk::ErrorCode::Unavailable,"Injected SR failure"};
                        }
                        const auto jitter=observedGameCycle[rendered%observedGameCycle.size()];
                        if(!usePreparedR32)
                            return presenter.renderSr(device.Get(),context.Get(),sceneInput.Get(),
                                motion.Get(),depth.Get(),backbuffer.Get(),jitter);
                        const std::array<ID3D11Texture2D*,3> sources{
                            sceneInput.Get(),motion.Get(),depth.Get()};
                        auto inputs=ngxPlanOwned?
                            rk::prepareSdrSrInputsFromOwnedScene(context.Get(),sources,width,height):
                            rk::prepareSdrSrInputsFromRegion(context.Get(),sources,
                                rk::SrSourceRegion{0,0,inputWidth,inputHeight},width,height);
                        if(const auto error=std::get_if<rk::Error>(&inputs))return *error;
                        const auto evaluated=presenter.evaluatePrepared(device.Get(),context.Get(),
                            std::move(std::get<rk::PreparedSrInputs>(inputs)),
                            {rendered+1,1,rendered==0},jitter);
                        if(const auto error=std::get_if<rk::Error>(&evaluated))return *error;
                        const auto token=std::get<std::optional<rk::SrEvaluationToken>>(evaluated);
                        if(!token)return false;
                        if(preparedFailure&&rendered==10&&!injected) {
                            injected=true;
                            return rk::Error{rk::ErrorCode::Unavailable,
                                "Injected after prepared NGX evaluation"};
                        }
                        return presenter.publishEvaluated(context.Get(),*token,backbuffer.Get());
                    });
                if(const auto error=std::get_if<rk::Error>(&presented))stop(error->message);
                auto frame=std::move(std::get<rk::SdrSrFrameResult>(presented));
                if(frame.mode()==rk::SdrSrFrameMode::SpatialFallback) {
                    ++fallbackFrames;
                    presenter.requestReset();
                    if(injected&&fallbackHash.empty()) {
                        const std::array<ID3D11Texture2D*,1> target{backbuffer.Get()};
                        const auto readback=rk::readbackCandidates(context.Get(),target);
                        if(const auto error=std::get_if<rk::Error>(&readback))stop(error->message);
                        fallbackHash=rk::sha256(std::get<std::vector<rk::ProbeImage>>(readback)[0].pixels);
                    }
                    retainedFallbacks.push_back(std::move(frame));
                }
                ++rendered;
            } else {
                const auto result=presenter.render(device.Get(),context.Get(),backbuffer.Get(),
                    motion.Get(),depth.Get(),observedGameCycle[rendered%observedGameCycle.size()]);
                if(const auto error=std::get_if<rk::Error>(&result))stop(error->message);
                if(std::get<bool>(result))++rendered;
            }
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
        for(const auto& frame:retainedFallbacks)for(;;) {
            const auto complete=frame.complete(context.Get());
            if(const auto error=std::get_if<rk::Error>(&complete))stop(error->message);
            if(std::get<bool>(complete))break;
            if(GetTickCount64()>deadline)stop("FALLBACK_RETIRE_TIMEOUT");
            Sleep(1);
        }
        retainedFallbacks.clear();
        for(;;) {
            const auto stopped=presenter.stop(context.Get());
            if(const auto error=std::get_if<rk::Error>(&stopped))stop(error->message);
            if(std::get<bool>(stopped))break;
            if(GetTickCount64()>deadline)stop("SHUTDOWN_TIMEOUT");
            Sleep(1);
        }
        if((injectFailure||preparedFailure)&&
           (!injected||!fallbackFrames||fallbackHash.empty()))
            stop("FALLBACK_NOT_OBSERVED");
        if(usePreparedR32&&presenter.submittedFrames()==0)stop("PREPARED_R32_NGX_NO_SUCCESS");
        std::cout<<"SDR_PRESENT_MODE="<<(sr?(injectFailure?"SR_WITH_FALLBACK":
            preparedFailure?"PREPARED_R32_WITH_FALLBACK":
            ngxPlanOwned?"NGX_PLAN_OWNED_R32_SR":
            preparedR32?"PREPARED_R32_SR":ngxPlan?"NGX_PLAN_SR":"SR"):"DLAA")
            <<"\nSDR_PRESENT_RENDER="<<inputWidth<<"x"<<inputHeight
            <<"\nSDR_PRESENT_DISPLAY="<<width<<"x"<<height
            <<"\nSDR_PRESENT_DLSS_FRAMES="<<presenter.submittedFrames()
            <<"\nSDR_PRESENT_FALLBACK_FRAMES="<<fallbackFrames
            <<"\nSDR_PRESENT_FALLBACK_SHA256="<<fallbackHash
            <<"\nSDR_PRESENT_FRAMES="<<rendered<<"\nSDR_PRESENT_SHA256="<<hash
            <<"\nSDR_PRESENT_REPLAY=PASS"<<std::endl;
        return 0;
    } catch(const std::exception& error) { stop(error.what()); }
}
