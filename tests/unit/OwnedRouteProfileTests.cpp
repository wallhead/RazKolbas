#include <catch2/catch_test_macros.hpp>
#include "rk/OwnedRouteProfile.hpp"
#include <array>
#include <cstring>
#include <vector>

TEST_CASE("Exact live ReShade factory and swap GetBuffer sites are separate", "[owned_route_profile]") {
    const auto& factory=rk::reshade673FactoryCreateSite();
    const auto& buffer=rk::reshade673SwapGetBufferSite();
    REQUIRE(factory.moduleSha256==buffer.moduleSha256);
    REQUIRE(factory.tableRva==0x3d79d0);
    REQUIRE(factory.slot==10);
    REQUIRE(factory.methodRva==0x13a5b0);
    REQUIRE(buffer.tableRva==0x3d7f90);
    REQUIRE(buffer.slot==9);
    REQUIRE(buffer.methodRva==0x13b460);
    constexpr std::uintptr_t base=0x7ffc6b260000;
    std::vector<std::uint8_t> image(factory.imageSize);
    for(const auto* site:{&factory,&buffer}) {
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

TEST_CASE("ENB context methods retain their current downstream ownership", "[owned_route_profile]") {
    const std::array sites{&rk::enbContextPsResourcesSite(),
        &rk::enbContextOmSite(),&rk::enbContextViewportSite()};
    REQUIRE(sites[0]->slot==8);
    REQUIRE(sites[1]->slot==33);
    REQUIRE(sites[2]->slot==44);
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
