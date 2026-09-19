#include "rk/Settings.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <set>
#include <sstream>
#include <Windows.h>

namespace rk {
namespace {
std::string trim(std::string_view value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == value.npos) return {};
    return std::string(value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1));
}
Error invalid(const std::string& key) { return {ErrorCode::InvalidInput, "Invalid setting: " + key}; }
template<class T> bool number(std::string_view text, T& value) {
    const auto [end, error] = std::from_chars(text.data(), text.data()+text.size(), value);
    return error == std::errc{} && end == text.data()+text.size();
}
std::string format(const SettingValue& value) {
    return std::visit([](const auto& item) -> std::string {
        using T = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<T, Choice> || std::is_same_v<T, Text>) return item.value;
        else if constexpr (std::is_same_v<T, bool>) return item ? "true" : "false";
        else { char buffer[64]; const auto result = std::to_chars(buffer, buffer+64, item); return {buffer, result.ptr}; }
    }, value);
}
}
Settings defaultSettings() {
    Settings settings;
    for (const auto& field : settingsSchema()) settings.values.emplace(field.key, field.initial);
    return settings;
}
Result<bool> validateSettings(const Settings& settings) {
    if (settings.values.size() != settingsSchema().size()) return invalid("schema shape");
    for (const auto& field : settingsSchema()) {
        const auto found = settings.values.find(field.key);
        if (found == settings.values.end() || found->second.index() != field.initial.index()) return invalid(field.key);
        bool valid = std::visit([&](const auto& item) {
            using T = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<T, bool>) return true;
            else if constexpr (std::is_same_v<T, Choice>) return std::find(field.choices.begin(), field.choices.end(), item.value) != field.choices.end();
            else if constexpr (std::is_same_v<T, Text>) {
                if (item.value.size() > 256 || item.value.find_first_of("\r\n\0", 0, 3) != std::string::npos) return false;
                if (field.key == "NeuralRendering.SkinStructureStrength" && item.value != "Auto") {
                    double numeric{};
                    return number(item.value, numeric) && std::isfinite(numeric) && numeric >= 0 && numeric <= 2;
                }
                // No patch IDs are registered in this milestone.
                if (field.key == "Patching.DisabledPatchIds" && !item.value.empty()) return false;
                return true;
            } else { const auto numeric = static_cast<double>(item); return std::isfinite(numeric) && numeric >= field.minimum && numeric <= field.maximum; }
        }, found->second);
        if (!valid) return invalid(field.key);
    }
    const auto scale = settings.get<double>("Upscaling.ManualRenderScale");
    if (scale != 0 && scale < 0.125) return invalid("Upscaling.ManualRenderScale");
    if (settings.get<bool>("Adapter.AllowCrossAdapter")) return Error{ErrorCode::Unsupported, "Cross-adapter processing is not implemented"};
    return true;
}
Result<Settings> parseIni(std::string_view text) {
    Settings result = defaultSettings();
    std::istringstream input{std::string(text)};
    std::string line, section;
    std::set<std::string> seen;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        result.originalLines.push_back(line);
        const auto stripped = trim(line);
        if (stripped.empty() || stripped[0] == ';' || stripped[0] == '#') continue;
        if (stripped[0] == '[') {
            if (stripped.back() != ']') return invalid("section");
            section = trim(std::string_view(stripped).substr(1, stripped.size()-2));
            continue;
        }
        const auto separator = stripped.find('=');
        if (separator == stripped.npos || section.empty()) return invalid("INI line");
        const auto key = section + '.' + trim(std::string_view(stripped).substr(0, separator));
        auto field = result.values.find(key);
        if (field == result.values.end()) continue; // Preserve unknown lines verbatim.
        if (!seen.insert(key).second) return invalid(key + " (duplicate)");
        const auto token = trim(std::string_view(stripped).substr(separator+1));
        const bool parsed = std::visit([&](auto& value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, bool>) {
                if (token != "true" && token != "false") return false;
                value = token == "true"; return true;
            } else if constexpr (std::is_same_v<T, Choice> || std::is_same_v<T, Text>) { value.value = token; return true; }
            else return number(token, value);
        }, field->second);
        if (!parsed) return invalid(key);
    }
    const auto validation = validateSettings(result);
    if (const auto error = std::get_if<Error>(&validation)) return *error;
    return result;
}
Result<std::string> serializeIni(const Settings& settings) {
    const auto validation = validateSettings(settings);
    if (const auto error = std::get_if<Error>(&validation)) return *error;
    std::ostringstream out;
    std::set<std::string> written;
    std::string section;
    for (const auto& line : settings.originalLines) {
        const auto stripped = trim(line);
        if (!stripped.empty() && stripped.front() == '[' && stripped.back() == ']') section = trim(std::string_view(stripped).substr(1, stripped.size()-2));
        const auto separator = stripped.find('=');
        const auto key = section + '.' + (separator == stripped.npos ? "" : trim(std::string_view(stripped).substr(0, separator)));
        const auto found = settings.values.find(key);
        if (separator != stripped.npos && !stripped.empty() && stripped[0] != ';' && stripped[0] != '#' && found != settings.values.end()) {
            if (!written.insert(key).second) return invalid(key + " (duplicate original line)");
            out << key.substr(key.find('.')+1) << " = " << format(found->second) << '\n';
        } else out << line << '\n';
    }
    section.clear();
    for (const auto& field : settingsSchema()) {
        if (written.contains(field.key)) continue;
        const auto split = field.key.find('.');
        const auto target = field.key.substr(0, split);
        if (target != section) { section = target; out << "\n[" << section << "]\n"; }
        out << field.key.substr(split+1) << " = " << format(settings.values.at(field.key)) << '\n';
    }
    return out.str();
}
Result<bool> saveIni(const std::filesystem::path& path, const Settings& settings, const FileReplace& replace) {
    auto serialized = serializeIni(settings);
    if (const auto error = std::get_if<Error>(&serialized)) return *error;
    const auto& bytes = std::get<std::string>(serialized);
    const auto full = std::filesystem::absolute(path);
    wchar_t temporary[MAX_PATH];
    if (!GetTempFileNameW(full.parent_path().c_str(), L"rki", 0, temporary)) return Error{ErrorCode::Io, "Cannot create temporary INI: " + std::to_string(GetLastError())};
    const auto file = CreateFileW(temporary, GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    DWORD written = 0;
    const bool writtenOk = file != INVALID_HANDLE_VALUE && bytes.size() <= MAXDWORD && WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) && written == bytes.size() && FlushFileBuffers(file);
    auto error = GetLastError();
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    if (!writtenOk) { DeleteFileW(temporary); return Error{ErrorCode::Io,"Temporary INI write failed: " + std::to_string(error)}; }
    const auto backup = std::filesystem::path(std::wstring(temporary)+L".previous");
    const bool existed = GetFileAttributesW(full.c_str()) != INVALID_FILE_ATTRIBUTES;
    if (existed) {
        // Keep a separate, flushed last-good copy before asking ReplaceFileW to
        // rename anything. Errors 1176/1177 may leave the destination absent.
        if (!CopyFileW(full.c_str(), backup.c_str(), TRUE)) {
            error = GetLastError(); DeleteFileW(temporary);
            return Error{ErrorCode::Io,"Cannot preserve last-good INI: " + std::to_string(error)};
        }
        const auto previous = CreateFileW(backup.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        const bool durable = previous != INVALID_HANDLE_VALUE && FlushFileBuffers(previous);
        error = GetLastError();
        if (previous != INVALID_HANDLE_VALUE) CloseHandle(previous);
        if (!durable) {
            DeleteFileW(temporary);
            return Error{ErrorCode::Io,"Cannot flush INI backup: " + std::to_string(error) + "; backup " + backup.string()};
        }
    }
    bool replaced = false;
    if (existed) {
        if (replace) { error = replace(full, temporary); replaced = error == 0; }
        else {
            replaced = ReplaceFileW(full.c_str(), temporary, nullptr, 0, nullptr, nullptr) != FALSE;
            error = GetLastError();
        }
    } else {
        replaced = MoveFileExW(temporary, full.c_str(), MOVEFILE_WRITE_THROUGH) != FALSE;
        error = GetLastError();
    }
    if (!replaced) {
        bool recovered = false;
        if (existed && GetFileAttributesW(full.c_str()) == INVALID_FILE_ATTRIBUTES)
            recovered = MoveFileExW(backup.c_str(), full.c_str(), MOVEFILE_WRITE_THROUGH) != FALSE;
        // Never discard either version on a partially failed replacement.
        return Error{ErrorCode::Io,"INI replacement failed: " + std::to_string(error) +
            (recovered ? "; original restored" : "; backup retained at " + backup.string()) +
            "; replacement retained at " + std::filesystem::path(temporary).string()};
    }
    if (existed) DeleteFileW(backup.c_str());
    return true;
}
}
