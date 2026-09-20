#include "rk/OwnedRouteProfile.hpp"
#include <algorithm>
#include <cstring>
#include <limits>

namespace rk {
const OwnedRouteSite& reshade673FactoryCreateSite() noexcept {
    static constexpr OwnedRouteSite site{"reshade673.factory.create-owned-scene-v1",
        "059168b9d8aaa694a02a64342409fa26dfdf335035f2c0184cc61581deffc3bc",
        5157144,0x51c000,0x3d79d0,0x13a5b0,10,
        {0x48,0x83,0xec,0x38,0x48,0x8d,0x05,0x15,0,0,0,0x48,0x89,0x44,0x24,0x20}};
    return site;
}
const OwnedRouteSite& reshade673SwapGetBufferSite() noexcept {
    static constexpr OwnedRouteSite site{"reshade673.swap.get-buffer-owned-scene-v1",
        "059168b9d8aaa694a02a64342409fa26dfdf335035f2c0184cc61581deffc3bc",
        5157144,0x51c000,0x3d7f90,0x13b460,9,
        {0x48,0x8b,0x49,0x08,0x48,0x8b,0x01,0x48,0xff,0x60,0x48,
         0xcc,0xcc,0xcc,0xcc,0xcc}};
    return site;
}
const OwnedRouteSite& enbContextOmSite() noexcept {
    static constexpr OwnedRouteSite site{"enb20260508.context.om-owned-ui-v1",
        "47ff220dd26a44520d4cec2d515d89effe87b632c1885c32388c93e8d0ceda58",
        4664320,0xaae000,0x1a49f8,0x68f40,33,
        {0x48,0x8b,0xc4,0x4c,0x89,0x48,0x20,0x48,0x81,0xec,0x28,0x07,0,0,0x48,0x89}};
    return site;
}
const OwnedRouteSite& enbContextViewportSite() noexcept {
    static constexpr OwnedRouteSite site{"enb20260508.context.viewport-owned-ui-v1",
        "47ff220dd26a44520d4cec2d515d89effe87b632c1885c32388c93e8d0ceda58",
        4664320,0xaae000,0x1a49f8,0x5d070,44,
        {0x4d,0x8b,0xd0,0x83,0xfa,0x10,0x77,0x65,0x89,0x91,0xb0,0x0a,0,0,0x4d,0x85}};
    return site;
}
const OwnedRouteSite& enbContextPsResourcesSite() noexcept {
    static constexpr OwnedRouteSite site{"enb20260508.context.ps-resources-owned-ui-v1",
        "47ff220dd26a44520d4cec2d515d89effe87b632c1885c32388c93e8d0ceda58",
        4664320,0xaae000,0x1a49f8,0x5c730,8,
        {0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57}};
    return site;
}
Result<bool> validateOwnedRouteSite(std::span<const std::uint8_t> image,
    std::uintptr_t moduleBase,std::string_view verifiedFileHash,
    std::size_t verifiedFileSize,std::uint32_t actualTableRva,
    const OwnedRouteSite& site) {
    if(site.id.empty()||site.moduleSha256.size()!=64||
       verifiedFileHash!=site.moduleSha256||verifiedFileSize!=site.fileSize||
       image.size()!=site.imageSize||actualTableRva!=site.tableRva||
       !moduleBase||moduleBase>std::numeric_limits<std::uintptr_t>::max()-site.imageSize)
        return Error{ErrorCode::Conflict,"Owned route module or table identity differs"};
    const auto slot=static_cast<std::uint64_t>(site.tableRva)+site.slot*sizeof(void*);
    if(slot>image.size()||sizeof(void*)>image.size()-slot||
       site.methodRva>image.size()||site.prologue.size()>image.size()-site.methodRva)
        return Error{ErrorCode::Conflict,"Owned route table or method is outside image"};
    std::uintptr_t current{};
    std::memcpy(&current,image.data()+slot,sizeof(current));
    if(current!=moduleBase+site.methodRva||
       !std::equal(site.prologue.begin(),site.prologue.end(),image.begin()+site.methodRva))
        return Error{ErrorCode::Conflict,"Owned route method pointer or prologue differs"};
    return true;
}
}
