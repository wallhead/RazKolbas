#include "rk/OwnedSwapBufferRoute.hpp"
#include "rk/OwnedRouteProfile.hpp"
#include <utility>

namespace rk {
namespace {
Microsoft::WRL::ComPtr<IUnknown> canonical(IUnknown* value) noexcept {
    Microsoft::WRL::ComPtr<IUnknown> result;
    if(value)value->QueryInterface(IID_PPV_ARGS(result.GetAddressOf()));
    return result;
}
}
HRESULT OwnedSwapBufferRoute::configure(IDXGISwapChain* swap,SwapGetBufferFn next,
    SwapGetDescFn nextDesc,
    ReducedSdrSurface scene,std::uint64_t generation) noexcept {
    if(swap_||!swap||!next||!nextDesc||!scene.texture()||!generation)return E_INVALIDARG;
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
    Microsoft::WRL::ComPtr<ID3D11Device> swapDevice,sceneDevice;
    if(FAILED(swap->GetDevice(IID_PPV_ARGS(swapDevice.GetAddressOf())))||!swapDevice)
        return E_INVALIDARG;
    scene.texture()->GetDevice(sceneDevice.GetAddressOf());
    if(!sceneDevice||canonical(swapDevice.Get()).Get()!=canonical(sceneDevice.Get()).Get())
        return E_INVALIDARG;
    swap_=swap;next_=next;nextDesc_=nextDesc;scene_=std::move(scene);
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
HRESULT OwnedSwapBufferRoute::getBufferForConsumers(std::uintptr_t returnAddress,
    std::uintptr_t verifiedGameBase,std::string_view verifiedGameHash,
    std::uintptr_t verifiedEnbBase,std::string_view verifiedEnbHash,
    IDXGISwapChain* swap,UINT index,REFIID iid,void** output) const noexcept {
    const auto game=isSkyrim1170OwnedSceneBufferCall(returnAddress,
        verifiedGameBase,verifiedGameHash,swap,swap_.Get(),index,iid);
    const auto enb=isVerifiedEnbOwnedSceneBufferCall(returnAddress,
        verifiedEnbBase,verifiedEnbHash)&&swap==swap_.Get()&&index==0&&
        IsEqualIID(iid,__uuidof(ID3D11Texture2D));
    return getBuffer(swap,index,iid,output,game||enb);
}
HRESULT OwnedSwapBufferRoute::getDescForCaller(std::uintptr_t returnAddress,
    std::uintptr_t verifiedEnbBase,std::string_view verifiedEnbHash,
    IDXGISwapChain* swap,DXGI_SWAP_CHAIN_DESC* output) const noexcept {
    if(!output)return E_POINTER;
    if(!nextDesc_)return DXGI_ERROR_INVALID_CALL;
    const auto result=nextDesc_(swap,output);
    if(SUCCEEDED(result)&&scene_&&swap==swap_.Get()&&
       isVerifiedEnbReducedDescriptionCall(returnAddress,
           verifiedEnbBase,verifiedEnbHash)) {
        output->BufferDesc.Width=scene_->renderExtent().width;
        output->BufferDesc.Height=scene_->renderExtent().height;
    }
    return result;
}
bool OwnedSwapBufferRoute::belongsToDevice(ID3D11Device* device) const noexcept {
    if(!scene_||!device)return false;
    Microsoft::WRL::ComPtr<ID3D11Device> owner;
    scene_->texture()->GetDevice(owner.GetAddressOf());
    return owner&&canonical(owner.Get()).Get()==canonical(device).Get();
}
void OwnedSwapBufferRoute::releaseAfterRetirement() noexcept {
    scene_.reset();swap_.Reset();next_=nullptr;nextDesc_=nullptr;
    bufferCount_=0;generation_=0;
}
}
