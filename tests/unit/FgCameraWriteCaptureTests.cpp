#include <catch2/catch_test_macros.hpp>
#include "rk/FgCameraWriteCapture.hpp"
#include "rk/FgCameraWriteHooks.hpp"
#include <array>
#include <cstring>
#include <d3d11.h>
#include <wrl/client.h>

TEST_CASE("Camera admission checks every constant buffer descriptor field",
    "[fg_camera_writes]") {
    D3D11_BUFFER_DESC desc{};desc.ByteWidth=720;
    desc.Usage=D3D11_USAGE_DYNAMIC;desc.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
    REQUIRE(rk::isFgCameraWriteBuffer(desc));
    for(unsigned field=0;field<6;++field) {
        auto changed=desc;
        switch(field) {
        case 0:changed.ByteWidth=736;break;
        case 1:changed.Usage=D3D11_USAGE_DEFAULT;break;
        case 2:changed.BindFlags|=D3D11_BIND_SHADER_RESOURCE;break;
        case 3:changed.CPUAccessFlags|=D3D11_CPU_ACCESS_READ;break;
        case 4:changed.MiscFlags=D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;break;
        case 5:changed.StructureByteStride=16;break;
        }
        CAPTURE(field);REQUIRE_FALSE(rk::isFgCameraWriteBuffer(changed));
    }
}

TEST_CASE("Camera snapshots survive a real D3D11 mapped buffer lifetime",
    "[fg_camera_writes]") {
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    REQUIRE(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,
        nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)));
    D3D11_BUFFER_DESC desc{};desc.ByteWidth=720;
    desc.Usage=D3D11_USAGE_DYNAMIC;desc.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
    Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
    REQUIRE(SUCCEEDED(device->CreateBuffer(&desc,nullptr,&buffer)));
    const auto ctx=reinterpret_cast<std::uintptr_t>(context.Get());
    const auto buf=reinterpret_cast<std::uintptr_t>(buffer.Get());
    rk::FgCameraWriteCapture capture{ctx};capture.selectBuffer(buf);
    for(unsigned i=1;i<=2;++i) {
        D3D11_MAPPED_SUBRESOURCE mapped{};
        REQUIRE(SUCCEEDED(context->Map(buffer.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped)));
        std::memset(mapped.pData,static_cast<int>(i),720);
        REQUIRE(capture.mapped(ctx,buf,1,100,mapped.pData,720,true));
        REQUIRE(capture.beforeUnmap(ctx,buf,1,200));
        context->Unmap(buffer.Get(),0);
        const auto saved=capture.latest();REQUIRE(saved);
        REQUIRE(saved->revision==i);
        REQUIRE(saved->bytes.front()==i);REQUIRE(saved->bytes.back()==i);
    }
}

TEST_CASE("Camera writes copy owned bytes before their mapped lifetime ends",
    "[fg_camera_writes]") {
    rk::FgCameraWriteCapture capture{10};
    capture.selectBuffer(20);
    std::array<std::uint8_t,720> bytes{};
    bytes[0]=42;bytes[719]=17;
    REQUIRE(capture.mapped(10,20,3,100,bytes.data(),bytes.size(),true));
    REQUIRE_FALSE(capture.latest());
    REQUIRE(capture.beforeUnmap(10,20,3,200));
    bytes.fill(0);
    const auto saved=capture.latest();
    REQUIRE(saved);
    REQUIRE(saved->bytes[0]==42);
    REQUIRE(saved->bytes[719]==17);
    REQUIRE(saved->revision==1);
    REQUIRE(saved->mapCaller==100);
    REQUIRE(saved->unmapCaller==200);
    REQUIRE_FALSE(capture.beforeUnmap(10,20,3,200));
}

TEST_CASE("Identical camera bytes still record a new producer write",
    "[fg_camera_writes]") {
    rk::FgCameraWriteCapture capture{10};capture.selectBuffer(20);
    std::array<std::uint8_t,720> bytes{};
    REQUIRE(capture.mapped(10,20,3,100,bytes.data(),720,true));
    REQUIRE(capture.beforeUnmap(10,20,3,200));
    const auto before=capture.latest();
    REQUIRE(capture.mapped(10,20,3,100,bytes.data(),720,true));
    REQUIRE(capture.beforeUnmap(10,20,3,200));
    const auto after=capture.latest();
    REQUIRE(after->revision==2);
    REQUIRE(after->bytes==before->bytes);
    REQUIRE_FALSE(rk::sameFgCameraWrite(*before,*after));
    REQUIRE(rk::sameFgCameraWrite(*after,*after));
}

TEST_CASE("Camera map admission requires the selected context buffer and full write",
    "[fg_camera_writes]") {
    rk::FgCameraWriteCapture capture{10};capture.selectBuffer(20);
    std::array<std::uint8_t,720> bytes{};
    REQUIRE_FALSE(capture.mapped(11,20,3,100,bytes.data(),720,true));
    REQUIRE_FALSE(capture.mapped(10,21,3,100,bytes.data(),720,true));
    REQUIRE_FALSE(capture.mapped(10,20,0,100,bytes.data(),720,true));
    REQUIRE_FALSE(capture.mapped(10,20,3,100,nullptr,720,true));
    REQUIRE_FALSE(capture.mapped(10,20,3,100,bytes.data(),719,true));
    REQUIRE_FALSE(capture.mapped(10,20,3,100,bytes.data(),721,true));
    REQUIRE_FALSE(capture.mapped(10,20,3,100,bytes.data(),720,false));
    REQUIRE_FALSE(capture.beforeUnmap(10,20,3,200));
    REQUIRE_FALSE(capture.latest());
}

TEST_CASE("Failed or overlapping maps cannot publish a prior mapped pointer",
    "[fg_camera_writes]") {
    rk::FgCameraWriteCapture capture{10};capture.selectBuffer(20);
    std::array<std::uint8_t,720> bytes{};
    REQUIRE(capture.mapped(10,20,3,100,bytes.data(),720,true));
    REQUIRE_FALSE(capture.mapped(10,20,3,100,bytes.data(),720,false));
    REQUIRE_FALSE(capture.beforeUnmap(10,20,3,200));
    REQUIRE(capture.mapped(10,20,3,100,bytes.data(),720,true));
    REQUIRE_FALSE(capture.mapped(10,20,3,100,bytes.data(),720,true));
    REQUIRE_FALSE(capture.beforeUnmap(10,20,3,200));
}

TEST_CASE("Camera unmap requires the mapping thread and selected buffer generation",
    "[fg_camera_writes]") {
    rk::FgCameraWriteCapture capture{10};capture.selectBuffer(20);
    std::array<std::uint8_t,720> bytes{};
    REQUIRE(capture.mapped(10,20,3,100,bytes.data(),720,true));
    REQUIRE_FALSE(capture.beforeUnmap(10,20,4,200));
    REQUIRE_FALSE(capture.beforeUnmap(10,20,3,200));
    REQUIRE(capture.mapped(10,20,3,100,bytes.data(),720,true));
    capture.selectBuffer(21);
    REQUIRE_FALSE(capture.beforeUnmap(10,20,3,200));
    REQUIRE_FALSE(capture.latest());
    REQUIRE(capture.mapped(10,21,3,100,bytes.data(),720,true));
    REQUIRE(capture.beforeUnmap(10,21,3,200));
    REQUIRE(capture.latest()->buffer==21);
    capture.selectBuffer(0);
    REQUIRE_FALSE(capture.latest());
}

TEST_CASE("A rejected target write invalidates a previously valid camera snapshot",
    "[fg_camera_writes]") {
    rk::FgCameraWriteCapture capture{10};capture.selectBuffer(20);
    std::array<std::uint8_t,720> bytes{};
    REQUIRE(capture.mapped(10,20,3,100,bytes.data(),720,true));
    REQUIRE(capture.beforeUnmap(10,20,3,200));
    REQUIRE(capture.latest());
    REQUIRE_FALSE(capture.mapped(10,20,3,100,bytes.data(),720,false));
    REQUIRE_FALSE(capture.latest());
}

TEST_CASE("Camera history cannot cross buffer generation or reuse its address",
    "[fg_camera_writes]") {
    rk::FgCameraWriteCapture capture{10};capture.selectBuffer(20);
    std::array<std::uint8_t,720> bytes{};
    REQUIRE(capture.mapped(10,20,3,100,bytes.data(),720,true));
    REQUIRE(capture.beforeUnmap(10,20,3,200));
    const auto prior=*capture.latest();REQUIRE(prior.generation>0);
    capture.selectBuffer(0);capture.selectBuffer(20);
    REQUIRE(capture.mapped(10,20,3,100,bytes.data(),720,true));
    REQUIRE(capture.beforeUnmap(10,20,3,200));
    const auto next=*capture.latest();REQUIRE(next.generation>prior.generation);
    REQUIRE_FALSE(rk::canCompareFgCameraWriteHistory(prior,next));
}

TEST_CASE("Camera history comparison requires two distinct writes in one generation",
    "[fg_camera_writes]") {
    rk::FgCameraWrite prior{},next{};
    prior.revision=1;next.revision=2;prior.buffer=next.buffer=20;
    prior.generation=next.generation=1;
    REQUIRE(rk::canCompareFgCameraWriteHistory(prior,next));
    next.generation=2;REQUIRE_FALSE(rk::canCompareFgCameraWriteHistory(prior,next));
    next.generation=1;next.buffer=21;
    REQUIRE_FALSE(rk::canCompareFgCameraWriteHistory(prior,next));
    next.buffer=20;next.revision=1;
    REQUIRE_FALSE(rk::canCompareFgCameraWriteHistory(prior,next));
    next.revision=2;prior.generation=next.generation=0;
    REQUIRE_FALSE(rk::canCompareFgCameraWriteHistory(prior,next));
}
