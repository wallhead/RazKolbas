#include "rk/CallSite.hpp"
#include <algorithm>
#include <cstring>
#include <limits>

namespace rk {
Result<bool> verifySkyrim1170WorldCallAbi(std::span<const std::uint8_t> caller,
    std::span<const std::uint8_t> originalTarget) {
    // Caller starts at RVA fa5071: xor edx,edx; lea rcx,[rip+22e3746];
    // CALL e44850; mov rax,[rip+...] (return is discarded).
    constexpr std::array<std::uint8_t,17> expectedCaller{
        0x33,0xd2,0x48,0x8d,0x0d,0x46,0x37,0x2e,0x02,
        0xe8,0xd1,0xf7,0xe9,0xff,0x48,0x8b,0x05};
    // Target starts at RVA e44850: RCX saved in RBP; DL zero-extended into
    // R15D before branch-dependent renderer work.
    constexpr std::array<std::uint8_t,33> expectedTarget{
        0x4c,0x8b,0xdc,0x53,0x48,0x81,0xec,0x80,0,0,0,
        0x44,0x8b,0x05,0x0e,0x63,0x1e,0x01,0x49,0x89,0x6b,0x08,
        0x48,0x8b,0xe9,0x4d,0x89,0x7b,0xe8,0x44,0x0f,0xb6,0xfa};
    if(caller.size()<expectedCaller.size()||originalTarget.size()<expectedTarget.size()||
       !std::equal(expectedCaller.begin(),expectedCaller.end(),caller.begin())||
       !std::equal(expectedTarget.begin(),expectedTarget.end(),originalTarget.begin()))
        return Error{ErrorCode::Conflict,"Decoded world-call argument or target prologue differs"};
    return true;
}
Result<bool> verifySkyrim1170DrsAbi(std::span<const std::uint8_t> caller,
    std::span<const std::uint8_t> originalTarget,
    std::span<const std::uint8_t> scissor) {
    // At RVA 643c26: LEA RCX, graphics state; CALL the vanilla DRS policy;
    // then resume renderer setup. The callee checks a lock counter and reads
    // current ratio floats before copying them to previous-ratio fields.
    constexpr std::array<std::uint8_t,19> expectedCaller{
        0x48,0x8d,0x0d,0xf3,0x8f,0xc4,0x02,
        0xe8,0xbe,0x4b,0x81,0x00,
        0x48,0x8d,0x0d,0x87,0x4b,0xc4,0x02};
    constexpr std::array<std::uint8_t,55> expectedTarget{
        0x83,0xb9,0x18,0x01,0x00,0x00,0x00,0x48,0x8b,0xd1,
        0x0f,0x85,0x6a,0x01,0x00,0x00,0x80,0xb9,0x1e,0x01,0x00,0x00,0x00,
        0xf3,0x0f,0x10,0x99,0x04,0x01,0x00,0x00,
        0xf3,0x0f,0x10,0x81,0x08,0x01,0x00,0x00,
        0xf3,0x0f,0x11,0x99,0x0c,0x01,0x00,0x00,
        0xf3,0x0f,0x11,0x81,0x10,0x01,0x00,0x00};
    // At RVA e4adf0: Win64 renderer,x,y,width,height; constructs a RECT
    // {x,y,x+width,y+height} for the renderer vtable call at +0x168.
    constexpr std::array<std::uint8_t,63> expectedScissor{
        0x48,0x83,0xec,0x38,0x8b,0x4c,0x24,0x60,0x42,0x8d,0x04,0x0a,
        0x41,0x03,0xc8,0x44,0x89,0x44,0x24,0x24,0x89,0x4c,0x24,0x2c,
        0x4c,0x8d,0x44,0x24,0x20,0x48,0x8b,0x0d,0x9c,0xd9,0x43,0x02,
        0x89,0x54,0x24,0x20,0xba,0x01,0x00,0x00,0x00,0x89,0x44,0x24,0x28,
        0x48,0x8b,0x01,0xff,0x90,0x68,0x01,0x00,0x00,0x48,0x83,0xc4,0x38,0xc3};
    if(caller.size()<expectedCaller.size()||originalTarget.size()<expectedTarget.size()||
       scissor.size()<expectedScissor.size()||
       !std::equal(expectedCaller.begin(),expectedCaller.end(),caller.begin())||
       !std::equal(expectedTarget.begin(),expectedTarget.end(),originalTarget.begin())||
       !std::equal(expectedScissor.begin(),expectedScissor.end(),scissor.begin()))
        return Error{ErrorCode::Conflict,"Decoded DRS call, policy or scissor ABI differs"};
    return true;
}
Result<bool> verifySkyrim1170JitterCallAbi(std::span<const std::uint8_t> caller,
    std::span<const std::uint8_t> originalTarget) {
    // Renderer Begin RVA e44664: LEA RCX, BSGraphics::State; set a byte;
    // CALL e58a10; return value unused. The target uses that state pointer.
    constexpr std::array<std::uint8_t,23> expectedCaller{
        0x48,0x8d,0x0d,0xb5,0x85,0x44,0x02,
        0xc6,0x05,0x02,0x86,0x44,0x02,0x01,
        0xe8,0x99,0x43,0x01,0x00,
        0x48,0x8b,0x0d,0x32};
    constexpr std::array<std::uint8_t,17> expectedTarget{
        0x48,0x8b,0x05,0x89,0x1c,0x4d,0x02,0x0f,0x57,0xc0,
        0x48,0x8b,0x90,0xf0,0x01,0x00,0x00};
    if(caller.size()<expectedCaller.size()||originalTarget.size()<expectedTarget.size()||
       !std::equal(expectedCaller.begin(),expectedCaller.end(),caller.begin())||
       !std::equal(expectedTarget.begin(),expectedTarget.end(),originalTarget.begin()))
        return Error{ErrorCode::Conflict,"Decoded Renderer Begin jitter argument or target differs"};
    return true;
}
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
