#include <catch2/catch_test_macros.hpp>
#include "rk/NrRuntimeSelection.hpp"
#include <array>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <Windows.h>

namespace {
std::filesystem::path uniqueNrTestDirectory() {
    wchar_t parent[MAX_PATH]{},created[MAX_PATH]{};
    if(!GetTempPathW(MAX_PATH,parent)||!GetTempFileNameW(parent,L"rkn",0,created))
        throw std::runtime_error("Cannot create NR test path");
    if(!DeleteFileW(created))throw std::runtime_error("Cannot prepare NR test directory");
    std::filesystem::create_directory(created);
    return created;
}
}

TEST_CASE("NR hardware classification uses reviewed NVIDIA PCI IDs", "[nr_runtime_selection]") {
    using rk::NrGpuFamily;
    REQUIRE(rk::classifyNrGpu({0x10de,0x1e89,false})==NrGpuFamily::Rtx20);
    REQUIRE(rk::classifyNrGpu({0x10de,0x2487,false})==NrGpuFamily::Rtx30);
    REQUIRE(rk::classifyNrGpu({0x10de,0x2702,false})==NrGpuFamily::Rtx40);
    REQUIRE(rk::classifyNrGpu({0x10de,0x2b85,false})==NrGpuFamily::Rtx50);
    REQUIRE(rk::classifyNrGpu({0x1002,0x2702,false})==NrGpuFamily::Unsupported);
    REQUIRE(rk::classifyNrGpu({0x8086,0x2702,false})==NrGpuFamily::Unsupported);
    REQUIRE(rk::classifyNrGpu({0x10de,0x2702,true})==NrGpuFamily::Unsupported);
    REQUIRE(rk::classifyNrGpu({0x10de,0x2230,false})==NrGpuFamily::Unsupported);
    REQUIRE(rk::classifyNrGpu({0x10de,0x2b85,true})==NrGpuFamily::Unsupported);
}
TEST_CASE("NR interop rejects another NVIDIA adapter despite matching family",
    "[nr_runtime_selection]") {
    REQUIRE(rk::sameNrLuid({0x1234,7},{0x1234,7}));
    REQUIRE_FALSE(rk::sameNrLuid({0x1234,7},{0x1235,7}));
    REQUIRE_FALSE(rk::sameNrLuid({0x1234,7},{0x1234,8}));
}
TEST_CASE("NR artifact access rejects traversal and verifies exact bytes before load",
    "[nr_runtime_selection]") {
    namespace fs=std::filesystem;
    const auto root=uniqueNrTestDirectory();
    fs::create_directories(root/"NR"/"probe");
    const auto file=root/"NR"/"probe"/"nvngx_dlssnr.dll";
    {std::ofstream out(file,std::ios::binary);out<<"abc";}
    const rk::NrRuntimeProfile good{"probe","NR/probe/nvngx_dlssnr.dll",
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
        3,rk::NrFamilyMask::Rtx40,rk::NrValidation::Experimental,1,false,0};
    const auto resolved=rk::nrRuntimePath(root,good);
    REQUIRE(std::holds_alternative<fs::path>(resolved));
    REQUIRE(std::holds_alternative<bool>(rk::verifyNrRuntimeFile(
        std::get<fs::path>(resolved),good)));
    auto bad=good;bad.relativePath="../escape/nvngx_dlssnr.dll";
    REQUIRE(std::holds_alternative<rk::Error>(rk::nrRuntimePath(root,bad)));
    bad=good;bad.size=4;
    REQUIRE(std::holds_alternative<rk::Error>(rk::verifyNrRuntimeFile(file,bad)));
    bad=good;bad.sha256="0000000000000000000000000000000000000000000000000000000000000000";
    REQUIRE(std::holds_alternative<rk::Error>(rk::verifyNrRuntimeFile(file,bad)));
    fs::remove(file);
    fs::remove(root/"NR"/"probe");
    fs::remove(root/"NR");
    fs::remove(root);
}
TEST_CASE("NR loader hashes its locked file and compares loaded identity",
    "[nr_runtime_selection]") {
    namespace fs=std::filesystem;
    const auto root=uniqueNrTestDirectory();
    const auto file=root/"nvngx_dlssnr.dll";
    const auto other=root/"other.dll";
    {std::ofstream out(file,std::ios::binary);out<<"abc";}
    {std::ofstream out(other,std::ios::binary);out<<"xyz";}
    const rk::NrRuntimeProfile profile{"probe","nvngx_dlssnr.dll",
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
        3,rk::NrFamilyMask::Rtx40,rk::NrValidation::Experimental,1,false,0};
    const auto handle=CreateFileW(file.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,
        OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    REQUIRE(handle!=INVALID_HANDLE_VALUE);
    REQUIRE(std::holds_alternative<bool>(rk::verifyNrRuntimeHandle(handle,profile)));
    REQUIRE(rk::sameNrFile(handle,file));
    REQUIRE_FALSE(rk::sameNrFile(handle,other));
    CloseHandle(handle);
    fs::remove(file);fs::remove(other);fs::remove(root);
}

TEST_CASE("Auto NR selects only a validated exact artifact for the renderer GPU",
    "[nr_runtime_selection]") {
    const auto catalog=rk::nrRuntimeCatalog();
    const std::array artifacts{rk::NrRuntimeArtifact{"legacy-fastfp16",true,true},
        rk::NrRuntimeArtifact{"ada-fastfp16",true,true}};
    const auto selected=rk::selectNrRuntime(catalog,artifacts,
        {0x10de,0x2702,false},"Auto",false);
    REQUIRE(selected.profile);
    REQUIRE(selected.profile->id=="legacy-fastfp16");
    REQUIRE_FALSE(rk::selectNrRuntime(catalog,artifacts,
        {0x1002,0x2702,false},"Auto",false).profile);
    REQUIRE_FALSE(rk::selectNrRuntime(catalog,artifacts,
        {0x8086,0x2702,false},"Auto",false).profile);
    REQUIRE_FALSE(rk::selectNrRuntime(catalog,artifacts,
        {0x10de,0x2487,false},"Auto",false).profile);
}

TEST_CASE("NR candidate profiles require explicit experimental enrollment and exact bytes",
    "[nr_runtime_selection]") {
    const auto catalog=rk::nrRuntimeCatalog();
    const std::array good{rk::NrRuntimeArtifact{"plain-fp16-20-30",true,true}};
    REQUIRE_FALSE(rk::selectNrRuntime(catalog,good,
        {0x10de,0x2487,false},"Auto",true).profile);
    REQUIRE_FALSE(rk::selectNrRuntime(catalog,good,
        {0x10de,0x2487,false},"plain-fp16-20-30",false).profile);
    REQUIRE(rk::selectNrRuntime(catalog,good,
        {0x10de,0x2487,false},"plain-fp16-20-30",true).profile);
    REQUIRE_FALSE(rk::selectNrRuntime(catalog,good,
        {0x10de,0x2702,false},"plain-fp16-20-30",true).profile);
    const std::array wrong{rk::NrRuntimeArtifact{"plain-fp16-20-30",true,false}};
    REQUIRE_FALSE(rk::selectNrRuntime(catalog,wrong,
        {0x10de,0x2487,false},"plain-fp16-20-30",true).profile);
}
TEST_CASE("NR caller identity workaround is profile-specific",
    "[nr_runtime_selection]") {
    const auto catalog=rk::nrRuntimeCatalog();
    const auto byId=[catalog](std::string_view id) {
        for(const auto& profile:catalog)if(profile.id==id)return &profile;
        return static_cast<const rk::NrRuntimeProfile*>(nullptr);
    };
    REQUIRE(byId("legacy-fastfp16"));
    REQUIRE(byId("legacy-fastfp16")->callerIdentityShim);
    REQUIRE(byId("plain-fp16-20-30")->callerIdentityShim);
    REQUIRE(byId("ada-fastfp16")->callerIdentityShim);
    REQUIRE_FALSE(byId("nvidia-50")->callerIdentityShim);
}

TEST_CASE("NR selector orders validated tuned and universal profiles by priority",
    "[nr_runtime_selection]") {
    constexpr std::array catalog{
        rk::NrRuntimeProfile{"universal","NR/universal/nvngx_dlssnr.dll","aa",1,
            rk::NrFamilyMask::Rtx30|rk::NrFamilyMask::Rtx40,
            rk::NrValidation::Validated,10,false,0},
        rk::NrRuntimeProfile{"ampere","NR/ampere/nvngx_dlssnr.dll","bb",1,
            rk::NrFamilyMask::Rtx30,rk::NrValidation::Validated,20,false,0}};
    constexpr std::array artifacts{
        rk::NrRuntimeArtifact{"ampere",true,true},
        rk::NrRuntimeArtifact{"universal",true,true}};
    const auto ampere=rk::selectNrRuntime(catalog,artifacts,
        {0x10de,0x2487,false},"Auto",false);
    REQUIRE(ampere.profile);
    REQUIRE(ampere.profile->id=="ampere");
    const auto ada=rk::selectNrRuntime(catalog,artifacts,
        {0x10de,0x2702,false},"Auto",false);
    REQUIRE(ada.profile);
    REQUIRE(ada.profile->id=="universal");
}
TEST_CASE("NR selector rejects a runtime with an incompatible host contract",
    "[nr_runtime_selection]") {
    constexpr std::array profiles{rk::NrRuntimeProfile{"wrong-abi",
        "NR/wrong-abi/nvngx_dlssnr.dll","aa",1,rk::NrFamilyMask::Rtx40,
        rk::NrValidation::Validated,100,false,0,"future-nr-abi"}};
    constexpr std::array artifacts{rk::NrRuntimeArtifact{"wrong-abi",true,true}};
    REQUIRE_FALSE(rk::selectNrRuntime(profiles,artifacts,
        {0x10de,0x2702,false},"Auto",false).profile);
}
