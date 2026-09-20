#include "rk/RipCall6.hpp"
#include <algorithm>
#include <cstring>
#include <limits>

namespace rk {
Result<std::uintptr_t> decodeRipCall6(std::uintptr_t site,
    std::span<const std::uint8_t> bytes) {
    constexpr auto maximum=std::numeric_limits<std::uintptr_t>::max();
    if(bytes.size()<6||bytes[0]!=0xff||bytes[1]!=0x15||site>maximum-6)
        return Error{ErrorCode::Conflict,"Invalid six-byte indirect CALL"};
    std::int32_t displacement{};
    std::memcpy(&displacement,bytes.data()+2,sizeof(displacement));
    const auto next=site+6;
    if(displacement<0) {
        const auto distance=static_cast<std::uintptr_t>(-static_cast<std::int64_t>(displacement));
        if(next<distance)return Error{ErrorCode::Conflict,"Indirect CALL cell underflows"};
        return next-distance;
    }
    const auto distance=static_cast<std::uintptr_t>(displacement);
    if(next>maximum-distance)return Error{ErrorCode::Conflict,"Indirect CALL cell overflows"};
    return next+distance;
}
Result<std::array<std::uint8_t,6>> encodeRipCall6(std::uintptr_t site,
    std::uintptr_t cell) {
    constexpr auto maximum=std::numeric_limits<std::uintptr_t>::max();
    if(site>maximum-6)return Error{ErrorCode::Conflict,"Indirect CALL site overflows"};
    const auto next=site+6;
    std::int32_t displacement{};
    if(cell>=next) {
        const auto distance=cell-next;
        if(distance>0x7fffffffULL)return Error{ErrorCode::Conflict,"Indirect CALL cell is out of range"};
        displacement=static_cast<std::int32_t>(distance);
    } else {
        const auto distance=next-cell;
        if(distance>0x80000000ULL)return Error{ErrorCode::Conflict,"Indirect CALL cell is out of range"};
        displacement=distance==0x80000000ULL?std::numeric_limits<std::int32_t>::min():
            -static_cast<std::int32_t>(distance);
    }
    std::array<std::uint8_t,6> result{0xff,0x15,0,0,0,0};
    std::memcpy(result.data()+2,&displacement,sizeof(displacement));
    return result;
}
Result<RipCall6Plan> prepareRipCall6(std::span<const std::uint8_t> live,
    std::string_view verifiedGameHash,std::size_t imageSize,
    const RipCall6Descriptor& descriptor) {
    if(descriptor.id.empty()||descriptor.gameSha256.size()!=64||
       verifiedGameHash!=descriptor.gameSha256||imageSize!=descriptor.imageSize)
        return Error{ErrorCode::Conflict,"Indirect CALL executable identity differs"};
    if(descriptor.siteRva>imageSize||imageSize-descriptor.siteRva<6||
       descriptor.originalCellRva>=imageSize||imageSize-descriptor.originalCellRva<sizeof(void*))
        return Error{ErrorCode::Conflict,"Indirect CALL site or cell is outside image"};
    if(live.size()<6||!std::equal(descriptor.expected.begin(),descriptor.expected.end(),live.begin()))
        return Error{ErrorCode::Conflict,"Indirect CALL live bytes differ"};
    const auto cell=decodeRipCall6(descriptor.siteRva,descriptor.expected);
    if(const auto error=std::get_if<Error>(&cell))return *error;
    if(std::get<std::uintptr_t>(cell)!=descriptor.originalCellRva)
        return Error{ErrorCode::Conflict,"Indirect CALL original pointer cell differs"};
    return RipCall6Plan{descriptor.id,descriptor.siteRva,descriptor.originalCellRva,descriptor.expected};
}
Result<bool> verifySkyrim1170ClientRectAbi(std::span<const std::uint8_t> caller) {
    // RVA e446d8..e44720: LEA RDX,[RSP+20]; intervening code leaves RDX
    // untouched; RCX loads the renderer's HWND, then FF15 calls GetClientRect.
    constexpr std::array<std::uint8_t,73> expected{
        0x48,0x8d,0x54,0x24,0x20,0x49,0x03,0xc6,0x48,0x83,0x78,0x70,0x00,
        0x49,0x0f,0x44,0xc6,0x48,0x83,0xc0,0x58,0x48,0x89,0x05,0x24,0x23,
        0x44,0x02,0x48,0x8b,0x40,0x30,0x49,0x89,0x86,0x68,0x0a,0x00,0x00,
        0x48,0x8b,0x05,0x12,0x23,0x44,0x02,0x48,0x8b,0x48,0x38,0x49,0x89,
        0x8e,0x70,0x0a,0x00,0x00,0x48,0x8b,0x0d,0x00,0x23,0x44,0x02,0x48,
        0x8b,0x09,0xff,0x15,0x07,0xb2,0x90,0x00};
    if(caller.size()<expected.size()||!std::equal(expected.begin(),expected.end(),caller.begin()))
        return Error{ErrorCode::Conflict,"Renderer client-rect call ABI differs"};
    return true;
}
}
