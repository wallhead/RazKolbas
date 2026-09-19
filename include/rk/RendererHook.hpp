#pragma once
#include "rk/Result.hpp"
#include "rk/Settings.hpp"
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <string>

namespace rk {
inline constexpr std::string_view rendererObserverPatchId = "skyrim1170.device-create.observe-v1";
bool rendererObserverRequested(const Settings& settings);
struct CreationImportProfile {
    std::string_view gameSha256;
    std::size_t fileSize;
    std::uint32_t imageSize;
    std::uint32_t iatRva;
};
struct CreationOwnerProfile {
    std::string_view name, sha256;
    std::size_t fileSize;
    std::uint32_t imageSize, exportRva;
    std::array<std::uint8_t, 16> prologue;
};
const CreationImportProfile& skyrim1170CreationProfile();
const CreationOwnerProfile* creationOwnerProfile(std::string_view hash);
Result<std::uint32_t> validateCreationImport(std::span<const std::uint8_t> mapped,
    std::string_view fileHash, std::size_t fileSize, const CreationImportProfile& profile);
using CreateD3D11 = decltype(&D3D11CreateDeviceAndSwapChain);
struct DeviceCreationArgs {
    IDXGIAdapter* adapter;
    D3D_DRIVER_TYPE driverType;
    HMODULE software;
    UINT flags;
    const D3D_FEATURE_LEVEL* levels;
    UINT levelCount, sdkVersion;
    const DXGI_SWAP_CHAIN_DESC* swapDesc;
    IDXGISwapChain** swapChain;
    ID3D11Device** device;
    D3D_FEATURE_LEVEL* featureLevel;
    ID3D11DeviceContext** context;
};
using CreationObserver = void(*)(const DeviceCreationArgs&, HRESULT);
// Exact original call once; observer errors never change original outputs,
// HRESULT or GetLastError. No ownership of returned COM objects is acquired here.
HRESULT observeDeviceCreation(CreateD3D11 original, const DeviceCreationArgs& args,
    CreationObserver observer) noexcept;
struct RendererSnapshot {
    std::string adapter;
    std::uint32_t vendorId{}, deviceId{}, luidLow{};
    std::int32_t luidHigh{};
    D3D_FEATURE_LEVEL featureLevel{};
    UINT deviceFlags{}, width{}, height{}, bufferCount{}, sampleCount{};
    DXGI_FORMAT format{};
    DXGI_SWAP_EFFECT swapEffect{};
    bool windowed{};
};
Result<RendererSnapshot> captureRendererSnapshot(const DeviceCreationArgs& args, HRESULT result);
}
