// Exercise the actual production functions without installing a global hook,
// injecting input, changing user settings, or touching the running desktop instance.
#include <windows.h>
#include <shellapi.h>
#include <wtsapi32.h>
#include <objbase.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <wrl/client.h>
#include <cstdio>
#include <vector>
#include <algorithm>
#include <string>
#include <functional>

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
int audioRequests = 0;
int menuShows = 0;
UINT selectedMenuCommand = 0;
std::function<void()> duringMenu;
std::function<void(HMENU)> inspectMenu;
LRESULT WINAPI SyncCommand(HWND, UINT, WPARAM, LPARAM);
BOOL WINAPI Post(HWND, UINT, WPARAM, LPARAM);
BOOL WINAPI Popup(HMENU menu, UINT flags, int, int, int, HWND owner, const RECT*) {
    ++menuShows;
    if (inspectMenu) { inspectMenu(menu); }
    if (duringMenu && menuShows == 1) { duringMenu(); }
    if ((flags & TPM_RETURNCMD) != 0) { return static_cast<BOOL>(selectedMenuCommand); }
    if (selectedMenuCommand) { Post(owner, WM_COMMAND, selectedMenuCommand, 0); }
    return TRUE;
}
std::vector<WPARAM> actionArguments;
const std::wstring settingsKey = L"Software\\CapSwitch.Tests." + std::to_wstring(GetCurrentProcessId());
const std::wstring startupKey = settingsKey + L".Startup";

BOOL WINAPI Post(HWND, UINT message, WPARAM argument, LPARAM) {
    messages.push_back(message); actionArguments.push_back(argument); return TRUE;
}
HRESULT WINAPI CreateInstance(REFCLSID, LPUNKNOWN, DWORD, REFIID, LPVOID* result) {
    ++audioRequests; *result = nullptr; return E_FAIL;
}
LSTATUS WINAPI OpenKey(HKEY root, LPCWSTR path, DWORD options, REGSAM access, PHKEY result) {
    if (wcscmp(path, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run") == 0) { path = startupKey.c_str(); }
    return ::RegOpenKeyExW(root, wcscmp(path, L"Software\\CapSwitch") == 0 ? settingsKey.c_str() : path,
        options, access, result);
}
LSTATUS WINAPI CreateKey(HKEY root, LPCWSTR path, DWORD reserved, LPWSTR cls, DWORD options,
    REGSAM access, const LPSECURITY_ATTRIBUTES security, PHKEY result, LPDWORD disposition) {
    if (wcscmp(path, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run") == 0) { path = startupKey.c_str(); }
    return ::RegCreateKeyExW(root, wcscmp(path, L"Software\\CapSwitch") == 0 ? settingsKey.c_str() : path,
        reserved, cls, options, access, security, result, disposition);
}
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
#define CoCreateInstance fake::CreateInstance
#define RegOpenKeyExW fake::OpenKey
#define RegCreateKeyExW fake::CreateKey
#define TrackPopupMenu fake::Popup
#define SendMessageW fake::SyncCommand
#ifdef CAPSWITCH_MUTANT
#include "../artifacts/mutant.cpp"
#else
#include "../CapSwitch/main.cpp"
#endif
#undef AppendMenuW
#undef CoCreateInstance
#undef RegOpenKeyExW
#undef RegCreateKeyExW

LRESULT WINAPI fake::SyncCommand(HWND owner, UINT message, WPARAM argument, LPARAM data)
{
    return WindowProc(owner, message, argument, data);
}

void ResetFixture()
{
    fake::messages.clear(); fake::inputs.clear(); fake::hookOperations.clear();
    fake::removedHooks.clear();
    std::fill(std::begin(fake::keys), std::end(fake::keys), SHORT{0});
    fake::tick = 0; fake::removeError = ERROR_SUCCESS; fake::failInstall = false;
    fake::sendLimit = UINT_MAX;
    fake::audioRequests = 0; fake::actionArguments.clear();
    fake::menuShows = 0; fake::selectedMenuCommand = 0;
    fake::duringMenu = {}; fake::inspectMenu = {};
    fake::capsOn = false; fake::failAppendPopup = false;
    g_enabled = true; g_capsDown = false; g_debug = false; g_noTray = true;
    g_capsAction = CapsActionLayout;
    g_hook = reinterpret_cast<HHOOK>(INT_PTR{1}); g_retiredHook = nullptr;
    g_sessionNotifications = true; g_window = nullptr; g_tray = {};
    g_tray.uID = 1; g_trayVersion4 = true; g_menuTracking = false;
}

LRESULT Caps(WPARAM message, DWORD flags = 0, ULONG_PTR marker = 0)
{
    KBDLLHOOKSTRUCT key = {};
    key.vkCode = VK_CAPITAL; key.flags = flags; key.dwExtraInfo = marker;
    return LowLevelKeyboardProc(HC_ACTION, message, reinterpret_cast<LPARAM>(&key));
}

size_t SwitchCount()
{
    return std::count(fake::messages.begin(), fake::messages.end(), kCmdCapsAction);
}

class TestVolume final : public IAudioEndpointVolume {
public:
    BOOL muted = FALSE;
    HRESULT readResult = S_OK, writeResult = S_OK;
    int writes = 0;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void**) override { return E_NOINTERFACE; }
    ULONG STDMETHODCALLTYPE AddRef() override { return 1; }
    ULONG STDMETHODCALLTYPE Release() override { return 1; }
    HRESULT STDMETHODCALLTYPE GetMute(BOOL* value) override { *value = muted; return readResult; }
    HRESULT STDMETHODCALLTYPE SetMute(BOOL value, LPCGUID) override {
        ++writes; if (SUCCEEDED(writeResult)) { muted = value; } return writeResult;
    }
    HRESULT STDMETHODCALLTYPE RegisterControlChangeNotify(IAudioEndpointVolumeCallback*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE UnregisterControlChangeNotify(IAudioEndpointVolumeCallback*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetChannelCount(UINT*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetMasterVolumeLevel(float, LPCGUID) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetMasterVolumeLevelScalar(float, LPCGUID) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetMasterVolumeLevel(float*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetMasterVolumeLevelScalar(float*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetChannelVolumeLevel(UINT, float, LPCGUID) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetChannelVolumeLevelScalar(UINT, float, LPCGUID) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetChannelVolumeLevel(UINT, float*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetChannelVolumeLevelScalar(UINT, float*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetVolumeStepInfo(UINT*, UINT*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE VolumeStepUp(LPCGUID) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE VolumeStepDown(LPCGUID) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE QueryHardwareSupport(DWORD*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetVolumeRange(float*, float*, float*) override { return E_NOTIMPL; }
};

int AudioSmoke()
{
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) { return 2; }
    struct ComCleanup { ~ComCleanup() { CoUninitialize(); } } cleanup;
    Microsoft::WRL::ComPtr<IMMDeviceEnumerator> enumerator;
    HRESULT result = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(enumerator.GetAddressOf()));
    Microsoft::WRL::ComPtr<IMMDevice> device;
    if (SUCCEEDED(result)) { result = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, device.GetAddressOf()); }
    Microsoft::WRL::ComPtr<IAudioEndpointVolume> volume;
    if (SUCCEEDED(result)) { result = device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_INPROC_SERVER,
        nullptr, reinterpret_cast<void**>(volume.GetAddressOf())); }
    BOOL initial = FALSE;
    if (FAILED(result) || FAILED(volume->GetMute(&initial))) {
        std::puts("Audio smoke unavailable: no accessible default output."); return 2;
    }
    BOOL changed = initial;
    result = ToggleEndpointMute(volume.Get());
    HRESULT read = volume->GetMute(&changed);
    // Always restore the original state, including when observation fails.
    HRESULT restore = volume->SetMute(initial, nullptr);
    BOOL restored = !initial;
    HRESULT verifyRestore = volume->GetMute(&restored);
    fake::Check(SUCCEEDED(result) && SUCCEEDED(read) && changed != initial, "live default output mute toggles");
    fake::Check(SUCCEEDED(restore) && SUCCEEDED(verifyRestore) && restored == initial, "original live mute state restored");
    return fake::failures == 0 ? 0 : 1;
}

int main(int argc, char** argv)
{
    if (argc == 2 && strcmp(argv[1], "--audio-smoke") == 0) { return AudioSmoke(); }
    struct SettingsCleanup {
        ~SettingsCleanup() {
            RegDeleteTreeW(HKEY_CURRENT_USER, fake::settingsKey.c_str());
            RegDeleteTreeW(HKEY_CURRENT_USER, fake::startupKey.c_str());
        }
    } settingsCleanup;
    ResetFixture();
    fake::selectedMenuCommand = kMenuActionMute;
    WindowProc(nullptr, kTrayMessage, 0, MAKELPARAM(WM_RBUTTONUP, 1));
    WindowProc(nullptr, kTrayMessage, 0, MAKELPARAM(WM_CONTEXTMENU, 1));
    fake::Check(fake::menuShows == 1, "one v4 right-click notification pair opens one menu");
    fake::Check(g_capsAction == CapsActionMute, "selected menu command applies before callback returns");
    fake::selectedMenuCommand = 0;
    bool currentChecked = false;
    fake::inspectMenu = [&](HMENU menu) {
        for (int i = 0; i < GetMenuItemCount(menu); ++i) {
            HMENU child = GetSubMenu(menu, i);
            if (child && GetMenuState(child, kMenuActionMute, MF_BYCOMMAND) != UINT(-1)) {
                currentChecked = (GetMenuState(child, kMenuActionMute, MF_BYCOMMAND) & MF_CHECKED) != 0;
            }
        }
    };
    WindowProc(nullptr, kTrayMessage, 0, MAKELPARAM(WM_CONTEXTMENU, 1));
    fake::Check(currentChecked, "reopened menu shows the newly selected mode");
    ResetFixture();
    fake::duringMenu = [] { WindowProc(nullptr, kTrayMessage, 0, MAKELPARAM(WM_CONTEXTMENU, 1)); };
    WindowProc(nullptr, kTrayMessage, 0, MAKELPARAM(WM_CONTEXTMENU, 1));
    fake::Check(fake::menuShows == 1, "context notification during menu tracking cannot open a nested menu");
    fake::duringMenu = {};
    WindowProc(nullptr, kTrayMessage, 0, MAKELPARAM(WM_CONTEXTMENU, 1));
    fake::Check(fake::menuShows == 2, "menu cancellation leaves future menu opening enabled");
    ResetFixture();
    fake::Check(SetStartupEnabled(true) && IsStartupEnabled(), "autostart check recognizes the current executable registration");
    bool startupChecked = false;
    fake::inspectMenu = [&](HMENU menu) {
        startupChecked = (GetMenuState(menu, kMenuToggleStartup, MF_BYCOMMAND) & MF_CHECKED) != 0;
    };
    fake::selectedMenuCommand = kMenuToggleStartup;
    WindowProc(nullptr, kTrayMessage, 0, MAKELPARAM(WM_CONTEXTMENU, 1));
    fake::Check(startupChecked && !IsStartupEnabled(), "startup toggle uses checked state and applies once immediately");
    fake::selectedMenuCommand = 0;
    WindowProc(nullptr, kTrayMessage, 0, MAKELPARAM(WM_CONTEXTMENU, 1));
    fake::Check(!startupChecked, "next menu reflects changed autostart registration");
    ResetFixture(); g_trayVersion4 = false;
    WindowProc(nullptr, kTrayMessage, 1, WM_RBUTTONUP);
    WindowProc(nullptr, kTrayMessage, 1, WM_CONTEXTMENU);
    fake::Check(fake::menuShows == 1, "legacy fallback also accepts only one context event");
    ResetFixture();
    WindowProc(nullptr, kTrayMessage, 0, MAKELPARAM(WM_CONTEXTMENU, 2));
    fake::Check(fake::menuShows == 0, "callbacks for another tray icon are ignored");
    RegDeleteTreeW(HKEY_CURRENT_USER, fake::settingsKey.c_str());
    ResetFixture();
    fake::Check(g_capsAction == CapsActionLayout, "default mode remains layout switching");
    LoadSettings();
    fake::Check(g_capsAction == CapsActionLayout, "missing saved mode retains layout default");
    WindowProc(nullptr, WM_COMMAND, kMenuActionMute, 0);
    g_capsAction = CapsActionLayout; LoadSettings();
    fake::Check(g_capsAction == CapsActionMute, "mute mode persists through settings save/reload");
    WindowProc(nullptr, WM_COMMAND, kMenuActionNothing, 0);
    g_capsAction = CapsActionLayout; LoadSettings();
    fake::Check(g_capsAction == CapsActionNothing, "nothing mode persists through settings save/reload");
    HKEY testKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, fake::settingsKey.c_str(), 0, KEY_SET_VALUE, &testKey) != ERROR_SUCCESS) { return 2; }
    DWORD invalidMode = 999;
    RegSetValueExW(testKey, L"CapsAction", 0, REG_DWORD, reinterpret_cast<BYTE*>(&invalidMode), sizeof(invalidMode));
    g_capsAction = CapsActionLayout; LoadSettings();
    fake::Check(g_capsAction == CapsActionLayout, "invalid saved mode retains safe layout default");
    invalidMode = CapsActionMute;
    RegSetValueExW(testKey, L"CapsAction", 0, REG_BINARY, reinterpret_cast<BYTE*>(&invalidMode), sizeof(invalidMode));
    LoadSettings();
    fake::Check(g_capsAction == CapsActionLayout, "wrong saved type is rejected");
    RegCloseKey(testKey);

    ResetFixture(); g_capsAction = CapsActionNothing;
    for (int i = 0; i < 100; ++i) { Caps(WM_KEYDOWN); WatchdogTick(); }
    fake::Check(Caps(WM_KEYUP) == 1 && SwitchCount() == 0 && fake::inputs.empty(),
        "nothing mode suppresses CapsLock without layout or audio input");
    PerformCapsAction(CapsActionNothing);
    fake::Check(fake::audioRequests == 0, "nothing mode never requests audio");
    ResetFixture(); g_capsAction = CapsActionMute;
    for (int i = 0; i < 100; ++i) { Caps(WM_KEYDOWN); }
    const auto actionPosition = std::find(fake::messages.begin(), fake::messages.end(), kCmdCapsAction);
    const size_t actionIndex = static_cast<size_t>(actionPosition - fake::messages.begin());
    fake::Check(SwitchCount() == 1 && actionIndex < fake::actionArguments.size() &&
        fake::actionArguments[actionIndex] == CapsActionMute,
        "mute hold queues one mute action");
    WindowProc(nullptr, kCmdCapsAction, CapsActionMute, 0);
    fake::Check(fake::audioRequests == 1 && fake::inputs.empty(), "mute routes to audio without keyboard injection");
    g_capsAction = CapsActionNothing;
    WindowProc(nullptr, kCmdCapsAction, CapsActionMute, 0);
    fake::Check(fake::audioRequests == 1, "mode change cancels stale queued mute action");
    g_capsAction = CapsActionMute; g_enabled = false; PerformCapsAction(CapsActionMute);
    fake::Check(fake::audioRequests == 1, "pause blocks mute action");
    g_enabled = true; fake::keys[VK_CONTROL] = SHORT{-32768}; PerformCapsAction(CapsActionMute);
    fake::Check(fake::audioRequests == 1, "modified CapsLock does not change sound");

    TestVolume testVolume;
    fake::Check(SUCCEEDED(ToggleEndpointMute(&testVolume)) && testVolume.muted,
        "audio endpoint toggles from audible to muted");
    fake::Check(SUCCEEDED(ToggleEndpointMute(&testVolume)) && !testVolume.muted,
        "audio endpoint toggles from muted to audible");
    testVolume.readResult = E_FAIL; int writes = testVolume.writes;
    fake::Check(FAILED(ToggleEndpointMute(&testVolume)) && testVolume.writes == writes,
        "failed mute query does not write an invented state");
    testVolume.readResult = S_OK; testVolume.writeResult = E_ACCESSDENIED;
    fake::Check(FAILED(ToggleEndpointMute(&testVolume)), "audio write failure is propagated");
    fake::Check(ToggleEndpointMute(nullptr) == E_POINTER, "missing audio endpoint is safe");
    ResetFixture(); g_capsAction = CapsActionMute;
    HMENU actionMenu = CreatePopupMenu(); AppendCapsActionMenu(actionMenu);
    HMENU actionSubmenu = GetSubMenu(actionMenu, 0);
    fake::Check(actionSubmenu && GetMenuItemCount(actionSubmenu) == 3 &&
        (GetMenuState(actionSubmenu, kMenuActionMute, MF_BYCOMMAND) & MF_CHECKED) != 0 &&
        (GetMenuState(actionSubmenu, kMenuActionLayout, MF_BYCOMMAND) & MF_CHECKED) == 0 &&
        (GetMenuState(actionSubmenu, kMenuActionNothing, MF_BYCOMMAND) & MF_CHECKED) == 0,
        "tray action menu has three modes with only the current mode selected");
    AppendSwitchMethodMenu(actionMenu);
    fake::Check((GetMenuState(actionMenu, 1, MF_BYPOSITION) & MF_GRAYED) != 0,
        "layout backend menu is inactive in mute mode");
    DestroyMenu(actionMenu);
    fake::failAppendPopup = true;
    DWORD actionMenusBefore = GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS);
    for (int i = 0; i < 1000; ++i) {
        HMENU menu = CreatePopupMenu(); AppendCapsActionMenu(menu); DestroyMenu(menu);
    }
    fake::Check(GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS) == actionMenusBefore,
        "failed action-menu attachment does not leak USER objects");
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
