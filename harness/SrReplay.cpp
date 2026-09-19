// Offline single-frame DLAA replay through NVIDIA's existing DLSS runtime.
// No reference host DLLs, game hooks, temporal-quality or performance claims.
#include "rk/PatchDescriptor.hpp"
#include <Windows.h>
#include <d3d11.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <nvsdk_ngx_helpers_d3d.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include <cstring>
#include <exception>
#include <stdexcept>

using Microsoft::WRL::ComPtr;
namespace fs = std::filesystem;
namespace {
constexpr UINT width = 2560, height = 1440;
constexpr char runtimeHash[] = "c85f971ce023c9f3492fc7455f0b01a24ba18ea39636407a846902c4360b0b7e";
[[noreturn]] void stop(const char* reason) {
    std::cerr << "SR_REPLAY_STOP=" << reason << std::endl;
    ExitProcess(10); // Child process teardown owns any uncertain GPU/NGX state.
}
void hr(HRESULT result, const char* name) { if (FAILED(result)) stop(name); }
void ngx(NVSDK_NGX_Result result, const char* name) {
    std::cout << name << "=0x" << std::hex << result << std::dec << std::endl;
    if (result != NVSDK_NGX_Result_Success) stop(name);
}
std::vector<std::uint8_t> read(const fs::path& path, std::size_t expected = 0) {
    const auto size = fs::file_size(path);
    if ((expected && size != expected) || size > 100*1024*1024) stop("INPUT_SIZE");
    std::ifstream stream(path, std::ios::binary);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    if (!stream.read(reinterpret_cast<char*>(bytes.data()), bytes.size())) stop("INPUT_READ");
    return bytes;
}
ComPtr<ID3D11Texture2D> texture(ID3D11Device* device, DXGI_FORMAT format,
                              UINT rowBytes, const void* data) {
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width; desc.Height = height; desc.MipLevels = desc.ArraySize = 1;
    desc.Format = format; desc.SampleDesc.Count = 1;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    D3D11_SUBRESOURCE_DATA initial{data, rowBytes, 0};
    ComPtr<ID3D11Texture2D> result;
    hr(device->CreateTexture2D(&desc, data ? &initial : nullptr, &result), "TEXTURE_CREATE");
    return result;
}
void waitGpu(ID3D11Device* device, ID3D11DeviceContext* context) {
    const D3D11_QUERY_DESC desc{D3D11_QUERY_EVENT, 0};
    ComPtr<ID3D11Query> event;
    hr(device->CreateQuery(&desc, &event), "EVENT_CREATE");
    context->End(event.Get()); context->Flush();
    const auto start = GetTickCount64();
    for (;;) {
        const auto status = context->GetData(event.Get(), nullptr, 0, D3D11_ASYNC_GETDATA_DONOTFLUSH);
        if (status == S_OK) break;
        if (FAILED(status) || GetTickCount64()-start > 20000) stop("GPU_WAIT");
        Sleep(1);
    }
    hr(device->GetDeviceRemovedReason(), "DEVICE_REMOVED");
}
}

int wmain(int argc, wchar_t** argv) {
    try {
        if (argc != 4) { std::cerr << "Usage: RazKolbasSrReplay <runtime-directory> <verified-2560x1440-capture> <output-directory>\n"; return 2; }
        const auto runtimeDir = fs::absolute(argv[1]);
        const auto capture = fs::absolute(argv[2]);
        const auto outputDir = fs::absolute(argv[3]);
        // Lock the exact supplied runtime against replacement until NGX shutdown.
        const auto runtimePath = runtimeDir / L"nvngx_dlss.dll";
        const auto fileLock = CreateFileW(runtimePath.c_str(), GENERIC_READ, FILE_SHARE_READ,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (fileLock == INVALID_HANDLE_VALUE || rk::sha256(read(runtimePath)) != runtimeHash) stop("RUNTIME_IDENTITY");
        auto colorBytes = read(capture/L"main-colour-candidate.raw", width*height*8);
        auto motionBytes = read(capture/L"motion-candidate.raw", width*height*4);
        auto depthBytes = read(capture/L"depth-candidate.raw", width*height*4);
        std::vector<float> depth(width*height);
        for (std::size_t i=0; i<depth.size(); ++i) {
            std::uint32_t packed;
            std::memcpy(&packed, depthBytes.data()+i*4, 4);
            depth[i] = static_cast<float>(packed & 0xffffffU) / 16777215.0f;
        }
        fs::create_directories(outputDir);
        ComPtr<IDXGIFactory6> factory;
        hr(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)), "FACTORY_CREATE");
        ComPtr<IDXGIAdapter1> adapter;
        DXGI_ADAPTER_DESC1 adapterDesc{};
        for (UINT i=0;; ++i) {
            ComPtr<IDXGIAdapter1> candidate;
            const auto result = factory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&candidate));
            if (result == DXGI_ERROR_NOT_FOUND) break;
            hr(result, "ADAPTER_ENUM");
            hr(candidate->GetDesc1(&adapterDesc), "ADAPTER_DESC");
            if (adapterDesc.VendorId == 0x10de && !(adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) { adapter = candidate; break; }
        }
        if (!adapter) stop("NVIDIA_ADAPTER_MISSING");
        std::wcout << L"Replay adapter: " << adapterDesc.Description << std::endl;
        ComPtr<ID3D11Device> device;
        ComPtr<ID3D11DeviceContext> context;
        ComPtr<ID3D11Texture2D> color, motion, z, output, staging;
        // Declare after every persistent COM owner: an unexpected C++ exception
        // must terminate this isolated child before any GPU owners unwind.
        struct FatalUnwind {
            int initial = std::uncaught_exceptions();
            ~FatalUnwind() { if (std::uncaught_exceptions() > initial) stop("EXCEPTION_WITH_GPU_OWNERS"); }
        } fatalUnwind;
        hr(D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0,
            nullptr, 0, D3D11_SDK_VERSION, &device, nullptr, &context), "DEVICE_CREATE");
        const wchar_t* searchPath = runtimeDir.c_str();
        NVSDK_NGX_FeatureCommonInfo info{};
        info.PathListInfo = {&searchPath, 1};
        ngx(NVSDK_NGX_D3D11_Init_with_ProjectID("b3340e44-a57e-4b98-9318-d7150829d110",
            NVSDK_NGX_ENGINE_TYPE_CUSTOM, "RazKolbas-SR-Replay-1", outputDir.c_str(), device.Get(), &info), "INIT");
        NVSDK_NGX_Parameter* parameters = nullptr;
        ngx(NVSDK_NGX_D3D11_GetCapabilityParameters(&parameters), "CAPABILITY_PARAMETERS");
        if (!parameters) stop("PARAMETERS_NULL");
        int available = 0;
        ngx(parameters->Get(NVSDK_NGX_Parameter_SuperSampling_Available, &available), "SR_CAPABILITY_QUERY");
        if (!available) stop("SR_UNAVAILABLE");
        NVSDK_NGX_DLSS_Create_Params create{};
        create.Feature.InWidth = create.Feature.InTargetWidth = width;
        create.Feature.InHeight = create.Feature.InTargetHeight = height;
        create.Feature.InPerfQualityValue = NVSDK_NGX_PerfQuality_Value_DLAA;
        create.InFeatureCreateFlags = NVSDK_NGX_DLSS_Feature_Flags_IsHDR |
            NVSDK_NGX_DLSS_Feature_Flags_MVLowRes | NVSDK_NGX_DLSS_Feature_Flags_AutoExposure;
        NVSDK_NGX_Handle* feature = nullptr;
        ngx(NGX_D3D11_CREATE_DLSS_EXT(context.Get(), &feature, parameters, &create), "CREATE_DLAA");
        if (!feature) stop("FEATURE_NULL");
        wchar_t loadedPath[32768]{};
        const auto loaded = GetModuleHandleW(L"nvngx_dlss.dll");
        const auto length = GetModuleFileNameW(loaded, loadedPath, 32768);
        if (!loaded || !length || length >= 32768 ||
            rk::sha256(read(loadedPath)) != runtimeHash || !fs::equivalent(loadedPath, runtimePath)) stop("LOADED_RUNTIME_IDENTITY");
        std::wcout << L"Verified loaded supplied runtime: " << loadedPath << std::endl;
        color = texture(device.Get(), DXGI_FORMAT_R16G16B16A16_FLOAT, width*8, colorBytes.data());
        motion = texture(device.Get(), DXGI_FORMAT_R16G16_FLOAT, width*4, motionBytes.data());
        z = texture(device.Get(), DXGI_FORMAT_R32_FLOAT, width*4, depth.data());
        std::vector<std::uint16_t> sentinel(width*height*4, 0x7e00); // NaNs detect unwritten output.
        output = texture(device.Get(), DXGI_FORMAT_R16G16B16A16_FLOAT, width*8, sentinel.data());
        NVSDK_NGX_D3D11_DLSS_Eval_Params eval{};
        eval.Feature.pInColor = color.Get(); eval.Feature.pInOutput = output.Get();
        eval.pInDepth = z.Get(); eval.pInMotionVectors = motion.Get();
        eval.InRenderSubrectDimensions = {width, height};
        // Reset-only replay: camera jitter is unavailable in old captures. These
        // settings are explicit experiment inputs, not inferred live contracts.
        eval.InReset = 1; eval.InJitterOffsetX = eval.InJitterOffsetY = 0;
        eval.InMVScaleX = static_cast<float>(width); eval.InMVScaleY = static_cast<float>(height);
        eval.InPreExposure = eval.InExposureScale = 1.0f;
        ngx(NGX_D3D11_EVALUATE_DLSS_EXT(context.Get(), feature, parameters, &eval), "EVALUATE_DLAA");
        wchar_t fault[32]{};
        GetEnvironmentVariableW(L"RAZKOLBAS_SR_REPLAY_FAULT", fault, 32);
        if (std::wstring_view(fault) == L"throw_after_evaluate")
            throw std::runtime_error("INJECTED_AFTER_EVALUATE");
        D3D11_TEXTURE2D_DESC desc{}; output->GetDesc(&desc);
        desc.Usage = D3D11_USAGE_STAGING; desc.BindFlags = 0; desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        hr(device->CreateTexture2D(&desc, nullptr, &staging), "STAGING_CREATE");
        context->CopyResource(staging.Get(), output.Get());
        waitGpu(device.Get(), context.Get());
        D3D11_MAPPED_SUBRESOURCE mapped{};
        hr(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped), "READBACK_MAP");
        std::vector<std::uint8_t> pixels(width*height*8);
        for (UINT y=0; y<height; ++y)
            std::memcpy(pixels.data()+y*width*8, static_cast<const char*>(mapped.pData)+y*mapped.RowPitch, width*8);
        context->Unmap(staging.Get(), 0);
        // RGB must have been overwritten with finite values. Alpha is not an SR
        // quality assertion and is reported separately by the Python validator.
        std::size_t finite = 0;
        for (std::size_t i=0; i<width*height; ++i) for (unsigned c=0; c<3; ++c) {
            std::uint16_t half; std::memcpy(&half, pixels.data()+i*8+c*2, 2);
            if ((half & 0x7c00U) != 0x7c00U) ++finite;
        }
        if (finite != width*height*3) stop("OUTPUT_RGB_NONFINITE_OR_UNWRITTEN");
        std::ofstream file(outputDir/L"dlss-output-rgba16f.raw", std::ios::binary);
        if (!file.write(reinterpret_cast<const char*>(pixels.data()), pixels.size())) stop("OUTPUT_WRITE");
        file.close();
        if (!file) stop("OUTPUT_CLOSE");
        std::cout << "OUTPUT_SHA256=" << rk::sha256(pixels) << "; RGB_FINITE=" << finite << std::endl;
        ngx(NVSDK_NGX_D3D11_ReleaseFeature(feature), "RELEASE_FEATURE");
        context->ClearState(); waitGpu(device.Get(), context.Get());
        ngx(NVSDK_NGX_D3D11_DestroyParameters(parameters), "DESTROY_PARAMETERS");
        ngx(NVSDK_NGX_D3D11_Shutdown1(device.Get()), "SHUTDOWN");
        CloseHandle(fileLock);
        std::cout << "SINGLE_FRAME_DLAA_REPLAY=PASS; LIVE_SKYRIM_EVALUATION=NOT_RUN; TEMPORAL_QUALITY=NOT_TESTED\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << std::endl; stop("EXCEPTION"); }
}
