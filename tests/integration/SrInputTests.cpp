#include <catch2/catch_test_macros.hpp>
#include "rk/SrInput.hpp"
#include "rk/FrameProbe.hpp"
#include <wrl/client.h>
#include <array>
#include <vector>
#include <utility>
#include <cstring>
#include <cmath>

using Microsoft::WRL::ComPtr;

TEST_CASE("Owned D3D11 SR inputs copy Skyrim colour, motion and native typeless depth", "[sr_input]") {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    REQUIRE(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,
        D3D11_SDK_VERSION,&device,nullptr,&context)));
    constexpr UINT width=17,height=13;
    const std::array formats{DXGI_FORMAT_R16G16B16A16_FLOAT,DXGI_FORMAT_R16G16_FLOAT,DXGI_FORMAT_R24G8_TYPELESS};
    const std::array sizes{8U,4U,4U};
    std::array<ComPtr<ID3D11Texture2D>,3> sources;
    std::array<std::vector<std::uint8_t>,3> pixels;
    for(std::size_t i=0;i<sources.size();++i) {
        pixels[i].resize(width*height*sizes[i]);
        for(std::size_t j=0;j<pixels[i].size();++j)pixels[i][j]=static_cast<std::uint8_t>((j+i*37)%251);
        D3D11_TEXTURE2D_DESC d{};d.Width=width;d.Height=height;d.MipLevels=d.ArraySize=1;
        d.SampleDesc.Count=1;d.Format=formats[i];
        d.BindFlags=D3D11_BIND_SHADER_RESOURCE|(i==2?D3D11_BIND_DEPTH_STENCIL:D3D11_BIND_RENDER_TARGET);
        D3D11_SUBRESOURCE_DATA initial{pixels[i].data(),width*sizes[i],0};
        REQUIRE(SUCCEEDED(device->CreateTexture2D(&d,&initial,&sources[i])));
    }
    std::array<ID3D11Texture2D*,3> raw{sources[0].Get(),sources[1].Get(),sources[2].Get()};
    auto prepared=rk::prepareSrInputs(context.Get(),raw);
    REQUIRE(std::holds_alternative<rk::PreparedSrInputs>(prepared));
    auto& owned=std::get<rk::PreparedSrInputs>(prepared);
    REQUIRE(owned.width()==width);
    REQUIRE(owned.height()==height);
    REQUIRE(owned.color()!=raw[0]);
    REQUIRE(owned.motion()!=raw[1]);
    REQUIRE(owned.depth()!=raw[2]);
    D3D11_TEXTURE2D_DESC output{};owned.output()->GetDesc(&output);
    REQUIRE(output.Format==DXGI_FORMAT_R16G16B16A16_FLOAT);
    REQUIRE((output.BindFlags&D3D11_BIND_UNORDERED_ACCESS)!=0);
    const std::array<ID3D11Texture2D*,3> copies{owned.color(),owned.motion(),owned.depth()};
    const auto readback=rk::readbackCandidates(context.Get(),copies);
    REQUIRE(std::holds_alternative<std::vector<rk::ProbeImage>>(readback));
    const auto& result=std::get<std::vector<rk::ProbeImage>>(readback);
    for(std::size_t i=0;i<3;++i)REQUIRE(result[i].pixels==pixels[i]);
    auto* originalOutput=owned.output();
    auto retainedOutput=owned.takeOutput();
    REQUIRE(retainedOutput.Get()==originalOutput);
    REQUIRE(owned.output()==nullptr);
    prepared=rk::Error{rk::ErrorCode::Unavailable,"retire source frame"};
    D3D11_TEXTURE2D_DESC retainedDescription{};
    retainedOutput->GetDesc(&retainedDescription);
    REQUIRE(retainedDescription.Width==width);
    REQUIRE(retainedDescription.Height==height);
    raw[1]=nullptr;
    REQUIRE(std::holds_alternative<rk::Error>(rk::prepareSrInputs(context.Get(),raw)));
    raw[1]=sources[1].Get();
    REQUIRE(std::holds_alternative<rk::Error>(rk::prepareSrInputs(nullptr,raw)));
    std::swap(raw[0],raw[1]);
    REQUIRE(std::holds_alternative<rk::Error>(rk::prepareSrInputs(context.Get(),raw)));
    std::swap(raw[0],raw[1]);
    ComPtr<ID3D11Device> otherDevice;
    REQUIRE(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,
        D3D11_SDK_VERSION,&otherDevice,nullptr,nullptr)));
    D3D11_TEXTURE2D_DESC foreignDesc{};sources[0]->GetDesc(&foreignDesc);
    ComPtr<ID3D11Texture2D> foreign;
    REQUIRE(SUCCEEDED(otherDevice->CreateTexture2D(&foreignDesc,nullptr,&foreign)));
    raw[0]=foreign.Get();
    REQUIRE(std::holds_alternative<rk::Error>(rk::prepareSrInputs(context.Get(),raw)));
}

TEST_CASE("Raw depth sample gate distinguishes a menu clear from world geometry", "[sr_input]") {
    constexpr UINT width=100,height=100;
    std::vector<std::uint8_t> bytes(width*height*4,0xff);
    auto menu=rk::sampleWorldDepth(bytes,width,height,width*4);
    REQUIRE(std::holds_alternative<rk::DepthSampleStats>(menu));
    REQUIRE(std::get<rk::DepthSampleStats>(menu).distinct==1);
    REQUIRE_FALSE(std::get<rk::DepthSampleStats>(menu).worldLike());
    for(UINT y=0;y<10;++y)for(UINT x=0;x<10;++x) {
        const auto sx=(2*x+1)*width/20,sy=(2*y+1)*height/20;
        const std::uint32_t depth=1000+y*10+x;
        std::memcpy(bytes.data()+(sy*width+sx)*4,&depth,4);
    }
    auto world=rk::sampleWorldDepth(bytes,width,height,width*4);
    REQUIRE(std::holds_alternative<rk::DepthSampleStats>(world));
    const auto stats=std::get<rk::DepthSampleStats>(world);
    REQUIRE(stats.distinct==100);
    REQUIRE(stats.nonFar==100);
    REQUIRE(stats.worldLike());
    REQUIRE(std::holds_alternative<rk::Error>(rk::sampleWorldDepth(bytes,width,height,width*4-1)));
}

TEST_CASE("Owned scene admission rejects a black transition despite world depth", "[sr_input]") {
    constexpr UINT width=64,height=64;
    std::vector<std::uint8_t> pixels(width*height*4,0);
    for(std::size_t i=3;i<pixels.size();i+=4)pixels[i]=255;
    auto black=rk::sampleWorldColor(pixels,width,height,width*4);
    REQUIRE(std::holds_alternative<rk::ColorSampleStats>(black));
    REQUIRE_FALSE(std::get<rk::ColorSampleStats>(black).sceneLike());
    for(UINT y=0;y<16;++y)for(UINT x=0;x<16;++x) {
        const auto sx=static_cast<UINT>((2*x+1)*static_cast<std::uint64_t>(width)/32);
        const auto sy=static_cast<UINT>((2*y+1)*static_cast<std::uint64_t>(height)/32);
        auto* pixel=pixels.data()+(static_cast<std::size_t>(sy)*width+sx)*4;
        pixel[0]=static_cast<std::uint8_t>(10+x);
        pixel[1]=static_cast<std::uint8_t>(20+y);
    }
    auto scene=rk::sampleWorldColor(pixels,width,height,width*4);
    REQUIRE(std::holds_alternative<rk::ColorSampleStats>(scene));
    const auto color=std::get<rk::ColorSampleStats>(scene);
    REQUIRE(color.nonBlack==256);
    REQUIRE(color.distinct==256);
    REQUIRE(color.sceneLike());
    REQUIRE(std::holds_alternative<rk::Error>(
        rk::sampleWorldColor(pixels,width,height,width*4-1)));

    rk::OwnedSceneAdmissionGate gate;
    const rk::DepthSampleStats worldDepth{95,94};
    const rk::ColorSampleStats blackColor{0,1};
    REQUIRE(gate.needsSample(1,4));
    gate.record(1,blackColor,worldDepth);
    REQUIRE_FALSE(gate.needsSample(1,4));
    gate.record(31,blackColor,worldDepth);
    REQUIRE_FALSE(gate.ready());
    gate.record(61,color,worldDepth);
    REQUIRE_FALSE(gate.ready());
    gate.record(91,color,worldDepth);
    REQUIRE(gate.ready());
    REQUIRE_FALSE(gate.needsSample(91,4));
    REQUIRE(gate.ready());
    gate.record(121,color,std::nullopt);
    REQUIRE_FALSE(gate.ready());
    REQUIRE(gate.needsSample(122,5));
    REQUIRE_FALSE(gate.ready());
}

TEST_CASE("Owned SR waits for two recent world depth samples and falls back when depth clears", "[sr_input]") {
    rk::WorldDepthGate gate;
    const rk::DepthSampleStats menu{1,0};
    const rk::DepthSampleStats world{95,94};
    REQUIRE(gate.needsSample(1,7));
    gate.record(1,menu);
    REQUIRE_FALSE(gate.needsSample(1,7));
    REQUIRE_FALSE(gate.ready());
    REQUIRE_FALSE(gate.needsSample(30,7));
    REQUIRE(gate.needsSample(31,7));
    gate.record(31,world);
    REQUIRE_FALSE(gate.ready());
    REQUIRE(gate.needsSample(61,7));
    gate.record(61,world);
    REQUIRE(gate.ready());
    REQUIRE_FALSE(gate.needsSample(62,7));
    REQUIRE(gate.needsSample(91,7));
    gate.record(91,menu);
    REQUIRE_FALSE(gate.ready());
    REQUIRE(gate.needsSample(121,7));
    gate.record(121,world);
    REQUIRE_FALSE(gate.ready());
    gate.record(151,std::nullopt);
    REQUIRE_FALSE(gate.ready());
    gate.record(181,world);
    gate.record(211,world);
    REQUIRE(gate.ready());
    REQUIRE(gate.needsSample(212,8));
    REQUIRE_FALSE(gate.ready());
}

TEST_CASE("SDR scene preparation accepts an RTV-only backbuffer and preserves its pixels", "[sr_input]") {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    REQUIRE(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,
        D3D11_SDK_VERSION,&device,nullptr,&context)));
    constexpr UINT width=8,height=6;
    const std::array formats{DXGI_FORMAT_R8G8B8A8_UNORM,DXGI_FORMAT_R16G16_FLOAT,
        DXGI_FORMAT_R24G8_TYPELESS};
    const std::array<UINT,3> binds{D3D11_BIND_RENDER_TARGET,
        D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE,
        D3D11_BIND_DEPTH_STENCIL|D3D11_BIND_SHADER_RESOURCE};
    std::array<ComPtr<ID3D11Texture2D>,3> sources;
    std::array<std::vector<std::uint8_t>,3> bytes;
    for(std::size_t i=0;i<sources.size();++i) {
        bytes[i].resize(width*height*4);
        for(std::size_t p=0;p<bytes[i].size();++p)
            bytes[i][p]=static_cast<std::uint8_t>((p+i*19)%251);
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width=width;desc.Height=height;desc.MipLevels=desc.ArraySize=1;
        desc.SampleDesc.Count=1;desc.Format=formats[i];desc.BindFlags=binds[i];
        const D3D11_SUBRESOURCE_DATA initial{bytes[i].data(),width*4,0};
        REQUIRE(SUCCEEDED(device->CreateTexture2D(&desc,&initial,&sources[i])));
    }
    const std::array<ID3D11Texture2D*,3> raw{
        sources[0].Get(),sources[1].Get(),sources[2].Get()};
    auto prepared=rk::prepareSdrSrInputs(context.Get(),raw);
    REQUIRE(std::holds_alternative<rk::PreparedSrInputs>(prepared));
    auto& owned=std::get<rk::PreparedSrInputs>(prepared);
    D3D11_TEXTURE2D_DESC colorDesc{},outputDesc{};
    owned.color()->GetDesc(&colorDesc);owned.output()->GetDesc(&outputDesc);
    REQUIRE(colorDesc.Format==DXGI_FORMAT_R8G8B8A8_UNORM);
    REQUIRE((colorDesc.BindFlags&D3D11_BIND_SHADER_RESOURCE)!=0);
    REQUIRE(outputDesc.Format==DXGI_FORMAT_R8G8B8A8_UNORM);
    REQUIRE((outputDesc.BindFlags&D3D11_BIND_UNORDERED_ACCESS)!=0);
    const std::array<ID3D11Texture2D*,3> copied{
        owned.color(),owned.motion(),owned.depth()};
    const auto readback=rk::readbackCandidates(context.Get(),copied);
    REQUIRE(std::holds_alternative<std::vector<rk::ProbeImage>>(readback));
    const auto& images=std::get<std::vector<rk::ProbeImage>>(readback);
    for(std::size_t i=0;i<3;++i)REQUIRE(images[i].pixels==bytes[i]);
    auto invalid=raw;
    invalid[0]=sources[1].Get();
    REQUIRE(std::holds_alternative<rk::Error>(rk::prepareSdrSrInputs(context.Get(),invalid)));
}

TEST_CASE("Reduced SR input retains its render extent and allocates a display-sized output", "[sr_input]") {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    REQUIRE(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,
        D3D11_SDK_VERSION,&device,nullptr,&context)));
    constexpr UINT renderWidth=640,renderHeight=360,displayWidth=1280,displayHeight=720;
    const std::array formats{DXGI_FORMAT_R8G8B8A8_UNORM,DXGI_FORMAT_R16G16_FLOAT,
        DXGI_FORMAT_R24G8_TYPELESS};
    const std::array<UINT,3> binds{D3D11_BIND_RENDER_TARGET,
        D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET,
        D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_DEPTH_STENCIL};
    std::array<ComPtr<ID3D11Texture2D>,3> sources;
    for(std::size_t i=0;i<sources.size();++i) {
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width=renderWidth;desc.Height=renderHeight;desc.MipLevels=desc.ArraySize=1;
        desc.SampleDesc.Count=1;desc.Format=formats[i];desc.BindFlags=binds[i];
        REQUIRE(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&sources[i])));
    }
    const std::array<ID3D11Texture2D*,3> raw{
        sources[0].Get(),sources[1].Get(),sources[2].Get()};
    auto prepared=rk::prepareSdrSrInputsForDisplay(context.Get(),raw,displayWidth,displayHeight);
    REQUIRE(std::holds_alternative<rk::PreparedSrInputs>(prepared));
    auto& owned=std::get<rk::PreparedSrInputs>(prepared);
    REQUIRE(owned.width()==renderWidth);
    REQUIRE(owned.height()==renderHeight);
    REQUIRE(owned.outputWidth()==displayWidth);
    REQUIRE(owned.outputHeight()==displayHeight);
    D3D11_TEXTURE2D_DESC output{};owned.output()->GetDesc(&output);
    REQUIRE(output.Width==displayWidth);
    REQUIRE(output.Height==displayHeight);
    REQUIRE(output.Format==DXGI_FORMAT_R8G8B8A8_UNORM);
    REQUIRE(std::holds_alternative<rk::Error>(
        rk::prepareSdrSrInputsForDisplay(context.Get(),raw,320,180)));
    REQUIRE(std::holds_alternative<rk::Error>(
        rk::prepareSdrSrInputsForDisplay(context.Get(),raw,0,displayHeight)));
}

TEST_CASE("Full-size scene guides copy only a reduced top-left active rectangle", "[sr_input]") {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    REQUIRE(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,
        D3D11_SDK_VERSION,&device,nullptr,&context)));
    constexpr UINT sourceWidth=8,sourceHeight=6,renderWidth=4,renderHeight=3;
    const std::array formats{DXGI_FORMAT_R8G8B8A8_UNORM,DXGI_FORMAT_R16G16_FLOAT,
        DXGI_FORMAT_R24G8_TYPELESS};
    const std::array<UINT,3> binds{D3D11_BIND_RENDER_TARGET,
        D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE,
        D3D11_BIND_DEPTH_STENCIL|D3D11_BIND_SHADER_RESOURCE};
    std::array<ComPtr<ID3D11Texture2D>,3> sources;
    std::array<std::vector<std::uint8_t>,3> pixels;
    for(std::size_t i=0;i<sources.size();++i) {
        pixels[i].resize(sourceWidth*sourceHeight*4);
        for(UINT y=0;y<sourceHeight;++y)for(UINT x=0;x<sourceWidth;++x)
            for(UINT channel=0;channel<4;++channel)
                pixels[i][(y*sourceWidth+x)*4+channel]=
                    static_cast<std::uint8_t>(i*47+y*13+x*3+channel);
        D3D11_TEXTURE2D_DESC d{};
        d.Width=sourceWidth;d.Height=sourceHeight;d.MipLevels=d.ArraySize=1;
        d.SampleDesc.Count=1;d.Format=formats[i];d.BindFlags=binds[i];
        const D3D11_SUBRESOURCE_DATA initial{pixels[i].data(),sourceWidth*4,0};
        REQUIRE(SUCCEEDED(device->CreateTexture2D(&d,&initial,&sources[i])));
    }
    const std::array<ID3D11Texture2D*,3> raw{
        sources[0].Get(),sources[1].Get(),sources[2].Get()};
    auto result=rk::prepareSdrSrInputsFromRegion(context.Get(),raw,
        renderWidth,renderHeight,sourceWidth,sourceHeight);
    REQUIRE(std::holds_alternative<rk::PreparedSrInputs>(result));
    auto& owned=std::get<rk::PreparedSrInputs>(result);
    REQUIRE(owned.width()==renderWidth);
    REQUIRE(owned.height()==renderHeight);
    REQUIRE(owned.outputWidth()==sourceWidth);
    REQUIRE(owned.outputHeight()==sourceHeight);
    const std::array<ID3D11Texture2D*,3> copies{
        owned.color(),owned.motion(),owned.depth()};
    const auto captured=rk::readbackCandidates(context.Get(),copies);
    REQUIRE(std::holds_alternative<std::vector<rk::ProbeImage>>(captured));
    const auto& images=std::get<std::vector<rk::ProbeImage>>(captured);
    D3D11_TEXTURE2D_DESC croppedDepth{};
    owned.depth()->GetDesc(&croppedDepth);
    REQUIRE(croppedDepth.Format==DXGI_FORMAT_R32_FLOAT);
    for(std::size_t i=0;i<images.size();++i) {
        std::vector<std::uint8_t> expected;
        for(UINT y=0;y<renderHeight;++y)
            expected.insert(expected.end(),pixels[i].begin()+y*sourceWidth*4,
                pixels[i].begin()+y*sourceWidth*4+renderWidth*4);
        if(i<2)REQUIRE(images[i].pixels==expected);
        else {
            REQUIRE(images[i].pixels.size()==expected.size());
            for(std::size_t pixel=0;pixel<renderWidth*renderHeight;++pixel) {
                std::uint32_t packed{};
                float actual{};
                std::memcpy(&packed,expected.data()+pixel*4,4);
                std::memcpy(&actual,images[i].pixels.data()+pixel*4,4);
                const float normalized=static_cast<float>(packed&0xffffffU)/16777215.0f;
                REQUIRE(std::abs(actual-normalized)<0.000001f);
            }
        }
    }
    REQUIRE(std::holds_alternative<rk::Error>(rk::prepareSdrSrInputsFromRegion(
        context.Get(),raw,sourceWidth+1,renderHeight,sourceWidth,sourceHeight)));
    REQUIRE(std::holds_alternative<rk::Error>(rk::prepareSdrSrInputsFromRegion(
        context.Get(),raw,renderWidth,renderHeight,renderWidth-1,sourceHeight)));
    REQUIRE(std::holds_alternative<rk::Error>(rk::prepareSdrSrInputsFromRegion(
        context.Get(),raw,0,renderHeight,sourceWidth,sourceHeight)));
}

TEST_CASE("Reduced SR crop preserves a nonzero source origin", "[sr_input]") {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    REQUIRE(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,
        nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)));
    constexpr UINT sourceWidth=8,sourceHeight=6;
    const std::array formats{DXGI_FORMAT_R8G8B8A8_UNORM,DXGI_FORMAT_R16G16_FLOAT,
        DXGI_FORMAT_R24G8_TYPELESS};
    std::array<ComPtr<ID3D11Texture2D>,3> sources;
    std::array<std::vector<std::uint8_t>,3> pixels;
    for(std::size_t i=0;i<sources.size();++i) {
        pixels[i].resize(sourceWidth*sourceHeight*4);
        for(UINT y=0;y<sourceHeight;++y)for(UINT x=0;x<sourceWidth;++x) {
            const auto value=static_cast<std::uint32_t>(i*0x12345+y*101+x*7);
            std::memcpy(pixels[i].data()+(y*sourceWidth+x)*4,&value,4);
        }
        D3D11_TEXTURE2D_DESC d{};d.Width=sourceWidth;d.Height=sourceHeight;
        d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;d.Format=formats[i];
        d.BindFlags=i==2?D3D11_BIND_DEPTH_STENCIL|D3D11_BIND_SHADER_RESOURCE:
            D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
        const D3D11_SUBRESOURCE_DATA initial{pixels[i].data(),sourceWidth*4,0};
        REQUIRE(SUCCEEDED(device->CreateTexture2D(&d,&initial,&sources[i])));
    }
    const std::array<ID3D11Texture2D*,3> raw{
        sources[0].Get(),sources[1].Get(),sources[2].Get()};
    const rk::SrSourceRegion region{2,1,4,3};
    auto enlarged=rk::prepareSdrSrInputsFromRegion(context.Get(),raw,
        rk::SrSourceRegion{0,0,sourceWidth,sourceHeight},
        sourceWidth*2,sourceHeight*2);
    REQUIRE(std::holds_alternative<rk::PreparedSrInputs>(enlarged));
    REQUIRE(std::get<rk::PreparedSrInputs>(enlarged).outputWidth()==sourceWidth*2);
    auto prepared=rk::prepareSdrSrInputsFromRegion(context.Get(),raw,region,
        sourceWidth,sourceHeight);
    REQUIRE(std::holds_alternative<rk::PreparedSrInputs>(prepared));
    auto& frame=std::get<rk::PreparedSrInputs>(prepared);
    REQUIRE(frame.sourceRegion()==region);
    const std::array<ID3D11Texture2D*,3> copied{
        frame.color(),frame.motion(),frame.depth()};
    const auto captured=rk::readbackCandidates(context.Get(),copied);
    REQUIRE(std::holds_alternative<std::vector<rk::ProbeImage>>(captured));
    const auto& images=std::get<std::vector<rk::ProbeImage>>(captured);
    for(UINT y=0;y<region.height;++y)for(UINT x=0;x<region.width;++x) {
        for(std::size_t i=0;i<2;++i)
            REQUIRE(std::memcmp(images[i].pixels.data()+(y*region.width+x)*4,
                pixels[i].data()+((y+region.top)*sourceWidth+x+region.left)*4,4)==0);
        std::uint32_t packed{};float actual{};
        std::memcpy(&packed,pixels[2].data()+((y+region.top)*sourceWidth+x+region.left)*4,4);
        std::memcpy(&actual,images[2].pixels.data()+(y*region.width+x)*4,4);
        REQUIRE(std::abs(actual-static_cast<float>(packed&0xffffffU)/16777215.0f)<0.000001f);
    }
}

TEST_CASE("Reduced owned SDR scene accepts native-sized motion and depth guides", "[sr_input]") {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    REQUIRE(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,
        nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)));
    constexpr UINT renderWidth=4,renderHeight=3,displayWidth=8,displayHeight=6;
    const std::array formats{DXGI_FORMAT_R8G8B8A8_UNORM,DXGI_FORMAT_R16G16_FLOAT,
        DXGI_FORMAT_R24G8_TYPELESS};
    std::array<ComPtr<ID3D11Texture2D>,3> sources;
    std::array<std::vector<std::uint8_t>,3> pixels;
    for(std::size_t i=0;i<sources.size();++i) {
        const UINT width=i?displayWidth:renderWidth;
        const UINT height=i?displayHeight:renderHeight;
        pixels[i].resize(width*height*4);
        for(UINT y=0;y<height;++y)for(UINT x=0;x<width;++x) {
            const auto value=static_cast<std::uint32_t>(i*0x12345+y*101+x*7);
            std::memcpy(pixels[i].data()+(y*width+x)*4,&value,4);
        }
        D3D11_TEXTURE2D_DESC d{};d.Width=width;d.Height=height;
        d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;d.Format=formats[i];
        d.BindFlags=i==2?D3D11_BIND_DEPTH_STENCIL|D3D11_BIND_SHADER_RESOURCE:
            D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
        const D3D11_SUBRESOURCE_DATA initial{pixels[i].data(),width*4,0};
        REQUIRE(SUCCEEDED(device->CreateTexture2D(&d,&initial,&sources[i])));
    }
    const std::array<ID3D11Texture2D*,3> raw{
        sources[0].Get(),sources[1].Get(),sources[2].Get()};
    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
    dsvDesc.Format=DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.ViewDimension=D3D11_DSV_DIMENSION_TEXTURE2D;
    ComPtr<ID3D11DepthStencilView> activeDepth;
    REQUIRE(SUCCEEDED(device->CreateDepthStencilView(sources[2].Get(),
        &dsvDesc,&activeDepth)));
    context->OMSetRenderTargets(0,nullptr,activeDepth.Get());
    auto prepared=rk::prepareSdrSrInputsFromOwnedScene(context.Get(),raw,
        displayWidth,displayHeight);
    REQUIRE(std::holds_alternative<rk::PreparedSrInputs>(prepared));
    ComPtr<ID3D11DepthStencilView> restoredDepth;
    context->OMGetRenderTargets(0,nullptr,&restoredDepth);
    REQUIRE(restoredDepth.Get()==activeDepth.Get());
    auto& frame=std::get<rk::PreparedSrInputs>(prepared);
    REQUIRE(frame.width()==renderWidth);
    REQUIRE(frame.height()==renderHeight);
    REQUIRE(frame.outputWidth()==displayWidth);
    REQUIRE(frame.outputHeight()==displayHeight);
    const std::array<ID3D11Texture2D*,3> copied{
        frame.color(),frame.motion(),frame.depth()};
    const auto captured=rk::readbackCandidates(context.Get(),copied);
    REQUIRE(std::holds_alternative<std::vector<rk::ProbeImage>>(captured));
    const auto& images=std::get<std::vector<rk::ProbeImage>>(captured);
    for(UINT y=0;y<renderHeight;++y)for(UINT x=0;x<renderWidth;++x) {
        for(std::size_t i=0;i<2;++i) {
            const UINT width=i?displayWidth:renderWidth;
            REQUIRE(std::memcmp(images[i].pixels.data()+(y*renderWidth+x)*4,
                pixels[i].data()+(y*width+x)*4,4)==0);
        }
        std::uint32_t packed{};float actual{};
        std::memcpy(&packed,pixels[2].data()+(y*displayWidth+x)*4,4);
        std::memcpy(&actual,images[2].pixels.data()+(y*renderWidth+x)*4,4);
        REQUIRE(std::abs(actual-static_cast<float>(packed&0xffffffU)/16777215.0f)<0.000001f);
    }
    D3D11_TEXTURE2D_DESC bad{};
    sources[1]->GetDesc(&bad);bad.Width=renderWidth;
    sources[1].Reset();
    REQUIRE(SUCCEEDED(device->CreateTexture2D(&bad,nullptr,&sources[1])));
    const std::array<ID3D11Texture2D*,3> mismatched{
        sources[0].Get(),sources[1].Get(),sources[2].Get()};
    REQUIRE(std::holds_alternative<rk::Error>(
        rk::prepareSdrSrInputsFromOwnedScene(context.Get(),mismatched,
            displayWidth,displayHeight)));
}
