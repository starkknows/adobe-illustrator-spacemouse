// SPDX-License-Identifier: MIT
// Copyright (c) 2026 SpaceNavigator for Illustrator contributors
// Applies to original project code; Adobe SDK material retains its own terms.

#include "InputState.h"
#include "Settings.h"
#include <cassert>
#include <iostream>
#include <string>
#include <map>
struct TestPreferences {
    std::map<std::string,int> integers;
    std::map<std::string,double> reals;
    bool failWrites = false;
    int GetIntegerPreference(const char*, const char* key, int* out) {
        auto it = integers.find(key); if (it == integers.end()) return 1; *out = it->second; return 0;
    }
    int GetRealPreference(const char*, const char* key, double* out) {
        auto it = reals.find(key); if (it == reals.end()) return 1; *out = it->second; return 0;
    }
    int PutIntegerPreference(const char*, const char* key, int value) { if (failWrites) return 1; integers[key] = value; return 0; }
    int PutRealPreference(const char*, const char* key, double value) { if (failWrites) return 1; reals[key] = value; return 0; }
};
int main() {
    TestPreferences preferences;
    auto defaults = ReadSettings(&preferences);
    assert(defaults.flags == 44 && defaults.panSpeed == 1200);
    SavedSettings saved; saved.flags = 1 | 2 | 16; saved.panSpeed = 1875; saved.zoomSpeed = 2.34375; saved.deadZone = 35;
    assert(WriteSettings(&preferences,saved));
    auto loaded = ReadSettings(&preferences);
    assert(loaded.flags == saved.flags && loaded.panSpeed == saved.panSpeed && loaded.zoomSpeed == saved.zoomSpeed && loaded.deadZone == saved.deadZone);
    preferences.reals["panSpeed"] = 99999; preferences.integers["deadZone"] = -50;
    loaded = ReadSettings(&preferences); assert(loaded.panSpeed == 3000 && loaded.deadZone == 0);
    preferences.integers["version"] = 99;
    loaded = ReadSettings(&preferences); assert(loaded.panSpeed == 1200 && loaded.flags == 44);
    preferences.failWrites = true; assert(!WriteSettings(&preferences,saved));
    TestPreferences missing; missing.integers["version"] = 1;
    loaded = ReadSettings(&missing); assert(loaded.flags == 44 && loaded.panSpeed == 1200);
    std::cout << "Preference adapter, defaults, versioning, bounds, and write-failure checks passed.\n";
    unsigned char report[] = {1, 0x5e, 0x01, 0xa2, 0xfe, 0xff, 0x7f};
    int x=0,y=0,z=0;
    assert(DecodeTranslation(report,7,x,y,z) && x==350 && y==-350 && z==32767);
    assert(!DecodeTranslation(report,6,x,y,z));
    report[0]=2; assert(!DecodeTranslation(report,7,x,y,z));
    assert(AxisResponse(0,20)==0 && AxisResponse(20,20)==0 && AxisResponse(-20,20)==0);
    assert(AxisResponse(350,20)==1 && AxisResponse(-350,20)==-1);
    assert(AxisResponse(5000,20)==1);
    assert(AxisResponse(100,20)>0 && AxisResponse(100,20)<AxisResponse(200,20));
    assert(ZoomResponse(1,0,1.5,0.05,0.01,100)==1);
    assert(std::abs(ZoomResponse(1,1,1,1,0.01,100)-2)<1e-12);
    assert(std::abs(ZoomResponse(1,-1,1,1,0.01,100)-0.5)<1e-12);
    assert(ZoomResponse(99,1,6,1,0.01,100)==100);
    assert(ZoomResponse(0.02,-1,6,1,0.01,100)==0.01);
    double split = 1;
    for (int i=0;i<60;++i) split=ZoomResponse(split,1,1,1.0/60,0.01,100);
    assert(std::abs(split-2)<1e-10);
    std::cout << "Zoom scaling, direction, clamping, and rate-independence checks passed.\n";
    std::cout << "Translation decoding and dead-zone response checks passed.\n";
    // Exercise the real reader/IPC startup and clean shutdown, without moving Illustrator.
    std::wstring name = L"Local\\IllustratorNavigatorInput_" + std::to_wstring(GetCurrentProcessId());
    HANDLE mapping = CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(InputState),name.c_str());
    assert(mapping);
    auto* state = static_cast<InputState*>(MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(InputState)));
    assert(state); ZeroMemory(state,sizeof(InputState));
    wchar_t ownPath[MAX_PATH]{}; GetModuleFileNameW(nullptr,ownPath,MAX_PATH);
    std::wstring executable(ownPath); executable=executable.substr(0,executable.find_last_of(L"\\")+1)+L"NavigatorInputBridge.exe";
    std::wstring command=L"\""+executable+L"\" "+std::to_wstring(GetCurrentProcessId());
    STARTUPINFOW startup{}; startup.cb=sizeof(startup); PROCESS_INFORMATION process{};
    BOOL launched=CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,false,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process);
    assert(launched);
    Sleep(400);
    LONG error=state->error, devices=state->devices;
    InterlockedExchange(&state->stop,1);
    DWORD result=WaitForSingleObject(process.hProcess,2000);
    CloseHandle(process.hThread); CloseHandle(process.hProcess);
    UnmapViewOfFile(state); CloseHandle(mapping);
    assert(error==0 && result==WAIT_OBJECT_0);
    std::cout << "Reader IPC/startup/shutdown passed. Connected SpaceNavigators: " << devices << "\n";
}





