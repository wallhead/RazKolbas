#include <catch2/catch_test_macros.hpp>
#include "rk/PatchDescriptor.hpp"
#include "rk/PointerPatch.hpp"
#include <thread>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <Windows.h>

TEST_CASE("File SHA-256 streams large inputs without changing their identity",
    "[patch][file_hash]") {
    std::vector<std::uint8_t> bytes(131077);
    for(std::size_t i=0;i<bytes.size();++i)
        bytes[i]=static_cast<std::uint8_t>((i*37u)&0xffu);
    const auto path=std::filesystem::temp_directory_path()/
        ("rk-file-hash-"+std::to_string(GetCurrentProcessId())+"-"+
            std::to_string(GetTickCount64())+".bin");
    {
        std::ofstream output(path,std::ios::binary);
        REQUIRE(output.good());
        output.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
        REQUIRE(output.good());
    }
    const auto hashed=rk::sha256File(path);
    std::filesystem::remove(path);
    REQUIRE(std::holds_alternative<std::string>(hashed));
    REQUIRE(std::get<std::string>(hashed)==rk::sha256(bytes));
    REQUIRE(std::holds_alternative<rk::Error>(rk::sha256File(path)));
}

TEST_CASE("Pointer rollback refuses a later owner and does not overwrite it", "[patch]") {
    int original{}, replacement{}, later{};
    void* slot = &original;
    rk::PointerPatch patch;
    REQUIRE(std::get<bool>(patch.apply(&slot, &original, &replacement)));
    REQUIRE(slot == &replacement);
    InterlockedExchangePointer(&slot, &later);
    REQUIRE(std::holds_alternative<rk::Error>(patch.restore()));
    REQUIRE(slot == &later);
}
TEST_CASE("Pointer patch requires matching original and restores exact owner", "[patch]") {
    int original{}, replacement{}, unrelated{};
    void* slot = &original;
    rk::PointerPatch patch;
    REQUIRE(std::holds_alternative<rk::Error>(patch.apply(&slot, &unrelated, &replacement)));
    REQUIRE(slot == &original);
    REQUIRE(std::get<bool>(patch.apply(&slot, &original, &replacement)));
    REQUIRE(slot == &replacement);
    REQUIRE(std::get<bool>(patch.restore()));
    REQUIRE(slot == &original);
}
TEST_CASE("Failed pointer restore retains its lease for a later successful retry", "[patch]") {
    int original{}, replacement{};
    auto slot = static_cast<void**>(VirtualAlloc(nullptr, 4096, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE));
    REQUIRE(slot != nullptr);
    *slot = &original;
    rk::PointerPatch patch;
    REQUIRE(std::get<bool>(patch.apply(slot, &original, &replacement)));
    DWORD prior;
    REQUIRE(VirtualProtect(slot,4096,PAGE_NOACCESS,&prior));
    const auto failed = patch.restore();
    DWORD ignored;
    REQUIRE(VirtualProtect(slot,4096,prior,&ignored));
    REQUIRE(std::holds_alternative<rk::Error>(failed));
    const auto retry = patch.restore();
    const auto restoredValue = *slot;
    VirtualFree(slot,0,MEM_RELEASE);
    REQUIRE(std::get<bool>(retry));
    REQUIRE(restoredValue == &original);
}

namespace {
const std::vector<std::uint8_t> one{0xb8,1,0,0,0,0xc3};
const std::vector<std::uint8_t> two{0xb8,2,0,0,0,0xc3};
rk::PatchDescriptor descriptor(const std::vector<std::uint8_t>& image = one) {
    return {"fixture.return-two", "Owned function returns two", rk::sha256(image), 0, image.size(), one, two};
}
}
TEST_CASE("Hash, ambiguous signature and truncated instruction reject before writes", "[patch]") {
    auto desc = descriptor();
    desc.imageHash = std::string(64, '0');
    REQUIRE(std::holds_alternative<rk::Error>(rk::preparePatch(one, desc)));
    auto duplicate = one;
    duplicate.insert(duplicate.end(), one.begin(), one.end());
    REQUIRE(std::holds_alternative<rk::Error>(rk::preparePatch(duplicate, descriptor(duplicate))));
    desc = descriptor();
    desc.expected.resize(3); desc.replacement.resize(3);
    REQUIRE(std::holds_alternative<rk::Error>(rk::preparePatch(one, desc)));
    desc = descriptor();
    desc.sectionOffset = 100;
    REQUIRE(std::holds_alternative<rk::Error>(rk::preparePatch(one, desc)));
}
TEST_CASE("Byte pattern inside another instruction is not a patch boundary", "[patch]") {
    const std::vector<std::uint8_t> embedded{0x48,0xb8,0xb8,1,0,0,0,0xc3,0,0,0xc3};
    REQUIRE(std::holds_alternative<rk::Error>(rk::preparePatch(embedded, descriptor(embedded))));
}
TEST_CASE("Executable fixture patches one to two and restores one", "[patch][patch_execution]") {
    const auto result = rk::preparePatch(one, descriptor());
    REQUIRE(std::holds_alternative<rk::PatchPlan>(result));
    rk::OwnedCode code(one);
    REQUIRE(code.invoke() == 1);
    REQUIRE(std::get<bool>(code.apply(std::get<rk::PatchPlan>(result))));
    REQUIRE(code.invoke() == 2);
    REQUIRE(std::get<bool>(code.restore()));
    REQUIRE(code.invoke() == 1);
    REQUIRE(std::get<bool>(code.restore()));
}
TEST_CASE("Controlled concurrent callers never execute partially patched instructions", "[patch][patch_execution]") {
    const auto result = rk::preparePatch(one, descriptor());
    REQUIRE(std::holds_alternative<rk::PatchPlan>(result));
    rk::OwnedCode code(one);
    std::atomic<bool> done{}, bad{};
    std::thread caller([&] { while (!done.load()) { const auto value = code.invoke(); if (value != 1 && value != 2) bad = true; } });
    for (int i=0; i<100; ++i) {
        if (!std::holds_alternative<bool>(code.apply(std::get<rk::PatchPlan>(result)))) bad = true;
        if (!std::holds_alternative<bool>(code.restore())) bad = true;
    }
    done = true;
    caller.join();
    REQUIRE_FALSE(bad.load());
    REQUIRE(code.invoke() == 1);
}
