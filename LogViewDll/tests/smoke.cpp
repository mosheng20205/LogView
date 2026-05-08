#include <windows.h>

#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using LogViewHandle = uintptr_t;
using LogViewHwnd = uintptr_t;
using LogViewColor = uint32_t;

using FnInit = int(__stdcall*)();
using FnCreatePopupEx = LogViewHandle(__stdcall*)(int, int, int);
using FnDestroy = int(__stdcall*)(LogViewHandle);
using FnShow = int(__stdcall*)(LogViewHandle);
using FnSetTitle = int(__stdcall*)(LogViewHandle, const char*);
using FnAddLine = int(__stdcall*)(LogViewHandle, const char*);
using FnAddLineEx = int(__stdcall*)(LogViewHandle, const char*, LogViewColor, int);
using FnGetLineCount = int(__stdcall*)(LogViewHandle);
using FnGetText = int(__stdcall*)(LogViewHandle, char*, int);
using FnSaveToFile = int(__stdcall*)(LogViewHandle, const char*);
using FnGetVersion = const char*(__stdcall*)();
using FnGetLastErrorText = const char*(__stdcall*)();

template <typename T>
T Load(HMODULE module, const char* name) {
    auto proc = reinterpret_cast<T>(GetProcAddress(module, name));
    if (!proc) {
        std::cerr << "Missing export: " << name << "\n";
        std::abort();
    }
    return proc;
}

void Check(int ok, FnGetLastErrorText getLastErrorText, const char* name) {
    if (!ok) {
        std::cerr << name << " failed: " << getLastErrorText() << "\n";
        std::abort();
    }
}

std::string ReadAllText(const char* path) {
    std::ifstream file(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

int main() {
#ifdef _WIN64
    HMODULE module = LoadLibraryW(L"LogView_x64.dll");
#else
    HMODULE module = LoadLibraryW(L"LogView_x86.dll");
#endif
    if (!module) {
        std::cerr << "Failed to load LogView DLL.\n";
        return 1;
    }

    auto init = Load<FnInit>(module, "LogView_Init");
    auto uninit = Load<FnInit>(module, "LogView_Uninit");
    auto createPopupEx = Load<FnCreatePopupEx>(module, "LogView_CreatePopupEx");
    auto destroy = Load<FnDestroy>(module, "LogView_Destroy");
    auto show = Load<FnShow>(module, "LogView_Show");
    auto setTitle = Load<FnSetTitle>(module, "LogView_SetTitle");
    auto addLine = Load<FnAddLine>(module, "LogView_AddLine");
    auto addLineEx = Load<FnAddLineEx>(module, "LogView_AddLineEx");
    auto getLineCount = Load<FnGetLineCount>(module, "LogView_GetLineCount");
    auto getText = Load<FnGetText>(module, "LogView_GetText");
    auto saveToFile = Load<FnSaveToFile>(module, "LogView_SaveToFile");
    auto getVersion = Load<FnGetVersion>(module, "LogView_GetVersion");
    auto getLastErrorText = Load<FnGetLastErrorText>(module, "LogView_GetLastErrorText");

    Check(init(), getLastErrorText, "LogView_Init");
    LogViewHandle handle = createPopupEx(360, 260, 180);
    if (!handle) {
        std::cerr << "LogView_CreatePopupEx failed: " << getLastErrorText() << "\n";
        return 1;
    }
    Check(setTitle(handle, "C++ smoke"), getLastErrorText, "LogView_SetTitle");
    Check(addLine(handle, "normal log"), getLastErrorText, "LogView_AddLine");
    Check(addLineEx(handle, "success log", 0xFF38D27A, 1), getLastErrorText, "LogView_AddLineEx");
    Check(show(handle), getLastErrorText, "LogView_Show");
    if (getLineCount(handle) != 2) {
        std::cerr << "Unexpected line count.\n";
        return 1;
    }

    const int required = getText(handle, nullptr, 0);
    if (required <= 1) {
        std::cerr << "LogView_GetText returned an invalid required size.\n";
        return 1;
    }
    std::vector<char> buffer(static_cast<size_t>(required));
    if (getText(handle, buffer.data(), required) != required) {
        std::cerr << "LogView_GetText returned inconsistent size.\n";
        return 1;
    }
    const std::string exportedText(buffer.data());
    if (exportedText.find("normal log") == std::string::npos ||
        exportedText.find("success log") == std::string::npos) {
        std::cerr << "Exported log text is missing expected lines.\n";
        return 1;
    }

    const char* exportPath = "LogViewSmoke_export.txt";
    DeleteFileA(exportPath);
    Check(saveToFile(handle, exportPath), getLastErrorText, "LogView_SaveToFile");
    const std::string fileText = ReadAllText(exportPath);
    if (fileText != exportedText) {
        std::cerr << "Saved log file content does not match LogView_GetText.\n";
        return 1;
    }
    DeleteFileA(exportPath);

    Check(destroy(handle), getLastErrorText, "LogView_Destroy");
    Check(uninit(), getLastErrorText, "LogView_Uninit");
    std::cout << "SMOKE_OK version=" << getVersion() << "\n";
    FreeLibrary(module);
    return 0;
}
