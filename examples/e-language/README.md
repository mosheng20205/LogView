# 易语言调用 LogView DLL 参考

本目录提供一个可直接照着导入/粘贴的示例：

- `LogViewExample.e.txt`：易语言窗口程序集示例，包含 DLL 命令声明、初始化、创建悬浮日志窗、追加日志、清空、隐藏/显示和退出清理。

## 位数要求

- 32 位易语言程序加载 `dist/x86/LogView_x86.dll`。
- 64 位易语言程序加载 `dist/x64/LogView_x64.dll`。
- 程序位数和 DLL 位数必须一致。

## 类型映射

| DLL 类型 | 易语言建议类型 |
| --- | --- |
| `LogViewHandle` | 整数型/长整数，按编译位数选择 |
| `LogViewHwnd` | 整数型/长整数，按编译位数选择 |
| `int` | 整数型 |
| `uint32_t` 颜色 | 整数型 |
| `const char*` | 文本型，按 UTF-8 传入 |

## 运行示例

1. 在易语言中新建一个 Windows 窗口程序。
2. 把 `LogViewExample.e.txt` 中的 DLL 命令声明和子程序复制到窗口程序集。
3. 在窗口上放 3 个按钮，并把名称改成 `按钮_追加日志`、`按钮_清空日志`、`按钮_隐藏显示`；不需要按钮时，可以只保留 `__启动窗口_创建完毕` 和 `__启动窗口_将被销毁`。
4. 32 位程序把 `dist/x86/LogView_x86.dll` 复制到生成的 EXE 同目录；64 位程序改用 `dist/x64/LogView_x64.dll`，同时把示例中的 DLL 名和句柄类型改成 64 位对应类型。

## 最小调用流程

```text
LogView_Init()
句柄 = LogView_CreatePopupEx(460, 360, 180)
LogView_SetTitle(句柄, "易语言日志窗口")
LogView_Show(句柄)
LogView_AddLine(句柄, "窗口启动")
LogView_AddLineEx(句柄, "成功日志", 0xFF38D27A, 1)
LogView_Clear(句柄)
LogView_Destroy(句柄)
LogView_Uninit()
```

## 重要声明

- DLL 导出名已经固定为未修饰名称，例如 `LogView_CreatePopupEx`。
- 调用约定使用 `stdcall`。
- 所有文本建议转成 UTF-8 后传入。
- 示例中的彩色日志颜色使用 32 位有符号整数传参，底层位模式对应 `0xAARRGGBB`。
- 创建多个日志框时，保存每个创建命令返回的 `LogViewHandle`，之后所有操作都传对应句柄。
