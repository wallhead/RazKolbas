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
HRESULT STDMETHODCALLTYPE downstreamDesc(IDXGISwapChain* swap,
    DXGI_SWAP_CHAIN_DESC* output) {return swap->GetDesc(output);}
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
    REQUIRE(SUCCEEDED(route.configure(fixture.swap.Get(),&downstream,&downstreamDesc,fixture.scene(),7)));
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

TEST_CASE("Exact Skyrim view-cache caller receives the alias and ENB remains native",
    "[owned_swap_buffer]") {
    Fixture fixture;
    rk::OwnedSwapBufferRoute route;
    REQUIRE(SUCCEEDED(route.configure(fixture.swap.Get(),&downstream,&downstreamDesc,fixture.scene(),1)));
    constexpr std::uintptr_t gameBase=0x7ff6f59a0000;
    constexpr auto gameHash=
        "c434208894f07f604b852f29b8edc3a58c4de63de783373733e72b2b73f33be9";
    ComPtr<ID3D11Texture2D> scene,native;
    REQUIRE(SUCCEEDED(route.getBufferForCaller(gameBase+0xe4cc87,
        gameBase,gameHash,fixture.swap.Get(),0,IID_PPV_ARGS(&scene))));
    REQUIRE(SUCCEEDED(route.getBufferForCaller(gameBase+0xe4cc88,
        gameBase,gameHash,fixture.swap.Get(),0,IID_PPV_ARGS(&native))));
    REQUIRE(scene.Get()==route.sceneTexture());
    REQUIRE(native.Get()!=scene.Get());
    D3D11_TEXTURE2D_DESC a{},b{};
    scene->GetDesc(&a);native->GetDesc(&b);
    REQUIRE(a.Width==32);
    REQUIRE(b.Width==64);
    scene.Reset();native.Reset();route.releaseAfterRetirement();
}

TEST_CASE("Exact early ENB calls share the scene while its other calls remain native",
    "[owned_swap_buffer]") {
    Fixture fixture;
    rk::OwnedSwapBufferRoute route;
    REQUIRE(SUCCEEDED(route.configure(fixture.swap.Get(),&downstream,&downstreamDesc,
        fixture.scene(),1)));
    constexpr std::uintptr_t enbBase=0x180000000;
    constexpr auto enbHash="47ff220dd26a44520d4cec2d515d89effe87b632c1885c32388c93e8d0ceda58";
    ComPtr<ID3D11Texture2D> early,native;
    REQUIRE(SUCCEEDED(route.getBufferForConsumers(enbBase+0x5e580,
        0,"",enbBase,enbHash,fixture.swap.Get(),0,IID_PPV_ARGS(&early))));
    REQUIRE(SUCCEEDED(route.getBufferForConsumers(enbBase+0x5e581,
        0,"",enbBase,enbHash,fixture.swap.Get(),0,IID_PPV_ARGS(&native))));
    REQUIRE(early.Get()==route.sceneTexture());
    REQUIRE(native.Get()!=early.Get());
    DXGI_SWAP_CHAIN_DESC reduced{},full{};
    REQUIRE(SUCCEEDED(route.getDescForCaller(enbBase+0x5e53e,enbBase,enbHash,
        fixture.swap.Get(),&reduced)));
    REQUIRE(SUCCEEDED(route.getDescForCaller(enbBase+0x5e53f,enbBase,enbHash,
        fixture.swap.Get(),&full)));
    REQUIRE(reduced.BufferDesc.Width==32);
    REQUIRE(reduced.BufferDesc.Height==16);
    REQUIRE(full.BufferDesc.Width==64);
    REQUIRE(full.BufferDesc.Height==32);
    early.Reset();native.Reset();route.releaseAfterRetirement();
}

TEST_CASE("Owned route rejects a reduced scene from another D3D11 device",
    "[owned_swap_buffer]") {
    Fixture fixture;
    ComPtr<ID3D11Device> foreignDevice;
    ComPtr<ID3D11DeviceContext> foreignContext;
    REQUIRE(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,
        nullptr,0,D3D11_SDK_VERSION,&foreignDevice,nullptr,&foreignContext)));
    auto result=rk::createReducedSdrSurface(foreignDevice.Get(),{64,32},{32,16});
    REQUIRE(std::holds_alternative<rk::ReducedSdrSurface>(result));
    rk::OwnedSwapBufferRoute route;
    REQUIRE(route.configure(fixture.swap.Get(),&downstream,&downstreamDesc,
        std::move(std::get<rk::ReducedSdrSurface>(result)),1)==E_INVALIDARG);
    REQUIRE(route.sceneTexture()==nullptr);
}
