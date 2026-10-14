// SPDX-License-Identifier: MIT
// Copyright (c) 2026 SpaceNavigator for Illustrator contributors
// Applies to original project code; Adobe SDK material retains its own terms.

#include "InputState.h"
#include <vector>
#include <string>
#include <cstddef>
static InputState* state = nullptr;
static DWORD ownerPid;
static HANDLE owner;
static HANDLE selectedDevice = nullptr;
static void Publish(int x, int y, int z) {
    InterlockedIncrement(&state->sequence);
    InterlockedExchange(&state->x, x); InterlockedExchange(&state->y, y); InterlockedExchange(&state->z, z);
    InterlockedExchange64(&state->tick, static_cast<LONG64>(GetTickCount64()));
    InterlockedIncrement(&state->sequence);
}
static bool Accept(HANDLE handle) {
    RID_DEVICE_INFO info{}; info.cbSize = sizeof(info); UINT size = sizeof(info);
    if (GetRawInputDeviceInfoW(handle, RIDI_DEVICEINFO, &info, &size) == static_cast<UINT>(-1)) return false;
    return info.dwType == RIM_TYPEHID && info.hid.dwVendorId == 0x046d && info.hid.dwProductId == 0xc626;
}
static void CountDevices() {
    UINT count = 0;
    GetRawInputDeviceList(nullptr, &count, sizeof(RAWINPUTDEVICELIST));
    std::vector<RAWINPUTDEVICELIST> list(count);
    LONG found = 0;
    UINT actual = GetRawInputDeviceList(list.data(), &count, sizeof(RAWINPUTDEVICELIST));
    if (actual != static_cast<UINT>(-1)) for (UINT i = 0; i < actual; ++i) if (Accept(list[i].hDevice)) ++found;
    InterlockedExchange(&state->devices, found);
}
static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wp, LPARAM lp) {
    if (message == WM_INPUT) {
        UINT size = 0;
        if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lp), RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER)) == 0 && size <= 65536 && size >= sizeof(RAWINPUTHEADER)) {
            // DWORD-aligned storage for RAWINPUT.
            std::vector<DWORD> storage((size + 3) / 4);
            if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lp), RID_INPUT, storage.data(), &size, sizeof(RAWINPUTHEADER)) == size) {
                auto* raw = reinterpret_cast<RAWINPUT*>(storage.data());
                constexpr size_t offset = offsetof(RAWINPUT, data) + offsetof(RAWHID, bRawData);
                if (raw->header.dwType == RIM_TYPEHID && size >= offset && Accept(raw->header.hDevice)) {
                    DWORD pid = 0; GetWindowThreadProcessId(GetForegroundWindow(), &pid);
                    auto& hid = raw->data.hid;
                    if (hid.dwSizeHid && hid.dwCount <= (size - offset) / hid.dwSizeHid) {
                        for (DWORD i = 0; i < hid.dwCount; ++i) {
                            int x, y, z;
                            if (DecodeTranslation(hid.bRawData + i * hid.dwSizeHid, hid.dwSizeHid, x, y, z)) {
                                selectedDevice = raw->header.hDevice;
                                InterlockedIncrement(&state->packets);
                                Publish(pid == ownerPid ? x : 0, pid == ownerPid ? y : 0, pid == ownerPid ? z : 0);
                            }
                        }
                    }
                }
            }
        }
        return DefWindowProcW(window, message, wp, lp);
    }
    if (message == WM_INPUT_DEVICE_CHANGE) {
        CountDevices();
        if (wp == GIDC_REMOVAL && reinterpret_cast<HANDLE>(lp) == selectedDevice) { selectedDevice = nullptr; Publish(0,0,0); }
        return 0;
    }
    if (message == WM_TIMER) {
        DWORD pid = 0; GetWindowThreadProcessId(GetForegroundWindow(), &pid);
        if (pid != ownerPid) Publish(0,0,0);
        if (InterlockedCompareExchange(&state->stop, 0, 0) || WaitForSingleObject(owner, 0) != WAIT_TIMEOUT) DestroyWindow(window);
        return 0;
    }
    if (message == WM_DESTROY) { Publish(0,0,0); PostQuitMessage(0); return 0; }
    return DefWindowProcW(window, message, wp, lp);
}
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR arguments, int) {
    ownerPid = wcstoul(arguments, nullptr, 10);
    if (!ownerPid) return 1;
    std::wstring name = L"Local\\IllustratorNavigatorInput_" + std::to_wstring(ownerPid);
    HANDLE mapping = OpenFileMappingW(FILE_MAP_ALL_ACCESS, false, name.c_str());
    if (!mapping) return 2;
    state = static_cast<InputState*>(MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(InputState)));
    if (!state) { CloseHandle(mapping); return 3; }
    owner = OpenProcess(SYNCHRONIZE, false, ownerPid);
    if (!owner) { InterlockedExchange(&state->error, GetLastError()); UnmapViewOfFile(state); CloseHandle(mapping); return 4; }
    WNDCLASSW klass{}; klass.lpfnWndProc = WindowProc; klass.hInstance = instance; klass.lpszClassName = L"NavigatorInputReader";
    RegisterClassW(&klass);
    HWND window = CreateWindowW(klass.lpszClassName, L"", 0, 0,0,0,0, HWND_MESSAGE, nullptr, instance, nullptr);
    RAWINPUTDEVICE device{1, 8, RIDEV_INPUTSINK | RIDEV_DEVNOTIFY, window};
    if (!window || !RegisterRawInputDevices(&device, 1, sizeof(device))) {
        InterlockedExchange(&state->error, GetLastError());
        if (window) DestroyWindow(window);
    } else {
        CountDevices(); SetTimer(window, 1, 50, nullptr);
        MSG msg{};
        while (GetMessageW(&msg, nullptr, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    CloseHandle(owner); UnmapViewOfFile(state); CloseHandle(mapping); return 0;
}

