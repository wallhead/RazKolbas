#include "rk/MenuInputHook.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>

namespace rk {
Result<bool> verifySkyrim1170InputDispatchCall(
    std::span<const std::uint8_t> caller) noexcept {
    constexpr std::array<std::uint8_t,8> prefix{
        0x48,0x89,0x4c,0x24,0x40,0x48,0x8b,0xce};
    constexpr std::array<std::uint8_t,19> suffix{
        0x48,0x8b,0x0d,0xa1,0x84,0x4d,0x02,
        0xe8,0xd4,0x1c,0x00,0x00,0x0f,0xb6,0x86,0xe0,0x00,0x00,0x00};
    if(caller.size()<32||
       !std::equal(prefix.begin(),prefix.end(),caller.begin())||
       caller[8]!=0xe8||
       !std::equal(suffix.begin(),suffix.end(),caller.begin()+13))
        return Error{ErrorCode::Conflict,
            "Skyrim input-dispatch caller or continuation differs"};
    return true;
}

Result<ChainedInputCall> prepareChainedInputCall(std::uintptr_t site,
    std::span<const std::uint8_t> call) noexcept {
    if(!site||call.size()<5||call[0]!=0xe8||
       site>std::numeric_limits<std::uintptr_t>::max()-5)
        return Error{ErrorCode::Conflict,"Invalid chained direct CALL"};
    std::int32_t displacement{};
    std::memcpy(&displacement,call.data()+1,sizeof(displacement));
    const auto next=site+5;
    std::uintptr_t target{};
    if(displacement>=0) {
        const auto distance=static_cast<std::uintptr_t>(displacement);
        if(next>std::numeric_limits<std::uintptr_t>::max()-distance)
            return Error{ErrorCode::Conflict,"Chained CALL target overflows"};
        target=next+distance;
    } else {
        const auto distance=static_cast<std::uintptr_t>(-
            static_cast<std::int64_t>(displacement));
        if(next<distance)
            return Error{ErrorCode::Conflict,"Chained CALL target underflows"};
        target=next-distance;
    }
    const auto base=std::min(site,target)&
        ~static_cast<std::uintptr_t>(std::numeric_limits<std::uint32_t>::max());
    if(site-base>std::numeric_limits<std::uint32_t>::max()||
       target-base>std::numeric_limits<std::uint32_t>::max())
        return Error{ErrorCode::Conflict,
            "Chained CALL span exceeds the patch representation"};
    std::array<std::uint8_t,5> original{};
    std::copy_n(call.begin(),original.size(),original.begin());
    return ChainedInputCall{base,target,
        {std::string(menuInputDispatchPatchId),
         static_cast<std::uint32_t>(site-base),
         static_cast<std::uint32_t>(target-base),original}};
}

void* const* selectMenuInputEvents(bool capture,void* const* events) noexcept {
    static void* const empty[]{nullptr};
    return capture?empty:events;
}
}
