#include "rk/PatchDescriptor.hpp"
#include <Windows.h>
#include <bcrypt.h>
#include <Zydis/Zydis.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>
#include <limits>
namespace rk {
std::string sha256(std::span<const std::uint8_t> bytes) {
    if (bytes.size() > std::numeric_limits<ULONG>::max()) throw std::length_error("Hash input exceeds supported size");
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) throw std::runtime_error("SHA256 provider unavailable");
    std::array<std::uint8_t, 32> hash{};
    const auto status = BCryptHash(algorithm, nullptr, 0, const_cast<PUCHAR>(bytes.data()), static_cast<ULONG>(bytes.size()), hash.data(), static_cast<ULONG>(hash.size()));
    BCryptCloseAlgorithmProvider(algorithm, 0);
    if (status < 0) throw std::runtime_error("SHA256 failed");
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    for (const auto byte : hash) { result += digits[byte >> 4]; result += digits[byte & 15]; }
    return result;
}
namespace {
bool wholeInstructions(std::span<const std::uint8_t> code) {
    ZydisDecoder decoder;
    if (!ZYAN_SUCCESS(ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64))) return false;
    std::size_t offset = 0;
    while (offset < code.size()) {
        ZydisDecodedInstruction instruction;
        ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
        if (!ZYAN_SUCCESS(ZydisDecoderDecodeFull(&decoder, code.data()+offset, code.size()-offset, &instruction, operands))) return false;
        offset += instruction.length;
    }
    return offset == code.size();
}
Error rejected(std::string reason) { return {ErrorCode::Conflict, std::move(reason)}; }
}
Result<PatchPlan> preparePatch(std::span<const std::uint8_t> image, const PatchDescriptor& descriptor) {
    if (descriptor.id.empty() || descriptor.purpose.empty() || descriptor.imageHash != sha256(image)) return rejected("Missing descriptor or wrong image hash");
    if (descriptor.sectionOffset > image.size() || descriptor.sectionSize > image.size()-descriptor.sectionOffset) return rejected("Section outside image");
    if (descriptor.expected.empty() || descriptor.expected.size() != descriptor.replacement.size() || descriptor.expected.size() > descriptor.sectionSize) return rejected("Invalid patch byte extents");
    if (!wholeInstructions(descriptor.expected) || !wholeInstructions(descriptor.replacement)) return rejected("Truncated or invalid instruction sequence");
    const auto section = image.subspan(descriptor.sectionOffset, descriptor.sectionSize);
    std::size_t count = 0, offset = 0;
    for (std::size_t at=0; at <= section.size()-descriptor.expected.size(); ++at) {
        if (std::equal(descriptor.expected.begin(), descriptor.expected.end(), section.begin()+at)) { ++count; offset = descriptor.sectionOffset+at; }
    }
    if (count != 1) return rejected("Expected bytes must have exactly one section-bounded match");
    if (!wholeInstructions(section.first(offset-descriptor.sectionOffset))) return rejected("Match begins inside an instruction");
    return PatchPlan{descriptor, offset};
}
OwnedCode::OwnedCode(std::span<const std::uint8_t> bytes) : size_(bytes.size()), original_(bytes.begin(), bytes.end()) {
    if (bytes.empty() || !wholeInstructions(bytes)) throw std::invalid_argument("Invalid owned code");
    memory_ = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, size_, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (!memory_) throw std::runtime_error("Code allocation failed");
    std::memcpy(memory_, bytes.data(), size_);
    DWORD old;
    if (!VirtualProtect(memory_, size_, PAGE_EXECUTE_READ, &old) || !FlushInstructionCache(GetCurrentProcess(), memory_, size_)) {
        VirtualFree(memory_, 0, MEM_RELEASE); memory_ = nullptr;
        throw std::runtime_error("Cannot finalize executable code");
    }
}
OwnedCode::~OwnedCode() { if (memory_) VirtualFree(memory_, 0, MEM_RELEASE); }
int OwnedCode::invoke() { std::scoped_lock lock(gate_); return reinterpret_cast<int(*)()>(memory_)(); }
Result<bool> OwnedCode::write(std::span<const std::uint8_t> bytes) {
    DWORD old;
    if (!VirtualProtect(memory_, size_, PAGE_READWRITE, &old)) return rejected("Code protection change failed; no write performed");
    std::memcpy(memory_, bytes.data(), size_);
    DWORD ignored;
    // Never release the execution gate with an unverified executable page.
    if (!VirtualProtect(memory_, size_, old, &ignored) || !FlushInstructionCache(GetCurrentProcess(), memory_, size_) ||
        !std::equal(bytes.begin(), bytes.end(), memory_)) std::terminate();
    return true;
}
Result<bool> OwnedCode::apply(const PatchPlan& plan) {
    std::scoped_lock lock(gate_);
    const auto verified = preparePatch(original_, plan.descriptor);
    if (const auto error = std::get_if<Error>(&verified)) return *error;
    if (std::get<PatchPlan>(verified).offset != plan.offset) return rejected("Forged patch offset");
    auto next = original_;
    std::copy(plan.descriptor.replacement.begin(), plan.descriptor.replacement.end(), next.begin()+plan.offset);
    if (!installed_.empty()) {
        if (installed_ == next && std::equal(installed_.begin(), installed_.end(), memory_)) return true;
        return rejected("Existing installed owner differs");
    }
    if (!std::equal(original_.begin(), original_.end(), memory_)) return rejected("Original bytes changed; zero writes");
    const auto result = write(next);
    if (std::holds_alternative<bool>(result)) installed_ = std::move(next);
    return result;
}
Result<bool> OwnedCode::restore() {
    std::scoped_lock lock(gate_);
    if (installed_.empty()) return true;
    if (!std::equal(installed_.begin(), installed_.end(), memory_)) return rejected("Patch ownership lost; refusing rollback");
    const auto result = write(original_);
    if (std::holds_alternative<bool>(result)) installed_.clear();
    return result;
}
}
