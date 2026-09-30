#include "rk/FgCameraWriteHooks.hpp"
#include "rk/OwnedRouteProfile.hpp"
#include "rk/PatchDescriptor.hpp"
#include "rk/PointerPatch.hpp"
#include "rk/RendererHook.hpp"
#include <Windows.h>
#include <wrl/client.h>
#include <cstring>
#include <iostream>

using Microsoft::WRL::ComPtr;
namespace {
void foreignOwner() {}
int run(const wchar_t* enbPath) {
    const auto sites=rk::enb505CameraWriteSites();
    const auto hash=rk::sha256File(enbPath);
    if(!std::holds_alternative<std::string>(hash)||
       std::get<std::string>(hash)!=sites.front().moduleSha256)return 2;
    const auto enb=LoadLibraryW(enbPath);if(!enb)return 3;
    const auto create=reinterpret_cast<PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN>(
        GetProcAddress(enb,"D3D11CreateDeviceAndSwapChain"));
    if(!create)return 4;
    const auto window=CreateWindowExW(0,L"STATIC",L"Camera write probe",
        WS_POPUP,0,0,160,96,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    if(!window)return 5;
    DXGI_SWAP_CHAIN_DESC swapDesc{};swapDesc.BufferDesc.Width=160;swapDesc.BufferDesc.Height=96;
    swapDesc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;swapDesc.SampleDesc.Count=1;
    swapDesc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT|DXGI_USAGE_SHADER_INPUT;
    swapDesc.BufferCount=2;swapDesc.OutputWindow=window;swapDesc.Windowed=TRUE;
    swapDesc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGISwapChain> swap;
    const auto hr=create(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,
        &swapDesc,&swap,&device,nullptr,&context);
    std::cout<<"ENB creation hr=0x"<<std::hex<<static_cast<unsigned>(hr)<<std::dec<<'\n';
    if(FAILED(hr)||!device||!context)return 6;
    auto** table=*reinterpret_cast<void***>(context.Get());
    const auto base=reinterpret_cast<std::uintptr_t>(enb);
    if(reinterpret_cast<std::uintptr_t>(table)!=base+sites.front().tableRva)return 7;
    // Synthetic cells, never a game identity claim. Production's caller has
    // already hashed Skyrim before supplying its loaded base.
    const auto game=reinterpret_cast<std::uintptr_t>(VirtualAlloc(nullptr,0x3300000,
        MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    if(!game)return 8;
    auto** bufferCell=reinterpret_cast<ID3D11Buffer**>(game+0x3288788);
    *reinterpret_cast<ID3D11DeviceContext**>(game+0x32887b0)=context.Get();
    const auto install=[&](std::string_view disabled) {
        return rk::installFgCameraWriteHooks(context.Get(),game,
            rk::skyrim1170CreationProfile().gameSha256,disabled);
    };
    for(const auto& site:sites) {
        const auto disabled=install(site.id);
        if(!std::holds_alternative<bool>(disabled)||std::get<bool>(disabled))return 9;
        if(reinterpret_cast<std::uintptr_t>(table[site.slot])!=base+site.methodRva)return 10;
    }
    for(const auto& site:sites) {
        rk::PointerPatch foreign;
        if(std::holds_alternative<rk::Error>(foreign.apply(table+site.slot,
            reinterpret_cast<void*>(base+site.methodRva),reinterpret_cast<void*>(&foreignOwner))))return 11;
        if(!std::holds_alternative<rk::Error>(install({})))return 12;
        const auto& other=sites[site.slot==14?1:0];
        if(reinterpret_cast<std::uintptr_t>(table[other.slot])!=base+other.methodRva)return 13;
        if(std::holds_alternative<rk::Error>(foreign.restore()))return 14;
    }
    const auto installed=install({});
    if(const auto* error=std::get_if<rk::Error>(&installed)) {
        std::cerr<<error->message<<'\n';return 15;
    }
    if(!std::get<bool>(installed))return 16;
    D3D11_BUFFER_DESC desc{};desc.ByteWidth=720;desc.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    desc.Usage=D3D11_USAGE_DYNAMIC;desc.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
    ComPtr<ID3D11Buffer> buffer,foreignBuffer;
    if(FAILED(device->CreateBuffer(&desc,nullptr,&buffer))||
       FAILED(device->CreateBuffer(&desc,nullptr,&foreignBuffer)))return 17;
    *bufferCell=buffer.Get();
    for(unsigned frame=1;frame<=240;++frame) {
        if(frame==121) {
            ComPtr<ID3D11Buffer> replacement;
            if(FAILED(device->CreateBuffer(&desc,nullptr,&replacement)))return 18;
            buffer=std::move(replacement);*bufferCell=buffer.Get();
            if(rk::snapshotFgCameraWrites().latest)return 19;
        }
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if(FAILED(context->Map(buffer.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped)))return 20;
        if(rk::snapshotFgCameraWrites().latest)return 21;
        std::memset(mapped.pData,0x5a,720);context->Unmap(buffer.Get(),0);
        const auto facts=rk::snapshotFgCameraWrites();
        if(!facts.latest||facts.completed!=frame||facts.latest->revision!=frame||
           facts.latest->buffer!=reinterpret_cast<std::uintptr_t>(buffer.Get()))return 22;
        for(auto byte:facts.latest->bytes)if(byte!=0x5a)return 23;
        if(FAILED(context->Map(foreignBuffer.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped)))return 24;
        std::memset(mapped.pData,0x99,720);context->Unmap(foreignBuffer.Get(),0);
        if(rk::snapshotFgCameraWrites().completed!=frame)return 25;
    }
    D3D11_BUFFER_DESC stagingDesc=desc;stagingDesc.Usage=D3D11_USAGE_STAGING;
    stagingDesc.BindFlags=0;stagingDesc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Buffer> staging;
    if(FAILED(device->CreateBuffer(&stagingDesc,nullptr,&staging)))return 26;
    context->CopyResource(staging.Get(),buffer.Get());
    D3D11_MAPPED_SUBRESOURCE readback{};
    if(FAILED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&readback)))return 27;
    const auto final=rk::snapshotFgCameraWrites();
    const bool same=std::memcmp(readback.pData,final.latest->bytes.data(),720)==0;
    context->Unmap(staging.Get(),0);if(!same||final.rejected)return 28;
    *bufferCell=nullptr;if(rk::snapshotFgCameraWrites().latest)return 29;
    std::cout<<"Exact ENB slots: disabled/foreign-owner rejection passed; 240 fresh identical writes, buffer replacement, foreign resource forwarding and GPU readback passed; writers="
        <<final.writerCount<<" rejected="<<final.rejected<<"; synthetic camera fixture, no Skyrim or FG-On\n";
    // Callback lease retains one context and pinned modules until exit. Its
    // synthetic fixture cells must remain mapped for that lifetime.
    swap.Reset();DestroyWindow(window);return 0;
}
}
int wmain(int argc,wchar_t** argv) {
    std::cout.setf(std::ios::unitbuf);
    if(argc!=2)return 1;
    try {return run(argv[1]);}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 30;}
}
