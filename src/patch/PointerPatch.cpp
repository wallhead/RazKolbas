#include "rk/PointerPatch.hpp"
#include <Windows.h>
#include <cstdint>
namespace rk {
namespace {
Result<bool> exchange(void** slot, void* expected, void* replacement) {
    const auto address = reinterpret_cast<std::uintptr_t>(slot);
    if (!slot || address % alignof(void*) != 0) return Error{ErrorCode::InvalidInput,"Unaligned pointer slot"};
    MEMORY_BASIC_INFORMATION memory{};
    if (!VirtualQuery(slot, &memory, sizeof(memory)) || memory.State != MEM_COMMIT ||
        memory.Protect & (PAGE_GUARD | PAGE_NOACCESS) || memory.RegionSize < sizeof(void*) ||
        address - reinterpret_cast<std::uintptr_t>(memory.BaseAddress) > memory.RegionSize-sizeof(void*))
        return Error{ErrorCode::InvalidInput,"Pointer slot is not in accessible mapped memory"};
    DWORD old;
    if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old)) return Error{ErrorCode::Io,"Pointer protection change failed"};
    const auto prior = InterlockedCompareExchangePointer(slot, replacement, expected);
    DWORD ignored;
    if (!VirtualProtect(slot, sizeof(void*), old, &ignored)) std::terminate();
    if (prior != expected) return Error{ErrorCode::Conflict,"Pointer owner changed; no overwrite performed"};
    return true;
}
}
PointerPatch::~PointerPatch() { if (slot_) (void)restore(); }
Result<bool> PointerPatch::apply(void** slot, void* expected, void* replacement) {
    if (slot_) return Error{ErrorCode::Conflict,"Patch lease already owns a slot"};
    if (!expected || !replacement) return Error{ErrorCode::InvalidInput,"Null pointer target"};
    const auto result = exchange(slot, expected, replacement);
    if (std::holds_alternative<bool>(result)) { slot_ = slot; original_ = expected; replacement_ = replacement; }
    return result;
}
Result<bool> PointerPatch::restore() {
    if (!slot_) return true;
    const auto result = exchange(slot_, replacement_, original_);
    // Retain no memory/code ownership: those lifetimes belong to the caller.
    // A later owner's pointer is deliberately left untouched.
    slot_ = nullptr;
    return result;
}
}
