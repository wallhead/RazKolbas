#include <catch2/catch_test_macros.hpp>
#include "rk/RendererHook.hpp"
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
