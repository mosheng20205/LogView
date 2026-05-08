# 火山视窗调用 LogView DLL 参考

本说明面向火山视窗。已按本地火山视窗手册 `vol_win_full.md` 检索 DLL/动态库声明语法，但没有找到完整的外部 DLL 声明章节；因此这里提供的是 LogView 的跨语言 ABI 映射和调用流程参考，具体导入界面/语法请以火山视窗 IDE 的 DLL 导入功能为准。

## 位数要求

- 32 位火山程序加载 `dist/x86/LogView_x86.dll`。
- 64 位火山程序加载 `dist/x64/LogView_x64.dll`。
- 程序位数和 DLL 位数必须一致。

## 类型映射

| DLL 类型 | 火山视窗建议类型 |
| --- | --- |
| `LogViewHandle` | 整数/长整数，按目标位数选择 |
| `LogViewHwnd` | 整数/长整数，按目标位数选择 |
| `int` | 整数 |
| `uint32_t` 颜色 | 整数 |
| `const char*` | 文本，调用前按 UTF-8 编码 |

## 最小调用流程

```text
LogView_Init()
句柄 = LogView_CreatePopupEx(460, 360, 180)
LogView_SetTitle(句柄, "火山日志窗口")
LogView_Show(句柄)
LogView_AddLine(句柄, "窗口启动")
LogView_AddLineEx(句柄, "警告日志", 0xFFF59E0B, 2)
LogView_Destroy(句柄)
LogView_Uninit()
```

## 导入注意

- 调用约定选择 `stdcall`。
- 函数名直接填写未修饰名称，例如 `LogView_AddLine`。
- 多开日志框时，每个窗口保存自己的 `LogViewHandle`。
- 嵌入模式使用 `LogView_CreateChild` 或 `LogView_CreateChildEx`，父窗口参数传火山窗口的 HWND。

