#include "rk/FgStreamlineRuntime.hpp"
#include "rk/FactoryCreateTrace.hpp"
#include "rk/OwnedRouteProfile.hpp"
#include "rk/PatchDescriptor.hpp"
#include <d3d12.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <iostream>
#include <variant>

int wmain(int argc,wchar_t** argv) {
    if(argc!=2&&argc!=3&&
       !(argc==4&&std::wcscmp(argv[2],L"--early-reshade")==0))return 1;
    HMODULE reshade{};
    Microsoft::WRL::ComPtr<IDXGIFactory1> wrapper;
    const auto reshadePath=argc==3?argv[2]:argc==4?argv[3]:nullptr;
    const auto createWrapper=[&]() -> int {
        const auto& site=rk::reshade680FactoryCreateSite();
        const auto digest=rk::sha256File(reshadePath);
        if(!std::holds_alternative<std::string>(digest)||
           std::get<std::string>(digest)!=site.moduleSha256)return 5;
        reshade=LoadLibraryW(reshadePath);
        if(!reshade)return 6;
        using Create=HRESULT(WINAPI*)(REFIID,void**);
        const auto create=reinterpret_cast<Create>(
            GetProcAddress(reshade,"CreateDXGIFactory1"));
        if(!create||FAILED(create(IID_PPV_ARGS(&wrapper))))return 7;
        const auto before=rk::inspectReshadeFactoryDelegate(wrapper.Get());
        if(!before.methodExecutable)return 8;
        std::cout<<"created factory method=0x"<<std::hex<<
            before.createMethod<<std::dec<<'\n';
        return 0;
    };
    if(argc==3)if(const auto result=createWrapper())return result;
    auto loaded=rk::FgStreamlineRuntime::initialize(argv[1]);
    if(const auto error=std::get_if<rk::Error>(&loaded)) {
        std::cerr<<"private-sl-init: "<<error->message<<'\n';
        return 2;
    }
    auto runtime=std::move(std::get<std::unique_ptr<rk::FgStreamlineRuntime>>(
        loaded));
    std::wcout<<L"private-sl-init=ok directory="<<runtime->directory()<<L'\n';
    if(runtime->setD3DDevice(nullptr)!=sl::Result::eErrorInvalidParameter||
       runtime->upgradeInterface(nullptr)!=sl::Result::eErrorInvalidParameter)
        return 3;
    Microsoft::WRL::ComPtr<ID3D12Device> d12;
    if(reshadePath) {
        Microsoft::WRL::ComPtr<IDXGIFactory1> nativeFactory;
        if(FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&nativeFactory))))return 13;
        for(UINT index=0;;++index) {
            Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
            if(nativeFactory->EnumAdapters1(index,&adapter)==DXGI_ERROR_NOT_FOUND)
                break;
            DXGI_ADAPTER_DESC1 description{};
            if(SUCCEEDED(adapter->GetDesc1(&description))&&
               description.VendorId==0x10de&&
               SUCCEEDED(D3D12CreateDevice(adapter.Get(),
                   D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&d12))))break;
        }
        if(!d12)return 14;
        const auto set=runtime->setD3DDevice(d12.Get());
        std::cout<<"slSetD3DDevice="<<static_cast<int>(set)<<'\n';
        if(set!=sl::Result::eOk)return 15;
    }
    if(argc==4)if(const auto result=createWrapper())return result;
    if(wrapper) {
        const auto before=rk::inspectReshadeFactoryDelegate(wrapper.Get());
        auto* delegate=reinterpret_cast<IDXGIFactory*>(before.delegate);
        if(!delegate)return 9;
        void* upgraded=delegate;
        const auto upgradeResult=runtime->upgradeInterface(&upgraded);
        std::cout<<"post-init factory upgrade="<<
            static_cast<int>(upgradeResult)<<" distinct="<<
            (upgraded!=delegate)<<'\n';
        if(upgradeResult!=sl::Result::eOk)return 10;
        const auto upgradedMethod=reinterpret_cast<std::uintptr_t>(
            (*reinterpret_cast<void***>(upgraded))[10]);
        HMODULE upgradedOwner{};
        if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
            reinterpret_cast<LPCWSTR>(upgradedMethod),&upgradedOwner))return 11;
        wchar_t upgradedPath[32768]{};
        GetModuleFileNameW(upgradedOwner,upgradedPath,32768);
        std::wcout<<L"upgraded method owner="<<upgradedPath<<L'\n';
        std::cout<<"upgraded method RVA=0x"<<std::hex<<
            (upgradedMethod-reinterpret_cast<std::uintptr_t>(upgradedOwner))
            <<std::dec<<'\n';
        const auto upgradedHash=rk::sha256File(upgradedPath);
        const bool upgradedVerified=upgraded!=delegate&&
            std::holds_alternative<std::string>(upgradedHash)&&
            std::get<std::string>(upgradedHash)==
                rk::streamline2141FactoryCreateSite().moduleSha256;
        FreeLibrary(upgradedOwner);
        if(upgraded!=delegate)static_cast<IUnknown*>(upgraded)->Release();
        if(!upgradedVerified)return 12;
        const auto after=rk::inspectReshadeFactoryDelegate(wrapper.Get());
        HMODULE owner{};
        if(!after.methodExecutable||
           !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
               reinterpret_cast<LPCWSTR>(after.createMethod),&owner))return 11;
        const auto ownerBase=reinterpret_cast<std::uintptr_t>(owner);
        wchar_t path[32768]{};
        const auto count=GetModuleFileNameW(owner,path,32768);
        const auto digest=count&&count<32768?
            rk::sha256File(path):rk::Result<std::string>{rk::Error{
                rk::ErrorCode::Unavailable,"path unavailable"}};
        std::wcout<<L"post-init method owner="<<path<<L'\n';
        std::cout<<"post-init method RVA=0x"<<std::hex<<
            (after.createMethod-ownerBase)<<std::dec<<'\n';
        const bool verified=std::holds_alternative<std::string>(digest)&&
            std::get<std::string>(digest)==rk::win11DxgiFactoryCreateSite().moduleSha256&&
            after.createMethod==ownerBase+
                rk::win11DxgiFactoryCreateSite().methodRva;
        FreeLibrary(owner);
        if(!verified)return 12;
        wrapper.Reset();
    }
    d12.Reset();
    const auto stopped=runtime->shutdown();
    if(const auto error=std::get_if<rk::Error>(&stopped)) {
        std::cerr<<"private-sl-shutdown: "<<error->message<<'\n';
        return 4;
    }
    std::cout<<"private-sl-shutdown=ok\n";
    if(reshade)FreeLibrary(reshade);
    return 0;
}
