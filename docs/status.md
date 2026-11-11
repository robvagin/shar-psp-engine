# Status

Where the port stands against the original PS2 game, as of November 11, 2026 (build 106 and its hardware tour of November 11). Every row says how it was checked, because a Mac emulator run does not prove anything about PSP speed or a story ending.

- **Hardware:** PSP-2000 with custom firmware, PAL disc. Nothing else has been tested.
- **Firmware:** tested on PRO-C and ARK-4. The game imports the kubridge library as a whole; ME/LME and Adrenaline were not tried.
- **Latest hardware tour:** build 106 on November 11 ran the scripted tour of 114 scenes to the end with 0 stalls, 0 crashes, 0 read errors and 0 memory failures (no city block failed to fit). An earlier tour of the same day on build 105 (it differs from 106 only in the videos started from the View Movies menu) is in the numbers below.
- **How it was checked:** *hardware* = scripted tour or play on a real PSP; *emulator* = headless PPSSPP on a Mac (logic only, speed means nothing); *code* = read, never run.

## What is new in this build

The whole list from build 93 to 106 is in the [changelog](../CHANGELOG.md). In build 106 (changes since build 104):

- **A car slides along a wall or fence again** at a shallow angle instead of stopping, as in builds 92 to 96, and stays on four wheels in a hard hit. At 45 degrees and more it stops at the wall. The PS2 recordings lose almost all speed at 26 to 35 degrees, so this is further from the original than the sticking of builds 99 to 104 was.
- **Traffic cars no longer swerve out of your way** (the swerve, in since build 90, is switched off; no PS2 recording of such a swerve exists).
- **A hit no longer moves the car in a jump on a long frame** (the shift is capped at 0.08 m per step).
- **Videos started from the View Movies menu now play inside the TV frame like the others (they were drawn over the list).**

In build 104:

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

## Hardware numbers (PSP-2000, tour on build 106, 114 scenes, November 11)

Like the build 104 tour, it ran without the texture-table dumping switch. Definitions are the same as for builds 101 and 104: frame-time and dropped-frame figures are per scene (the 95th percentile of a scene's frames; dropped = missed the 33.3 ms deadline), the median second is over every second spent in play, and shared scenes are the ones every compared tour ran. The build 105 tour (an earlier run of the same day, same code except for the video fix) is given for comparison. The tours have one scene more than the build 104 tour (`F_BONUS_T4`, the level 4 bonus game). Each tour was run once.

| Measure | Build 106 tour | Build 104 tour | Build 105 tour |
|---|---|---|---|
| Tour finished to the end | yes; 0 faults, 0 stalls, 0 read errors, 0 memory failures, no city block that did not fit | same | same |
| Frame rate, whole tour | median second 24.5 fps (capped at 30); 22 percent of seconds at 29 fps or more | 26.2 fps; 28 percent | 23.6 fps; 21 percent |
| Frame time p95 by scene | median 34.9 ms (30 fps is 33.3 ms); interiors 17.5 to 24.3 ms; 12 of 114 scenes at 40.0 ms or more; the worst scene 53.3 ms (a mission scene on level 7) | 34.5 ms; 17.5 to 24.5; 11 of 113; 52.8 ms (the same scene) | 35.5 ms; 17.5 to 24.3; 14 of 114; 54.3 ms |
| Dropped frames | median 20 percent per scene, 26 percent of all frames; levels 6 and 5 are the weakest (32 and 31 percent of frames in their scenes) | 19 and 21 percent; levels 6 and 4 (32 and 26 percent) | 21 and 28 percent; levels 2, 6 and 5 (34, 33 and 27 percent) |
| Streaming | 3257 cells loaded, worst request-to-ready delay 2.1 s; frames with a visible, not yet loaded block: 143 over the whole tour (27 inside the scenes, 11 of them in the new bonus scene) | 3313 cells, 1.9 s; 144 (21 inside the scenes) | 3076 cells, 1.8 s; 147 (29, 10 in the bonus scene) |
| Memory | lowest free heap 3.3 MB, lowest largest block 3.2 MB | 3.5 MB and 3.4 MB | 3.3 MB and 3.2 MB |
| Audio | 3173 s, 0 late buffers; the second core handed 2 buffers back to the main one | 3191 s, 0 late; 2 buffers | 3238 s, 0 late; 3 buffers |
| Level load | 2.9 to 3.7 s per level, 3.2 s on average | 2.9 to 3.7 s, 3.3 s | 2.9 to 3.7 s, 3.3 s |

On the 113 scenes that the build 104 and 106 tours both ran, build 106 is slower on most frame measures: median second 24.6 fps against 26.0, p95 34.8 ms against 34.5, dropped frames 19 against 19 percent per scene but 26 against 21 percent of all frames. It is a little better than build 105 (24.0 fps, p95 35.5 ms, 27 percent of all frames on the same scenes). Level 2, which dropped 34 percent of its frames in the build 105 tour (build 104: 20), is back to 17 percent; level 5 went from 19 (build 104) and 27 (build 105) to 31 percent. We have not found the cause of the slowdown against build 104.

Compared on the 81 scenes that the build 93, 98, 101 and 106 tours share: p95 36.5 to 35.2 ms (build 104: 34.8; build 101: 34.8), dropped frames 36 to 20 percent (median over scenes; build 104: 19; build 101: 14), lowest free memory 1.1 to 3.3 MB, frames with a visible, not yet loaded block 77 to 12 (build 104: 15; build 101: 10).

## Known problems

- Phone booths are off during missions that give you a car (as in the original); on levels 3 and 6 this holds from the start of the level.
- A car that hits a wall or fence at 45 degrees or more still stops at the wall; it slides only at angles up to about 30 degrees. The PS2 recordings lose almost all speed at 26 to 35 degrees, so the slide is stronger than in the original.
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

Build 106 played by hand to the end; trees and bushes hit at a slide; getting into a car in a traffic jam; the memory ladder on hardware (cost of the kept block, shorter draw distance, the long five-round play); the repair paths of the watchdog on hardware (nothing fell in the tour); a refused allocation during a level change on hardware; PSP-3000 and Go; NTSC discs; ME/LME and Adrenaline firmware; sleep and wake; a full memory card; a corrupt save on hardware; a slow memory card; the ending to the credits by hand; every cutscene on hardware; the bonus game on hardware. Building on Linux and WSL: the paths exist and came from outside contributors, we have not run them ourselves.

## If the game stops

Send three things: `SHARVIEW/shar_report.txt`, `SHARVIEW/blackbox.txt`, and the map code from the bottom left of the pause screen (for example L1 K9-7). The first two are written by the game itself: `shar_report.txt` when the game detects a freeze, shows an error screen or exits cleanly (each time it overwrites the previous one, so copy it before the next run); after a hard crash it may be missing, and then `blackbox.txt` and the code are what we have. The code tells where in the city it happened. Reports from other PSP models and NTSC discs help the most.
