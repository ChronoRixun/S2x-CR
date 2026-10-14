# Saved card order

Drag the body of a server card onto another card. The highlighted hint says **Place before** or **Place after**, depending on which half of the destination card the pointer is over. Buttons and editable controls keep their normal click behavior. To reorder with the keyboard, focus a card and press **Alt+Left** or **Alt+Right**.

Order is shared by Cards and Roster and survives restart. Hidden presets remain in the master order and return to their relative position when shown. Duplicate-port presets are separate cards: identity is the normalized preset file path, never the port. Changing a server's display name or port keeps its position. New presets and Save As copies append after existing cards. Externally renaming the JSON file creates a new identity and appends it; this does not try to guess that two configurations represent the same server.

The preference is stored outside preset files, under `<game>/s2x/server-manager/card-order-<preset-directory SHA256>.json`. Separate game/preset folders have separate orders. Writes use a temporary file and atomic replacement. A failed reorder save displays an error and leaves the displayed order unchanged. No server settings, processes, or preset contents change during reordering. Invalid preference JSON falls back to the original deterministic preset order.

Regression check (from repository root, after building):

```powershell
powershell.exe -NoProfile -STA -ExecutionPolicy Bypass -File tests/server-manager/card-order.ps1
```

The test loads the built application's actual FleetViewModel, creates GUID-scoped temporary game/preset folders, and checks before/after moves, keyboard bounds, duplicate ports, hidden cards, restart, edited titles/ports, new and deleted files, external renames, SavePreset/Save As, scoped storage, corrupt preferences and failed writes. It verifies reorder operations leave preset bytes untouched. It does not launch, stop, or query any server, and does not substitute a separate model implementation. Drag hit testing and visual hints still require an application UI check.
