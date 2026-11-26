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
