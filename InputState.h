// SPDX-License-Identifier: MIT
// Copyright (c) 2026 SpaceNavigator for Illustrator contributors
// Applies to original project code; Adobe SDK material retains its own terms.

#pragma once
#include <windows.h>
#include <cstdint>
#include <cmath>
#include <algorithm>
struct InputState {
    volatile LONG stop;
    volatile LONG sequence;
    volatile LONG x, y, z;
    volatile LONG packets;
    volatile LONG devices;
    volatile LONG error;
    alignas(8) volatile LONG64 tick;
};
inline bool DecodeTranslation(const unsigned char* data, size_t size, int& x, int& y, int& z) {
    if (size < 7 || data[0] != 1) return false;
    auto axis = [&](int at) { return static_cast<int>(static_cast<int16_t>(static_cast<uint16_t>(data[at]) | (static_cast<uint16_t>(data[at + 1]) << 8))); };
    x = axis(1); y = axis(3); z = axis(5); return true;
}
inline double AxisResponse(int value, int deadZone) {
    double magnitude = std::abs(static_cast<double>(value));
    if (magnitude <= deadZone) return 0;
    double normalized = std::min(1.0, (magnitude - deadZone) / (350.0 - deadZone));
    return std::copysign(normalized * normalized, value);
}
// Exponential scaling gives the same proportional zoom response at any magnification.
inline double ZoomResponse(double zoom, double axis, double speed, double dt, double minimum, double maximum) {
    return std::clamp(zoom * std::exp2(axis * speed * dt), minimum, maximum);
}

