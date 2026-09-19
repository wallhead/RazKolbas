#include <catch2/catch_test_macros.hpp>
#include "rk/RendererHook.hpp"
#include "rk/SwapObserver.hpp"
#include "rk/PointerPatch.hpp"
#include <wrl/client.h>
#include <optional>
#include <vector>
#include <cstring>
using Microsoft::WRL::ComPtr;
namespace {
std::optional<rk::Result<rk::RendererSnapshot>> observation;
void capture(const rk::DeviceCreationArgs& args,HRESULT result) { observation=rk::captureRendererSnapshot(args,result); }
struct WindowOwner {
    HWND window=CreateWindowExW(0,L"STATIC",L"RazKolbas renderer test",WS_POPUP,0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    ~WindowOwner() { if(window) DestroyWindow(window); }
};
}
namespace {
rk::PresentFn realPresent;
rk::ResizeFn realResize;
rk::ReleaseFn realRelease;
std::vector<rk::SwapEvent> swapEvents;
void traceSwap(const rk::SwapEvent& e) { swapEvents.push_back(e); }
HRESULT WINAPI presentProxy(IDXGISwapChain* s,UINT i,UINT f) { return rk::observePresent(realPresent,s,i,f,traceSwap); }
HRESULT WINAPI resizeProxy(IDXGISwapChain* s,UINT n,UINT w,UINT h,DXGI_FORMAT fmt,UINT f) { return rk::observeResize(realResize,s,n,w,h,fmt,f,traceSwap); }
ULONG WINAPI releaseProxy(IUnknown* s) { return rk::observeRelease(realRelease,s,traceSwap); }
}
TEST_CASE("Real swap-chain hooks preserve identity and genuine resize failure and success", "[renderer_capture]") {
    WindowOwner window;REQUIRE(window.window);
    DXGI_SWAP_CHAIN_DESC desc{};desc.BufferDesc.Width=64;desc.BufferDesc.Height=64;
    desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;
    desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.BufferCount=1;desc.OutputWindow=window.window;
    desc.Windowed=TRUE;desc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;ComPtr<IDXGISwapChain> swap;
    REQUIRE(SUCCEEDED(D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&desc,&swap,&device,nullptr,&context)));
    auto** table=*reinterpret_cast<void***>(swap.Get());
    realPresent=reinterpret_cast<rk::PresentFn>(table[8]);realResize=reinterpret_cast<rk::ResizeFn>(table[13]);realRelease=reinterpret_cast<rk::ReleaseFn>(table[2]);
    rk::PointerPatch presentPatch,resizePatch,releasePatch;
    REQUIRE(std::get<bool>(presentPatch.apply(table+8,reinterpret_cast<void*>(realPresent),reinterpret_cast<void*>(&presentProxy))));
    REQUIRE(std::get<bool>(resizePatch.apply(table+13,reinterpret_cast<void*>(realResize),reinterpret_cast<void*>(&resizeProxy))));
    REQUIRE(std::get<bool>(releasePatch.apply(table+2,reinterpret_cast<void*>(realRelease),reinterpret_cast<void*>(&releaseProxy))));
    ComPtr<IUnknown> identity;REQUIRE(SUCCEEDED(swap.As(&identity)));REQUIRE(identity.Get()==static_cast<IUnknown*>(swap.Get()));identity.Reset();
    swapEvents.clear();
    const auto test=swap->Present(0,DXGI_PRESENT_TEST);
    REQUIRE(swapEvents.size()==2);REQUIRE(swapEvents.back().result==test);REQUIRE(swapEvents.back().flags==DXGI_PRESENT_TEST);
    ComPtr<ID3D11Texture2D> heldBack;REQUIRE(SUCCEEDED(swap->GetBuffer(0,IID_PPV_ARGS(&heldBack))));
    const auto failed=swap->ResizeBuffers(1,128,96,DXGI_FORMAT_UNKNOWN,0);
    REQUIRE(failed==DXGI_ERROR_INVALID_CALL);REQUIRE(swapEvents.back().result==failed);
    REQUIRE(SUCCEEDED(swap->GetDesc(&desc)));REQUIRE(desc.BufferDesc.Width==64);
    heldBack.Reset();
    for(UINT i=0;i<8;++i) {
        const auto resized=swap->ResizeBuffers(1,128+i,96+i,DXGI_FORMAT_UNKNOWN,0);
        REQUIRE(resized==S_OK);REQUIRE(swapEvents.back().result==resized);
        REQUIRE(SUCCEEDED(swap->GetDesc(&desc)));REQUIRE(desc.BufferDesc.Width==128+i);REQUIRE(desc.BufferDesc.Height==96+i);
    }
    swap.Reset();REQUIRE(swapEvents.back().call==rk::SwapCall::Release);REQUIRE(swapEvents.back().references==0);
    REQUIRE(std::get<bool>(releasePatch.restore()));REQUIRE(std::get<bool>(resizePatch.restore()));REQUIRE(std::get<bool>(presentPatch.restore()));
    REQUIRE(table[8]==reinterpret_cast<void*>(realPresent));REQUIRE(table[13]==reinterpret_cast<void*>(realResize));
}
TEST_CASE("Failed device creation cannot publish stale output pointers", "[renderer_capture]") {
    auto badDevice=reinterpret_cast<ID3D11Device*>(0x1234);
    auto badSwap=reinterpret_cast<IDXGISwapChain*>(0x5678);
    rk::DeviceCreationArgs args{}; args.device=&badDevice;args.swapChain=&badSwap;
    REQUIRE(std::holds_alternative<rk::Error>(rk::captureRendererSnapshot(args,E_FAIL)));
}
TEST_CASE("Real D3D11 capture identifies the returned adapter and preserves backbuffer pixels", "[renderer_capture]") {
    WindowOwner window;
    REQUIRE(window.window!=nullptr);
    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferDesc.Width=64;desc.BufferDesc.Height=64;desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount=1;desc.OutputWindow=window.window;desc.Windowed=TRUE;desc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
    ComPtr<ID3D11Device> device; ComPtr<ID3D11DeviceContext> context; ComPtr<IDXGISwapChain> swap;
    D3D_FEATURE_LEVEL feature{};
    rk::DeviceCreationArgs args{nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,
        &desc,swap.GetAddressOf(),device.GetAddressOf(),&feature,context.GetAddressOf()};
    observation.reset();
    const auto status=rk::observeDeviceCreation(&D3D11CreateDeviceAndSwapChain,args,&capture);
    REQUIRE(SUCCEEDED(status)); REQUIRE(observation.has_value());
    REQUIRE(std::holds_alternative<rk::RendererSnapshot>(*observation));
    const auto snapshot=std::get<rk::RendererSnapshot>(*observation);
    ComPtr<IDXGIDevice> dxgi; REQUIRE(SUCCEEDED(device.As(&dxgi)));
    ComPtr<IDXGIAdapter> adapter; REQUIRE(SUCCEEDED(dxgi->GetAdapter(&adapter)));
    DXGI_ADAPTER_DESC actual{}; REQUIRE(SUCCEEDED(adapter->GetDesc(&actual)));
    REQUIRE(snapshot.luidLow==actual.AdapterLuid.LowPart); REQUIRE(snapshot.luidHigh==actual.AdapterLuid.HighPart);
    REQUIRE(snapshot.vendorId==actual.VendorId); REQUIRE(snapshot.deviceId==actual.DeviceId);
    REQUIRE_FALSE(snapshot.adapter.empty()); REQUIRE(snapshot.featureLevel==feature);
    REQUIRE(snapshot.width==64);REQUIRE(snapshot.height==64);REQUIRE(snapshot.format==DXGI_FORMAT_R8G8B8A8_UNORM);
    REQUIRE(snapshot.windowed);
    ComPtr<ID3D11Texture2D> back;
    REQUIRE(SUCCEEDED(swap->GetBuffer(0,IID_PPV_ARGS(&back))));
    ComPtr<ID3D11RenderTargetView> view;
    REQUIRE(SUCCEEDED(device->CreateRenderTargetView(back.Get(),nullptr,&view)));
    const float color[4]={0.25f,0.5f,0.75f,1.0f};context->ClearRenderTargetView(view.Get(),color);
    D3D11_TEXTURE2D_DESC stagingDesc{};back->GetDesc(&stagingDesc);
    stagingDesc.Usage=D3D11_USAGE_STAGING;stagingDesc.BindFlags=0;stagingDesc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;stagingDesc.MiscFlags=0;
    ComPtr<ID3D11Texture2D> staging;REQUIRE(SUCCEEDED(device->CreateTexture2D(&stagingDesc,nullptr,&staging)));
    const auto pixels=[&] {
        context->CopyResource(staging.Get(),back.Get());
        D3D11_MAPPED_SUBRESOURCE data{};
        const auto mapped=context->Map(staging.Get(),0,D3D11_MAP_READ,0,&data);
        REQUIRE(SUCCEEDED(mapped));
        std::vector<std::uint8_t> result(64*64*4);
        for(unsigned y=0;y<64;++y)std::memcpy(result.data()+y*64*4,static_cast<const std::uint8_t*>(data.pData)+y*data.RowPitch,64*4);
        context->Unmap(staging.Get(),0);return result;
    };
    const auto before=pixels();
    REQUIRE(std::holds_alternative<rk::RendererSnapshot>(rk::captureRendererSnapshot(args,status)));
    REQUIRE(pixels()==before);
    REQUIRE(before[0]==64);REQUIRE(before[1]==128);REQUIRE(before[2]==191);REQUIRE(before[3]==255);
}
