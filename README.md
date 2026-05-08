# LogView

LogView 是一个 Windows 原生日志窗口 DLL，用于在 C/C++、C#、Python、易语言、火山视窗等可以调用 Win32 DLL 的程序中快速显示调试日志。它可以创建独立悬浮日志窗口，也可以嵌入到已有窗口句柄中作为子窗口使用。

项目使用 C++17、Win32、Direct2D 和 DirectWrite 实现，导出稳定的 C ABI，方便不同语言集成。

## 功能特性

- 支持 32 位和 64 位程序调用。
- 支持独立悬浮窗口和嵌入式子窗口。
- 支持半透明背景，文字保持清晰不透明。
- 支持普通、成功、警告、错误、调试、自定义等级日志。
- 支持日志自动滚动、清空、导出文本和保存到文件。
- 支持设置标题、字体、颜色、圆角、透明度、滚动条和最大行数。
- 支持事件回调和 Win32 消息通知。
- 提供 C++、C#、Python、易语言、火山视窗调用示例。

## 目录结构

```text
LogView/
|-- LogViewDll/             # DLL 源码、头文件、导出定义和 smoke 测试
|   |-- include/LogView.h   # C ABI 头文件
|   |-- src/LogView.cpp     # 核心实现
|   |-- exports/            # x86 / x64 导出定义
|   `-- tests/smoke.cpp     # C++ smoke 测试
|-- dist/                   # 发布文件
|   |-- include/LogView.h
|   |-- x86/LogView_x86.dll # 32 位 DLL，开源仓库必须保留
|   `-- x64/LogView_x64.dll # 64 位 DLL，开源仓库必须保留
|-- examples/               # 多语言调用示例
|   |-- cpp/
|   |-- csharp/
|   |-- python/
|   |-- e-language/
|   `-- volcano/
`-- DLL命令/                # DLL 命令和接口说明
```

## 快速开始

根据调用方程序位数选择对应 DLL：

| 调用方程序 | DLL |
| --- | --- |
| 32 位程序 | `dist/x86/LogView_x86.dll` |
| 64 位程序 | `dist/x64/LogView_x64.dll` |

运行程序时，请将对应 DLL 放到 EXE 同目录，或者放到系统可以搜索到的 DLL 路径中。

最小调用流程：

```text
LogView_Init()
LogView_CreatePopupEx()
LogView_SetTitle()
LogView_Show()
LogView_AddLine() / LogView_AddLineEx()
LogView_Destroy()
LogView_Uninit()
```

## C/C++ 示例

仓库发布包默认保留 DLL。没有 `.lib` 导入库时，C/C++ 可以使用 `LoadLibrary` 和 `GetProcAddress` 动态加载：

```cpp
#include <windows.h>
#include <cstdint>

using LogViewHandle = uintptr_t;
using FnInit = int(__stdcall*)();
using FnCreatePopupEx = LogViewHandle(__stdcall*)(int, int, int);
using FnHandleInt = int(__stdcall*)(LogViewHandle);
using FnSetTitle = int(__stdcall*)(LogViewHandle, const char*);
using FnAddLine = int(__stdcall*)(LogViewHandle, const char*);

template <typename T>
T load(HMODULE dll, const char* name) {
    return reinterpret_cast<T>(GetProcAddress(dll, name));
}

int main() {
    HMODULE dll = LoadLibraryW(L"LogView_x64.dll");
    if (!dll) {
        return 1;
    }

    auto init = load<FnInit>(dll, "LogView_Init");
    auto uninit = load<FnInit>(dll, "LogView_Uninit");
    auto createPopup = load<FnCreatePopupEx>(dll, "LogView_CreatePopupEx");
    auto destroy = load<FnHandleInt>(dll, "LogView_Destroy");
    auto show = load<FnHandleInt>(dll, "LogView_Show");
    auto setTitle = load<FnSetTitle>(dll, "LogView_SetTitle");
    auto addLine = load<FnAddLine>(dll, "LogView_AddLine");

    if (!init || !uninit || !createPopup || !destroy || !show || !setTitle || !addLine) {
        return 1;
    }

    if (!init()) {
        return 1;
    }

    LogViewHandle log = createPopup(520, 420, 180);
    if (!log) {
        return 1;
    }

    setTitle(log, u8"C++ 日志窗口");
    show(log);
    addLine(log, u8"普通日志");

    Sleep(3000);

    destroy(log);
    uninit();
    FreeLibrary(dll);
    return 0;
}
```

也可以参考完整示例：

- `examples/cpp/main.cpp`
- `LogViewDll/tests/smoke.cpp`

## C# 示例

```csharp
using System;
using System.Runtime.InteropServices;
using System.Text;

internal static class LogViewNative
{
    private const string DllName = "LogView_x64.dll";

    [DllImport(DllName, CallingConvention = CallingConvention.StdCall)]
    public static extern int LogView_Init();

    [DllImport(DllName, CallingConvention = CallingConvention.StdCall)]
    public static extern int LogView_Uninit();

    [DllImport(DllName, CallingConvention = CallingConvention.StdCall)]
    public static extern UIntPtr LogView_CreatePopupEx(int width, int height, int alpha);

    [DllImport(DllName, CallingConvention = CallingConvention.StdCall)]
    public static extern int LogView_SetTitle(UIntPtr handle, byte[] text);

    [DllImport(DllName, CallingConvention = CallingConvention.StdCall)]
    public static extern int LogView_Show(UIntPtr handle);

    [DllImport(DllName, CallingConvention = CallingConvention.StdCall)]
    public static extern int LogView_AddLine(UIntPtr handle, byte[] text);

    [DllImport(DllName, CallingConvention = CallingConvention.StdCall)]
    public static extern int LogView_Destroy(UIntPtr handle);

    private static byte[] Utf8(string text) => Encoding.UTF8.GetBytes(text + "\0");
}
```

C# 重点：

- 必须指定 `CallingConvention.StdCall`。
- 64 位程序使用 `LogView_x64.dll`，32 位程序使用 `LogView_x86.dll`。
- 字符串使用 UTF-8 字节传入，建议 `Encoding.UTF8.GetBytes(text + "\0")`。
- WinForms 嵌入子窗口时，将控件的 `Handle` 传给 `LogView_CreateChildEx`。

完整示例见 `examples/csharp/`。

## Python 示例

```python
import ctypes
from pathlib import Path

root = Path(__file__).resolve().parent
dll = ctypes.WinDLL(str(root / "dist" / "x64" / "LogView_x64.dll"))

dll.LogView_Init.restype = ctypes.c_int
dll.LogView_CreatePopupEx.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int]
dll.LogView_CreatePopupEx.restype = ctypes.c_size_t
dll.LogView_SetTitle.argtypes = [ctypes.c_size_t, ctypes.c_char_p]
dll.LogView_Show.argtypes = [ctypes.c_size_t]
dll.LogView_AddLine.argtypes = [ctypes.c_size_t, ctypes.c_char_p]
dll.LogView_Destroy.argtypes = [ctypes.c_size_t]
dll.LogView_Uninit.restype = ctypes.c_int

dll.LogView_Init()
handle = dll.LogView_CreatePopupEx(520, 420, 180)
dll.LogView_SetTitle(handle, "Python 日志窗口".encode("utf-8"))
dll.LogView_Show(handle)
dll.LogView_AddLine(handle, "普通日志".encode("utf-8"))

input("按回车关闭...")
dll.LogView_Destroy(handle)
dll.LogView_Uninit()
```

完整示例见：

- `examples/python/logview_demo.py`
- `examples/python/logview_full_demo.py`

## 从源码构建

构建要求：

- Windows
- CMake 3.20 或更高版本
- Visual Studio / MSVC

构建 64 位 DLL：

```powershell
cmake -S LogViewDll -B build/x64 -A x64
cmake --build build/x64 --config Release
```

构建 32 位 DLL：

```powershell
cmake -S LogViewDll -B build/x86 -A Win32
cmake --build build/x86 --config Release
```

构建产物会输出到：

```text
dist/x64/LogView_x64.dll
dist/x86/LogView_x86.dll
dist/include/LogView.h
```

## ABI 约定

所有导出函数使用：

```cpp
extern "C" __stdcall
```

核心类型：

```cpp
typedef uintptr_t LogViewHandle;
typedef uintptr_t LogViewHwnd;
typedef int LogViewBool;
typedef uint32_t LogViewColor;
```

字符串统一使用 UTF-8 编码的 `const char*`。颜色统一使用 `0xAARRGGBB`，例如：

```text
0xFFFFFFFF = 不透明白色
0xFF38D27A = 不透明绿色
0xFFEF4444 = 不透明红色
0x80000000 = 50% alpha 黑色
```

返回值规则：

- 创建类函数返回 `LogViewHandle`，失败返回 `0`。
- 普通操作函数返回 `int`，`1` 表示成功，`0` 表示失败。
- 失败原因可通过 `LogView_GetLastErrorCode()` 和 `LogView_GetLastErrorText()` 获取。

## 常用接口

| 分类 | 接口 |
| --- | --- |
| 初始化 | `LogView_Init`, `LogView_Uninit` |
| 创建销毁 | `LogView_CreatePopup`, `LogView_CreatePopupEx`, `LogView_CreateChild`, `LogView_CreateChildEx`, `LogView_Destroy`, `LogView_DestroyAll` |
| 显示控制 | `LogView_Show`, `LogView_Hide`, `LogView_Close`, `LogView_IsVisible`, `LogView_SetCloseMode` |
| 位置大小 | `LogView_SetPosition`, `LogView_SetSize`, `LogView_SetRect`, `LogView_GetRect`, `LogView_CenterScreen`, `LogView_EnableDrag` |
| 样式 | `LogView_SetAlpha`, `LogView_SetTopMost`, `LogView_SetBackgroundColor`, `LogView_SetBorderColor`, `LogView_SetRoundCorner`, `LogView_SetFont` |
| 日志 | `LogView_AddText`, `LogView_AddLine`, `LogView_AddLineEx`, `LogView_AddColoredLine`, `LogView_Clear`, `LogView_GetText`, `LogView_SaveToFile` |
| 事件 | `LogView_SetCallback`, `LogView_SetNotifyWindow` |
| 状态 | `LogView_SetName`, `LogView_FindByName`, `LogView_GetCount`, `LogView_IsValid`, `LogView_GetVersion` |

完整接口以 `LogViewDll/include/LogView.h` 为准。

## 日志等级

| 值 | 名称 |
| --- | --- |
| 0 | `LOGVIEW_LEVEL_INFO` |
| 1 | `LOGVIEW_LEVEL_SUCCESS` |
| 2 | `LOGVIEW_LEVEL_WARNING` |
| 3 | `LOGVIEW_LEVEL_ERROR` |
| 4 | `LOGVIEW_LEVEL_DEBUG` |
| 5 | `LOGVIEW_LEVEL_CUSTOM` |

## 事件类型

| 值 | 名称 |
| --- | --- |
| 1 | `LOGVIEW_EVENT_CLOSE_CLICKED` |
| 2 | `LOGVIEW_EVENT_CLEAR_CLICKED` |
| 3 | `LOGVIEW_EVENT_MOVED` |
| 4 | `LOGVIEW_EVENT_RESIZED` |
| 5 | `LOGVIEW_EVENT_DESTROYED` |
| 6 | `LOGVIEW_EVENT_LOG_LINE_CLICKED` |
| 7 | `LOGVIEW_EVENT_SCROLL_CHANGED` |

回调函数原型：

```cpp
void __stdcall LogViewCallback(
    LogViewHandle handle,
    int eventType,
    uintptr_t wParam,
    uintptr_t lParam,
    uintptr_t userData
);
```

## 常见问题

### 看不到窗口

请确认调用方程序位数和 DLL 位数一致，并确认程序没有立即退出。日志窗口创建后，主程序需要保持运行。

### 中文乱码

DLL 接收 UTF-8 字符串。C#、Python、易语言、火山视窗等语言调用时，请先将字符串转换为 UTF-8 字节再传入。

### 嵌入窗口不显示

请确认父窗口 HWND 已经创建。WinForms 中通常应在 `Shown`、`Load` 之后，或控件 `HandleCreated` 之后再调用 `LogView_CreateChildEx`。

### 透明后文字也变透明

LogView 的透明度主要作用于绘制背景，文字保持不透明，以保证日志可读性。

## 许可证

本项目基于 MIT License 开源，详见 `LICENSE`。

## 开源发布说明

本仓库的 `.gitignore` 已经忽略常见构建产物，但会保留发布必须的两个 DLL：

```text
dist/x86/LogView_x86.dll
dist/x64/LogView_x64.dll
```

发布到 GitHub 前，建议补充：

- GitHub 仓库描述、Topics 和 Release 包说明。
- 如果需要对外稳定分发，可以在 Release 中附带 `dist/include/LogView.h` 和对应 DLL。
