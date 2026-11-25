# Friendship Streaks

Daily streaks, but with your friends: pair up with someone and every day you both write each other a message keeps the shared flame alive.

## The idea

TikTok style streaks work because two people are in it together. This mod brings that to Geometry Dash: the streak belongs to the pair, not to you, so it only grows on the days where both of you showed up. Miss a whole day and it starts over, while your best streak stays on record.

## In the main menu

A small flame sits above your profile button with your best active streak next to it. Click it to open the streaks window.

## Streaks window

- Every paired friend with their current streak, best streak and today's state
- A link button that gives you a 6 character code to hand to a friend
- A sync button, so you can pull the latest state before you write

## Friend detail

- Current streak and best streak
- The date your current streak started
- The date of the last day you both wrote
- A 14 day calendar, with a golden ring on today and a clear difference between "both wrote", "only you wrote" and "only they wrote"
- A flame visualisation of the current run, so a long streak actually looks long

## Flames everywhere

Paired friends get a flame next to their name in the friends list, in the message list, and on their profile page, so you always know who is waiting for a reply.

## Notifications

Streak growth, streak loss and "your friend is waiting" reminders, plus special messages on days 7, 14, 30, 50 and 100. All of it can be turned off.

## What it needs

A Geometry Dash account and a small sync server, since both players have to agree on the same days. `server/server.py` in the repository is a complete one file server with no dependencies. Without a connection the mod still shows the last synced state, including the calendar and the badge.
