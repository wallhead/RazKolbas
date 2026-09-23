#include "rk/Settings.hpp"
namespace rk {
ChangeCategory classifyChange(const Settings& before, const Settings& after) {
    for (const auto key : {"General.Presentation", "General.SafeMode",
        "Diagnostics.CaptureHotkey", "Diagnostics.SpatialBaselineOnly",
        "Patching.EnableVersionedPatches", "Patching.ExperimentalPatches",
        "Patching.DisabledPatchIds"})
        if (before.values.at(key) != after.values.at(key)) return ChangeCategory::RestartRequired;
    for (const auto& [key, value] : before.values) {
        if (value == after.values.at(key)) continue;
        if (!key.starts_with("Interface.") && !key.starts_with("Diagnostics.") && key != "General.LogLevel") return ChangeCategory::Recreate;
    }
    return ChangeCategory::Live;
}
SettingsTransaction::SettingsTransaction() : current_(std::make_shared<SettingsSnapshot>(SettingsSnapshot{defaultSettings(), 0})) {}
std::shared_ptr<const SettingsSnapshot> SettingsTransaction::snapshot() const { std::scoped_lock lock(mutex_); return current_; }
std::shared_ptr<const Settings> SettingsTransaction::pendingRestart() const { std::scoped_lock lock(mutex_); return pending_; }
Result<ChangeCategory> SettingsTransaction::apply(Settings next, const std::function<Result<bool>(const Settings&)>& prepare) {
    const auto validation = validateSettings(next);
    if (const auto error = std::get_if<Error>(&validation)) return *error;
    std::scoped_lock lock(mutex_);
    const auto category = classifyChange(current_->requested, next);
    if (category == ChangeCategory::RestartRequired) {
        pending_ = std::make_shared<const Settings>(std::move(next));
        return category;
    }
    // This policy controller is called at the coordinator boundary. The callback
    // prepares a replacement; it must not mutate the active pipeline or reenter us.
    const auto prepared = prepare(next);
    if (const auto error = std::get_if<Error>(&prepared)) return *error;
    if (!std::get<bool>(prepared)) return Error{ErrorCode::Unavailable, "Replacement preparation failed; active settings retained"};
    current_ = std::make_shared<SettingsSnapshot>(SettingsSnapshot{std::move(next), current_->generation+1});
    pending_.reset();
    return category;
}
}
