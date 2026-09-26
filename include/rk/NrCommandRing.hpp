#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace rk {
// Fence-only policy for the three D3D12 evaluation command contexts. A slot
// can be reset only after its previous queue submission has retired.
class NrCommandRing final {
public:
    static constexpr std::size_t size=3;

    std::optional<std::size_t> acquire(std::uint64_t completedFence) noexcept {
        for(std::size_t offset=0;offset<size;++offset) {
            const auto index=(next_+offset)%size;
            if(retireFence_[index]<=completedFence) {
                next_=(index+1)%size;
                return index;
            }
        }
        return std::nullopt;
    }

    void markSubmitted(std::size_t index,std::uint64_t outputFence) noexcept {
        retireFence_[index]=outputFence;
    }

private:
    std::array<std::uint64_t,size> retireFence_{};
    std::size_t next_{};
};
}
