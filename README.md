<p align="center"><img src="assets/capswitch.png" width="144" alt="CapSwitch retro keycap mascot"></p>

# CapSwitch 1.2.1

A small, portable Windows 11 utility that gives CapsLock one of three actions: **switch keyboard layout, toggle sound mute, or do nothing**. The real CapsLock state stays off.

## Download and run

Open this repository's **Releases** page and download `CapSwitch-1.2.1-windows-x64.zip` or the standalone `.exe`. GitHub's automatic **Source code** archives contain source, not the ready-to-run application.

Extract the ZIP into a permanent folder and run `CapSwitch.exe`. No installer or separate Visual C++ runtime is required. The icon appears in the notification area (possibly its overflow menu).

To update, choose **Exit** in the old instance's tray menu, replace the executable, then run the new version. If you move it to another folder, enable **Start with Windows** again from the new location.

## Controls

- Right-click the tray icon and choose **CapsLock Action**:
  - **Switch keyboard layout (default)** — request the next installed layout.
  - **Toggle sound mute** — mute/unmute the current default Windows output device, without changing its volume level or microphone state.
  - **Nothing (disable CapsLock)** — block the key completely; no layout or audio action.
- Press **CapsLock alone** to perform the selected action. Holding it performs the action once, including across watchdog ticks.
- The selected action is saved and restored on the next launch. Upgrading an earlier build keeps layout switching as the default.
- CapsLock with **Ctrl, Alt, Shift or Windows** held is ignored. Release CapsLock before trying again.
- **Pause / Resume is available only in the right-click tray menu.** It temporarily suspends the selected action. Ctrl+CapsLock and tray double-click cannot pause the app.
- Physical CapsLock stays suppressed in all three modes and while paused.
- **Exit** restores normal CapsLock behavior by removing the hook.
- Enable **Start with Windows** from the tray menu if desired; it affects only the current Windows user.

## Switching methods

**Window Message** is the default: one `WM_INPUTLANGCHANGEREQUEST` is posted to the focused window in the foreground application. Some applications do not honor this request.

**Layout Switching Method** also offers **Alt+Shift**, **Ctrl+Shift** and **Win+Space**. This submenu is active only in layout mode. Choose a shortcut supported by your Windows keyboard settings. These use marked `SendInput` events. Synthetic switching is skipped while a modifier or a participating key is already held, so the application does not release your held keys.

Mute uses the Windows audio endpoint API directly and resolves the default output on each press, so switching between speakers and headphones is supported. If no output device is available, the action fails safely; diagnostics are available with debug logging.

Windows integrity restrictions apply: a normally launched CapSwitch may not control an elevated application. Applications, games, IMEs and remote desktops may handle layout switching differently.

## Tray menu

CapsLock Action, Pause / Resume, Start with Windows, Layout Switching Method, Enable Debug Logging, Open Log File, Reload keyboard hook, Exit.

The tooltip includes the version and selected action (or Paused). The application starts unpaused and restores the selected action. Neither keyboard chords nor double-clicks change that state.

## Recovery and diagnostics

The watchdog runs every 500 ms and periodically replaces the low-level keyboard hook. A replacement is installed before the previous hook is removed; installation failure preserves the previous handle. Failed removals retain bounded ownership for retry.

The physical CapsLock latch is cleared by key-up, not asynchronous key state or a hold timeout. Unlock/reconnect and sleep-resume notifications recover state after a desktop/session interruption. If a release is lost outside these boundaries, one complete CapsLock press/release re-arms the latch; its initial press may be ignored.

CapsLock-off correction waits until modifiers are released, then sends a single marked down/up batch. Debug logs are opt-in and limited to a current file plus one rotated file, each at most approximately 1 MiB:

- `%LOCALAPPDATA%\CapSwitch\CapSwitch.log`
- `%LOCALAPPDATA%\CapSwitch\CapSwitch.log.old`

Debug logs can include foreground window titles. Routine successful hook replacement is not logged.

Command line: `CapSwitch.exe [--debug] [--no-tray]`. `--debug` enables logging for that run. `--no-tray` hides the notification icon; use Task Manager to exit that instance.

CapsLock action (`CapsAction`), layout switching method and debug settings are stored in `HKCU\Software\CapSwitch`. Startup uses `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`.

## Build and test

Requirements: Windows 11 x64, Visual Studio 2022 with **Desktop development with C++**, the v143 toolset and a Windows SDK.

From PowerShell in this project directory:

```powershell
.\scripts\Test.ps1
.\scripts\Test.ps1 -Calibrate
.\scripts\Build-Release.ps1
```

`Test.ps1` executes production functions with substituted keyboard/hook APIs, plus real Win32 menu and log-file checks. `-Calibrate` confirms the tests fail when the original watchdog and submenu defects are deliberately reintroduced into a disposable build. These are not a substitute for testing physical keyboard input and your target applications.

The mode-setting checks use a temporary registry key, which is deleted afterwards. `artifacts\tests\CapSwitchTests.exe --audio-smoke` optionally verifies the endpoint toggle against the real default output: it briefly changes mute, then restores and verifies its original state.

`Build-Release.ps1` runs tests, builds with a static C++ runtime, verifies the executable version, and creates EXE, ZIP and SHA-256 checksums under `artifacts\release-1.2.1`. It does not stop or replace an already-running instance. Build outputs and local agent settings are excluded from source publication.

Alternatively open `CapSwitch.sln` and build `Release|x64`; the default output is `x64\Release\CapSwitch.exe`.

The version is maintained in `CapSwitch\version.h` and embedded in Windows file properties and the tray tooltip. See [changes](CHANGELOG.md), [release notes](docs/RELEASE-NOTES.md), [validation](docs/VALIDATION.md), and the [Russian publishing guide](docs/PUBLISHING-RU.md).

## Artwork

The transparent retro keycap mascot is embedded in the EXE as a multi-resolution icon. The PNG source and generation prompt are in `assets/`.

## License

No license has been selected yet.


