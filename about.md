# Friendship Streaks

<img src="logo.png" alt="Friendship Streaks" width="96" align="right">

Daily streaks, but with your friends: pair up with someone and every day you both write each other a message keeps the shared flame alive.

## The idea

TikTok style streaks work because two people are in it together. This mod brings that to Geometry Dash: the streak belongs to the pair, not to you, so it only grows on the days where both of you showed up. Miss a whole day and it starts over, while your best streak stays on record.

## In your chats

The streak lives where the messages are, so there is no second inbox to learn.

- In the chat list, the message list and the friends list, every friend gets a small flame right after their name. It is bright with a day count while the streak is running, and dimmed without a number while there is nothing to count yet.
- In an open chat, the same flame sits on the title line, lined up with the game's own header buttons, and it never runs under a long name.
- Tap it and a compact card opens: current streak, best streak, the date the streak started, the last day you both wrote, the last seven days, what is still missing today, and the code exchange that starts a new streak.

## Flame colours

The flame changes colour as the streak grows, and because both players derive it from the same day count it is always the same on both screens:

| Days | Colour |
|---|---|
| 1 to 24 | Orange |
| 25 to 49 | Blue |
| 50 to 99 | Purple |
| 100 and more | Gold |

The switch to a new stage plays a short flare, so a milestone is visible without a popup.

## Flames everywhere

Paired friends also get a flame next to their name in the friends list, in the message list and on their profile page, so you always know who is waiting for a reply.

## Keeping both accounts in step

The game stays open for hours, so the mod polls on its own: every 6 seconds while a chat is on screen and every 20 seconds in the menus. Opening a chat, sending a message, receiving one, or switching GD accounts triggers a sync immediately. Identical answers do not repaint the UI or rewrite the save file.

## Notifications

Streak growth, streak loss and "your friend is waiting" reminders, plus special messages on days 7, 14, 30, 50 and 100. All of it can be turned off.

## What it needs

A Geometry Dash account and a small sync server, since both players have to agree on the same days. `server/server.py` in the repository is a complete one file server with no dependencies. Without a connection the mod still shows the last synced state, including the calendar and the flame colours.
