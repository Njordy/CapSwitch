CapSwitch 1.1.0 - Windows 11 x64

Extract to a permanent folder and run CapSwitch.exe. No installation or separate
Visual C++ runtime is required. Look for the beige keycap icon in the system tray
or its overflow menu.

Press CapsLock alone to switch to the next keyboard layout. Holding the key
requests just one switch. CapsLock with Ctrl, Alt, Shift or Windows held is ignored.

Right-click the tray icon for settings and Exit. Enable/Disable is only in this
menu; Ctrl+CapsLock and double-click no longer disable the application.
Physical CapsLock is suppressed even when layout switching is disabled.

Window Message is the default switching method. If an application does not respond,
try an alternative under Switching Method; it must match your Windows shortcuts.
Elevated apps, games, IMEs and remote desktops can handle input differently.

To start automatically, select Start with Windows. If you move the executable,
enable this option again from the new location.

Updating: choose Exit in the old instance's tray menu, replace the executable,
then launch the new one. Settings for switching method and logging are preserved.

Optional arguments: --debug (logging), --no-tray (hide icon).
Debug logs can include window titles and are kept in:
  %LOCALAPPDATA%\CapSwitch\CapSwitch.log
  %LOCALAPPDATA%\CapSwitch\CapSwitch.log.old
Each file is limited to approximately 1 MiB. Logging is off by default.

If a key release is lost, one complete CapsLock press/release re-arms detection;
the first press can be ignored. Session unlock and resume also recover state.

See CHANGELOG.md for the changes in this version.
