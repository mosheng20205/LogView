# LogView DLL 命令与调用方法

本文档整理 LogView DLL 对外导出的全部函数、ABI 约定和常用语言调用方式。

适用对象：

- 易语言
- 火山视窗
- C#
- Python
- C/C++
- 其他任何可以调用 Windows DLL 的语言

## DLL 文件

发布目标：

- `LogView_x86.dll`：32 位调用方使用。
- `LogView_x64.dll`：64 位调用方使用。

调用方位数必须和 DLL 位数一致：

- 32 位程序加载 `LogView_x86.dll`
- 64 位程序加载 `LogView_x64.dll`

## ABI 规则

所有导出函数使用：

```cpp
extern "C" __stdcall
```

导出名为未修饰函数名，例如：

```text
LogView_Init
LogView_CreatePopupEx
LogView_AddLine
```

字符串统一使用 UTF-8：

```cpp
const char*
```

颜色统一使用 `0xAARRGGBB`：

```text
0xFFFFFFFF = 不透明白色
0xFFFF5555 = 不透明红色
0xFF55FF55 = 不透明绿色
0x80000000 = 50% alpha 黑色
```

句柄类型：

```cpp
typedef uintptr_t LogViewHandle;
typedef uintptr_t LogViewHwnd;
typedef int LogViewBool;
typedef uint32_t LogViewColor;
```

返回规则：

- 创建类函数返回 `LogViewHandle`，失败返回 `0`。
- 普通动作函数返回 `int`，`1` 表示成功，`0` 表示失败。
- 获取字符串的函数返回 DLL 内部维护的 `const char*`，调用方不要释放。

## 默认行为

- 默认窗口无边框。
- 默认支持圆角。
- 默认透明度为 50%，对应 alpha `128`。
- 透明效果只作用于绘制背景，文字保持不透明，避免文字模糊。
- 默认支持右上角关闭按钮。
- 默认支持右下角清空日志按钮。
- 默认支持右侧纵向滚动条。
- 默认支持多开，每个日志窗口都有独立句柄。

## 事件回调

回调原型：

```cpp
void __stdcall LogViewCallback(
    LogViewHandle handle,
    int eventType,
    uintptr_t wParam,
    uintptr_t lParam,
    uintptr_t userData
);
```

事件类型：

| 值 | 名称 | 说明 |
| --- | --- | --- |
| 1 | `LOGVIEW_EVENT_CLOSE_CLICKED` | 点击关闭按钮 |
| 2 | `LOGVIEW_EVENT_CLEAR_CLICKED` | 点击清空按钮 |
| 3 | `LOGVIEW_EVENT_MOVED` | 窗口移动 |
| 4 | `LOGVIEW_EVENT_RESIZED` | 窗口大小变化 |
| 5 | `LOGVIEW_EVENT_DESTROYED` | 窗口销毁 |
| 6 | `LOGVIEW_EVENT_LOG_LINE_CLICKED` | 点击日志行 |
| 7 | `LOGVIEW_EVENT_SCROLL_CHANGED` | 滚动位置变化 |

Windows 消息通知规则：

```cpp
PostMessage(hwnd, messageId, eventType, handle);
```

## 日志等级

| 值 | 名称 | 说明 |
| --- | --- | --- |
| 0 | `LOGVIEW_LEVEL_INFO` | 普通日志 |
| 1 | `LOGVIEW_LEVEL_SUCCESS` | 成功日志 |
| 2 | `LOGVIEW_LEVEL_WARNING` | 警告日志 |
| 3 | `LOGVIEW_LEVEL_ERROR` | 错误日志 |
| 4 | `LOGVIEW_LEVEL_DEBUG` | 调试日志 |
| 5 | `LOGVIEW_LEVEL_CUSTOM` | 自定义日志 |

## 关闭模式

| 值 | 名称 | 说明 |
| --- | --- | --- |
| 0 | `LOGVIEW_CLOSE_HIDE` | 点击关闭按钮时隐藏窗口 |
| 1 | `LOGVIEW_CLOSE_DESTROY` | 点击关闭按钮时销毁窗口 |
| 2 | `LOGVIEW_CLOSE_NOTIFY_ONLY` | 点击关闭按钮时只通知调用方 |

## 推荐调用流程

独立悬浮窗口：

```text
LogView_Init
LogView_CreatePopupEx
LogView_SetTitle
LogView_Show
LogView_AddLine / LogView_AddLineEx
LogView_Destroy
LogView_Uninit
```

内嵌窗口：

```text
LogView_Init
LogView_CreateChildEx
LogView_SetTitle
LogView_Show
LogView_AddLine / LogView_AddLineEx
父窗口大小变化时调用 LogView_SetRect
LogView_Destroy
LogView_Uninit
```

## C/C++ 调用方法

```cpp
#include "LogView.h"

int main() {
    LogView_Init();

    LogViewHandle log = LogView_CreatePopupEx(520, 420, 180);
    LogView_SetTitle(log, u8"C++ 日志窗口");
    LogView_Show(log);
    LogView_AddLine(log, u8"普通日志");
    LogView_AddLineEx(log, u8"错误日志", 0xFFFF5555, LOGVIEW_LEVEL_ERROR);

    // 程序退出前销毁
    LogView_Destroy(log);
    LogView_Uninit();
    return 0;
}
```

## C# 调用方法

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
    public static extern UIntPtr LogView_CreateChildEx(UIntPtr parentHwnd, int x, int y, int width, int height, int alpha);

    [DllImport(DllName, CallingConvention = CallingConvention.StdCall)]
    public static extern int LogView_SetTitle(UIntPtr handle, byte[] text);

    [DllImport(DllName, CallingConvention = CallingConvention.StdCall)]
    public static extern int LogView_Show(UIntPtr handle);

    [DllImport(DllName, CallingConvention = CallingConvention.StdCall)]
    public static extern int LogView_AddLine(UIntPtr handle, byte[] text);

    [DllImport(DllName, CallingConvention = CallingConvention.StdCall)]
    public static extern int LogView_Destroy(UIntPtr handle);

    private static byte[] Utf8(string value) => Encoding.UTF8.GetBytes(value + "\0");
}
```

C# 重点：

- `CallingConvention.StdCall` 必须指定。
- 64 位程序使用 `LogView_x64.dll`。
- 32 位程序使用 `LogView_x86.dll`。
- 中文字符串建议用 `Encoding.UTF8.GetBytes(text + "\0")`。
- WinForms 内嵌窗口传 `Panel.Handle`。

## Python ctypes 调用方法

```python
import ctypes
import time

dll = ctypes.WinDLL(r"LogView_x64.dll")

dll.LogView_Init.restype = ctypes.c_int
dll.LogView_CreatePopupEx.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int]
dll.LogView_CreatePopupEx.restype = ctypes.c_size_t
dll.LogView_SetTitle.argtypes = [ctypes.c_size_t, ctypes.c_char_p]
dll.LogView_SetTitle.restype = ctypes.c_int
dll.LogView_Show.argtypes = [ctypes.c_size_t]
dll.LogView_Show.restype = ctypes.c_int
dll.LogView_AddLine.argtypes = [ctypes.c_size_t, ctypes.c_char_p]
dll.LogView_AddLine.restype = ctypes.c_int
dll.LogView_Destroy.argtypes = [ctypes.c_size_t]
dll.LogView_Destroy.restype = ctypes.c_int
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

Python 重点：

- 使用 `ctypes.WinDLL`，因为 DLL 是 `__stdcall`。
- 64 位 Python 加载 `LogView_x64.dll`。
- 32 位 Python 加载 `LogView_x86.dll`。
- 字符串传 `bytes`，编码用 UTF-8。
- 句柄用 `ctypes.c_size_t` 或 `ctypes.c_uint64`/`ctypes.c_uint32` 按位数处理。

## 易语言调用方法

易语言声明时注意：

- DLL 调用约定选择 `stdcall`。
- 32 位易语言程序使用 `LogView_x86.dll`。
- 如果使用 64 位环境，使用 `LogView_x64.dll`。
- 字符串应按 UTF-8 传入；如果易语言内部是 ANSI/Unicode，需要先转换 UTF-8 字节。
- `LogViewHandle` 可以按整数型/长整数型保存，按程序位数选择合适类型。

示例声明思路：

```text
DLL命令 LogView_Init, 整数型, "LogView_x86.dll", "LogView_Init"
DLL命令 LogView_Uninit, 整数型, "LogView_x86.dll", "LogView_Uninit"
DLL命令 LogView_CreatePopupEx, 整数型, "LogView_x86.dll", "LogView_CreatePopupEx", 整数型, 整数型, 整数型
DLL命令 LogView_SetTitle, 整数型, "LogView_x86.dll", "LogView_SetTitle", 整数型, 文本型
DLL命令 LogView_Show, 整数型, "LogView_x86.dll", "LogView_Show", 整数型
DLL命令 LogView_AddLine, 整数型, "LogView_x86.dll", "LogView_AddLine", 整数型, 文本型
DLL命令 LogView_Destroy, 整数型, "LogView_x86.dll", "LogView_Destroy", 整数型
```

易语言推荐流程：

```text
LogView_Init()
句柄 = LogView_CreatePopupEx(520, 420, 180)
LogView_SetTitle(句柄, UTF8文本("易语言 日志窗口"))
LogView_Show(句柄)
LogView_AddLine(句柄, UTF8文本("普通日志"))
程序退出时 LogView_Destroy(句柄)
程序退出时 LogView_Uninit()
```

## 火山视窗调用方法

火山调用时注意：

- DLL 调用约定使用标准调用。
- 32 位火山程序加载 `LogView_x86.dll`。
- 64 位火山程序加载 `LogView_x64.dll`。
- HWND 父窗口句柄传给 `LogView_CreateChildEx` 即可实现内嵌。
- 中文字符串统一转 UTF-8 后传给 DLL。

火山声明思路：

```text
LogView_Init() -> 整数
LogView_Uninit() -> 整数
LogView_CreatePopupEx(宽度: 整数, 高度: 整数, 透明度: 整数) -> 整数/长整数
LogView_CreateChildEx(父窗口句柄: 整数/长整数, x: 整数, y: 整数, 宽度: 整数, 高度: 整数, 透明度: 整数) -> 整数/长整数
LogView_SetTitle(日志句柄: 整数/长整数, 标题UTF8: 文本/字节集) -> 整数
LogView_Show(日志句柄: 整数/长整数) -> 整数
LogView_AddLine(日志句柄: 整数/长整数, 内容UTF8: 文本/字节集) -> 整数
LogView_Destroy(日志句柄: 整数/长整数) -> 整数
```

## 全部 DLL 函数

### 初始化

| 函数 | 返回 | 参数 | 说明 |
| --- | --- | --- | --- |
| `LogView_Init` | `int` | 无 | 初始化 DLL 内部环境 |
| `LogView_Uninit` | `int` | 无 | 释放 DLL 全局资源 |

### 创建与销毁

| 函数 | 返回 | 参数 | 说明 |
| --- | --- | --- | --- |
| `LogView_CreatePopup` | `LogViewHandle` | `int width, int height` | 创建独立悬浮日志框，透明度使用默认 128 |
| `LogView_CreatePopupEx` | `LogViewHandle` | `int width, int height, int alpha` | 创建独立悬浮日志框并指定透明度 |
| `LogView_CreateChild` | `LogViewHandle` | `LogViewHwnd parentHwnd, int x, int y, int width, int height` | 创建内嵌日志框，透明度使用默认 128 |
| `LogView_CreateChildEx` | `LogViewHandle` | `LogViewHwnd parentHwnd, int x, int y, int width, int height, int alpha` | 创建内嵌日志框并指定透明度 |
| `LogView_Destroy` | `int` | `LogViewHandle handle` | 销毁指定日志框 |
| `LogView_DestroyAll` | `int` | 无 | 销毁当前进程内所有日志框 |

### 显示控制

| 函数 | 返回 | 参数 | 说明 |
| --- | --- | --- | --- |
| `LogView_Show` | `int` | `LogViewHandle handle` | 显示日志框 |
| `LogView_Hide` | `int` | `LogViewHandle handle` | 隐藏日志框 |
| `LogView_Close` | `int` | `LogViewHandle handle` | 按关闭模式执行关闭动作 |
| `LogView_IsVisible` | `int` | `LogViewHandle handle` | 判断窗口是否可见 |
| `LogView_SetCloseMode` | `int` | `LogViewHandle handle, int mode` | 设置关闭按钮行为 |

### 窗口位置与大小

| 函数 | 返回 | 参数 | 说明 |
| --- | --- | --- | --- |
| `LogView_SetPosition` | `int` | `LogViewHandle handle, int x, int y` | 设置窗口位置 |
| `LogView_SetSize` | `int` | `LogViewHandle handle, int width, int height` | 设置窗口大小 |
| `LogView_SetRect` | `int` | `LogViewHandle handle, int x, int y, int width, int height` | 同时设置位置和大小 |
| `LogView_GetRect` | `int` | `LogViewHandle handle, int* outX, int* outY, int* outWidth, int* outHeight` | 获取窗口矩形 |
| `LogView_CenterScreen` | `int` | `LogViewHandle handle` | 居中到屏幕 |
| `LogView_EnableDrag` | `int` | `LogViewHandle handle, int enable` | 启用或禁用拖动 |

### 嵌入与弹出切换

| 函数 | 返回 | 参数 | 说明 |
| --- | --- | --- | --- |
| `LogView_AttachToParent` | `int` | `LogViewHandle handle, LogViewHwnd parentHwnd, int x, int y, int width, int height` | 嵌入到指定父窗口 |
| `LogView_DetachToDesktop` | `int` | `LogViewHandle handle` | 从父窗口脱离，变成桌面悬浮窗口 |
| `LogView_GetWindowHandle` | `LogViewHwnd` | `LogViewHandle handle` | 获取真实 HWND |

### 置顶与透明

| 函数 | 返回 | 参数 | 说明 |
| --- | --- | --- | --- |
| `LogView_SetTopMost` | `int` | `LogViewHandle handle, int enable` | 设置是否置顶 |
| `LogView_SetAlpha` | `int` | `LogViewHandle handle, int alpha` | 设置背景透明度，范围 0-255 |
| `LogView_SetBackgroundColor` | `int` | `LogViewHandle handle, LogViewColor color` | 设置背景颜色 |
| `LogView_SetBorderColor` | `int` | `LogViewHandle handle, LogViewColor color` | 设置边框颜色 |
| `LogView_SetRoundCorner` | `int` | `LogViewHandle handle, int radius` | 设置圆角半径 |

### 标题与按钮

| 函数 | 返回 | 参数 | 说明 |
| --- | --- | --- | --- |
| `LogView_SetTitle` | `int` | `LogViewHandle handle, const char* text` | 设置标题 |
| `LogView_ShowTitleBar` | `int` | `LogViewHandle handle, int enable` | 显示或隐藏标题区域 |
| `LogView_ShowCloseButton` | `int` | `LogViewHandle handle, int enable` | 显示或隐藏关闭按钮 |
| `LogView_ShowClearButton` | `int` | `LogViewHandle handle, int enable` | 显示或隐藏清空按钮 |
| `LogView_SetClearButtonText` | `int` | `LogViewHandle handle, const char* text` | 设置清空按钮文字 |
| `LogView_SetCloseButtonText` | `int` | `LogViewHandle handle, const char* text` | 设置关闭按钮文字或图标文本 |

### 滚动条

| 函数 | 返回 | 参数 | 说明 |
| --- | --- | --- | --- |
| `LogView_ShowScrollBar` | `int` | `LogViewHandle handle, int enable` | 显示或隐藏右侧纵向滚动条 |
| `LogView_SetAutoScroll` | `int` | `LogViewHandle handle, int enable` | 添加日志后是否自动滚动到底部 |
| `LogView_ScrollToTop` | `int` | `LogViewHandle handle` | 滚动到顶部 |
| `LogView_ScrollToBottom` | `int` | `LogViewHandle handle` | 滚动到底部 |
| `LogView_SetScrollPosition` | `int` | `LogViewHandle handle, int position` | 设置滚动位置 |
| `LogView_GetScrollPosition` | `int` | `LogViewHandle handle` | 获取当前滚动位置 |

### 日志输出

| 函数 | 返回 | 参数 | 说明 |
| --- | --- | --- | --- |
| `LogView_AddText` | `int` | `LogViewHandle handle, const char* text` | 追加普通文本 |
| `LogView_AddLine` | `int` | `LogViewHandle handle, const char* text` | 追加一行日志 |
| `LogView_AddLineEx` | `int` | `LogViewHandle handle, const char* text, LogViewColor color, int level` | 追加带颜色和等级的日志 |
| `LogView_AddColoredLine` | `int` | `LogViewHandle handle, LogViewColor timeColor, LogViewColor prefixColor, LogViewColor textColor, const char* text` | 追加时间、分隔符、正文分别着色的日志 |
| `LogView_Clear` | `int` | `LogViewHandle handle` | 清空日志 |
| `LogView_GetLineCount` | `int` | `LogViewHandle handle` | 获取当前日志行数 |
| `LogView_GetText` | `int` | `LogViewHandle handle, char* buffer, int bufferSize` | 获取当前日志文本，返回写入长度 |
| `LogView_SaveToFile` | `int` | `LogViewHandle handle, const char* path` | 保存当前日志到文件 |
| `LogView_SetMaxLines` | `int` | `LogViewHandle handle, int maxLines` | 设置最大日志行数 |

### 日志样式

| 函数 | 返回 | 参数 | 说明 |
| --- | --- | --- | --- |
| `LogView_SetFont` | `int` | `LogViewHandle handle, const char* fontName, int fontSize` | 设置字体和字号 |
| `LogView_SetTextColor` | `int` | `LogViewHandle handle, LogViewColor color` | 设置默认文字颜色 |
| `LogView_SetTimeColor` | `int` | `LogViewHandle handle, LogViewColor color` | 设置时间颜色 |
| `LogView_SetInfoColor` | `int` | `LogViewHandle handle, LogViewColor color` | 设置普通日志颜色 |
| `LogView_SetSuccessColor` | `int` | `LogViewHandle handle, LogViewColor color` | 设置成功日志颜色 |
| `LogView_SetWarningColor` | `int` | `LogViewHandle handle, LogViewColor color` | 设置警告日志颜色 |
| `LogView_SetErrorColor` | `int` | `LogViewHandle handle, LogViewColor color` | 设置错误日志颜色 |
| `LogView_SetDebugColor` | `int` | `LogViewHandle handle, LogViewColor color` | 设置调试日志颜色 |
| `LogView_ShowTime` | `int` | `LogViewHandle handle, int enable` | 是否显示时间 |
| `LogView_SetTimeFormat` | `int` | `LogViewHandle handle, const char* format` | 设置时间格式 |

### 布局设置

| 函数 | 返回 | 参数 | 说明 |
| --- | --- | --- | --- |
| `LogView_SetPadding` | `int` | `LogViewHandle handle, int left, int top, int right, int bottom` | 设置内容边距 |
| `LogView_SetLineHeight` | `int` | `LogViewHandle handle, int height` | 设置日志行高 |
| `LogView_SetHeaderHeight` | `int` | `LogViewHandle handle, int height` | 设置标题区域高度 |
| `LogView_SetButtonAreaHeight` | `int` | `LogViewHandle handle, int height` | 设置底部按钮区域高度 |
| `LogView_SetScrollBarWidth` | `int` | `LogViewHandle handle, int width` | 设置滚动条宽度 |

### 事件回调

| 函数 | 返回 | 参数 | 说明 |
| --- | --- | --- | --- |
| `LogView_SetCallback` | `int` | `LogViewHandle handle, LogViewCallback callback, uintptr_t userData` | 设置事件回调 |
| `LogView_SetNotifyWindow` | `int` | `LogViewHandle handle, LogViewHwnd hwnd, int messageId` | 设置事件通知窗口 |

### 配置与状态

| 函数 | 返回 | 参数 | 说明 |
| --- | --- | --- | --- |
| `LogView_SetUserData` | `int` | `LogViewHandle handle, uintptr_t userData` | 绑定调用方自定义数据 |
| `LogView_GetUserData` | `uintptr_t` | `LogViewHandle handle` | 获取调用方自定义数据 |
| `LogView_SetName` | `int` | `LogViewHandle handle, const char* name` | 设置实例名称 |
| `LogView_FindByName` | `LogViewHandle` | `const char* name` | 按名称查找实例 |
| `LogView_GetCount` | `int` | 无 | 获取当前日志框数量 |
| `LogView_IsValid` | `int` | `LogViewHandle handle` | 判断句柄是否有效 |

### 错误处理与版本

| 函数 | 返回 | 参数 | 说明 |
| --- | --- | --- | --- |
| `LogView_GetLastErrorCode` | `int` | 无 | 获取最后错误码 |
| `LogView_GetLastErrorText` | `const char*` | 无 | 获取最后错误文本，UTF-8 |
| `LogView_GetVersion` | `const char*` | 无 | 获取 DLL 版本字符串 |

## 常见问题

### 为什么 C# 或 Python 只能看到终端，看不到窗口？

确认调用方位数和 DLL 位数一致，并确认程序没有马上退出。窗口创建后，主程序需要保持运行。

### 为什么中文乱码？

DLL 接收 UTF-8。调用方需要传 UTF-8 字节，而不是 ANSI、GBK 或 UTF-16。

### 为什么内嵌窗口不显示？

确认父窗口 HWND 已经创建。WinForms 中应在 `Shown`、`Load` 后或控件 `HandleCreated` 后再创建内嵌日志框。

### 为什么文字不能跟着背景一起透明？

为了文字清晰，LogView 只让背景半透明，文字保持不透明绘制。

### 如何多开？

每次调用 `LogView_CreatePopupEx` 或 `LogView_CreateChildEx` 都会返回一个独立句柄。后续所有操作都传对应句柄即可。

