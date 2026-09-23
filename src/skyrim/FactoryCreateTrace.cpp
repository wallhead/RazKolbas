#include "rk/FactoryCreateTrace.hpp"

namespace rk {
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
