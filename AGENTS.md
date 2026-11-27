# AGENTS.md

Repository notes for future sessions working on Friendship Streaks, a Geode mod for Geometry Dash.

## What this is

Shared (mutual) daily streaks between two paired GD players. The streak is shown where the messages are: a flame with the day count in the chat list and in the open chat, and a compact card (streak numbers, last 7 days, code exchange) that opens from that flame. Colours follow the streak length. Notifications, settings, and a one file sync server in `server/`. There is deliberately no main menu badge and no separate streaks window.

## Targets

- GD 2.2091 with Geode SDK 6.0.0, Windows, macOS, Android32 and Android64
- GD 2.2081 is **not** supported. It has no chat threads (`MessageThreadsPage` / `ThreadViewPopup`), so the whole "streak lives in the chat" design has nothing to attach to. The 2.2081 build and its `GJMessagePopup` fallback were deleted in v1.2.0; do not bring them back.
- iOS is disabled because Geode's own bindings fail to build on the runner (FMOD types missing), not because of mod code

One `mod.json`, one workflow (`.github/workflows/multi-platform.yml`), one combined `.geode`.

## Version differences that matter

- GD 2.2091 renamed the friends list row: `GJUserCell` -> `GJCompactUserCell`. `src/main.cpp` picks the header with `__has_include` and defines `FSTREAK_USER_CELL`, so the `$modify` block and the `Base::method()` call both go through the macro.
- Geode 6 takes `asp::Duration` for `WebRequest::timeout`; the Geode 5 branch is gone.
- `gd::string` no longer exists in Geode 6. Use the `toStdString` template in `main.cpp` instead of naming the type.
- `$modify` passes the base to `geode::Modify<derived, base>`, so a macro expanding to the class name works fine.

## Gotchas learned the hard way

- `Notification::create(...)` does **not** show anything by itself; always call `->show()`. `Notify.cpp` wraps this in `pop()`.
- Do not add a `CCNode` that owns a `CCMenu` into another `CCMenu`. Add it to the layer and position it from the anchor node's world position.
- While `MenuLayer::init` runs the menu scene is not the running scene yet, so a helper node added to `getRunningScene()` there is dropped with the loading scene. Schedule the attach one frame later.
- `CCLabelBMFont::getString()` returns `char const*` in Geode 5 and a string view in places in Geode 6; build text with `std::string(...)` rather than `.c_str()` on it.
- Never walk a whole GD screen to find a label you want to decorate if the screen exposes one: `ProfilePage::m_usernameLabel` exists for exactly this. Walking the tree reads `getString()`/`m_sFntFile` on labels the mod does not own, which is where the profile page crash reports came from.
- `ProfilePage::m_ownProfile` is true for the page you open from the main menu. Skip it: no pair can point at yourself, so decorating it can only touch a page that GD is still laying out.
- Async callbacks that touch a popup must capture `geode::Ref<Popup>`; the user can close the window before the response arrives.
- `mod.json`'s `geode` field has no `v` prefix.
- The Geode 6 SDK checkout needs `bin/6.0.0/Geode.android{32,64}.so` from the release zips before CMake can link.
- Local Android builds need the NDK extracted with `unzip` (Python's zipfile breaks the clang wrapper symlinks) and its `bin` on `PATH`.

## Releasing

1. Push to `main`. The workflow builds Windows, macOS, Android32 and Android64 and then a `package` job combines them into one `.geode` (artifact `Build Output`).
2. Download the artifact and upload it as `friendship-streaks-v<ver>-gd2.2091-windows-macos-android.geode`, plus a zip with the package, the docs, `logo.png` and `server/`.
3. Asset uploads go to `https://uploads.github.com/repos/<owner>/<repo>/releases/<id>/assets?name=<file>`; `api.github.com` returns 404 for uploads.
4. The packaged `.geode` already contains `logo.png`, `about.md` and `changelog.md` from the repository root.

CI notes: a macOS job can sit in the queue for a long time and is cancelled when a newer push arrives, so check the job list, not just the run conclusion. Bindings differ per platform: `ThreadViewPopup::onRefresh` exists on Windows only, and hooking a function without an address fails at compile time with a `static_assert`.

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

- `FSTREAK_CAN_HOOK_MESSAGE` — GD 2.2091 does not bind `uploadMessageFinished` on Windows, so the hook and the automatic tracking are compiled out there. "I wrote today" is read from the loaded chat messages on every platform, so nothing is lost.

- `FSTREAK_CAN_HOOK_USER_CELL` — GD 2.2091 has no macOS address for `GJCompactUserCell::loadFromScore`, so the friends list flame is compiled out there.

Both come from the `GEODE_IS_*` macros. Check `build-<target>/bindings/bindings/Geode/CodegenData.json` (`bindings.win` / `bindings.android32`) before adding a call to a GD class: `missing` means the symbol is not available on that platform.

## Chat integration (GD 2.2091)

- Chat rows (`GJThreadCell`) have a name label, but the game draws the preview and the timestamp with `chatFont.fnt`. `threadNameLabel()` in `main.cpp` picks the non `chatFont` label that matches the thread name, and falls back to the topmost text when `m_userName` is empty (some DM threads keep the name on the other member).
- The row decoration is clamped by `rowRightLimit()`, which measures the real left edge of the timestamp, the unread bubble and the mute/star marks. If there is no room the flame is simply not added: never move or truncate vanilla text.
- Rows only get a flame when `pair->current > 0`.
- The open chat chip is placed on the header line. Its Y comes from the game's own top-right buttons (`m_buttonMenu`), not from the title label box, and its X is clamped before the first header button and after `m_roleBadge`/`m_titleEmote`.
- `ThreadViewPopup::onRefresh` has **no macOS address** in the bindings; do not hook it. Sync already happens on `threadMessagesFinished`, on chat open and on the poll timer.
- `GJThreadMessage` is a `CCObject`, not a `CCNode`: iterate message arrays as `CCArrayExt<cocos2d::CCObject*>`.

## Syncing

`StreakStore` owns the mirror and the scheduling, so no screen has to remember to refresh:

- `onTick` runs from `StreakTicker` (one node attached to the running scene, re-attached by `MenuLayer::init`). It polls every 6 s with a chat open and every 20 s otherwise.
- `enterChat()`/`leaveChat()` set the fast interval; `requestRefresh()` collapses bursts with a 3 s floor; `applyServerPairs()` compares before it saves or notifies, so a poll that changed nothing costs no disk write and no redraw.
- `checkSignedInAccount()` runs every 2 s and drops the previous account's token, pairs and queued events when another GD account logs in.
- Sending is detected from the messages themselves (`reportOwnMessage`), because the send callbacks are not bound on every platform.

## Layout rules of thumb

- Streak card is 300x244; vertical constants live at the top of `StreakInfoPopup.cpp`; check for overlaps before moving things.
- `FlameNode` switches between pre-tinted textures (`flame.png`, `flame-blue.png`, `flame-purple.png`, `flame-gold.png`). Multiplying the orange art with a blue tint looks muddy, so do not go back to `setColor` for stages.
- All drawing helpers are in `src/ui/Ui.hpp` (rect, label, button, dots) and take plain nodes, so they work in any layer.
- The card sets its own opacity to 0: the game already dims the screen behind a chat, and a second dim looks like an extra black background.
- Days are server (UTC) days everywhere in the UI. Do not mix in local time.

## Binary assets

`logo.png` lives in the repository root (README/about only). Everything the mod loads at runtime is in `resources/`.
