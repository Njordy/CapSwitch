## CapSwitch 1.2.1

A portable CapsLock utility for Windows 11 x64 with three selectable actions.

### Changes

- **Tray menu fix:** one right-click opens one menu; selecting a command closes it once and updates settings immediately. Checkmarks reflect the current state on the next opening.
- Prevent nested popups and preserve existing autostart settings.

- **CapsLock Action → Switch keyboard layout:** the default behavior from earlier versions.
- **CapsLock Action → Toggle sound mute:** mute/unmute the current default Windows output. The volume level and microphone state are preserved.
- **CapsLock Action → Nothing (disable CapsLock):** suppress CapsLock completely, with no layout or sound action.
- The selected mode survives restart and appears in the tray tooltip.
- One action per press, safe mode changes, and an explicit tray-only Pause / Resume command.
- Existing transparent mascot, bounded debug logs and no separate Visual C++ runtime requirement.

### Download

- **ZIP:** extract and run `CapSwitch.exe`.
- **EXE:** standalone portable application.
- **SHA256SUMS.txt:** checksums for both downloads.

Use CapsLock alone. CapsLock combined with Ctrl, Alt, Shift or Windows is ignored. To update, exit the previous instance from its tray menu before replacing it.

Window Message is the default switching method. Alternative shortcuts depend on your Windows settings; elevated applications, IMEs, games and remote sessions may need separate validation.


