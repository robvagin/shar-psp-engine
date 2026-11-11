# Status

Where the port stands against the original PS2 game, as of November 10, 2026 (build 104; the latest hardware tour is build 104 of November 11). Every row says how it was checked, because a Mac emulator run does not prove anything about PSP speed or a story ending.

- **Hardware:** PSP-2000 with custom firmware, PAL disc. Nothing else has been tested.
- **Firmware:** tested on PRO-C and ARK-4. The game imports the kubridge library as a whole; ME/LME and Adrenaline were not tried.
- **Latest hardware tour:** build 104 on November 11 ran the scripted tour of 113 scenes to the end with 0 stalls, 0 crashes, 0 read errors and 0 memory failures (no city block failed to fit).
- **How it was checked:** *hardware* = scripted tour or play on a real PSP; *emulator* = headless PPSSPP on a Mac (logic only, speed means nothing); *code* = read, never run.

## What is new in this build

The whole list from build 93 to 104 is in the [changelog](../CHANGELOG.md). In build 104:

- **Mission intro cameras use the original fly-ins from the disc again,** shortened to the last 40 m before the hero and stopped in front of walls, so the camera no longer passes through buildings.
- **You can get back into your car after it has exploded** (the wreck drives slowly), as in the original.
- **Glow at entrances and phone booths** (a pulse, drawn by the engine) after frames of the PS2 original.
- **No black screen after a failed mission, race bet paid at the card, speech bubble slide-in.** Checked in the emulator and by the gates in `tests/`.

## What works, what is partial, what is missing

| Area | Status | Checked on |
|---|---|---|
| Levels 1 to 7 load and run end to end in the tour | works | hardware |
| Player character per level (Bart, Lisa, Marge, Apu, Homer) | works | hardware |
| 16:9 3D, menus, HUD and loading screens rebuilt from the original files | works | hardware |
| Pause, options, radar, mission select | works, one reported glitch not reproduced | hardware |
| Save slots, SAVE, RESUME, level unlocking | works | hardware |
| Sound and music streaming (Media Engine mixer) | works: 0 late buffers in 3193 s | hardware |
| Music on every level | plays on all 7 in the tour log; listened to on level 1 only | hardware (log), ear on L1 |
| Story missions, level 1 | works when played by hand; bots finish 7 of 13 runs, the rest hit their time limit | hardware (by hand), bots |
| Story missions, levels 2 to 7 | partial: starts verified, full playthrough not proven | emulator, bots |
| Level 7 UFO tractor beam (`destroyboss` objective) | works since build 65: the UFO and its beam are in the world, missions 3, 5, 6 and 7 complete; not yet played on hardware | emulator |
| Street races | partial: start verified on L2 to L7, rival pace differs from PS2 | emulator |
| Bonus missions | partial | emulator, bots |
| Wager race | partial: start verified, win not proven | emulator |
| Bonus game (7 tracks) | implemented, not tested on hardware | emulator |
| Cutscenes (16 videos) | all 16 open and play on a PSP-2000 in the automated hardware tour (build 69), and in the emulator | emulator, hardware |
| Ending, save after the last mission, credits | the automated tour reaches the credits and returns to the menu on a PSP-2000 (build 69) and in the emulator; not yet played to the end by hand | emulator, hardware |
| Poles and lamp posts when hit | works: fall like the PS2 (whole, pivoting at the base), fall time within 0.3 s of PS2 recordings at four speeds on level 1; other levels and large trees not recorded on the PS2 | emulator vs PS2 recording |
| Player-character cars, damage, smoke, wheels | partial: tuned against PS2 recordings for two vehicles only; tires are black on all cars (emulator frames) | hardware (level 1), emulator |
| Police and Hit & Run | partial: distance to a standing player (27 m) and chase meter timing follow PS2 recordings (emulator); hardware not rechecked | emulator |
| Traffic and pedestrians | partial: density and variety not compared with PS2 | hardware, emulator |
| On foot: run, jump, kick, enter and exit cars, interiors | works on level 1; interiors on levels 2 to 7 not re-verified | hardware (L1), emulator |
| Collectibles: cards, wasps, gags | partial: all 7 levels have data, level 1 verified | hardware (L1), emulator |
| Phone booth, shop, wardrobe | works: opens every time; any car of any level can be called (emulator matrix of 42 cars on levels 1 to 7); level 1 also on hardware | hardware (L1), emulator |
| Dialogue voice | level 1 verified; levels 2 to 7 not listened to | ear on L1 |
| Subtitles | missing: the disc data has none | n/a |
| Repeat a finished race | missing (audit, October 16) | code |
| Cinema mission on level 3 | missing (audit, October 16) | code |
| Reset Car | removed on purpose | author decision |
| Cartoon mode (ink outlines, flat fills, the show's palette) | works; on by default; ink only on the hero, pedestrians, cars, booth and props; Settings: Textures, Curbs, line width, eye lines, ink, texture detail, old TV | hardware, emulator |
| Watchdog (fall, below world, stuck, no ground at placement) | works in the emulator; 0 events in the earlier build 93 hardware tour, so the repair path has not fired on hardware | emulator, hardware tour |
| Placement by scene (hero and mission characters wait for the ground and the city block) | works in the emulator; 49 waits of 0.3 to 0.4 s in the earlier build 93 hardware tour | emulator, hardware tour |
| Memory ladder | works in the emulator with a starved memory scenario; fired 3 times by itself in the hardware tour; cost on hardware not measured | emulator, hardware tour (partial) |
| Level loading under low memory | the loading screen and the sound bank survive a refused allocation (retry after freeing the old level), proven in the emulator with a memory-starved scenario; the build 78 hardware tour had no refused allocation, sound on all 14 level loads and the loading screen on every change | emulator, hardware tour |
| Mission Select, loading and pop-in | a mission change inside a level restarts the music and always shows the loading screen; the loading screen waits for the mission characters, nearby bonus characters and mission cars, so nothing appears after it opens; the mission car keeps its own spot unless you stand close on the same ground or are more than 60 m away. Old code fails the same check: music 0 of 2, car on the ledge, 6 late loads | emulator (green and red builds) |
| Settings defaults | a fresh save matches the author's PSP menus (see README) | emulator menu frames |
| Jumps, ramps and fences | level 5 ramp rails and fire escape, fence links on levels 2 to 7 follow PS2 recordings; not played on hardware | emulator vs PS2 recording |
| Other PSP models, NTSC discs | PSP-3000 untested; PSP-1000 has half the memory, untested | n/a |

## Hardware numbers (PSP-2000, tour build 104, 113 scenes, November 11)

This tour ran without the texture-table dumping switch (the build 101 tour had it on, which adds about 0.6 s to each level load). Definitions are the same as for build 101: frame-time and dropped-frame figures are per scene (the 95th percentile of a scene's frames; dropped = missed the 33.3 ms deadline), the median second is over every second spent in play, and shared scenes are the ones every compared tour ran.

| Measure | Result |
|---|---|
| Tour finished to the end | yes; 0 faults, 0 stalls, 0 read errors, 0 memory failures, no city block that did not fit |
| Frame rate, whole tour | median second 26 fps (capped at 30; build 101: 27); 28 percent of seconds at 29 fps or more (build 101: 25) |
| Frame time p95 by scene | median 34.5 ms (build 101: 33.5; 30 fps is 33.3 ms); interiors 17.5 to 24.5 ms; the slowest 10 percent of scenes 40.0 ms or more; the worst scene 52.8 ms (a mission scene on level 7) |
| Dropped frames | median 19 percent per scene, 21 percent of all frames (build 101: 10 and 19, so this is worse); levels 6 and 4 are the weakest (32 and 26 percent of frames in their scenes; the tour has 27 scenes on level 4 against 8 in build 101) |
| Streaming | 3313 cells loaded, worst request-to-ready delay 1.9 s (build 101: 6.5 s); frames with a visible, not yet loaded block: 144 over the whole tour (build 101: 234), but 21 inside the scenes against 13 in build 101 |
| Memory | lowest free heap 3.5 MB, lowest largest block 3.4 MB (build 101: 3.00 and 2.72) |
| Audio | 3191 s, 0 late buffers; the second core handed 2 buffers back to the main one |
| Level load | 2.9 to 3.7 s per level, 3.3 s on average (build 101: 5.0 s with the dumping switch on) |

Compared on the 81 scenes that the build 93, 101 and 104 tours share: p95 36.5 to 34.8 ms (build 101: 34.8), dropped frames 36 to 19 percent (median over scenes; build 101: 14), lowest free memory 1.1 to 3.5 MB, frames with a visible, not yet loaded block 77 to 15 (build 101: 10).

## Known problems

- Phone booths are off during missions that give you a car (as in the original); on levels 3 and 6 this holds from the start of the level.
- City blocks that have not loaded yet can still show as holes for a moment, for example right after a teleport (see the streaming row above).
- A tree or a pole that you kick can fall over as one piece and stay stuck instead of breaking apart.
- The VENT GAS sign at the power plant can hide the hero from the camera.
- Simplified far city blocks exist in the code but are switched off by default and the builder no longer makes their file, after a playtest of build 102 on the PSP reported map textures flickering (the cause was not confirmed).
- Driving in the city on levels 3 to 7 does not hold 30 fps and dips under 20 fps in places. The cost is spread evenly over the whole frame.
- Far-distance flicker is not solved. The fix for it hid the horizon on one level, so it is switched off in the final build.
- A "go home" icon was reported once after a level change; it could not be reproduced. Every level change closes open menus and clears the previous mission icon, and the game logs the icon state for the next report.
- Memory fragmentation over many level changes (the 11th level load in the build 86 tour could not fit the level 1 collectibles) is addressed by the memory ladder in this build, but that is **not verified on hardware**: the long play of five rounds from level 1 to level 7 has not been run on the PSP.
- Not covered by the memory ladder: the ring buffer that the background thread fills at level start, vehicle and character models loaded with a level, and sounds. A long chase with many police cars through all missions has not been checked.
- Level 4 is the tightest level for memory. In the emulator, with memory squeezed to the PSP figure, a long drive still had refused city block loads. Refused blocks are retried and load later; the game does not stop. Not verified on hardware.
- Changing CURBS applies on the next level load: rebuilding the flat fills while playing shifted the sidewalk colour on level 1 (cause not found).
- A cell can take 2 to 5 s to load right after a teleport in the tour or a mission transition; the placement wait above covers the hero, other objects are not covered. Cause not found.
- The rocket vehicle can flip after hitting a wall at about 39 m/s (found in the emulator, open).

## Not verified

Build 104 played by hand to the end; the memory ladder on hardware (cost of the kept block, shorter draw distance, the long five-round play); the repair paths of the watchdog on hardware (nothing fell in the tour); a refused allocation during a level change on hardware; PSP-3000 and Go; NTSC discs; ME/LME and Adrenaline firmware; sleep and wake; a full memory card; a corrupt save on hardware; a slow memory card; the ending to the credits by hand; every cutscene on hardware; the bonus game on hardware. Building on Linux and WSL: the paths exist and came from outside contributors, we have not run them ourselves.

## If the game stops

Send three things: `SHARVIEW/shar_report.txt`, `SHARVIEW/blackbox.txt`, and the map code from the bottom left of the pause screen (for example L1 K9-7). The first two are written by the game itself: `shar_report.txt` when the game detects a freeze, shows an error screen or exits cleanly (each time it overwrites the previous one, so copy it before the next run); after a hard crash it may be missing, and then `blackbox.txt` and the code are what we have. The code tells where in the city it happened. Reports from other PSP models and NTSC discs help the most.
