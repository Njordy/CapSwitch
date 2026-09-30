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
constexpr UINT kCmdForceCapsOff = WM_APP + 4;
constexpr UINT kMenuToggleEnabled = 1001;
constexpr UINT kMenuReloadHook = 1002;
constexpr UINT kMenuToggleStartup = 1003;
constexpr UINT kMenuExit = 1004;
constexpr UINT kMenuMethodMsg = 1010;
constexpr UINT kMenuMethodAltShift = 1011;
constexpr UINT kMenuMethodCtrlShift = 1012;
constexpr UINT kMenuMethodWinSpace = 1013;
constexpr UINT kMenuToggleDebug = 1014;
constexpr UINT kMenuOpenLog = 1015;
constexpr UINT kTimerWatchdog = 1;
constexpr ULONG_PTR kInjectedMarker = 0x435357544348ULL;

enum SwitchMethod : DWORD {
    SwitchMethodMessage = 0,
    SwitchMethodAltShift = 1,
    SwitchMethodCtrlShift = 2,
    SwitchMethodWinSpace = 3,
};

HINSTANCE g_instance = nullptr;
HWND g_window = nullptr;
HHOOK g_hook = nullptr;
HANDLE g_mutex = nullptr;
NOTIFYICONDATAW g_tray = {};
UINT g_taskbarCreatedMessage = 0;

std::atomic_bool g_enabled{ true };
std::atomic_bool g_capsDown{ false };
std::atomic<DWORD> g_capsDownTick{ 0 };
std::atomic<DWORD> g_switchMethod{ SwitchMethodMessage };

bool g_debug = false;
bool g_noTray = false;

bool InstallHook();

bool GetLogFilePath(wchar_t* path, DWORD pathCount)
{
    const DWORD len = GetEnvironmentVariableW(L"LOCALAPPDATA", path, pathCount);
    if (len == 0 || len >= pathCount - 32) {
        return false;
    }

    wcscat_s(path, pathCount, L"\\CapSwitch");
    CreateDirectoryW(path, nullptr);
    wcscat_s(path, pathCount, L"\\CapSwitch.log");
    return true;
}

void DebugLog(const wchar_t* text)
{
    if (!g_debug) {
        return;
    }

    wchar_t path[1024] = {};
    if (!GetLogFilePath(path, static_cast<DWORD>(sizeof(path) / sizeof(path[0])))) {
        return;
    }

    HANDLE file = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
        OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    SYSTEMTIME st = {};
    GetLocalTime(&st);

    wchar_t line[512] = {};
    _snwprintf_s(line, sizeof(line) / sizeof(line[0]), _TRUNCATE, L"%04u-%02u-%02u %02u:%02u:%02u.%03u %s\r\n",
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
    g_capsDownTick.store(0);
}

HWND GetLayoutTargetWindow()
{
    HWND foreground = GetForegroundWindow();
    if (!foreground) {
        return nullptr;
    }

    HWND focus = nullptr;
    DWORD targetThread = GetWindowThreadProcessId(foreground, nullptr);
    if (targetThread != 0) {
        GUITHREADINFO gti = {};
        gti.cbSize = sizeof(GUITHREADINFO);
        if (GetGUIThreadInfo(targetThread, &gti) && gti.hwndFocus) {
            focus = gti.hwndFocus;
        }
    }

    return focus ? focus : foreground;
}

void RequestLayoutSwitch(HWND hwnd)
{
    if (!hwnd) {
        return;
    }

    PostMessageW(hwnd, WM_INPUTLANGCHANGEREQUEST, 0, static_cast<LPARAM>(static_cast<LONG_PTR>(HKL_NEXT)));
}

const wchar_t* GetSwitchMethodName(DWORD method)
{
    switch (method) {
    case SwitchMethodMessage:
        return L"Window Message";
    case SwitchMethodAltShift:
        return L"Alt+Shift";
    case SwitchMethodCtrlShift:
        return L"Ctrl+Shift";
    case SwitchMethodWinSpace:
        return L"Win+Space";
    default:
        return L"Unknown";
    }
}

void SimulateKeyCombo(const WORD* keys, int count)
{
    if (!keys || count <= 0 || count > 8) {
        return;
    }

    INPUT inputs[16] = {};
    int inputCount = 0;
    for (int i = 0; i < count; ++i) {
        inputs[inputCount].type = INPUT_KEYBOARD;
        inputs[inputCount].ki.wVk = keys[i];
        inputs[inputCount].ki.dwExtraInfo = kInjectedMarker;
        ++inputCount;
    }

    for (int i = count - 1; i >= 0; --i) {
        inputs[inputCount].type = INPUT_KEYBOARD;
        inputs[inputCount].ki.wVk = keys[i];
        inputs[inputCount].ki.dwFlags = KEYEVENTF_KEYUP;
        inputs[inputCount].ki.dwExtraInfo = kInjectedMarker;
        ++inputCount;
    }

    SendInput(static_cast<UINT>(inputCount), inputs, sizeof(INPUT));
}

void LogSwitchDiagnostics(HWND foreground, HWND target, DWORD threadId, DWORD method)
{
    if (!g_debug) {
        return;
    }

    wchar_t title[256] = {};
    wchar_t className[128] = {};
    GetWindowTextW(foreground, title, static_cast<int>(sizeof(title) / sizeof(title[0])));
    GetClassNameW(foreground, className, static_cast<int>(sizeof(className) / sizeof(className[0])));

    DWORD processId = 0;
    GetWindowThreadProcessId(foreground, &processId);
    HKL currentLayout = GetKeyboardLayout(threadId);

    wchar_t line[768] = {};
    _snwprintf_s(line, sizeof(line) / sizeof(line[0]), _TRUNCATE,
        L"SwitchLayout method=%s hwnd=0x%p target=0x%p pid=%lu tid=%lu hkl=0x%p class=\"%s\" title=\"%s\"",
        GetSwitchMethodName(method), foreground, target, processId, threadId,
        currentLayout, className, title);
    DebugLog(line);
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
    if (threadId == 0) {
        DebugLog(L"Failed to get thread ID for foreground window");
        return;
    }

    HWND target = GetLayoutTargetWindow();
    if (!target) {
        target = foreground;
    }

    const DWORD method = g_switchMethod.load();
    LogSwitchDiagnostics(foreground, target, threadId, method);

    switch (method) {
    case SwitchMethodAltShift: {
        const WORD keys[] = { VK_LMENU, VK_LSHIFT };
        SimulateKeyCombo(keys, 2);
        break;
    }
    case SwitchMethodCtrlShift: {
        const WORD keys[] = { VK_LCONTROL, VK_LSHIFT };
        SimulateKeyCombo(keys, 2);
        break;
    }
    case SwitchMethodWinSpace: {
        const WORD keys[] = { VK_LWIN, VK_SPACE };
        SimulateKeyCombo(keys, 2);
        break;
    }
    case SwitchMethodMessage:
    default:
        RequestLayoutSwitch(target);
        break;
    }

    DebugLog(L"Switch layout requested");
}

void ToggleEnabled()
{
    const bool enabled = !g_enabled.load();
    g_enabled.store(enabled);
    InstallHook();
    ResetInputState();
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

    // Ensure safe null-termination
    const DWORD maxChars = sizeof(value) / sizeof(value[0]);
    const DWORD charsWritten = valueSize / sizeof(wchar_t);
    if (charsWritten < maxChars) {
        value[charsWritten] = L'\0';
    } else {
        value[maxChars - 1] = L'\0';
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

void LoadSettings()
{
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\CapSwitch", 0, KEY_READ, &key) != ERROR_SUCCESS) {
        return;
    }

    DWORD value = 0;
    DWORD valueSize = sizeof(value);
    if (RegQueryValueExW(key, L"SwitchMethod", nullptr, nullptr,
        reinterpret_cast<LPBYTE>(&value), &valueSize) == ERROR_SUCCESS &&
        value <= SwitchMethodWinSpace) {
        g_switchMethod.store(value);
    }

    value = 0;
    valueSize = sizeof(value);
    if (RegQueryValueExW(key, L"DebugLogging", nullptr, nullptr,
        reinterpret_cast<LPBYTE>(&value), &valueSize) == ERROR_SUCCESS) {
        g_debug = value != 0;
    }

    RegCloseKey(key);
}

void SaveSettings()
{
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\CapSwitch", 0, nullptr, 0,
        KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) {
        return;
    }

    DWORD value = g_switchMethod.load();
    RegSetValueExW(key, L"SwitchMethod", 0, REG_DWORD,
        reinterpret_cast<const BYTE*>(&value), sizeof(value));

    value = g_debug ? 1u : 0u;
    RegSetValueExW(key, L"DebugLogging", 0, REG_DWORD,
        reinterpret_cast<const BYTE*>(&value), sizeof(value));

    RegCloseKey(key);
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
    if (!key || ((key->flags & LLKHF_INJECTED) && key->dwExtraInfo == kInjectedMarker)) {
        return CallNextHookEx(g_hook, code, wParam, lParam);
    }

    if (key->vkCode != VK_CAPITAL) {
        return CallNextHookEx(g_hook, code, wParam, lParam);
    }

    PostMessageW(g_window, kCmdForceCapsOff, 0, 0);

    if (IsKeyUpMessage(wParam)) {
        g_capsDown.store(false);
        g_capsDownTick.store(0);
        return 1;
    }

    if (!IsKeyDownMessage(wParam)) {
        return 1;
    }

    if (g_capsDown.exchange(true)) {
        return 1;
    }
    g_capsDownTick.store(GetTickCount());

    const bool ctrlDown = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;

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

void OpenLogFile()
{
    wchar_t path[1024] = {};
    if (!GetLogFilePath(path, static_cast<DWORD>(sizeof(path) / sizeof(path[0])))) {
        return;
    }

    HANDLE file = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
        OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        CloseHandle(file);
    }

    wchar_t parameters[1200] = {};
    _snwprintf_s(parameters, sizeof(parameters) / sizeof(parameters[0]), _TRUNCATE, L"\"%s\"", path);
    ShellExecuteW(nullptr, L"open", L"notepad.exe", parameters, nullptr, SW_SHOWNORMAL);
}

void AppendSwitchMethodMenu(HMENU menu)
{
    HMENU submenu = CreatePopupMenu();
    if (!submenu) {
        return;
    }

    const DWORD method = g_switchMethod.load();
    AppendMenuW(submenu, MF_STRING | (method == SwitchMethodMessage ? MF_CHECKED : MF_UNCHECKED),
        kMenuMethodMsg, L"Window Message");
    AppendMenuW(submenu, MF_STRING | (method == SwitchMethodAltShift ? MF_CHECKED : MF_UNCHECKED),
        kMenuMethodAltShift, L"Alt+Shift");
    AppendMenuW(submenu, MF_STRING | (method == SwitchMethodCtrlShift ? MF_CHECKED : MF_UNCHECKED),
        kMenuMethodCtrlShift, L"Ctrl+Shift");
    AppendMenuW(submenu, MF_STRING | (method == SwitchMethodWinSpace ? MF_CHECKED : MF_UNCHECKED),
        kMenuMethodWinSpace, L"Win+Space");

    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(submenu), L"Switching Method");
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
    AppendSwitchMethodMenu(menu);
    AppendMenuW(menu, MF_STRING | (g_debug ? MF_CHECKED : MF_UNCHECKED),
        kMenuToggleDebug, L"Enable Debug Logging");
    AppendMenuW(menu, MF_STRING, kMenuOpenLog, L"Open Log File");
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
    static DWORD lastReinstall = GetTickCount();
    const DWORD now = GetTickCount();

    ForceCapsLockOff();

    if (!g_hook || now - lastReinstall > 5000) {
        InstallHook();
        lastReinstall = now;
    }

    const DWORD capsTick = g_capsDownTick.load();
    if (g_capsDown.load() &&
        ((GetAsyncKeyState(VK_CAPITAL) & 0x8000) == 0 || (capsTick != 0 && now - capsTick > 1500))) {
        g_capsDown.store(false);
        g_capsDownTick.store(0);
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == g_taskbarCreatedMessage) {
        AddTrayIcon();
        UpdateTrayIcon();
        return 0;
    }

    switch (message) {
    case kCmdSwitchLayout:
        SwitchLayout();
        return 0;
    case kCmdToggleEnabled:
        ToggleEnabled();
        UpdateTrayIcon();
        return 0;
    case kCmdForceCapsOff:
        ForceCapsLockOff();
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
        case kMenuMethodMsg:
            g_switchMethod.store(SwitchMethodMessage);
            SaveSettings();
            return 0;
        case kMenuMethodAltShift:
            g_switchMethod.store(SwitchMethodAltShift);
            SaveSettings();
            return 0;
        case kMenuMethodCtrlShift:
            g_switchMethod.store(SwitchMethodCtrlShift);
            SaveSettings();
            return 0;
        case kMenuMethodWinSpace:
            g_switchMethod.store(SwitchMethodWinSpace);
            SaveSettings();
            return 0;
        case kMenuToggleDebug:
            g_debug = !g_debug;
            SaveSettings();
            DebugLog(g_debug ? L"Debug logging enabled" : L"Debug logging disabled");
            return 0;
        case kMenuOpenLog:
            OpenLogFile();
            return 0;
        case kMenuExit:
            DestroyWindow(hwnd);
            return 0;
        default:
            break;
        }
        break;
    case WM_TIMER:
        if (wParam == kTimerWatchdog) {
            WatchdogTick();
            return 0;
        }
        break;
    case WM_DESTROY:
        KillTimer(hwnd, kTimerWatchdog);
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

    g_window = CreateWindowExW(0, kWindowClass, kAppName, WS_OVERLAPPED,
        0, 0, 0, 0, nullptr, nullptr, g_instance, nullptr);
    return g_window != nullptr;
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    // Enable Per-Monitor DPI Awareness V2 for crisp tray menus and dialogs
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    g_instance = instance;
    g_taskbarCreatedMessage = RegisterWindowMessageW(L"TaskbarCreated");
    LoadSettings();
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
