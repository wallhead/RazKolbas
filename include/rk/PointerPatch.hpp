#pragma once
#include "rk/Result.hpp"
namespace rk {
// Atomic, naturally aligned pointer-slot ownership. Caller supplies a verified,
// mapped slot and keeps the containing module + replacement code alive.
class PointerPatch {
public:
    PointerPatch() = default;
    ~PointerPatch();
    PointerPatch(const PointerPatch&) = delete;
    PointerPatch& operator=(const PointerPatch&) = delete;
    Result<bool> apply(void** slot, void* expected, void* replacement);
    Result<bool> restore();
private:
    void** slot_{};
    void* original_{};
    void* replacement_{};
};
}
