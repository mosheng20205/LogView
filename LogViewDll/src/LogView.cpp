#include "LogView.h"

#include <windows.h>
#include <windowsx.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <functional>
#include <future>
#include <limits>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace {

constexpr wchar_t kWindowClassName[] = L"LogView.FloatingWindow";
constexpr UINT kTaskMessage = WM_APP + 0x3510;
constexpr int kDefaultAlpha = 128;
constexpr int kDefaultCornerRadius = 10;
constexpr int kDefaultMinWidth = 260;
constexpr int kDefaultMinHeight = 160;
constexpr int kDefaultHeaderHeight = 64;
constexpr int kDefaultButtonAreaHeight = 44;
constexpr int kDefaultScrollBarWidth = 8;
constexpr int kDefaultLineHeight = 24;
constexpr int kDefaultFontSize = 16;
constexpr int kDefaultMaxLines = 1000;
constexpr const char* kVersion = "1.0.0";

enum ErrorCode {
    kErrorOk = 0,
    kErrorRuntime = 1,
    kErrorInvalidHandle = 2,
    kErrorInvalidArgument = 3,
    kErrorWindow = 4
};

enum class LineKind {
    Normal,
    Colored
};

struct LogLine {
    std::wstring time;
    std::wstring text;
    LogViewColor textColor = 0xFFF1F5F9;
    LogViewColor timeColor = 0xFF9CA3AF;
    LogViewColor prefixColor = 0xFFD8CE58;
    int level = LOGVIEW_LEVEL_INFO;
    LineKind kind = LineKind::Normal;
};

struct LogViewInstance {
    HWND hwnd = nullptr;
    HWND parentHwnd = nullptr;
    DWORD ownerThreadId = 0;
    bool child = false;
    bool destroyed = false;
    bool dragEnabled = true;
    bool topMost = false;
    bool showTitleBar = true;
    bool showCloseButton = true;
    bool showClearButton = true;
    bool showScrollBar = true;
    bool autoScroll = true;
    bool showTime = true;
    bool closeHover = false;
    bool clearHover = false;
    bool draggingScroll = false;
    int scrollDragOffset = 0;
    int closeMode = LOGVIEW_CLOSE_HIDE;
    int alpha = kDefaultAlpha;
    int cornerRadius = kDefaultCornerRadius;
    int minWidth = kDefaultMinWidth;
    int minHeight = kDefaultMinHeight;
    int fontSize = kDefaultFontSize;
    int lineHeight = kDefaultLineHeight;
    int headerHeight = kDefaultHeaderHeight;
    int buttonAreaHeight = kDefaultButtonAreaHeight;
    int scrollBarWidth = kDefaultScrollBarWidth;
    int paddingLeft = 12;
    int paddingTop = 12;
    int paddingRight = 12;
    int paddingBottom = 12;
    int scrollPosition = 0;
    int maxLines = kDefaultMaxLines;
    uintptr_t userData = 0;
    uint64_t order = 0;
    std::wstring title = L"日志系统 v1.0";
    std::wstring clearButtonText = L"清空";
    std::wstring closeButtonText = L"X";
    std::wstring fontName = L"Microsoft YaHei UI";
    std::wstring timeFormat = L"HH:mm:ss";
    std::wstring name;
    LogViewColor backgroundColor = 0xFF202428;
    LogViewColor borderColor = 0xFFD8CE58;
    LogViewColor titleColor = 0xFFFFFFFF;
    LogViewColor textColor = 0xFFF1F5F9;
    LogViewColor timeColor = 0xFF9CA3AF;
    LogViewColor infoColor = 0xFFF1F5F9;
    LogViewColor successColor = 0xFF38D27A;
    LogViewColor warningColor = 0xFFF59E0B;
    LogViewColor errorColor = 0xFFEF4444;
    LogViewColor debugColor = 0xFFA78BFA;
    LogViewCallback callback = nullptr;
    uintptr_t callbackUserData = 0;
    HWND notifyHwnd = nullptr;
    UINT notifyMessage = 0;
    RECT closeRect{};
    RECT clearRect{};
    RECT logRect{};
    RECT scrollTrackRect{};
    RECT scrollThumbRect{};
    std::vector<LogLine> lines;
    ComPtr<ID2D1HwndRenderTarget> renderTarget;
    ComPtr<IDWriteTextFormat> textFormatObject;
    ComPtr<IDWriteTextFormat> titleFormatObject;
};

HINSTANCE g_module = nullptr;
volatile LONG g_runtimeRunning = 0;
volatile LONG64 g_nextOrder = 0;
HANDLE g_uiThreadHandle = nullptr;
DWORD g_uiThreadId = 0;
SRWLOCK g_runtimeLock = SRWLOCK_INIT;
SRWLOCK g_taskLock = SRWLOCK_INIT;
std::queue<std::function<void()>> g_tasks;
SRWLOCK g_instanceLock = SRWLOCK_INIT;
std::unordered_map<LogViewHandle, std::shared_ptr<LogViewInstance>> g_instances;
ComPtr<ID2D1Factory> g_d2dFactory;
ComPtr<IDWriteFactory> g_dwriteFactory;

thread_local int t_lastErrorCode = kErrorOk;
thread_local std::string t_lastErrorText = "OK";

bool InitFactories();

bool IsRuntimeRunning() {
    return InterlockedCompareExchange(&g_runtimeRunning, 0, 0) != 0;
}

void SetRuntimeRunning(bool running) {
    InterlockedExchange(&g_runtimeRunning, running ? 1 : 0);
}

void EnableDpiAwareness() {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (!user32) {
        return;
    }
    using SetDpiAwarenessContextFn = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
    auto setContext = reinterpret_cast<SetDpiAwarenessContextFn>(
        GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
    if (setContext && setContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
        return;
    }
    using SetProcessDpiAwareFn = BOOL(WINAPI*)();
    auto setAware = reinterpret_cast<SetProcessDpiAwareFn>(
        GetProcAddress(user32, "SetProcessDPIAware"));
    if (setAware) {
        setAware();
    }
}

UINT GetDpiForTargetWindow(HWND hwnd) {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        using GetDpiForWindowFn = UINT(WINAPI*)(HWND);
        auto getDpiForWindow = reinterpret_cast<GetDpiForWindowFn>(
            GetProcAddress(user32, "GetDpiForWindow"));
        if (getDpiForWindow && hwnd) {
            UINT dpi = getDpiForWindow(hwnd);
            if (dpi != 0) {
                return dpi;
            }
        }
    }
    HDC dc = GetDC(hwnd);
    UINT dpi = 96;
    if (dc) {
        dpi = static_cast<UINT>(GetDeviceCaps(dc, LOGPIXELSX));
        ReleaseDC(hwnd, dc);
    }
    return dpi == 0 ? 96 : dpi;
}

int ScaleByDpi(int value, UINT dpi) {
    return MulDiv(value, static_cast<int>(dpi), 96);
}

int ScaleForWindow(const LogViewInstance& inst, int value) {
    if (inst.child) {
        return value;
    }
    return ScaleByDpi(value, GetDpiForTargetWindow(inst.hwnd));
}

void SetApiError(int code, const char* text) {
    t_lastErrorCode = code;
    t_lastErrorText = text ? text : "";
}

void SetWin32ApiError(const char* prefix, DWORD error) {
    char message[160]{};
    sprintf_s(
        message,
        "%s Win32 error: %lu.",
        prefix ? prefix : "Win32 call failed.",
        static_cast<unsigned long>(error));
    SetApiError(kErrorRuntime, message);
}

LogViewHandle ToHandle(const std::shared_ptr<LogViewInstance>& inst) {
    return reinterpret_cast<LogViewHandle>(inst.get());
}

std::wstring Utf8ToWide(const char* text) {
    if (!text) {
        return L"";
    }
    const int needed = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, nullptr, 0);
    if (needed <= 0) {
        return L"";
    }
    std::wstring result(static_cast<size_t>(needed - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, result.data(), needed);
    return result;
}

std::string WideToUtf8(const std::wstring& text) {
    if (text.empty()) {
        return {};
    }
    const int needed = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (needed <= 0) {
        return {};
    }
    std::string result(static_cast<size_t>(needed - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, result.data(), needed, nullptr, nullptr);
    return result;
}

std::wstring BuildLogTextWide(const LogViewInstance& inst) {
    std::wstring result;
    for (size_t i = 0; i < inst.lines.size(); ++i) {
        if (i > 0) {
            result += L"\r\n";
        }

        const LogLine& line = inst.lines[i];
        if (!line.time.empty()) {
            result += line.time;
            result += line.kind == LineKind::Colored ? L" >> " : L" | ";
        }
        result += line.text;
    }
    return result;
}

std::string BuildLogTextUtf8(const LogViewInstance& inst) {
    return WideToUtf8(BuildLogTextWide(inst));
}

int ClampInt(int value, int minValue, int maxValue) {
    return std::max(minValue, std::min(maxValue, value));
}

BYTE ClampAlpha(int alpha) {
    return static_cast<BYTE>(ClampInt(alpha, 0, 255));
}

D2D1_COLOR_F ToD2DColor(LogViewColor color) {
    const float a = static_cast<float>((color >> 24) & 0xFF) / 255.0f;
    const float r = static_cast<float>((color >> 16) & 0xFF) / 255.0f;
    const float g = static_cast<float>((color >> 8) & 0xFF) / 255.0f;
    const float b = static_cast<float>(color & 0xFF) / 255.0f;
    return D2D1::ColorF(r, g, b, a);
}

D2D1_RECT_F ToRectF(const RECT& rect) {
    return D2D1::RectF(
        static_cast<float>(rect.left),
        static_cast<float>(rect.top),
        static_cast<float>(rect.right),
        static_cast<float>(rect.bottom));
}

bool IsPointInRect(const RECT& rect, POINT pt) {
    return PtInRect(&rect, pt) != FALSE;
}

void ReplaceAll(std::wstring& text, const std::wstring& from, const std::wstring& to) {
    size_t pos = 0;
    while ((pos = text.find(from, pos)) != std::wstring::npos) {
        text.replace(pos, from.length(), to);
        pos += to.length();
    }
}

std::wstring TwoDigits(int value) {
    wchar_t buf[8]{};
    swprintf_s(buf, L"%02d", value);
    return buf;
}

std::wstring FormatCurrentTime(const LogViewInstance& inst) {
    SYSTEMTIME st{};
    GetLocalTime(&st);
    std::wstring result = inst.timeFormat.empty() ? L"HH:mm:ss" : inst.timeFormat;
    ReplaceAll(result, L"HH", TwoDigits(st.wHour));
    ReplaceAll(result, L"mm", TwoDigits(st.wMinute));
    ReplaceAll(result, L"ss", TwoDigits(st.wSecond));
    return result;
}

LogViewColor ColorForLevel(const LogViewInstance& inst, int level) {
    switch (level) {
    case LOGVIEW_LEVEL_SUCCESS:
        return inst.successColor;
    case LOGVIEW_LEVEL_WARNING:
        return inst.warningColor;
    case LOGVIEW_LEVEL_ERROR:
        return inst.errorColor;
    case LOGVIEW_LEVEL_DEBUG:
        return inst.debugColor;
    case LOGVIEW_LEVEL_INFO:
    default:
        return inst.infoColor;
    }
}

LogViewColor WithAlpha(LogViewColor color, int alpha) {
    return (static_cast<LogViewColor>(ClampInt(alpha, 0, 255)) << 24) | (color & 0x00FFFFFF);
}

int MaxScrollPosition(const LogViewInstance& inst) {
    const int logHeight = std::max(0, static_cast<int>(inst.logRect.bottom - inst.logRect.top));
    const int visibleLines = std::max(1, logHeight / std::max(1, inst.lineHeight));
    return std::max(0, static_cast<int>(inst.lines.size()) - visibleLines);
}

void ClampScroll(LogViewInstance& inst) {
    inst.scrollPosition = ClampInt(inst.scrollPosition, 0, MaxScrollPosition(inst));
}

void NotifyEvent(LogViewInstance& inst, int eventType, uintptr_t wParam = 0, uintptr_t lParam = 0) {
    const LogViewHandle handle = reinterpret_cast<LogViewHandle>(&inst);
    if (inst.callback) {
        inst.callback(handle, eventType, wParam, lParam, inst.callbackUserData);
    }
    if (inst.notifyHwnd && inst.notifyMessage != 0) {
        PostMessageW(inst.notifyHwnd, inst.notifyMessage, static_cast<WPARAM>(eventType), static_cast<LPARAM>(handle));
    }
}

void RequestRepaint(LogViewInstance& inst) {
    if (inst.hwnd) {
        InvalidateRect(inst.hwnd, nullptr, FALSE);
    }
}

void ReleaseDeviceResources(LogViewInstance& inst) {
    inst.renderTarget.Reset();
}

void ReleaseTextFormats(LogViewInstance& inst) {
    inst.textFormatObject.Reset();
    inst.titleFormatObject.Reset();
}

bool EnsureTextFormats(LogViewInstance& inst) {
    if (!g_dwriteFactory && !InitFactories()) {
        return false;
    }
    if (!g_dwriteFactory) {
        return false;
    }
    if (!inst.textFormatObject) {
        HRESULT hr = g_dwriteFactory->CreateTextFormat(
            inst.fontName.c_str(),
            nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            static_cast<float>(inst.fontSize),
            L"",
            inst.textFormatObject.GetAddressOf());
        if (FAILED(hr)) {
            return false;
        }
        inst.textFormatObject->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        inst.textFormatObject->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        inst.textFormatObject->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
    if (!inst.titleFormatObject) {
        HRESULT hr = g_dwriteFactory->CreateTextFormat(
            inst.fontName.c_str(),
            nullptr,
            DWRITE_FONT_WEIGHT_SEMI_BOLD,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            static_cast<float>(std::max(inst.fontSize + 2, 18)),
            L"",
            inst.titleFormatObject.GetAddressOf());
        if (FAILED(hr)) {
            return false;
        }
        inst.titleFormatObject->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        inst.titleFormatObject->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
    return true;
}

bool EnsureRenderTarget(LogViewInstance& inst) {
    if (!g_d2dFactory && !InitFactories()) {
        return false;
    }
    if (inst.renderTarget) {
        return true;
    }
    if (!inst.hwnd || !g_d2dFactory) {
        return false;
    }
    RECT rc{};
    GetClientRect(inst.hwnd, &rc);
    const D2D1_SIZE_U size = D2D1::SizeU(
        static_cast<UINT32>(std::max(1L, rc.right - rc.left)),
        static_cast<UINT32>(std::max(1L, rc.bottom - rc.top)));
    HRESULT hr = g_d2dFactory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED)),
        D2D1::HwndRenderTargetProperties(inst.hwnd, size),
        inst.renderTarget.GetAddressOf());
    if (SUCCEEDED(hr)) {
        // The rest of this file lays out controls using GetClientRect pixels.
        // Keep Direct2D in a 1:1 pixel coordinate space so high-DPI monitors do
        // not push the close/clear buttons outside the visible client area.
        inst.renderTarget->SetDpi(96.0f, 96.0f);
        inst.renderTarget->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    }
    return SUCCEEDED(hr);
}

void ComputeLayout(LogViewInstance& inst) {
    RECT rc{};
    if (inst.hwnd) {
        GetClientRect(inst.hwnd, &rc);
    }
    const int width = rc.right - rc.left;
    const int height = rc.bottom - rc.top;
    SetRectEmpty(&inst.closeRect);
    SetRectEmpty(&inst.clearRect);
    SetRectEmpty(&inst.logRect);
    SetRectEmpty(&inst.scrollTrackRect);
    SetRectEmpty(&inst.scrollThumbRect);

    const int headerBottom = inst.showTitleBar ? inst.headerHeight : 0;
    if (inst.showCloseButton) {
        inst.closeRect = { width - 36, 8, width - 8, 36 };
    }
    const int bottomReserved = inst.showClearButton ? inst.buttonAreaHeight : inst.paddingBottom;
    if (inst.showClearButton) {
        inst.clearRect = {
            std::max(8, width - 116),
            std::max(headerBottom, height - 38),
            width - 12,
            height - 10
        };
    }
    const int scrollReserved = inst.showScrollBar ? inst.scrollBarWidth + 8 : 0;
    inst.logRect = {
        inst.paddingLeft,
        headerBottom + inst.paddingTop,
        std::max(inst.paddingLeft, width - inst.paddingRight - scrollReserved),
        std::max(headerBottom + inst.paddingTop, height - bottomReserved)
    };

    if (inst.showScrollBar) {
        inst.scrollTrackRect = {
            width - inst.paddingRight - inst.scrollBarWidth,
            inst.logRect.top,
            width - inst.paddingRight,
            inst.logRect.bottom
        };
        const int trackHeight = std::max(1, static_cast<int>(inst.scrollTrackRect.bottom - inst.scrollTrackRect.top));
        const int visibleLines = std::max(1, static_cast<int>(inst.logRect.bottom - inst.logRect.top) / std::max(1, inst.lineHeight));
        const int totalLines = std::max(1, static_cast<int>(inst.lines.size()));
        const int maxScroll = std::max(0, totalLines - visibleLines);
        int thumbHeight = maxScroll <= 0 ? trackHeight : std::max(24, trackHeight * visibleLines / totalLines);
        thumbHeight = std::min(trackHeight, thumbHeight);
        const int maxThumbTop = inst.scrollTrackRect.top + trackHeight - thumbHeight;
        const int thumbTop = maxScroll <= 0
            ? inst.scrollTrackRect.top
            : inst.scrollTrackRect.top + (maxThumbTop - inst.scrollTrackRect.top) * inst.scrollPosition / maxScroll;
        inst.scrollThumbRect = {
            inst.scrollTrackRect.left,
            thumbTop,
            inst.scrollTrackRect.right,
            thumbTop + thumbHeight
        };
    }
    ClampScroll(inst);
}

void ApplyRoundCorner(LogViewInstance& inst) {
    if (!inst.hwnd) {
        return;
    }
    RECT rc{};
    GetClientRect(inst.hwnd, &rc);
    const int radius = std::max(0, inst.cornerRadius);
    if (radius <= 0) {
        SetWindowRgn(inst.hwnd, nullptr, TRUE);
        return;
    }
    HRGN region = CreateRoundRectRgn(0, 0, rc.right + 1, rc.bottom + 1, radius * 2, radius * 2);
    SetWindowRgn(inst.hwnd, region, TRUE);
}

void ApplyAlpha(LogViewInstance& inst) {
    if (!inst.hwnd) {
        return;
    }
    if (inst.child) {
        RequestRepaint(inst);
        return;
    }
    LONG_PTR exStyle = GetWindowLongPtrW(inst.hwnd, GWL_EXSTYLE);
    if ((exStyle & WS_EX_LAYERED) == 0) {
        SetWindowLongPtrW(inst.hwnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
    }
    // Keep the native window fully opaque at the compositor level. The public
    // alpha value is applied only to the painted background so text stays sharp.
    SetLayeredWindowAttributes(inst.hwnd, 0, 255, LWA_ALPHA);
    RequestRepaint(inst);
}

void ApplyTopMost(LogViewInstance& inst) {
    if (!inst.hwnd || inst.child) {
        return;
    }
    SetWindowPos(
        inst.hwnd,
        inst.topMost ? HWND_TOPMOST : HWND_NOTOPMOST,
        0,
        0,
        0,
        0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void SetScrollPositionInternal(LogViewInstance& inst, int position, bool notify) {
    ComputeLayout(inst);
    const int oldPosition = inst.scrollPosition;
    inst.scrollPosition = ClampInt(position, 0, MaxScrollPosition(inst));
    if (oldPosition != inst.scrollPosition && notify) {
        NotifyEvent(inst, LOGVIEW_EVENT_SCROLL_CHANGED, static_cast<uintptr_t>(inst.scrollPosition), 0);
    }
    RequestRepaint(inst);
}

void ScrollToBottom(LogViewInstance& inst) {
    ComputeLayout(inst);
    inst.scrollPosition = MaxScrollPosition(inst);
}

void EnforceMaxLines(LogViewInstance& inst) {
    if (inst.maxLines <= 0) {
        inst.maxLines = 1;
    }
    if (static_cast<int>(inst.lines.size()) > inst.maxLines) {
        const size_t removeCount = inst.lines.size() - static_cast<size_t>(inst.maxLines);
        inst.lines.erase(inst.lines.begin(), inst.lines.begin() + static_cast<std::ptrdiff_t>(removeCount));
    }
    ClampScroll(inst);
}

void AddLineInternal(LogViewInstance& inst, const std::wstring& text, LogViewColor color, int level, LineKind kind, LogViewColor timeColor, LogViewColor prefixColor) {
    LogLine line;
    line.text = text;
    line.level = level;
    line.kind = kind;
    line.time = inst.showTime ? FormatCurrentTime(inst) : L"";
    line.textColor = color == 0 ? ColorForLevel(inst, level) : color;
    line.timeColor = timeColor == 0 ? inst.timeColor : timeColor;
    line.prefixColor = prefixColor == 0 ? inst.borderColor : prefixColor;
    inst.lines.push_back(std::move(line));
    EnforceMaxLines(inst);
    if (inst.autoScroll) {
        ScrollToBottom(inst);
    }
    RequestRepaint(inst);
}

ComPtr<ID2D1Bitmap> CaptureParentBackground(LogViewInstance& inst, const RECT& rc) {
    ComPtr<ID2D1Bitmap> result;
    if (!inst.child || !inst.parentHwnd || !inst.renderTarget) {
        return result;
    }

    const int width = std::max(0L, rc.right - rc.left);
    const int height = std::max(0L, rc.bottom - rc.top);
    if (width <= 0 || height <= 0) {
        return result;
    }

    RECT windowRect{};
    GetWindowRect(inst.hwnd, &windowRect);
    POINT parentPoints[2] = {
        { windowRect.left, windowRect.top },
        { windowRect.right, windowRect.bottom }
    };
    MapWindowPoints(nullptr, inst.parentHwnd, parentPoints, 2);

    HDC screenDc = GetDC(nullptr);
    if (!screenDc) {
        return result;
    }
    HDC memoryDc = CreateCompatibleDC(screenDc);
    if (!memoryDc) {
        ReleaseDC(nullptr, screenDc);
        return result;
    }

    BITMAPINFO bitmapInfo{};
    bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth = width;
    bitmapInfo.bmiHeader.biHeight = -height;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    void* pixels = nullptr;
    HBITMAP bitmap = CreateDIBSection(screenDc, &bitmapInfo, DIB_RGB_COLORS, &pixels, nullptr, 0);
    ReleaseDC(nullptr, screenDc);
    if (!bitmap || !pixels) {
        DeleteDC(memoryDc);
        return result;
    }

    HGDIOBJ oldBitmap = SelectObject(memoryDc, bitmap);
    RECT fillRect{ 0, 0, width, height };
    HBRUSH fillBrush = CreateSolidBrush(RGB(42, 54, 66));
    FillRect(memoryDc, &fillRect, fillBrush);
    DeleteObject(fillBrush);

    POINT oldOrigin{};
    SetViewportOrgEx(memoryDc, -parentPoints[0].x, -parentPoints[0].y, &oldOrigin);
    SendMessageW(
        inst.parentHwnd,
        WM_PRINTCLIENT,
        reinterpret_cast<WPARAM>(memoryDc),
        PRF_CLIENT | PRF_ERASEBKGND);
    SetViewportOrgEx(memoryDc, oldOrigin.x, oldOrigin.y, nullptr);

    D2D1_BITMAP_PROPERTIES properties = D2D1::BitmapProperties(
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE));
    inst.renderTarget->CreateBitmap(
        D2D1::SizeU(static_cast<UINT32>(width), static_cast<UINT32>(height)),
        pixels,
        static_cast<UINT32>(width * 4),
        properties,
        result.GetAddressOf());

    SelectObject(memoryDc, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(memoryDc);
    return result;
}

void DrawTextBlock(
    ID2D1HwndRenderTarget* target,
    IDWriteTextFormat* format,
    const std::wstring& text,
    const D2D1_RECT_F& rect,
    LogViewColor color) {
    if (!target || !format || text.empty()) {
        return;
    }
    ComPtr<ID2D1SolidColorBrush> brush;
    if (FAILED(target->CreateSolidColorBrush(ToD2DColor(color), brush.GetAddressOf()))) {
        return;
    }
    target->DrawTextW(text.c_str(), static_cast<UINT32>(text.size()), format, rect, brush.Get());
}

void DrawButton(LogViewInstance& inst, const RECT& rect, const std::wstring& text, bool hover) {
    if (!inst.renderTarget || rect.right <= rect.left || rect.bottom <= rect.top) {
        return;
    }
    ComPtr<ID2D1SolidColorBrush> fill;
    ComPtr<ID2D1SolidColorBrush> border;
    inst.renderTarget->CreateSolidColorBrush(
        hover ? D2D1::ColorF(0.18f, 0.20f, 0.16f, 0.98f) : D2D1::ColorF(0.04f, 0.05f, 0.06f, 0.92f),
        fill.GetAddressOf());
    inst.renderTarget->CreateSolidColorBrush(ToD2DColor(inst.borderColor), border.GetAddressOf());
    const D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(ToRectF(rect), 6.0f, 6.0f);
    if (fill) {
        inst.renderTarget->FillRoundedRectangle(rr, fill.Get());
    }
    if (border) {
        inst.renderTarget->DrawRoundedRectangle(rr, border.Get(), hover ? 1.8f : 1.0f);
    }
    if (inst.textFormatObject) {
        inst.textFormatObject->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        inst.textFormatObject->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        DrawTextBlock(inst.renderTarget.Get(), inst.textFormatObject.Get(), text, ToRectF(rect), inst.titleColor);
        inst.textFormatObject->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    }
}

void DrawCloseButton(LogViewInstance& inst) {
    if (!inst.renderTarget || inst.closeRect.right <= inst.closeRect.left || inst.closeRect.bottom <= inst.closeRect.top) {
        return;
    }
    ComPtr<ID2D1SolidColorBrush> fill;
    ComPtr<ID2D1SolidColorBrush> border;
    ComPtr<ID2D1SolidColorBrush> icon;
    inst.renderTarget->CreateSolidColorBrush(
        inst.closeHover ? D2D1::ColorF(0.34f, 0.09f, 0.10f, 0.98f) : D2D1::ColorF(0.04f, 0.05f, 0.06f, 0.96f),
        fill.GetAddressOf());
    inst.renderTarget->CreateSolidColorBrush(ToD2DColor(inst.borderColor), border.GetAddressOf());
    inst.renderTarget->CreateSolidColorBrush(ToD2DColor(0xFFFFFFFF), icon.GetAddressOf());
    const D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(ToRectF(inst.closeRect), 6.0f, 6.0f);
    if (fill) {
        inst.renderTarget->FillRoundedRectangle(rr, fill.Get());
    }
    if (border) {
        inst.renderTarget->DrawRoundedRectangle(rr, border.Get(), inst.closeHover ? 1.9f : 1.2f);
    }
    if (icon) {
        const float left = static_cast<float>(inst.closeRect.left + 9);
        const float top = static_cast<float>(inst.closeRect.top + 9);
        const float right = static_cast<float>(inst.closeRect.right - 9);
        const float bottom = static_cast<float>(inst.closeRect.bottom - 9);
        inst.renderTarget->DrawLine(D2D1::Point2F(left, top), D2D1::Point2F(right, bottom), icon.Get(), inst.closeHover ? 2.4f : 2.0f);
        inst.renderTarget->DrawLine(D2D1::Point2F(right, top), D2D1::Point2F(left, bottom), icon.Get(), inst.closeHover ? 2.4f : 2.0f);
    }
}

void RenderWindow(LogViewInstance& inst) {
    if (!EnsureRenderTarget(inst) || !EnsureTextFormats(inst)) {
        return;
    }
    ComputeLayout(inst);
    RECT rc{};
    GetClientRect(inst.hwnd, &rc);

    inst.renderTarget->BeginDraw();
    inst.renderTarget->Clear(D2D1::ColorF(0, 0, 0, 0));
    if (inst.child && inst.alpha < 255) {
        ComPtr<ID2D1Bitmap> parentBackground = CaptureParentBackground(inst, rc);
        if (parentBackground) {
            inst.renderTarget->DrawBitmap(
                parentBackground.Get(),
                ToRectF(rc),
                1.0f,
                D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
        }
    }

    ComPtr<ID2D1SolidColorBrush> background;
    ComPtr<ID2D1SolidColorBrush> border;
    ComPtr<ID2D1SolidColorBrush> accent;
    inst.renderTarget->CreateSolidColorBrush(ToD2DColor(WithAlpha(inst.backgroundColor, inst.alpha)), background.GetAddressOf());
    inst.renderTarget->CreateSolidColorBrush(ToD2DColor(inst.borderColor), border.GetAddressOf());
    inst.renderTarget->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.15f), accent.GetAddressOf());

    const D2D1_ROUNDED_RECT body = D2D1::RoundedRect(ToRectF(rc), static_cast<float>(inst.cornerRadius), static_cast<float>(inst.cornerRadius));
    if (background) {
        inst.renderTarget->FillRoundedRectangle(body, background.Get());
    }
    if (border) {
        inst.renderTarget->DrawRoundedRectangle(body, border.Get(), 1.0f);
    }

    if (inst.showTitleBar) {
        RECT headerRect{ 0, 0, rc.right, inst.headerHeight };
        RECT titleRect{
            12,
            12,
            inst.showCloseButton ? std::max(12L, inst.closeRect.left - 16) : rc.right - 12,
            inst.headerHeight - 12
        };
        DrawTextBlock(inst.renderTarget.Get(), inst.titleFormatObject.Get(), inst.title, ToRectF(titleRect), inst.titleColor);
        if (border) {
            inst.renderTarget->DrawLine(
                D2D1::Point2F(8.0f, static_cast<float>(inst.headerHeight - 8)),
                D2D1::Point2F(static_cast<float>(rc.right - 8), static_cast<float>(inst.headerHeight - 8)),
                border.Get(),
                1.0f);
        }
        if (accent) {
            inst.renderTarget->DrawLine(
                D2D1::Point2F(8.0f, 8.0f),
                D2D1::Point2F(static_cast<float>(rc.right - 8), 8.0f),
                accent.Get(),
                1.0f);
        }
        (void)headerRect;
    }

    if (inst.showCloseButton) {
        DrawCloseButton(inst);
    }

    const int firstLine = ClampInt(inst.scrollPosition, 0, std::max(0, static_cast<int>(inst.lines.size())));
    const int visibleLines = std::max(1, static_cast<int>(inst.logRect.bottom - inst.logRect.top) / std::max(1, inst.lineHeight));
    const int endLine = std::min(static_cast<int>(inst.lines.size()), firstLine + visibleLines + 1);
    for (int i = firstLine; i < endLine; ++i) {
        const LogLine& line = inst.lines[static_cast<size_t>(i)];
        const float y = static_cast<float>(inst.logRect.top + (i - firstLine) * inst.lineHeight);
        D2D1_RECT_F rowRect = D2D1::RectF(
            static_cast<float>(inst.logRect.left),
            y,
            static_cast<float>(inst.logRect.right),
            y + static_cast<float>(inst.lineHeight));
        if (!line.time.empty()) {
            D2D1_RECT_F timeRect = rowRect;
            timeRect.right = std::min(timeRect.left + 86.0f, rowRect.right);
            DrawTextBlock(inst.renderTarget.Get(), inst.textFormatObject.Get(), line.time, timeRect, line.timeColor);
            D2D1_RECT_F prefixRect = rowRect;
            prefixRect.left = timeRect.right;
            prefixRect.right = std::min(prefixRect.left + 32.0f, rowRect.right);
            DrawTextBlock(inst.renderTarget.Get(), inst.textFormatObject.Get(), line.kind == LineKind::Colored ? L">>" : L"|", prefixRect, line.prefixColor);
            rowRect.left = prefixRect.right + 2.0f;
        }
        DrawTextBlock(inst.renderTarget.Get(), inst.textFormatObject.Get(), line.text, rowRect, line.textColor);
    }

    if (inst.showScrollBar && inst.scrollTrackRect.bottom > inst.scrollTrackRect.top) {
        ComPtr<ID2D1SolidColorBrush> trackBrush;
        ComPtr<ID2D1SolidColorBrush> thumbBrush;
        inst.renderTarget->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.12f), trackBrush.GetAddressOf());
        inst.renderTarget->CreateSolidColorBrush(ToD2DColor(inst.borderColor), thumbBrush.GetAddressOf());
        const D2D1_ROUNDED_RECT track = D2D1::RoundedRect(ToRectF(inst.scrollTrackRect), 4.0f, 4.0f);
        const D2D1_ROUNDED_RECT thumb = D2D1::RoundedRect(ToRectF(inst.scrollThumbRect), 4.0f, 4.0f);
        if (trackBrush) {
            inst.renderTarget->FillRoundedRectangle(track, trackBrush.Get());
        }
        if (thumbBrush) {
            inst.renderTarget->FillRoundedRectangle(thumb, thumbBrush.Get());
        }
    }

    if (inst.showClearButton) {
        DrawButton(inst, inst.clearRect, inst.clearButtonText, inst.clearHover);
    }

    HRESULT hr = inst.renderTarget->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        ReleaseDeviceResources(inst);
    }
}

std::shared_ptr<LogViewInstance> GetInstance(LogViewHandle handle) {
    if (handle == 0) {
        return {};
    }
    AcquireSRWLockExclusive(&g_instanceLock);
    auto it = g_instances.find(handle);
    if (it == g_instances.end()) {
        ReleaseSRWLockExclusive(&g_instanceLock);
        return {};
    }
    auto result = it->second;
    ReleaseSRWLockExclusive(&g_instanceLock);
    return result;
}

void RegisterInstance(const std::shared_ptr<LogViewInstance>& inst) {
    AcquireSRWLockExclusive(&g_instanceLock);
    g_instances[ToHandle(inst)] = inst;
    ReleaseSRWLockExclusive(&g_instanceLock);
}

void UnregisterInstance(LogViewHandle handle) {
    AcquireSRWLockExclusive(&g_instanceLock);
    g_instances.erase(handle);
    ReleaseSRWLockExclusive(&g_instanceLock);
}

std::vector<std::shared_ptr<LogViewInstance>> SnapshotInstances() {
    std::vector<std::shared_ptr<LogViewInstance>> result;
    AcquireSRWLockExclusive(&g_instanceLock);
    result.reserve(g_instances.size());
    for (auto& pair : g_instances) {
        result.push_back(pair.second);
    }
    ReleaseSRWLockExclusive(&g_instanceLock);
    return result;
}

void ClearRegistry() {
    AcquireSRWLockExclusive(&g_instanceLock);
    g_instances.clear();
    ReleaseSRWLockExclusive(&g_instanceLock);
}

LRESULT HitTest(LogViewInstance& inst, LPARAM lParam) {
    POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
    ScreenToClient(inst.hwnd, &pt);
    ComputeLayout(inst);
    if (IsPointInRect(inst.closeRect, pt) || IsPointInRect(inst.clearRect, pt) || IsPointInRect(inst.scrollTrackRect, pt)) {
        return HTCLIENT;
    }
    RECT rc{};
    GetClientRect(inst.hwnd, &rc);
    const int grip = 8;
    const bool left = pt.x >= rc.left && pt.x < rc.left + grip;
    const bool right = pt.x <= rc.right && pt.x > rc.right - grip;
    const bool top = pt.y >= rc.top && pt.y < rc.top + grip;
    const bool bottom = pt.y <= rc.bottom && pt.y > rc.bottom - grip;
    if (top && left) {
        return HTTOPLEFT;
    }
    if (top && right) {
        return HTTOPRIGHT;
    }
    if (bottom && left) {
        return HTBOTTOMLEFT;
    }
    if (bottom && right) {
        return HTBOTTOMRIGHT;
    }
    if (left) {
        return HTLEFT;
    }
    if (right) {
        return HTRIGHT;
    }
    if (top) {
        return HTTOP;
    }
    if (bottom) {
        return HTBOTTOM;
    }
    if (inst.dragEnabled && inst.showTitleBar && pt.y < inst.headerHeight) {
        return HTCAPTION;
    }
    return HTCLIENT;
}

void DestroyInstanceWindow(LogViewInstance& inst, bool sendEvent) {
    const LogViewHandle handle = reinterpret_cast<LogViewHandle>(&inst);
    UnregisterInstance(handle);
    inst.destroyed = true;
    if (sendEvent) {
        NotifyEvent(inst, LOGVIEW_EVENT_DESTROYED, 0, 0);
    }
    if (inst.hwnd) {
        DestroyWindow(inst.hwnd);
    }
}

void HandleCloseButton(LogViewInstance& inst) {
    NotifyEvent(inst, LOGVIEW_EVENT_CLOSE_CLICKED, 0, 0);
    if (inst.closeMode == LOGVIEW_CLOSE_HIDE) {
        ShowWindow(inst.hwnd, SW_HIDE);
    } else if (inst.closeMode == LOGVIEW_CLOSE_DESTROY) {
        DestroyInstanceWindow(inst, true);
    }
}

void HandleClearButton(LogViewInstance& inst) {
    inst.lines.clear();
    inst.scrollPosition = 0;
    NotifyEvent(inst, LOGVIEW_EVENT_CLEAR_CLICKED, 0, 0);
    RequestRepaint(inst);
}

void ResizeRenderTarget(LogViewInstance& inst) {
    if (!inst.renderTarget || !inst.hwnd) {
        return;
    }
    RECT rc{};
    GetClientRect(inst.hwnd, &rc);
    D2D1_SIZE_U size = D2D1::SizeU(
        static_cast<UINT32>(std::max(1L, rc.right - rc.left)),
        static_cast<UINT32>(std::max(1L, rc.bottom - rc.top)));
    inst.renderTarget->Resize(size);
}

LRESULT CALLBACK LogViewWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* inst = reinterpret_cast<LogViewInstance*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
    case WM_NCCREATE: {
        auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
        inst = reinterpret_cast<LogViewInstance*>(createStruct->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(inst));
        inst->hwnd = hwnd;
        return TRUE;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_NCHITTEST:
        if (inst) {
            return HitTest(*inst, lParam);
        }
        break;
    case WM_GETMINMAXINFO:
        if (inst) {
            auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
            info->ptMinTrackSize.x = inst->minWidth;
            info->ptMinTrackSize.y = inst->minHeight;
            return 0;
        }
        break;
    case WM_SIZE:
        if (inst) {
            ResizeRenderTarget(*inst);
            ApplyRoundCorner(*inst);
            ComputeLayout(*inst);
            NotifyEvent(*inst, LOGVIEW_EVENT_RESIZED, static_cast<uintptr_t>(LOWORD(lParam)), static_cast<uintptr_t>(HIWORD(lParam)));
            RequestRepaint(*inst);
        }
        return 0;
    case WM_DPICHANGED:
        if (inst) {
            auto* suggested = reinterpret_cast<RECT*>(lParam);
            if (suggested) {
                SetWindowPos(
                    hwnd,
                    nullptr,
                    suggested->left,
                    suggested->top,
                    suggested->right - suggested->left,
                    suggested->bottom - suggested->top,
                    SWP_NOZORDER | SWP_NOACTIVATE);
            }
            ReleaseDeviceResources(*inst);
            ReleaseTextFormats(*inst);
            ApplyRoundCorner(*inst);
            ComputeLayout(*inst);
            RequestRepaint(*inst);
        }
        return 0;
    case WM_MOVE:
        if (inst) {
            NotifyEvent(*inst, LOGVIEW_EVENT_MOVED, static_cast<uintptr_t>(static_cast<short>(LOWORD(lParam))), static_cast<uintptr_t>(static_cast<short>(HIWORD(lParam))));
        }
        return 0;
    case WM_PAINT:
        if (inst) {
            PAINTSTRUCT ps{};
            BeginPaint(hwnd, &ps);
            RenderWindow(*inst);
            EndPaint(hwnd, &ps);
            return 0;
        }
        break;
    case WM_MOUSEWHEEL:
        if (inst) {
            const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            const int step = delta > 0 ? -3 : 3;
            SetScrollPositionInternal(*inst, inst->scrollPosition + step, true);
            return 0;
        }
        break;
    case WM_LBUTTONDOWN:
        if (inst) {
            POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ComputeLayout(*inst);
            if (IsPointInRect(inst->closeRect, pt)) {
                HandleCloseButton(*inst);
                return 0;
            }
            if (IsPointInRect(inst->clearRect, pt)) {
                HandleClearButton(*inst);
                return 0;
            }
            if (IsPointInRect(inst->scrollThumbRect, pt)) {
                inst->draggingScroll = true;
                inst->scrollDragOffset = pt.y - inst->scrollThumbRect.top;
                SetCapture(hwnd);
                return 0;
            }
            if (IsPointInRect(inst->logRect, pt)) {
                const int lineIndex = inst->scrollPosition + (pt.y - inst->logRect.top) / std::max(1, inst->lineHeight);
                if (lineIndex >= 0 && lineIndex < static_cast<int>(inst->lines.size())) {
                    NotifyEvent(*inst, LOGVIEW_EVENT_LOG_LINE_CLICKED, static_cast<uintptr_t>(lineIndex), 0);
                }
            }
        }
        break;
    case WM_MOUSEMOVE:
        if (inst) {
            POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ComputeLayout(*inst);
            const bool closeHover = inst->showCloseButton && IsPointInRect(inst->closeRect, pt);
            const bool clearHover = inst->showClearButton && IsPointInRect(inst->clearRect, pt);
            if (closeHover != inst->closeHover || clearHover != inst->clearHover) {
                inst->closeHover = closeHover;
                inst->clearHover = clearHover;
                TRACKMOUSEEVENT tme{};
                tme.cbSize = sizeof(tme);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = hwnd;
                TrackMouseEvent(&tme);
                RequestRepaint(*inst);
            }
        }
        if (inst && inst->draggingScroll) {
            POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ComputeLayout(*inst);
            const int trackHeight = std::max(1, static_cast<int>(inst->scrollTrackRect.bottom - inst->scrollTrackRect.top));
            const int thumbHeight = std::max(1, static_cast<int>(inst->scrollThumbRect.bottom - inst->scrollThumbRect.top));
            const int maxThumbMove = std::max(1, trackHeight - thumbHeight);
            const int thumbTop = ClampInt(pt.y - inst->scrollDragOffset, inst->scrollTrackRect.top, inst->scrollTrackRect.bottom - thumbHeight);
            const int maxScroll = MaxScrollPosition(*inst);
            const int newScroll = maxScroll == 0 ? 0 : (thumbTop - inst->scrollTrackRect.top) * maxScroll / maxThumbMove;
            SetScrollPositionInternal(*inst, newScroll, true);
            return 0;
        }
        break;
    case WM_MOUSELEAVE:
        if (inst && (inst->closeHover || inst->clearHover)) {
            inst->closeHover = false;
            inst->clearHover = false;
            RequestRepaint(*inst);
        }
        return 0;
    case WM_LBUTTONUP:
        if (inst && inst->draggingScroll) {
            inst->draggingScroll = false;
            ReleaseCapture();
            return 0;
        }
        break;
    case WM_NCDESTROY:
        if (inst) {
            ReleaseDeviceResources(*inst);
            ReleaseTextFormats(*inst);
            inst->hwnd = nullptr;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        }
        break;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

bool InitFactories() {
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, g_d2dFactory.GetAddressOf()))) {
        return false;
    }
    IUnknown* factory = nullptr;
    HRESULT hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), &factory);
    if (FAILED(hr) || !factory) {
        return false;
    }
    g_dwriteFactory.Attach(reinterpret_cast<IDWriteFactory*>(factory));
    return true;
}

bool RegisterWindowClass() {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = LogViewWndProc;
    wc.hInstance = g_module ? g_module : GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kWindowClassName;
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    ATOM atom = RegisterClassExW(&wc);
    if (atom == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }
    return true;
}

void DrainTasks() {
    std::queue<std::function<void()>> local;
    {
        AcquireSRWLockExclusive(&g_taskLock);
        std::swap(local, g_tasks);
        ReleaseSRWLockExclusive(&g_taskLock);
    }
    while (!local.empty()) {
        local.front()();
        local.pop();
    }
}

struct UiStartContext {
    HANDLE readyEvent = nullptr;
    bool ok = false;
};

DWORD WINAPI UiThreadMain(LPVOID param) {
    auto* context = reinterpret_cast<UiStartContext*>(param);
    g_uiThreadId = GetCurrentThreadId();
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool ok = RegisterWindowClass();
    if (context) {
        context->ok = ok;
        SetEvent(context->readyEvent);
    }
    if (!ok) {
        CoUninitialize();
        return 1;
    }
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == kTaskMessage) {
            DrainTasks();
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    DrainTasks();
    g_dwriteFactory.Reset();
    g_d2dFactory.Reset();
    UnregisterClassW(kWindowClassName, g_module ? g_module : GetModuleHandleW(nullptr));
    CoUninitialize();
    return 0;
}

bool EnsureRuntime() {
    if (IsRuntimeRunning()) {
        return true;
    }
    AcquireSRWLockExclusive(&g_runtimeLock);
    if (IsRuntimeRunning()) {
        ReleaseSRWLockExclusive(&g_runtimeLock);
        return true;
    }
    EnableDpiAwareness();
    HANDLE readyEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!readyEvent) {
        ReleaseSRWLockExclusive(&g_runtimeLock);
        SetApiError(kErrorRuntime, "Failed to create LogView runtime event.");
        return false;
    }
    UiStartContext context;
    context.readyEvent = readyEvent;
    DWORD threadId = 0;
    HANDLE threadHandle = CreateThread(nullptr, 0, UiThreadMain, &context, 0, &threadId);
    if (!threadHandle) {
        CloseHandle(readyEvent);
        ReleaseSRWLockExclusive(&g_runtimeLock);
        SetApiError(kErrorRuntime, "Failed to start LogView UI thread.");
        return false;
    }
    WaitForSingleObject(readyEvent, INFINITE);
    CloseHandle(readyEvent);
    if (!context.ok) {
        WaitForSingleObject(threadHandle, INFINITE);
        CloseHandle(threadHandle);
        g_uiThreadId = 0;
        g_uiThreadHandle = nullptr;
        ReleaseSRWLockExclusive(&g_runtimeLock);
        SetApiError(kErrorRuntime, "Failed to initialize LogView UI runtime.");
        return false;
    }
    g_uiThreadHandle = threadHandle;
    g_uiThreadId = threadId;
    SetRuntimeRunning(true);
    ReleaseSRWLockExclusive(&g_runtimeLock);
    SetApiError(kErrorOk, "OK");
    return true;
}

template <typename F>
auto InvokeUi(F&& fn) -> decltype(fn()) {
    using Result = decltype(fn());
    if (!EnsureRuntime()) {
        if constexpr (std::is_void_v<Result>) {
            return;
        } else {
            return Result{};
        }
    }
    if (GetCurrentThreadId() == g_uiThreadId) {
        return fn();
    }
    HANDLE doneEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!doneEvent) {
        if constexpr (std::is_void_v<Result>) {
            return;
        } else {
            return Result{};
        }
    }
    if constexpr (std::is_void_v<Result>) {
        auto taskFn = std::forward<F>(fn);
        AcquireSRWLockExclusive(&g_taskLock);
        g_tasks.emplace([taskFn, doneEvent]() mutable {
            taskFn();
            SetEvent(doneEvent);
        });
        ReleaseSRWLockExclusive(&g_taskLock);
    } else {
        auto result = std::make_shared<Result>();
        auto taskFn = std::forward<F>(fn);
        AcquireSRWLockExclusive(&g_taskLock);
        g_tasks.emplace([taskFn, result, doneEvent]() mutable {
            *result = taskFn();
            SetEvent(doneEvent);
        });
        ReleaseSRWLockExclusive(&g_taskLock);
        PostThreadMessageW(g_uiThreadId, kTaskMessage, 0, 0);
        WaitForSingleObject(doneEvent, INFINITE);
        CloseHandle(doneEvent);
        return *result;
    }
    PostThreadMessageW(g_uiThreadId, kTaskMessage, 0, 0);
    WaitForSingleObject(doneEvent, INFINITE);
    CloseHandle(doneEvent);
}

LogViewHandle CreateWindowInstance(bool child, HWND parentHwnd, int x, int y, int width, int height, int alpha) {
    if (width <= 0 || height <= 0) {
        SetApiError(kErrorInvalidArgument, "Window size must be positive.");
        return 0;
    }
    auto createOnCurrentThread = [=]() -> LogViewHandle {
        auto inst = std::make_shared<LogViewInstance>();
        inst->child = child;
        inst->parentHwnd = parentHwnd;
        inst->alpha = ClampInt(alpha, 0, 255);
        inst->order = static_cast<uint64_t>(InterlockedIncrement64(&g_nextOrder));
        const UINT dpi = GetDpiForTargetWindow(parentHwnd);
        const int finalWidth = child ? std::max(1, width) : std::max(ScaleByDpi(width, dpi), ScaleByDpi(inst->minWidth, dpi));
        const int finalHeight = child ? std::max(1, height) : std::max(ScaleByDpi(height, dpi), ScaleByDpi(inst->minHeight, dpi));

        int finalX = x;
        int finalY = y;
        if (!child) {
            RECT work{};
            SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
            finalX = work.left + ((work.right - work.left) - finalWidth) / 2;
            finalY = work.top + ((work.bottom - work.top) - finalHeight) / 3;
        }

        const DWORD style = child ? (WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS) : (WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
        const DWORD exStyle = child ? 0 : (WS_EX_LAYERED | WS_EX_TOOLWINDOW);
        HWND hwnd = CreateWindowExW(
            exStyle,
            kWindowClassName,
            inst->title.c_str(),
            style,
            finalX,
            finalY,
            finalWidth,
            finalHeight,
            parentHwnd,
            nullptr,
            g_module ? g_module : GetModuleHandleW(nullptr),
            inst.get());
        if (!hwnd) {
            DWORD error = GetLastError();
            char message[128]{};
            sprintf_s(message, "CreateWindowEx failed. Win32 error: %lu.", static_cast<unsigned long>(error));
            SetApiError(kErrorWindow, message);
            return static_cast<LogViewHandle>(0);
        }
        inst->hwnd = hwnd;
        inst->ownerThreadId = GetCurrentThreadId();
        RegisterInstance(inst);
        ApplyAlpha(*inst);
        ApplyRoundCorner(*inst);
        ComputeLayout(*inst);
        RequestRepaint(*inst);
        return ToHandle(inst);
    };

    if (child) {
        if (!EnsureRuntime()) {
            return 0;
        }
        return createOnCurrentThread();
    }
    return InvokeUi(createOnCurrentThread);
}

int ExecuteOnInstance(LogViewHandle handle, const std::function<int(LogViewInstance&)>& fn) {
    auto inst = GetInstance(handle);
    if (!inst || inst->destroyed) {
        SetApiError(kErrorInvalidHandle, "Invalid LogView handle.");
        return 0;
    }
    if (inst->child && inst->ownerThreadId == GetCurrentThreadId()) {
        if (inst->destroyed || !inst->hwnd) {
            return 0;
        }
        return fn(*inst);
    }
    return InvokeUi([inst, fn]() -> int {
        if (inst->destroyed || !inst->hwnd) {
            return 0;
        }
        return fn(*inst);
    });
}

struct LogTextSnapshot {
    bool ok = false;
    std::string text;
};

LogTextSnapshot SnapshotLogText(LogViewHandle handle) {
    auto inst = GetInstance(handle);
    if (!inst || inst->destroyed) {
        SetApiError(kErrorInvalidHandle, "Invalid LogView handle.");
        return {};
    }
    if (inst->child && inst->ownerThreadId == GetCurrentThreadId()) {
        if (inst->destroyed || !inst->hwnd) {
            SetApiError(kErrorInvalidHandle, "Invalid LogView handle.");
            return {};
        }
        return { true, BuildLogTextUtf8(*inst) };
    }
    LogTextSnapshot snapshot = InvokeUi([inst]() -> LogTextSnapshot {
        if (inst->destroyed || !inst->hwnd) {
            return {};
        }
        return { true, BuildLogTextUtf8(*inst) };
    });
    if (!snapshot.ok) {
        SetApiError(kErrorInvalidHandle, "Invalid LogView handle.");
    }
    return snapshot;
}

bool WriteUtf8File(const std::wstring& path, const std::string& text) {
    HANDLE file = CreateFileW(
        path.c_str(),
        GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        SetWin32ApiError("Failed to create log file.", GetLastError());
        return false;
    }

    const char* data = text.data();
    size_t remaining = text.size();
    while (remaining > 0) {
        const DWORD chunk = static_cast<DWORD>(std::min<size_t>(
            remaining,
            static_cast<size_t>(std::numeric_limits<DWORD>::max())));
        DWORD written = 0;
        if (!WriteFile(file, data, chunk, &written, nullptr)) {
            const DWORD error = GetLastError();
            CloseHandle(file);
            SetWin32ApiError("Failed to write log file.", error);
            return false;
        }
        if (written != chunk) {
            CloseHandle(file);
            SetApiError(kErrorRuntime, "Failed to write complete log file.");
            return false;
        }
        data += written;
        remaining -= written;
    }

    if (!CloseHandle(file)) {
        SetWin32ApiError("Failed to close log file.", GetLastError());
        return false;
    }
    return true;
}

} // namespace

BOOL APIENTRY DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = instance;
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}

LOGVIEW_API int LOGVIEW_CALL LogView_Init(void) {
    return EnsureRuntime() ? 1 : 0;
}

LOGVIEW_API int LOGVIEW_CALL LogView_Uninit(void) {
    if (!IsRuntimeRunning()) {
        SetApiError(kErrorOk, "OK");
        return 1;
    }
    LogView_DestroyAll();
    AcquireSRWLockExclusive(&g_runtimeLock);
    if (IsRuntimeRunning()) {
        PostThreadMessageW(g_uiThreadId, WM_QUIT, 0, 0);
        if (g_uiThreadHandle) {
            WaitForSingleObject(g_uiThreadHandle, INFINITE);
            CloseHandle(g_uiThreadHandle);
            g_uiThreadHandle = nullptr;
        }
        g_uiThreadId = 0;
        SetRuntimeRunning(false);
    }
    ReleaseSRWLockExclusive(&g_runtimeLock);
    SetApiError(kErrorOk, "OK");
    return 1;
}

LOGVIEW_API LogViewHandle LOGVIEW_CALL LogView_CreatePopup(int width, int height) {
    return LogView_CreatePopupEx(width, height, kDefaultAlpha);
}

LOGVIEW_API LogViewHandle LOGVIEW_CALL LogView_CreatePopupEx(int width, int height, int alpha) {
    LogViewHandle handle = CreateWindowInstance(false, nullptr, 0, 0, width, height, alpha);
    if (handle == 0) {
        SetApiError(kErrorWindow, "Failed to create popup LogView window.");
    } else {
        SetApiError(kErrorOk, "OK");
    }
    return handle;
}

LOGVIEW_API LogViewHandle LOGVIEW_CALL LogView_CreateChild(LogViewHwnd parentHwnd, int x, int y, int width, int height) {
    return LogView_CreateChildEx(parentHwnd, x, y, width, height, kDefaultAlpha);
}

LOGVIEW_API LogViewHandle LOGVIEW_CALL LogView_CreateChildEx(LogViewHwnd parentHwnd, int x, int y, int width, int height, int alpha) {
    HWND parent = reinterpret_cast<HWND>(parentHwnd);
    if (!parent) {
        SetApiError(kErrorInvalidArgument, "Parent HWND is required for child LogView windows.");
        return 0;
    }
    LogViewHandle handle = CreateWindowInstance(true, parent, x, y, width, height, alpha);
    if (handle == 0) {
        SetApiError(kErrorWindow, "Failed to create child LogView window.");
    } else {
        SetApiError(kErrorOk, "OK");
    }
    return handle;
}

LOGVIEW_API int LOGVIEW_CALL LogView_Destroy(LogViewHandle handle) {
    auto inst = GetInstance(handle);
    if (!inst) {
        SetApiError(kErrorInvalidHandle, "Invalid LogView handle.");
        return 0;
    }
    if (inst->child && inst->ownerThreadId == GetCurrentThreadId()) {
        DestroyInstanceWindow(*inst, true);
        SetApiError(kErrorOk, "OK");
        return 1;
    }
    InvokeUi([inst]() {
        DestroyInstanceWindow(*inst, true);
    });
    SetApiError(kErrorOk, "OK");
    return 1;
}

LOGVIEW_API int LOGVIEW_CALL LogView_DestroyAll(void) {
    auto instances = SnapshotInstances();
    ClearRegistry();
    InvokeUi([instances]() {
        for (auto& inst : instances) {
            if (inst && !inst->destroyed) {
                inst->destroyed = true;
                NotifyEvent(*inst, LOGVIEW_EVENT_DESTROYED, 0, 0);
                if (inst->hwnd) {
                    DestroyWindow(inst->hwnd);
                }
            }
        }
    });
    SetApiError(kErrorOk, "OK");
    return 1;
}

LOGVIEW_API int LOGVIEW_CALL LogView_Show(LogViewHandle handle) {
    return ExecuteOnInstance(handle, [](LogViewInstance& inst) {
        ShowWindow(inst.hwnd, inst.child ? SW_SHOW : SW_SHOWNORMAL);
        UpdateWindow(inst.hwnd);
        return 1;
    });
}

LOGVIEW_API int LOGVIEW_CALL LogView_Hide(LogViewHandle handle) {
    return ExecuteOnInstance(handle, [](LogViewInstance& inst) {
        ShowWindow(inst.hwnd, SW_HIDE);
        return 1;
    });
}

LOGVIEW_API int LOGVIEW_CALL LogView_Close(LogViewHandle handle) {
    return ExecuteOnInstance(handle, [](LogViewInstance& inst) {
        HandleCloseButton(inst);
        return 1;
    });
}

LOGVIEW_API int LOGVIEW_CALL LogView_IsVisible(LogViewHandle handle) {
    return ExecuteOnInstance(handle, [](LogViewInstance& inst) {
        return IsWindowVisible(inst.hwnd) ? 1 : 0;
    });
}

LOGVIEW_API int LOGVIEW_CALL LogView_SetCloseMode(LogViewHandle handle, int mode) {
    return ExecuteOnInstance(handle, [mode](LogViewInstance& inst) {
        inst.closeMode = ClampInt(mode, LOGVIEW_CLOSE_HIDE, LOGVIEW_CLOSE_NOTIFY_ONLY);
        return 1;
    });
}

LOGVIEW_API int LOGVIEW_CALL LogView_SetPosition(LogViewHandle handle, int x, int y) {
    return ExecuteOnInstance(handle, [x, y](LogViewInstance& inst) {
        SetWindowPos(inst.hwnd, nullptr, ScaleForWindow(inst, x), ScaleForWindow(inst, y), 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        return 1;
    });
}

LOGVIEW_API int LOGVIEW_CALL LogView_SetSize(LogViewHandle handle, int width, int height) {
    return ExecuteOnInstance(handle, [width, height](LogViewInstance& inst) {
        SetWindowPos(inst.hwnd, nullptr, 0, 0,
            inst.child ? std::max(1, width) : std::max(ScaleForWindow(inst, width), ScaleForWindow(inst, inst.minWidth)),
            inst.child ? std::max(1, height) : std::max(ScaleForWindow(inst, height), ScaleForWindow(inst, inst.minHeight)),
            SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        return 1;
    });
}

LOGVIEW_API int LOGVIEW_CALL LogView_SetRect(LogViewHandle handle, int x, int y, int width, int height) {
    return ExecuteOnInstance(handle, [x, y, width, height](LogViewInstance& inst) {
        SetWindowPos(inst.hwnd, nullptr,
            ScaleForWindow(inst, x),
            ScaleForWindow(inst, y),
            inst.child ? std::max(1, width) : std::max(ScaleForWindow(inst, width), ScaleForWindow(inst, inst.minWidth)),
            inst.child ? std::max(1, height) : std::max(ScaleForWindow(inst, height), ScaleForWindow(inst, inst.minHeight)),
            SWP_NOZORDER | SWP_NOACTIVATE);
        return 1;
    });
}

LOGVIEW_API int LOGVIEW_CALL LogView_GetRect(LogViewHandle handle, int* outX, int* outY, int* outWidth, int* outHeight) {
    if (!outX || !outY || !outWidth || !outHeight) {
        SetApiError(kErrorInvalidArgument, "Output rectangle pointers are required.");
        return 0;
    }
    return ExecuteOnInstance(handle, [outX, outY, outWidth, outHeight](LogViewInstance& inst) {
        RECT rect{};
        GetWindowRect(inst.hwnd, &rect);
        if (inst.child && inst.parentHwnd) {
            POINT pts[2] = { { rect.left, rect.top }, { rect.right, rect.bottom } };
            MapWindowPoints(nullptr, inst.parentHwnd, pts, 2);
            rect.left = pts[0].x;
            rect.top = pts[0].y;
            rect.right = pts[1].x;
            rect.bottom = pts[1].y;
        }
        *outX = rect.left;
        *outY = rect.top;
        *outWidth = rect.right - rect.left;
        *outHeight = rect.bottom - rect.top;
        return 1;
    });
}

LOGVIEW_API int LOGVIEW_CALL LogView_CenterScreen(LogViewHandle handle) {
    return ExecuteOnInstance(handle, [](LogViewInstance& inst) {
        RECT rect{};
        RECT work{};
        GetWindowRect(inst.hwnd, &rect);
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
        const int width = rect.right - rect.left;
        const int height = rect.bottom - rect.top;
        const int x = work.left + ((work.right - work.left) - width) / 2;
        const int y = work.top + ((work.bottom - work.top) - height) / 2;
        SetWindowPos(inst.hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        return 1;
    });
}

LOGVIEW_API int LOGVIEW_CALL LogView_EnableDrag(LogViewHandle handle, int enable) {
    return ExecuteOnInstance(handle, [enable](LogViewInstance& inst) {
        inst.dragEnabled = enable != 0;
        return 1;
    });
}

LOGVIEW_API int LOGVIEW_CALL LogView_AttachToParent(LogViewHandle handle, LogViewHwnd parentHwnd, int x, int y, int width, int height) {
    HWND parent = reinterpret_cast<HWND>(parentHwnd);
    if (!parent) {
        SetApiError(kErrorInvalidArgument, "Parent HWND is required.");
        return 0;
    }
    return ExecuteOnInstance(handle, [parent, x, y, width, height](LogViewInstance& inst) {
        SetParent(inst.hwnd, parent);
        inst.parentHwnd = parent;
        inst.child = true;
        LONG_PTR style = GetWindowLongPtrW(inst.hwnd, GWL_STYLE);
        style &= ~WS_POPUP;
        style |= WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
        SetWindowLongPtrW(inst.hwnd, GWL_STYLE, style);
        SetWindowPos(inst.hwnd, nullptr,
            x,
            y,
            std::max(1, width),
            std::max(1, height),
            SWP_NOZORDER | SWP_FRAMECHANGED);
        ApplyAlpha(inst);
        ApplyRoundCorner(inst);
        RequestRepaint(inst);
        return 1;
    });
}

LOGVIEW_API int LOGVIEW_CALL LogView_DetachToDesktop(LogViewHandle handle) {
    return ExecuteOnInstance(handle, [](LogViewInstance& inst) {
        RECT rect{};
        GetWindowRect(inst.hwnd, &rect);
        SetParent(inst.hwnd, nullptr);
        inst.parentHwnd = nullptr;
        inst.child = false;
        LONG_PTR style = GetWindowLongPtrW(inst.hwnd, GWL_STYLE);
        style &= ~WS_CHILD;
        style |= WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
        SetWindowLongPtrW(inst.hwnd, GWL_STYLE, style);
        SetWindowPos(inst.hwnd, nullptr, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, SWP_FRAMECHANGED);
        ApplyTopMost(inst);
        ApplyAlpha(inst);
        ApplyRoundCorner(inst);
        RequestRepaint(inst);
        return 1;
    });
}

LOGVIEW_API LogViewHwnd LOGVIEW_CALL LogView_GetWindowHandle(LogViewHandle handle) {
    auto inst = GetInstance(handle);
    if (!inst || inst->destroyed) {
        SetApiError(kErrorInvalidHandle, "Invalid LogView handle.");
        return 0;
    }
    return InvokeUi([inst]() -> LogViewHwnd {
        return reinterpret_cast<LogViewHwnd>(inst->hwnd);
    });
}

LOGVIEW_API int LOGVIEW_CALL LogView_SetTopMost(LogViewHandle handle, int enable) {
    return ExecuteOnInstance(handle, [enable](LogViewInstance& inst) {
        inst.topMost = enable != 0;
        ApplyTopMost(inst);
        return 1;
    });
}

LOGVIEW_API int LOGVIEW_CALL LogView_SetAlpha(LogViewHandle handle, int alpha) {
    return ExecuteOnInstance(handle, [alpha](LogViewInstance& inst) {
        inst.alpha = ClampInt(alpha, 0, 255);
        ApplyAlpha(inst);
        return 1;
    });
}

LOGVIEW_API int LOGVIEW_CALL LogView_SetBackgroundColor(LogViewHandle handle, LogViewColor color) {
    return ExecuteOnInstance(handle, [color](LogViewInstance& inst) {
        inst.backgroundColor = color;
        RequestRepaint(inst);
        return 1;
    });
}

LOGVIEW_API int LOGVIEW_CALL LogView_SetBorderColor(LogViewHandle handle, LogViewColor color) {
    return ExecuteOnInstance(handle, [color](LogViewInstance& inst) {
        inst.borderColor = color;
        RequestRepaint(inst);
        return 1;
    });
}

LOGVIEW_API int LOGVIEW_CALL LogView_SetRoundCorner(LogViewHandle handle, int radius) {
    return ExecuteOnInstance(handle, [radius](LogViewInstance& inst) {
        inst.cornerRadius = std::max(0, radius);
        ApplyRoundCorner(inst);
        RequestRepaint(inst);
        return 1;
    });
}

LOGVIEW_API int LOGVIEW_CALL LogView_SetTitle(LogViewHandle handle, const char* text) {
    std::wstring value = Utf8ToWide(text);
    return ExecuteOnInstance(handle, [value](LogViewInstance& inst) {
        inst.title = value;
        SetWindowTextW(inst.hwnd, inst.title.c_str());
        RequestRepaint(inst);
        return 1;
    });
}

LOGVIEW_API int LOGVIEW_CALL LogView_ShowTitleBar(LogViewHandle handle, int enable) {
    return ExecuteOnInstance(handle, [enable](LogViewInstance& inst) {
        inst.showTitleBar = enable != 0;
        ComputeLayout(inst);
        RequestRepaint(inst);
        return 1;
    });
}

LOGVIEW_API int LOGVIEW_CALL LogView_ShowCloseButton(LogViewHandle handle, int enable) {
    return ExecuteOnInstance(handle, [enable](LogViewInstance& inst) {
        inst.showCloseButton = enable != 0;
        ComputeLayout(inst);
        RequestRepaint(inst);
        return 1;
    });
}

LOGVIEW_API int LOGVIEW_CALL LogView_ShowClearButton(LogViewHandle handle, int enable) {
    return ExecuteOnInstance(handle, [enable](LogViewInstance& inst) {
        inst.showClearButton = enable != 0;
        ComputeLayout(inst);
        RequestRepaint(inst);
        return 1;
    });
}

LOGVIEW_API int LOGVIEW_CALL LogView_SetClearButtonText(LogViewHandle handle, const char* text) {
    std::wstring value = Utf8ToWide(text);
    return ExecuteOnInstance(handle, [value](LogViewInstance& inst) {
        inst.clearButtonText = value.empty() ? L"清空" : value;
        RequestRepaint(inst);
        return 1;
    });
}

LOGVIEW_API int LOGVIEW_CALL LogView_SetCloseButtonText(LogViewHandle handle, const char* text) {
    std::wstring value = Utf8ToWide(text);
    return ExecuteOnInstance(handle, [value](LogViewInstance& inst) {
        inst.closeButtonText = value.empty() ? L"X" : value;
        RequestRepaint(inst);
        return 1;
    });
}

LOGVIEW_API int LOGVIEW_CALL LogView_ShowScrollBar(LogViewHandle handle, int enable) {
    return ExecuteOnInstance(handle, [enable](LogViewInstance& inst) {
        inst.showScrollBar = enable != 0;
        ComputeLayout(inst);
        RequestRepaint(inst);
        return 1;
    });
}

LOGVIEW_API int LOGVIEW_CALL LogView_SetAutoScroll(LogViewHandle handle, int enable) {
    return ExecuteOnInstance(handle, [enable](LogViewInstance& inst) {
        inst.autoScroll = enable != 0;
        if (inst.autoScroll) {
            ScrollToBottom(inst);
        }
        RequestRepaint(inst);
        return 1;
    });
}

LOGVIEW_API int LOGVIEW_CALL LogView_ScrollToTop(LogViewHandle handle) {
    return ExecuteOnInstance(handle, [](LogViewInstance& inst) {
        SetScrollPositionInternal(inst, 0, true);
        return 1;
    });
}

LOGVIEW_API int LOGVIEW_CALL LogView_ScrollToBottom(LogViewHandle handle) {
    return ExecuteOnInstance(handle, [](LogViewInstance& inst) {
        ComputeLayout(inst);
        SetScrollPositionInternal(inst, MaxScrollPosition(inst), true);
        return 1;
    });
}

LOGVIEW_API int LOGVIEW_CALL LogView_SetScrollPosition(LogViewHandle handle, int position) {
    return ExecuteOnInstance(handle, [position](LogViewInstance& inst) {
        SetScrollPositionInternal(inst, position, true);
        return 1;
    });
}

LOGVIEW_API int LOGVIEW_CALL LogView_GetScrollPosition(LogViewHandle handle) {
    return ExecuteOnInstance(handle, [](LogViewInstance& inst) {
        return inst.scrollPosition;
    });
}

LOGVIEW_API int LOGVIEW_CALL LogView_AddText(LogViewHandle handle, const char* text) {
    std::wstring value = Utf8ToWide(text);
    return ExecuteOnInstance(handle, [value](LogViewInstance& inst) {
        if (inst.lines.empty()) {
            AddLineInternal(inst, value, inst.textColor, LOGVIEW_LEVEL_INFO, LineKind::Normal, inst.timeColor, inst.borderColor);
        } else {
            inst.lines.back().text += value;
            if (inst.autoScroll) {
                ScrollToBottom(inst);
            }
            RequestRepaint(inst);
        }
        return 1;
    });
}

LOGVIEW_API int LOGVIEW_CALL LogView_AddLine(LogViewHandle handle, const char* text) {
    return LogView_AddLineEx(handle, text, 0, LOGVIEW_LEVEL_INFO);
}

LOGVIEW_API int LOGVIEW_CALL LogView_AddLineEx(LogViewHandle handle, const char* text, LogViewColor color, int level) {
    std::wstring value = Utf8ToWide(text);
    const int safeLevel = ClampInt(level, LOGVIEW_LEVEL_INFO, LOGVIEW_LEVEL_CUSTOM);
    return ExecuteOnInstance(handle, [value, color, safeLevel](LogViewInstance& inst) {
        AddLineInternal(inst, value, color == 0 ? ColorForLevel(inst, safeLevel) : color, safeLevel, LineKind::Normal, inst.timeColor, inst.borderColor);
        return 1;
    });
}

LOGVIEW_API int LOGVIEW_CALL LogView_AddColoredLine(LogViewHandle handle, LogViewColor timeColor, LogViewColor prefixColor, LogViewColor textColor, const char* text) {
    std::wstring value = Utf8ToWide(text);
    return ExecuteOnInstance(handle, [value, timeColor, prefixColor, textColor](LogViewInstance& inst) {
        AddLineInternal(inst, value, textColor, LOGVIEW_LEVEL_CUSTOM, LineKind::Colored, timeColor, prefixColor);
        return 1;
    });
}

LOGVIEW_API int LOGVIEW_CALL LogView_Clear(LogViewHandle handle) {
    return ExecuteOnInstance(handle, [](LogViewInstance& inst) {
        inst.lines.clear();
        inst.scrollPosition = 0;
        RequestRepaint(inst);
        return 1;
    });
}

LOGVIEW_API int LOGVIEW_CALL LogView_GetLineCount(LogViewHandle handle) {
    return ExecuteOnInstance(handle, [](LogViewInstance& inst) {
        return static_cast<int>(inst.lines.size());
    });
}

LOGVIEW_API int LOGVIEW_CALL LogView_GetText(LogViewHandle handle, char* buffer, int bufferSize) {
    if (bufferSize < 0) {
        SetApiError(kErrorInvalidArgument, "Buffer size must not be negative.");
        return 0;
    }
    if (!buffer && bufferSize > 0) {
        SetApiError(kErrorInvalidArgument, "Output buffer is required when buffer size is positive.");
        return 0;
    }

    LogTextSnapshot snapshot = SnapshotLogText(handle);
    if (!snapshot.ok) {
        return 0;
    }
    if (snapshot.text.size() > static_cast<size_t>(std::numeric_limits<int>::max() - 1)) {
        SetApiError(kErrorRuntime, "Log text is too large.");
        return 0;
    }

    const int requiredSize = static_cast<int>(snapshot.text.size() + 1);
    if (buffer && bufferSize > 0) {
        const int copySize = std::min(bufferSize - 1, requiredSize - 1);
        if (copySize > 0) {
            std::memcpy(buffer, snapshot.text.data(), static_cast<size_t>(copySize));
        }
        buffer[copySize] = '\0';
    }

    SetApiError(kErrorOk, "OK");
    return requiredSize;
}

LOGVIEW_API int LOGVIEW_CALL LogView_SaveToFile(LogViewHandle handle, const char* path) {
    if (!path || path[0] == '\0') {
        SetApiError(kErrorInvalidArgument, "File path is required.");
        return 0;
    }
    std::wstring filePath = Utf8ToWide(path);
    if (filePath.empty()) {
        SetApiError(kErrorInvalidArgument, "File path must be valid UTF-8.");
        return 0;
    }

    LogTextSnapshot snapshot = SnapshotLogText(handle);
    if (!snapshot.ok) {
        return 0;
    }
    if (!WriteUtf8File(filePath, snapshot.text)) {
        return 0;
    }

    SetApiError(kErrorOk, "OK");
    return 1;
}

LOGVIEW_API int LOGVIEW_CALL LogView_SetMaxLines(LogViewHandle handle, int maxLines) {
    return ExecuteOnInstance(handle, [maxLines](LogViewInstance& inst) {
        inst.maxLines = std::max(1, maxLines);
        EnforceMaxLines(inst);
        RequestRepaint(inst);
        return 1;
    });
}

LOGVIEW_API int LOGVIEW_CALL LogView_SetFont(LogViewHandle handle, const char* fontName, int fontSize) {
    std::wstring value = Utf8ToWide(fontName);
    return ExecuteOnInstance(handle, [value, fontSize](LogViewInstance& inst) {
        if (!value.empty()) {
            inst.fontName = value;
        }
        inst.fontSize = std::max(8, fontSize);
        ReleaseTextFormats(inst);
        RequestRepaint(inst);
        return 1;
    });
}

LOGVIEW_API int LOGVIEW_CALL LogView_SetTextColor(LogViewHandle handle, LogViewColor color) {
    return ExecuteOnInstance(handle, [color](LogViewInstance& inst) {
        inst.textColor = color;
        inst.infoColor = color;
        RequestRepaint(inst);
        return 1;
    });
}

LOGVIEW_API int LOGVIEW_CALL LogView_SetTimeColor(LogViewHandle handle, LogViewColor color) {
    return ExecuteOnInstance(handle, [color](LogViewInstance& inst) {
        inst.timeColor = color;
        RequestRepaint(inst);
        return 1;
    });
}

LOGVIEW_API int LOGVIEW_CALL LogView_SetInfoColor(LogViewHandle handle, LogViewColor color) {
    return ExecuteOnInstance(handle, [color](LogViewInstance& inst) {
        inst.infoColor = color;
        RequestRepaint(inst);
        return 1;
    });
}

LOGVIEW_API int LOGVIEW_CALL LogView_SetSuccessColor(LogViewHandle handle, LogViewColor color) {
    return ExecuteOnInstance(handle, [color](LogViewInstance& inst) {
        inst.successColor = color;
        RequestRepaint(inst);
        return 1;
    });
}

LOGVIEW_API int LOGVIEW_CALL LogView_SetWarningColor(LogViewHandle handle, LogViewColor color) {
    return ExecuteOnInstance(handle, [color](LogViewInstance& inst) {
        inst.warningColor = color;
        RequestRepaint(inst);
        return 1;
    });
}

LOGVIEW_API int LOGVIEW_CALL LogView_SetErrorColor(LogViewHandle handle, LogViewColor color) {
    return ExecuteOnInstance(handle, [color](LogViewInstance& inst) {
        inst.errorColor = color;
        RequestRepaint(inst);
        return 1;
    });
}

LOGVIEW_API int LOGVIEW_CALL LogView_SetDebugColor(LogViewHandle handle, LogViewColor color) {
    return ExecuteOnInstance(handle, [color](LogViewInstance& inst) {
        inst.debugColor = color;
        RequestRepaint(inst);
        return 1;
    });
}

LOGVIEW_API int LOGVIEW_CALL LogView_ShowTime(LogViewHandle handle, int enable) {
    return ExecuteOnInstance(handle, [enable](LogViewInstance& inst) {
        inst.showTime = enable != 0;
        return 1;
    });
}

LOGVIEW_API int LOGVIEW_CALL LogView_SetTimeFormat(LogViewHandle handle, const char* format) {
    std::wstring value = Utf8ToWide(format);
    return ExecuteOnInstance(handle, [value](LogViewInstance& inst) {
        inst.timeFormat = value.empty() ? L"HH:mm:ss" : value;
        return 1;
    });
}

LOGVIEW_API int LOGVIEW_CALL LogView_SetPadding(LogViewHandle handle, int left, int top, int right, int bottom) {
    return ExecuteOnInstance(handle, [left, top, right, bottom](LogViewInstance& inst) {
        inst.paddingLeft = std::max(0, left);
        inst.paddingTop = std::max(0, top);
        inst.paddingRight = std::max(0, right);
        inst.paddingBottom = std::max(0, bottom);
        ComputeLayout(inst);
        RequestRepaint(inst);
        return 1;
    });
}

LOGVIEW_API int LOGVIEW_CALL LogView_SetLineHeight(LogViewHandle handle, int height) {
    return ExecuteOnInstance(handle, [height](LogViewInstance& inst) {
        inst.lineHeight = std::max(12, height);
        ComputeLayout(inst);
        RequestRepaint(inst);
        return 1;
    });
}

LOGVIEW_API int LOGVIEW_CALL LogView_SetHeaderHeight(LogViewHandle handle, int height) {
    return ExecuteOnInstance(handle, [height](LogViewInstance& inst) {
        inst.headerHeight = std::max(0, height);
        ComputeLayout(inst);
        RequestRepaint(inst);
        return 1;
    });
}

LOGVIEW_API int LOGVIEW_CALL LogView_SetButtonAreaHeight(LogViewHandle handle, int height) {
    return ExecuteOnInstance(handle, [height](LogViewInstance& inst) {
        inst.buttonAreaHeight = std::max(0, height);
        ComputeLayout(inst);
        RequestRepaint(inst);
        return 1;
    });
}

LOGVIEW_API int LOGVIEW_CALL LogView_SetScrollBarWidth(LogViewHandle handle, int width) {
    return ExecuteOnInstance(handle, [width](LogViewInstance& inst) {
        inst.scrollBarWidth = std::max(2, width);
        ComputeLayout(inst);
        RequestRepaint(inst);
        return 1;
    });
}

LOGVIEW_API int LOGVIEW_CALL LogView_SetCallback(LogViewHandle handle, LogViewCallback callback, uintptr_t userData) {
    return ExecuteOnInstance(handle, [callback, userData](LogViewInstance& inst) {
        inst.callback = callback;
        inst.callbackUserData = userData;
        return 1;
    });
}

LOGVIEW_API int LOGVIEW_CALL LogView_SetNotifyWindow(LogViewHandle handle, LogViewHwnd hwnd, int messageId) {
    return ExecuteOnInstance(handle, [hwnd, messageId](LogViewInstance& inst) {
        inst.notifyHwnd = reinterpret_cast<HWND>(hwnd);
        inst.notifyMessage = static_cast<UINT>(std::max(0, messageId));
        return 1;
    });
}

LOGVIEW_API int LOGVIEW_CALL LogView_SetUserData(LogViewHandle handle, uintptr_t userData) {
    return ExecuteOnInstance(handle, [userData](LogViewInstance& inst) {
        inst.userData = userData;
        return 1;
    });
}

LOGVIEW_API uintptr_t LOGVIEW_CALL LogView_GetUserData(LogViewHandle handle) {
    auto inst = GetInstance(handle);
    if (!inst || inst->destroyed) {
        SetApiError(kErrorInvalidHandle, "Invalid LogView handle.");
        return 0;
    }
    return InvokeUi([inst]() -> uintptr_t {
        return inst->userData;
    });
}

LOGVIEW_API int LOGVIEW_CALL LogView_SetName(LogViewHandle handle, const char* name) {
    std::wstring value = Utf8ToWide(name);
    return ExecuteOnInstance(handle, [value](LogViewInstance& inst) {
        inst.name = value;
        return 1;
    });
}

LOGVIEW_API LogViewHandle LOGVIEW_CALL LogView_FindByName(const char* name) {
    const std::wstring value = Utf8ToWide(name);
    if (value.empty()) {
        SetApiError(kErrorInvalidArgument, "Name is required.");
        return 0;
    }
    AcquireSRWLockExclusive(&g_instanceLock);
    LogViewHandle result = 0;
    uint64_t bestOrder = UINT64_MAX;
    for (const auto& pair : g_instances) {
        const auto& inst = pair.second;
        if (inst && !inst->destroyed && inst->name == value && inst->order < bestOrder) {
            result = pair.first;
            bestOrder = inst->order;
        }
    }
    ReleaseSRWLockExclusive(&g_instanceLock);
    if (result == 0) {
        SetApiError(kErrorInvalidHandle, "No LogView instance found by name.");
    } else {
        SetApiError(kErrorOk, "OK");
    }
    return result;
}

LOGVIEW_API int LOGVIEW_CALL LogView_GetCount(void) {
    AcquireSRWLockExclusive(&g_instanceLock);
    const int count = static_cast<int>(g_instances.size());
    ReleaseSRWLockExclusive(&g_instanceLock);
    return count;
}

LOGVIEW_API int LOGVIEW_CALL LogView_IsValid(LogViewHandle handle) {
    auto inst = GetInstance(handle);
    return (inst && !inst->destroyed) ? 1 : 0;
}

LOGVIEW_API int LOGVIEW_CALL LogView_GetLastErrorCode(void) {
    return t_lastErrorCode;
}

LOGVIEW_API const char* LOGVIEW_CALL LogView_GetLastErrorText(void) {
    return t_lastErrorText.c_str();
}

LOGVIEW_API const char* LOGVIEW_CALL LogView_GetVersion(void) {
    return kVersion;
}
