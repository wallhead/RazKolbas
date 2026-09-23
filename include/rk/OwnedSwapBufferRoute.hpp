#pragma once
#include "rk/ReducedSdrSurface.hpp"
#include <dxgi.h>
#include <wrl/client.h>
#include <cstdint>
#include <optional>
#include <string_view>

namespace rk {
using SwapGetBufferFn=HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*,UINT,REFIID,void**);
using SwapGetDescFn=HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*,DXGI_SWAP_CHAIN_DESC*);
// Instance-scoped GetBuffer route for a verified ReShade swap table hook.
// The outer installer decides which callers own world scene buffers; internal
// ReShade/presentation calls must continue through the saved downstream method.
class OwnedSwapBufferRoute final {
public:
    HRESULT configure(IDXGISwapChain* swap,SwapGetBufferFn next,SwapGetDescFn nextDesc,
        ReducedSdrSurface scene,std::uint64_t generation) noexcept;
    HRESULT getBuffer(IDXGISwapChain* swap,UINT index,REFIID iid,void** output,
        bool worldConsumer) const noexcept;
    HRESULT getBufferForCaller(std::uintptr_t returnAddress,
        std::uintptr_t verifiedGameBase,std::string_view verifiedGameHash,
        IDXGISwapChain* swap,UINT index,REFIID iid,void** output) const noexcept;
    HRESULT getBufferForConsumers(std::uintptr_t returnAddress,
        std::uintptr_t verifiedGameBase,std::string_view verifiedGameHash,
        std::uintptr_t verifiedEnbBase,std::string_view verifiedEnbHash,
        IDXGISwapChain* swap,UINT index,REFIID iid,void** output) const noexcept;
    HRESULT getDescForCaller(std::uintptr_t returnAddress,
        std::uintptr_t verifiedEnbBase,std::string_view verifiedEnbHash,
        IDXGISwapChain* swap,DXGI_SWAP_CHAIN_DESC* output) const noexcept;
    ID3D11Texture2D* sceneTexture() const noexcept {
        return scene_?scene_->texture():nullptr;
    }
    std::uint64_t generation() const noexcept { return generation_; }
    Extent renderExtent() const noexcept { return scene_?scene_->renderExtent():Extent{}; }
    Extent displayExtent() const noexcept { return scene_?scene_->displayExtent():Extent{}; }
    bool belongsToDevice(ID3D11Device* device) const noexcept;
    void releaseAfterRetirement() noexcept;
private:
    Microsoft::WRL::ComPtr<IDXGISwapChain> swap_;
    SwapGetBufferFn next_{};
    SwapGetDescFn nextDesc_{};
    std::optional<ReducedSdrSurface> scene_;
    UINT bufferCount_{};
    std::uint64_t generation_{};
};
}
