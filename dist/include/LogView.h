#pragma once

#include <stdint.h>

#ifdef _WIN32
#define LOGVIEW_CALL __stdcall
#else
#define LOGVIEW_CALL
#endif

#ifdef __cplusplus
#define LOGVIEW_EXTERN extern "C"
#else
#define LOGVIEW_EXTERN extern
#endif

#ifdef LOGVIEW_BUILD
#define LOGVIEW_API LOGVIEW_EXTERN
#else
#ifdef _WIN32
#define LOGVIEW_API LOGVIEW_EXTERN __declspec(dllimport)
#else
#define LOGVIEW_API LOGVIEW_EXTERN
#endif
#endif

typedef uintptr_t LogViewHandle;
typedef uintptr_t LogViewHwnd;
typedef int LogViewBool;
typedef uint32_t LogViewColor;

typedef void(LOGVIEW_CALL *LogViewCallback)(
    LogViewHandle handle,
    int eventType,
    uintptr_t wParam,
    uintptr_t lParam,
    uintptr_t userData);

enum LogViewEventType {
    LOGVIEW_EVENT_CLOSE_CLICKED = 1,
    LOGVIEW_EVENT_CLEAR_CLICKED = 2,
    LOGVIEW_EVENT_MOVED = 3,
    LOGVIEW_EVENT_RESIZED = 4,
    LOGVIEW_EVENT_DESTROYED = 5,
    LOGVIEW_EVENT_LOG_LINE_CLICKED = 6,
    LOGVIEW_EVENT_SCROLL_CHANGED = 7
};

enum LogViewCloseMode {
    LOGVIEW_CLOSE_HIDE = 0,
    LOGVIEW_CLOSE_DESTROY = 1,
    LOGVIEW_CLOSE_NOTIFY_ONLY = 2
};

enum LogViewLogLevel {
    LOGVIEW_LEVEL_INFO = 0,
    LOGVIEW_LEVEL_SUCCESS = 1,
    LOGVIEW_LEVEL_WARNING = 2,
    LOGVIEW_LEVEL_ERROR = 3,
    LOGVIEW_LEVEL_DEBUG = 4,
    LOGVIEW_LEVEL_CUSTOM = 5
};

LOGVIEW_API int LOGVIEW_CALL LogView_Init(void);
LOGVIEW_API int LOGVIEW_CALL LogView_Uninit(void);

LOGVIEW_API LogViewHandle LOGVIEW_CALL LogView_CreatePopup(int width, int height);
LOGVIEW_API LogViewHandle LOGVIEW_CALL LogView_CreatePopupEx(int width, int height, int alpha);
LOGVIEW_API LogViewHandle LOGVIEW_CALL LogView_CreateChild(LogViewHwnd parentHwnd, int x, int y, int width, int height);
LOGVIEW_API LogViewHandle LOGVIEW_CALL LogView_CreateChildEx(LogViewHwnd parentHwnd, int x, int y, int width, int height, int alpha);
LOGVIEW_API int LOGVIEW_CALL LogView_Destroy(LogViewHandle handle);
LOGVIEW_API int LOGVIEW_CALL LogView_DestroyAll(void);

LOGVIEW_API int LOGVIEW_CALL LogView_Show(LogViewHandle handle);
LOGVIEW_API int LOGVIEW_CALL LogView_Hide(LogViewHandle handle);
LOGVIEW_API int LOGVIEW_CALL LogView_Close(LogViewHandle handle);
LOGVIEW_API int LOGVIEW_CALL LogView_IsVisible(LogViewHandle handle);
LOGVIEW_API int LOGVIEW_CALL LogView_SetCloseMode(LogViewHandle handle, int mode);

LOGVIEW_API int LOGVIEW_CALL LogView_SetPosition(LogViewHandle handle, int x, int y);
LOGVIEW_API int LOGVIEW_CALL LogView_SetSize(LogViewHandle handle, int width, int height);
LOGVIEW_API int LOGVIEW_CALL LogView_SetRect(LogViewHandle handle, int x, int y, int width, int height);
LOGVIEW_API int LOGVIEW_CALL LogView_GetRect(LogViewHandle handle, int* outX, int* outY, int* outWidth, int* outHeight);
LOGVIEW_API int LOGVIEW_CALL LogView_CenterScreen(LogViewHandle handle);
LOGVIEW_API int LOGVIEW_CALL LogView_EnableDrag(LogViewHandle handle, int enable);

LOGVIEW_API int LOGVIEW_CALL LogView_AttachToParent(LogViewHandle handle, LogViewHwnd parentHwnd, int x, int y, int width, int height);
LOGVIEW_API int LOGVIEW_CALL LogView_DetachToDesktop(LogViewHandle handle);
LOGVIEW_API LogViewHwnd LOGVIEW_CALL LogView_GetWindowHandle(LogViewHandle handle);

LOGVIEW_API int LOGVIEW_CALL LogView_SetTopMost(LogViewHandle handle, int enable);
LOGVIEW_API int LOGVIEW_CALL LogView_SetAlpha(LogViewHandle handle, int alpha);
LOGVIEW_API int LOGVIEW_CALL LogView_SetBackgroundColor(LogViewHandle handle, LogViewColor color);
LOGVIEW_API int LOGVIEW_CALL LogView_SetBorderColor(LogViewHandle handle, LogViewColor color);
LOGVIEW_API int LOGVIEW_CALL LogView_SetRoundCorner(LogViewHandle handle, int radius);

LOGVIEW_API int LOGVIEW_CALL LogView_SetTitle(LogViewHandle handle, const char* text);
LOGVIEW_API int LOGVIEW_CALL LogView_ShowTitleBar(LogViewHandle handle, int enable);
LOGVIEW_API int LOGVIEW_CALL LogView_ShowCloseButton(LogViewHandle handle, int enable);
LOGVIEW_API int LOGVIEW_CALL LogView_ShowClearButton(LogViewHandle handle, int enable);
LOGVIEW_API int LOGVIEW_CALL LogView_SetClearButtonText(LogViewHandle handle, const char* text);
LOGVIEW_API int LOGVIEW_CALL LogView_SetCloseButtonText(LogViewHandle handle, const char* text);

LOGVIEW_API int LOGVIEW_CALL LogView_ShowScrollBar(LogViewHandle handle, int enable);
LOGVIEW_API int LOGVIEW_CALL LogView_SetAutoScroll(LogViewHandle handle, int enable);
LOGVIEW_API int LOGVIEW_CALL LogView_ScrollToTop(LogViewHandle handle);
LOGVIEW_API int LOGVIEW_CALL LogView_ScrollToBottom(LogViewHandle handle);
LOGVIEW_API int LOGVIEW_CALL LogView_SetScrollPosition(LogViewHandle handle, int position);
LOGVIEW_API int LOGVIEW_CALL LogView_GetScrollPosition(LogViewHandle handle);

LOGVIEW_API int LOGVIEW_CALL LogView_AddText(LogViewHandle handle, const char* text);
LOGVIEW_API int LOGVIEW_CALL LogView_AddLine(LogViewHandle handle, const char* text);
LOGVIEW_API int LOGVIEW_CALL LogView_AddLineEx(LogViewHandle handle, const char* text, LogViewColor color, int level);
LOGVIEW_API int LOGVIEW_CALL LogView_AddColoredLine(LogViewHandle handle, LogViewColor timeColor, LogViewColor prefixColor, LogViewColor textColor, const char* text);
LOGVIEW_API int LOGVIEW_CALL LogView_Clear(LogViewHandle handle);
LOGVIEW_API int LOGVIEW_CALL LogView_GetLineCount(LogViewHandle handle);
LOGVIEW_API int LOGVIEW_CALL LogView_GetText(LogViewHandle handle, char* buffer, int bufferSize);
LOGVIEW_API int LOGVIEW_CALL LogView_SaveToFile(LogViewHandle handle, const char* path);
LOGVIEW_API int LOGVIEW_CALL LogView_SetMaxLines(LogViewHandle handle, int maxLines);

LOGVIEW_API int LOGVIEW_CALL LogView_SetFont(LogViewHandle handle, const char* fontName, int fontSize);
LOGVIEW_API int LOGVIEW_CALL LogView_SetTextColor(LogViewHandle handle, LogViewColor color);
LOGVIEW_API int LOGVIEW_CALL LogView_SetTimeColor(LogViewHandle handle, LogViewColor color);
LOGVIEW_API int LOGVIEW_CALL LogView_SetInfoColor(LogViewHandle handle, LogViewColor color);
LOGVIEW_API int LOGVIEW_CALL LogView_SetSuccessColor(LogViewHandle handle, LogViewColor color);
LOGVIEW_API int LOGVIEW_CALL LogView_SetWarningColor(LogViewHandle handle, LogViewColor color);
LOGVIEW_API int LOGVIEW_CALL LogView_SetErrorColor(LogViewHandle handle, LogViewColor color);
LOGVIEW_API int LOGVIEW_CALL LogView_SetDebugColor(LogViewHandle handle, LogViewColor color);
LOGVIEW_API int LOGVIEW_CALL LogView_ShowTime(LogViewHandle handle, int enable);
LOGVIEW_API int LOGVIEW_CALL LogView_SetTimeFormat(LogViewHandle handle, const char* format);

LOGVIEW_API int LOGVIEW_CALL LogView_SetPadding(LogViewHandle handle, int left, int top, int right, int bottom);
LOGVIEW_API int LOGVIEW_CALL LogView_SetLineHeight(LogViewHandle handle, int height);
LOGVIEW_API int LOGVIEW_CALL LogView_SetHeaderHeight(LogViewHandle handle, int height);
LOGVIEW_API int LOGVIEW_CALL LogView_SetButtonAreaHeight(LogViewHandle handle, int height);
LOGVIEW_API int LOGVIEW_CALL LogView_SetScrollBarWidth(LogViewHandle handle, int width);

LOGVIEW_API int LOGVIEW_CALL LogView_SetCallback(LogViewHandle handle, LogViewCallback callback, uintptr_t userData);
LOGVIEW_API int LOGVIEW_CALL LogView_SetNotifyWindow(LogViewHandle handle, LogViewHwnd hwnd, int messageId);

LOGVIEW_API int LOGVIEW_CALL LogView_SetUserData(LogViewHandle handle, uintptr_t userData);
LOGVIEW_API uintptr_t LOGVIEW_CALL LogView_GetUserData(LogViewHandle handle);
LOGVIEW_API int LOGVIEW_CALL LogView_SetName(LogViewHandle handle, const char* name);
LOGVIEW_API LogViewHandle LOGVIEW_CALL LogView_FindByName(const char* name);
LOGVIEW_API int LOGVIEW_CALL LogView_GetCount(void);
LOGVIEW_API int LOGVIEW_CALL LogView_IsValid(LogViewHandle handle);

LOGVIEW_API int LOGVIEW_CALL LogView_GetLastErrorCode(void);
LOGVIEW_API const char* LOGVIEW_CALL LogView_GetLastErrorText(void);
LOGVIEW_API const char* LOGVIEW_CALL LogView_GetVersion(void);
