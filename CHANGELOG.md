# Changelog

Builds 93 to 106, November 7 to 11, 2026. Newest first.

**How to read the numbers.** Every number below is from the scripted tour on a real PSP-2000 unless it says *emulator*. The frame time is the 95th percentile of a scene's frames (p95), then the median over scenes; "dropped" is the share of frames that missed the 30 fps deadline (33.3 ms), also the median over scenes. The tour grew from 55 scenes (build 86) to 113 (build 101), so tours are only compared on the scenes they share. A tour that ends early or runs with debugging switches on says so.

## Hardware tours, side by side

| Build | Date | Scenes | p95, ms | Dropped | Lowest free memory | Memory failures |
|---|---|---|---|---|---|---|
| 93 (final) | Nov 7 | 103 | 36.5 | 31% | 1.1 MB | 0 |
| 94 | Nov 8 | 103 | 37.0 | 37% | 0.25 MB | 5 |
| 95 (candidate) | Nov 8 | 106 | 32.6 | 7% | 0.64 MB | 106 |
| 97 | Nov 8 | 107 | 36.5 | 23% | 2.1 MB | 6 |
| 98 (tour stopped early) | Nov 9 | 85 | 34.3 | 12% | 3.0 MB | 0 |
| 100 | Nov 9 | 109 | 34.0 | 10% | 3.0 MB | 0 |
| 101 | Nov 9 | 113 | 33.5 | 10% | 3.0 MB | 0 |
| 104 | Nov 11 | 113 | 34.5 | 19% | 3.5 MB | 0 |
| 105 (earlier tour) | Nov 11 | 114 | 35.5 | 21% | 3.3 MB | 0 |
| 106 | Nov 11 | 114 | 34.9 | 20% | 3.3 MB | 0 |

Because the scene lists differ, the fair comparison is the 81 scenes that builds 93, 94, 95, 97, 98, 100, 101, 104, 105 and 106 all ran:

| Build | p95, ms | Dropped (median over scenes) | Dropped (all frames) | Lowest free memory |
|---|---|---|---|---|
| 93 | 36.5 | 36% | 35% | 1.1 MB |
| 94 | 37.3 | 38% | 36% | 0.25 MB |
| 95 | 33.8 | 9% | 18% | 0.64 MB |
| 97 | 37.0 | 26% | 32% | 2.1 MB |
| 98 | 34.3 | 12% | 20% | 3.0 MB |
| 100 | 35.0 | 18% | 23% | 3.0 MB |
| 101 | 34.8 | 14% | 21% | 3.0 MB |
| 104 | 34.8 | 19% | 23% | 3.5 MB |
| 105 (earlier tour) | 35.8 | 21% | 26% | 3.3 MB |
| 106 | 35.2 | 20% | 24% | 3.3 MB |

Build 95 was the fastest and also the one that ran out of memory 106 times in the middle of the tour: its memory zones were too greedy. Builds 97 and 98 fixed that, at the price of a slower frame than build 95, which is still not recovered.

## Build 106 (November 11)

Build 106 is build 105 plus a fix for the videos started from the View Movies menu. Hardware tour on build 106 itself (November 11): 114 scenes (build 104: 113; the new one is the level 4 bonus game, `F_BONUS_T4`), ran to the end, no stalls, 0 read errors, 0 memory failures, no city block that did not fit, 0 late audio buffers in 53 minutes. Level loads took 3.2 s on average (build 104: 3.3 s). The worst wait for a city block was 2.1 s (build 104: 1.9 s, build 105: 1.8 s). An earlier tour on build 105 (same code except for the video fix) is in the tables above.

It is still slower than build 104 on most frame measures, on the 113 scenes both tours ran: median second 24.6 fps against 26.0, frame time p95 34.8 ms against 34.5, dropped frames 19 against 19 percent of scenes and 26 against 21 percent of all frames, seconds at 29 fps or more 23 against 25 percent (on all scenes of each tour: 24.5 against 26.2 fps and 22 against 28 percent). It is a little better than the build 105 tour (23.6 fps, p95 35.5 ms, 28 percent of all frames dropped). On the 81 scenes shared with builds 93 to 101: p95 35.2 ms (build 104: 34.8, build 101: 34.8), dropped frames 20 percent (build 104: 19, build 101: 14), lowest free memory 3.3 MB, frames with a visible, not yet loaded block 12 (build 104: 15, build 101: 10). Levels 6 and 5 are the weakest (32 and 31 percent of dropped frames in their scenes). Level 2, which dropped 34 percent of frames in the build 105 tour (build 104: 20), came back to 17 percent on build 106; level 5 went the other way (31 percent; builds 104 and 105: 19 and 27). Each tour was run once and the cause of the slowdown against build 104 has not been found.

- **A car slides along a wall or fence again** at a shallow angle instead of stopping, as in builds 92 to 96, and still stays on four wheels in a hard hit. The stop at the wall came with the fix for flipping over in build 99. Now the car slides at angles up to about 30 degrees, as in build 92; at 45 degrees and more it still stops at the wall. The PS2 recordings lose almost all speed at 26 to 35 degrees, so on this point build 106 is further from the original than builds 99 to 104 were.
- **Traffic cars no longer swerve out of your way.** The swerve was added in build 90; it is switched off, the code is still there. We have no PS2 recording of such a swerve.
- **A hit no longer moves the car in a jump on a long frame.** The shift after a hit is capped at 0.08 m per step; in the emulator a frame of 45 to 50 ms used to move the car up to 0.33 m at once.
- **Videos started from the View Movies menu now play inside the TV frame like the others (they were drawn over the list).** The video played while the options pack was open, and the TV drew the second screen of that pack (the list of videos) instead of the frame.

Not proven: trees and bushes hit at a slide, and getting into a car in a traffic jam.

## Build 104 (November 10)

Builds 102 and 103 were test builds and are folded in. Hardware tour (November 11): 113 scenes, ran to the end, no stalls, 0 read errors, 0 memory failures, no city block that did not fit, 0 late audio buffers in 53 minutes. Level loads took 3.3 s on average (build 101: 5.0 s, with the texture-dump switch on, which adds about 0.6 s). The worst wait for a city block was 1.9 s (build 101: 6.5 s). Not better than build 101: on the 81 shared scenes the dropped frames went from 14 to 19 percent (median over scenes) and the frames with a visible, not yet loaded block from 10 to 15; the median second was 26 fps against 27.

- **Mission intro cameras use the original fly-ins from the disc again,** shortened to the last 40 m before the hero and stopped in front of walls, so the camera no longer passes through buildings.
- **You can get back into your car after it has exploded** (the wreck drives slowly), as in the original.
- **Doors and phone booths glow.** Entrances and phone booths pulse when you are near, drawn by the engine (a star-shaped burst around doors, a blue disc and halo at booths), after frames of the PS2 original.
- **No black screen after a failed mission.** The failure screen now lifts the transition iris (it was left closed on level 5, mission 1, after getting in a car).
- **A race bet is paid at the card.** The stake is taken when you press X on the bet card.
- **Speech bubbles slide in** from the left over about a third of a second, and the text appears when the bubble is almost in place.
- **The card title** no longer slides under the "COLLECTOR CARD!" header.
- **Draw distance after a jump.** After a teleport the game keeps the draw distance at the reduced circle until the blocks that did not fit are in, and the request time in the log is now honest.

## Build 101 (November 9)

Hardware tour: 113 scenes, ran to the end, no stalls, 0 read errors, 0 memory failures, 1 city block that did not fit, 0 late audio buffers in 53 minutes. The tour ran with texture-table dumping on, so its level loads (4.99 s on average) are 0.6 s slower than in build 100 and do not show the gain below.

- **Two watchdogs.** One opens a transition iris that stays closed too long; one watches for city blocks that stay visibly missing and asks for them again.
- **Draw distance after a teleport** follows the shortened circle.
- **Footsteps.** A baked correction for the feet (no jerk when a new clip starts).
- **Names in the △ prompt**, and the buttons on the mission briefing cards.
- **Texture tables are baked for all 7 levels** (`texbake.shr`). The table holds each texture's average color and its palette after the cartoon curve, so a level load skips about 1.5 to 2 seconds of texture analysis (measured in the build 98 tour, where it ran every time). The baked tables matched the PSP's own keys on every level (0 mismatches in the tour log). Builds 100 and 101 had the files on the card but did not use them on levels 2 to 7: the tool that baked them left two side files out of the key. Fixed in this build.
- **The log says why a block is missing** (`MISSHIST`, `MISSCELL`).

## Build 100 (November 9)

Hardware tour: 109 scenes, ran to the end, 0 memory failures, level loading 4.4 s on average (5.6 s in builds 86 and 98).

- **Pedestrians and characters stand on their soles.** The placement cache is cleared when the level changes.
- **Draw distance by the capacity of the city area**, without collapses or swings (levels 5, 6 and 2); the circle of blocks works on level 4 too.
- **One load of the circle around the hero** at a level start instead of two.
- **Entering levels 3, 5 and 6** without refused loads.
- **A room gag** (the Observatory on level 3) lives in the heap, not in the level area.
- **Parked cars and jets** settle on the ground lazily, with no collision reads at a level start.
- **Credits as in the original:** you leave only with ✕, there is no Back button.

## Build 99 (November 9)

No separate hardware tour.

- **A car explodes from hits as in the original**, with mercy once.
- **A glancing hit on a wall** slows the car like the PS2 (two walls compared on level 4).
- **RESET works on all levels:** not into a wall, not inside a building, candidates picked along the lane.
- **Level 7:** invisible state props are no longer solid.
- **The level 6 booth**, the **level 2 block circles** (load and release counted as one), a **leak of cartoon outlines between missions**, and the **step-2 cap for NPCs** (64).
- **Character geometry in 16 bits:** 844 KB smaller over 89 character and menu files (checked file by file against the old data; worst point shift 0.19 mm).
- **Credits scroll at 20 px/s.**

## Build 98 (November 9)

Hardware tour: 85 scenes (it stopped at the last scene), 0 memory failures, 104 city blocks that did not fit on levels 5 and 6.

- **Sleep mode:** the graphics chip is reset after the PSP wakes.
- **Draw-distance regulator, 150 to 250 m:** it shortens the distance when the frame is late and lengthens it back.
- **Menu arrows** when the cursor rides onto a TV, a shorter run-up when a mission starts, and **speech bubbles** that wait for the arrival and the avatar.
- **Level 7:** soft bushes at one gap, and RESET in a dead end.
- **The frame is cleared every time**, so no leftovers of the previous scene show.
- **The phone booth always gives the chosen car** (it waits for memory, with no message), plus a watchdog for the car. Sky and car-body blocks now come from the heap: this was the root of "no sky" and "an invisible car".
- **Level 7, mission 2:** the car dealer is no longer mistaken for the race host.
- **Far NPCs** no longer trigger collision reads.

## Build 97 (November 8)

Hardware tour: 107 scenes, 0 stalls, lowest free memory 2.1 MB, 6 memory failures.

- **Gags and sound are loaded on demand.**
- **A collision ring:** the collision data is kept in pages around the player instead of whole (emulator: about 0.5 MB more free memory in a level 4 street, no differences against the whole data in a million probes on each level).
- **Character animations are packed:** a hero file is about 38% smaller on disk (148 to 161 KB), the pedestrian block on level 4 is 43% smaller (checked against the old animations to 0.15 degrees).
- **A clean reload** of a level, the **mission NPCs**, the load-screen guard.
- **Marge's hair stands up in open cars;** in closed ones it lies as in the original.

## Build 96 (November 8)

No separate hardware tour.

- **Memory zones as one region** behind the hero's area, sized by the free heap; **the city without slabs**; the **memory ladder works by zone**; newspaper transitions through the new loader; room placement.
- A **data cache for the graphics chip.**

## Build 95 (November 8)

Hardware tour of a candidate: 106 scenes; frames were the fastest of the series (p95 32.6 ms) but the memory zones ran out of memory 106 times in the middle of the tour.

- **Transitions** (phone booth, shop, wardrobe, cutscene, teleport) go through the new loader path.
- **Memory zones** for the hero and for the city with a pool for small objects, a **level block zone**, a **repeat of mission models**, and the texts of the fallback screens taken from the original game.

## Build 94 (November 8)

Hardware tour: 103 scenes, lowest free memory 0.25 MB, 5 memory failures.

- **Camera:** L+R handling.
- **Level-end transitions** and saving the next level; **character and mission-car models** load through the memory ladder.
- **The "Use Extra Memory" screen** explains what to switch on in the PSP settings when the game starts with too little memory.
- **Cutscenes** play through the preloader; the **shop and the wardrobe** no longer cancel silently.
- **New diagnostics:** the log keeps the history of memory failures.

## Build 93 (November 7)

- **Nobody appears before the ground under them.** At a level start, a door, a room exit or a mission move the hero is placed only when the city block and the floor under the spot are loaded (up to 6 seconds).
- **A watchdog.** Every frame the game checks for a hero or a car with nothing underneath, a body below the world, or a body stuck inside a wall, and puts it back on solid ground. Every case goes to the black box.
- **A memory ladder.** When a load does not fit, the game frees memory in steps and brings it back later, instead of failing.
- **The game checks its own files.** The builder stamps them (`set.txt`); files from another build show "Game files are from another build. Run the builder again."
- **Police and cars.** With the Hit & Run meter at zero the police give up after about 6 seconds; wheelies are limited so the ATV and the red brick car no longer flip on ramps.

Earlier builds: build 86 (November 3) is described in the [README](README.md).
