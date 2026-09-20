#include "rk/FactoryCreateTrace.hpp"

namespace rk {
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
