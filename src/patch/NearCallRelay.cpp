#include "rk/CallSite.hpp"
#include <Windows.h>
#include <algorithm>
#include <cstring>
#include <limits>
#include <utility>

namespace rk {
NearCallRelay::~NearCallRelay() { if(memory_)VirtualFree(memory_,0,MEM_RELEASE); }
NearCallRelay::NearCallRelay(NearCallRelay&& other) noexcept:
    memory_(std::exchange(other.memory_,nullptr)),callBytes_(other.callBytes_),target_(other.target_) {}

Result<NearCallRelay> prepareNearCallRelay(const CallSitePlan& plan,
    std::uintptr_t imageBase,std::uintptr_t target) {
    if(!imageBase||!target)
        return Error{ErrorCode::InvalidInput,"Null image base or relay target"};
    constexpr auto maximum=std::numeric_limits<std::uintptr_t>::max();
    if(imageBase>maximum-plan.siteRva-5)
        return Error{ErrorCode::Conflict,"CALL site address overflows"};
    const auto next=imageBase+plan.siteRva+5;
    if(const auto checked=encodeCallSiteReplacement(plan,imageBase,next);
       std::holds_alternative<Error>(checked))return std::get<Error>(checked);
    const auto jump=encodeRegisterPreservingJump(target);
    if(const auto error=std::get_if<Error>(&jump))return *error;

    SYSTEM_INFO system{};
    GetSystemInfo(&system);
    const auto granularity=static_cast<std::uintptr_t>(system.dwAllocationGranularity);
    const auto pageSize=static_cast<std::uintptr_t>(system.dwPageSize);
    if(!granularity||pageSize<14)
        return Error{ErrorCode::Unavailable,"Invalid system allocation geometry"};
    const auto applicationLow=reinterpret_cast<std::uintptr_t>(system.lpMinimumApplicationAddress);
    const auto applicationHigh=reinterpret_cast<std::uintptr_t>(system.lpMaximumApplicationAddress);
    const auto lower=std::max(applicationLow,next>=0x80000000ULL?next-0x80000000ULL:0ULL);
    const auto upper=std::min(applicationHigh,
        next>maximum-0x7fffffffULL?maximum:next+0x7fffffffULL);
    if(lower>upper)return Error{ErrorCode::Unavailable,"No address range within CALL reach"};

    for(auto cursor=lower;cursor<=upper;) {
        MEMORY_BASIC_INFORMATION region{};
        if(!VirtualQuery(reinterpret_cast<const void*>(cursor),&region,sizeof(region)))
            return Error{ErrorCode::Io,"Cannot inspect candidate relay region"};
        const auto base=reinterpret_cast<std::uintptr_t>(region.BaseAddress);
        if(!region.RegionSize||base>maximum-region.RegionSize)
            return Error{ErrorCode::Io,"Invalid candidate relay region"};
        const auto end=base+region.RegionSize;
        if(end<=cursor)return Error{ErrorCode::Io,"Candidate relay scan did not advance"};
        if(region.State==MEM_FREE) {
            const auto start=std::max(base,cursor);
            if(start<=maximum-(granularity-1)) {
                auto candidate=(start+granularity-1)/granularity*granularity;
                while(candidate<=upper&&candidate<end&&pageSize<=end-candidate) {
                    void* memory=VirtualAlloc(reinterpret_cast<void*>(candidate),pageSize,
                        MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
                    if(memory) {
                        const auto call=encodeCallSiteReplacement(plan,imageBase,
                            reinterpret_cast<std::uintptr_t>(memory));
                        if(const auto error=std::get_if<Error>(&call)) {
                            VirtualFree(memory,0,MEM_RELEASE);
                            return *error;
                        }
                        const auto& code=std::get<std::array<std::uint8_t,14>>(jump);
                        std::memcpy(memory,code.data(),code.size());
                        DWORD previous{};
                        if(!VirtualProtect(memory,pageSize,PAGE_EXECUTE_READ,&previous)||
                           !FlushInstructionCache(GetCurrentProcess(),memory,code.size())||
                           std::memcmp(memory,code.data(),code.size())) {
                            VirtualFree(memory,0,MEM_RELEASE);
                            return Error{ErrorCode::Io,"Cannot seal and verify near relay"};
                        }
                        return NearCallRelay(memory,std::get<std::array<std::uint8_t,5>>(call),target);
                    }
                    if(candidate>maximum-granularity)break;
                    candidate+=granularity;
                }
            }
        }
        cursor=end;
    }
    return Error{ErrorCode::Unavailable,"No free executable relay page within CALL reach"};
}
}
