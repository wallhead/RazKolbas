#include "rk/FrameProbeRuntime.hpp"
#include "rk/FrameProbe.hpp"
#include "rk/CaptureTrigger.hpp"
#include "rk/PatchDescriptor.hpp"
#include <ShlObj.h>
#include <wrl/client.h>
#include <spdlog/spdlog.h>
#include <atomic>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>

namespace rk {
namespace {
std::atomic<std::uintptr_t> gameBase{0};
std::mutex probeMutex;
CaptureTrigger trigger;
HWND gameWindow=nullptr;
std::uintptr_t createdDevice=0,createdContext=0,createdSwap=0;
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
void armFrameProbe(HMODULE verifiedGame,const Settings& settings) {
    if(settings.get<Choice>("Diagnostics.CaptureHotkey").value!="CtrlShiftF10") {
        spdlog::info("Candidate capture disabled: Diagnostics.CaptureHotkey=Off; no automatic startup capture");return;
    }
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
    spdlog::info("Candidate capture armed: Ctrl+Shift+F10 while Skyrim has focus; release between requests; 5s cooldown; six requests/session; 64MiB/bundle; no automatic capture; requires renderer lock ownership");
}
void bindFrameProbe(const DeviceCreationArgs& args) {
    std::scoped_lock guard(probeMutex);
    if(createdSwap)return; // First verified renderer only; no stale rebinding.
    if(!args.device||!*args.device||!args.context||!*args.context||!args.swapChain||!*args.swapChain)return;
    DXGI_SWAP_CHAIN_DESC desc{};
    if(FAILED((*args.swapChain)->GetDesc(&desc))||!desc.OutputWindow)return;
    gameWindow=desc.OutputWindow;
    createdDevice=reinterpret_cast<std::uintptr_t>(*args.device);
    createdContext=reinterpret_cast<std::uintptr_t>(*args.context);
    createdSwap=reinterpret_cast<std::uintptr_t>(*args.swapChain);
}
void probePresentCandidates(IDXGISwapChain* swap) {
    const auto base=gameBase.load(std::memory_order_acquire);
    if(!base||!swap)return;
    std::unique_lock guard(probeMutex,std::try_to_lock);
    if(!guard)return;
    if(!createdDevice||!createdContext||createdSwap!=reinterpret_cast<std::uintptr_t>(swap))return;
    const auto now=GetTickCount64();
    const bool foreground=gameWindow&&GetForegroundWindow()==gameWindow;
    // Only the current down bit is used; the global since-last-call bit is racy.
    const bool down=foreground&&(GetAsyncKeyState(VK_CONTROL)&0x8000)&&(GetAsyncKeyState(VK_SHIFT)&0x8000)&&(GetAsyncKeyState(VK_F10)&0x8000);
    const auto signal=trigger.poll(now,foreground,down);
    if(signal==CaptureSignal::LimitReached) { spdlog::info("Candidate capture limit reached: six requests this session");return; }
    if(signal==CaptureSignal::Expired) { spdlog::warn("Candidate capture request #{} expired before a safe renderer boundary",trigger.requests());return; }
    if(signal==CaptureSignal::Interrupted) { spdlog::info("Candidate capture request #{} cancelled after a presentation gap; release the chord and try again",trigger.requests());return; }
    if(signal!=CaptureSignal::Attempt)return;
    const auto attempts=trigger.attempts();
    if(attempts==1)spdlog::info("Candidate capture request #{}/6: Ctrl+Shift+F10; next eligible owned-lock boundary, expires after 2s",trigger.requests());
    std::array<std::uint8_t,renderer1170Size> snapshot{};
    if(!read(base+renderer1170Rva,snapshot.data(),snapshot.size()))return;
    const auto number=[&](std::size_t offset){std::uintptr_t v{};std::memcpy(&v,snapshot.data()+offset,sizeof(v));return v;};
    if(attempts==1)spdlog::info("Candidate pointer provenance: renderer device=0x{:x} context=0x{:x} swap=0x{:x}; verified creation device=0x{:x} context=0x{:x} swap=0x{:x}",
        number(0x48),number(0x50),number(0x70),createdDevice,createdContext,createdSwap);
    const auto candidates=rendererCandidatePointers(snapshot,GetCurrentThreadId(),createdDevice,createdContext,createdSwap);
    if(const auto error=std::get_if<Error>(&candidates)) {
        if(attempts<=3)spdlog::info("Candidate probe skipped ({}/32): {}",attempts,error->message);
        return;
    }
    // Current thread already owns the engine critical section. No lock is
    // acquired here; borrowed resource references never survive this callback.
    trigger.consume(); // A failed GPU/file operation requires a fresh request.
    auto* device=reinterpret_cast<ID3D11Device*>(createdDevice);
    auto* context=reinterpret_cast<ID3D11DeviceContext*>(createdContext);
    DeviceCreationArgs current{};current.device=&device;current.context=&context;current.swapChain=&swap;
    const auto revalidated=captureRendererSnapshot(current,S_OK);
    if(const auto error=std::get_if<Error>(&revalidated)) {
        spdlog::warn("Candidate probe device revalidation failed: {}",error->message);return;
    }
    const auto& pointers=std::get<std::array<std::uintptr_t,3>>(candidates);
    std::array<ID3D11Texture2D*,3> textures{};
    constexpr std::array labels{"main-colour-candidate","motion-candidate","depth-candidate"};
    for(std::size_t i=0;i<textures.size();++i) {
        textures[i]=reinterpret_cast<ID3D11Texture2D*>(pointers[i]);
        D3D11_TEXTURE2D_DESC d{};textures[i]->GetDesc(&d);
        spdlog::info("Candidate {}: {}x{} format={} mips={} array={} samples={} bind=0x{:x}",labels[i],d.Width,d.Height,
            static_cast<unsigned>(d.Format),d.MipLevels,d.ArraySize,d.SampleDesc.Count,d.BindFlags);
    }
    const auto result=readbackCandidates(context,textures);
    if(const auto error=std::get_if<Error>(&result)) { spdlog::warn("Candidate readback failed: {}",error->message);return; }
    const auto directory=captureDirectory();
    std::ofstream manifest(directory/"manifest.txt");manifest.exceptions(std::ios::failbit|std::ios::badbit);
    manifest<<"RazKolbas 0.1.7 candidate-only capture; before ENB Present; no world/pre-UI/guide semantics proven\n";
    manifest<<"trigger=CtrlShiftF10 request="<<trigger.requests()<<" captureTickMs="<<now<<" attempt="<<attempts<<"\n";
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
    spdlog::info("Candidate readback complete (request #{}): {}; CPU bytes only; all staging resources released",trigger.requests(),directory.string());
}
}
