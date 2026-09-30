# Changelog

## 1.2.1 — 2026-10-01

- Fix duplicate tray-menu opening by accepting only the context event for the negotiated notification protocol.
- Prevent nested menu tracking while an existing popup is active.
- Apply the selected menu command synchronously after closing the popup, so the next opening reflects updated checkmarks and settings.
- Return focus to the notification area after menu completion.
- Add regression tests for the complete tray-notification sequence, immediate command application, updated checkmarks, cancellation, protocol fallback and startup registration state.
- Autostart remains opt-in; existing current-path startup registrations are preserved.

## 1.2.0 — 2026-10-01

- Add three persistent CapsLock actions: Switch keyboard layout (default), Toggle sound mute, and Nothing (disable CapsLock).
- Toggle master mute on the current default Windows output using Core Audio. Volume level and microphone mute are unchanged.
- Show the current action in the tray tooltip and disable the layout-backend submenu outside layout mode.
- Rename the temporary enable/disable control to Pause / Resume. All modes keep physical CapsLock suppressed.
- Discard queued presses whose action no longer matches the selected mode, and preserve the hold latch across mode changes.
- Add regression checks for audio toggling/failure paths, mode persistence/validation, menu state and action routing.

## 1.1.0 — 2026-09-30

- Replace the tray/executable icon with a transparent beige retro keycap mascot, including 16–256 px icon sizes.
- Remove Ctrl+CapsLock and double-click enable/disable shortcuts; toggling is available only through the explicit tray menu command.
- Suppress repeated switching during a long CapsLock hold; remove async-state and elapsed-hold resets.
- Preserve held-key state when enabling/disabling and replacing the hook. Recover state on session reconnect/unlock and resume.
- Ignore modified CapsLock chords and skip synthetic shortcuts when participating keys are held.
- Defer CapsLock-off correction while modifiers are held; send correction in one batch and attempt cleanup after partial injection.
- Install replacement hooks before removing old ones, preserve the old hook on installation failure, and retain failed removals for retry.
- Release unattached submenus when menu insertion fails; post WM_NULL after the tray menu closes.
- Rotate debug logs at approximately 1 MiB, retaining one previous file. Remove periodic success-log noise.
- Initialize COM for shell operations, report relevant API failures, validate stored setting types, and clean up startup/message-loop failure paths.
- Add version resources, a static C++ runtime, regression tests and a reproducible release-packaging script.

## Earlier builds

Earlier local builds did not embed a product version. Their exact release history is not reconstructed here.
