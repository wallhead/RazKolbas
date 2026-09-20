#include <catch2/catch_test_macros.hpp>
#include "rk/CallSite.hpp"
#include "rk/PatchDescriptor.hpp"
#include <Windows.h>
#include <array>
#include <cstring>
#include <limits>
#include <vector>

namespace {
int relayVisits{};
int relayTarget(int first,int second) {
    ++relayVisits;
    return first*3+second;
}
constexpr std::array<std::uint8_t,5> worldCall{0xe8,0xd1,0xf7,0xe9,0xff};
rk::CallSiteDescriptor worldDescriptor() {
    return {"skyrim1170.world-draw.sr-v1",
            "c434208894f07f604b852f29b8edc3a58c4de63de783373733e72b2b73f33be9",
            0x3870000,0xfa507a,0xe44850,worldCall};
}
}

TEST_CASE("Verified Skyrim world call resolves to the original engine target", "[patch][call_site]") {
    const auto d=worldDescriptor();
    const auto result=rk::prepareCallSite(worldCall,d.gameSha256,d.imageSize,d);
    REQUIRE(std::holds_alternative<rk::CallSitePlan>(result));
    const auto& plan=std::get<rk::CallSitePlan>(result);
    REQUIRE(plan.siteRva==0xfa507a);
    REQUIRE(plan.originalTargetRva==0xe44850);
}

TEST_CASE("Decoded Skyrim world-call ABI requires exact caller and callee context", "[patch][call_site]") {
    constexpr std::array<std::uint8_t,17> caller{
        0x33,0xd2,0x48,0x8d,0x0d,0x46,0x37,0x2e,0x02,
        0xe8,0xd1,0xf7,0xe9,0xff,0x48,0x8b,0x05};
    constexpr std::array<std::uint8_t,33> callee{
        0x4c,0x8b,0xdc,0x53,0x48,0x81,0xec,0x80,0,0,0,
        0x44,0x8b,0x05,0x0e,0x63,0x1e,0x01,0x49,0x89,0x6b,0x08,
        0x48,0x8b,0xe9,0x4d,0x89,0x7b,0xe8,0x44,0x0f,0xb6,0xfa};
    REQUIRE(std::get<bool>(rk::verifySkyrim1170WorldCallAbi(caller,callee)));
    auto changedCaller=caller;
    changedCaller[0]=0x90;
    REQUIRE(std::holds_alternative<rk::Error>(rk::verifySkyrim1170WorldCallAbi(changedCaller,callee)));
    changedCaller=caller;
    changedCaller[9]=0xe9;
    REQUIRE(std::holds_alternative<rk::Error>(rk::verifySkyrim1170WorldCallAbi(changedCaller,callee)));
    auto changedCallee=callee;
    changedCallee[32]=0xd1;
    REQUIRE(std::holds_alternative<rk::Error>(rk::verifySkyrim1170WorldCallAbi(caller,changedCallee)));
    REQUIRE(std::holds_alternative<rk::Error>(rk::verifySkyrim1170WorldCallAbi(
        std::span(caller).first(16),callee)));
}

TEST_CASE("Changed world call, identity, and expected target all reject before a code write", "[patch][call_site]") {
    auto d=worldDescriptor();
    auto changed=worldCall;
    changed[4]^=1;
    REQUIRE(std::holds_alternative<rk::Error>(rk::prepareCallSite(changed,d.gameSha256,d.imageSize,d)));
    REQUIRE(std::holds_alternative<rk::Error>(rk::prepareCallSite(worldCall,std::string(64,'0'),d.imageSize,d)));
    d.originalTargetRva++;
    REQUIRE(std::holds_alternative<rk::Error>(rk::prepareCallSite(worldCall,d.gameSha256,d.imageSize,d)));
}

TEST_CASE("Call-site planner rejects truncated, non-CALL, and out-of-image sites", "[patch][call_site]") {
    auto d=worldDescriptor();
    REQUIRE(std::holds_alternative<rk::Error>(rk::prepareCallSite(
        std::span<const std::uint8_t>(worldCall.data(),4),d.gameSha256,d.imageSize,d)));
    d.expected[0]=0xe9;
    REQUIRE(std::holds_alternative<rk::Error>(rk::prepareCallSite(worldCall,d.gameSha256,d.imageSize,d)));
    d=worldDescriptor();
    d.siteRva=static_cast<std::uint32_t>(d.imageSize-4);
    REQUIRE(std::holds_alternative<rk::Error>(rk::prepareCallSite(worldCall,d.gameSha256,d.imageSize,d)));
}

TEST_CASE("Owned fixture CALL can forward through a detour once and restore", "[patch][call_site][patch_execution]") {
    // Entry calls an original function returning 1. The detour counts one
    // invocation, then tail-jumps to that same original function.
    std::uint32_t visits{};
    std::vector<std::uint8_t> code{
        0xe8,1,0,0,0, 0xc3, 0xb8,1,0,0,0,0xc3,
        0x48,0xb8, 0,0,0,0,0,0,0,0, 0xff,0x00,
        0xe9,0xe9,0xff,0xff,0xff
    };
    const auto counter=reinterpret_cast<std::uintptr_t>(&visits);
    static_assert(sizeof(counter)==8);
    std::memcpy(code.data()+14,&counter,sizeof(counter));
    const rk::CallSiteDescriptor d{"fixture.forward-call",rk::sha256(code),code.size(),0,6,{0xe8,1,0,0,0}};
    const auto prepared=rk::prepareCallSite(code,d.gameSha256,code.size(),d);
    REQUIRE(std::holds_alternative<rk::CallSitePlan>(prepared));
    const auto replacement=rk::encodeCallSiteReplacement(std::get<rk::CallSitePlan>(prepared),0,12);
    REQUIRE(std::holds_alternative<std::array<std::uint8_t,5>>(replacement));
    const auto patched=std::get<std::array<std::uint8_t,5>>(replacement);
    REQUIRE(patched==std::array<std::uint8_t,5>{0xe8,7,0,0,0});
    const rk::PatchDescriptor patch{"fixture.forward-call","Pass through owned fixture",rk::sha256(code),
        0,code.size(),{code.begin(),code.begin()+5},{patched.begin(),patched.end()}};
    const auto patchPlan=rk::preparePatch(code,patch);
    REQUIRE(std::holds_alternative<rk::PatchPlan>(patchPlan));
    rk::OwnedCode owned(code);
    REQUIRE(owned.invoke()==1);
    REQUIRE(visits==0);
    REQUIRE(std::get<bool>(owned.apply(std::get<rk::PatchPlan>(patchPlan))));
    REQUIRE(owned.invoke()==1);
    REQUIRE(owned.invoke()==1);
    REQUIRE(visits==2);
    REQUIRE(std::get<bool>(owned.restore()));
    REQUIRE(owned.invoke()==1);
    REQUIRE(visits==2);
}

TEST_CASE("CALL replacement rejects unreachable targets and a forged original", "[patch][call_site]") {
    const auto d=worldDescriptor();
    const auto prepared=rk::prepareCallSite(worldCall,d.gameSha256,d.imageSize,d);
    REQUIRE(std::holds_alternative<rk::CallSitePlan>(prepared));
    auto plan=std::get<rk::CallSitePlan>(prepared);
    constexpr std::uintptr_t base=0x100000000ULL;
    REQUIRE(std::holds_alternative<rk::Error>(rk::encodeCallSiteReplacement(plan,base,base+0x90000000ULL)));
    REQUIRE(std::holds_alternative<rk::Error>(rk::encodeCallSiteReplacement(plan,
        std::numeric_limits<std::uintptr_t>::max()-2,base)));
    plan.originalTargetRva++;
    REQUIRE(std::holds_alternative<rk::Error>(rk::encodeCallSiteReplacement(plan,base,base+0xe44850)));
}

TEST_CASE("CALL encoder accepts both signed displacement limits", "[patch][call_site]") {
    const rk::CallSitePlan plan{"fixture.range",0,6,{0xe8,1,0,0,0}};
    constexpr std::uintptr_t base=0x200000000ULL;
    constexpr auto next=base+5;
    const auto forward=rk::encodeCallSiteReplacement(plan,base,
        next+static_cast<std::uintptr_t>(std::numeric_limits<std::int32_t>::max()));
    const auto backward=rk::encodeCallSiteReplacement(plan,base,next-0x80000000ULL);
    REQUIRE(std::get<std::array<std::uint8_t,5>>(forward)==
        std::array<std::uint8_t,5>{0xe8,0xff,0xff,0xff,0x7f});
    REQUIRE(std::get<std::array<std::uint8_t,5>>(backward)==
        std::array<std::uint8_t,5>{0xe8,0,0,0,0x80});
    REQUIRE(std::holds_alternative<rk::Error>(rk::encodeCallSiteReplacement(plan,base,next+0x80000000ULL)));
    REQUIRE(std::holds_alternative<rk::Error>(rk::encodeCallSiteReplacement(plan,base,next-0x80000001ULL)));
}

TEST_CASE("Absolute relay preserves argument registers and return address", "[patch][call_site][patch_execution]") {
    const auto encoded=rk::encodeRegisterPreservingJump(reinterpret_cast<std::uintptr_t>(&relayTarget));
    REQUIRE(std::holds_alternative<std::array<std::uint8_t,14>>(encoded));
    const auto bytes=std::get<std::array<std::uint8_t,14>>(encoded);
    REQUIRE(bytes[0]==0xff);
    REQUIRE(bytes[1]==0x25);
    struct Page {
        void* address=VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
        ~Page() { if(address)VirtualFree(address,0,MEM_RELEASE); }
    } page;
    REQUIRE(page.address!=nullptr);
    std::memcpy(page.address,bytes.data(),bytes.size());
    DWORD prior{};
    REQUIRE(VirtualProtect(page.address,4096,PAGE_EXECUTE_READ,&prior));
    REQUIRE(FlushInstructionCache(GetCurrentProcess(),page.address,bytes.size()));
    relayVisits=0;
    const auto relay=reinterpret_cast<int(*)(int,int)>(page.address);
    REQUIRE(relay(7,11)==32);
    REQUIRE(relay(2,5)==11);
    REQUIRE(relayVisits==2);
    REQUIRE(std::holds_alternative<rk::Error>(rk::encodeRegisterPreservingJump(0)));
}

TEST_CASE("Near relay allocation reaches an owned CALL and retires after restore", "[patch][call_site][patch_execution]") {
    // Reserve the Win64 shadow space and align RSP before invoking a C++
    // callee. The original callee is a small integer-add assembly fixture.
    const std::array<std::uint8_t,18> code{
        0x48,0x83,0xec,0x28, 0xe8,5,0,0,0,
        0x48,0x83,0xc4,0x28, 0xc3, 0x8d,0x04,0x11,0xc3};
    struct Page {
        void* address=VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
        ~Page() { if(address)VirtualFree(address,0,MEM_RELEASE); }
    } page;
    REQUIRE(page.address!=nullptr);
    std::memcpy(page.address,code.data(),code.size());
    const rk::CallSiteDescriptor descriptor{"fixture.near-relay",rk::sha256(code),code.size(),4,14,
        {0xe8,5,0,0,0}};
    const auto prepared=rk::prepareCallSite(std::span(code).subspan(4),descriptor.gameSha256,code.size(),descriptor);
    REQUIRE(std::holds_alternative<rk::CallSitePlan>(prepared));
    auto relayResult=rk::prepareNearCallRelay(std::get<rk::CallSitePlan>(prepared),
        reinterpret_cast<std::uintptr_t>(page.address),reinterpret_cast<std::uintptr_t>(&relayTarget));
    REQUIRE(std::holds_alternative<rk::NearCallRelay>(relayResult));
    auto& relay=std::get<rk::NearCallRelay>(relayResult);
    REQUIRE(relay.entry()!=nullptr);
    MEMORY_BASIC_INFORMATION information{};
    REQUIRE(VirtualQuery(relay.entry(),&information,sizeof(information))==sizeof(information));
    REQUIRE(information.Protect==PAGE_EXECUTE_READ);
    const auto jump=rk::encodeRegisterPreservingJump(reinterpret_cast<std::uintptr_t>(&relayTarget));
    REQUIRE(std::memcmp(relay.entry(),std::get<std::array<std::uint8_t,14>>(jump).data(),14)==0);
    DWORD old{};
    REQUIRE(VirtualProtect(page.address,4096,PAGE_EXECUTE_READ,&old));
    REQUIRE(FlushInstructionCache(GetCurrentProcess(),page.address,code.size()));
    const auto call=reinterpret_cast<int(*)(int,int)>(page.address);
    relayVisits=0;
    REQUIRE(call(7,11)==18);
    REQUIRE(relayVisits==0);
    REQUIRE(VirtualProtect(page.address,4096,PAGE_READWRITE,&old));
    std::memcpy(static_cast<std::uint8_t*>(page.address)+4,relay.callBytes().data(),5);
    REQUIRE(VirtualProtect(page.address,4096,PAGE_EXECUTE_READ,&old));
    REQUIRE(FlushInstructionCache(GetCurrentProcess(),static_cast<std::uint8_t*>(page.address)+4,5));
    std::int32_t relative{};
    std::memcpy(&relative,static_cast<std::uint8_t*>(page.address)+5,4);
    REQUIRE(static_cast<std::uint8_t*>(page.address)+9+relative==relay.entry());
    REQUIRE(call(7,11)==32);
    REQUIRE(relayVisits==1);
    REQUIRE(VirtualProtect(page.address,4096,PAGE_READWRITE,&old));
    std::memcpy(static_cast<std::uint8_t*>(page.address)+4,code.data()+4,5);
    REQUIRE(VirtualProtect(page.address,4096,PAGE_EXECUTE_READ,&old));
    REQUIRE(FlushInstructionCache(GetCurrentProcess(),static_cast<std::uint8_t*>(page.address)+4,5));
    REQUIRE(call(7,11)==18);
    REQUIRE(relayVisits==1);
}

TEST_CASE("Near relay rejects invalid ownership before allocation", "[patch][call_site]") {
    auto plan=rk::CallSitePlan{"fixture.invalid",0,6,{0xe8,1,0,0,0}};
    plan.originalTargetRva=7;
    REQUIRE(std::holds_alternative<rk::Error>(rk::prepareNearCallRelay(plan,0x100000000ULL,
        reinterpret_cast<std::uintptr_t>(&relayTarget))));
    plan.originalTargetRva=6;
    REQUIRE(std::holds_alternative<rk::Error>(rk::prepareNearCallRelay(plan,0,
        reinterpret_cast<std::uintptr_t>(&relayTarget))));
    REQUIRE(std::holds_alternative<rk::Error>(rk::prepareNearCallRelay(plan,0x100000000ULL,0)));
}

TEST_CASE("Quiescent CALL write installs, refuses another owner, and restores exact bytes", "[patch][call_site][patch_execution]") {
    const std::array<std::uint8_t,18> code{
        0x48,0x83,0xec,0x28, 0xe8,5,0,0,0,
        0x48,0x83,0xc4,0x28, 0xc3, 0x8d,0x04,0x11,0xc3};
    struct Page {
        void* address=VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
        ~Page() { if(address)VirtualFree(address,0,MEM_RELEASE); }
    } page;
    REQUIRE(page.address);
    std::memcpy(page.address,code.data(),code.size());
    DWORD prior{};
    REQUIRE(VirtualProtect(page.address,4096,PAGE_EXECUTE_READ,&prior));
    REQUIRE(FlushInstructionCache(GetCurrentProcess(),page.address,code.size()));
    const rk::CallSiteDescriptor descriptor{"fixture.quiescent-call",rk::sha256(code),code.size(),4,14,
        {0xe8,5,0,0,0}};
    const auto prepared=rk::prepareCallSite(std::span(code).subspan(4),descriptor.gameSha256,code.size(),descriptor);
    REQUIRE(std::holds_alternative<rk::CallSitePlan>(prepared));
    const auto plan=std::get<rk::CallSitePlan>(prepared);
    const auto imageBase=reinterpret_cast<std::uintptr_t>(page.address);
    auto relayResult=rk::prepareNearCallRelay(plan,imageBase,reinterpret_cast<std::uintptr_t>(&relayTarget));
    REQUIRE(std::holds_alternative<rk::NearCallRelay>(relayResult));
    auto& relay=std::get<rk::NearCallRelay>(relayResult);
    using Boundary=rk::CallWriteBoundary;
    const auto call=reinterpret_cast<int(*)(int,int)>(page.address);
    relayVisits=0;
    REQUIRE(call(7,11)==18);
    REQUIRE(std::get<bool>(rk::applyCallInstruction(plan,imageBase,relay,Boundary::OwnedFixtureExclusive)));
    REQUIRE(call(7,11)==32);
    REQUIRE(relayVisits==1);
    REQUIRE(std::get<bool>(rk::applyCallInstruction(plan,imageBase,relay,Boundary::OwnedFixtureExclusive)));
    REQUIRE(std::holds_alternative<rk::Error>(rk::restoreCallInstruction(
        plan,imageBase,relay,Boundary::None)));
    REQUIRE(std::get<bool>(rk::restoreCallInstruction(plan,imageBase,relay,Boundary::OwnedFixtureExclusive)));
    REQUIRE(call(7,11)==18);
    REQUIRE(std::get<bool>(rk::restoreCallInstruction(plan,imageBase,relay,Boundary::OwnedFixtureExclusive)));
    REQUIRE(relayVisits==1);
    REQUIRE(std::get<bool>(rk::applyCallInstruction(plan,imageBase,relay,Boundary::OwnedFixtureExclusive)));
    REQUIRE(VirtualProtect(page.address,4096,PAGE_READWRITE,&prior));
    static_cast<std::uint8_t*>(page.address)[4]=0x90;
    REQUIRE(VirtualProtect(page.address,4096,PAGE_EXECUTE_READ,&prior));
    REQUIRE(std::holds_alternative<rk::Error>(rk::restoreCallInstruction(
        plan,imageBase,relay,Boundary::OwnedFixtureExclusive)));
    REQUIRE(static_cast<const std::uint8_t*>(page.address)[4]==0x90);
    REQUIRE(VirtualProtect(page.address,4096,PAGE_READWRITE,&prior));
    static_cast<std::uint8_t*>(page.address)[4]=relay.callBytes()[0];
    REQUIRE(VirtualProtect(page.address,4096,PAGE_EXECUTE_READ,&prior));
    REQUIRE(FlushInstructionCache(GetCurrentProcess(),page.address,code.size()));
    REQUIRE(std::get<bool>(rk::restoreCallInstruction(plan,imageBase,relay,Boundary::OwnedFixtureExclusive)));
    // A changed opcode is another owner. Both installation and rollback refuse it.
    REQUIRE(VirtualProtect(page.address,4096,PAGE_READWRITE,&prior));
    static_cast<std::uint8_t*>(page.address)[4]=0x90;
    REQUIRE(VirtualProtect(page.address,4096,PAGE_EXECUTE_READ,&prior));
    REQUIRE(std::holds_alternative<rk::Error>(rk::applyCallInstruction(
        plan,imageBase,relay,Boundary::OwnedFixtureExclusive)));
    REQUIRE(std::holds_alternative<rk::Error>(rk::restoreCallInstruction(
        plan,imageBase,relay,Boundary::OwnedFixtureExclusive)));
    REQUIRE(static_cast<const std::uint8_t*>(page.address)[4]==0x90);
    REQUIRE(VirtualProtect(page.address,4096,PAGE_READWRITE,&prior));
    static_cast<std::uint8_t*>(page.address)[4]=0xe8;
    REQUIRE(VirtualProtect(page.address,4096,PAGE_EXECUTE_READ,&prior));
    REQUIRE(FlushInstructionCache(GetCurrentProcess(),page.address,code.size()));
}
