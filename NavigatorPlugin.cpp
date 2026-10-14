// SPDX-License-Identifier: MIT
// Copyright (c) 2026 SpaceNavigator for Illustrator contributors
// Applies to original project code; Adobe SDK material retains its own terms.

#include "IllustratorSDK.h"
#include "Plugin.hpp"
#include "AIDocument.h"
#include "AIPreference.h"
#include "AIDocumentView.h"
#include "AITimer.h"
#include <windows.h>
#include <chrono>
#include <cmath>


#include <sstream>
#include <filesystem>
#include "InputState.h"
#include "Settings.h"
extern "C" IMAGE_DOS_HEADER __ImageBase;

extern "C" {
SPBlocksSuite* sSPBlocks = nullptr;
AIMenuSuite* sAIMenu = nullptr;
AIUnicodeStringSuite* sAIUnicodeString = nullptr;
AIDocumentSuite* sDocument = nullptr;
AIDocumentViewSuite* sView = nullptr;
AITimerSuite* sTimer = nullptr;
AIPreferenceSuite* sPreferences = nullptr;
}
ImportSuite gImportSuites[] = {
    kSPBlocksSuite, kSPBlocksSuiteVersion, &sSPBlocks,
    kAIMenuSuite, kAIMenuSuiteVersion, &sAIMenu,
    kAIUnicodeStringSuite, kAIUnicodeStringVersion, &sAIUnicodeString,
    kAIDocumentSuite, kAIDocumentSuiteVersion, &sDocument,
    kAIDocumentViewSuite, kAIDocumentViewSuiteVersion, &sView,
    kAITimerSuite, kAITimerSuiteVersion, &sTimer,
    kAIPreferenceSuite, kAIPreferenceSuiteVersion, &sPreferences,
    nullptr, 0, nullptr
};
extern "C" AINotifierSuite* sAINotifier;
using Clock = std::chrono::steady_clock;


class NavigatorPlugin : public Plugin {
public:
    explicit NavigatorPlugin(SPPluginRef ref) : Plugin(ref) { strcpy_s(fPluginName, "IllustratorNavigator"); }
    FIXUP_VTABLE_EX(NavigatorPlugin, Plugin);
    ASErr StartupPlugin(SPInterfaceMessage* message) override;
    ASErr ShutdownPlugin(SPInterfaceMessage* message) override {
        DisableInput();
        return Plugin::ShutdownPlugin(message);
    }
    ASErr PostStartupPlugin() override {
        ASErr error = Plugin::PostStartupPlugin();
        if (!error && desiredEnabled) EnableInput();
        return error;
    }
    ASErr GoMenuItem(AIMenuMessage* message) override;
    ASErr GoTimer(AITimerMessage* message) override;
private:
    AIMenuItemHandle menus[15]{};
    AITimerHandle inputTimer{};
    HANDLE mapping{}, inputProcess{};
    InputState* input{};
    bool inputEnabled = false, swapAxes = false, reverseX = true, reverseY = true, reverseZoom = false, zoomEnabled = true;
    bool desiredEnabled = false;
    void LoadSettings();
    void SaveSettings();
    double speed = 1200;
    double zoomSpeed = 1.5;
    int deadZone = 20;
    Clock::time_point lastPan;
    void EnableInput();
    void DisableInput();
    void PanInput();
    void InputStatus();
    void RefreshMenu();
    void Alert(const std::string& text) { MessageBoxA(nullptr, text.c_str(), "Illustrator Navigator", MB_OK); }
};
Plugin* AllocatePlugin(SPPluginRef ref) { return new NavigatorPlugin(ref); }
void FixupReload(Plugin* plugin) { NavigatorPlugin::FixupVTable(static_cast<NavigatorPlugin*>(plugin)); }

ASErr NavigatorPlugin::StartupPlugin(SPInterfaceMessage* message) {
    AIErr error = Plugin::StartupPlugin(message);
    if (error) return error;
    LoadSettings();
    auto submenu = [&](const char* parent, const char* label, const char* name) -> AIErr {
        AIPlatformAddMenuItemDataUS data;
        data.groupName = parent; data.itemText = ai::UnicodeString(label);
        AIMenuItemHandle item{}; AIMenuGroup group{};
        AIErr result = sAIMenu->AddMenuItem(message->d.self, name, &data, 0, &item);
        if (result) return result;
        return sAIMenu->AddMenuGroupAsSubMenu(name, 0, item, &group);
    };
    auto item = [&](int id, const char* parent, const char* label) -> AIErr {
        AIPlatformAddMenuItemDataUS data;
        data.groupName = parent; data.itemText = ai::UnicodeString(label);
        std::string name = "NavigatorSetting" + std::to_string(id);
        return sAIMenu->AddMenuItem(message->d.self, name.c_str(), &data, 0, &menus[id]);
    };
    if ((error = submenu("Window Utilities", "SpaceNavigator", "NavigatorMenu"))) return error;
    if ((error = item(0, "NavigatorMenu", "Enable navigation"))) return error;
    if ((error = submenu("NavigatorMenu", "Pan", "NavigatorPan"))) return error;
    if ((error = item(1, "NavigatorPan", "Faster"))) return error;
    if ((error = item(2, "NavigatorPan", "Slower"))) return error;
    if ((error = item(3, "NavigatorPan", "Reverse direction"))) return error;
    if ((error = submenu("NavigatorMenu", "Zoom", "NavigatorZoom"))) return error;
    if ((error = item(4, "NavigatorZoom", "Enable zoom"))) return error;
    if ((error = item(5, "NavigatorZoom", "Reverse direction"))) return error;
    if ((error = item(6, "NavigatorZoom", "Faster"))) return error;
    if ((error = item(7, "NavigatorZoom", "Slower"))) return error;
    if ((error = submenu("NavigatorMenu", "Advanced", "NavigatorAdvanced"))) return error;
    if ((error = item(8, "NavigatorAdvanced", "Swap forward and lift axes"))) return error;
    if ((error = item(9, "NavigatorAdvanced", "Reverse horizontal only"))) return error;
    if ((error = item(10, "NavigatorAdvanced", "Reverse vertical only"))) return error;
    if ((error = item(11, "NavigatorAdvanced", "Increase dead zone"))) return error;
    if ((error = item(12, "NavigatorAdvanced", "Decrease dead zone"))) return error;
    if ((error = item(13, "NavigatorMenu", "Reset settings"))) return error;
    if ((error = item(14, "NavigatorMenu", "Device status..."))) return error;
    error = sTimer->AddTimer(message->d.self, "NavigatorInputTimer", 1, &inputTimer);
    if (error) return error;
    error = sTimer->SetTimerActive(inputTimer, false);
    if (error) return error;
    RefreshMenu();
    return kNoErr;
}
void NavigatorPlugin::RefreshMenu() {
    if (!sAIMenu || !menus[0]) return;
    sAIMenu->CheckItem(menus[0], inputEnabled);
    sAIMenu->CheckItem(menus[3], reverseX && reverseY);
    sAIMenu->CheckItem(menus[4], zoomEnabled);
    sAIMenu->CheckItem(menus[5], reverseZoom);
    sAIMenu->CheckItem(menus[8], swapAxes);
    sAIMenu->CheckItem(menus[9], reverseX);
    sAIMenu->CheckItem(menus[10], reverseY);
}
ASErr NavigatorPlugin::GoMenuItem(AIMenuMessage* message) {
    int command = -1;
    for (int i = 0; i < 15; ++i) if (message->menuItem == menus[i]) command = i;
    switch (command) {
        case 0: if (inputEnabled) DisableInput(); else EnableInput(); desiredEnabled = inputEnabled; break;
        case 1: speed = std::min(3000.0, speed * 1.25); break;
        case 2: speed = std::max(30.0, speed / 1.25); break;
        case 3: reverseX = reverseY = !(reverseX && reverseY); break;
        case 4: zoomEnabled = !zoomEnabled; break;
        case 5: reverseZoom = !reverseZoom; break;
        case 6: zoomSpeed = std::min(6.0, zoomSpeed * 1.25); break;
        case 7: zoomSpeed = std::max(0.1, zoomSpeed / 1.25); break;
        case 8: swapAxes = !swapAxes; break;
        case 9: reverseX = !reverseX; break;
        case 10: reverseY = !reverseY; break;
        case 11: deadZone = std::min(150, deadZone + 5); break;
        case 12: deadZone = std::max(0, deadZone - 5); break;
        case 13: speed = 1200; zoomSpeed = 1.5; deadZone = 20; swapAxes = reverseZoom = false; reverseX = reverseY = zoomEnabled = true; break;
        case 14: InputStatus(); break;
        default: return kNoErr;
    }
    if (command != 14) SaveSettings();
    RefreshMenu();
    return kNoErr;
}
ASErr NavigatorPlugin::GoTimer(AITimerMessage* message) {
    if (message->timer == inputTimer) PanInput();
    return kNoErr;
}
void NavigatorPlugin::EnableInput() {
    if (inputEnabled) { InputStatus(); return; }

    std::wstring name = L"Local\\IllustratorNavigatorInput_" + std::to_wstring(GetCurrentProcessId());
    mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(InputState), name.c_str());
    if (!mapping) { Alert("Could not create device connection."); return; }
    input = static_cast<InputState*>(MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(InputState)));
    if (!input) { DisableInput(); Alert("Could not map device connection."); return; }
    ZeroMemory(input, sizeof(InputState));
    wchar_t module[MAX_PATH]{};
    GetModuleFileNameW(reinterpret_cast<HMODULE>(&__ImageBase), module, MAX_PATH);
    auto executable = std::filesystem::path(module).parent_path() / "NavigatorInputBridge.exe";
    std::wstring command = L"\"" + executable.wstring() + L"\" " + std::to_wstring(GetCurrentProcessId());
    STARTUPINFOW startup{}; startup.cb = sizeof(startup); PROCESS_INFORMATION process{};
    if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, false, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process)) {
        DWORD error = GetLastError(); DisableInput(); Alert("Could not start input reader. Windows error " + std::to_string(error)); return;
    }
    inputProcess = process.hProcess; CloseHandle(process.hThread);
    lastPan = Clock::now(); inputEnabled = true; RefreshMenu();
    AIErr error = sTimer->SetTimerActive(inputTimer, true);
    if (error) { DisableInput(); Alert("Could not activate input timer: " + std::to_string(error)); }
}
void NavigatorPlugin::DisableInput() {
    inputEnabled = false; RefreshMenu();
    if (inputTimer && sTimer) sTimer->SetTimerActive(inputTimer, false);
    if (input) InterlockedExchange(&input->stop, 1);
    // Reader checks shutdown every 50ms. Do not reuse its mapping until it exits.
    if (inputProcess) {
        if (WaitForSingleObject(inputProcess, 1500) == WAIT_TIMEOUT) TerminateProcess(inputProcess, 0);
        CloseHandle(inputProcess); inputProcess = nullptr;
    }
    if (input) { UnmapViewOfFile(input); input = nullptr; }
    if (mapping) { CloseHandle(mapping); mapping = nullptr; }
}
void NavigatorPlugin::PanInput() {
    auto now = Clock::now();
    double dt = std::min(0.05, std::chrono::duration<double>(now - lastPan).count()); lastPan = now;
    if (!inputEnabled || !input) return;
    if (WaitForSingleObject(inputProcess, 0) != WAIT_TIMEOUT) { DisableInput(); Alert("Input reader exited. Try enabling it again."); return; }
    if (InterlockedCompareExchange(&input->error,0,0)) { LONG error = input->error; DisableInput(); Alert("Input reader error " + std::to_string(error)); return; }
    DWORD pid = 0; GetWindowThreadProcessId(GetForegroundWindow(), &pid);
    if (pid != GetCurrentProcessId()) return;
    wchar_t windowClass[64]{}; GetClassNameW(GetForegroundWindow(), windowClass, 64);
    if (wcscmp(windowClass, L"#32770") == 0) return;
    GUITHREADINFO gui{}; gui.cbSize = sizeof(gui);
    if (GetGUIThreadInfo(0, &gui) && (gui.flags & (GUI_INMENUMODE | GUI_POPUPMENUMODE | GUI_SYSTEMMENUMODE))) return;
    LONG before = InterlockedCompareExchange(&input->sequence,0,0);
    if (before & 1) return;
    int x = input->x, y = swapAxes ? input->z : input->y, z = swapAxes ? input->y : input->z;
    LONG64 timestamp = InterlockedCompareExchange64(&input->tick,0,0);
    if (before != InterlockedCompareExchange(&input->sequence,0,0) || GetTickCount64() - timestamp > 250) return;
    double dx = AxisResponse(x, deadZone) * (reverseX ? -1 : 1);
    double dy = AxisResponse(y, deadZone) * (reverseY ? -1 : 1);
    double dz = zoomEnabled ? AxisResponse(z, deadZone) * (reverseZoom ? -1 : 1) : 0;
    if (dx == 0 && dy == 0 && dz == 0) return;
    AIDocumentHandle doc{}; AIDocumentViewHandle active{}; AIRealPoint center{}; AIReal zoom{};
    if (sDocument->GetDocument(&doc) || !doc || sView->GetNthDocumentView(0,&active) || !active ||
        sView->GetDocumentViewCenter(active,&center) || sView->GetDocumentViewZoom(active,&zoom)) return;
    if (!std::isfinite(zoom) || zoom <= 0 || !std::isfinite(center.h) || !std::isfinite(center.v)) return;
    AIErr error = kNoErr;
    if (dz != 0) {
        AIReal minimum{}, maximum{};
        error = sDocument->GetDocumentZoomLimit(&minimum, &maximum);
        if (error || !std::isfinite(minimum) || !std::isfinite(maximum) || minimum <= 0 || maximum < minimum) {
            DisableInput(); Alert("Could not read valid zoom limits; navigation stopped."); return;
        }
        AIReal targetZoom = static_cast<AIReal>(ZoomResponse(zoom, dz, zoomSpeed, dt, minimum, maximum));
        if (targetZoom != zoom) error = sView->SetDocumentViewZoom(active, targetZoom);
        if (error) { DisableInput(); Alert("Zoom stopped after view error: " + std::to_string(error)); return; }
        // Scale panning by the zoom Illustrator actually accepted, including host clamping.
        if (sView->GetDocumentViewZoom(active, &zoom) || !std::isfinite(zoom) || zoom <= 0) return;
    }
    if (dx == 0 && dy == 0) return;
    center.h += dx * speed * dt / zoom;
    center.v += dy * speed * dt / zoom;
    error = sView->SetDocumentViewCenter(active, &center);
    if (error) { DisableInput(); Alert("Device pan stopped after view error: " + std::to_string(error)); }
}
void NavigatorPlugin::InputStatus() {
    std::ostringstream text;
    text << "SpaceNavigator navigation: " << (inputEnabled ? "enabled" : "disabled") << "\nSpeed: " << speed
        << " screen units/sec\nDead zone: " << deadZone << "\nHorizontal reversed: " << reverseX
        << "\nVertical reversed: " << reverseY << "\nForward/lift swapped: " << swapAxes
        << "\nZoom enabled: " << zoomEnabled << "; reversed: " << reverseZoom << "; speed: " << zoomSpeed << " octaves/sec";
    if (input) text << "\nDevices found: " << input->devices << "\nTranslation reports: " << input->packets
        << "\nRaw X/Y/Z: " << input->x << "/" << input->y << "/" << input->z;
    text << "\n\nLift/press zooms around the view center. Settings are stored in Illustrator preferences and saved to disk when Illustrator quits normally.";
    Alert(text.str());
}



void NavigatorPlugin::LoadSettings() {
    auto settings = ReadSettings(sPreferences);
    desiredEnabled = (settings.flags & 1) != 0;
    swapAxes = (settings.flags & 2) != 0;
    reverseX = (settings.flags & 4) != 0;
    reverseY = (settings.flags & 8) != 0;
    reverseZoom = (settings.flags & 16) != 0;
    zoomEnabled = (settings.flags & 32) != 0;
    speed = settings.panSpeed; zoomSpeed = settings.zoomSpeed; deadZone = settings.deadZone;
}
void NavigatorPlugin::SaveSettings() {
    SavedSettings settings;
    settings.flags = (desiredEnabled ? 1 : 0) | (swapAxes ? 2 : 0) | (reverseX ? 4 : 0) |
        (reverseY ? 8 : 0) | (reverseZoom ? 16 : 0) | (zoomEnabled ? 32 : 0);
    settings.panSpeed = speed; settings.zoomSpeed = zoomSpeed; settings.deadZone = deadZone;
    if (!WriteSettings(sPreferences, settings)) Alert("Your setting changed for this session, but Illustrator could not update its preferences.");
}


