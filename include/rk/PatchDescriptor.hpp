#pragma once
#include "rk/Result.hpp"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>
#include <mutex>

namespace rk {
std::string sha256(std::span<const std::uint8_t> bytes);
Result<std::string> sha256File(const std::filesystem::path& path);
struct PatchDescriptor {
    std::string id;
    std::string purpose;
    std::string imageHash;
    // Start must be a verified instruction boundary (owned function in this
    // milestone), not an arbitrary byte offset within a mixed code/data section.
    std::size_t sectionOffset{}, sectionSize{};
    std::vector<std::uint8_t> expected, replacement;
};
struct PatchPlan { PatchDescriptor descriptor; std::size_t offset{}; };
Result<PatchPlan> preparePatch(std::span<const std::uint8_t> image, const PatchDescriptor& descriptor);

// Only an exclusively owned synthetic/code-generated function. All execution
// enters through invoke(), allowing this mutex to prove quiescence. This is NOT
// an engine-thread suspension mechanism or an arbitrary game hook installer.
class OwnedCode {
public:
    explicit OwnedCode(std::span<const std::uint8_t> bytes);
    ~OwnedCode();
    OwnedCode(const OwnedCode&) = delete;
    OwnedCode& operator=(const OwnedCode&) = delete;
    int invoke();
    Result<bool> apply(const PatchPlan& plan);
    Result<bool> restore();
private:
    std::mutex gate_;
    std::uint8_t* memory_{};
    std::size_t size_{};
    std::vector<std::uint8_t> original_, installed_;
    Result<bool> write(std::span<const std::uint8_t> bytes);
};
}
