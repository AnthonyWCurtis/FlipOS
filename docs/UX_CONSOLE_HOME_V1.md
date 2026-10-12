# FlipOS Console UX v1

This branch is the first implementation pass toward a console-first FlipOS UI.

## Home

The Home screen is now a dashboard instead of a three-item launcher. It keeps
the primary navigation on the left and uses the larger right-hand surface for
the most useful "pick up and play" context:

- last played game;
- resume availability;
- per-game total play time;
- launch count;
- last session duration;
- last played time;
- library-wide play time and launch total.

The first pass deliberately keeps the navigation simple and controller-native:
Library, GameSwitcher, Play Stats, and Settings.

## Play Stats

Play Stats are stored locally under the FlipOS state directory. There is no
network telemetry or account dependency.

Per-game aggregates currently include:

- total active play time;
- launch count;
- last session duration;
- longest session duration;
- first played;
- last played.

The Play Stats screen also shows:

- all-games total play time;
- total launches;
- number of games played;
- longest session;
- average session length;
- a Most Played ranking.

Pressing Y on a game in Library or GameSwitcher opens that game's detail page.

## Timing semantics

"Play time" means actual emulator child runtime. FlipOS uses CLOCK_MONOTONIC for
duration measurement so wall-clock/NTP changes cannot change a session length.

Time spent in the Play Options menu is not counted.

When the lid is closed during a game, the emulator segment ends before deep
sleep. The automatic lid-open continuation is joined to the same user play
session, but the sleep interval itself is not added to play time.

An automatic lid-open continuation does not increment the launch counter.
A deliberate start or GameSwitcher resume does.

## Existing installations

There is no reliable historical play-time source in older FlipOS builds, so
the new statistics begin accumulating after this feature is installed rather
than inventing values for older sessions.

## Scope

This is intentionally a focused UX foundation, not a full visual rewrite in one
patch. Once the Home/Stats direction has been proven on the real 640x480 Miyoo
Flip display, the same design language can be extended to Library,
GameSwitcher, Settings, Updates, battery/power, and the in-game Quick Menu.
