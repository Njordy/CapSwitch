<p align="center"><img src="assets/capswitch.png" width="144" alt="CapSwitch retro keycap mascot"></p>

# CapSwitch 1.1.0

A small, portable Windows 11 utility that turns **CapsLock into a keyboard layout switcher** and keeps the real CapsLock state off.

## Download and run

Open this repository's **Releases** page and download `CapSwitch-1.1.0-windows-x64.zip` or the standalone `.exe`. GitHub's automatic **Source code** archives contain source, not the ready-to-run application.

Extract the ZIP into a permanent folder and run `CapSwitch.exe`. No installer or separate Visual C++ runtime is required. The icon appears in the notification area (possibly its overflow menu).

To update, choose **Exit** in the old instance's tray menu, replace the executable, then run the new version. If you move it to another folder, enable **Start with Windows** again from the new location.

## Controls

- Press **CapsLock alone** to request the next installed keyboard layout.
- Holding CapsLock requests one switch, including across watchdog ticks.
- CapsLock with **Ctrl, Alt, Shift or Windows** held is ignored. Release CapsLock before trying again.
- **Enable / Disable is available only in the right-click tray menu.** Ctrl+CapsLock and tray double-click no longer toggle the app.
- Physical CapsLock stays suppressed even while layout switching is disabled.
- **Exit** restores normal CapsLock behavior by removing the hook.
- Enable **Start with Windows** from the tray menu if desired; it affects only the current Windows user.

## Switching methods

**Window Message** is the default: one `WM_INPUTLANGCHANGEREQUEST` is posted to the focused window in the foreground application. Some applications do not honor this request.

The tray menu also offers **Alt+Shift**, **Ctrl+Shift** and **Win+Space**. Choose a shortcut supported by your Windows keyboard settings. These use marked `SendInput` events. Synthetic switching is skipped while a modifier or a participating key is already held, so the application does not release your held keys.

Windows integrity restrictions apply: a normally launched CapSwitch may not control an elevated application. Applications, games, IMEs and remote desktops may handle layout switching differently.

## Tray menu

Enable / Disable, Start with Windows, Switching Method, Enable Debug Logging, Open Log File, Reload keyboard hook, Exit.

The tooltip includes the version and enabled/disabled state. The application starts enabled. Neither keyboard chords nor double-clicks change that state.

## Recovery and diagnostics

The watchdog runs every 500 ms and periodically replaces the low-level keyboard hook. A replacement is installed before the previous hook is removed; installation failure preserves the previous handle. Failed removals retain bounded ownership for retry.

The physical CapsLock latch is cleared by key-up, not asynchronous key state or a hold timeout. Unlock/reconnect and sleep-resume notifications recover state after a desktop/session interruption. If a release is lost outside these boundaries, one complete CapsLock press/release re-arms the latch; its initial press may be ignored.

CapsLock-off correction waits until modifiers are released, then sends a single marked down/up batch. Debug logs are opt-in and limited to a current file plus one rotated file, each at most approximately 1 MiB:

- `%LOCALAPPDATA%\CapSwitch\CapSwitch.log`
- `%LOCALAPPDATA%\CapSwitch\CapSwitch.log.old`

Debug logs can include foreground window titles. Routine successful hook replacement is not logged.

Command line: `CapSwitch.exe [--debug] [--no-tray]`. `--debug` enables logging for that run. `--no-tray` hides the notification icon; use Task Manager to exit that instance.

Switching method and debug settings are stored in `HKCU\Software\CapSwitch`. Startup uses `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`.

## Build and test

Requirements: Windows 11 x64, Visual Studio 2022 with **Desktop development with C++**, the v143 toolset and a Windows SDK.

From PowerShell in this project directory:

```powershell
.\scripts\Test.ps1
.\scripts\Test.ps1 -Calibrate
.\scripts\Build-Release.ps1
```

`Test.ps1` executes production functions with substituted keyboard/hook APIs, plus real Win32 menu and log-file checks. `-Calibrate` confirms the tests fail when the original watchdog and submenu defects are deliberately reintroduced into a disposable build. These are not a substitute for testing physical keyboard input and your target applications.

`Build-Release.ps1` runs tests, builds with a static C++ runtime, verifies the executable version, and creates EXE, ZIP and SHA-256 checksums under `artifacts\release-1.1.0`. It does not stop or replace an already-running instance. Build outputs and local agent settings are excluded from source publication.

Alternatively open `CapSwitch.sln` and build `Release|x64`; the default output is `x64\Release\CapSwitch.exe`.

The version is maintained in `CapSwitch\version.h` and embedded in Windows file properties and the tray tooltip. See [changes](CHANGELOG.md), [release notes](docs/RELEASE-NOTES.md), [validation](docs/VALIDATION.md), and the [Russian publishing guide](docs/PUBLISHING-RU.md).

## Artwork

The transparent retro keycap mascot is embedded in the EXE as a multi-resolution icon. The PNG source and generation prompt are in `assets/`.

## License

No license has been selected yet.
