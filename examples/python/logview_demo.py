import ctypes
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
DLL_PATH = ROOT / "dist" / ("x64" if ctypes.sizeof(ctypes.c_void_p) == 8 else "x86")
DLL_PATH = DLL_PATH / ("LogView_x64.dll" if ctypes.sizeof(ctypes.c_void_p) == 8 else "LogView_x86.dll")

Handle = ctypes.c_size_t
Hwnd = ctypes.c_size_t
Color = ctypes.c_uint32

logview = ctypes.WinDLL(str(DLL_PATH))

logview.LogView_Init.restype = ctypes.c_int
logview.LogView_Uninit.restype = ctypes.c_int
logview.LogView_CreatePopupEx.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int]
logview.LogView_CreatePopupEx.restype = Handle
logview.LogView_Destroy.argtypes = [Handle]
logview.LogView_Destroy.restype = ctypes.c_int
logview.LogView_Show.argtypes = [Handle]
logview.LogView_Show.restype = ctypes.c_int
logview.LogView_Hide.argtypes = [Handle]
logview.LogView_Hide.restype = ctypes.c_int
logview.LogView_SetTitle.argtypes = [Handle, ctypes.c_char_p]
logview.LogView_AddLine.argtypes = [Handle, ctypes.c_char_p]
logview.LogView_AddLineEx.argtypes = [Handle, ctypes.c_char_p, Color, ctypes.c_int]
logview.LogView_Clear.argtypes = [Handle]
logview.LogView_GetLineCount.argtypes = [Handle]
logview.LogView_GetLineCount.restype = ctypes.c_int
logview.LogView_GetLastErrorText.restype = ctypes.c_char_p


def b(text: str) -> bytes:
    return text.encode("utf-8")


def check(ok: int) -> None:
    if not ok:
        raise RuntimeError(logview.LogView_GetLastErrorText().decode("utf-8", errors="replace"))


def main() -> None:
    check(logview.LogView_Init())

    first = logview.LogView_CreatePopupEx(460, 360, 180)
    second = logview.LogView_CreatePopupEx(420, 300, 150)
    if not first or not second:
        raise RuntimeError(logview.LogView_GetLastErrorText().decode("utf-8", errors="replace"))

    check(logview.LogView_SetTitle(first, b("Python 日志窗口 A")))
    check(logview.LogView_SetTitle(second, b("Python 日志窗口 B")))
    check(logview.LogView_Show(first))
    check(logview.LogView_Show(second))

    for i in range(120):
        check(logview.LogView_AddLine(first, b(f"普通日志 {i}")))
        check(logview.LogView_AddLineEx(second, b(f"成功日志 {i}"), 0xFF38D27A, 1))
        time.sleep(0.02)

    print("first line count:", logview.LogView_GetLineCount(first))
    input("按回车关闭日志窗口...")

    check(logview.LogView_Clear(first))
    check(logview.LogView_Destroy(first))
    check(logview.LogView_Destroy(second))
    check(logview.LogView_Uninit())


if __name__ == "__main__":
    main()
