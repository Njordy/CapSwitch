#include <windows.h>
#include <shellapi.h>

#include "resource.h"

#include <atomic>
#include <cwchar>

namespace {

constexpr wchar_t kAppName[] = L"CapSwitch";
constexpr wchar_t kMutexName[] = L"Local\\CapSwitch.SingleInstance";
constexpr wchar_t kWindowClass[] = L"CapSwitch.MessageWindow";
constexpr UINT kTrayMessage = WM_APP + 1;
constexpr UINT kCmdSwitchLayout = WM_APP + 2;
constexpr UINT kCmdToggleEnabled = WM_APP + 3;
constexpr UINT kMenuToggleEnabled = 1001;
constexpr UINT kMenuReloadHook = 1002;
constexpr UINT kMenuToggleStartup = 1003;
constexpr UINT kMenuExit = 1004;
constexpr UINT kTimerWatchdog = 1;
constexpr UINT kTimerResetState = 2;
constexpr ULONG_PTR kInjectedMarker = 0x435357544348ULL;

HINSTANCE g_instance = nullptr;
HWND g_window = nullptr;
HHOOK g_hook = nullptr;
HANDLE g_mutex = nullptr;
NOTIFYICONDATAW g_tray = {};

std::atomic_bool g_enabled{ true };
std::atomic<DWORD> g_lastHookTick{ 0 };
std::atomic_bool g_capsDown{ false };
std::atomic_bool g_ctrlDown{ false };
std::atomic_bool g_shiftDown{ false };

bool g_debug = false;
bool g_noTray = false;

void DebugLog(const wchar_t* text)
{
    if (!g_debug) {
        return;
    }

    wchar_t path[MAX_PATH] = {};
    const DWORD len = GetEnvironmentVariableW(L"LOCALAPPDATA", path, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        return;
    }

    wcscat_s(path, L"\\CapSwitch");
    CreateDirectoryW(path, nullptr);
    wcscat_s(path, L"\\CapSwitch.log");

    HANDLE file = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
        OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    SYSTEMTIME st = {};
    GetLocalTime(&st);

    wchar_t line[512] = {};
    swprintf_s(line, L"%04u-%02u-%02u %02u:%02u:%02u.%03u %s\r\n",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
        st.wMilliseconds, text);

    DWORD bytes = 0;
    WriteFile(file, line, static_cast<DWORD>(wcslen(line) * sizeof(wchar_t)), &bytes, nullptr);
    CloseHandle(file);
}

bool IsCapsLockOn()
{
    return (GetKeyState(VK_CAPITAL) & 1) != 0;
}

void SendKey(WORD key, bool down)
{
    INPUT input = {};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = key;
    input.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
    input.ki.dwExtraInfo = kInjectedMarker;
    SendInput(1, &input, sizeof(input));
}

void ForceCapsLockOff()
{
    if (!IsCapsLockOn()) {
        return;
    }

    DebugLog(L"CapsLock was on; forcing it off");
    SendKey(VK_CAPITAL, true);
    SendKey(VK_CAPITAL, false);
}

void ResetInputState()
{
    g_capsDown.store(false);
    g_ctrlDown.store((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0);
    g_shiftDown.store((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0);
}

HWND GetLayoutTargetWindow()
{
    HWND foreground = GetForegroundWindow();
    if (!foreground) {
        return nullptr;
    }

    HWND focus = nullptr;
    DWORD targetThread = GetWindowThreadProcessId(foreground, nullptr);
    DWORD currentThread = GetCurrentThreadId();

    if (targetThread != 0 && AttachThreadInput(currentThread, targetThread, TRUE)) {
        focus = GetFocus();
        AttachThreadInput(currentThread, targetThread, FALSE);
    }

    return focus ? focus : foreground;
}

HKL GetNextKeyboardLayout(DWORD threadId)
{
    const int count = GetKeyboardLayoutList(0, nullptr);
    if (count <= 1) {
        return nullptr;
    }

    HKL layouts[32] = {};
    const int stored = GetKeyboardLayoutList(static_cast<int>(sizeof(layouts) / sizeof(layouts[0])), layouts);
    if (stored <= 1) {
        return nullptr;
    }

    HKL current = GetKeyboardLayout(threadId);
    for (int i = 0; i < stored; ++i) {
        if (layouts[i] == current) {
            return layouts[(i + 1) % stored];
        }
    }

    return layouts[0];
}

void SwitchLayout()
{
    if (!g_enabled.load()) {
        return;
    }

    ForceCapsLockOff();

    HWND foreground = GetForegroundWindow();
    if (!foreground) {
        DebugLog(L"No foreground window for layout switch");
        return;
    }

    DWORD threadId = GetWindowThreadProcessId(foreground, nullptr);
    HKL nextLayout = GetNextKeyboardLayout(threadId);
    if (!nextLayout) {
        DebugLog(L"No next keyboard layout found");
        return;
    }

    HWND target = GetLayoutTargetWindow();
    if (target && target != foreground) {
        PostMessageW(target, WM_INPUTLANGCHANGEREQUEST, 0, reinterpret_cast<LPARAM>(nextLayout));
    }
    PostMessageW(foreground, WM_INPUTLANGCHANGEREQUEST, 0, reinterpret_cast<LPARAM>(nextLayout));
    DebugLog(L"Switch layout requested");
}

void ToggleEnabled()
{
    const bool enabled = !g_enabled.load();
    g_enabled.store(enabled);
    ForceCapsLockOff();
    DebugLog(enabled ? L"Enabled" : L"Disabled");
}

bool GetExecutablePath(wchar_t* path, DWORD pathCount)
{
    const DWORD len = GetModuleFileNameW(nullptr, path, pathCount);
    return len > 0 && len < pathCount;
}

bool IsStartupEnabled()
{
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &key) != ERROR_SUCCESS) {
        return false;
    }

    wchar_t value[MAX_PATH + 8] = {};
    DWORD valueSize = sizeof(value);
    const LSTATUS status = RegQueryValueExW(key, kAppName, nullptr, nullptr,
        reinterpret_cast<LPBYTE>(value), &valueSize);
    RegCloseKey(key);

    if (status != ERROR_SUCCESS) {
        return false;
    }

    wchar_t exePath[MAX_PATH] = {};
    if (!GetExecutablePath(exePath, static_cast<DWORD>(sizeof(exePath) / sizeof(exePath[0])))) {
        return false;
    }

    wchar_t expected[MAX_PATH + 8] = {};
    swprintf_s(expected, L"\"%s\"", exePath);
    return _wcsicmp(value, expected) == 0 || _wcsicmp(value, exePath) == 0;
}

bool SetStartupEnabled(bool enabled)
{
    HKEY key = nullptr;
    const LSTATUS openStatus = RegCreateKeyExW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, nullptr, 0,
        KEY_SET_VALUE, nullptr, &key, nullptr);
    if (openStatus != ERROR_SUCCESS) {
        DebugLog(L"Failed to open Run key");
        return false;
    }

    LSTATUS status = ERROR_SUCCESS;
    if (enabled) {
        wchar_t exePath[MAX_PATH] = {};
        if (!GetExecutablePath(exePath, static_cast<DWORD>(sizeof(exePath) / sizeof(exePath[0])))) {
            RegCloseKey(key);
            return false;
        }

        wchar_t value[MAX_PATH + 8] = {};
        swprintf_s(value, L"\"%s\"", exePath);
        status = RegSetValueExW(key, kAppName, 0, REG_SZ,
            reinterpret_cast<const BYTE*>(value),
            static_cast<DWORD>((wcslen(value) + 1) * sizeof(wchar_t)));
    } else {
        status = RegDeleteValueW(key, kAppName);
        if (status == ERROR_FILE_NOT_FOUND) {
            status = ERROR_SUCCESS;
        }
    }

    RegCloseKey(key);
    DebugLog(enabled ? L"Startup enabled" : L"Startup disabled");
    return status == ERROR_SUCCESS;
}

bool IsCtrlKey(WPARAM vk)
{
    return vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL;
}

bool IsShiftKey(WPARAM vk)
{
    return vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT;
}

bool IsKeyDownMessage(WPARAM message)
{
    return message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
}

bool IsKeyUpMessage(WPARAM message)
{
    return message == WM_KEYUP || message == WM_SYSKEYUP;
}

LRESULT CALLBACK LowLevelKeyboardProc(int code, WPARAM wParam, LPARAM lParam)
{
    if (code != HC_ACTION) {
        return CallNextHookEx(g_hook, code, wParam, lParam);
    }

    auto* key = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
    if ((key->flags & LLKHF_INJECTED) && key->dwExtraInfo == kInjectedMarker) {
        return CallNextHookEx(g_hook, code, wParam, lParam);
    }

    g_lastHookTick.store(GetTickCount());

    if (IsCtrlKey(key->vkCode)) {
        g_ctrlDown.store(IsKeyDownMessage(wParam));
        return CallNextHookEx(g_hook, code, wParam, lParam);
    }

    if (IsShiftKey(key->vkCode)) {
        g_shiftDown.store(IsKeyDownMessage(wParam));
        return CallNextHookEx(g_hook, code, wParam, lParam);
    }

    if (key->vkCode != VK_CAPITAL) {
        return CallNextHookEx(g_hook, code, wParam, lParam);
    }

    ForceCapsLockOff();

    if (IsKeyUpMessage(wParam)) {
        g_capsDown.store(false);
        return 1;
    }

    if (!IsKeyDownMessage(wParam)) {
        return 1;
    }

    if (g_capsDown.exchange(true)) {
        return 1;
    }

    const bool ctrlDown =
        g_ctrlDown.load() || (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;

    if (ctrlDown) {
        PostMessageW(g_window, kCmdToggleEnabled, 0, 0);
    } else if (g_enabled.load()) {
        PostMessageW(g_window, kCmdSwitchLayout, 0, 0);
    }

    return 1;
}

bool InstallHook()
{
    if (g_hook) {
        UnhookWindowsHookEx(g_hook);
        g_hook = nullptr;
    }

    g_hook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, g_instance, 0);
    g_lastHookTick.store(GetTickCount());
    DebugLog(g_hook ? L"Hook installed" : L"Hook install failed");
    return g_hook != nullptr;
}

void RemoveHook()
{
    if (g_hook) {
        UnhookWindowsHookEx(g_hook);
        g_hook = nullptr;
    }
}

void UpdateTrayIcon()
{
    if (g_noTray || g_tray.cbSize == 0) {
        return;
    }

    g_tray.uFlags = NIF_TIP;
    swprintf_s(g_tray.szTip, L"CapSwitch: %s", g_enabled.load() ? L"enabled" : L"disabled");
    Shell_NotifyIconW(NIM_MODIFY, &g_tray);
}

void AddTrayIcon()
{
    if (g_noTray) {
        return;
    }

    g_tray = {};
    g_tray.cbSize = sizeof(g_tray);
    g_tray.hWnd = g_window;
    g_tray.uID = 1;
    g_tray.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_tray.uCallbackMessage = kTrayMessage;
    g_tray.hIcon = LoadIconW(g_instance, MAKEINTRESOURCEW(IDI_CAPSWITCH));
    if (!g_tray.hIcon) {
        g_tray.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    }
    swprintf_s(g_tray.szTip, L"CapSwitch: enabled");
    Shell_NotifyIconW(NIM_ADD, &g_tray);
    g_tray.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &g_tray);
}

void RemoveTrayIcon()
{
    if (!g_noTray && g_tray.cbSize != 0) {
        Shell_NotifyIconW(NIM_DELETE, &g_tray);
    }
}

void ShowTrayMenu()
{
    HMENU menu = CreatePopupMenu();
    if (!menu) {
        return;
    }

    AppendMenuW(menu, MF_STRING, kMenuToggleEnabled,
        g_enabled.load() ? L"Disable CapSwitch" : L"Enable CapSwitch");
    AppendMenuW(menu, MF_STRING | (IsStartupEnabled() ? MF_CHECKED : MF_UNCHECKED),
        kMenuToggleStartup, L"Start with Windows");
    AppendMenuW(menu, MF_STRING, kMenuReloadHook, L"Reload keyboard hook");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuExit, L"Exit");

    POINT pt = {};
    GetCursorPos(&pt);
    SetForegroundWindow(g_window);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, g_window, nullptr);
    DestroyMenu(menu);
}

void WatchdogTick()
{
    static DWORD lastReinstall = 0;
    const DWORD now = GetTickCount();

    ForceCapsLockOff();

    if (!g_hook || now - lastReinstall > 30000) {
        InstallHook();
        lastReinstall = now;
    }

    if (g_capsDown.load() && (GetAsyncKeyState(VK_CAPITAL) & 0x8000) == 0) {
        g_capsDown.store(false);
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case kCmdSwitchLayout:
        SwitchLayout();
        return 0;
    case kCmdToggleEnabled:
        ToggleEnabled();
        UpdateTrayIcon();
        return 0;
    case kTrayMessage:
        if (LOWORD(lParam) == WM_CONTEXTMENU || LOWORD(lParam) == WM_RBUTTONUP) {
            ShowTrayMenu();
        } else if (LOWORD(lParam) == WM_LBUTTONDBLCLK) {
            PostMessageW(hwnd, kCmdToggleEnabled, 0, 0);
        }
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case kMenuToggleEnabled:
            PostMessageW(hwnd, kCmdToggleEnabled, 0, 0);
            return 0;
        case kMenuReloadHook:
            InstallHook();
            ResetInputState();
            ForceCapsLockOff();
            return 0;
        case kMenuToggleStartup:
            SetStartupEnabled(!IsStartupEnabled());
            return 0;
        case kMenuExit:
            DestroyWindow(hwnd);
            return 0;
        default:
            break;
        }
        break;
    case WM_TIMER:
        if (wParam == kTimerWatchdog || wParam == kTimerResetState) {
            WatchdogTick();
            return 0;
        }
        break;
    case WM_DESTROY:
        KillTimer(hwnd, kTimerWatchdog);
        KillTimer(hwnd, kTimerResetState);
        RemoveTrayIcon();
        RemoveHook();
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

void ParseCommandLine()
{
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) {
        return;
    }

    for (int i = 1; i < argc; ++i) {
        if (wcscmp(argv[i], L"--debug") == 0) {
            g_debug = true;
        } else if (wcscmp(argv[i], L"--no-tray") == 0) {
            g_noTray = true;
        }
    }

    LocalFree(argv);
}

bool CreateMessageWindow()
{
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = g_instance;
    wc.lpszClassName = kWindowClass;
    wc.hIcon = LoadIconW(g_instance, MAKEINTRESOURCEW(IDI_CAPSWITCH));

    if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    g_window = CreateWindowExW(0, kWindowClass, kAppName, 0, 0, 0, 0, 0,
        HWND_MESSAGE, nullptr, g_instance, nullptr);
    return g_window != nullptr;
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    g_instance = instance;
    ParseCommandLine();

    g_mutex = CreateMutexW(nullptr, FALSE, kMutexName);
    if (!g_mutex || GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(nullptr, L"CapSwitch is already running.", kAppName, MB_OK | MB_ICONINFORMATION);
        return 1;
    }

    if (!CreateMessageWindow()) {
        MessageBoxW(nullptr, L"Failed to create CapSwitch message window.", kAppName, MB_OK | MB_ICONERROR);
        return 1;
    }

    ResetInputState();
    ForceCapsLockOff();

    if (!InstallHook()) {
        MessageBoxW(nullptr, L"Failed to install the keyboard hook.", kAppName, MB_OK | MB_ICONERROR);
        return 1;
    }

    AddTrayIcon();
    SetTimer(g_window, kTimerWatchdog, 500, nullptr);
    SetTimer(g_window, kTimerResetState, 3000, nullptr);

    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (g_mutex) {
        CloseHandle(g_mutex);
        g_mutex = nullptr;
    }

    return 0;
}
