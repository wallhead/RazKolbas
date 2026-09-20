#include "rk/CallSite.hpp"
#include <Windows.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <exception>
#include <limits>

namespace rk {
namespace {
using Bytes=std::array<std::uint8_t,5>;

Result<bool> writeIfOwned(const CallSitePlan& plan,std::uintptr_t imageBase,
    const NearCallRelay& relay,CallWriteBoundary boundary,bool restore) {
    if(boundary==CallWriteBoundary::None)
        return Error{ErrorCode::Conflict,"No exclusive execution boundary for CALL write"};
    if(!relay.entry()||!relay.target()||!imageBase)
        return Error{ErrorCode::InvalidInput,"Missing CALL site or relay"};
    const auto replacement=encodeCallSiteReplacement(plan,imageBase,
        reinterpret_cast<std::uintptr_t>(relay.entry()));
    if(const auto error=std::get_if<Error>(&replacement))return *error;
    const auto& installed=std::get<Bytes>(replacement);
    if(installed!=relay.callBytes()||installed==plan.original)
        return Error{ErrorCode::Conflict,"Relay does not own prepared CALL replacement"};
    const auto jump=encodeRegisterPreservingJump(relay.target());
    if(const auto error=std::get_if<Error>(&jump))return *error;
    MEMORY_BASIC_INFORMATION relayRegion{};
    if(VirtualQuery(relay.entry(),&relayRegion,sizeof(relayRegion))!=sizeof(relayRegion)||
       relayRegion.State!=MEM_COMMIT||relayRegion.Protect!=PAGE_EXECUTE_READ||
       std::memcmp(relay.entry(),std::get<std::array<std::uint8_t,14>>(jump).data(),14))
        return Error{ErrorCode::Conflict,"Near relay bytes or protection changed"};

    constexpr auto maximum=std::numeric_limits<std::uintptr_t>::max();
    if(imageBase>maximum-plan.siteRva-5)
        return Error{ErrorCode::Conflict,"CALL site address overflows"};
    const auto address=imageBase+plan.siteRva;
    auto* site=reinterpret_cast<std::uint8_t*>(address);
    MEMORY_BASIC_INFORMATION region{};
    if(VirtualQuery(site,&region,sizeof(region))!=sizeof(region)||region.State!=MEM_COMMIT||
       (region.Protect&PAGE_GUARD)||region.Protect==PAGE_NOACCESS)
        return Error{ErrorCode::Conflict,"CALL site is not readable committed code"};
    const auto protection=region.Protect&0xff;
    if(protection!=PAGE_EXECUTE&&protection!=PAGE_EXECUTE_READ&&
       protection!=PAGE_EXECUTE_READWRITE&&protection!=PAGE_EXECUTE_WRITECOPY)
        return Error{ErrorCode::Conflict,"CALL site is not executable code"};
    const auto regionBase=reinterpret_cast<std::uintptr_t>(region.BaseAddress);
    if(address<regionBase||address-regionBase>region.RegionSize||
       region.RegionSize-(address-regionBase)<plan.original.size())
        return Error{ErrorCode::Conflict,"CALL write crosses a protection region"};
    const auto& expected=restore?installed:plan.original;
    const auto& desired=restore?plan.original:installed;
    Bytes current{};
    SIZE_T copied{};
    if(!ReadProcessMemory(GetCurrentProcess(),site,current.data(),current.size(),&copied)||copied!=current.size())
        return Error{ErrorCode::Io,"Cannot read live CALL instruction"};
    if(current==desired)return true; // Idempotent after exact owner validation.
    if(current!=expected)
        return Error{ErrorCode::Conflict,"CALL bytes changed; refusing another owner's instruction"};

    DWORD oldProtection{};
    if(!VirtualProtect(site,desired.size(),PAGE_READWRITE,&oldProtection))
        return Error{ErrorCode::Io,"CALL write protection change failed; no bytes written"};
    // The caller's exclusive execution boundary must remain valid here.
    if(std::memcmp(site,expected.data(),expected.size())) {
        DWORD ignored{};
        if(!VirtualProtect(site,desired.size(),oldProtection,&ignored))std::terminate();
        return Error{ErrorCode::Conflict,"CALL bytes changed during protected write"};
    }
    std::memcpy(site,desired.data(),desired.size());
    DWORD ignored{};
    // Never resume the caller after an unverified partial code write.
    if(!VirtualProtect(site,desired.size(),oldProtection,&ignored)||
       !FlushInstructionCache(GetCurrentProcess(),site,desired.size())||
       std::memcmp(site,desired.data(),desired.size()))std::terminate();
    return true;
}
}
Result<bool> applyCallInstruction(const CallSitePlan& plan,std::uintptr_t imageBase,
    const NearCallRelay& relay,CallWriteBoundary boundary) {
    return writeIfOwned(plan,imageBase,relay,boundary,false);
}
Result<bool> restoreCallInstruction(const CallSitePlan& plan,std::uintptr_t imageBase,
    const NearCallRelay& relay,CallWriteBoundary boundary) {
    return writeIfOwned(plan,imageBase,relay,boundary,true);
}
}
