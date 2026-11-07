# Status

Where the port stands against the original PS2 game, as of November 7, 2026 (build 93). Every row says how it was checked, because a Mac emulator run does not prove anything about PSP speed or a story ending.

- **Hardware:** PSP-2000 with custom firmware, PAL disc. Nothing else has been tested.
- **Firmware:** tested on PRO-C and ARK-4. The game imports the kubridge library as a whole; ME/LME and Adrenaline were not tried.
- **Latest hardware tour:** an earlier build 93 candidate on November 7 ran the scripted tour to the end with 0 stalls, 0 crashes and 0 read errors. It showed slower interiors and 4 refused allocations on level 4; both are fixed in this build and checked in the emulator only. The final build 93 has not been toured on hardware yet.
- **How it was checked:** *hardware* = scripted tour or play on a real PSP; *emulator* = headless PPSSPP on a Mac (logic only, speed means nothing); *code* = read, never run.

## What is new in this build

- **Watchdog.** A small monitor in the game logs and fixes four situations: a hero or car with no ground under it (1.5 s for the hero, 2.5 s for a car), a body below the lowest point of the world (30 m), a body stuck inside geometry (1.5 s hero, 4 s car), and a placement that finds no ground. Each case writes one `WATCHDOG` line to the black box. The thresholds are the port's own rule; the original game has no fall timer because it never lets a walking character fall through. Checked in the emulator: placement without ground, 53 to 58 violations out of 60 on every level with the old code, 0 with the new; each watchdog kind is broken on purpose by a flag and repaired. Hardware: an earlier build 93 tour (November 7) had 0 falls, 0 below-world, 0 stuck.
- **Placement by scene.** The hero is put into the world only when the ground and the city block under the point are in memory (waits up to 6 s, not drawn, not moving), then released onto the nearest ground. The screen transition (iris) stays closed until the hero stands on the ground, so the player never sees an empty street. Traffic pedestrians are not spawned over a block that is not loaded. Mission characters in the city are not drawn until the block under them is loaded. Checked in the emulator with the blocks unloaded on purpose; an earlier build 93 hardware tour (November 7) showed 49 such waits, 0.3 to 0.4 s each, no falls.
- **Memory ladder.** When a load would not fit, the game now frees memory in steps and re-measures after each one: read-ahead models, far blocks behind the player, a shorter draw distance for a while, all blocks out of view while the world stands still (pause, menu, booth, shop), and as the last step a simpler menu. Blocks and the draw distance come back by themselves. A 448 KB block is kept free for the Mission Select pack. Checked in the emulator with a memory-starved scenario and a red control (ladder switched off). **Not checked on hardware: the cost of the 448 KB block and of the shorter draw distance.**
- **Honest data screens.** The builder writes a stamp (`set.txt`) next to the game; if it comes from another build the game says "Game files are from another build. Run the builder again." The game also checks at start that every file of every level is there; a missing level shows "Level N files are missing" instead of quietly returning to the menu after a level. Checked in the emulator by deleting a level file and by swapping the stamp.
- **Police and cars.** The pitch of a wheelie is limited to 15 degrees (an ATV and one other car flipped over before); Hit & Run limbo checked live in the emulator: meter at zero, police stop after 5.9 s and are removed out of view.

## What works, what is partial, what is missing

| Area | Status | Checked on |
|---|---|---|
| Levels 1 to 7 load and run end to end in the tour | works | hardware |
| Player character per level (Bart, Lisa, Marge, Apu, Homer) | works | hardware |
| 16:9 3D, menus, HUD and loading screens rebuilt from the original files | works | hardware |
| Pause, options, radar, mission select | works, one reported glitch not reproduced | hardware |
| Save slots, SAVE, RESUME, level unlocking | works | hardware |
| Sound and music streaming (Media Engine mixer) | works: 0 late buffers in 1014 s | hardware |
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

## Hardware numbers (PSP-2000, tour build TEST62; not refreshed for build 93)

| Measure | Result |
|---|---|
| Tour finished to the end | yes; 0 faults, 0 stalls, 0 memory failures, 0 read errors |
| Frame rate, whole tour | median 28 fps (capped at 30) |
| Seconds under 20 fps | 6 to 9 percent, mostly in city driving on levels 3 to 6 |
| Frame time p95 by scene | 23 to 43 ms; driving scenes on levels 3 to 7 sit at 33.5 to 36.8 ms (30 fps is 33.3 ms) |
| Worst single frames | 212 to 263 ms when leaving the house at the start of levels 4 and 7 (the world is set up in that frame) |
| Streaming | 2162 cells loaded, worst request-to-ready delay 877 ms |
| Memory | lowest free heap 3.97 MB, lowest largest block 1.00 MB |
| Audio | 1014 s, 0 late buffers |
| Level load | 5.3 to 6.9 s per level in the tour |

## Known problems

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

Build 93 played by hand to the end; the memory ladder on hardware (cost of the kept block, shorter draw distance, the long five-round play); the repair paths of the watchdog on hardware (nothing fell in the tour); a refused allocation during a level change on hardware; PSP-3000 and Go; NTSC discs; ME/LME and Adrenaline firmware; sleep and wake; a full memory card; a corrupt save on hardware; a slow memory card; the ending to the credits by hand; every cutscene on hardware; the bonus game on hardware. Building on Linux and WSL: the paths exist and came from outside contributors, we have not run them ourselves.

## If the game stops

Send three things: `SHARVIEW/shar_report.txt`, `SHARVIEW/blackbox.txt`, and the map code from the bottom left of the pause screen (for example L1 K9-7). The first two are written by the game itself: `shar_report.txt` when the game detects a freeze, shows an error screen or exits cleanly (each time it overwrites the previous one, so copy it before the next run); after a hard crash it may be missing, and then `blackbox.txt` and the code are what we have. The code tells where in the city it happened. Reports from other PSP models and NTSC discs help the most.
