#include <catch2/catch_test_macros.hpp>
#include "rk/RendererHook.hpp"
#include "rk/SwapObserver.hpp"
#include "rk/PointerPatch.hpp"
#include "rk/PatchDescriptor.hpp"
#include <cstring>
#include <vector>
#include <stdexcept>
#include <filesystem>
#include <fstream>

namespace {
template<class T> void put(std::vector<std::uint8_t>& image, std::size_t offset, const T& value) {
    std::memcpy(image.data()+offset, &value, sizeof(value));
}
void text(std::vector<std::uint8_t>& image, std::size_t offset, const char* value) {
    std::memcpy(image.data()+offset, value, std::strlen(value)+1);
}
std::vector<std::uint8_t> fixture() {
    std::vector<std::uint8_t> image(0x4000);
    IMAGE_DOS_HEADER dos{}; dos.e_magic = IMAGE_DOS_SIGNATURE; dos.e_lfanew = 0x80; put(image,0,dos);
    IMAGE_NT_HEADERS64 nt{}; nt.Signature=IMAGE_NT_SIGNATURE;
    nt.FileHeader.Machine=IMAGE_FILE_MACHINE_AMD64; nt.FileHeader.NumberOfSections=1;
    nt.FileHeader.SizeOfOptionalHeader=sizeof(IMAGE_OPTIONAL_HEADER64);
    nt.OptionalHeader.Magic=IMAGE_NT_OPTIONAL_HDR64_MAGIC; nt.OptionalHeader.SizeOfImage=0x4000;
    nt.OptionalHeader.NumberOfRvaAndSizes=IMAGE_NUMBEROF_DIRECTORY_ENTRIES;
    nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT]={0x1100,60}; put(image,0x80,nt);
    IMAGE_SECTION_HEADER section{}; std::memcpy(section.Name,".rdata",6);
    section.VirtualAddress=0x1000; section.Misc.VirtualSize=0x2000;
    section.Characteristics=IMAGE_SCN_MEM_READ; put(image,0x80+sizeof(nt),section);
    IMAGE_IMPORT_DESCRIPTOR import{}; import.Name=0x1180; import.OriginalFirstThunk=0x1200;
    import.FirstThunk=0x1300; put(image,0x1100,import);
    text(image,0x1180,"d3d11.dll"); put(image,0x1200,std::uint64_t{0x1400});
    text(image,0x1402,"D3D11CreateDeviceAndSwapChain");
    put(image,0x1300,std::uintptr_t{0x12345678});
    return image;
}
constexpr rk::CreationImportProfile profile{"fixture-hash", 1234, 0x4000, 0x1300};
int calls{}, observations{};
rk::DeviceCreationArgs received{};
HRESULT WINAPI original(IDXGIAdapter* adapter,D3D_DRIVER_TYPE driver,HMODULE software,UINT flags,
    const D3D_FEATURE_LEVEL* levels,UINT count,UINT sdk,const DXGI_SWAP_CHAIN_DESC* desc,
    IDXGISwapChain** swap,ID3D11Device** device,D3D_FEATURE_LEVEL* feature,ID3D11DeviceContext** context) {
    ++calls; received={adapter,driver,software,flags,levels,count,sdk,desc,swap,device,feature,context};
    if (feature) *feature=D3D_FEATURE_LEVEL_11_1;
    SetLastError(123);
    return S_FALSE; // Preserve nonzero successful HRESULT too.
}
void throwingObserver(const rk::DeviceCreationArgs&, HRESULT result) {
    ++observations;
    REQUIRE(result==S_FALSE);
    SetLastError(456);
    throw std::runtime_error("observer must not break renderer");
}
}
TEST_CASE("Exact named renderer import supports an owned reversible pointer lease", "[hook_profiles]") {
    auto image=fixture();
    const auto result=rk::validateCreationImport(image,"fixture-hash",1234,profile);
    REQUIRE(std::holds_alternative<std::uint32_t>(result));
    auto slot=reinterpret_cast<void**>(image.data()+std::get<std::uint32_t>(result));
    rk::PointerPatch patch;
    REQUIRE(std::holds_alternative<bool>(patch.apply(slot,reinterpret_cast<void*>(0x12345678),reinterpret_cast<void*>(0x23456789))));
    REQUIRE(*slot==reinterpret_cast<void*>(0x23456789));
    REQUIRE(std::holds_alternative<bool>(patch.restore()));
    REQUIRE(*slot==reinterpret_cast<void*>(0x12345678));
}
TEST_CASE("Unknown identity and malformed renderer imports reject without writes", "[hook_profiles]") {
    auto image=fixture(); auto hash=std::string_view("fixture-hash"); auto size=std::size_t{1234};
    SECTION("wrong game hash") { hash="wrong"; }
    SECTION("wrong file size") { ++size; }
    SECTION("truncated image") { image.resize(0x2000); }
    SECTION("wrong import spelling") { image[0x1402]='X'; }
    SECTION("outside image name") { put(image,0x1200,std::uint64_t{0x8000}); }
    SECTION("duplicate named import") { put(image,0x1208,std::uint64_t{0x1400}); }
    SECTION("wrong IAT location") { put(image,0x1100+offsetof(IMAGE_IMPORT_DESCRIPTOR,FirstThunk),std::uint32_t{0x1310}); }
    SECTION("outside rdata") { put(image,0x80+sizeof(IMAGE_NT_HEADERS64)+offsetof(IMAGE_SECTION_HEADER,Misc),std::uint32_t{0x100}); }
    SECTION("unterminated descriptor array") { put(image,0x80+offsetof(IMAGE_NT_HEADERS64,OptionalHeader)+offsetof(IMAGE_OPTIONAL_HEADER64,DataDirectory)+IMAGE_DIRECTORY_ENTRY_IMPORT*sizeof(IMAGE_DATA_DIRECTORY)+4,std::uint32_t{20}); }
    const auto before=image;
    REQUIRE(std::holds_alternative<rk::Error>(rk::validateCreationImport(image,hash,size,profile)));
    REQUIRE(image==before);
}
TEST_CASE("D3D11 pass-through calls prior owner exactly once preserving arguments and outputs", "[hook_profiles]") {
    calls=observations=0;
    D3D_FEATURE_LEVEL level=D3D_FEATURE_LEVEL_10_0, requested=D3D_FEATURE_LEVEL_11_0;
    DXGI_SWAP_CHAIN_DESC desc{}; IDXGISwapChain* swap=nullptr; ID3D11Device* device=nullptr;
    ID3D11DeviceContext* context=nullptr;
    rk::DeviceCreationArgs args{reinterpret_cast<IDXGIAdapter*>(0x1010),D3D_DRIVER_TYPE_UNKNOWN,
        reinterpret_cast<HMODULE>(0x2020),7,&requested,1,D3D11_SDK_VERSION,&desc,&swap,&device,&level,&context};
    const auto result=rk::observeDeviceCreation(&original,args,&throwingObserver);
    REQUIRE(result==S_FALSE); REQUIRE(calls==1); REQUIRE(observations==1);
    REQUIRE(GetLastError()==123);
    REQUIRE(received.adapter==args.adapter); REQUIRE(received.driverType==args.driverType);
    REQUIRE(received.software==args.software); REQUIRE(received.flags==args.flags);
    REQUIRE(received.levels==args.levels); REQUIRE(received.levelCount==args.levelCount);
    REQUIRE(received.sdkVersion==args.sdkVersion); REQUIRE(received.swapDesc==args.swapDesc);
    REQUIRE(received.swapChain==args.swapChain); REQUIRE(received.device==args.device);
    REQUIRE(received.featureLevel==args.featureLevel); REQUIRE(received.context==args.context);
    REQUIRE(level==D3D_FEATURE_LEVEL_11_1);
}
TEST_CASE("Only exact observed D3D11 owners are accepted", "[hook_profiles]") {
    REQUIRE(rk::creationOwnerProfile("unrecognized")==nullptr);
    const auto enb=rk::creationOwnerProfile("47ff220dd26a44520d4cec2d515d89effe87b632c1885c32388c93e8d0ceda58");
    REQUIRE(enb!=nullptr); REQUIRE(enb->exportRva==0x5e410);
    const auto system=rk::creationOwnerProfile("722871e4ac32972617483197709fe0d924ced5ed894b18fd13e0813d0b25950f");
    REQUIRE(system!=nullptr); REQUIRE(system->exportRva==0x429f0);
}
TEST_CASE("Local Skyrim executable reproduces the compiled import profile without executing game code", "[.local_game_profile]") {
    wchar_t path[32768]{};
    const auto count=GetEnvironmentVariableW(L"RAZKOLBAS_GAME_TEST_FILE",path,32768);
    if (!count || count>=32768) SKIP("Set RAZKOLBAS_GAME_TEST_FILE for the opt-in local executable audit");
    std::ifstream stream(std::filesystem::path(path),std::ios::binary);
    REQUIRE(stream.good());
    std::vector<std::uint8_t> file((std::istreambuf_iterator<char>(stream)),{});
    const auto& expected=rk::skyrim1170CreationProfile();
    REQUIRE(file.size()==expected.fileSize); REQUIRE(rk::sha256(file)==expected.gameSha256);
    // Exact file identity checked before reading the known PE layout. Map only
    // bytes into owned memory; this does not load or execute SkyrimSE.exe.
    IMAGE_DOS_HEADER dos{}; std::memcpy(&dos,file.data(),sizeof(dos));
    IMAGE_NT_HEADERS64 nt{};std::memcpy(&nt,file.data()+dos.e_lfanew,sizeof(nt));
    std::vector<std::uint8_t> mapped(expected.imageSize);
    REQUIRE(nt.OptionalHeader.SizeOfHeaders<=file.size());
    REQUIRE(nt.OptionalHeader.SizeOfHeaders<=mapped.size());
    std::memcpy(mapped.data(),file.data(),nt.OptionalHeader.SizeOfHeaders);
    for (std::size_t index=0;index<nt.FileHeader.NumberOfSections;++index) {
        IMAGE_SECTION_HEADER section{};
        std::memcpy(&section,file.data()+dos.e_lfanew+sizeof(nt)+index*sizeof(section),sizeof(section));
        REQUIRE(section.PointerToRawData<=file.size());
        REQUIRE(section.SizeOfRawData<=file.size()-section.PointerToRawData);
        REQUIRE(section.VirtualAddress<=mapped.size());
        REQUIRE(section.SizeOfRawData<=mapped.size()-section.VirtualAddress);
        std::memcpy(mapped.data()+section.VirtualAddress,file.data()+section.PointerToRawData,section.SizeOfRawData);
    }
    const auto result=rk::validateCreationImport(mapped,expected.gameSha256,file.size(),expected);
    REQUIRE(std::holds_alternative<std::uint32_t>(result));
    REQUIRE(std::get<std::uint32_t>(result)==0x17502a0);
}
TEST_CASE("Local ENB or ReShade reproduces the swap-chain profile without executing vendor code", "[.local_swap_profile]") {
    wchar_t path[32768]{};
    const auto count=GetEnvironmentVariableW(L"RAZKOLBAS_SWAP_OWNER_TEST_FILE",path,32768);
    if(!count||count>=32768)SKIP("Set RAZKOLBAS_SWAP_OWNER_TEST_FILE for the opt-in local audit");
    std::ifstream stream(std::filesystem::path(path),std::ios::binary);REQUIRE(stream.good());
    std::vector<std::uint8_t> file((std::istreambuf_iterator<char>(stream)),{});
    const auto& expected=rk::sha256(file)==rk::enbSwapProfile().hash?rk::enbSwapProfile():rk::reshade673SwapProfile();
    REQUIRE(file.size()==expected.fileSize);REQUIRE(rk::sha256(file)==expected.hash);
    IMAGE_DOS_HEADER dos{};std::memcpy(&dos,file.data(),sizeof(dos));
    IMAGE_NT_HEADERS64 nt{};std::memcpy(&nt,file.data()+dos.e_lfanew,sizeof(nt));
    std::vector<std::uint8_t> mapped(expected.imageSize);
    REQUIRE(nt.OptionalHeader.SizeOfHeaders<=file.size());REQUIRE(nt.OptionalHeader.SizeOfHeaders<=mapped.size());
    std::memcpy(mapped.data(),file.data(),nt.OptionalHeader.SizeOfHeaders);
    for(std::size_t i=0;i<nt.FileHeader.NumberOfSections;++i) {
        IMAGE_SECTION_HEADER s{};std::memcpy(&s,file.data()+dos.e_lfanew+sizeof(nt)+i*sizeof(s),sizeof(s));
        REQUIRE(s.PointerToRawData<=file.size());REQUIRE(s.SizeOfRawData<=file.size()-s.PointerToRawData);
        REQUIRE(s.VirtualAddress<=mapped.size());REQUIRE(s.SizeOfRawData<=mapped.size()-s.VirtualAddress);
        std::memcpy(mapped.data()+s.VirtualAddress,file.data()+s.PointerToRawData,s.SizeOfRawData);
    }
    REQUIRE(std::get<bool>(rk::validateSwapTable(mapped,nt.OptionalHeader.ImageBase,expected.hash,file.size(),expected.tableRva,expected)));
}
