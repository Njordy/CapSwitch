// Exercise the actual production functions without installing a global hook,
// injecting input, changing settings, or touching the running desktop instance.
#include <windows.h>
#include <shellapi.h>
#include <wtsapi32.h>
#include <objbase.h>
#include <cstdio>
#include <vector>
#include <algorithm>
#include <string>

namespace fake {
std::vector<UINT> messages;
std::vector<INPUT> inputs;
std::vector<int> hookOperations;
std::vector<HHOOK> removedHooks;
SHORT keys[256] = {};
DWORD tick = 0;
DWORD removeError = ERROR_SUCCESS;
bool failInstall = false;
bool failAppendPopup = false;
int nextHook = 100;
bool capsOn = false;
int failures = 0;
int checks = 0;
UINT sendLimit = UINT_MAX;
std::wstring logRoot;

BOOL WINAPI Post(HWND, UINT message, WPARAM, LPARAM) { messages.push_back(message); return TRUE; }
SHORT WINAPI Async(int key) { return keys[key]; }
SHORT WINAPI State(int key) { return key == VK_CAPITAL && capsOn ? 1 : 0; }
UINT WINAPI Send(UINT count, LPINPUT data, int) {
    const UINT sent = (std::min)(count, sendLimit);
    sendLimit = UINT_MAX;
    inputs.insert(inputs.end(), data, data + sent);
    return sent;
}
DWORD WINAPI Environment(LPCWSTR name, LPWSTR buffer, DWORD size) {
    if (wcscmp(name, L"LOCALAPPDATA") == 0 && !logRoot.empty()) {
        if (size <= logRoot.size()) { return static_cast<DWORD>(logRoot.size() + 1); }
        wcscpy_s(buffer, size, logRoot.c_str());
        return static_cast<DWORD>(logRoot.size());
    }
    return ::GetEnvironmentVariableW(name, buffer, size);
}
DWORD WINAPI Tick() { return tick; }
HHOOK WINAPI Install(int, HOOKPROC, HINSTANCE, DWORD) {
    hookOperations.push_back(1);
    return failInstall ? nullptr : reinterpret_cast<HHOOK>(static_cast<INT_PTR>(++nextHook));
}
BOOL WINAPI Remove(HHOOK hook) {
    hookOperations.push_back(2);
    if (removeError != ERROR_SUCCESS) { SetLastError(removeError); return FALSE; }
    removedHooks.push_back(hook);
    return TRUE;
}
BOOL WINAPI RegisterSession(HWND, DWORD) { return TRUE; }
LRESULT WINAPI Next(HHOOK, int, WPARAM, LPARAM) { return 77; }
BOOL WINAPI Append(HMENU menu, UINT flags, UINT_PTR item, LPCWSTR text) {
    if (failAppendPopup && (flags & MF_POPUP)) { SetLastError(ERROR_NOT_ENOUGH_MEMORY); return FALSE; }
    return ::AppendMenuW(menu, flags, item, text);
}
void Check(bool condition, const char* name) {
    ++checks;
    std::printf("%s: %s\n", condition ? "PASS" : "FAIL", name);
    if (!condition) { ++failures; }
}
}

#define PostMessageW fake::Post
#define GetAsyncKeyState fake::Async
#define GetKeyState fake::State
#define SendInput fake::Send
#define GetTickCount fake::Tick
#define SetWindowsHookExW fake::Install
#define UnhookWindowsHookEx fake::Remove
#define WTSRegisterSessionNotification fake::RegisterSession
#define CallNextHookEx fake::Next
#define AppendMenuW fake::Append
#define GetEnvironmentVariableW fake::Environment
#ifdef CAPSWITCH_MUTANT
#include "../artifacts/mutant.cpp"
#else
#include "../CapSwitch/main.cpp"
#endif
#undef AppendMenuW

void ResetFixture()
{
    fake::messages.clear(); fake::inputs.clear(); fake::hookOperations.clear();
    fake::removedHooks.clear();
    std::fill(std::begin(fake::keys), std::end(fake::keys), SHORT{0});
    fake::tick = 0; fake::removeError = ERROR_SUCCESS; fake::failInstall = false;
    fake::sendLimit = UINT_MAX;
    fake::capsOn = false; fake::failAppendPopup = false;
    g_enabled = true; g_capsDown = false; g_debug = false; g_noTray = true;
    g_hook = reinterpret_cast<HHOOK>(INT_PTR{1}); g_retiredHook = nullptr;
    g_sessionNotifications = true; g_window = nullptr; g_tray = {};
}

LRESULT Caps(WPARAM message, DWORD flags = 0, ULONG_PTR marker = 0)
{
    KBDLLHOOKSTRUCT key = {};
    key.vkCode = VK_CAPITAL; key.flags = flags; key.dwExtraInfo = marker;
    return LowLevelKeyboardProc(HC_ACTION, message, reinterpret_cast<LPARAM>(&key));
}

size_t SwitchCount()
{
    return std::count(fake::messages.begin(), fake::messages.end(), kCmdSwitchLayout);
}

int main()
{
    ResetFixture();
    fake::Check(Caps(WM_KEYDOWN) == 1, "physical CapsLock is suppressed");
    // Async CapsLock is always up, just as it is for suppressed input.
    for (DWORD time = 500; time <= 30000; time += 500) {
        fake::tick = time; WatchdogTick(); Caps(WM_KEYDOWN);
    }
    fake::Check(SwitchCount() == 1, "30-second hold across watchdog and hook replacement switches once");
    fake::tick += 60000; WatchdogTick(); Caps(WM_KEYDOWN);
    fake::Check(SwitchCount() == 1, "hold without typematic repeats never expires");
    Caps(WM_KEYUP); Caps(WM_KEYDOWN);
    fake::Check(SwitchCount() == 2, "release then press switches again");

    ResetFixture(); fake::keys[VK_CONTROL] = SHORT{-32768};
    for (int repeat = 0; repeat < 100; ++repeat) { Caps(WM_KEYDOWN); }
    fake::Check(g_enabled && SwitchCount() == 0, "Ctrl+CapsLock cannot disable or switch");
    fake::keys[VK_CONTROL] = 0; Caps(WM_KEYDOWN);
    fake::Check(SwitchCount() == 0, "releasing Ctrl while Caps remains down does not trigger a switch");
    Caps(WM_KEYUP); Caps(WM_KEYDOWN);
    fake::Check(SwitchCount() == 1, "normal press after modified chord works");
    WindowProc(nullptr, kTrayMessage, 0, WM_LBUTTONDBLCLK);
    fake::Check(g_enabled, "tray double-click cannot disable");
    WindowProc(nullptr, WM_COMMAND, kMenuToggleEnabled, 0);
    fake::Check(!g_enabled && g_capsDown, "explicit menu disables without releasing physical latch");
    WindowProc(nullptr, WM_COMMAND, kMenuToggleEnabled, 0);
    Caps(WM_KEYDOWN);
    fake::Check(g_enabled && SwitchCount() == 1, "re-enabling during a hold cannot retrigger");

    ResetFixture(); Caps(WM_KEYDOWN);
    Caps(WM_KEYUP, LLKHF_INJECTED); Caps(WM_KEYDOWN);
    fake::Check(SwitchCount() == 1, "foreign injected key-up cannot release physical latch");
    fake::Check(Caps(WM_KEYUP, LLKHF_INJECTED, kInjectedMarker) == 77,
        "own CapsLock correction passes to Windows");
    fake::Check(g_capsDown, "own correction does not alter physical latch");

    ResetFixture();
    const WORD combo[] = { VK_LMENU, VK_LSHIFT };
    fake::keys[VK_SHIFT] = SHORT{-32768}; SimulateKeyCombo(combo, 2);
    fake::Check(fake::inputs.empty(), "held modifier prevents synthetic releases");
    fake::capsOn = true; ForceCapsLockOff();
    fake::Check(fake::inputs.empty(), "Caps correction waits for modifier release");
    fake::keys[VK_SHIFT] = 0; ForceCapsLockOff();
    fake::Check(fake::inputs.size() == 2 && fake::inputs[0].ki.dwFlags == 0 &&
        fake::inputs[1].ki.dwFlags == KEYEVENTF_KEYUP, "Caps correction uses one ordered down/up batch");
    fake::inputs.clear(); SimulateKeyCombo(combo, 2);
    fake::Check(fake::inputs.size() == 4 && fake::inputs[2].ki.wVk == VK_LSHIFT &&
        fake::inputs[3].ki.wVk == VK_LMENU, "shortcut releases keys in reverse order");
    fake::inputs.clear(); fake::keys[VK_SPACE] = SHORT{-32768};
    const WORD winSpace[] = { VK_LWIN, VK_SPACE }; SimulateKeyCombo(winSpace, 2);
    fake::Check(fake::inputs.empty(), "held Space cannot be synthetically released");
    fake::keys[VK_SPACE] = 0; fake::sendLimit = 1; SimulateKeyCombo(combo, 2);
    fake::Check(fake::inputs.size() == 2 && fake::inputs[1].ki.wVk == VK_LMENU &&
        fake::inputs[1].ki.dwFlags == KEYEVENTF_KEYUP, "partial injection releases unmatched synthetic key-down");
    fake::inputs.clear(); fake::sendLimit = 0; SimulateKeyCombo(combo, 2);
    fake::Check(fake::inputs.empty(), "blocked injection does not send unrelated releases");

    ResetFixture(); HHOOK original = g_hook; fake::failInstall = true;
    fake::Check(!InstallHook() && g_hook == original && fake::removedHooks.empty(),
        "failed replacement preserves previous hook");
    fake::failInstall = false; fake::hookOperations.clear();
    fake::Check(InstallHook() && fake::hookOperations == std::vector<int>({1, 2}),
        "new hook is installed before old hook removal");
    fake::removeError = ERROR_ACCESS_DENIED; InstallHook();
    HHOOK retired = g_retiredHook; HHOOK active = g_hook;
    fake::Check(retired != nullptr && !InstallHook() && g_retiredHook == retired && g_hook == active,
        "failed unhook retains bounded ownership instead of leaking handles");
    fake::removeError = ERROR_INVALID_HOOK_HANDLE;
    fake::Check(InstallHook() && g_retiredHook == nullptr, "silently removed hook does not block recovery");

    ResetFixture(); fake::failAppendPopup = true;
    // Warm up Win32 menu allocation before measuring repeated failure cleanup.
    HMENU warmup = CreatePopupMenu(); AppendSwitchMethodMenu(warmup); DestroyMenu(warmup);
    DWORD before = GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS);
    for (int iteration = 0; iteration < 1000; ++iteration) {
        HMENU menu = CreatePopupMenu(); AppendSwitchMethodMenu(menu); DestroyMenu(menu);
    }
    fake::Check(GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS) == before,
        "1000 failed submenu attachments do not leak USER objects");

    ResetFixture(); Caps(WM_KEYDOWN);
    WindowProc(nullptr, WM_WTSSESSION_CHANGE, WTS_SESSION_UNLOCK, 0);
    Caps(WM_KEYDOWN);
    fake::Check(SwitchCount() == 2, "unlock recovers a key-up lost on the secure desktop");
    WindowProc(nullptr, WM_POWERBROADCAST, PBT_APMRESUMEAUTOMATIC, 0);
    Caps(WM_KEYDOWN);
    fake::Check(SwitchCount() == 3, "resume recovers a key-up lost during sleep");

    // All log I/O is redirected beside this test executable, away from user settings.
    wchar_t testPath[32768] = {};
    GetModuleFileNameW(nullptr, testPath, 32768);
    wchar_t* separator = wcsrchr(testPath, L'\\');
    if (!separator) { return 2; }
    *separator = L'\0';
    fake::logRoot = std::wstring(testPath) + L"\\log-test";
    CreateDirectoryW(fake::logRoot.c_str(), nullptr);
    wchar_t logPath[1024] = {};
    fake::Check(GetLogFilePath(logPath, 1024), "test log directory is available");
    HANDLE seed = CreateFileW(logPath, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, 0, nullptr);
    if (seed == INVALID_HANDLE_VALUE) { return 2; }
    LARGE_INTEGER limit = {}; limit.QuadPart = kMaxLogBytes;
    const bool seeded = SetFilePointerEx(seed, limit, nullptr, FILE_BEGIN) && SetEndOfFile(seed);
    CloseHandle(seed);
    fake::Check(seeded, "rotation fixture reaches the configured size limit");
    g_debug = true; DebugLog(L"rotation probe"); g_debug = false;
    WIN32_FILE_ATTRIBUTE_DATA current = {}, previous = {};
    const std::wstring previousPath = std::wstring(logPath) + L".old";
    fake::Check(GetFileAttributesExW(logPath, GetFileExInfoStandard, &current) &&
        GetFileAttributesExW(previousPath.c_str(), GetFileExInfoStandard, &previous) &&
        current.nFileSizeHigh == 0 && current.nFileSizeLow > 0 && current.nFileSizeLow < 1024 &&
        previous.nFileSizeLow == kMaxLogBytes, "oversized debug log rotates and new log accepts writes");
    std::printf("%d checks, %d failures\n", fake::checks, fake::failures);
    return fake::failures == 0 ? 0 : 1;
}
