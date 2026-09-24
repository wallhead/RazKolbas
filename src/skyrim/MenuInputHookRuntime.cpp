#include "rk/MenuInputHook.hpp"
#include "rk/DiagnosticsMenu.hpp"
#include "rk/RendererHook.hpp"
#include "rk/SwapObserver.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <filesystem>
#include <memory>

namespace rk {
namespace {
using InputDispatchFn=void(*)(void*,void* const*);
std::atomic<InputDispatchFn> nextInputDispatch{};
std::atomic<bool> installed{};

bool readExact(std::uintptr_t address,void* output,std::size_t size) noexcept {
    SIZE_T copied{};
    return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(address),
        output,size,&copied)&&copied==size;
}

bool executableAddress(std::uintptr_t address) noexcept {
    MEMORY_BASIC_INFORMATION region{};
    if(!address||VirtualQuery(reinterpret_cast<const void*>(address),&region,
        sizeof(region))!=sizeof(region)||region.State!=MEM_COMMIT||
       (region.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
    const auto access=region.Protect&0xff;
    return access==PAGE_EXECUTE||access==PAGE_EXECUTE_READ||
        access==PAGE_EXECUTE_READWRITE||access==PAGE_EXECUTE_WRITECOPY;
}

std::uintptr_t relayDestination(std::uintptr_t target) noexcept {
    std::array<std::uint8_t,14> bytes{};
    if(!readExact(target,bytes.data(),bytes.size())||
       bytes[0]!=0xff||bytes[1]!=0x25||bytes[2]!=0||bytes[3]!=0||
       bytes[4]!=0||bytes[5]!=0)return target;
    std::uintptr_t destination{};
    std::memcpy(&destination,bytes.data()+6,sizeof(destination));
    return destination;
}

void inputDispatchProxy(void* dispatcher,void* const* events) noexcept {
    const auto next=nextInputDispatch.load(std::memory_order_acquire);
    if(!next)return;
    next(dispatcher,selectMenuInputEvents(
        diagnosticsMenuCapturingInput(),events));
}
}

Result<bool> installMenuInputDispatchHook(HMODULE game,
    std::string_view verifiedGameHash,const Settings& settings) {
    if(!settings.get<bool>("Interface.Enabled")||
       !settings.get<bool>("Patching.EnableVersionedPatches")||
       !settings.get<bool>("Patching.ExperimentalPatches")||
       patchDisabled(settings.get<Text>("Patching.DisabledPatchIds").value,
           menuInputDispatchPatchId))return false;
    if(installed.load(std::memory_order_acquire))return true;
    const auto& profile=skyrim1170CreationProfile();
    if(!game||verifiedGameHash!=profile.gameSha256)
        return Error{ErrorCode::Unsupported,
            "Menu input hook executable identity differs"};

    constexpr std::uint32_t siteRva=0xcd8fbb;
    constexpr std::size_t prefixSize=8;
    const auto base=reinterpret_cast<std::uintptr_t>(game);
    std::array<std::uint8_t,32> caller{};
    if(!readExact(base+siteRva-prefixSize,caller.data(),caller.size()))
        return Error{ErrorCode::Io,"Cannot read Skyrim input-dispatch caller"};
    if(const auto verified=verifySkyrim1170InputDispatchCall(caller);
       const auto error=std::get_if<Error>(&verified))return *error;

    std::int32_t displacement{};
    std::memcpy(&displacement,caller.data()+prefixSize+1,sizeof(displacement));
    const auto site=base+siteRva;
    const auto prior=static_cast<std::uintptr_t>(
        static_cast<std::int64_t>(site+5)+displacement);
    const auto destination=relayDestination(prior);
    if(!executableAddress(prior)||!executableAddress(destination))
        return Error{ErrorCode::Conflict,
            "Existing input-dispatch chain is not executable"};

    HMODULE owner{};
    std::filesystem::path ownerPath;
    if(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(destination),&owner)&&owner) {
        wchar_t path[32768]{};
        const auto count=GetModuleFileNameW(owner,path,32768);
        if(count&&count<32768)ownerPath=std::filesystem::path(path).filename();
    }

    std::array<std::uint8_t,5> currentCall{};
    std::copy_n(caller.begin()+prefixSize,currentCall.size(),currentCall.begin());
    const auto chainedPlan=prepareChainedInputCall(site,currentCall);
    if(const auto error=std::get_if<Error>(&chainedPlan))return *error;
    const auto& chain=std::get<ChainedInputCall>(chainedPlan);
    if(chain.priorTarget!=prior)
        return Error{ErrorCode::Conflict,"Decoded input chain target changed"};
    auto prepared=prepareNearCallRelay(chain.plan,chain.syntheticBase,
        reinterpret_cast<std::uintptr_t>(&inputDispatchProxy));
    if(const auto error=std::get_if<Error>(&prepared))return *error;
    auto relay=std::make_unique<NearCallRelay>(
        std::move(std::get<NearCallRelay>(prepared)));
    HMODULE pinnedSelf{};
    constexpr DWORD pinFlags=GET_MODULE_HANDLE_EX_FLAG_PIN|
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS;
    if(!GetModuleHandleExW(pinFlags,reinterpret_cast<LPCWSTR>(&inputDispatchProxy),
        &pinnedSelf))
        return Error{ErrorCode::Unavailable,"Cannot pin menu input callback DLL"};
    nextInputDispatch.store(reinterpret_cast<InputDispatchFn>(prior),
        std::memory_order_release);
    const auto applied=applyCallInstruction(chain.plan,chain.syntheticBase,*relay,
        CallWriteBoundary::SkyrimStartupBeforeWorldThreads);
    if(const auto error=std::get_if<Error>(&applied))return *error;
    installed.store(true,std::memory_order_release);
    relay.release();
    spdlog::info("Installed {}: CALL RVA=0x{:x}; prior chain=0x{:x}; owner={}; visible menu dispatches an empty event list",
        menuInputDispatchPatchId,siteRva,prior,
        ownerPath.empty()?"private executable relay":ownerPath.string());
    return true;
}
}
