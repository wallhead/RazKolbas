#include <catch2/catch_test_macros.hpp>
#include "rk/OwnedSceneDomain.hpp"
#include "rk/RipCall6.hpp"
#include "rk/RendererLogicalSize.hpp"
#include <Windows.h>
#include <array>
#include <cstring>
#include <limits>
#include <thread>

namespace {
using RectCall=BOOL(WINAPI*)(HWND,RECT*);
RectCall priorRectCall{};
unsigned rectCalls{};
BOOL WINAPI fixtureGetClientRect(HWND window,RECT* rect) {
    ++rectCalls;
    if(window!=reinterpret_cast<HWND>(0x1234)||!rect)return FALSE;
    *rect={0,0,2560,1440};return TRUE;
}
BOOL WINAPI fixtureReducedRect(HWND window,RECT* rect) {
    const auto result=priorRectCall(window,rect);
    if(result&&rect){rect->right=1280;rect->bottom=720;}
    return result;
}
}

TEST_CASE("Owned scene requires a reduced resource generation and a published native image", "[owned_scene]") {
    rk::OwnedSceneDomain route;
    REQUIRE_FALSE(route.configure({{2560,1440},{2560,1440},1}));
    REQUIRE(route.configure({{1707,960},{2560,1440},1}));
    REQUIRE_FALSE(route.enterUi(1,1,true));
    REQUIRE(route.begin(1,1));
    REQUIRE_FALSE(route.begin(2,1));
    REQUIRE_FALSE(route.startProcessing(1,2));
    REQUIRE(route.startProcessing(1,1));
    REQUIRE_FALSE(route.enterUi(1,1,false));
    REQUIRE(route.enterUi(1,1,true));
    float width=1707, height=960;
    REQUIRE_FALSE(route.remapFullUiViewport(width,height,0,0,false));
    REQUIRE(route.remapFullUiViewport(width,height,0,0,true));
    REQUIRE(width==2560);
    REQUIRE(height==1440);
    width=1707;height=960;
    REQUIRE_FALSE(route.remapFullUiViewport(width,height,1,0,true));
    REQUIRE_FALSE(route.closePublishedFrame(2,1));
    REQUIRE(route.closePublishedFrame(1,1));
    REQUIRE(route.phase()==rk::ScenePhase::Dormant);
    REQUIRE_FALSE(route.closePublishedFrame(1,1));
    REQUIRE_FALSE(route.begin(1,1));
    REQUIRE(route.begin(2,1));
    REQUIRE(route.phase()==rk::ScenePhase::World);
    route.suspend();
    REQUIRE_FALSE(route.begin(3,1));
    REQUIRE(route.configure({{1280,720},{2560,1440},2}));
    REQUIRE_FALSE(route.begin(3,1));
    REQUIRE(route.begin(3,2));
}

TEST_CASE("Six-byte indirect call resolves the exact Skyrim cell and rejects a mid-instruction address", "[rip_call6]") {
    constexpr std::uintptr_t base=0x140000000;
    constexpr std::array<std::uint8_t,6> call{0xff,0x15,0x07,0xb2,0x90,0x00};
    const rk::RipCall6Descriptor d{"skyrim1170.renderer-client-rect-v1",
        "c434208894f07f604b852f29b8edc3a58c4de63de783373733e72b2b73f33be9",
        0x3870000,0xe4471b,0x174f928,call};
    const auto prepared=rk::prepareRipCall6(call,d.gameSha256,d.imageSize,d);
    REQUIRE(std::holds_alternative<rk::RipCall6Plan>(prepared));
    REQUIRE(std::get<std::uintptr_t>(rk::decodeRipCall6(base+d.siteRva,call))==base+d.originalCellRva);
    REQUIRE(std::get<std::array<std::uint8_t,6>>(rk::encodeRipCall6(base+d.siteRva,base+d.originalCellRva))==call);
    auto wrong=d;wrong.siteRva=0xe44722;
    REQUIRE(std::holds_alternative<rk::Error>(rk::prepareRipCall6(call,wrong.gameSha256,wrong.imageSize,wrong)));
    wrong=d;wrong.originalCellRva++;
    REQUIRE(std::holds_alternative<rk::Error>(rk::prepareRipCall6(call,wrong.gameSha256,wrong.imageSize,wrong)));
    REQUIRE(std::holds_alternative<rk::Error>(rk::prepareRipCall6(call,std::string(64,'0'),d.imageSize,d)));
    auto mutated=call;mutated[0]=0xe8;
    REQUIRE(std::holds_alternative<rk::Error>(rk::prepareRipCall6(mutated,d.gameSha256,d.imageSize,d)));
    REQUIRE(std::holds_alternative<rk::Error>(rk::decodeRipCall6(base+d.siteRva,mutated)));
}

TEST_CASE("Indirect-call displacement boundaries remain signed 32-bit", "[rip_call6]") {
    constexpr std::uintptr_t site=0x200000000;
    const auto next=site+6;
    REQUIRE(std::holds_alternative<std::array<std::uint8_t,6>>(rk::encodeRipCall6(site,next+0x7fffffff)));
    REQUIRE(std::holds_alternative<std::array<std::uint8_t,6>>(rk::encodeRipCall6(site,next-0x80000000)));
    REQUIRE(std::holds_alternative<rk::Error>(rk::encodeRipCall6(site,next+0x80000000)));
    REQUIRE(std::holds_alternative<rk::Error>(rk::encodeRipCall6(site,next-0x80000001)));
    REQUIRE(std::holds_alternative<rk::Error>(rk::encodeRipCall6(std::numeric_limits<std::uintptr_t>::max(),site)));
}

TEST_CASE("Renderer client rectangle ABI is the decoded HWND and stack RECT call", "[rip_call6]") {
    constexpr std::array<std::uint8_t,73> caller{
        0x48,0x8d,0x54,0x24,0x20,0x49,0x03,0xc6,0x48,0x83,0x78,0x70,0x00,
        0x49,0x0f,0x44,0xc6,0x48,0x83,0xc0,0x58,0x48,0x89,0x05,0x24,0x23,
        0x44,0x02,0x48,0x8b,0x40,0x30,0x49,0x89,0x86,0x68,0x0a,0x00,0x00,
        0x48,0x8b,0x05,0x12,0x23,0x44,0x02,0x48,0x8b,0x48,0x38,0x49,0x89,
        0x8e,0x70,0x0a,0x00,0x00,0x48,0x8b,0x0d,0x00,0x23,0x44,0x02,0x48,
        0x8b,0x09,0xff,0x15,0x07,0xb2,0x90,0x00};
    REQUIRE(std::get<bool>(rk::verifySkyrim1170ClientRectAbi(caller)));
    auto mutated=caller;mutated[0]=0x90;
    REQUIRE(std::holds_alternative<rk::Error>(rk::verifySkyrim1170ClientRectAbi(mutated)));
    mutated=caller;mutated[67]=0xe8;
    REQUIRE(std::holds_alternative<rk::Error>(rk::verifySkyrim1170ClientRectAbi(mutated)));
}

TEST_CASE("Six-byte indirect CALL fixture forwards HWND and RECT, then restores exactly", "[rip_call6][patch_execution]") {
    struct Page {
        void* address=VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
        ~Page(){if(address)VirtualFree(address,0,MEM_RELEASE);}
    } page;
    REQUIRE(page.address!=nullptr);
    const auto base=reinterpret_cast<std::uintptr_t>(page.address);
    // Win64 call frame, FF15 through the local +0x28 cell, frame restore, RET.
    constexpr std::array<std::uint8_t,15> code{
        0x48,0x83,0xec,0x28,0xff,0x15,0x1e,0,0,0,
        0x48,0x83,0xc4,0x28,0xc3};
    std::memcpy(page.address,code.data(),code.size());
    const auto original=reinterpret_cast<std::uintptr_t>(&fixtureGetClientRect);
    std::memcpy(reinterpret_cast<void*>(base+0x28),&original,sizeof(original));
    DWORD previous{};
    REQUIRE(VirtualProtect(page.address,4096,PAGE_EXECUTE_READ,&previous));
    REQUIRE(FlushInstructionCache(GetCurrentProcess(),page.address,code.size()));
    const rk::RipCall6Descriptor d{"fixture.client-rect",std::string(64,'a'),4096,4,0x28,
        {0xff,0x15,0x1e,0,0,0}};
    const auto prepared=rk::prepareRipCall6(std::span(code).subspan(4,6),d.gameSha256,d.imageSize,d);
    REQUIRE(std::holds_alternative<rk::RipCall6Plan>(prepared));
    const auto& plan=std::get<rk::RipCall6Plan>(prepared);
    auto cellResult=rk::prepareNearRipCall6Cell(plan,base,
        reinterpret_cast<std::uintptr_t>(&fixtureReducedRect));
    REQUIRE(std::holds_alternative<rk::NearRipCall6Cell>(cellResult));
    auto& cell=std::get<rk::NearRipCall6Cell>(cellResult);
    priorRectCall=reinterpret_cast<RectCall>(original);
    auto call=reinterpret_cast<RectCall>(page.address);
    RECT rect{};rectCalls=0;
    REQUIRE(call(reinterpret_cast<HWND>(0x1234),&rect));
    REQUIRE(rect.right==2560);
    REQUIRE(std::holds_alternative<rk::Error>(rk::applyRipCall6(plan,base,cell,rk::CallWriteBoundary::None)));
    REQUIRE(std::get<bool>(rk::applyRipCall6(plan,base,cell,rk::CallWriteBoundary::OwnedFixtureExclusive)));
    REQUIRE(std::get<bool>(rk::applyRipCall6(plan,base,cell,rk::CallWriteBoundary::OwnedFixtureExclusive)));
    REQUIRE(call(reinterpret_cast<HWND>(0x1234),&rect));
    REQUIRE(rect.right==1280);
    REQUIRE(rect.bottom==720);
    REQUIRE_FALSE(call(nullptr,&rect));
    REQUIRE(rectCalls==3);
    REQUIRE(std::get<bool>(rk::restoreRipCall6(plan,base,cell,rk::CallWriteBoundary::OwnedFixtureExclusive)));
    REQUIRE(call(reinterpret_cast<HWND>(0x1234),&rect));
    REQUIRE(rect.right==2560);
    REQUIRE(rectCalls==4);
}

TEST_CASE("Renderer logical rectangle changes only the matching world call", "[owned_scene]") {
    rk::OwnedSceneDomain route;
    REQUIRE(route.configure({{1280,720},{2560,1440},7}));
    const auto window=reinterpret_cast<HWND>(0x1234);
    rk::RendererLogicalSize body(route,&fixtureGetClientRect,window,GetCurrentThreadId());
    RECT rect{};rectCalls=0;
    REQUIRE(body.query(window,&rect));
    REQUIRE(rect.right==2560);
    REQUIRE(route.begin(1,7));
    REQUIRE(body.query(window,&rect));
    REQUIRE(rect.right==1280);
    REQUIRE(rect.bottom==720);
    REQUIRE_FALSE(body.query(nullptr,&rect));
    REQUIRE(rectCalls==3);
    REQUIRE(route.startProcessing(1,7));
    REQUIRE(body.query(window,&rect));
    REQUIRE(rect.right==2560);
    REQUIRE(route.enterUi(1,7,true));
    REQUIRE(body.query(window,&rect));
    REQUIRE(rect.right==2560);
}

TEST_CASE("Owned scene moves its render-thread lease between real frames", "[owned_scene]") {
    rk::OwnedSceneDomain route;
    REQUIRE(route.configure({{1707,960},{2560,1440},11}));
    const auto window=reinterpret_cast<HWND>(0x1234);
    rk::RendererLogicalSize body(route,&fixtureGetClientRect,window,GetCurrentThreadId());
    RECT workerRect{};
    bool begun{};
    std::uint32_t workerThread{};
    std::thread worker([&] {
        workerThread=GetCurrentThreadId();
        begun=route.begin(1,11,workerThread);
        body.query(window,&workerRect);
    });
    worker.join();
    REQUIRE(begun);
    REQUIRE(route.renderThread()==workerThread);
    REQUIRE(workerRect.right==1707);
    REQUIRE(workerRect.bottom==960);
    REQUIRE(route.startProcessing(1,11));
    REQUIRE(route.enterUi(1,11,true));
    REQUIRE(route.begin(2,11,GetCurrentThreadId()));
    REQUIRE(route.renderThread()==GetCurrentThreadId());
    RECT nextRect{};
    REQUIRE(body.query(window,&nextRect));
    REQUIRE(nextRect.right==1707);
}
