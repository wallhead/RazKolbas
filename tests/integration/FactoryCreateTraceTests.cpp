#include <catch2/catch_test_macros.hpp>
#include "rk/FactoryCreateTrace.hpp"
#include "rk/OwnedRouteProfile.hpp"
#include "rk/PointerPatch.hpp"
#include "rk/PatchDescriptor.hpp"
#include <wrl/client.h>
#include <filesystem>
#include <cstring>

namespace {
using Microsoft::WRL::ComPtr;
struct Observation {
    unsigned calls{};
    IDXGIFactory* factory{};
    IDXGISwapChain* swap{};
    HRESULT result{E_FAIL};
};
void observed(IDXGIFactory* factory,IUnknown*,const DXGI_SWAP_CHAIN_DESC*,
    IDXGISwapChain* swap,HRESULT result,void* value) noexcept {
    auto& state=*static_cast<Observation*>(value);
    ++state.calls;state.factory=factory;state.swap=swap;state.result=result;
}

TEST_CASE("Factory method probe captures code and bounded jump targets without executing them",
    "[factory_create_trace]") {
    std::array<std::uint8_t,256> code{};
    code.fill(0xcc);
    const auto base=reinterpret_cast<std::uintptr_t>(code.data());
    code[0]=0xe9;
    const std::int32_t forward=128-5;
    std::memcpy(code.data()+1,&forward,sizeof(forward));
    auto facts=rk::inspectFactoryMethodCode(base);
    REQUIRE(facts.readable);
    REQUIRE(facts.bytes[0]==0xe9);
    REQUIRE(facts.jumpTarget==base+128);
    REQUIRE(facts.targetReadable);
    REQUIRE(facts.targetBytes[0]==0xcc);
    code[128]=0xe9;
    const std::int32_t backward=-128-5;
    std::memcpy(code.data()+129,&backward,sizeof(backward));
    REQUIRE(rk::inspectFactoryMethodCode(base+128).jumpTarget==base);
    code[0]=0xff;code[1]=0x25;
    const std::int32_t pointerOffset=80-6;
    std::memcpy(code.data()+2,&pointerOffset,sizeof(pointerOffset));
    const auto target=base+128;
    std::memcpy(code.data()+80,&target,sizeof(target));
    facts=rk::inspectFactoryMethodCode(base);
    REQUIRE(facts.indirectSlot==base+80);
    REQUIRE(facts.jumpTarget==target);
    code[0]=0x48;code[1]=0xb8;
    std::memcpy(code.data()+2,&target,sizeof(target));
    code[10]=0xff;code[11]=0xe0;
    REQUIRE(rk::inspectFactoryMethodCode(base).jumpTarget==target);
    code.fill(0xcc);
    REQUIRE(rk::inspectFactoryMethodCode(base).jumpTarget==0);
    REQUIRE_FALSE(rk::inspectFactoryMethodCode(1).readable);
    REQUIRE_FALSE(rk::inspectFactoryMethodCode(UINTPTR_MAX-32).readable);
}

TEST_CASE("Read-only code probe resolves a direct CALL without executing it",
    "[factory_create_trace]") {
    std::array<std::uint8_t,256> code{};
    code.fill(0xcc);
    const auto base=reinterpret_cast<std::uintptr_t>(code.data());
    code[0]=0xe8;
    const std::int32_t forward=128-5;
    std::memcpy(code.data()+1,&forward,sizeof(forward));
    auto facts=rk::inspectFactoryMethodCode(base);
    REQUIRE(facts.readable);
    REQUIRE(facts.callTarget==base+128);
    REQUIRE(facts.targetReadable);
    REQUIRE(facts.targetBytes[0]==0xcc);
    REQUIRE(facts.jumpTarget==0);
    code[128]=0xe8;
    const std::int32_t backward=-128-5;
    std::memcpy(code.data()+129,&backward,sizeof(backward));
    facts=rk::inspectFactoryMethodCode(base+128);
    REQUIRE(facts.callTarget==base);
    code[0]=0xcc;
    REQUIRE(rk::inspectFactoryMethodCode(base).callTarget==0);
}

TEST_CASE("Factory method probe refuses guard pages and records execute-only code",
    "[factory_create_trace]") {
    struct Allocation {
        void* value{VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE)};
        ~Allocation(){if(value)VirtualFree(value,0,MEM_RELEASE);}
    } allocation;
    REQUIRE(allocation.value!=nullptr);
    std::memset(allocation.value,0xcc,64);
    const auto base=reinterpret_cast<std::uintptr_t>(allocation.value);
    DWORD previous{};
    REQUIRE(VirtualProtect(allocation.value,4096,PAGE_EXECUTE,&previous));
    const auto executable=rk::inspectFactoryMethodCode(base);
    REQUIRE(executable.protection==PAGE_EXECUTE);
    REQUIRE(executable.readable);
    REQUIRE(executable.bytes[0]==0xcc);
    REQUIRE(VirtualProtect(allocation.value,4096,PAGE_READWRITE|PAGE_GUARD,&previous));
    REQUIRE_FALSE(rk::inspectFactoryMethodCode(base).readable);
    MEMORY_BASIC_INFORMATION region{};
    REQUIRE(VirtualQuery(allocation.value,&region,sizeof(region))==sizeof(region));
    REQUIRE((region.Protect&PAGE_GUARD)!=0);
    REQUIRE(VirtualProtect(allocation.value,4096,PAGE_NOACCESS,&previous));
    REQUIRE_FALSE(rk::inspectFactoryMethodCode(base).readable);
}
TEST_CASE("Exact Steam factory inline chain preserves the native ABI and rejects altered links",
    "[factory_create_trace]") {
    const auto& profile=rk::steamFactoryInlineProfile();
    rk::SteamFactoryInlineFacts facts{};
    facts.nativeMethod=0x180067c90;
    facts.relay=0x18010000a;
    facts.overlayBase=0x200000000;
    facts.callback=facts.overlayBase+profile.callbackRva;
    facts.originalTrampoline=facts.relay-10;
    facts.overlayHash=std::string(profile.moduleHash);
    facts.overlayFileSize=profile.fileSize;facts.overlayImageSize=profile.imageSize;
    facts.entry=rk::win11DxgiFactoryCreateSite().prologue;
    facts.entry[0]=0xe9;
    auto displacement=static_cast<std::int32_t>(facts.relay-(facts.nativeMethod+5));
    std::memcpy(facts.entry.data()+1,&displacement,4);
    facts.relayCode={0xff,0x25,0,0,0,0};
    std::memcpy(facts.relayCode.data()+6,&facts.callback,8);
    std::copy_n(rk::win11DxgiFactoryCreateSite().prologue.begin(),5,
        facts.trampolineCode.begin());
    facts.trampolineCode[5]=0xe9;
    displacement=static_cast<std::int32_t>(facts.nativeMethod+5-
        (facts.originalTrampoline+10));
    std::memcpy(facts.trampolineCode.data()+6,&displacement,4);
    facts.callbackCode=profile.callbackCode;
    REQUIRE(std::get<bool>(rk::validateSteamFactoryInline(facts)));
    const auto rejects=[](const auto& value) {
        return std::holds_alternative<rk::Error>(rk::validateSteamFactoryInline(value));
    };
    auto bad=facts;bad.overlayHash[0]^=1;REQUIRE(rejects(bad));
    bad=facts;bad.overlayFileSize++;REQUIRE(rejects(bad));
    bad=facts;bad.overlayImageSize++;REQUIRE(rejects(bad));
    bad=facts;bad.callback++;REQUIRE(rejects(bad));
    bad=facts;bad.entry[5]^=1;REQUIRE(rejects(bad));
    bad=facts;bad.entry[1]^=1;REQUIRE(rejects(bad));
    bad=facts;bad.relayCode[2]=1;REQUIRE(rejects(bad));
    bad=facts;bad.relayCode[6]^=1;REQUIRE(rejects(bad));
    bad=facts;bad.originalTrampoline++;REQUIRE(rejects(bad));
    bad=facts;bad.trampolineCode[0]^=1;REQUIRE(rejects(bad));
    bad=facts;bad.trampolineCode[6]^=1;REQUIRE(rejects(bad));
    bad=facts;bad.callbackCode[0x58]^=1;REQUIRE(rejects(bad));
    bad=facts;bad.overlayBase=UINTPTR_MAX-100;REQUIRE(rejects(bad));
    bad=facts;bad.nativeMethod=UINTPTR_MAX-3;REQUIRE(rejects(bad));
    bad=facts;bad.relay=9;REQUIRE(rejects(bad));
}
rk::FactoryCreateFn nativeNext{};
unsigned nativeCalls{};
HRESULT WINAPI nativeProxy(IDXGIFactory* factory,IUnknown* device,
    DXGI_SWAP_CHAIN_DESC* desc,IDXGISwapChain** swap) noexcept {
    ++nativeCalls;
    return nativeNext(factory,device,desc,swap);
}
}

TEST_CASE("Factory CreateSwapChain trace preserves the real WARP COM result",
    "[factory_create_trace]") {
    const auto window=CreateWindowW(L"STATIC",L"RazKolbas factory trace fixture",
        WS_OVERLAPPED,0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    REQUIRE(window!=nullptr);
    ComPtr<ID3D11Device> device;
    REQUIRE(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,
        nullptr,0,D3D11_SDK_VERSION,&device,nullptr,nullptr)));
    ComPtr<IDXGIDevice> dxgiDevice;
    REQUIRE(SUCCEEDED(device.As(&dxgiDevice)));
    ComPtr<IDXGIAdapter> adapter;
    REQUIRE(SUCCEEDED(dxgiDevice->GetAdapter(&adapter)));
    ComPtr<IDXGIFactory> factory;
    REQUIRE(SUCCEEDED(adapter->GetParent(IID_PPV_ARGS(&factory))));
    auto** table=*reinterpret_cast<void***>(factory.Get());
    auto next=reinterpret_cast<rk::FactoryCreateFn>(table[10]);
    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferDesc.Width=64;desc.BufferDesc.Height=32;
    desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount=2;desc.OutputWindow=window;
    desc.Windowed=TRUE;desc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
    Observation state;
    ComPtr<IDXGISwapChain> swap;
    const auto result=rk::observeFactoryCreate(next,factory.Get(),device.Get(),
        &desc,swap.GetAddressOf(),&observed,&state);
    REQUIRE(SUCCEEDED(result));
    REQUIRE(swap!=nullptr);
    REQUIRE(state.calls==1);
    REQUIRE(state.factory==factory.Get());
    REQUIRE(state.swap==swap.Get());
    REQUIRE(state.result==result);
    DXGI_SWAP_CHAIN_DESC actual{};
    REQUIRE(SUCCEEDED(swap->GetDesc(&actual)));
    REQUIRE(actual.BufferDesc.Width==64);
    REQUIRE(actual.BufferDesc.Height==32);
    swap.Reset();
    DestroyWindow(window);
}

TEST_CASE("Early owned scene admits only the captured native SDR factory contract",
    "[factory_create_trace]") {
    auto* expected=reinterpret_cast<IDXGIFactory*>(0x1000);
    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferDesc.Width=2560;desc.BufferDesc.Height=1440;
    desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount=3;desc.OutputWindow=reinterpret_cast<HWND>(0x2000);
    desc.Windowed=TRUE;desc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
    REQUIRE(rk::isOwnedSceneFactoryCandidate(expected,expected,&desc));
    REQUIRE_FALSE(rk::isOwnedSceneFactoryCandidate(
        reinterpret_cast<IDXGIFactory*>(0x1001),expected,&desc));
    auto changed=desc;changed.BufferDesc.Format=DXGI_FORMAT_R10G10B10A2_UNORM;
    REQUIRE_FALSE(rk::isOwnedSceneFactoryCandidate(expected,expected,&changed));
    changed=desc;changed.SampleDesc.Count=2;
    REQUIRE_FALSE(rk::isOwnedSceneFactoryCandidate(expected,expected,&changed));
    changed=desc;changed.BufferCount=1;
    REQUIRE_FALSE(rk::isOwnedSceneFactoryCandidate(expected,expected,&changed));
    changed=desc;changed.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
    REQUIRE_FALSE(rk::isOwnedSceneFactoryCandidate(expected,expected,&changed));
    changed=desc;changed.BufferUsage=DXGI_USAGE_SHADER_INPUT;
    REQUIRE_FALSE(rk::isOwnedSceneFactoryCandidate(expected,expected,&changed));
    changed=desc;changed.OutputWindow=nullptr;
    REQUIRE_FALSE(rk::isOwnedSceneFactoryCandidate(expected,expected,&changed));
}

TEST_CASE("ReShade factory delegate probe reads the bounded native vtable slot",
    "[factory_create_trace]") {
    void* methods[11]{};
    methods[10]=reinterpret_cast<void*>(&observed);
    struct Native { void** table; } native{methods};
    struct Wrapper { void* table; Native* delegate; } wrapper{methods,&native};
    const auto facts=rk::inspectReshadeFactoryDelegate(
        reinterpret_cast<IDXGIFactory*>(&wrapper));
    REQUIRE(facts.delegate==reinterpret_cast<std::uintptr_t>(&native));
    REQUIRE(facts.vtable==reinterpret_cast<std::uintptr_t>(methods));
    REQUIRE(facts.createMethod==reinterpret_cast<std::uintptr_t>(&observed));
    REQUIRE(facts.methodExecutable);
    REQUIRE_FALSE(rk::inspectReshadeFactoryDelegate(
        reinterpret_cast<IDXGIFactory*>(1)).methodExecutable);
}

TEST_CASE("ReShade delegate site selects the factory table, not the swap table",
    "[factory_create_trace]") {
    const auto& site=rk::reshade680FactoryCreateSite();
    void* factoryMethods[11]{};
    struct Wrapper { void** table; void* delegate; } wrapper{factoryMethods,nullptr};
    const auto table=reinterpret_cast<std::uintptr_t>(factoryMethods);
    const auto base=table-site.tableRva;
    const auto next=reinterpret_cast<rk::FactoryCreateFn>(base+site.methodRva);
    REQUIRE(rk::isReshadeFactoryDelegateSite(
        reinterpret_cast<IDXGIFactory*>(&wrapper),base,site.moduleSha256,
        next,site));
    REQUIRE_FALSE(rk::isReshadeFactoryDelegateSite(
        reinterpret_cast<IDXGIFactory*>(&wrapper),base,"different",next,site));
    void* swapMethods[11]{};
    wrapper.table=swapMethods;
    REQUIRE_FALSE(rk::isReshadeFactoryDelegateSite(
        reinterpret_cast<IDXGIFactory*>(&wrapper),base,site.moduleSha256,
        next,site));
}

TEST_CASE("Pinned native DXGI factory slot passes through a real WARP swap",
    "[factory_create_trace]") {
    const auto& site=rk::win11DxgiFactoryCreateSite();
    wchar_t systemDirectory[MAX_PATH]{};
    const auto length=GetSystemDirectoryW(systemDirectory,MAX_PATH);
    REQUIRE(length>0);
    REQUIRE(length<MAX_PATH);
    const auto path=std::filesystem::path(systemDirectory)/L"dxgi.dll";
    const auto hash=rk::sha256File(path);
    REQUIRE(std::holds_alternative<std::string>(hash));
    if(std::get<std::string>(hash)!=site.moduleSha256)
        SKIP("Local Windows DXGI differs from the pinned profile");
    ComPtr<ID3D11Device> device;
    REQUIRE(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,
        nullptr,0,D3D11_SDK_VERSION,&device,nullptr,nullptr)));
    ComPtr<IDXGIDevice> dxgiDevice;
    REQUIRE(SUCCEEDED(device.As(&dxgiDevice)));
    ComPtr<IDXGIAdapter> adapter;
    REQUIRE(SUCCEEDED(dxgiDevice->GetAdapter(&adapter)));
    ComPtr<IDXGIFactory> factory;
    REQUIRE(SUCCEEDED(adapter->GetParent(IID_PPV_ARGS(&factory))));
    auto** table=*reinterpret_cast<void***>(factory.Get());
    HMODULE owner{};
    REQUIRE(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
        reinterpret_cast<LPCWSTR>(table),&owner));
    const auto base=reinterpret_cast<std::uintptr_t>(owner);
    if(reinterpret_cast<std::uintptr_t>(table)-base!=site.tableRva) {
        FreeLibrary(owner);
        SKIP("WARP factory uses another native DXGI vtable class");
    }
    REQUIRE(table[site.slot]==reinterpret_cast<void*>(base+site.methodRva));
    nativeNext=reinterpret_cast<rk::FactoryCreateFn>(table[site.slot]);
    nativeCalls=0;
    const auto window=CreateWindowW(L"STATIC",L"RazKolbas native factory fixture",
        WS_OVERLAPPED,0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    REQUIRE(window!=nullptr);
    rk::PointerPatch patch;
    REQUIRE(std::holds_alternative<bool>(patch.apply(table+site.slot,
        reinterpret_cast<void*>(nativeNext),reinterpret_cast<void*>(&nativeProxy))));
    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferDesc.Width=64;desc.BufferDesc.Height=32;
    desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount=2;desc.OutputWindow=window;
    desc.Windowed=TRUE;desc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
    ComPtr<IDXGISwapChain> swap;
    const auto result=factory->CreateSwapChain(device.Get(),&desc,&swap);
    REQUIRE(SUCCEEDED(result));
    REQUIRE(swap!=nullptr);
    REQUIRE(nativeCalls==1);
    REQUIRE(std::holds_alternative<bool>(patch.restore()));
    swap.Reset();
    DestroyWindow(window);
    FreeLibrary(owner);
}
