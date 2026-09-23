#include "rk/EnbTargetProbe.hpp"
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <limits>

namespace rk {
bool EnbTargetProbeBudget::admit(std::uint64_t frame) noexcept {
    if(windows_&&frame<windowStart_)return false;
    if(!windows_||frame-windowStart_>=600) {
        if(windows_>=64)return false;
        ++windows_;
        windowStart_=frame;
        attempts_=0;
    }
    if(attempts_>=8)return false;
    ++attempts_;
    return true;
}
bool validateEnbTargetProbeImage(std::span<const std::uint8_t> mapped,
    std::string_view verifiedHash) noexcept {
    constexpr std::array<std::uint8_t,27> comparison{0x3b,0x2d,0x3f,0x23,0x1e,0x00,
        0x75,0x48,0x44,0x3b,0x25,0x3a,0x23,0x1e,0x00,0x75,0x38,
        0x83,0xfb,0x0a,0x74,0x05,0x83,0xfb,0x1a,0x75,0x3c};
    return verifiedHash=="47ff220dd26a44520d4cec2d515d89effe87b632c1885c32388c93e8d0ceda58"&&
        mapped.size()==0xaae000&&
        std::equal(comparison.begin(),comparison.end(),mapped.begin()+0x691cb);
}
std::optional<Extent> readEnbTargetProbeExtent(std::uintptr_t verifiedBase) noexcept {
    if(!verifiedBase||verifiedBase>std::numeric_limits<std::uintptr_t>::max()-0x24b518)
        return std::nullopt;
    std::array<std::uint32_t,2> dimensions{};
    SIZE_T copied{};
    if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(verifiedBase+0x24b510),
        dimensions.data(),sizeof(dimensions),&copied)||copied!=sizeof(dimensions))
        return std::nullopt;
    return Extent{dimensions[0],dimensions[1]};
}
EnbTargetSample inspectEnbTargets(ID3D11DeviceContext* context,
    ID3D11RenderTargetView* input) noexcept {
    using Microsoft::WRL::ComPtr;
    EnbTargetSample sample;
    if(input) {
        ComPtr<ID3D11Resource> resource;
        input->GetResource(&resource);
        ComPtr<ID3D11Texture2D> texture;
        if(resource&&SUCCEEDED(resource.As(&texture))) {
            D3D11_TEXTURE2D_DESC desc{};
            texture->GetDesc(&desc);
            sample.actual={desc.Width,desc.Height};
            sample.format=static_cast<std::uint32_t>(desc.Format);
        }
        // Exact ENB 47ff... metadata copied by its CreateRenderTargetView wrapper.
        constexpr GUID guid{0xb272d61a,0xacbe,0x4117,{0x8e,0xde,0xe2,0x4c,0x4e,0xe8,0x87,0x21}};
        std::array<std::uint32_t,12> record{};
        UINT size=sizeof(record);
        if(SUCCEEDED(input->GetPrivateData(guid,&size,record.data()))&&
            size==sizeof(record)&&record[0]==2) {
            sample.metadataValid=true;
            sample.metadata={record[1],record[2]};
            sample.metadataFormat=record[4];
        }
    }
    if(context) {
        std::array<ID3D11RenderTargetView*,8> bound{};
        context->OMGetRenderTargets(static_cast<UINT>(bound.size()),bound.data(),nullptr);
        for(std::size_t i=0;i<bound.size();++i) {
            if(bound[i]) {
                sample.boundMask|=1u<<i;
                bound[i]->Release();
            }
        }
    }
    return sample;
}
}
