# v1.2.4

The ticker is a real widget in the main menu instead of an invisible node on the scene.

- Removed the node that Geode DevTools showed as `anonymous namespace::StreakTicker` at `(0, 0)` on the scene. It was never a visual: it only carried the one-second heartbeat that drives syncing, and it was parented to `CCScene` with z-order 999.
- The heartbeat is now a scheduler target that is not a node at all, so background syncing still runs and the scene tree stays clean.
- The visible ticker is a child of the layer that owns it: a `CCMenu` with id `friendship-streak-ticker` holding the flame and the day count, added to `MenuLayer` with z-order 5. It is placed from the profile button in the layer's own coordinates, so it keeps its spot on every resolution instead of sitting at the origin.
- The ticker is deduplicated by node id and cleared in `onExit`, so navigating between screens no longer leaves copies behind.
- The ticker follows the mirror: the number and the flame stage update as soon as the streak changes, and it stays dimmed while no streak is running.
- If the mod's flame texture fails to load the ticker falls back to a vanilla sprite instead of rendering nothing.

# v1.2.3

A flame in the main menu, and a report from every surface that should draw one.

- The streak card is reachable from the main menu again: a flame sits left of the profile button, shows the longest running streak and opens the card. With no pair yet the card opens on the code exchange, so pairing no longer requires a chat.
- If the mod's own flame sprite cannot be loaded the button falls back to a vanilla sprite instead of disappearing, and the launch toast says which one was used.
- The launch toast now also reports whether the server account exists and whether the flame sprite loaded.
- Each surface that should draw a flame shows one toast per launch with what it found: `chat row: dm true, id 1234, flame true`, `chat header: drawn at (x, y)`, `friend row: drawn at (x, y), 0 days, label true, cell 320x60`, or the reason it bailed out. A surface with no toast means its hook never ran.

# v1.2.2

Diagnostics build: tells us where the flame gets lost.

- On the main menu the mod now shows one toast per launch with the number of pairs in the local mirror and the state of the flame setting. No toast at all means the mod is not loading; a toast with `0 pair(s)` means the mod is fine and there is simply nothing to draw.
- Every decoration path writes to the Geode log: which row it was, whether the name label was found, whether there was room for the flame, whether the flame sprite loaded, and where it was placed. Failures are logged as warnings and errors.
- Nothing about the drawing itself changed from v1.2.1.

# v1.2.1

The flame is back on every friend, not only on live streaks.

- Chat list rows, the message list and the friends list draw the flame again for every DM friend, dimmed with no day count when there is no running streak. Only a live streak gets the bright flame and the number, so the mod is visible and the streak card stays one tap away even before the first pair exists.
- v1.2.0 hid those indicators entirely for friends without a running streak. Nothing was broken, but with an empty pair list the mod looked like it was not installed.
- The profile page keeps the v1.2.0 fix: only a friend's profile is decorated, and only while a streak is running.

# v1.2.0

GD 2.2091 only, and the profile page stops being touched where it should not be.

- The mod now targets GD 2.2091 (Geode 6.0.0) only. 2.2081 has no chat threads, so the whole "streak lives in the chat" design has nothing to attach to there, and the 2.2081 build was a second, untested implementation of the same features. One package, one code path.
- This also removes the overlap with the older **GD Streaks** mod (`openhands.gd-streaks`): that one is 2.2081 only, so the two can no longer load into the same game and share the main menu.
- Your own profile page is no longer decorated at all. A friendship streak belongs to a pair, your own score can never be your friend, and the page the main menu button opens is rebuilt by the game while it is still laying out. Only a friend's profile shows the "N day friendship streak" line now.
- The profile line is attached to the label the game itself drew the name into (`m_usernameLabel`) instead of scanning every label on the page, so the mod no longer reads text and font fields of labels it does not own, including the comments list.
- The profile line no longer depends on the saved pair list having been read first: the mirror is loaded before the lookup, which is what made the line appear or disappear depending on which screen you opened first.
- Deleted the 2.2081 fallbacks: the open message window flame, the message list flame, the flat message list handling and the Geode 5 timeout branch.

# v1.1.1

Cleanup pass on the chat integration, plus faster syncing.

- Removed the flame and day counter above the profile button in the main menu. The streak is opened from the chat it belongs to, and the badge no longer sits on top of your own avatar. The now useless "Show flame in main menu" setting is gone with it.
- Chat list rows: the flame is found by the same rule the game uses to name its labels, it is placed only for friends with a running streak, and it never runs into the timestamp, the preview, the unread bubble or a long name. Rows that cannot fit it stay untouched.
- Open chat: the flame now sits on the game's header line instead of under the name, measured from the header buttons and clamped before the first button.
- No more extra dark layer behind the streak card: the game already dims the chat, so the card no longer dims the screen a second time.
- Syncing no longer waits for a restart. The mod polls every 6 seconds while a chat is open and every 20 seconds otherwise, syncs on chat open, on new messages and right after a reported message, notices GD account switches while the game runs, re-registers once when a token is rejected, and collapses bursts into one request. Unchanged answers no longer rewrite save data or redraw the UI.
- Flame colour stages: orange 1-24 days, blue 25-49, purple 50-99, gold 100+, with a short flare on the change. The same colour appears in the chat list, in the chat, on profiles and in the card.
- `logo.png` moved from `resources/` to the repository root and referenced from the README and this page.
- GD 2.2091 now detects "I wrote today" from the chat messages themselves, so it works on Windows, macOS and Android instead of only where the upload callback is bound.

# v1.1.0

The streak now lives in your chats instead of in a window of its own.

- Flame and day count next to the friend name in the chat list rows on GD 2.2091, and in the title row of an open chat.
- Tapping that flame opens a small streak card: current streak, best streak, start date, last mutual day, the last seven days and what is still missing today.
- The card also holds the link flow (My code, Copy, code field, Link), so pairing happens where the streak happens.
- Removed the standalone streaks window, the friend list inside the mod, and the Pair a friend and Refresh buttons. Nothing in the mod duplicates the message screens any more.
- Removed the drawn pill backgrounds behind buttons; every button is a Geometry Dash ButtonSprite again, and the menu flame no longer sits on a dark plate.
- GD 2.2081 keeps working through the open message window and the message list, which is the same screen at that version.

# v1.0.2

- Fixed "GD reports no account signed in" showing up while the player is logged in: the pairing window gave up instead of registering, and a registration already in flight was reported as a failure. It now waits for the registration and shows the reason when it really fails.
- The pairing window and the streaks window show which GD account the game reports, so a missing login is visible instead of guessed.
- Registration and network failures now say what happened ("Cannot reach the server", "The server answered 500") instead of "sign in first".

# v1.0.1

- Fixed linking a second GD account on the same install: the cached token of the previous account was reused, so the server rejected the code as `self_pair`.
- Fixed the pairing buttons rendering as pink missing-texture squares on GD 2.2091: buttons are drawn from the mod's own texture instead of a game sheet frame.
- Link and invite errors are now plain sentences instead of raw server codes, and they no longer pop a notification over the code field.

# v1.0.0

- Added friendship streaks: a shared daily streak between two paired players, grown when both write each other on the same day.
- Added the flame badge in the main menu with the current streak next to it.
- Added the streaks window with every paired friend, their current and best streak, today's state and a sync button.
- Added the friend detail window with the 14 day activity calendar, the streak start date, the last mutual day and a flame visualisation.
- Added flames next to paired friends in the friends list, the message list and the profile page.
- Added notifications for streak growth, streak loss, "friend is waiting" reminders and milestones at 7, 14, 30, 50 and 100 days.
- Added pairing with 6 character link codes, unlinking, and a pending event queue so writing offline still counts once the connection is back.
- Added the one file reference sync server (`server/streak_server.py`) with tests.
- Added settings for the badge, the day counter, the friend flames, notifications and milestone effects.
- Supports GD 2.2081 (Geode 5.11.2) and GD 2.2091 (Geode 6.0.0) on Windows and Android.
