#include "rk/Logging.hpp"
#include <Windows.h>
#include <ShlObj.h>
#include <filesystem>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

namespace rk {
bool initializeLogging() {
    PWSTR documents = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, nullptr, &documents))) return false;
    const std::filesystem::path base(documents);
    CoTaskMemFree(documents);
    const auto directory = base / "My Games" / "Skyrim Special Edition" / "SKSE";
    std::filesystem::create_directories(directory);
    auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>((directory / "RazKolbas.log").string(), false);
    spdlog::set_default_logger(std::make_shared<spdlog::logger>("RazKolbas", sink));
    spdlog::set_pattern("[%Y-%m-%d %T] [%l] %v");
    spdlog::flush_on(spdlog::level::info);
    return true;
}
}
