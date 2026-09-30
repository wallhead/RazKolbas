#include "rk/FactoryCreateTrace.hpp"
#include "rk/OwnedRouteProfile.hpp"
#include "rk/PatchDescriptor.hpp"
#include <Windows.h>
#include <limits>
#include <cstring>
#include <algorithm>
#include <filesystem>

namespace rk {
namespace {
bool readCodeBytes(std::uintptr_t address,void* output,SIZE_T size) noexcept {
    if(!address||address>std::numeric_limits<std::uintptr_t>::max()-size)
        return false;
    MEMORY_BASIC_INFORMATION region{};
    if(!VirtualQuery(reinterpret_cast<const void*>(address),&region,sizeof(region))||
       region.State!=MEM_COMMIT||(region.Protect&PAGE_GUARD)||
       (region.Protect&0xff)==PAGE_NOACCESS)return false;
    const auto regionBase=reinterpret_cast<std::uintptr_t>(region.BaseAddress);
    if(address<regionBase||address-regionBase>=region.RegionSize||
       size>region.RegionSize-(address-regionBase))return false;
    SIZE_T copied{};
    return ReadProcessMemory(GetCurrentProcess(),
        reinterpret_cast<const void*>(address),output,size,&copied)&&copied==size;
}
std::uintptr_t relativeTarget(std::uintptr_t address,unsigned length,
    std::int32_t displacement) noexcept {
    if(address>std::numeric_limits<std::uintptr_t>::max()-length)return 0;
    const auto end=address+length;
    if(displacement<0) {
        const auto distance=static_cast<std::uint64_t>(
            -static_cast<std::int64_t>(displacement));
        return distance<=end?end-static_cast<std::uintptr_t>(distance):0;
    }
    const auto distance=static_cast<std::uintptr_t>(displacement);
    return distance<=std::numeric_limits<std::uintptr_t>::max()-end?
        end+distance:0;
}
bool executable(std::uint32_t protection) noexcept {
    if(protection&PAGE_GUARD)return false;
    switch(protection&0xff) {
    case PAGE_EXECUTE:case PAGE_EXECUTE_READ:case PAGE_EXECUTE_READWRITE:
    case PAGE_EXECUTE_WRITECOPY:return true;
    default:return false;
    }
}
}
FactoryMethodCodeFacts inspectFactoryMethodCode(std::uintptr_t method) noexcept {
    FactoryMethodCodeFacts facts{};
    MEMORY_BASIC_INFORMATION region{};
    if(VirtualQuery(reinterpret_cast<const void*>(method),&region,sizeof(region))) {
        facts.allocationBase=reinterpret_cast<std::uintptr_t>(region.AllocationBase);
        facts.protection=region.Protect;facts.state=region.State;facts.type=region.Type;
    }
    facts.readable=readCodeBytes(method,facts.bytes.data(),facts.bytes.size());
    if(!facts.readable)return facts;
    std::int32_t displacement{};
    if(facts.bytes[0]==0xe9) {
        std::memcpy(&displacement,facts.bytes.data()+1,sizeof(displacement));
        facts.jumpTarget=relativeTarget(method,5,displacement);
    } else if(facts.bytes[0]==0xff&&facts.bytes[1]==0x25) {
        std::memcpy(&displacement,facts.bytes.data()+2,sizeof(displacement));
        facts.indirectSlot=relativeTarget(method,6,displacement);
        std::uintptr_t target{};
        if(readCodeBytes(facts.indirectSlot,&target,sizeof(target)))
            facts.jumpTarget=target;
    } else if(facts.bytes[0]==0x48&&facts.bytes[1]==0xb8&&
              facts.bytes[10]==0xff&&facts.bytes[11]==0xe0) {
        std::memcpy(&facts.jumpTarget,facts.bytes.data()+2,sizeof(facts.jumpTarget));
    }
    if(facts.jumpTarget)
        facts.targetReadable=readCodeBytes(facts.jumpTarget,facts.targetBytes.data(),
            facts.targetBytes.size());
    return facts;
}
const SteamFactoryInlineProfile& steamFactoryInlineProfile() noexcept {
    static constexpr auto code=[] {
        constexpr std::string_view hex="40535556574883ec28488bf9498bf1488d0d3acb0900498bd8488beae8ef700100488d0d58370e00e82329fdff488d0d9c370e00e8b766fdff488d0df0370e00e8db7cfeff488b05a4400e004c8bce4c8bc3488bd5488bcfffd08bd885c078184885f6741348833e00740d488bd5488bcee83afcffff8bc34883c4285f5e5d5bc3";
        static_assert(hex.size()==129*2);
        std::array<std::uint8_t,129> result{};
        const auto nibble=[](char c){return c<='9'?c-'0':c-'a'+10;};
        for(std::size_t i=0;i<result.size();++i)
            result[i]=static_cast<std::uint8_t>((nibble(hex[i*2])<<4)|nibble(hex[i*2+1]));
        return result;
    }();
    static constexpr SteamFactoryInlineProfile profile{
        "steam11057416.factory.inline-native-chain-v1",
        "fe4440b1027e96052b376ee04290d91df95a76b0d97d808cd92c993f0dec47b9",
        1679512,0x1d0000,0x9c250,0x180340,code};
    return profile;
}
Result<bool> validateSteamFactoryInline(const SteamFactoryInlineFacts& facts) {
    const auto& profile=steamFactoryInlineProfile();
    const auto& native=win11DxgiFactoryCreateSite();
    constexpr auto max=std::numeric_limits<std::uintptr_t>::max();
    if(!facts.nativeMethod||facts.nativeMethod>max-16||!facts.overlayBase||
       facts.overlayBase>max-profile.imageSize||facts.relay<10||
       facts.relay>max-facts.relayCode.size()||
       facts.overlayHash!=profile.moduleHash||facts.overlayFileSize!=profile.fileSize||
       facts.overlayImageSize!=profile.imageSize||
       facts.callback!=facts.overlayBase+profile.callbackRva||
       facts.originalTrampoline!=facts.relay-10||
       facts.callbackCode!=profile.callbackCode||facts.entry[0]!=0xe9||
       !std::equal(native.prologue.begin()+5,native.prologue.end(),facts.entry.begin()+5))
        return Error{ErrorCode::Conflict,"Steam native factory chain identity differs"};
    std::int32_t displacement{};
    std::memcpy(&displacement,facts.entry.data()+1,4);
    if(relativeTarget(facts.nativeMethod,5,displacement)!=facts.relay||
       !std::equal(facts.relayCode.begin(),facts.relayCode.begin()+6,
           std::array<std::uint8_t,6>{0xff,0x25,0,0,0,0}.begin()))
        return Error{ErrorCode::Conflict,"Steam native factory relay differs"};
    std::uintptr_t callback{};
    std::memcpy(&callback,facts.relayCode.data()+6,8);
    if(callback!=facts.callback||facts.trampolineCode[5]!=0xe9||
       !std::equal(native.prologue.begin(),native.prologue.begin()+5,
           facts.trampolineCode.begin()))
        return Error{ErrorCode::Conflict,"Steam original factory trampoline differs"};
    std::memcpy(&displacement,facts.trampolineCode.data()+6,4);
    if(relativeTarget(facts.originalTrampoline,10,displacement)!=facts.nativeMethod+5)
        return Error{ErrorCode::Conflict,"Steam original factory continuation differs"};
    return true;
}
Result<bool> inspectAndPinSteamFactoryInline(std::uintptr_t nativeMethod) {
    try {
        const auto entry=inspectFactoryMethodCode(nativeMethod);
        const auto relay=inspectFactoryMethodCode(entry.jumpTarget);
        if(!entry.readable||!entry.jumpTarget||!relay.readable||!relay.jumpTarget||
           entry.type!=MEM_IMAGE||!executable(entry.protection)||
           relay.type!=MEM_PRIVATE||!executable(relay.protection))
            return Error{ErrorCode::Unsupported,"Native factory has no supported Steam relay"};
        HMODULE overlay{};
        if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
            reinterpret_cast<LPCWSTR>(relay.jumpTarget),&overlay))
            return Error{ErrorCode::Unsupported,"Native factory callback has no module owner"};
        struct Ref {HMODULE value;~Ref(){FreeLibrary(value);}} reference{overlay};
        const auto& profile=steamFactoryInlineProfile();
        const auto base=reinterpret_cast<std::uintptr_t>(overlay);
        if(base>std::numeric_limits<std::uintptr_t>::max()-profile.imageSize||
           relay.jumpTarget!=base+profile.callbackRva)
            return Error{ErrorCode::Unsupported,"Native factory callback is not the pinned Steam site"};
        wchar_t path[32768]{};
        const auto count=GetModuleFileNameW(overlay,path,32768);
        if(!count||count>=32768||_wcsicmp(std::filesystem::path(path).filename().c_str(),
               L"GameOverlayRenderer64.dll")!=0)
            return Error{ErrorCode::Unsupported,"Native factory callback is not Steam overlay"};
        const auto hash=sha256File(path);
        if(!std::holds_alternative<std::string>(hash)||
           std::get<std::string>(hash)!=profile.moduleHash||
           std::filesystem::file_size(path)!=profile.fileSize)
            return Error{ErrorCode::Unsupported,"Steam overlay file identity differs"};
        IMAGE_DOS_HEADER dos{};
        IMAGE_NT_HEADERS64 nt{};
        if(!readCodeBytes(base,&dos,sizeof(dos))||dos.e_magic!=IMAGE_DOS_SIGNATURE||
           dos.e_lfanew<0||dos.e_lfanew>4096||
           !readCodeBytes(base+static_cast<unsigned>(dos.e_lfanew),&nt,sizeof(nt))||
           nt.Signature!=IMAGE_NT_SIGNATURE||nt.FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64||
           nt.OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC||
           nt.OptionalHeader.SizeOfImage!=profile.imageSize)
            return Error{ErrorCode::Conflict,"Steam overlay mapped image differs"};
        SteamFactoryInlineFacts facts{};
        facts.nativeMethod=nativeMethod;facts.relay=entry.jumpTarget;
        facts.callback=relay.jumpTarget;facts.overlayBase=base;
        facts.overlayHash=std::get<std::string>(hash);
        facts.overlayFileSize=profile.fileSize;facts.overlayImageSize=profile.imageSize;
        std::copy_n(entry.bytes.begin(),facts.entry.size(),facts.entry.begin());
        std::copy_n(relay.bytes.begin(),facts.relayCode.size(),facts.relayCode.begin());
        if(!readCodeBytes(base+profile.originalPointerRva,&facts.originalTrampoline,8)||
           !readCodeBytes(facts.originalTrampoline,facts.trampolineCode.data(),
               facts.trampolineCode.size())||
           !readCodeBytes(facts.callback,facts.callbackCode.data(),facts.callbackCode.size()))
            return Error{ErrorCode::Unavailable,"Steam factory proof bytes unavailable"};
        const auto trampoline=inspectFactoryMethodCode(facts.originalTrampoline);
        const auto callback=inspectFactoryMethodCode(facts.callback);
        if(trampoline.type!=MEM_PRIVATE||!executable(trampoline.protection)||
           trampoline.allocationBase!=relay.allocationBase||
           callback.type!=MEM_IMAGE||!executable(callback.protection)||
           callback.allocationBase!=base)
            return Error{ErrorCode::Conflict,"Steam factory code allocation/protection differs"};
        const auto validated=validateSteamFactoryInline(facts);
        if(const auto* error=std::get_if<Error>(&validated))return *error;
        HMODULE pinned{};
        if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN|
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
            reinterpret_cast<LPCWSTR>(facts.callback),&pinned)||pinned!=overlay)
            return Error{ErrorCode::Unavailable,"Cannot pin Steam callback lifetime"};
        // Re-read all mutable links after pinning. The saved next remains the
        // native entry, so Steam's code and pointer are never overwritten.
        auto current=facts;
        if(!readCodeBytes(nativeMethod,current.entry.data(),current.entry.size())||
           !readCodeBytes(current.relay,current.relayCode.data(),current.relayCode.size())||
           !readCodeBytes(base+profile.originalPointerRva,&current.originalTrampoline,8)||
           current.originalTrampoline!=facts.originalTrampoline||
           !readCodeBytes(current.originalTrampoline,current.trampolineCode.data(),current.trampolineCode.size())||
           !readCodeBytes(current.callback,current.callbackCode.data(),current.callbackCode.size())||
           current.entry!=facts.entry||current.relayCode!=facts.relayCode||
           current.trampolineCode!=facts.trampolineCode||current.callbackCode!=facts.callbackCode)
            return Error{ErrorCode::Conflict,"Steam factory proof changed during inspection"};
        return true;
    } catch(const std::exception& error) {
        return Error{ErrorCode::Unavailable,error.what()};
    }
}
bool isReshadeFactoryDelegateSite(IDXGIFactory* factory,
    std::uintptr_t moduleBase,std::string_view moduleHash,
    FactoryCreateFn originalMethod,const OwnedRouteSite& site) noexcept {
    if(!factory||!moduleBase||moduleHash!=site.moduleSha256||
       moduleBase>std::numeric_limits<std::uintptr_t>::max()-site.methodRva||
       reinterpret_cast<std::uintptr_t>(originalMethod)!=
           moduleBase+site.methodRva)return false;
    std::uintptr_t table{};
    SIZE_T count{};
    if(!ReadProcessMemory(GetCurrentProcess(),factory,&table,sizeof(table),
           &count)||count!=sizeof(table)||table<moduleBase)return false;
    return table-moduleBase==site.tableRva;
}

ReshadeFactoryDelegateFacts inspectReshadeFactoryDelegate(
    IDXGIFactory* verifiedReshadeFactory) noexcept {
    ReshadeFactoryDelegateFacts facts{};
    const auto wrapper=reinterpret_cast<std::uintptr_t>(
        verifiedReshadeFactory);
    if(!wrapper||wrapper>std::numeric_limits<std::uintptr_t>::max()-8)
        return facts;
    SIZE_T count{};
    if(!ReadProcessMemory(GetCurrentProcess(),
        reinterpret_cast<const void*>(wrapper+8),&facts.delegate,
        sizeof(facts.delegate),&count)||count!=sizeof(facts.delegate)||
       !facts.delegate)return facts;
    if(!ReadProcessMemory(GetCurrentProcess(),
        reinterpret_cast<const void*>(facts.delegate),&facts.vtable,
        sizeof(facts.vtable),&count)||count!=sizeof(facts.vtable)||
       !facts.vtable)return facts;
    if(facts.vtable>std::numeric_limits<std::uintptr_t>::max()-0x50)
        return facts;
    if(!ReadProcessMemory(GetCurrentProcess(),
        reinterpret_cast<const void*>(facts.vtable+0x50),
        &facts.createMethod,sizeof(facts.createMethod),&count)||
       count!=sizeof(facts.createMethod)||!facts.createMethod)return facts;
    MEMORY_BASIC_INFORMATION page{};
    if(!VirtualQuery(reinterpret_cast<const void*>(facts.createMethod),
        &page,sizeof(page))||page.State!=MEM_COMMIT||
       (page.Protect&PAGE_GUARD))return facts;
    const auto protection=page.Protect&0xff;
    facts.methodExecutable=protection==PAGE_EXECUTE||
        protection==PAGE_EXECUTE_READ||
        protection==PAGE_EXECUTE_READWRITE||
        protection==PAGE_EXECUTE_WRITECOPY;
    return facts;
}

bool isOwnedSceneFactoryCandidate(IDXGIFactory* factory,
    IDXGIFactory* expected,const DXGI_SWAP_CHAIN_DESC* description) noexcept {
    if(!factory||factory!=expected||!description)return false;
    const auto& desc=*description;
    return desc.BufferDesc.Width&&desc.BufferDesc.Height&&
        desc.BufferDesc.Width<=8192&&desc.BufferDesc.Height<=8192&&
        desc.BufferDesc.Format==DXGI_FORMAT_R8G8B8A8_UNORM&&
        desc.SampleDesc.Count==1&&desc.SampleDesc.Quality==0&&
        desc.BufferCount>=2&&desc.BufferCount<=16&&desc.OutputWindow&&
        (desc.BufferUsage&DXGI_USAGE_RENDER_TARGET_OUTPUT)&&
        (desc.SwapEffect==DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL||
         desc.SwapEffect==DXGI_SWAP_EFFECT_FLIP_DISCARD);
}
HRESULT observeFactoryCreate(FactoryCreateFn next,IDXGIFactory* factory,
    IUnknown* device,DXGI_SWAP_CHAIN_DESC* description,IDXGISwapChain** output,
    FactoryCreatedFn observed,void* context) noexcept {
    if(!next||!factory)return E_INVALIDARG;
    const auto result=next(factory,device,description,output);
    if(observed)observed(factory,device,description,
        output&&SUCCEEDED(result)?*output:nullptr,result,context);
    return result;
}
}
