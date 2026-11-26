# Friendship Streaks

A Geode mod for Geometry Dash that adds TikTok style friendship streaks: pair up with a friend, and every day you both write each other a message keeps the shared flame alive.

The streak is shared, so it only grows when **both** players show up. That is the whole point: the flame is a small daily reason to say hi.

## What you get

- **Flame in your chats.** GD 2.2091: the flame and its day count sit next to the friend name in the chat list rows and in the title row of an open chat. GD 2.2081: the flame sits in the open message window and in the message list.
- **Streak card.** Tapping the flame opens a small card with the current streak, the best streak, the start date, the last mutual day, the last seven days as dots, and what is still missing today. It also holds the link flow, so there is no separate screen to learn.
- **Flame badge in the main menu** with the best active streak, next to your profile button. Tapping it opens the card of that friend.
- **Flames next to friends** in the friends list and on the profile page.
- **Notifications** when a streak grows, when it breaks, when a friend is waiting for your reply, and special messages on days 7, 14, 30, 50 and 100.
- **Settings** to hide the badge, hide the day counter, hide the flames in chats and friend lists, and turn notifications or milestone effects off.

Nothing about vanilla gameplay changes: the mod only decorates existing screens and hooks the "message sent" event.

## Requirements

| | |
|---|---|
| Geometry Dash | 2.2081 (Geode 5.11.2) or 2.2091 (Geode 6.0.0) |
| Platform | Windows, macOS, Android 32/64 (iOS is untested and not built) |
| Known limits on GD 2.2091 | Windows: the game does not expose the message callback, so the streak card shows **I wrote today** and you confirm the send yourself. macOS: the friends list row callback is unbound, so the flame in the friends list is not drawn (chats and the profile still show it) |
| Account | a real Geometry Dash account, that is how the two players identify each other |
| Server | one small sync server, see below |

## Install

1. Install [Geode](https://geode-sdk.org/install/) for your GD version.
2. Drop the matching `.geode` file from `dist/` into your Geode mods folder, or use the in-game mod browser once the mod is indexed.
3. Open the mod settings and set **Server URL** to your sync server.
4. In game: open a chat with the friend, tap the dimmed flame next to their name, then **My code**.

Pick the file that matches your GD version. This matters: a build made for 2.2081 will not load on 2.2091 and Geode will tell you the mod is for the wrong version.

| Your game | Geode | Install this |
|---|---|---|
| GD 2.2081 | 5.11.2 | `friendship-streaks-v1.1.0-gd2.2081-windows-macos-android.geode` |
| GD 2.2091 | 6.0.0 | `friendship-streaks-v1.1.0-gd2.2091-windows-macos-android.geode` |

Each of those single files contains the Windows, macOS and both Android binaries, and Geode loads the right one for your platform. If Geode reports the wrong GD version, the other file is the one you want: the version is baked into the package metadata and cannot be switched at install time.

## Pairing with a friend

Both players need a GD account and the same server URL.

1. Open a chat with the friend you want to pair with and tap the flame next to their name.
2. Press **My code**: the code is shown and copied to your clipboard.
3. Send the code to your friend however you like (a GD message, Discord, whatever).
4. Your friend opens the same chat, pastes the code into the field and presses **Link**.
5. The pair is created and today starts counting.

Only one code per account is active at a time, a code cannot be used to link to yourself, and linking the same two players twice just returns the existing pair.

## How the streak is counted

- A day counts when **both** players have written to each other on that day.
- The current streak is the number of consecutive counted days ending today, or ending yesterday if today is still pending. Writing today never loses a streak, it only keeps it.
- Missing one or more whole days resets the current streak to 0. The best streak is stored forever.
- Days are UTC days on the server, so both players see the same day no matter their timezone. The client shows the server's day, not the local one, which is what makes the streak honest across timezones.
- Writing to a friend is detected when a GD message to that friend finishes uploading. Repeated messages on the same day do not count twice.

## Running the sync server

`server/server.py` is a complete reference implementation in one file. It uses only the Python standard library and SQLite, so there is nothing to install and nothing to pay for.

```sh
python3 server/server.py --host 0.0.0.0 --port 8080 --db /var/lib/friendship-streaks/streaks.sqlite3
```

Then put `http://your-host:8080` (or the HTTPS URL of a reverse proxy) into the mod settings.

Behind nginx or Caddy, terminate TLS there and proxy everything to the port above. The server keeps no personal data beyond the GD account ID, the GD username, a random token, and one row per player per day they were active.

### API

| Method | Path | Purpose |
|---|---|---|
| `POST` | `/v1/register` | Exchange a GD account ID and username for a token |
| `GET` | `/v1/pairs` | List your pairs with the full streak state |
| `POST` | `/v1/pairs/invite` | Create (or reuse) your link code |
| `POST` | `/v1/pairs/redeem` | Link to the player that owns the code |
| `POST` | `/v1/pairs/{id}/events` | Report "I wrote to my friend today" |
| `DELETE` | `/v1/pairs/{id}` | Unlink |
| `GET` | `/v1/health` | Liveness probe |

Authentication is `Authorization: Bearer <token>` with the token returned by `/v1/register`.

### Offline behaviour

The mod keeps a local mirror of the last synced state, so the streak window, the calendar, the badge and the flames all still render without a connection. Events that could not be delivered are queued in save data and sent on the next successful sync. Only new mutual days need the server, since both players have to agree on them.

## Settings

| Setting | Default | Effect |
|---|---|---|
| Server URL | empty | Sync server base URL |
| Show flame in main menu | on | Show or hide the badge |
| Show day counter | on | Show or hide the number next to the flame |
| Show flames next to friends | on | Show or hide the flames in chats, message lists and profiles |
| Streak notifications | on | Turn streak notifications off |
| Milestone effects | on | Turn the milestone flame burst off |

## Building

```sh
export GEODE_SDK=$HOME/geode-sdk          # Geode 5.11.2 for GD 2.2081
export ANDROID_NDK=/path/to/android-ndk-r29

cmake -B build-android64 -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-23
cmake --build build-android64
```

Use `-DANDROID_ABI=armeabi-v7a` for the 32 bit build. For GD 2.2091, point `GEODE_SDK` at a Geode 6.0.0 checkout with its Android binaries installed and set `geode`/`gd` in `mod.json` to `6.0.0`/`2.2091`. `scripts/build_all.sh` does both variants in one go.

Windows and macOS builds run in CI: `.github/workflows/multi-platform.yml` builds GD 2.2081, `.github/workflows/multi-platform-2091.yml` retargets `mod.json` to Geode 6.0.0 and builds GD 2.2091. Each produces its own combined `.geode`.

## Tests

```sh
g++ -std=c++23 -O2 tests/core_test.cpp -I src -o /tmp/core_test && /tmp/core_test
cd server && python3 -m unittest test_server -v
```

`src/core/FriendStreakCore.hpp` is deliberately free of Geode headers so the streak rules, the day maths and the milestone messages can be tested on the host. The server tests cover the full streak lifecycle, the reset rules, error handling, and the exact JSON shape the mod parses.

## Layout

```
src/core/       streak rules, day maths, milestones (no Geode)
src/StreakApi   HTTP layer
src/StreakStore local mirror, pending event queue, listeners
src/Notify      notifications and milestone effect
src/ui/         menu badge, streak card, flame widget, drawing helpers
server/         one file sync server + tests
tests/          host tests for the core
```

## Notes and limitations

- Streaks are shared state, so they need a server. There is no way around that: two devices have to agree on the same days.
- The mod never reads your GD password or anything beyond the account ID and username you already have, and it never touches gameplay code.
- GD 2.2091 renamed the friends list row class, so the friend list decoration is compiled against `GJUserCell` on 2.2081 and `GJCompactUserCell` on 2.2091. Everything else uses stable APIs.
