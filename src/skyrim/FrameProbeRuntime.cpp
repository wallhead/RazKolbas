#include "rk/FrameProbeRuntime.hpp"
#include "rk/FrameProbe.hpp"
#include "rk/PatchDescriptor.hpp"
#include <ShlObj.h>
#include <wrl/client.h>
#include <spdlog/spdlog.h>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <mutex>

namespace rk {
namespace {
std::atomic<std::uintptr_t> gameBase{0};
std::mutex probeMutex;
bool done=false;
unsigned attempts=0;
ULONGLONG start=0,previous=0;
bool read(std::uintptr_t address,void* destination,std::size_t size) {
    SIZE_T copied=0;
    return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(address),destination,size,&copied)&&copied==size;
}
std::filesystem::path captureDirectory() {
    PWSTR documents=nullptr;
    if(FAILED(SHGetKnownFolderPath(FOLDERID_Documents,KF_FLAG_DEFAULT,nullptr,&documents)))
        throw std::runtime_error("Cannot find capture directory");
    struct Free { PWSTR p;~Free(){CoTaskMemFree(p);} } free{documents};
    const auto directory=std::filesystem::path(documents)/"My Games"/"Skyrim Special Edition"/"SKSE"/"RazKolbasCaptures"/
        (std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
    std::filesystem::create_directories(directory);
    return directory;
}
}
void armFrameProbe(HMODULE verifiedGame) {
    const auto base=reinterpret_cast<std::uintptr_t>(verifiedGame);
    // Live code is decoded by the game; disk text is not a valid signature here.
    constexpr std::array<std::uint8_t,14> lock{0x48,0x81,0xc1,0xf0,0x27,0,0,0x48,0xff,0x25,0x72,0xab,0x90,0};
    constexpr std::array<std::uint8_t,14> unlock{0x48,0x81,0xc1,0xf0,0x27,0,0,0x48,0xff,0x25,0x5a,0xab,0x90,0};
    std::array<std::uint8_t,14> actual{};
    if(!read(base+0xe44550,actual.data(),actual.size())||actual!=lock||
       !read(base+0xe44570,actual.data(),actual.size())||actual!=unlock) {
        spdlog::warn("Candidate probe disabled: live renderer lock layout signature differs");return;
    }
    gameBase.store(base,std::memory_order_release);
    spdlog::info("Candidate probe armed: one CPU readback bundle after 90s; requires current renderer lock ownership; 64MiB limit; no SR guide validation");
}
void probePresentCandidates(IDXGISwapChain* swap) {
    using Microsoft::WRL::ComPtr;
    const auto base=gameBase.load(std::memory_order_acquire);
    if(!base||!swap)return;
    std::unique_lock guard(probeMutex,std::try_to_lock);
    if(!guard||done)return;
    const auto now=GetTickCount64();
    if(!start)start=now;
    if(now-start<90000||now-previous<5000)return;
    previous=now;
    if(++attempts>=32)done=true;
    ComPtr<ID3D11Device> device;
    if(FAILED(swap->GetDevice(IID_PPV_ARGS(&device))))return;
    ComPtr<ID3D11DeviceContext> context;device->GetImmediateContext(&context);
    std::array<std::uint8_t,renderer1170Size> snapshot{};
    if(!read(base+renderer1170Rva,snapshot.data(),snapshot.size()))return;
    const auto candidates=rendererCandidatePointers(snapshot,GetCurrentThreadId(),reinterpret_cast<std::uintptr_t>(device.Get()),
        reinterpret_cast<std::uintptr_t>(context.Get()),reinterpret_cast<std::uintptr_t>(swap));
    if(const auto error=std::get_if<Error>(&candidates)) {
        if(attempts<=3||done)spdlog::info("Candidate probe skipped ({}/32): {}",attempts,error->message);
        return;
    }
    // Current thread already owns the engine critical section. No lock is
    // acquired here; borrowed resource references never survive this callback.
    done=true; // A failed GPU/file operation is not retried every frame.
    const auto& pointers=std::get<std::array<std::uintptr_t,3>>(candidates);
    std::array<ID3D11Texture2D*,3> textures{};
    constexpr std::array labels{"main-colour-candidate","motion-candidate","depth-candidate"};
    for(std::size_t i=0;i<textures.size();++i) {
        textures[i]=reinterpret_cast<ID3D11Texture2D*>(pointers[i]);
        D3D11_TEXTURE2D_DESC d{};textures[i]->GetDesc(&d);
        spdlog::info("Candidate {}: {}x{} format={} mips={} array={} samples={} bind=0x{:x}",labels[i],d.Width,d.Height,
            static_cast<unsigned>(d.Format),d.MipLevels,d.ArraySize,d.SampleDesc.Count,d.BindFlags);
    }
    const auto result=readbackCandidates(context.Get(),textures);
    if(const auto error=std::get_if<Error>(&result)) { spdlog::warn("Candidate readback failed: {}",error->message);return; }
    const auto directory=captureDirectory();
    std::ofstream manifest(directory/"manifest.txt");manifest.exceptions(std::ios::failbit|std::ios::badbit);
    manifest<<"RazKolbas 0.1.5 candidate-only capture; before ENB Present; no world/pre-UI/guide semantics proven\n";
    manifest<<"thread="<<GetCurrentThreadId()<<" rendererLockOwned=true\n";
    const auto& images=std::get<std::vector<ProbeImage>>(result);
    for(std::size_t i=0;i<images.size();++i) {
        const auto& frame=images[i];const auto& d=frame.descriptor;
        std::ofstream file(directory/(std::string(labels[i])+".raw"),std::ios::binary);
        file.exceptions(std::ios::failbit|std::ios::badbit);
        file.write(reinterpret_cast<const char*>(frame.pixels.data()),static_cast<std::streamsize>(frame.pixels.size()));file.close();
        manifest<<labels[i]<<" width="<<d.Width<<" height="<<d.Height<<" format="<<d.Format<<" rowBytes="<<frame.rowBytes
            <<" bytes="<<frame.pixels.size()<<" sha256="<<sha256(frame.pixels)<<"\n";
    }
    manifest<<"complete=true\n";manifest.close();
    spdlog::info("Candidate readback complete: {}; CPU bytes only; all staging resources released",directory.string());
}
}
