#include <windows.h>

#include <cstdint>
#include <iostream>
#include <string>
#include <thread>

using LogViewHandle = uintptr_t;
using FnInit = int(__stdcall*)();
using FnCreatePopupEx = LogViewHandle(__stdcall*)(int, int, int);
using FnIntHandle = int(__stdcall*)(LogViewHandle);
using FnSetTitle = int(__stdcall*)(LogViewHandle, const char*);
using FnAddLine = int(__stdcall*)(LogViewHandle, const char*);
using FnAddLineEx = int(__stdcall*)(LogViewHandle, const char*, uint32_t, int);
using FnErrorText = const char*(__stdcall*)();

template <typename T>
T load(HMODULE dll, const char* name) {
    return reinterpret_cast<T>(GetProcAddress(dll, name));
}

int main() {
    HMODULE dll = LoadLibraryW(L"LogView_x64.dll");
    if (!dll) {
        std::cerr << "Copy LogView_x64.dll next to this example before running.\n";
        return 1;
    }

    auto init = load<FnInit>(dll, "LogView_Init");
    auto uninit = load<FnInit>(dll, "LogView_Uninit");
    auto createPopup = load<FnCreatePopupEx>(dll, "LogView_CreatePopupEx");
    auto destroy = load<FnIntHandle>(dll, "LogView_Destroy");
    auto show = load<FnIntHandle>(dll, "LogView_Show");
    auto setTitle = load<FnSetTitle>(dll, "LogView_SetTitle");
    auto addLine = load<FnAddLine>(dll, "LogView_AddLine");
    auto addLineEx = load<FnAddLineEx>(dll, "LogView_AddLineEx");
    auto getLastErrorText = load<FnErrorText>(dll, "LogView_GetLastErrorText");

    if (!init || !uninit || !createPopup || !destroy || !show || !setTitle || !addLine || !addLineEx) {
        std::cerr << "Missing LogView export.\n";
        return 1;
    }

    if (!init()) {
        std::cerr << getLastErrorText() << "\n";
        return 1;
    }

    LogViewHandle log = createPopup(460, 360, 180);
    if (!log) {
        std::cerr << getLastErrorText() << "\n";
        return 1;
    }

    setTitle(log, u8"C++ 日志窗口");
    show(log);
    for (int i = 0; i < 20; ++i) {
        addLine(log, u8"普通日志");
        addLineEx(log, u8"成功日志", 0xFF38D27A, 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
    }
    std::this_thread::sleep_for(std::chrono::seconds(2));
    destroy(log);
    uninit();
    FreeLibrary(dll);
    return 0;
}

