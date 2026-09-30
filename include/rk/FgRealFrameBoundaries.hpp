#pragma once
#include <cstdint>
#include <mutex>

namespace rk {
enum class FgBoundaryKind { Ready, NoWorld, MultipleWorlds, ThreadMismatch,
    Test, ForeignSwap };

struct FgBoundarySample {
    FgBoundaryKind kind{};
    std::uint64_t world{},epoch{},realPresent{};
    std::uint64_t worldThread{},presentThread{};
    std::uint64_t ready{},noWorld{},multipleWorlds{},threadMismatch{};
};

// Read-only pairing of the completed world callback and the next real
// Present attempt. It does not establish a simulation phase or submit FG.
class FgRealFrameBoundaries {
public:
    void world(std::uint64_t thread);
    FgBoundarySample present(std::uint64_t thread,bool test,bool ownSwap);
    void reset();
private:
    std::mutex mutex_;
    std::uint64_t world_{},consumedWorld_{},worldThread_{},epoch_{1};
    std::uint64_t realPresent_{},ready_{},noWorld_{},multipleWorlds_{},
        threadMismatch_{};
};
}
