#include <catch2/catch_test_macros.hpp>
#include "rk/OwnedSwapBufferRoute.hpp"
#include <wrl/client.h>

namespace {
using Microsoft::WRL::ComPtr;
struct Window {
    HWND handle{CreateWindowW(L"STATIC",L"RazKolbas buffer route fixture",
        WS_OVERLAPPED,0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr)};
    ~Window(){if(handle)DestroyWindow(handle);}
};
HRESULT STDMETHODCALLTYPE downstream(IDXGISwapChain* swap,UINT index,
    REFIID iid,void** output) {return swap->GetBuffer(index,iid,output);}
struct Fixture {
    Window window;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGISwapChain> swap;
    Fixture() {
        REQUIRE(window.handle!=nullptr);
        DXGI_SWAP_CHAIN_DESC desc{};
        desc.BufferDesc.Width=64;desc.BufferDesc.Height=32;
        desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount=2;desc.OutputWindow=window.handle;
        desc.Windowed=TRUE;desc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
        REQUIRE(SUCCEEDED(D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_WARP,
            nullptr,0,nullptr,0,D3D11_SDK_VERSION,&desc,&swap,&device,nullptr,&context)));
    }
    rk::ReducedSdrSurface scene() {
        auto result=rk::createReducedSdrSurface(device.Get(),{64,32},{32,16});
        REQUIRE(std::holds_alternative<rk::ReducedSdrSurface>(result));
        return std::move(std::get<rk::ReducedSdrSurface>(result));
    }
};
}

TEST_CASE("World GetBuffer exposes stable reduced identity while native calls forward",
    "[owned_swap_buffer]") {
    Fixture fixture;
    rk::OwnedSwapBufferRoute route;
    REQUIRE(SUCCEEDED(route.configure(fixture.swap.Get(),&downstream,fixture.scene(),7)));
    REQUIRE(route.generation()==7);
    ComPtr<ID3D11Texture2D> first,second,native;
    REQUIRE(SUCCEEDED(route.getBuffer(fixture.swap.Get(),0,
        IID_PPV_ARGS(&first),true)));
    REQUIRE(SUCCEEDED(route.getBuffer(fixture.swap.Get(),0,
        IID_PPV_ARGS(&second),true)));
    REQUIRE(first.Get()==second.Get());
    REQUIRE(first.Get()==route.sceneTexture());
    REQUIRE(SUCCEEDED(route.getBuffer(fixture.swap.Get(),0,
        IID_PPV_ARGS(&native),false)));
    REQUIRE(native.Get()!=first.Get());
    D3D11_TEXTURE2D_DESC reduced{},full{};
    first->GetDesc(&reduced);native->GetDesc(&full);
    REQUIRE(reduced.Width==32);REQUIRE(reduced.Height==16);
    REQUIRE(full.Width==64);REQUIRE(full.Height==32);
    void* invalid=reinterpret_cast<void*>(1);
    REQUIRE(route.getBuffer(fixture.swap.Get(),2,__uuidof(ID3D11Texture2D),
        &invalid,true)==DXGI_ERROR_INVALID_CALL);
    REQUIRE(invalid==nullptr);
    native.Reset();first.Reset();second.Reset();
    route.releaseAfterRetirement();
    REQUIRE(route.sceneTexture()==nullptr);
    REQUIRE(route.generation()==0);
}
