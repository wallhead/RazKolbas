#include "rk/SwapObserver.hpp"
#include <algorithm>
#include <cstring>
#include <limits>
#include <optional>
namespace rk {
namespace {
std::string_view trimId(std::string_view id) {
    const auto first=id.find_first_not_of(" \t");
    return first==id.npos?std::string_view{}:id.substr(first,id.find_last_not_of(" \t")-first+1);
}
void notify(SwapObserver observer,const SwapEvent& event) noexcept {
    const auto error=GetLastError();
    try { if(observer)observer(event); } catch (...) {}
    SetLastError(error);
}
template<class Call> auto invoke(SwapEvent event,SwapObserver observer,Call call) noexcept {
    event.before=true;notify(observer,event);
    const auto result=call();
    event.before=false;
    if constexpr(std::is_same_v<decltype(result),const ULONG>)event.references=result;
    else event.result=result;
    notify(observer,event);return result;
}
template<class T> std::optional<T> read(std::span<const std::uint8_t> image,std::size_t offset) {
    if(offset>image.size()||sizeof(T)>image.size()-offset)return std::nullopt;
    T value{};std::memcpy(&value,image.data()+offset,sizeof(value));return value;
}
}
bool patchDisabled(std::string_view ids,std::string_view id) {
    while(!ids.empty()) {
        const auto comma=ids.find(',');
        if(trimId(ids.substr(0,comma))==id)return true;
        if(comma==ids.npos)break;
        ids.remove_prefix(comma+1);
    }
    return false;
}
bool validDisabledPatchIds(std::string_view ids) {
    if(ids.empty())return true;
    unsigned seen=0;
    while(true) {
        const auto comma=ids.find(',');const auto id=trimId(ids.substr(0,comma));
        const unsigned bit=id==rendererObserverPatchId?1U:id==swapObserverPatchId?2U:0U;
        if(!bit||(seen&bit))return false;
        seen|=bit;
        if(comma==ids.npos)return true;
        ids.remove_prefix(comma+1);
    }
}
HRESULT observePresent(PresentFn original,IDXGISwapChain* object,UINT interval,UINT flags,SwapObserver observer) noexcept {
    SwapEvent event{SwapCall::Present};event.object=reinterpret_cast<std::uintptr_t>(object);event.interval=interval;event.flags=flags;
    return invoke(event,observer,[&]{return original(object,interval,flags);});
}
HRESULT observePresent1(Present1Fn original,IDXGISwapChain1* object,UINT interval,UINT flags,const DXGI_PRESENT_PARAMETERS* params,SwapObserver observer) noexcept {
    SwapEvent event{SwapCall::Present1};event.object=reinterpret_cast<std::uintptr_t>(object);event.interval=interval;event.flags=flags;
    return invoke(event,observer,[&]{return original(object,interval,flags,params);});
}
HRESULT observeResize(ResizeFn original,IDXGISwapChain* object,UINT buffers,UINT width,UINT height,DXGI_FORMAT format,UINT flags,SwapObserver observer) noexcept {
    SwapEvent event{SwapCall::Resize};event.object=reinterpret_cast<std::uintptr_t>(object);event.buffers=buffers;event.width=width;event.height=height;event.format=format;event.flags=flags;
    return invoke(event,observer,[&]{return original(object,buffers,width,height,format,flags);});
}
HRESULT observeResize1(Resize1Fn original,IDXGISwapChain3* object,UINT buffers,UINT width,UINT height,DXGI_FORMAT format,UINT flags,const UINT* nodes,IUnknown* const* queues,SwapObserver observer) noexcept {
    SwapEvent event{SwapCall::Resize1};event.object=reinterpret_cast<std::uintptr_t>(object);event.buffers=buffers;event.width=width;event.height=height;event.format=format;event.flags=flags;
    return invoke(event,observer,[&]{return original(object,buffers,width,height,format,flags,nodes,queues);});
}
ULONG observeRelease(ReleaseFn original,IUnknown* object,SwapObserver observer) noexcept {
    SwapEvent event{SwapCall::Release};event.object=reinterpret_cast<std::uintptr_t>(object);
    return invoke(event,observer,[&]{return original(object);});
}
const SwapTableProfile& reshade673SwapProfile() {
    static constexpr SwapTableProfile profile{
        "059168b9d8aaa694a02a64342409fa26dfdf335035f2c0184cc61581deffc3bc",5157144,0x51c000,0x3d7f90,{{
        {2,0x13b230,{0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57,0x48,0x83,0xec,0x30,0x48}},
        {8,0x13b360,{0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x6c,0x24,0x18,0x48,0x89,0x74,0x24,0x20,0x57}},
        {13,0x13b7a0,{0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x6c,0x24,0x18,0x48,0x89,0x74,0x24,0x20,0x57}},
        {22,0x13bcc0,{0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x6c,0x24,0x18,0x56,0x57,0x41,0x54,0x41,0x56}},
        {39,0x13c060,{0x44,0x89,0x4c,0x24,0x20,0x44,0x89,0x44,0x24,0x18,0x55,0x53,0x56,0x57,0x41,0x54}}
    }}};return profile;
}
Result<bool> validateSwapTable(std::span<const std::uint8_t> image,std::uintptr_t base,
    std::string_view hash,std::size_t fileSize,std::uint32_t tableRva,const SwapTableProfile& profile) {
    const auto reject=[](const char* message)->Result<bool>{return Error{ErrorCode::Unsupported,message};};
    if(hash!=profile.hash||fileSize!=profile.fileSize)return reject("Unknown swap-chain owner identity");
    if(image.size()!=profile.imageSize||tableRva!=profile.tableRva||tableRva%8||base>std::numeric_limits<std::uintptr_t>::max()-image.size())return reject("Invalid swap-chain image/table extent");
    const auto dos=read<IMAGE_DOS_HEADER>(image,0);
    if(!dos||dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<0)return reject("Invalid swap owner DOS header");
    const auto nt=read<IMAGE_NT_HEADERS64>(image,static_cast<std::size_t>(dos->e_lfanew));
    if(!nt||nt->Signature!=IMAGE_NT_SIGNATURE||nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64||
       nt->FileHeader.SizeOfOptionalHeader!=sizeof(IMAGE_OPTIONAL_HEADER64)||nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC||
       nt->OptionalHeader.SizeOfImage!=image.size()||!nt->FileHeader.NumberOfSections||nt->FileHeader.NumberOfSections>96)return reject("Invalid swap owner PE header");
    const auto inSection=[&](std::size_t rva,std::size_t size,bool code){
        if(rva>image.size()||size>image.size()-rva)return false;
        unsigned matches=0;
        for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
            const auto s=read<IMAGE_SECTION_HEADER>(image,static_cast<std::size_t>(dos->e_lfanew)+sizeof(*nt)+i*sizeof(IMAGE_SECTION_HEADER));
            if(!s||s->VirtualAddress>image.size()||s->Misc.VirtualSize>image.size()-s->VirtualAddress)return false;
            if(rva>=s->VirtualAddress&&rva-s->VirtualAddress<=s->Misc.VirtualSize&&size<=s->Misc.VirtualSize-(rva-s->VirtualAddress)) {
                if(!(s->Characteristics&IMAGE_SCN_MEM_READ)||static_cast<bool>(s->Characteristics&IMAGE_SCN_MEM_EXECUTE)!=code)return false;
                ++matches;
            }
        }
        return matches==1;
    };
    for(const auto& method:profile.methods) {
        const auto slot=static_cast<std::size_t>(tableRva)+static_cast<std::size_t>(method.slot)*8;
        if(!inSection(slot,8,false)||!inSection(method.rva,method.prologue.size(),true))return reject("Swap method/slot outside expected section");
        const auto pointer=read<std::uintptr_t>(image,slot);
        if(!pointer||*pointer!=base+method.rva)return reject("Swap method pointer owner changed");
        if(!std::equal(method.prologue.begin(),method.prologue.end(),image.begin()+method.rva))return reject("Swap method prologue changed");
    }
    return true;
}
}
