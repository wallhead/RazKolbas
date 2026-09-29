#pragma once
#include "rk/FactoryCreateTrace.hpp"
#include "rk/Result.hpp"
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <memory>

namespace rk {
// A real D3D11 swap-chain back buffer supplies both ordinary and sRGB RTVs.
// Each resize prepares a new hidden-window generation before the lower swap
// changes. The caller must pin and verify the native factory method's module.
class FgD3D11AuxSwapSource final {
public:
    static Result<std::unique_ptr<FgD3D11AuxSwapSource>> create(
        FactoryCreateFn nativeCreate,IDXGIFactory* nativeFactory,
        ID3D11Device* nativeDevice,const DXGI_SWAP_CHAIN_DESC& gameDesc);
    ~FgD3D11AuxSwapSource() noexcept;
    FgD3D11AuxSwapSource(const FgD3D11AuxSwapSource&)=delete;
    FgD3D11AuxSwapSource& operator=(const FgD3D11AuxSwapSource&)=delete;
    Result<std::unique_ptr<FgD3D11AuxSwapSource>> prepare(
        UINT width,UINT height,DXGI_FORMAT format) const;
    ID3D11Texture2D* buffer() const noexcept {return buffer_.Get();}
private:
    FgD3D11AuxSwapSource(FactoryCreateFn nativeCreate,
        IDXGIFactory* nativeFactory,ID3D11Device* nativeDevice,
        const DXGI_SWAP_CHAIN_DESC& gameDesc) noexcept;
    FactoryCreateFn nativeCreate_{};
    Microsoft::WRL::ComPtr<IDXGIFactory> nativeFactory_;
    Microsoft::WRL::ComPtr<ID3D11Device> nativeDevice_;
    DXGI_SWAP_CHAIN_DESC gameDesc_{};
    HWND window_{};
    Microsoft::WRL::ComPtr<IDXGISwapChain> swap_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> buffer_;
};
}
