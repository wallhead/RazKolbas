#include "rk/RipCall6.hpp"
#include <Windows.h>
#include <algorithm>
#include <cstring>
#include <exception>
#include <limits>
#include <utility>

namespace rk {
NearRipCall6Cell::~NearRipCall6Cell() { if(memory_)VirtualFree(memory_,0,MEM_RELEASE); }
NearRipCall6Cell::NearRipCall6Cell(NearRipCall6Cell&& other) noexcept:
    memory_(std::exchange(other.memory_,nullptr)),callback_(other.callback_),
    callBytes_(other.callBytes_) {}

Result<NearRipCall6Cell> prepareNearRipCall6Cell(const RipCall6Plan& plan,
    std::uintptr_t imageBase,std::uintptr_t callback) {
    constexpr auto maximum=std::numeric_limits<std::uintptr_t>::max();
    if(plan.id.empty()||!imageBase||!callback||imageBase>maximum-plan.siteRva-6)
        return Error{ErrorCode::InvalidInput,"Invalid indirect CALL plan or callback"};
    const auto site=imageBase+plan.siteRva;
    const auto original=decodeRipCall6(site,plan.original);
    if(const auto error=std::get_if<Error>(&original))return *error;
    if(imageBase>maximum-plan.originalCellRva||
       std::get<std::uintptr_t>(original)!=imageBase+plan.originalCellRva)
        return Error{ErrorCode::Conflict,"Indirect CALL plan original cell changed"};
    SYSTEM_INFO system{};GetSystemInfo(&system);
    const auto granularity=static_cast<std::uintptr_t>(system.dwAllocationGranularity);
    const auto pageSize=static_cast<std::uintptr_t>(system.dwPageSize);
    if(!granularity||pageSize<sizeof(callback))
        return Error{ErrorCode::Unavailable,"Invalid pointer-cell allocation geometry"};
    const auto next=site+6;
    const auto low=std::max(reinterpret_cast<std::uintptr_t>(system.lpMinimumApplicationAddress),
        next>=0x80000000ULL?next-0x80000000ULL:0ULL);
    const auto high=std::min(reinterpret_cast<std::uintptr_t>(system.lpMaximumApplicationAddress),
        next>maximum-0x7fffffffULL?maximum:next+0x7fffffffULL);
    if(low>high)return Error{ErrorCode::Unavailable,"No address range for indirect CALL cell"};
    for(auto cursor=low;cursor<=high;) {
        MEMORY_BASIC_INFORMATION region{};
        if(!VirtualQuery(reinterpret_cast<const void*>(cursor),&region,sizeof(region)))
            return Error{ErrorCode::Io,"Cannot inspect candidate pointer-cell region"};
        const auto base=reinterpret_cast<std::uintptr_t>(region.BaseAddress);
        if(!region.RegionSize||base>maximum-region.RegionSize)
            return Error{ErrorCode::Io,"Invalid pointer-cell candidate region"};
        const auto end=base+region.RegionSize;
        if(end<=cursor)return Error{ErrorCode::Io,"Pointer-cell scan did not advance"};
        if(region.State==MEM_FREE) {
            const auto start=std::max(base,cursor);
            if(start<=maximum-(granularity-1)) {
                auto candidate=(start+granularity-1)/granularity*granularity;
                while(candidate<=high&&candidate<end&&pageSize<=end-candidate) {
                    void* memory=VirtualAlloc(reinterpret_cast<void*>(candidate),pageSize,
                        MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
                    if(memory) {
                        const auto bytes=encodeRipCall6(site,reinterpret_cast<std::uintptr_t>(memory));
                        if(const auto error=std::get_if<Error>(&bytes)) {
                            VirtualFree(memory,0,MEM_RELEASE);return *error;
                        }
                        std::memcpy(memory,&callback,sizeof(callback));
                        DWORD previous{};
                        if(!VirtualProtect(memory,pageSize,PAGE_READONLY,&previous)||
                           std::memcmp(memory,&callback,sizeof(callback))) {
                            VirtualFree(memory,0,MEM_RELEASE);
                            return Error{ErrorCode::Io,"Cannot seal and verify indirect CALL cell"};
                        }
                        return NearRipCall6Cell(memory,callback,
                            std::get<std::array<std::uint8_t,6>>(bytes));
                    }
                    if(candidate>maximum-granularity)break;
                    candidate+=granularity;
                }
            }
        }
        cursor=end;
    }
    return Error{ErrorCode::Unavailable,"No nearby pointer-cell page"};
}

namespace {
Result<bool> writeOwned(const RipCall6Plan& plan,std::uintptr_t imageBase,
    const NearRipCall6Cell& cell,CallWriteBoundary boundary,bool restore) {
    if(boundary==CallWriteBoundary::None)
        return Error{ErrorCode::Conflict,"No exclusive boundary for indirect CALL write"};
    if(!cell.address()||!cell.callback()||!imageBase)
        return Error{ErrorCode::InvalidInput,"Missing indirect CALL cell"};
    constexpr auto maximum=std::numeric_limits<std::uintptr_t>::max();
    if(imageBase>maximum-plan.siteRva-6)
        return Error{ErrorCode::Conflict,"Indirect CALL site address overflows"};
    const auto siteAddress=imageBase+plan.siteRva;
    const auto encoded=encodeRipCall6(siteAddress,reinterpret_cast<std::uintptr_t>(cell.address()));
    if(const auto error=std::get_if<Error>(&encoded))return *error;
    if(std::get<std::array<std::uint8_t,6>>(encoded)!=cell.callBytes()||
       cell.callBytes()==plan.original)
        return Error{ErrorCode::Conflict,"Pointer cell does not own this replacement"};
    MEMORY_BASIC_INFORMATION cellRegion{};
    const auto callback=cell.callback();
    if(VirtualQuery(cell.address(),&cellRegion,sizeof(cellRegion))!=sizeof(cellRegion)||
       cellRegion.State!=MEM_COMMIT||cellRegion.Protect!=PAGE_READONLY||
       std::memcmp(cell.address(),&callback,sizeof(callback)))
        return Error{ErrorCode::Conflict,"Pointer cell or callback changed"};
    auto* site=reinterpret_cast<std::uint8_t*>(siteAddress);
    MEMORY_BASIC_INFORMATION region{};
    if(VirtualQuery(site,&region,sizeof(region))!=sizeof(region)||region.State!=MEM_COMMIT||
       (region.Protect&PAGE_GUARD)||region.Protect==PAGE_NOACCESS)
        return Error{ErrorCode::Conflict,"Indirect CALL site not readable code"};
    const auto protection=region.Protect&0xff;
    if(protection!=PAGE_EXECUTE&&protection!=PAGE_EXECUTE_READ&&
       protection!=PAGE_EXECUTE_READWRITE&&protection!=PAGE_EXECUTE_WRITECOPY)
        return Error{ErrorCode::Conflict,"Indirect CALL site is not executable"};
    const auto regionBase=reinterpret_cast<std::uintptr_t>(region.BaseAddress);
    if(siteAddress<regionBase||siteAddress-regionBase>region.RegionSize||
       region.RegionSize-(siteAddress-regionBase)<6)
        return Error{ErrorCode::Conflict,"Indirect CALL crosses protection region"};
    const auto& expected=restore?cell.callBytes():plan.original;
    const auto& desired=restore?plan.original:cell.callBytes();
    std::array<std::uint8_t,6> current{};SIZE_T copied{};
    if(!ReadProcessMemory(GetCurrentProcess(),site,current.data(),6,&copied)||copied!=6)
        return Error{ErrorCode::Io,"Cannot read live indirect CALL"};
    if(current==desired)return true;
    if(current!=expected)
        return Error{ErrorCode::Conflict,"Indirect CALL owned by another patch"};
    DWORD oldProtection{};
    if(!VirtualProtect(site,6,PAGE_READWRITE,&oldProtection))
        return Error{ErrorCode::Io,"Cannot protect indirect CALL for write"};
    if(std::memcmp(site,expected.data(),6)) {
        DWORD ignored{};
        if(!VirtualProtect(site,6,oldProtection,&ignored))std::terminate();
        return Error{ErrorCode::Conflict,"Indirect CALL changed during protected write"};
    }
    std::memcpy(site,desired.data(),6);
    DWORD ignored{};
    if(!VirtualProtect(site,6,oldProtection,&ignored)||
       !FlushInstructionCache(GetCurrentProcess(),site,6)||
       std::memcmp(site,desired.data(),6))std::terminate();
    return true;
}
}
Result<bool> applyRipCall6(const RipCall6Plan& plan,std::uintptr_t imageBase,
    const NearRipCall6Cell& cell,CallWriteBoundary boundary) {
    return writeOwned(plan,imageBase,cell,boundary,false);
}
Result<bool> restoreRipCall6(const RipCall6Plan& plan,std::uintptr_t imageBase,
    const NearRipCall6Cell& cell,CallWriteBoundary boundary) {
    return writeOwned(plan,imageBase,cell,boundary,true);
}
}
