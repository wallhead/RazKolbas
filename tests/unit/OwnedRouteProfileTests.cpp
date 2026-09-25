#include <catch2/catch_test_macros.hpp>
#include "rk/OwnedRouteProfile.hpp"
#include <array>
#include <cstring>
#include <vector>
#include <d3d11.h>

TEST_CASE("Native UI publication boundary remains stable after admission",
    "[owned_route_profile]") {
    REQUIRE_FALSE(rk::shouldUseMenuPublication(false,false,true));
    REQUIRE(rk::shouldUseMenuPublication(false,true,true));
    REQUIRE(rk::shouldUseMenuPublication(true,false,true));
    REQUIRE_FALSE(rk::shouldUseMenuPublication(true,true,false));
}

TEST_CASE("Successful menu provider admission remains stable within its scene generation",
    "[owned_route_profile]") {
    REQUIRE_FALSE(rk::shouldSubmitOwnedProvider(false,0,17));
    REQUIRE(rk::shouldSubmitOwnedProvider(true,0,17));
    REQUIRE(rk::shouldSubmitOwnedProvider(false,17,17));
    REQUIRE_FALSE(rk::shouldSubmitOwnedProvider(false,17,18));
    REQUIRE_FALSE(rk::shouldSubmitOwnedProvider(false,0,0));
}

TEST_CASE("Exact live ReShade factory and swap GetBuffer sites are separate", "[owned_route_profile]") {
    const auto& factory=rk::reshade673FactoryCreateSite();
    const auto& buffer=rk::reshade673SwapGetBufferSite();
    const auto& description=rk::reshade673SwapGetDescSite();
    REQUIRE(factory.moduleSha256==buffer.moduleSha256);
    REQUIRE(factory.tableRva==0x3d79d0);
    REQUIRE(factory.slot==10);
    REQUIRE(factory.methodRva==0x13a5b0);
    REQUIRE(buffer.tableRva==0x3d7f90);
    REQUIRE(buffer.slot==9);
    REQUIRE(buffer.methodRva==0x13b460);
    REQUIRE(description.tableRva==0x3d7f90);
    REQUIRE(description.slot==12);
    REQUIRE(description.methodRva==0x13b690);
    constexpr std::uintptr_t base=0x7ffc6b260000;
    std::vector<std::uint8_t> image(factory.imageSize);
    for(const auto* site:{&factory,&buffer,&description}) {
        const auto address=base+site->methodRva;
        std::memcpy(image.data()+site->tableRva+site->slot*8,&address,8);
        std::memcpy(image.data()+site->methodRva,site->prologue.data(),16);
        REQUIRE(std::get<bool>(rk::validateOwnedRouteSite(image,base,site->moduleSha256,
            site->fileSize,site->tableRva,*site)));
    }
    auto changed=image;
    changed[factory.methodRva]^=1;
    REQUIRE(std::holds_alternative<rk::Error>(rk::validateOwnedRouteSite(changed,base,
        factory.moduleSha256,factory.fileSize,factory.tableRva,factory)));
    REQUIRE(std::holds_alternative<rk::Error>(rk::validateOwnedRouteSite(image,base,
        std::string(64,'0'),factory.fileSize,factory.tableRva,factory)));
    REQUIRE(std::holds_alternative<rk::Error>(rk::validateOwnedRouteSite(image,base,
        factory.moduleSha256,factory.fileSize,factory.tableRva+8,factory)));
}

TEST_CASE("Only exact ENB creation consumers use the early scene contract", "[owned_route_profile]") {
    constexpr std::uintptr_t base=0x180000000;
    constexpr auto hash="47ff220dd26a44520d4cec2d515d89effe87b632c1885c32388c93e8d0ceda58";
    REQUIRE(rk::isEnb20260508OwnedSceneBufferCall(base+0x5e580,base,hash));
    REQUIRE(rk::isEnb20260508OwnedSceneBufferCall(base+0x5e795,base,hash));
    REQUIRE_FALSE(rk::isEnb20260508OwnedSceneBufferCall(base+0x5e581,base,hash));
    REQUIRE_FALSE(rk::isEnb20260508OwnedSceneBufferCall(base+0x5e580,base,"other"));
    REQUIRE(rk::isEnb20260508ReducedDescriptionCall(base+0x5e53e,base,hash));
    REQUIRE(rk::isEnb20260508ReducedDescriptionCall(base+0x4872d,base,hash));
    REQUIRE_FALSE(rk::isEnb20260508ReducedDescriptionCall(base+0x4872e,base,hash));
}

TEST_CASE("ENB context methods retain their current downstream ownership", "[owned_route_profile]") {
    const std::array sites{&rk::enbContextPsResourcesSite(),
        &rk::enbContextOmSite(),&rk::enbContextViewportSite(),
        &rk::enbContextScissorSite()};
    REQUIRE(sites[0]->slot==8);
    REQUIRE(sites[1]->slot==33);
    REQUIRE(sites[2]->slot==44);
    REQUIRE(sites[3]->slot==45);
    REQUIRE(sites[3]->methodRva==0x5d100);
    std::vector<std::uint8_t> image(sites[0]->imageSize);
    constexpr std::uintptr_t base=0x180000000;
    for(const auto* site:sites) {
        const auto address=base+site->methodRva;
        std::memcpy(image.data()+site->tableRva+site->slot*8,&address,8);
        std::memcpy(image.data()+site->methodRva,site->prologue.data(),16);
    }
    for(const auto* site:sites)REQUIRE(std::get<bool>(rk::validateOwnedRouteSite(image,
        base,site->moduleSha256,site->fileSize,site->tableRva,*site)));
    // A later mod's single slot replacement must veto the whole prepared set.
    image[sites[1]->tableRva+sites[1]->slot*8]^=1;
    REQUIRE(std::holds_alternative<rk::Error>(rk::validateOwnedRouteSite(image,
        base,sites[1]->moduleSha256,sites[1]->fileSize,sites[1]->tableRva,*sites[1])));
}

TEST_CASE("All six exact ENB sampler stages share the mip-bias contract",
    "[owned_route_profile]") {
    const auto sites=rk::enbContextSamplerSites();
    REQUIRE(sites.size()==6);
    constexpr std::array expectedSlots{10u,26u,32u,61u,65u,70u};
    constexpr std::array expectedRvas{0x5c800u,0x5cd30u,0x5cf20u,
        0x5d3a0u,0x5d690u,0x5d9b0u};
    std::vector<std::uint8_t> image(sites.front().imageSize);
    constexpr std::uintptr_t base=0x180000000;
    for(std::size_t i=0;i<sites.size();++i) {
        REQUIRE(sites[i].slot==expectedSlots[i]);
        REQUIRE(sites[i].methodRva==expectedRvas[i]);
        const auto address=base+sites[i].methodRva;
        std::memcpy(image.data()+sites[i].tableRva+sites[i].slot*8,&address,8);
        std::memcpy(image.data()+sites[i].methodRva,sites[i].prologue.data(),16);
    }
    for(const auto& site:sites)REQUIRE(std::get<bool>(rk::validateOwnedRouteSite(image,
        base,site.moduleSha256,site.fileSize,site.tableRva,site)));
    image[sites.back().methodRva]^=1;
    REQUIRE(std::holds_alternative<rk::Error>(rk::validateOwnedRouteSite(image,
        base,sites.back().moduleSha256,sites.back().fileSize,
        sites.back().tableRva,sites.back())));
}

TEST_CASE("Only the observed Skyrim view-cache call owns the reduced buffer", "[owned_route_profile]") {
    constexpr std::uintptr_t base=0x7ff6f59a0000;
    constexpr auto hash="c434208894f07f604b852f29b8edc3a58c4de63de783373733e72b2b73f33be9";
    auto* selected=reinterpret_cast<IDXGISwapChain*>(0x12340000);
    auto* other=reinterpret_cast<IDXGISwapChain*>(0x12350000);
    const auto matches=[&](std::uintptr_t caller,IDXGISwapChain* swap,
        UINT index,REFIID iid,std::string_view gameHash={}) {
        return rk::isSkyrim1170OwnedSceneBufferCall(caller,base,
            gameHash.empty()?std::string_view(hash):gameHash,
            swap,selected,index,iid);
    };
    REQUIRE(matches(base+0xe4cc87,selected,0,__uuidof(ID3D11Texture2D)));
    REQUIRE_FALSE(matches(base+0xe4cc86,selected,0,__uuidof(ID3D11Texture2D)));
    REQUIRE_FALSE(matches(base+0xe48f0d,selected,0,__uuidof(ID3D11Texture2D)));
    REQUIRE_FALSE(matches(base+0xe4cc87,other,0,__uuidof(ID3D11Texture2D)));
    REQUIRE_FALSE(matches(base+0xe4cc87,selected,1,__uuidof(ID3D11Texture2D)));
    REQUIRE_FALSE(matches(base+0xe4cc87,selected,0,__uuidof(ID3D11Resource)));
    REQUIRE_FALSE(matches(base+0xe4cc87,selected,0,__uuidof(ID3D11Texture2D),
        std::string(64,'0')));
}
