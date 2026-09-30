#include "rk/FgD3D11AuxSwapSource.hpp"
#include <Windows.h>
#include <limits>

namespace rk {
namespace {
bool sameIdentity(IUnknown* a,IUnknown* b) noexcept {
    Microsoft::WRL::ComPtr<IUnknown> left,right;
    return a&&b&&SUCCEEDED(a->QueryInterface(IID_PPV_ARGS(&left)))&&
        SUCCEEDED(b->QueryInterface(IID_PPV_ARGS(&right)))&&
        left.Get()==right.Get();
}
DXGI_FORMAT srgbFormat(DXGI_FORMAT format) noexcept {
    if(format==DXGI_FORMAT_R8G8B8A8_UNORM)
        return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    if(format==DXGI_FORMAT_B8G8R8A8_UNORM)
        return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    return DXGI_FORMAT_UNKNOWN;
}
}
FgD3D11AuxSwapSource::FgD3D11AuxSwapSource(FactoryCreateFn nativeCreate,
    IDXGIFactory* nativeFactory,ID3D11Device* nativeDevice,
    const DXGI_SWAP_CHAIN_DESC& gameDesc) noexcept:
    nativeCreate_(nativeCreate),nativeFactory_(nativeFactory),
    nativeDevice_(nativeDevice),gameDesc_(gameDesc) {}
FgD3D11AuxSwapSource::~FgD3D11AuxSwapSource() noexcept {
    buffer_.Reset();
    swap_.Reset();
    if(window_)DestroyWindow(window_);
}
Result<std::unique_ptr<FgD3D11AuxSwapSource>> FgD3D11AuxSwapSource::create(
    FactoryCreateFn nativeCreate,IDXGIFactory* nativeFactory,
    ID3D11Device* nativeDevice,const DXGI_SWAP_CHAIN_DESC& gameDesc) {
    if(!nativeCreate||!nativeFactory||!nativeDevice||
       !gameDesc.BufferDesc.Width||!gameDesc.BufferDesc.Height||
       gameDesc.BufferDesc.Width>static_cast<UINT>(
           std::numeric_limits<int>::max())||
       gameDesc.BufferDesc.Height>static_cast<UINT>(
           std::numeric_limits<int>::max())||
       srgbFormat(gameDesc.BufferDesc.Format)==DXGI_FORMAT_UNKNOWN||
       gameDesc.SampleDesc.Count!=1||gameDesc.SampleDesc.Quality!=0)
        return Error{ErrorCode::InvalidInput,
            "Auxiliary D3D11 swap requires a verified native factory and SDR format"};
    auto source=std::unique_ptr<FgD3D11AuxSwapSource>(new
        FgD3D11AuxSwapSource(nativeCreate,nativeFactory,nativeDevice,gameDesc));
    source->window_=CreateWindowExW(0,L"STATIC",L"RazKolbas auxiliary D3D11",
        WS_POPUP,0,0,static_cast<int>(gameDesc.BufferDesc.Width),
        static_cast<int>(gameDesc.BufferDesc.Height),nullptr,nullptr,
        GetModuleHandleW(nullptr),nullptr);
    if(!source->window_)
        return Error{ErrorCode::Unavailable,"Cannot create auxiliary D3D11 window"};
    auto auxiliaryDesc=gameDesc;
    auxiliaryDesc.OutputWindow=source->window_;
    auxiliaryDesc.Windowed=TRUE;
    auxiliaryDesc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
    auxiliaryDesc.BufferCount=1;
    auxiliaryDesc.Flags=0;
    auxiliaryDesc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT|
        (gameDesc.BufferUsage&DXGI_USAGE_SHADER_INPUT);
    if(FAILED(nativeCreate(nativeFactory,nativeDevice,&auxiliaryDesc,
        &source->swap_))||!source->swap_||
       FAILED(source->swap_->GetBuffer(0,IID_PPV_ARGS(&source->buffer_))))
        return Error{ErrorCode::Unavailable,"Cannot create auxiliary D3D11 swap buffer"};
    Microsoft::WRL::ComPtr<ID3D11Device> swapDevice;
    DXGI_SWAP_CHAIN_DESC actual{};
    if(FAILED(source->swap_->GetDevice(IID_PPV_ARGS(&swapDevice)))||
       !sameIdentity(swapDevice.Get(),nativeDevice)||
       FAILED(source->swap_->GetDesc(&actual))||
       actual.BufferDesc.Width!=gameDesc.BufferDesc.Width||
       actual.BufferDesc.Height!=gameDesc.BufferDesc.Height||
       actual.BufferDesc.Format!=gameDesc.BufferDesc.Format)
        return Error{ErrorCode::Conflict,
            "Auxiliary D3D11 swap device or descriptor differs"};
    D3D11_RENDER_TARGET_VIEW_DESC srgb{};
    srgb.Format=srgbFormat(gameDesc.BufferDesc.Format);
    srgb.ViewDimension=D3D11_RTV_DIMENSION_TEXTURE2D;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> ordinary,srgbView;
    if(FAILED(nativeDevice->CreateRenderTargetView(source->buffer_.Get(),
           nullptr,&ordinary))||
       FAILED(nativeDevice->CreateRenderTargetView(source->buffer_.Get(),
           &srgb,&srgbView)))
        return Error{ErrorCode::Unsupported,
            "Auxiliary D3D11 buffer lacks ordinary or sRGB render-target view"};
    if(gameDesc.BufferUsage&DXGI_USAGE_SHADER_INPUT) {
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> sampled;
        if(FAILED(nativeDevice->CreateShaderResourceView(source->buffer_.Get(),
            nullptr,&sampled)))
            return Error{ErrorCode::Unsupported,
                "Auxiliary D3D11 buffer lacks the requested shader-input view"};
    }
    return source;
}
Result<std::unique_ptr<FgD3D11AuxSwapSource>>
FgD3D11AuxSwapSource::prepare(UINT width,UINT height,DXGI_FORMAT format) const {
    auto desc=gameDesc_;
    desc.BufferDesc.Width=width;
    desc.BufferDesc.Height=height;
    desc.BufferDesc.Format=format;
    return create(nativeCreate_,nativeFactory_.Get(),nativeDevice_.Get(),desc);
}
}
