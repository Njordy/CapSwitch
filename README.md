# CapSwitch

CapSwitch is a small Windows 11 utility that turns `CapsLock` into a keyboard
layout switcher while keeping the real CapsLock state off.

It was written as a modern, more defensive replacement for Switchy-style tools:
no `Win+Space` simulation, no `Alt+Shift` simulation, and no held modifier keys.

## Features

- `CapsLock` switches to the next installed keyboard layout.
- `Ctrl+CapsLock` enables or disables CapSwitch.
- Physical `CapsLock` is always suppressed.
- If Windows still manages to enable CapsLock, CapSwitch turns it back off.
- Tray menu with enable/disable, startup toggle, hook reload, and exit.
- Single native x64 executable with no runtime dependencies.
- Optional debug log.

## Windows Support

CapSwitch targets Windows 11 only.

The project intentionally does not carry compatibility branches for older
Windows versions.

## How It Works

CapSwitch installs a low-level keyboard hook with `WH_KEYBOARD_LL`. The hook is
used only to intercept and suppress the physical CapsLock key before the active
application receives it.

Layout switching is requested directly from the foreground window:

1. Read the foreground window and its input thread.
2. Find the focused child window when Windows exposes it.
3. Post `WM_INPUTLANGCHANGEREQUEST` with `HKL_NEXT` to both focused and foreground targets.

Synthetic keyboard input is used only for one thing: forcing CapsLock off if the
system state becomes enabled.

## Reliability

Windows can silently remove a low-level keyboard hook if the hook callback stalls
too long during a system freeze or input stack delay. CapSwitch keeps the hook
callback small and adds recovery logic:

- a watchdog checks CapsLock state every 500 ms;
- the watchdog periodically reinstalls the keyboard hook;
- enable/disable also reinstalls the keyboard hook;
- injected cleanup keystrokes are marked and ignored by the hook;
- stale internal key state is reset if a key-up event is lost;
- the tray menu exposes `Reload keyboard hook` for manual recovery.

## Tray Menu

Right-click the tray icon:

- `Enable/Disable CapSwitch`
- `Start with Windows`
- `Reload keyboard hook`
- `Exit`

The startup option writes to:

```text
HKCU\Software\Microsoft\Windows\CurrentVersion\Run
```

It does not require administrator privileges.

## Elevated Apps

Windows integrity levels still apply. If the foreground app is running as
administrator, an unelevated CapSwitch process may not be able to control it
reliably. Run CapSwitch as administrator if you need it to work inside elevated
applications.

## Build

Requirements:

- Windows 11
- Visual Studio 2022 with the v143 C++ toolset

Build from Visual Studio:

1. Open `CapSwitch.sln`.
2. Select `Release|x64`.
3. Build.

Build from PowerShell:

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe" CapSwitch.sln /p:Configuration=Release /p:Platform=x64 /m
```

Output:

```text
x64\Release\CapSwitch.exe
```

## Command Line

```text
CapSwitch.exe [--debug] [--no-tray]
```

- `--debug` writes `%LOCALAPPDATA%\CapSwitch\CapSwitch.log`.
- `--no-tray` runs without a tray icon.

## License

No license has been selected yet.
