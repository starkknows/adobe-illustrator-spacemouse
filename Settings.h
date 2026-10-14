// SPDX-License-Identifier: MIT
// Copyright (c) 2026 SpaceNavigator for Illustrator contributors
#pragma once
#include <cmath>
#include <algorithm>
struct SavedSettings {
    int version = 1;
    int flags = 4 | 8 | 32;
    double panSpeed = 1200;
    double zoomSpeed = 1.5;
    int deadZone = 20;
};
inline constexpr char PreferencePrefix[] = "IllustratorNavigator";
inline SavedSettings ValidateSettings(SavedSettings value) {
    if (value.version != 1 || !std::isfinite(value.panSpeed) || !std::isfinite(value.zoomSpeed)) return {};
    value.panSpeed = std::clamp(value.panSpeed, 30.0, 3000.0);
    value.zoomSpeed = std::clamp(value.zoomSpeed, 0.1, 6.0);
    value.deadZone = std::clamp(value.deadZone, 0, 150);
    value.flags &= 63;
    return value;
}
// The suite interface is supplied by Illustrator; tests use an in-memory stand-in.
template<class Preferences>
SavedSettings ReadSettings(Preferences* prefs) {
    SavedSettings value;
    if (!prefs) return value;
    int version = 0;
    if (prefs->GetIntegerPreference(PreferencePrefix, "version", &version) || version != 1) return value;
    prefs->GetIntegerPreference(PreferencePrefix, "flags", &value.flags);
    prefs->GetIntegerPreference(PreferencePrefix, "deadZone", &value.deadZone);
    prefs->GetRealPreference(PreferencePrefix, "panSpeed", &value.panSpeed);
    prefs->GetRealPreference(PreferencePrefix, "zoomSpeed", &value.zoomSpeed);
    return ValidateSettings(value);
}
template<class Preferences>
bool WriteSettings(Preferences* prefs, const SavedSettings& input) {
    if (!prefs) return false;
    auto value = ValidateSettings(input);
    bool ok = true;
    ok = prefs->PutIntegerPreference(PreferencePrefix, "flags", value.flags) == 0 && ok;
    ok = prefs->PutIntegerPreference(PreferencePrefix, "deadZone", value.deadZone) == 0 && ok;
    ok = prefs->PutRealPreference(PreferencePrefix, "panSpeed", value.panSpeed) == 0 && ok;
    ok = prefs->PutRealPreference(PreferencePrefix, "zoomSpeed", value.zoomSpeed) == 0 && ok;
    if (ok) ok = prefs->PutIntegerPreference(PreferencePrefix, "version", 1) == 0;
    return ok;
}
