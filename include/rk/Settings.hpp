#pragma once
#include "rk/Result.hpp"
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <functional>
#include <string_view>
#include <vector>

namespace rk {
struct Choice { std::string value; bool operator==(const Choice&) const = default; };
struct Text { std::string value; bool operator==(const Text&) const = default; };
using SettingValue = std::variant<bool, std::int64_t, double, Choice, Text>;
struct Field {
    std::string key;
    SettingValue initial;
    double minimum{}, maximum{};
    std::vector<std::string> choices;
};
const std::vector<Field>& settingsSchema();
struct Settings {
    std::map<std::string, SettingValue> values;
    std::vector<std::string> originalLines;
    template<class T> const T& get(const std::string& key) const { return std::get<T>(values.at(key)); }
    bool operator==(const Settings&) const = default;
};
Settings defaultSettings();
Result<Settings> parseIni(std::string_view text);
Result<std::string> serializeIni(const Settings& settings);
// OS replacement boundary: return 0 on success, otherwise the native error.
// An implementation may fail after renaming either file; recovery must handle it.
using FileReplace = std::function<std::uint32_t(const std::filesystem::path&, const std::filesystem::path&)>;
Result<bool> saveIni(const std::filesystem::path& path, const Settings& settings, const FileReplace& replace = {});
Result<bool> validateSettings(const Settings& settings);
enum class ChangeCategory { Live, Recreate, RestartRequired };
ChangeCategory classifyChange(const Settings& before, const Settings& after);
struct SettingsSnapshot { Settings requested; std::uint64_t generation{}; };
class SettingsTransaction {
public:
    SettingsTransaction();
    std::shared_ptr<const SettingsSnapshot> snapshot() const;
    Result<ChangeCategory> apply(Settings next, const std::function<Result<bool>(const Settings&)>& prepare);
    std::shared_ptr<const Settings> pendingRestart() const;
private:
    mutable std::mutex mutex_;
    std::shared_ptr<const SettingsSnapshot> current_;
    std::shared_ptr<const Settings> pending_;
};
}
