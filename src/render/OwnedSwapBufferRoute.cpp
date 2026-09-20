#include "rk/OwnedSwapBufferRoute.hpp"
#include "rk/OwnedRouteProfile.hpp"
#include <utility>

namespace rk {
HRESULT OwnedSwapBufferRoute::configure(IDXGISwapChain* swap,SwapGetBufferFn next,
    ReducedSdrSurface scene,std::uint64_t generation) noexcept {
    if(swap_||!swap||!next||!scene.texture()||!generation)return E_INVALIDARG;
    DXGI_SWAP_CHAIN_DESC desc{};
    const auto result=swap->GetDesc(&desc);
    if(FAILED(result))return result;
    if(!desc.BufferCount||desc.BufferCount>16||
       desc.BufferDesc.Width!=scene.displayExtent().width||
       desc.BufferDesc.Height!=scene.displayExtent().height||
       desc.BufferDesc.Format!=DXGI_FORMAT_R8G8B8A8_UNORM)return E_INVALIDARG;
    D3D11_TEXTURE2D_DESC reduced{};scene.texture()->GetDesc(&reduced);
    if(reduced.Width!=scene.renderExtent().width||
       reduced.Height!=scene.renderExtent().height||
       reduced.Format!=desc.BufferDesc.Format||
       reduced.Width>=desc.BufferDesc.Width&&
       reduced.Height>=desc.BufferDesc.Height)return E_INVALIDARG;
    swap_=swap;next_=next;scene_=std::move(scene);
    generation_=generation;bufferCount_=desc.BufferCount;
    return S_OK;
}
HRESULT OwnedSwapBufferRoute::getBuffer(IDXGISwapChain* swap,UINT index,
    REFIID iid,void** output,bool worldConsumer) const noexcept {
    if(!output)return E_POINTER;
    if(worldConsumer&&swap==swap_.Get()&&scene_) {
        *output=nullptr;
        if(index>=bufferCount_)return DXGI_ERROR_INVALID_CALL;
        return scene_->queryBuffer(iid,output);
    }
    if(!next_) { *output=nullptr;return DXGI_ERROR_INVALID_CALL; }
    return next_(swap,index,iid,output);
}
HRESULT OwnedSwapBufferRoute::getBufferForCaller(std::uintptr_t returnAddress,
    std::uintptr_t verifiedGameBase,std::string_view verifiedGameHash,
    IDXGISwapChain* swap,UINT index,REFIID iid,void** output) const noexcept {
    const auto world=isSkyrim1170OwnedSceneBufferCall(returnAddress,
        verifiedGameBase,verifiedGameHash,swap,swap_.Get(),index,iid);
    return getBuffer(swap,index,iid,output,world);
}
void OwnedSwapBufferRoute::releaseAfterRetirement() noexcept {
    scene_.reset();swap_.Reset();next_=nullptr;bufferCount_=0;generation_=0;
}
}
