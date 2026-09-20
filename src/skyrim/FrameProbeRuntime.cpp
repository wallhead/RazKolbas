#include "rk/FrameProbeRuntime.hpp"
#include "rk/FrameProbe.hpp"
#include "rk/CaptureTrigger.hpp"
#include "rk/PatchDescriptor.hpp"
#include "rk/CallSite.hpp"
#include "rk/RendererHook.hpp"
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
template<std::size_t N>
void logLiveCode(std::uintptr_t base,std::uint32_t rva,const char* label) {
    std::array<std::uint8_t,N> bytes{};
    if(!read(base+rva,bytes.data(),bytes.size())) {
        spdlog::warn("Live code snapshot unavailable: {} RVA=0x{:x}",label,rva);return;
    }
    constexpr char digits[]="0123456789abcdef";
    std::array<char,N*2+1> hex{};
    for(std::size_t i=0;i<N;++i) {
        hex[i*2]=digits[bytes[i]>>4];hex[i*2+1]=digits[bytes[i]&15];
    }
    spdlog::info("Live code snapshot: {} RVA=0x{:x} size=0x{:x} bytes={}; read-only",
        label,rva,N,hex.data());
}
void logReferenceSites(std::uintptr_t base,std::string_view verifiedGameHash) {
    // RVA/addends were recovered from the exact supplied SkyrimUpscaler.dll
    // and the exact Skyrim 1.6.1170 Address Library. Read-only: other mods may
    // already own these instructions. No signature or hook claim is made here.
    struct Site { const char* name; std::uint32_t rva; };
    constexpr std::array sites{
        Site{"Renderer Begin jitter reference #1",0xe44675},
        Site{"Renderer Begin jitter reference #2",0xe446c3},
        Site{"Main_DrawWorld_MainDraw reference call",0xfa507a},
    };
    constexpr char digits[]="0123456789abcdef";
    for(const auto& site:sites) {
        std::array<std::uint8_t,16> bytes{};
        if(!read(base+site.rva,bytes.data(),bytes.size())) {
            spdlog::warn("Reference site read failed: {} RVA=0x{:x}",site.name,site.rva);
            continue;
        }
        std::array<char,33> hex{};
        for(std::size_t i=0;i<bytes.size();++i) {
            hex[i*2]=digits[bytes[i]>>4];hex[i*2+1]=digits[bytes[i]&15];
        }
        spdlog::info("Reference site observed: {} RVA=0x{:x} live16={}; read-only, owner/ABI not yet established",
            site.name,site.rva,hex.data());
        if(site.rva==0xfa507a) {
            const auto& game=skyrim1170CreationProfile();
            const CallSiteDescriptor descriptor{"skyrim1170.world-draw.sr-v1",
                std::string(game.gameSha256),game.imageSize,0xfa507a,0xe44850,
                {0xe8,0xd1,0xf7,0xe9,0xff}};
            const auto prepared=prepareCallSite(bytes,verifiedGameHash,game.imageSize,descriptor);
            if(const auto* plan=std::get_if<CallSitePlan>(&prepared)) {
                spdlog::info("Reference world-draw CALL contract verified: site RVA=0x{:x}; original target RVA=0x{:x}; no patch installed",
                    plan->siteRva,plan->originalTargetRva);
                std::array<std::uint8_t,17> callerAbi{};
                std::array<std::uint8_t,33> targetAbi{};
                if(read(base+0xfa5071,callerAbi.data(),callerAbi.size())&&
                   read(base+plan->originalTargetRva,targetAbi.data(),targetAbi.size())) {
                    const auto abi=verifySkyrim1170WorldCallAbi(callerAbi,targetAbi);
                    if(std::holds_alternative<bool>(abi))
                        spdlog::info("Decoded world-call ABI verified: RCX=game object, EDX=0, target reads DL, return unused; read-only");
                    else
                        spdlog::warn("Decoded world-call ABI differs: {}; no patch installed",std::get<Error>(abi).message);
                } else
                    spdlog::warn("Decoded world-call ABI unavailable; no patch installed");
                // The exact file's text is encoded on disk. Capture bounded
                // decoded live bytes to recover the caller/callee ABI offline.
                logLiveCode<0x200>(base,0xfa4f00,"Main_DrawWorld caller (AE ID 82084)");
                logLiveCode<0x100>(base,plan->originalTargetRva,"Original world target (AE ID 77247)");
            } else
                spdlog::warn("Reference world-draw CALL contract rejected: {}; no patch installed",
                    std::get<Error>(prepared).message);
        }
    }
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
void armFrameProbe(HMODULE verifiedGame,std::string_view verifiedGameHash,const Settings& settings) {
    const auto base=reinterpret_cast<std::uintptr_t>(verifiedGame);
    if(settings.get<Choice>("Diagnostics.CaptureHotkey").value!="CtrlShiftF10") {
        spdlog::info("Candidate capture disabled: Diagnostics.CaptureHotkey=Off; no automatic startup capture");return;
    }
    // Live code is decoded by the game; disk text is not a valid signature here.
    constexpr std::array<std::uint8_t,14> lock{0x48,0x81,0xc1,0xf0,0x27,0,0,0x48,0xff,0x25,0x72,0xab,0x90,0};
    constexpr std::array<std::uint8_t,14> unlock{0x48,0x81,0xc1,0xf0,0x27,0,0,0x48,0xff,0x25,0x5a,0xab,0x90,0};
    std::array<std::uint8_t,14> actual{};
    if(!read(base+0xe44550,actual.data(),actual.size())||actual!=lock||
       !read(base+0xe44570,actual.data(),actual.size())||actual!=unlock) {
        spdlog::warn("Candidate probe disabled: live renderer lock layout signature differs");return;
    }
    logReferenceSites(base,verifiedGameHash);
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
    manifest<<"RazKolbas 0.1.11 candidate-only capture; before ENB Present; no world/pre-UI/guide semantics proven\n";
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
