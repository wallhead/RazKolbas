#include <catch2/catch_test_macros.hpp>
#include "rk/SwapObserver.hpp"
#include <vector>
#include <stdexcept>
#include <cstring>
#include <limits>
namespace {
std::vector<rk::SwapEvent> events;
unsigned calls;
bool argsOk;
bool throwObserver;
int marker;
const DXGI_PRESENT_PARAMETERS parameters{};
UINT nodes[2]{1,2};
IUnknown* queues[2]{};
void observe(const rk::SwapEvent& event) { events.push_back(event);SetLastError(999);if(throwObserver)throw std::runtime_error("observer"); }
HRESULT WINAPI present(IDXGISwapChain* p,UINT interval,UINT flags) {
    ++calls;argsOk=p==reinterpret_cast<IDXGISwapChain*>(&marker)&&interval==2&&flags==DXGI_PRESENT_TEST&&GetLastError()==42;
    SetLastError(73);return DXGI_STATUS_OCCLUDED;
}
HRESULT WINAPI present1(IDXGISwapChain1* p,UINT interval,UINT flags,const DXGI_PRESENT_PARAMETERS* params) {
    auto result=present(reinterpret_cast<IDXGISwapChain*>(p),interval,flags);argsOk=argsOk&&params==&parameters;return result;
}
HRESULT WINAPI resize(IDXGISwapChain* p,UINT count,UINT width,UINT height,DXGI_FORMAT format,UINT flags) {
    ++calls;argsOk=p==reinterpret_cast<IDXGISwapChain*>(&marker)&&count==3&&width==0&&height==720&&format==DXGI_FORMAT_R8G8B8A8_UNORM&&flags==0x802&&GetLastError()==42;
    SetLastError(73);return DXGI_ERROR_INVALID_CALL;
}
HRESULT WINAPI resize1(IDXGISwapChain3* p,UINT count,UINT width,UINT height,DXGI_FORMAT format,UINT flags,const UINT* n,IUnknown* const* q) {
    auto result=resize(reinterpret_cast<IDXGISwapChain*>(p),count,width,height,format,flags);argsOk=argsOk&&n==nodes&&q==queues;return result;
}
ULONG WINAPI release(IUnknown* p) { ++calls;argsOk=p==reinterpret_cast<IUnknown*>(&marker)&&GetLastError()==42;SetLastError(73);return 0; }
}
TEST_CASE("Swap observers preserve full ABI status last error and exactly one original call", "[swap_observer]") {
    for(bool throws:{false,true}) for(unsigned method=0;method<5;++method) {
        events.clear();calls=0;argsOk=false;throwObserver=throws;SetLastError(42);
        HRESULT result{};
        switch(method) {
        case 0: result=static_cast<HRESULT>(rk::observeRelease(&release,reinterpret_cast<IUnknown*>(&marker),&observe));break;
        case 1: result=rk::observePresent(&present,reinterpret_cast<IDXGISwapChain*>(&marker),2,DXGI_PRESENT_TEST,&observe);break;
        case 2: result=rk::observeResize(&resize,reinterpret_cast<IDXGISwapChain*>(&marker),3,0,720,DXGI_FORMAT_R8G8B8A8_UNORM,0x802,&observe);break;
        case 3: result=rk::observePresent1(&present1,reinterpret_cast<IDXGISwapChain1*>(&marker),2,DXGI_PRESENT_TEST,&parameters,&observe);break;
        case 4: result=rk::observeResize1(&resize1,reinterpret_cast<IDXGISwapChain3*>(&marker),3,0,720,DXGI_FORMAT_R8G8B8A8_UNORM,0x802,nodes,queues,&observe);break;
        }
        const auto lastError=GetLastError();
        REQUIRE(calls==1);REQUIRE(argsOk);REQUIRE(lastError==73);
        REQUIRE(result==(method==0?0:(method==1||method==3?DXGI_STATUS_OCCLUDED:DXGI_ERROR_INVALID_CALL)));
        REQUIRE(events.size()==2);REQUIRE(events[0].before);REQUIRE_FALSE(events[1].before);
        REQUIRE(events[0].call==static_cast<rk::SwapCall>(method));
        REQUIRE(events[1].object==reinterpret_cast<std::uintptr_t>(&marker));
        if(method==0) REQUIRE(events[1].references==0);
        if(method==2||method==4) { REQUIRE(events[0].width==0);REQUIRE(events[0].height==720);REQUIRE(events[1].result==DXGI_ERROR_INVALID_CALL); }
    }
}
TEST_CASE("Both observer IDs support selective disable lists", "[swap_observer]") {
    const std::string ids=std::string(rk::rendererObserverPatchId)+", "+std::string(rk::swapObserverPatchId);
    REQUIRE(rk::validDisabledPatchIds(ids));REQUIRE(rk::patchDisabled(ids,rk::swapObserverPatchId));
    REQUIRE_FALSE(rk::patchDisabled(rk::rendererObserverPatchId,rk::swapObserverPatchId));
    REQUIRE_FALSE(rk::validDisabledPatchIds("unknown"));REQUIRE_FALSE(rk::validDisabledPatchIds(ids+","));
    REQUIRE_FALSE(rk::validDisabledPatchIds(std::string(rk::swapObserverPatchId)+","+std::string(rk::swapObserverPatchId)));
}
TEST_CASE("Swap table validation rejects unknown shifted or modified owners before writes", "[swap_observer]") {
    constexpr std::uintptr_t base=0x180000000;
    std::vector<std::uint8_t> image(0x4000);
    const auto put=[&](std::size_t offset,const auto& value) { std::memcpy(image.data()+offset,&value,sizeof(value)); };
    IMAGE_DOS_HEADER dos{};dos.e_magic=IMAGE_DOS_SIGNATURE;dos.e_lfanew=0x80;put(0,dos);
    IMAGE_NT_HEADERS64 nt{};nt.Signature=IMAGE_NT_SIGNATURE;nt.FileHeader.Machine=IMAGE_FILE_MACHINE_AMD64;
    nt.FileHeader.NumberOfSections=2;nt.FileHeader.SizeOfOptionalHeader=sizeof(IMAGE_OPTIONAL_HEADER64);
    nt.OptionalHeader.Magic=IMAGE_NT_OPTIONAL_HDR64_MAGIC;nt.OptionalHeader.SizeOfImage=0x4000;put(0x80,nt);
    IMAGE_SECTION_HEADER text{};text.VirtualAddress=0x1000;text.Misc.VirtualSize=0x1000;text.Characteristics=IMAGE_SCN_MEM_EXECUTE|IMAGE_SCN_MEM_READ;put(0x80+sizeof(nt),text);
    IMAGE_SECTION_HEADER data{};data.VirtualAddress=0x2000;data.Misc.VirtualSize=0x1000;data.Characteristics=IMAGE_SCN_MEM_READ;put(0x80+sizeof(nt)+sizeof(text),data);
    rk::SwapTableProfile profile{"fixture",123,0x4000,0x2100,{}};
    unsigned i=0;
    for(auto& method:profile.methods) { method.slot=i;method.rva=0x1100+32*i++;method.prologue.fill(0x90);put(method.rva,method.prologue);const auto ptr=base+method.rva;put(profile.tableRva+method.slot*8,ptr); }
    const auto valid=image;
    REQUIRE(std::get<bool>(rk::validateSwapTable(image,base,"fixture",123,0x2100,profile)));
    REQUIRE(std::holds_alternative<rk::Error>(rk::validateSwapTable(image,base,"wrong",123,0x2100,profile)));
    REQUIRE(std::holds_alternative<rk::Error>(rk::validateSwapTable(image,base,"fixture",124,0x2100,profile)));
    REQUIRE(std::holds_alternative<rk::Error>(rk::validateSwapTable(image,base,"fixture",123,0x2108,profile)));
    REQUIRE(std::holds_alternative<rk::Error>(rk::validateSwapTable(image,std::numeric_limits<std::uintptr_t>::max(),"fixture",123,0x2100,profile)));
    for(unsigned mode=0;mode<5;++mode) {
        image=valid;
        if(mode==0)image[0x1100]=0xcc;
        if(mode==1)image[0x2100]^=8;
        if(mode==2)image.resize(0x3000);
        if(mode==3) { data.Characteristics|=IMAGE_SCN_MEM_EXECUTE;put(0x80+sizeof(nt)+sizeof(text),data); }
        if(mode==4)image[0]=0;
        const auto before=image;
        REQUIRE(std::holds_alternative<rk::Error>(rk::validateSwapTable(image,base,"fixture",123,0x2100,profile)));
        REQUIRE(image==before);
    }
}
