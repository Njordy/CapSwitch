# CapSwitch validation

## 1.2.1 — 2026-10-01

The reported menu regression was reproduced against 1.2.0 by five newly added failing checks. The previous suite exercised menu contents and commands independently and did not cover popup tracking or duplicate tray notifications.

The fix selects one context event according to the successfully negotiated tray protocol, guards nested tracking, and uses TPM_RETURNCMD | TPM_NONOTIFY to apply the returned command synchronously. NIM_SETFOCUS restores tray focus after completion.

The regression checks substitute TrackPopupMenu to replay paired right-click notifications, nested callbacks, cancellation and a selected command. Real Win32 menus and temporary registry keys verify the next menu's mode/startup checkmarks. This tests production event routing; it is not a live Explorer mouse-click recording.

At diagnosis the user's current Run value was `"C:\Apps\CapSwitch.exe"`, matching the active executable. This setting is preserved rather than re-created or disabled by the update.

Final evidence: **61 checks, 0 failures**; release build with 0 warnings and 0 errors. Version 1.2.1 was installed at `C:\Apps\CapSwitch.exe`, started successfully and answered a bounded WM_NULL request. The startup Run value was identical before/after replacement. Release checksum verification and installed/standalone EXE comparison passed. Raw evidence: `artifacts/menu-before-fix.txt` (the five pre-fix failures), `artifacts/tests.txt`, `artifacts/release-build.log`, and `artifacts/runtime-1.2.1.json`.

## 1.2.0 — 2026-10-01

- Release x64 build passed with 0 warnings and 0 errors. FileVersion and ProductVersion are 1.2.0.
- Regression suite: **51 checks, 0 failures**. Added mode persistence and validation, radio-menu selection, Nothing suppression, single queued mute on hold, stale-action cancellation, paused/modified mute suppression, endpoint mute/unmute and error handling.
- Settings tests use a temporary per-process test registry key and delete it afterwards; user settings are not modified by the suite.
- Real audio smoke: acquired the current default render/eConsole endpoint and executed the production endpoint toggle. Observed the mute state change, then restored and verified the original state. Both checks passed. This verifies the endpoint operation, not a physical CapsLock press through the global hook.
- Updated the project executable, then discovered that the active instance was the separately installed `C:\Apps\CapSwitch-1.1.0-windows-x64.exe`. Closed that instance normally and launched version 1.2.0 as `C:\Apps\CapSwitch.exe`. Verified startup and message-loop responsiveness. The separately installed earlier EXE remains in place; the old project EXE is retained under `artifacts/previous-1.1.0/CapSwitch.exe`.
- Verified SHA-256 checksums and equality of installed, standalone and ZIP-contained EXEs. Release files are under `artifacts/release-1.2.0`.
- Raw evidence: `artifacts/tests.txt`, `artifacts/audio-smoke.txt`, `artifacts/release-build.log`, `artifacts/runtime-1.2.0.json`.
- Remaining manual coverage: physical presses in all modes, tray use, live default-output device changes, and the broader manual scenarios below. No new long-duration leak-free claim is made.

## 1.1.0 — previous validation

Checked on Windows x64, 2026-09-30, with Visual Studio 2022 v143 (MSVC 14.44).

## Automated evidence

- Release x64 build completed successfully; the executable embeds FileVersion and ProductVersion `1.1.0`.
- `scripts/Test.ps1`: **30 checks, 0 failures** against the actual production functions. Keyboard, hook and session-registration APIs are substituted to avoid changing live desktop input.
- Coverage: long holds with and without typematic repeats; watchdog/replacement while held; modifier chords; explicit menu enable/disable; ignored double-click; injected-event isolation; safe shortcut/correction batches; partial/blocked injection; replacement failure; failed/invalid unhook; session-unlock and resume recovery.
- Real Win32 resource check: 1,000 failed submenu attachments leave USER-object count unchanged after warm-up.
- Real file check: a seeded 1 MiB debug log rotates to `.old`, and the new log receives the next entry. Test logs are redirected under the test executable directory.
- Calibration: deliberately restoring the async-state watchdog reset and omitting unattached-submenu destruction makes the relevant checks fail. The mutant run exits 1 with four failures; the normal suite exits 0. Production source is not mutated.
- ICO inspection: transparent RGBA at 16, 20, 24, 32, 40, 48, 64, 128 and 256 px. The 16 px raster was visually inspected enlarged.
- Dependency inspection: only SHELL32, USER32, ADVAPI32, ole32, WTSAPI32 and KERNEL32. No separate MSVC runtime DLL is imported.
- Release archive contains exactly `CapSwitch.exe`, `README.txt`, and `CHANGELOG.md`. The archived, standalone and installed executables have matching SHA-256 hashes; the checksum manifest verifies both release downloads.

## Live smoke check

The previous instance was closed through its own WM_CLOSE handler. Its executable was retained under `artifacts/previous-build/CapSwitch.exe` before replacement. Version 1.1.0 was launched from the existing `x64/Release/CapSwitch.exe` path, preserving the startup path.

The new process remained running and its message window answered a bounded WM_NULL request. This verifies startup and message-loop responsiveness, not physical keyboard behavior or a long-duration leak-free claim.

## Remaining manual coverage

- Physical CapsLock holds and repeated taps in the user's actual applications.
- Tray rendering at different display scales and repeated menu open/dismiss cycles.
- Actual lock/unlock, sleep/resume, Explorer restart, elevated apps, IMEs and remote sessions.
- CapsLock LED/toggle correction, including the background thread's GetKeyState behavior. The earlier theoretical stale-state concern is not claimed resolved by the mock tests.
- Long-duration memory/resource behavior under representative use. Short process snapshots cannot prove the absence of every leak.

## Reproduce

```powershell
.\scripts\Test.ps1 -Calibrate
.\scripts\Build-Release.ps1
```

Local raw evidence: `artifacts/tests.txt`, `artifacts/calibration.txt`, `artifacts/release-build.log`, `artifacts/runtime-start.json`. These local artifacts are excluded from source publication.
