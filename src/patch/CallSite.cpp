#include "rk/CallSite.hpp"
#include <algorithm>
#include <cstring>

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
}
