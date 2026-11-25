# AGENTS.md

Repository notes for future sessions working on Friendship Streaks, a Geode mod for Geometry Dash.

## What this is

Shared (mutual) daily streaks between two paired GD players. Flame badge in the main menu, streaks window, friend detail with a 14 day calendar, flames in friend lists, notifications, settings, plus a one file sync server in `server/`.

## Targets

- GD 2.2081 with Geode SDK 5.11.2 (`mod.json` in the repo points here)
- GD 2.2091 with Geode SDK 6.0.0
- CI also builds Windows and macOS; iOS is disabled because Geode's own bindings fail to build on the runner (FMOD types missing), not because of mod code

Same sources for both; only `mod.json`'s `geode` and `gd` fields change. `scripts/build_all.sh` does both.

## Version differences that matter

- GD 2.2091 renamed the friends list row: `GJUserCell` -> `GJCompactUserCell`. `src/main.cpp` picks the header with `__has_include` and defines `FSTREAK_USER_CELL`, so the `$modify` block and the `Base::method()` call both go through the macro.
- Geode 6 takes `asp::Duration` for `WebRequest::timeout`, Geode 5 takes `std::chrono::seconds`. Guarded by `GEODE_COMP_GD_VERSION >= 22091` in `StreakApi.cpp`.
- `gd::string` no longer exists in Geode 6. Use the `toStdString` template in `main.cpp` instead of naming the type.
- `$modify` passes the base to `geode::Modify<derived, base>`, so a macro expanding to the class name works fine.

## Gotchas learned the hard way

- `Notification::create(...)` does **not** show anything by itself; always call `->show()`. `Notify.cpp` wraps this in `pop()`.
- Do not add a `CCNode` that owns a `CCMenu` into another `CCMenu` (the main menu badge). Add it to the layer and position it from the profile button's world position.
- Async callbacks that touch a popup must capture `geode::Ref<Popup>`; the user can close the window before the response arrives.
- `mod.json`'s `geode` field has no `v` prefix.
- The Geode 6 SDK checkout needs `bin/6.0.0/Geode.android{32,64}.so` from the release zips before CMake can link.
- Local Android builds need the NDK extracted with `unzip` (Python's zipfile breaks the clang wrapper symlinks) and its `bin` on `PATH`.

## Tests

```sh
g++ -std=c++23 -O2 tests/core_test.cpp -I src -o /tmp/core_test && /tmp/core_test
cd server && python3 -m unittest test_server -v
```

`src/core/FriendStreakCore.hpp` stays free of Geode headers so it can be tested on the host. Keep it that way.

## Server contract

The mod parses exactly the JSON produced by `server/server.py` (`PairState::fromServer`). If you change one side, change `test_wire_contract_matches_mod` too.

## Platform switches

`src/Platform.hpp` decides what the message integration can do:

- `FSTREAK_CAN_OPEN_WRITE_POPUP` — `GJWriteMessagePopup::create` only has an address on Android for GD 2.2091 (all platforms for 2.2081). Elsewhere the friend window opens the profile page instead.
- `FSTREAK_CAN_HOOK_MESSAGE` — GD 2.2091 does not bind `uploadMessageFinished` on Windows, so the hook and the automatic tracking are compiled out there and the window shows **Mark written**, which reports the event by pair id.

Both come from `GEODE_COMP_GD_VERSION` (22081 / 22091) and the `GEODE_IS_*` macros. Check `build-<target>/bindings/bindings/Geode/CodegenData.json` (`bindings.win` / `bindings.android32`) before adding a call to a GD class: `missing` means the symbol is not available on that platform.

## Layout rules of thumb

- Streaks window is 460x300, friend detail is 420x340, pairing is 420x260. Vertical constants live at the top of each popup; check for overlaps before moving things.
- All drawing helpers are in `src/ui/Ui.hpp` (panel, pill, button, dots) and take plain nodes, so they work in any layer.
- Days are server (UTC) days everywhere in the UI. Do not mix in local time.
