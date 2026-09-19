#include "rk/RendererHook.hpp"
#include <wrl/client.h>
namespace rk {
Result<RendererSnapshot> captureRendererSnapshot(const DeviceCreationArgs& args, HRESULT result) {
    using Microsoft::WRL::ComPtr;
    if (FAILED(result)) return Error{ErrorCode::Unavailable,"Device creation failed; output pointers are not inspected"};
    if (!args.device || !*args.device || !args.swapChain || !*args.swapChain)
        return Error{ErrorCode::Unavailable,"Creation returned no device/swap chain pair"};
    ComPtr<IUnknown> deviceIdentity, swapIdentity, contextIdentity;
    ComPtr<ID3D11Device> swapDevice;
    if (FAILED((*args.device)->QueryInterface(IID_PPV_ARGS(&deviceIdentity))) ||
        FAILED((*args.swapChain)->GetDevice(IID_PPV_ARGS(&swapDevice))) || FAILED(swapDevice.As(&swapIdentity)) ||
        deviceIdentity.Get()!=swapIdentity.Get())
        return Error{ErrorCode::Conflict,"Device and swap chain identities differ"};
    if (args.context && *args.context) {
        ComPtr<ID3D11Device> contextDevice;
        (*args.context)->GetDevice(&contextDevice);
        if (!contextDevice || FAILED(contextDevice.As(&contextIdentity)) || deviceIdentity.Get()!=contextIdentity.Get())
            return Error{ErrorCode::Conflict,"Immediate context belongs to another device"};
    }
    ComPtr<IDXGIDevice> dxgi;
    ComPtr<IDXGIAdapter> adapter;
    DXGI_ADAPTER_DESC adapterDesc{};
    DXGI_SWAP_CHAIN_DESC swapDesc{};
    if (FAILED((*args.device)->QueryInterface(IID_PPV_ARGS(&dxgi))) || FAILED(dxgi->GetAdapter(&adapter)) ||
        FAILED(adapter->GetDesc(&adapterDesc)) || FAILED((*args.swapChain)->GetDesc(&swapDesc)))
        return Error{ErrorCode::Unavailable,"Renderer adapter or swap-chain description unavailable"};
    RendererSnapshot snapshot;
    const auto length=WideCharToMultiByte(CP_UTF8,0,adapterDesc.Description,-1,nullptr,0,nullptr,nullptr);
    if (length<=0) return Error{ErrorCode::Unavailable,"Adapter name conversion failed"};
    snapshot.adapter.resize(static_cast<std::size_t>(length));
    if (!WideCharToMultiByte(CP_UTF8,0,adapterDesc.Description,-1,snapshot.adapter.data(),length,nullptr,nullptr))
        return Error{ErrorCode::Unavailable,"Adapter name conversion failed"};
    snapshot.adapter.pop_back();
    snapshot.vendorId=adapterDesc.VendorId; snapshot.deviceId=adapterDesc.DeviceId;
    snapshot.luidLow=adapterDesc.AdapterLuid.LowPart; snapshot.luidHigh=adapterDesc.AdapterLuid.HighPart;
    snapshot.featureLevel=(*args.device)->GetFeatureLevel(); snapshot.deviceFlags=(*args.device)->GetCreationFlags();
    snapshot.width=swapDesc.BufferDesc.Width; snapshot.height=swapDesc.BufferDesc.Height;
    snapshot.format=swapDesc.BufferDesc.Format; snapshot.bufferCount=swapDesc.BufferCount;
    snapshot.sampleCount=swapDesc.SampleDesc.Count; snapshot.swapEffect=swapDesc.SwapEffect;
    snapshot.windowed=swapDesc.Windowed!=FALSE;
    return snapshot;
}
}
