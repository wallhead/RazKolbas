#include "rk/FgSwapFacadeProbe.hpp"
#include <wrl/client.h>

namespace rk {
FgSwapFacadeFacts inspectFgSwapFacade(IDXGISwapChain* swap,
    IUnknown* expectedDevice) noexcept {
    FgSwapFacadeFacts facts{};
    if(!swap)return facts;
    facts.getDesc=swap->GetDesc(&facts.desc);
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap1;
    Microsoft::WRL::ComPtr<IDXGISwapChain3> swap3;
    Microsoft::WRL::ComPtr<IDXGISwapChain4> swap4;
    facts.swap1=swap->QueryInterface(IID_PPV_ARGS(&swap1));
    facts.swap3=swap->QueryInterface(IID_PPV_ARGS(&swap3));
    facts.swap4=swap->QueryInterface(IID_PPV_ARGS(&swap4));
    Microsoft::WRL::ComPtr<ID3D11Device> d11;
    Microsoft::WRL::ComPtr<ID3D12Device> d12;
    facts.getD3D11Device=swap->GetDevice(IID_PPV_ARGS(&d11));
    facts.getD3D12Device=swap->GetDevice(IID_PPV_ARGS(&d12));
    if(SUCCEEDED(facts.getD3D11Device)&&expectedDevice) {
        Microsoft::WRL::ComPtr<IUnknown> actualIdentity,expectedIdentity;
        if(SUCCEEDED(d11.As(&actualIdentity))&&
           SUCCEEDED(expectedDevice->QueryInterface(
               IID_PPV_ARGS(&expectedIdentity))))
            facts.expectedDeviceIdentity=
                actualIdentity.Get()==expectedIdentity.Get();
    }
    return facts;
}
}
