#include <catch2/catch_test_macros.hpp>
#include "rk/Settings.hpp"
#include "rk/RendererHook.hpp"
#include "rk/SwapObserver.hpp"
#include <limits>
#include <fstream>
#include <Windows.h>

TEST_CASE("Renderer observer patch can be selectively disabled", "[config]") {
    const auto parsed = rk::parseIni("[Patching]\nExperimentalPatches=true\nDisabledPatchIds=skyrim1170.device-create.observe-v1\n");
    REQUIRE(std::holds_alternative<rk::Settings>(parsed));
    auto settings = std::get<rk::Settings>(parsed);
    REQUIRE_FALSE(rk::rendererObserverRequested(settings));
    settings.values["Patching.DisabledPatchIds"] = rk::Text{};
    REQUIRE(rk::rendererObserverRequested(settings));
    settings.values["General.SafeMode"] = true;
    REQUIRE_FALSE(rk::rendererObserverRequested(settings));
    REQUIRE(std::holds_alternative<rk::Error>(rk::parseIni("[Patching]\nDisabledPatchIds=unknown.patch\n")));
    const auto both=rk::parseIni("[Patching]\nExperimentalPatches=true\nDisabledPatchIds=reshade673.swapchain-observe-v1, skyrim1170.device-create.observe-v1\n");
    REQUIRE(std::holds_alternative<rk::Settings>(both));
    REQUIRE_FALSE(rk::rendererObserverRequested(std::get<rk::Settings>(both)));
}

TEST_CASE("Supplied INI parses through the same schema as defaults", "[config]") {
    std::ifstream input(RK_SOURCE_DIR "/config/RazKolbas.ini.example");
    REQUIRE(input.good());
    const std::string text((std::istreambuf_iterator<char>(input)), {});
    const auto parsed = rk::parseIni(text);
    REQUIRE(std::holds_alternative<rk::Settings>(parsed));
    REQUIRE(std::get<rk::Settings>(parsed).values == rk::defaultSettings().values);
}

TEST_CASE("Schema defaults preserve user intent and unknown INI data", "[config]") {
    auto parsed = rk::parseIni("; keep this\n[Upscaling]\nProvider = DLSS\n[Future]\nSecretSauce = 42\n");
    REQUIRE(std::holds_alternative<rk::Settings>(parsed));
    const auto settings = std::get<rk::Settings>(parsed);
    REQUIRE(settings.get<rk::Choice>("Upscaling.Provider").value == "DLSS");
    REQUIRE_FALSE(settings.get<bool>("FrameGeneration.Enabled"));
    REQUIRE_FALSE(settings.get<bool>("NeuralRendering.Enabled"));
    REQUIRE_FALSE(settings.get<bool>("Diagnostics.SpatialBaselineOnly"));
    const auto baseline=rk::parseIni(
        "[Diagnostics]\nSpatialBaselineOnly=true\n");
    REQUIRE(std::holds_alternative<rk::Settings>(baseline));
    REQUIRE(std::get<rk::Settings>(baseline).get<bool>(
        "Diagnostics.SpatialBaselineOnly"));
    REQUIRE(rk::classifyChange(rk::defaultSettings(),
        std::get<rk::Settings>(baseline))==rk::ChangeCategory::RestartRequired);
    const auto text = rk::serializeIni(settings);
    REQUIRE(std::holds_alternative<std::string>(text));
    REQUIRE(std::get<std::string>(text).find("SecretSauce = 42") != std::string::npos);
    REQUIRE(std::get<std::string>(text).find("; keep this") != std::string::npos);
    REQUIRE(std::get<rk::Settings>(rk::parseIni(std::get<std::string>(text))).values == settings.values);
}
TEST_CASE("Invalid input rejects whole snapshot rather than publishing partial values", "[config]") {
    for (const auto input : {"[Upscaling]\nProvider=Magic\n", "[Upscaling]\nSharpness=nan\n", "[Upscaling]\nSharpness=inf\n", "[Upscaling]\nSharpness=0.5garbage\n", "[Upscaling]\nManualMipBias=-3.01\n", "[Upscaling]\nManualMipBias=3.01\n", "[Interface]\nFontScale=0\n", "[General]\nEnabled=true\nEnabled=false\n"})
        REQUIRE(std::holds_alternative<rk::Error>(rk::parseIni(input)));
    REQUIRE(std::holds_alternative<rk::Settings>(
        rk::parseIni("[Upscaling]\nManualMipBias=-3\n")));
    REQUIRE(std::holds_alternative<rk::Settings>(
        rk::parseIni("[Upscaling]\nManualMipBias=3\n")));
    auto settings = rk::defaultSettings();
    settings.values["Upscaling.Sharpness"] = std::numeric_limits<double>::quiet_NaN();
    REQUIRE(std::holds_alternative<rk::Error>(rk::validateSettings(settings)));
}
TEST_CASE("DLSS menu choices accept only implemented quality and current model presets", "[config]") {
    for(const auto preset:{"Auto","J","K","L","M"}) {
        const auto parsed=rk::parseIni(std::string("[Upscaling]\nModelPreset=")+preset+"\n");
        REQUIRE(std::holds_alternative<rk::Settings>(parsed));
        REQUIRE(std::get<rk::Settings>(parsed).get<rk::Choice>(
            "Upscaling.ModelPreset").value==preset);
    }
    REQUIRE(std::holds_alternative<rk::Error>(
        rk::parseIni("[Upscaling]\nModelPreset=A\n")));
    REQUIRE(std::holds_alternative<rk::Error>(
        rk::parseIni("[Upscaling]\nQuality=UltraQuality\n")));
    auto live=rk::defaultSettings();
    live.values["Upscaling.Sharpening"]=false;
    live.values["Upscaling.Sharpness"]=0.75;
    REQUIRE(rk::classifyChange(rk::defaultSettings(),live)==
        rk::ChangeCategory::Live);
    live.values["Upscaling.Quality"]=rk::Choice{"Balanced"};
    REQUIRE(rk::classifyChange(rk::defaultSettings(),live)==
        rk::ChangeCategory::Recreate);
}
TEST_CASE("Neural rendering settings match the before-SR community runtime contract", "[config][nr]") {
    const auto parsed=rk::parseIni(
        "[NeuralRendering]\n"
        "Position=BeforeSR\n"
        "Preset=Shipping\n"
        "Style=7\n"
        "PassCount=3\n"
        "InputResolutionScale=0\n"
        "InputColorIsHDR=true\n");
    REQUIRE(std::holds_alternative<rk::Settings>(parsed));
    const auto& settings=std::get<rk::Settings>(parsed);
    REQUIRE(settings.get<rk::Choice>("NeuralRendering.Position").value=="BeforeSR");
    REQUIRE(settings.get<rk::Choice>("NeuralRendering.Preset").value=="Shipping");
    REQUIRE(settings.get<std::int64_t>("NeuralRendering.Style")==7);
    REQUIRE(settings.get<std::int64_t>("NeuralRendering.PassCount")==3);
    REQUIRE(settings.get<double>("NeuralRendering.InputResolutionScale")==0.0);
    REQUIRE(settings.get<bool>("NeuralRendering.InputColorIsHDR"));

    for(const auto* invalid:{
        "[NeuralRendering]\nPosition=AfterSR\n",
        "[NeuralRendering]\nPreset=Unused2\n",
        "[NeuralRendering]\nStyle=8\n",
        "[NeuralRendering]\nPassCount=4\n",
        "[NeuralRendering]\nInputResolutionScale=0.1\n"})
        REQUIRE(std::holds_alternative<rk::Error>(rk::parseIni(invalid)));
    REQUIRE(std::holds_alternative<rk::Settings>(
        rk::parseIni("[NeuralRendering]\nInputResolutionScale=0.25\n")));
}
TEST_CASE("Failed replacement preserves last-good state; restart remains pending", "[config]") {
    rk::SettingsTransaction transaction;
    auto before = transaction.snapshot();
    auto next = before->requested;
    next.values["Upscaling.Provider"] = rk::Choice{"DLSS"};
    REQUIRE(std::holds_alternative<rk::Error>(transaction.apply(next, [](const auto&) -> rk::Result<bool> { return false; })));
    REQUIRE(transaction.snapshot() == before);
    REQUIRE(std::get<rk::ChangeCategory>(transaction.apply(next, [](const auto&) -> rk::Result<bool> { return true; })) == rk::ChangeCategory::Recreate);
    REQUIRE(transaction.snapshot()->generation == 1);
    next.values["General.Presentation"] = rk::Choice{"ProxyD3D12"};
    bool prepared = false;
    REQUIRE(std::get<rk::ChangeCategory>(transaction.apply(next, [&](const auto&) -> rk::Result<bool> { prepared = true; return true; })) == rk::ChangeCategory::RestartRequired);
    REQUIRE_FALSE(prepared);
    REQUIRE(transaction.snapshot()->generation == 1);
    REQUIRE(transaction.pendingRestart()->get<rk::Choice>("General.Presentation").value == "ProxyD3D12");
}
TEST_CASE("Locked destination leaves last-good INI bytes intact", "[config]") {
    const auto directory = std::filesystem::temp_directory_path() / ("rk-config-test-" + std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directories(directory);
    const auto path = directory / "settings.ini";
    { std::ofstream out(path); out << "original"; }
    const auto handle = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    REQUIRE(handle != INVALID_HANDLE_VALUE);
    const auto saved = rk::saveIni(path, rk::defaultSettings());
    CloseHandle(handle);
    REQUIRE(std::holds_alternative<rk::Error>(saved));
    std::ifstream input(path);
    std::string bytes((std::istreambuf_iterator<char>(input)), {});
    input.close();
    REQUIRE(bytes == "original");
    REQUIRE(std::get<bool>(rk::saveIni(path, rk::defaultSettings())));
    std::filesystem::remove_all(directory);
}
TEST_CASE("Partially failed replacement recovers original INI from a durable backup", "[config]") {
    const auto directory = std::filesystem::temp_directory_path() / ("rk-partial-save-" + std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directories(directory);
    for (const auto code : {1176U,1177U}) {
        const auto path = directory / (std::to_string(code)+".ini");
        { std::ofstream out(path); out << "last-good-original"; }
        bool invoked = false;
        const auto result = rk::saveIni(path, rk::defaultSettings(), [&](const auto& destination, const auto&) {
            invoked = true;
            // Model the documented partial replacement transition: the original
            // destination name no longer exists, and the new temp still does.
            std::filesystem::rename(destination, directory/(std::to_string(code)+".displaced"));
            return code;
        });
        REQUIRE(invoked);
        REQUIRE(std::holds_alternative<rk::Error>(result));
        std::ifstream input(path);
        const std::string bytes((std::istreambuf_iterator<char>(input)), {});
        REQUIRE(bytes == "last-good-original");
    }
    std::filesystem::remove_all(directory);
}
