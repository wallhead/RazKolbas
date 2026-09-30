#include "rk/FactoryCreateTrace.hpp"
#include "rk/OwnedRouteProfile.hpp"
#include <Windows.h>
#include <limits>
#include <cstring>

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
