#include "rk/CallSite.hpp"
#include <algorithm>
#include <cstring>
#include <limits>

namespace rk {
Result<CallSitePlan> prepareCallSite(std::span<const std::uint8_t> live,
    std::string_view verifiedGameHash,std::size_t imageSize,const CallSiteDescriptor& d) {
    const auto reject=[](const char* message)->Result<CallSitePlan> {
        return Error{ErrorCode::Conflict,message};
    };
    if(d.id.empty()||d.gameSha256.size()!=64||verifiedGameHash!=d.gameSha256||imageSize!=d.imageSize)
        return reject("Call-site executable identity differs");
    if(d.expected[0]!=0xe8||live.size()<d.expected.size()||
       !std::equal(d.expected.begin(),d.expected.end(),live.begin()))
        return reject("Call-site live bytes differ from the direct CALL contract");
    if(d.siteRva>imageSize||imageSize-d.siteRva<d.expected.size())
        return reject("Call-site outside verified image");
    std::int32_t displacement{};
    std::memcpy(&displacement,d.expected.data()+1,sizeof(displacement));
    const auto target=static_cast<std::int64_t>(d.siteRva)+5+displacement;
    if(target<0||static_cast<std::uint64_t>(target)>=imageSize||
       static_cast<std::uint64_t>(target)!=d.originalTargetRva)
        return reject("Call-site original target differs from verified engine target");
    return CallSitePlan{d.id,d.siteRva,d.originalTargetRva,d.expected};
}

Result<std::array<std::uint8_t,5>> encodeCallSiteReplacement(const CallSitePlan& plan,
    std::uintptr_t imageBase,std::uintptr_t detourTarget) {
    const auto reject=[](const char* message)->Result<std::array<std::uint8_t,5>> {
        return Error{ErrorCode::Conflict,message};
    };
    if(plan.id.empty()||plan.original[0]!=0xe8)
        return reject("Invalid prepared direct CALL");
    std::int32_t originalDisplacement{};
    std::memcpy(&originalDisplacement,plan.original.data()+1,sizeof(originalDisplacement));
    const auto originalTarget=static_cast<std::int64_t>(plan.siteRva)+5+originalDisplacement;
    if(originalTarget<0||static_cast<std::uint64_t>(originalTarget)!=plan.originalTargetRva)
        return reject("Prepared CALL original target changed");
    constexpr auto maximum=std::numeric_limits<std::uintptr_t>::max();
    if(imageBase>maximum-plan.siteRva-5)
        return reject("CALL site address overflows");
    const auto nextInstruction=imageBase+plan.siteRva+5;
    std::int32_t displacement{};
    if(detourTarget>=nextInstruction) {
        const auto distance=detourTarget-nextInstruction;
        if(distance>static_cast<std::uintptr_t>(std::numeric_limits<std::int32_t>::max()))
            return reject("Detour target is beyond direct CALL reach");
        displacement=static_cast<std::int32_t>(distance);
    } else {
        const auto distance=nextInstruction-detourTarget;
        constexpr auto negativeLimit=static_cast<std::uintptr_t>(std::numeric_limits<std::int32_t>::max())+1;
        if(distance>negativeLimit)
            return reject("Detour target is beyond direct CALL reach");
        displacement=distance==negativeLimit?std::numeric_limits<std::int32_t>::min():
            -static_cast<std::int32_t>(distance);
    }
    std::array<std::uint8_t,5> replacement{0xe8,0,0,0,0};
    std::memcpy(replacement.data()+1,&displacement,sizeof(displacement));
    return replacement;
}
Result<std::array<std::uint8_t,14>> encodeRegisterPreservingJump(std::uintptr_t target) {
    if(!target)return Error{ErrorCode::InvalidInput,"Null relay target"};
    std::array<std::uint8_t,14> code{0xff,0x25,0,0,0,0};
    static_assert(sizeof(target)==8);
    std::memcpy(code.data()+6,&target,sizeof(target));
    return code;
}
}
