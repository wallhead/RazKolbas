#include "rk/FrameContracts.hpp"
namespace rk {
std::uint64_t HistoryEpoch::request() { return requested_.fetch_add(1) + 1; }
std::uint64_t HistoryEpoch::capture() const { return requested_.load(); }
void HistoryEpoch::consume(std::uint64_t captured, bool success) {
    if (!success || captured > requested_.load()) return;
    auto old = consumed_.load();
    while (old < captured && !consumed_.compare_exchange_weak(old, captured)) {}
}
bool HistoryEpoch::pending() const { return consumed_.load() < requested_.load(); }
std::uint64_t FrameIdentity::beginSource() { return ++source_; }
bool FrameIdentity::presentGenerated(std::uint64_t source) {
    if (source == 0 || source != source_) return false;
    ++generated_;
    return true;
}
}
