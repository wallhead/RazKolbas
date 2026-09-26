#include "rk/OwnedRouteProfile.hpp"
#include <algorithm>
#include <cstring>
#include <limits>
#include <d3d11.h>

namespace rk {
namespace {
constexpr std::string_view reshade680Hash=
    "b2945c29e7095491a901746b400e58db9b1592ab092bacf2a888ce37f02d08da";
constexpr std::string_view enb505Hash=
    "35ff1543c8aaa5435a9002dc58d5459c29557ce8e5e5f91b25dfe4645be7bae3";
}
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
const OwnedRouteSite& reshade673SwapGetDescSite() noexcept {
    static constexpr OwnedRouteSite site{"reshade673.swap.get-desc-owned-scene-v1",
        "059168b9d8aaa694a02a64342409fa26dfdf335035f2c0184cc61581deffc3bc",
        5157144,0x51c000,0x3d7f90,0x13b690,12,
        {0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,
         0x48,0x83,0xec,0x70,0x44}};
    return site;
}
const OwnedRouteSite& reshade680FactoryCreateSite() noexcept {
    static constexpr OwnedRouteSite site{"reshade680.factory.create-owned-scene-v1",
        reshade680Hash,5255448,0x534000,0x3ee350,0x14a4f0,10,
        {0x40,0x55,0x53,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8d,0x6c}};
    return site;
}
const OwnedRouteSite& reshade680SwapGetBufferSite() noexcept {
    static constexpr OwnedRouteSite site{"reshade680.swap.get-buffer-owned-scene-v1",
        reshade680Hash,5255448,0x534000,0x3ee960,0x14c510,9,
        {0x48,0x8b,0x49,0x08,0x48,0x8b,0x01,0x48,0xff,0x60,0x48,0xcc,0xcc,0xcc,0xcc,0xcc}};
    return site;
}
const OwnedRouteSite& reshade680SwapGetDescSite() noexcept {
    static constexpr OwnedRouteSite site{"reshade680.swap.get-desc-owned-scene-v1",
        reshade680Hash,5255448,0x534000,0x3ee960,0x14c750,12,
        {0x40,0x53,0x55,0x56,0x57,0x48,0x83,0xec,0x78,0x80,0xb9,0x1c,0x01,0,0,0}};
    return site;
}
const OwnedRouteSite* findFactoryCreateSite(std::string_view hash) noexcept {
    if(hash==reshade673FactoryCreateSite().moduleSha256)return &reshade673FactoryCreateSite();
    if(hash==reshade680Hash)return &reshade680FactoryCreateSite();
    return nullptr;
}
const OwnedRouteSite* findSwapGetBufferSite(std::string_view hash) noexcept {
    if(hash==reshade673SwapGetBufferSite().moduleSha256)return &reshade673SwapGetBufferSite();
    if(hash==reshade680Hash)return &reshade680SwapGetBufferSite();
    return nullptr;
}
const OwnedRouteSite* findSwapGetDescSite(std::string_view hash) noexcept {
    if(hash==reshade673SwapGetDescSite().moduleSha256)return &reshade673SwapGetDescSite();
    if(hash==reshade680Hash)return &reshade680SwapGetDescSite();
    return nullptr;
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
const OwnedRouteSite& enbContextScissorSite() noexcept {
    static constexpr OwnedRouteSite site{"enb20260508.context.scissor-owned-ui-v1",
        "47ff220dd26a44520d4cec2d515d89effe87b632c1885c32388c93e8d0ceda58",
        4664320,0xaae000,0x1a49f8,0x5d100,45,
        {0x4d,0x8b,0xd0,0x83,0xfa,0x10,0x77,0x55,0x89,0x91,0xb4,0x0a,0,0,0x4d,0x85}};
    return site;
}
const OwnedRouteSite& enbContextPsResourcesSite() noexcept {
    static constexpr OwnedRouteSite site{"enb20260508.context.ps-resources-owned-ui-v1",
        "47ff220dd26a44520d4cec2d515d89effe87b632c1885c32388c93e8d0ceda58",
        4664320,0xaae000,0x1a49f8,0x5c730,8,
        {0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57}};
    return site;
}
std::span<const OwnedRouteSite> enbContextSamplerSites() noexcept {
    static constexpr std::array sites{
        OwnedRouteSite{"enb20260508.context.ps-samplers-mip-bias-v1",
            "47ff220dd26a44520d4cec2d515d89effe87b632c1885c32388c93e8d0ceda58",
            4664320,0xaae000,0x1a49f8,0x5c800,10,
            {0x48,0x89,0x5c,0x24,0x10,0x57,0x48,0x83,0xec,0x20,0x4d,0x8b,0xd9,0x41,0x8b,0xf8}},
        OwnedRouteSite{"enb20260508.context.vs-samplers-mip-bias-v1",
            "47ff220dd26a44520d4cec2d515d89effe87b632c1885c32388c93e8d0ceda58",
            4664320,0xaae000,0x1a49f8,0x5cd30,26,
            {0x48,0x89,0x5c,0x24,0x10,0x57,0x48,0x83,0xec,0x20,0x4d,0x8b,0xd9,0x41,0x8b,0xf8}},
        OwnedRouteSite{"enb20260508.context.gs-samplers-mip-bias-v1",
            "47ff220dd26a44520d4cec2d515d89effe87b632c1885c32388c93e8d0ceda58",
            4664320,0xaae000,0x1a49f8,0x5cf20,32,
            {0x48,0x89,0x5c,0x24,0x10,0x57,0x48,0x83,0xec,0x20,0x4d,0x8b,0xd9,0x41,0x8b,0xf8}},
        OwnedRouteSite{"enb20260508.context.hs-samplers-mip-bias-v1",
            "47ff220dd26a44520d4cec2d515d89effe87b632c1885c32388c93e8d0ceda58",
            4664320,0xaae000,0x1a49f8,0x5d3a0,61,
            {0x48,0x89,0x5c,0x24,0x10,0x57,0x48,0x83,0xec,0x20,0x4d,0x8b,0xd9,0x41,0x8b,0xf8}},
        OwnedRouteSite{"enb20260508.context.ds-samplers-mip-bias-v1",
            "47ff220dd26a44520d4cec2d515d89effe87b632c1885c32388c93e8d0ceda58",
            4664320,0xaae000,0x1a49f8,0x5d690,65,
            {0x48,0x89,0x5c,0x24,0x10,0x57,0x48,0x83,0xec,0x20,0x4d,0x8b,0xd9,0x41,0x8b,0xf8}},
        OwnedRouteSite{"enb20260508.context.cs-samplers-mip-bias-v1",
            "47ff220dd26a44520d4cec2d515d89effe87b632c1885c32388c93e8d0ceda58",
            4664320,0xaae000,0x1a49f8,0x5d9b0,70,
            {0x48,0x89,0x5c,0x24,0x10,0x57,0x48,0x83,0xec,0x20,0x4d,0x8b,0xd9,0x41,0x8b,0xf8}}
    };
    return sites;
}
namespace {
const OwnedRouteSite& enb505ContextOmSite() noexcept {
    static constexpr OwnedRouteSite site{"enb0505.context.om-owned-ui-v1",
        enb505Hash,4553216,0xa92000,0x18fa08,0x690d0,33,
        {0x48,0x8b,0xc4,0x4c,0x89,0x48,0x20,0x48,0x81,0xec,0x28,0x07,0,0,0x48,0x89}};
    return site;
}
const OwnedRouteSite& enb505ContextViewportSite() noexcept {
    static constexpr OwnedRouteSite site{"enb0505.context.viewport-owned-ui-v1",
        enb505Hash,4553216,0xa92000,0x18fa08,0x5d110,44,
        {0x4d,0x8b,0xd0,0x83,0xfa,0x10,0x77,0x65,0x89,0x91,0xb0,0x0a,0,0,0x4d,0x85}};
    return site;
}
const OwnedRouteSite& enb505ContextScissorSite() noexcept {
    static constexpr OwnedRouteSite site{"enb0505.context.scissor-owned-ui-v1",
        enb505Hash,4553216,0xa92000,0x18fa08,0x5d1a0,45,
        {0x4d,0x8b,0xd0,0x83,0xfa,0x10,0x77,0x55,0x89,0x91,0xb4,0x0a,0,0,0x4d,0x85}};
    return site;
}
const OwnedRouteSite& enb505ContextPsResourcesSite() noexcept {
    static constexpr OwnedRouteSite site{"enb0505.context.ps-resources-owned-ui-v1",
        enb505Hash,4553216,0xa92000,0x18fa08,0x5c7d0,8,
        {0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57}};
    return site;
}
std::span<const OwnedRouteSite> enb505ContextSamplerSites() noexcept {
    static constexpr std::array sites{
        OwnedRouteSite{"enb0505.context.ps-samplers-mip-bias-v1",enb505Hash,
            4553216,0xa92000,0x18fa08,0x5c8a0,10,
            {0x48,0x89,0x5c,0x24,0x10,0x57,0x48,0x83,0xec,0x20,0x4d,0x8b,0xd9,0x41,0x8b,0xf8}},
        OwnedRouteSite{"enb0505.context.vs-samplers-mip-bias-v1",enb505Hash,
            4553216,0xa92000,0x18fa08,0x5cdd0,26,
            {0x48,0x89,0x5c,0x24,0x10,0x57,0x48,0x83,0xec,0x20,0x4d,0x8b,0xd9,0x41,0x8b,0xf8}},
        OwnedRouteSite{"enb0505.context.gs-samplers-mip-bias-v1",enb505Hash,
            4553216,0xa92000,0x18fa08,0x5cfc0,32,
            {0x48,0x89,0x5c,0x24,0x10,0x57,0x48,0x83,0xec,0x20,0x4d,0x8b,0xd9,0x41,0x8b,0xf8}},
        OwnedRouteSite{"enb0505.context.hs-samplers-mip-bias-v1",enb505Hash,
            4553216,0xa92000,0x18fa08,0x5d440,61,
            {0x48,0x89,0x5c,0x24,0x10,0x57,0x48,0x83,0xec,0x20,0x4d,0x8b,0xd9,0x41,0x8b,0xf8}},
        OwnedRouteSite{"enb0505.context.ds-samplers-mip-bias-v1",enb505Hash,
            4553216,0xa92000,0x18fa08,0x5d730,65,
            {0x48,0x89,0x5c,0x24,0x10,0x57,0x48,0x83,0xec,0x20,0x4d,0x8b,0xd9,0x41,0x8b,0xf8}},
        OwnedRouteSite{"enb0505.context.cs-samplers-mip-bias-v1",enb505Hash,
            4553216,0xa92000,0x18fa08,0x5da50,70,
            {0x48,0x89,0x5c,0x24,0x10,0x57,0x48,0x83,0xec,0x20,0x4d,0x8b,0xd9,0x41,0x8b,0xf8}}
    };
    return sites;
}
}
EnbContextSites findEnbContextSites(std::string_view hash) noexcept {
    if(hash==enbContextOmSite().moduleSha256)
        return {&enbContextOmSite(),&enbContextViewportSite(),
            &enbContextScissorSite(),&enbContextPsResourcesSite(),enbContextSamplerSites()};
    if(hash==enb505Hash)
        return {&enb505ContextOmSite(),&enb505ContextViewportSite(),
            &enb505ContextScissorSite(),&enb505ContextPsResourcesSite(),enb505ContextSamplerSites()};
    return {};
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
bool isSkyrim1170OwnedSceneBufferCall(std::uintptr_t returnAddress,
    std::uintptr_t gameBase,std::string_view verifiedGameHash,
    IDXGISwapChain* swap,IDXGISwapChain* selectedSwap,
    UINT index,REFIID iid) noexcept {
    constexpr std::string_view gameHash=
        "c434208894f07f604b852f29b8edc3a58c4de63de783373733e72b2b73f33be9";
    constexpr std::uintptr_t returnRva=0xe4cc87;
    return gameBase&&gameBase<=std::numeric_limits<std::uintptr_t>::max()-returnRva&&
        verifiedGameHash==gameHash&&returnAddress==gameBase+returnRva&&
        swap&&swap==selectedSwap&&index==0&&
        IsEqualIID(iid,__uuidof(ID3D11Texture2D));
}
bool isEnb20260508OwnedSceneBufferCall(std::uintptr_t returnAddress,
    std::uintptr_t enbBase,std::string_view verifiedEnbHash) noexcept {
    constexpr std::string_view hash=
        "47ff220dd26a44520d4cec2d515d89effe87b632c1885c32388c93e8d0ceda58";
    if(!enbBase||verifiedEnbHash!=hash||
       enbBase>std::numeric_limits<std::uintptr_t>::max()-0x5e795)return false;
    return returnAddress==enbBase+0x5e580||returnAddress==enbBase+0x5e795;
}
bool isEnb20260508ReducedDescriptionCall(std::uintptr_t returnAddress,
    std::uintptr_t enbBase,std::string_view verifiedEnbHash) noexcept {
    constexpr std::string_view hash=
        "47ff220dd26a44520d4cec2d515d89effe87b632c1885c32388c93e8d0ceda58";
    if(!enbBase||verifiedEnbHash!=hash||
       enbBase>std::numeric_limits<std::uintptr_t>::max()-0x5e53e)return false;
    return returnAddress==enbBase+0x5e53e||returnAddress==enbBase+0x4872d;
}
bool isVerifiedEnbOwnedSceneBufferCall(std::uintptr_t returnAddress,
    std::uintptr_t enbBase,std::string_view verifiedEnbHash) noexcept {
    if(isEnb20260508OwnedSceneBufferCall(returnAddress,enbBase,verifiedEnbHash))return true;
    if(!enbBase||verifiedEnbHash!=enb505Hash||
       enbBase>std::numeric_limits<std::uintptr_t>::max()-0x5e835)return false;
    return returnAddress==enbBase+0x5e620||returnAddress==enbBase+0x5e835;
}
bool isVerifiedEnbReducedDescriptionCall(std::uintptr_t returnAddress,
    std::uintptr_t enbBase,std::string_view verifiedEnbHash) noexcept {
    if(isEnb20260508ReducedDescriptionCall(returnAddress,enbBase,verifiedEnbHash))return true;
    if(!enbBase||verifiedEnbHash!=enb505Hash||
       enbBase>std::numeric_limits<std::uintptr_t>::max()-0x5e5de)return false;
    return returnAddress==enbBase+0x5e5de||returnAddress==enbBase+0x4872d;
}
}
