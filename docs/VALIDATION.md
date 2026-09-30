# CapSwitch 1.1.0 validation

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
