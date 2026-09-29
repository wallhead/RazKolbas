#include "rk/FactoryCreateTrace.hpp"
#include "rk/OwnedRouteProfile.hpp"
#include <Windows.h>
#include <limits>

namespace rk {
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
