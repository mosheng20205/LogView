"""
Full LogView DLL demo for Python.

This file intentionally exercises every exported LogView_* function in the DLL:
popup windows, embedded child windows, styling, scrolling, callbacks, Win32
notifications, text export, file export, name lookup, validity checks, and
cleanup.

Run with:
    python examples/python/logview_full_demo.py
"""

from __future__ import annotations

import ctypes
import sys
import time
from ctypes import wintypes
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
ARCH = "x64" if ctypes.sizeof(ctypes.c_void_p) == 8 else "x86"
DLL_PATH = ROOT / "dist" / ARCH / f"LogView_{ARCH}.dll"
EXPORT_PATH = Path(__file__).with_name("logview_python_export.txt")

Handle = ctypes.c_size_t
Hwnd = ctypes.c_size_t
Color = ctypes.c_uint32
UIntPtr = ctypes.c_size_t

LOGVIEW_LEVEL_INFO = 0
LOGVIEW_LEVEL_SUCCESS = 1
LOGVIEW_LEVEL_WARNING = 2
LOGVIEW_LEVEL_ERROR = 3
LOGVIEW_LEVEL_DEBUG = 4
LOGVIEW_LEVEL_CUSTOM = 5

LOGVIEW_CLOSE_HIDE = 0
LOGVIEW_CLOSE_DESTROY = 1
LOGVIEW_CLOSE_NOTIFY_ONLY = 2

LOGVIEW_EVENT_NAMES = {
    1: "close_clicked",
    2: "clear_clicked",
    3: "moved",
    4: "resized",
    5: "destroyed",
    6: "log_line_clicked",
    7: "scroll_changed",
}

NOTIFY_MESSAGE = 0x8000 + 0x5044

EXPORTED_FUNCTIONS = {
    "LogView_Init",
    "LogView_Uninit",
    "LogView_CreatePopup",
    "LogView_CreatePopupEx",
    "LogView_CreateChild",
    "LogView_CreateChildEx",
    "LogView_Destroy",
    "LogView_DestroyAll",
    "LogView_Show",
    "LogView_Hide",
    "LogView_Close",
    "LogView_IsVisible",
    "LogView_SetCloseMode",
    "LogView_SetPosition",
    "LogView_SetSize",
    "LogView_SetRect",
    "LogView_GetRect",
    "LogView_CenterScreen",
    "LogView_EnableDrag",
    "LogView_AttachToParent",
    "LogView_DetachToDesktop",
    "LogView_GetWindowHandle",
    "LogView_SetTopMost",
    "LogView_SetAlpha",
    "LogView_SetBackgroundColor",
    "LogView_SetBorderColor",
    "LogView_SetRoundCorner",
    "LogView_SetTitle",
    "LogView_ShowTitleBar",
    "LogView_ShowCloseButton",
    "LogView_ShowClearButton",
    "LogView_SetClearButtonText",
    "LogView_SetCloseButtonText",
    "LogView_ShowScrollBar",
    "LogView_SetAutoScroll",
    "LogView_ScrollToTop",
    "LogView_ScrollToBottom",
    "LogView_SetScrollPosition",
    "LogView_GetScrollPosition",
    "LogView_AddText",
    "LogView_AddLine",
    "LogView_AddLineEx",
    "LogView_AddColoredLine",
    "LogView_Clear",
    "LogView_GetLineCount",
    "LogView_GetText",
    "LogView_SaveToFile",
    "LogView_SetMaxLines",
    "LogView_SetFont",
    "LogView_SetTextColor",
    "LogView_SetTimeColor",
    "LogView_SetInfoColor",
    "LogView_SetSuccessColor",
    "LogView_SetWarningColor",
    "LogView_SetErrorColor",
    "LogView_SetDebugColor",
    "LogView_ShowTime",
    "LogView_SetTimeFormat",
    "LogView_SetPadding",
    "LogView_SetLineHeight",
    "LogView_SetHeaderHeight",
    "LogView_SetButtonAreaHeight",
    "LogView_SetScrollBarWidth",
    "LogView_SetCallback",
    "LogView_SetNotifyWindow",
    "LogView_SetUserData",
    "LogView_GetUserData",
    "LogView_SetName",
    "LogView_FindByName",
    "LogView_GetCount",
    "LogView_IsValid",
    "LogView_GetLastErrorCode",
    "LogView_GetLastErrorText",
    "LogView_GetVersion",
}


def utf8(text: str | Path) -> bytes:
    return str(text).encode("utf-8")


def as_int(value: object) -> int:
    if isinstance(value, int):
        return value
    raw = getattr(value, "value", value)
    return 0 if raw is None else int(raw)


class RECT(ctypes.Structure):
    _fields_ = [
        ("left", ctypes.c_long),
        ("top", ctypes.c_long),
        ("right", ctypes.c_long),
        ("bottom", ctypes.c_long),
    ]


class PAINTSTRUCT(ctypes.Structure):
    _fields_ = [
        ("hdc", wintypes.HDC),
        ("fErase", wintypes.BOOL),
        ("rcPaint", RECT),
        ("fRestore", wintypes.BOOL),
        ("fIncUpdate", wintypes.BOOL),
        ("rgbReserved", ctypes.c_byte * 32),
    ]


WNDPROC = ctypes.WINFUNCTYPE(wintypes.LPARAM, wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM)


class WNDCLASSW(ctypes.Structure):
    _fields_ = [
        ("style", wintypes.UINT),
        ("lpfnWndProc", WNDPROC),
        ("cbClsExtra", ctypes.c_int),
        ("cbWndExtra", ctypes.c_int),
        ("hInstance", wintypes.HINSTANCE),
        ("hIcon", wintypes.HANDLE),
        ("hCursor", wintypes.HANDLE),
        ("hbrBackground", wintypes.HANDLE),
        ("lpszMenuName", wintypes.LPCWSTR),
        ("lpszClassName", wintypes.LPCWSTR),
    ]


class MSG(ctypes.Structure):
    _fields_ = [
        ("hwnd", wintypes.HWND),
        ("message", wintypes.UINT),
        ("wParam", wintypes.WPARAM),
        ("lParam", wintypes.LPARAM),
        ("time", wintypes.DWORD),
        ("pt_x", wintypes.LONG),
        ("pt_y", wintypes.LONG),
    ]


user32 = ctypes.WinDLL("user32", use_last_error=True)
gdi32 = ctypes.WinDLL("gdi32", use_last_error=True)
kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

user32.RegisterClassW.argtypes = [ctypes.POINTER(WNDCLASSW)]
user32.RegisterClassW.restype = wintypes.ATOM
user32.CreateWindowExW.argtypes = [
    wintypes.DWORD,
    wintypes.LPCWSTR,
    wintypes.LPCWSTR,
    wintypes.DWORD,
    ctypes.c_int,
    ctypes.c_int,
    ctypes.c_int,
    ctypes.c_int,
    wintypes.HWND,
    wintypes.HMENU,
    wintypes.HINSTANCE,
    wintypes.LPVOID,
]
user32.CreateWindowExW.restype = wintypes.HWND
user32.DefWindowProcW.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM]
user32.DefWindowProcW.restype = wintypes.LPARAM
user32.ShowWindow.argtypes = [wintypes.HWND, ctypes.c_int]
user32.ShowWindow.restype = wintypes.BOOL
user32.UpdateWindow.argtypes = [wintypes.HWND]
user32.UpdateWindow.restype = wintypes.BOOL
user32.DestroyWindow.argtypes = [wintypes.HWND]
user32.DestroyWindow.restype = wintypes.BOOL
user32.PostQuitMessage.argtypes = [ctypes.c_int]
user32.BeginPaint.argtypes = [wintypes.HWND, ctypes.POINTER(PAINTSTRUCT)]
user32.BeginPaint.restype = wintypes.HDC
user32.EndPaint.argtypes = [wintypes.HWND, ctypes.POINTER(PAINTSTRUCT)]
user32.FillRect.argtypes = [wintypes.HDC, ctypes.POINTER(RECT), wintypes.HBRUSH]
user32.PeekMessageW.argtypes = [ctypes.POINTER(MSG), wintypes.HWND, wintypes.UINT, wintypes.UINT, wintypes.UINT]
user32.PeekMessageW.restype = wintypes.BOOL
user32.TranslateMessage.argtypes = [ctypes.POINTER(MSG)]
user32.DispatchMessageW.argtypes = [ctypes.POINTER(MSG)]
user32.GetClientRect.argtypes = [wintypes.HWND, ctypes.POINTER(RECT)]
user32.GetClientRect.restype = wintypes.BOOL
user32.InvalidateRect.argtypes = [wintypes.HWND, ctypes.POINTER(RECT), wintypes.BOOL]
user32.SetTimer.argtypes = [wintypes.HWND, ctypes.c_size_t, wintypes.UINT, ctypes.c_void_p]
user32.KillTimer.argtypes = [wintypes.HWND, ctypes.c_size_t]
user32.PostMessageW.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM]

kernel32.GetModuleHandleW.argtypes = [wintypes.LPCWSTR]
kernel32.GetModuleHandleW.restype = wintypes.HMODULE

gdi32.CreateSolidBrush.argtypes = [wintypes.COLORREF]
gdi32.CreateSolidBrush.restype = wintypes.HBRUSH
gdi32.CreatePen.argtypes = [ctypes.c_int, ctypes.c_int, wintypes.COLORREF]
gdi32.CreatePen.restype = wintypes.HPEN
gdi32.SelectObject.argtypes = [wintypes.HDC, wintypes.HGDIOBJ]
gdi32.SelectObject.restype = wintypes.HGDIOBJ
gdi32.DeleteObject.argtypes = [wintypes.HGDIOBJ]
gdi32.MoveToEx.argtypes = [wintypes.HDC, ctypes.c_int, ctypes.c_int, ctypes.c_void_p]
gdi32.LineTo.argtypes = [wintypes.HDC, ctypes.c_int, ctypes.c_int]


def rgb(r: int, g: int, b: int) -> int:
    return r | (g << 8) | (b << 16)


class LogViewApi:
    def __init__(self, dll_path: Path) -> None:
        if not dll_path.exists():
            raise FileNotFoundError(f"LogView DLL not found: {dll_path}")
        self.dll = ctypes.WinDLL(str(dll_path))
        self.covered: set[str] = set()
        self._bind_all()

    def _bind(self, name: str, restype: object, argtypes: list[object] | None = None) -> None:
        fn = getattr(self.dll, name)
        fn.restype = restype
        if argtypes is not None:
            fn.argtypes = argtypes

    def _bind_all(self) -> None:
        cb_type = ctypes.WINFUNCTYPE(None, Handle, ctypes.c_int, UIntPtr, UIntPtr, UIntPtr)
        self.callback_type = cb_type

        self._bind("LogView_Init", ctypes.c_int, [])
        self._bind("LogView_Uninit", ctypes.c_int, [])
        self._bind("LogView_CreatePopup", Handle, [ctypes.c_int, ctypes.c_int])
        self._bind("LogView_CreatePopupEx", Handle, [ctypes.c_int, ctypes.c_int, ctypes.c_int])
        self._bind("LogView_CreateChild", Handle, [Hwnd, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int])
        self._bind("LogView_CreateChildEx", Handle, [Hwnd, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int])
        self._bind("LogView_Destroy", ctypes.c_int, [Handle])
        self._bind("LogView_DestroyAll", ctypes.c_int, [])
        self._bind("LogView_Show", ctypes.c_int, [Handle])
        self._bind("LogView_Hide", ctypes.c_int, [Handle])
        self._bind("LogView_Close", ctypes.c_int, [Handle])
        self._bind("LogView_IsVisible", ctypes.c_int, [Handle])
        self._bind("LogView_SetCloseMode", ctypes.c_int, [Handle, ctypes.c_int])
        self._bind("LogView_SetPosition", ctypes.c_int, [Handle, ctypes.c_int, ctypes.c_int])
        self._bind("LogView_SetSize", ctypes.c_int, [Handle, ctypes.c_int, ctypes.c_int])
        self._bind("LogView_SetRect", ctypes.c_int, [Handle, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int])
        self._bind("LogView_GetRect", ctypes.c_int, [Handle, ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_int)])
        self._bind("LogView_CenterScreen", ctypes.c_int, [Handle])
        self._bind("LogView_EnableDrag", ctypes.c_int, [Handle, ctypes.c_int])
        self._bind("LogView_AttachToParent", ctypes.c_int, [Handle, Hwnd, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int])
        self._bind("LogView_DetachToDesktop", ctypes.c_int, [Handle])
        self._bind("LogView_GetWindowHandle", Hwnd, [Handle])
        self._bind("LogView_SetTopMost", ctypes.c_int, [Handle, ctypes.c_int])
        self._bind("LogView_SetAlpha", ctypes.c_int, [Handle, ctypes.c_int])
        self._bind("LogView_SetBackgroundColor", ctypes.c_int, [Handle, Color])
        self._bind("LogView_SetBorderColor", ctypes.c_int, [Handle, Color])
        self._bind("LogView_SetRoundCorner", ctypes.c_int, [Handle, ctypes.c_int])
        self._bind("LogView_SetTitle", ctypes.c_int, [Handle, ctypes.c_char_p])
        self._bind("LogView_ShowTitleBar", ctypes.c_int, [Handle, ctypes.c_int])
        self._bind("LogView_ShowCloseButton", ctypes.c_int, [Handle, ctypes.c_int])
        self._bind("LogView_ShowClearButton", ctypes.c_int, [Handle, ctypes.c_int])
        self._bind("LogView_SetClearButtonText", ctypes.c_int, [Handle, ctypes.c_char_p])
        self._bind("LogView_SetCloseButtonText", ctypes.c_int, [Handle, ctypes.c_char_p])
        self._bind("LogView_ShowScrollBar", ctypes.c_int, [Handle, ctypes.c_int])
        self._bind("LogView_SetAutoScroll", ctypes.c_int, [Handle, ctypes.c_int])
        self._bind("LogView_ScrollToTop", ctypes.c_int, [Handle])
        self._bind("LogView_ScrollToBottom", ctypes.c_int, [Handle])
        self._bind("LogView_SetScrollPosition", ctypes.c_int, [Handle, ctypes.c_int])
        self._bind("LogView_GetScrollPosition", ctypes.c_int, [Handle])
        self._bind("LogView_AddText", ctypes.c_int, [Handle, ctypes.c_char_p])
        self._bind("LogView_AddLine", ctypes.c_int, [Handle, ctypes.c_char_p])
        self._bind("LogView_AddLineEx", ctypes.c_int, [Handle, ctypes.c_char_p, Color, ctypes.c_int])
        self._bind("LogView_AddColoredLine", ctypes.c_int, [Handle, Color, Color, Color, ctypes.c_char_p])
        self._bind("LogView_Clear", ctypes.c_int, [Handle])
        self._bind("LogView_GetLineCount", ctypes.c_int, [Handle])
        self._bind("LogView_GetText", ctypes.c_int, [Handle, ctypes.c_void_p, ctypes.c_int])
        self._bind("LogView_SaveToFile", ctypes.c_int, [Handle, ctypes.c_char_p])
        self._bind("LogView_SetMaxLines", ctypes.c_int, [Handle, ctypes.c_int])
        self._bind("LogView_SetFont", ctypes.c_int, [Handle, ctypes.c_char_p, ctypes.c_int])
        self._bind("LogView_SetTextColor", ctypes.c_int, [Handle, Color])
        self._bind("LogView_SetTimeColor", ctypes.c_int, [Handle, Color])
        self._bind("LogView_SetInfoColor", ctypes.c_int, [Handle, Color])
        self._bind("LogView_SetSuccessColor", ctypes.c_int, [Handle, Color])
        self._bind("LogView_SetWarningColor", ctypes.c_int, [Handle, Color])
        self._bind("LogView_SetErrorColor", ctypes.c_int, [Handle, Color])
        self._bind("LogView_SetDebugColor", ctypes.c_int, [Handle, Color])
        self._bind("LogView_ShowTime", ctypes.c_int, [Handle, ctypes.c_int])
        self._bind("LogView_SetTimeFormat", ctypes.c_int, [Handle, ctypes.c_char_p])
        self._bind("LogView_SetPadding", ctypes.c_int, [Handle, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int])
        self._bind("LogView_SetLineHeight", ctypes.c_int, [Handle, ctypes.c_int])
        self._bind("LogView_SetHeaderHeight", ctypes.c_int, [Handle, ctypes.c_int])
        self._bind("LogView_SetButtonAreaHeight", ctypes.c_int, [Handle, ctypes.c_int])
        self._bind("LogView_SetScrollBarWidth", ctypes.c_int, [Handle, ctypes.c_int])
        self._bind("LogView_SetCallback", ctypes.c_int, [Handle, cb_type, UIntPtr])
        self._bind("LogView_SetNotifyWindow", ctypes.c_int, [Handle, Hwnd, ctypes.c_int])
        self._bind("LogView_SetUserData", ctypes.c_int, [Handle, UIntPtr])
        self._bind("LogView_GetUserData", UIntPtr, [Handle])
        self._bind("LogView_SetName", ctypes.c_int, [Handle, ctypes.c_char_p])
        self._bind("LogView_FindByName", Handle, [ctypes.c_char_p])
        self._bind("LogView_GetCount", ctypes.c_int, [])
        self._bind("LogView_IsValid", ctypes.c_int, [Handle])
        self._bind("LogView_GetLastErrorCode", ctypes.c_int, [])
        self._bind("LogView_GetLastErrorText", ctypes.c_char_p, [])
        self._bind("LogView_GetVersion", ctypes.c_char_p, [])

    def ok(self, name: str, *args: object) -> int:
        self.covered.add(name)
        result = int(getattr(self.dll, name)(*args))
        if result == 0:
            raise RuntimeError(f"{name} failed: {self.last_error_text()}")
        return result

    def handle(self, name: str, *args: object) -> int:
        self.covered.add(name)
        result = as_int(getattr(self.dll, name)(*args))
        if result == 0:
            raise RuntimeError(f"{name} failed: {self.last_error_text()}")
        return result

    def value(self, name: str, *args: object) -> int:
        self.covered.add(name)
        return as_int(getattr(self.dll, name)(*args))

    def last_error_text(self) -> str:
        self.covered.add("LogView_GetLastErrorText")
        raw = self.dll.LogView_GetLastErrorText()
        return raw.decode("utf-8", errors="replace") if raw else ""

    def version(self) -> str:
        self.covered.add("LogView_GetVersion")
        raw = self.dll.LogView_GetVersion()
        return raw.decode("utf-8", errors="replace") if raw else ""

    def assert_full_coverage(self) -> None:
        missing = sorted(EXPORTED_FUNCTIONS - self.covered)
        if missing:
            raise RuntimeError("These LogView exports were not demonstrated: " + ", ".join(missing))


class NativeHost:
    def __init__(self, title: str, width: int, height: int) -> None:
        self.hinstance = kernel32.GetModuleHandleW(None)
        self.class_name = "LogViewPythonDemoHostWindow"
        self._wndproc = WNDPROC(self._wnd_proc)
        self.child_a = 0
        self.child_b = 0
        self._register_class()
        self.hwnd = user32.CreateWindowExW(
            0,
            self.class_name,
            title,
            0x00CF0000 | 0x10000000,
            80,
            80,
            width,
            height,
            None,
            None,
            self.hinstance,
            None,
        )
        if not self.hwnd:
            raise ctypes.WinError(ctypes.get_last_error())
        user32.ShowWindow(self.hwnd, 5)
        user32.UpdateWindow(self.hwnd)

    def _register_class(self) -> None:
        wc = WNDCLASSW()
        wc.style = 0x0002 | 0x0001
        wc.lpfnWndProc = self._wndproc
        wc.hInstance = self.hinstance
        wc.hbrBackground = gdi32.CreateSolidBrush(rgb(28, 35, 44))
        wc.lpszClassName = self.class_name
        user32.RegisterClassW(ctypes.byref(wc))

    def set_children(self, child_a: int, child_b: int) -> None:
        self.child_a = child_a
        self.child_b = child_b
        self.layout_children()

    def layout_children(self) -> None:
        if not (self.child_a or self.child_b):
            return
        rect = RECT()
        user32.GetClientRect(self.hwnd, ctypes.byref(rect))
        width = max(1, rect.right - rect.left)
        height = max(1, rect.bottom - rect.top)
        gap = 10
        top = 14
        left_width = max(1, (width - gap * 3) // 2)
        right_width = max(1, width - left_width - gap * 3)
        child_height = max(1, height - top - gap)
        if DEMO is not None:
            if self.child_a:
                DEMO.api.ok("LogView_SetRect", Handle(self.child_a), gap, top, left_width, child_height)
            if self.child_b:
                DEMO.api.ok("LogView_SetRect", Handle(self.child_b), gap * 2 + left_width, top, right_width, child_height)

    def _draw_background(self, hdc: int, width: int, height: int) -> None:
        bg = gdi32.CreateSolidBrush(rgb(47, 61, 77))
        rect = RECT(0, 0, width, height)
        user32.FillRect(hdc, ctypes.byref(rect), bg)
        gdi32.DeleteObject(bg)

        block = gdi32.CreateSolidBrush(rgb(40, 112, 148))
        for y in range(20, height, 112):
            for x in range(22, width, 132):
                user32.FillRect(hdc, ctypes.byref(RECT(x, y, min(width, x + 54), min(height, y + 54))), block)
        gdi32.DeleteObject(block)

        pen = gdi32.CreatePen(0, 2, rgb(72, 132, 180))
        old = gdi32.SelectObject(hdc, pen)
        for x in range(-height, width + height, 28):
            gdi32.MoveToEx(hdc, x, height, None)
            gdi32.LineTo(hdc, x + height, 0)
        gdi32.SelectObject(hdc, old)
        gdi32.DeleteObject(pen)

    def _paint(self, hwnd: int, hdc: int | None = None) -> None:
        rect = RECT()
        user32.GetClientRect(hwnd, ctypes.byref(rect))
        width = max(1, rect.right - rect.left)
        height = max(1, rect.bottom - rect.top)
        if hdc is not None:
            self._draw_background(hdc, width, height)
            return
        ps = PAINTSTRUCT()
        paint_dc = user32.BeginPaint(hwnd, ctypes.byref(ps))
        self._draw_background(paint_dc, width, height)
        user32.EndPaint(hwnd, ctypes.byref(ps))

    def _wnd_proc(self, hwnd: int, msg: int, wparam: int, lparam: int) -> int:
        if msg == 0x000F:
            self._paint(hwnd)
            return 0
        if msg == 0x0318:
            self._paint(hwnd, as_int(wparam))
            return 0
        if msg == 0x0005:
            self.layout_children()
            return 0
        if msg == NOTIFY_MESSAGE:
            print(f"[notify] {LOGVIEW_EVENT_NAMES.get(as_int(wparam), as_int(wparam))} handle=0x{as_int(lparam):X}")
            return 0
        if msg == 0x0002:
            user32.PostQuitMessage(0)
            return 0
        return user32.DefWindowProcW(hwnd, msg, wparam, lparam)

    def pump_for(self, seconds: float) -> None:
        end = time.monotonic() + seconds
        msg = MSG()
        while time.monotonic() < end:
            while user32.PeekMessageW(ctypes.byref(msg), None, 0, 0, 1):
                if msg.message == 0x0012:
                    return
                user32.TranslateMessage(ctypes.byref(msg))
                user32.DispatchMessageW(ctypes.byref(msg))
            time.sleep(0.01)

    def destroy(self) -> None:
        if self.hwnd:
            user32.DestroyWindow(self.hwnd)
            self.hwnd = None


class FullDemo:
    def __init__(self) -> None:
        self.api = LogViewApi(DLL_PATH)
        self.host: NativeHost | None = None
        self.callback_events: list[tuple[int, int, int, int, int]] = []
        self._callback_ref = self.api.callback_type(self._on_logview_event)

    def _on_logview_event(self, handle: int, event_type: int, wparam: int, lparam: int, user_data: int) -> None:
        self.callback_events.append((as_int(handle), event_type, as_int(wparam), as_int(lparam), as_int(user_data)))
        print(f"[callback] {LOGVIEW_EVENT_NAMES.get(event_type, event_type)} handle=0x{as_int(handle):X} user=0x{as_int(user_data):X}")

    def run(self) -> None:
        global DEMO
        DEMO = self

        print(f"Loading {DLL_PATH}")
        self.api.ok("LogView_Init")
        print("LogView version:", self.api.version())
        print("Initial error:", self.api.value("LogView_GetLastErrorCode"), self.api.last_error_text())

        self.host = NativeHost("Python LogView full demo host - embedded windows", 980, 560)
        parent = Hwnd(as_int(self.host.hwnd))

        child_a = self.api.handle("LogView_CreateChild", parent, 10, 14, 450, 480)
        child_b = self.api.handle("LogView_CreateChildEx", parent, 480, 14, 450, 480, 128)
        self.host.set_children(child_a, child_b)

        popup = self.api.handle("LogView_CreatePopupEx", 520, 380, 190)
        scratch = self.api.handle("LogView_CreatePopup", 320, 220)
        attachable = self.api.handle("LogView_CreatePopupEx", 360, 260, 220)

        self._setup_embedded(child_a, child_b)
        self._setup_popup(popup)
        self._exercise_window_management(popup, scratch, attachable, parent)
        self._exercise_logging(child_a, child_b, popup)
        self._exercise_callbacks_and_metadata(child_a)
        self._exercise_export(child_b)

        print("Current instance count:", self.api.value("LogView_GetCount"))
        print("Child A valid:", self.api.value("LogView_IsValid", Handle(child_a)))
        print("Child B HWND:", hex(self.api.value("LogView_GetWindowHandle", Handle(child_b))))

        print("Demo is visible for 8 seconds. Resize the host window if you want to see embedded SetRect handling.")
        self.host.pump_for(8.0)

        self.api.ok("LogView_Destroy", Handle(child_a))
        self.api.ok("LogView_Destroy", Handle(child_b))
        self.api.ok("LogView_Destroy", Handle(popup))

        temp_a = self.api.handle("LogView_CreatePopupEx", 260, 180, 180)
        temp_b = self.api.handle("LogView_CreatePopupEx", 260, 180, 180)
        self.api.ok("LogView_SetTitle", Handle(temp_a), utf8("DestroyAll temp A"))
        self.api.ok("LogView_SetTitle", Handle(temp_b), utf8("DestroyAll temp B"))
        self.api.ok("LogView_Show", Handle(temp_a))
        self.api.ok("LogView_Show", Handle(temp_b))
        self.host.pump_for(0.2)
        self.api.ok("LogView_DestroyAll")

        self.api.ok("LogView_Uninit")
        self.host.destroy()
        self.api.assert_full_coverage()
        print(f"All {len(EXPORTED_FUNCTIONS)} exported functions were demonstrated.")
        print(f"Exported log text file: {EXPORT_PATH}")

    def _setup_embedded(self, child_a: int, child_b: int) -> None:
        a = Handle(child_a)
        b = Handle(child_b)
        self.api.ok("LogView_SetTitle", a, utf8("Python embedded log - opaque"))
        self.api.ok("LogView_SetTitle", b, utf8("Python embedded log - 50% background alpha"))
        self.api.ok("LogView_SetAlpha", a, 255)
        self.api.ok("LogView_SetAlpha", b, 128)
        self.api.ok("LogView_SetBackgroundColor", a, Color(0xFF1F242A))
        self.api.ok("LogView_SetBackgroundColor", b, Color(0xFF202428))
        self.api.ok("LogView_SetBorderColor", a, Color(0xFFD8CE58))
        self.api.ok("LogView_SetBorderColor", b, Color(0xFF38BDF8))
        self.api.ok("LogView_SetRoundCorner", a, 12)
        self.api.ok("LogView_SetRoundCorner", b, 12)
        self.api.ok("LogView_SetFont", a, utf8("Microsoft YaHei UI"), 15)
        self.api.ok("LogView_SetFont", b, utf8("Microsoft YaHei UI"), 15)
        self.api.ok("LogView_SetTextColor", a, Color(0xFFF8FAFC))
        self.api.ok("LogView_SetTimeColor", a, Color(0xFF9CA3AF))
        self.api.ok("LogView_SetInfoColor", a, Color(0xFFF8FAFC))
        self.api.ok("LogView_SetSuccessColor", a, Color(0xFF22C55E))
        self.api.ok("LogView_SetWarningColor", a, Color(0xFFFBBF24))
        self.api.ok("LogView_SetErrorColor", a, Color(0xFFF87171))
        self.api.ok("LogView_SetDebugColor", a, Color(0xFFA78BFA))
        self.api.ok("LogView_ShowTime", a, 0)
        self.api.ok("LogView_AddLine", a, utf8("This line has no timestamp because LogView_ShowTime(0) was called."))
        self.api.ok("LogView_ShowTime", a, 1)
        self.api.ok("LogView_SetTimeFormat", a, utf8("HH:mm:ss"))
        self.api.ok("LogView_SetPadding", a, 14, 12, 14, 12)
        self.api.ok("LogView_SetLineHeight", a, 25)
        self.api.ok("LogView_SetHeaderHeight", a, 58)
        self.api.ok("LogView_SetButtonAreaHeight", a, 42)
        self.api.ok("LogView_SetScrollBarWidth", a, 8)
        self.api.ok("LogView_SetClearButtonText", a, utf8("Clear"))
        self.api.ok("LogView_SetCloseButtonText", a, utf8("Close"))
        self.api.ok("LogView_ShowTitleBar", a, 0)
        self.api.ok("LogView_ShowTitleBar", a, 1)
        self.api.ok("LogView_ShowCloseButton", a, 0)
        self.api.ok("LogView_ShowCloseButton", a, 1)
        self.api.ok("LogView_ShowClearButton", a, 0)
        self.api.ok("LogView_ShowClearButton", a, 1)
        self.api.ok("LogView_ShowScrollBar", a, 0)
        self.api.ok("LogView_ShowScrollBar", a, 1)
        self.api.ok("LogView_SetAutoScroll", a, 1)
        self.api.ok("LogView_SetMaxLines", a, 240)
        self.api.ok("LogView_Show", a)
        self.api.ok("LogView_Show", b)

    def _setup_popup(self, popup: int) -> None:
        h = Handle(popup)
        self.api.ok("LogView_SetTitle", h, utf8("Python popup log"))
        self.api.ok("LogView_SetTopMost", h, 1)
        self.api.ok("LogView_SetPosition", h, 1120, 120)
        self.api.ok("LogView_SetSize", h, 520, 380)
        self.api.ok("LogView_GetRect", h, ctypes.byref(ctypes.c_int()), ctypes.byref(ctypes.c_int()), ctypes.byref(ctypes.c_int()), ctypes.byref(ctypes.c_int()))
        self.api.ok("LogView_CenterScreen", h)
        self.api.ok("LogView_EnableDrag", h, 1)
        self.api.ok("LogView_SetAlpha", h, 190)
        self.api.ok("LogView_Show", h)

    def _exercise_window_management(self, popup: int, scratch: int, attachable: int, parent: Hwnd) -> None:
        s = Handle(scratch)
        self.api.ok("LogView_SetTitle", s, utf8("Python scratch close/hide demo"))
        self.api.ok("LogView_Show", s)
        print("Scratch visible:", self.api.value("LogView_IsVisible", s))
        self.api.ok("LogView_Hide", s)
        print("Scratch visible after hide:", self.api.value("LogView_IsVisible", s))
        self.api.ok("LogView_Show", s)
        self.api.ok("LogView_SetCloseMode", s, LOGVIEW_CLOSE_NOTIFY_ONLY)
        self.api.ok("LogView_Close", s)
        self.api.ok("LogView_SetCloseMode", s, LOGVIEW_CLOSE_HIDE)
        self.api.ok("LogView_Close", s)
        self.api.ok("LogView_Show", s)
        self.api.ok("LogView_SetCloseMode", s, LOGVIEW_CLOSE_DESTROY)
        self.api.ok("LogView_Close", s)
        print("Scratch valid after close-destroy:", self.api.value("LogView_IsValid", s))

        a = Handle(attachable)
        self.api.ok("LogView_SetTitle", a, utf8("Attach/detach demo"))
        self.api.ok("LogView_Show", a)
        self.api.ok("LogView_AttachToParent", a, parent, 40, 80, 360, 220)
        self.host.pump_for(0.3)
        self.api.ok("LogView_DetachToDesktop", a)
        self.api.ok("LogView_SetRect", a, 100, 100, 360, 260)
        self.api.ok("LogView_Destroy", a)

    def _exercise_logging(self, child_a: int, child_b: int, popup: int) -> None:
        a = Handle(child_a)
        b = Handle(child_b)
        p = Handle(popup)
        self.api.ok("LogView_AddLine", a, utf8("LogView_AddLine: normal information."))
        self.api.ok("LogView_AddText", a, utf8("  + LogView_AddText appended to previous line."))
        self.api.ok("LogView_AddLineEx", a, utf8("SUCCESS level through LogView_AddLineEx"), Color(0), LOGVIEW_LEVEL_SUCCESS)
        self.api.ok("LogView_AddLineEx", a, utf8("WARNING level through LogView_AddLineEx"), Color(0), LOGVIEW_LEVEL_WARNING)
        self.api.ok("LogView_AddLineEx", a, utf8("ERROR level through LogView_AddLineEx"), Color(0), LOGVIEW_LEVEL_ERROR)
        self.api.ok("LogView_AddLineEx", a, utf8("DEBUG level through LogView_AddLineEx"), Color(0), LOGVIEW_LEVEL_DEBUG)
        self.api.ok("LogView_AddColoredLine", a, Color(0xFF60A5FA), Color(0xFFFBBF24), Color(0xFFFB7185), utf8("LogView_AddColoredLine custom time/prefix/text colors."))

        for i in range(42):
            self.api.ok("LogView_AddLine", a, utf8(f"Embedded opaque log line {i:03d}"))
            self.api.ok("LogView_AddLine", b, utf8(f"Embedded 50% background log line {i:03d}"))
            if i % 4 == 0:
                self.api.ok("LogView_AddLineEx", p, utf8(f"Popup success line {i:03d}"), Color(0xFF22C55E), LOGVIEW_LEVEL_SUCCESS)

        self.api.ok("LogView_ScrollToTop", a)
        self.api.ok("LogView_SetScrollPosition", a, 3)
        print("Scroll position:", self.api.value("LogView_GetScrollPosition", a))
        self.api.ok("LogView_ScrollToBottom", a)
        print("Line count:", self.api.value("LogView_GetLineCount", a))

    def _exercise_callbacks_and_metadata(self, child_a: int) -> None:
        h = Handle(child_a)
        self.api.ok("LogView_SetCallback", h, self._callback_ref, UIntPtr(0xBEEF))
        self.api.ok("LogView_SetNotifyWindow", h, Hwnd(as_int(self.host.hwnd)), NOTIFY_MESSAGE)
        self.api.ok("LogView_SetUserData", h, UIntPtr(0x12345678))
        print("User data:", hex(self.api.value("LogView_GetUserData", h)))
        self.api.ok("LogView_SetName", h, utf8("python-primary-log"))
        found = self.api.handle("LogView_FindByName", utf8("python-primary-log"))
        print("FindByName matched:", found == child_a)
        self.api.ok("LogView_ScrollToTop", h)
        self.api.ok("LogView_SetScrollPosition", h, 6)
        self.host.pump_for(0.2)
        self.api.ok("LogView_Clear", h)
        self.api.ok("LogView_AddLine", h, utf8("LogView_Clear was called, then this line was added."))
        self.host.pump_for(0.2)

    def _exercise_export(self, child_b: int) -> None:
        h = Handle(child_b)
        required = self.api.value("LogView_GetText", h, None, 0)
        if required <= 1:
            raise RuntimeError("LogView_GetText did not report text content.")
        buffer = ctypes.create_string_buffer(required)
        actual = self.api.value("LogView_GetText", h, ctypes.cast(buffer, ctypes.c_void_p), required)
        text = buffer.value.decode("utf-8", errors="replace")
        print(f"GetText bytes: required={required}, actual={actual}, preview={text.splitlines()[0]!r}")
        self.api.ok("LogView_SaveToFile", h, utf8(EXPORT_PATH))


DEMO: FullDemo | None = None


def main() -> int:
    if sys.platform != "win32":
        print("This demo requires Windows.")
        return 1
    demo = FullDemo()
    try:
        demo.run()
    except Exception:
        try:
            demo.api.ok("LogView_DestroyAll")
            demo.api.ok("LogView_Uninit")
        except Exception:
            pass
        if demo.host:
            demo.host.destroy()
        raise
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
