## CapSwitch 1.1.0

A portable keyboard layout switcher for Windows 11 x64, now with a friendly retro keycap icon.

### Changes

- **No more accidental disabling:** enable/disable is only in the right-click tray menu. Ctrl+CapsLock and tray double-click no longer toggle it.
- **One switch per CapsLock press:** long holds no longer retrigger through the watchdog.
- Safer keyboard shortcut injection and hook replacement; fixes for tray menus and resource cleanup.
- Bounded debug logs, version information and no separate Visual C++ runtime requirement.

### Download

- **ZIP:** extract and run `CapSwitch.exe`.
- **EXE:** standalone portable application.
- **SHA256SUMS.txt:** checksums for both downloads.

Use CapsLock alone. CapsLock combined with Ctrl, Alt, Shift or Windows is ignored. To update, exit the previous instance from its tray menu before replacing it.

Window Message is the default switching method. Alternative shortcuts depend on your Windows settings; elevated applications, IMEs, games and remote sessions may need separate validation.
