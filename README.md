<p align="center"><img src="docs/social.png" width="100%" alt="PSP Engine: a native engine that runs a PS2 game on the PSP. Not an emulator"></p>

# SHAR-PSP

A native PSP engine for *The Simpsons: Hit & Run*. Not an emulator and not decompiled code: a new engine in C that reads the original game data, converted from your own PS2 disc.

No game files are included, and none will be.

- All 7 levels, on foot and behind the wheel: missions, traffic, pedestrians, phone booths, collectibles
- Built for the PSP screen: 3D in true 16:9, every menu and HUD element rebuilt from the original menu files, nothing stretched
- Cartoon mode: the look of the show's early seasons, with ink outlines, flat fills and the show's palette
- About 24 fps on a PSP-2000 (median second of a full hardware tour of all 7 levels, build 106; build 104: 26; build 108 test sets: about 25 fps per scene on average, see below)

## Read next

- [What works and what is missing](docs/status.md)
- [Using the PSP Media Engine](docs/research/media-engine.md): sound mixing on the second core, what worked and what did not
- [How this port is built and tested](docs/process.md): the rules and the path of a fix from a playtest note to the handheld
- [Tests](tests/README.md): how every fix is checked
- [Changelog](CHANGELOG.md): what changed from build 93 to build 108, with the numbers from the PSP tours
- [Build 86 in pictures](docs/slides/README.md): what was checked, what changed and how fast it runs, in five slides
- [All documents](docs/index.md)

## What's new in build 108

Changes since build 106 (the full list for builds 93 to 108 is in the [changelog](CHANGELOG.md)). Checked in a headless emulator on a Mac, most of it with a test that fails on the old behaviour; tours on a PSP-2000 of the test sets are listed at the end of this part.

- **Missions start when the world is there.** The card closes only after the city around you is drawn; the countdown and the mission timer do not run under a closed screen, and the number shown is the real time left.
- **Retrying a failed mission is clean.** Yes after a failure restarts the mission without the long black screen (it could last several seconds) and without a second start from nowhere; the failed tries are counted once, so Skip appears on the right try.
- **Buttons on the mission card and in the talk before a mission wait for their hints.** X and Triangle are taken only once the button hints are on screen, so a press made during the fly-in no longer skips the card. The button icons sit 1 to 2 px lower, and Skip and its icon share a line like Continue.
- **A locked stage explains itself.** When a stage needs a car or an outfit you do not have, a hint window with Continue says what to get, and the next icon points to the seller.
- **Stage text and the pause.** The text of a stage is shown for about 3 seconds when it can be seen, the icon flies to the corner and stays. Pausing in a mission always shows the objective window with its text, centred under the menu frame. Pausing between two stages opens the mission pause (Restart and Cancel, with the objective window).
- **Mission cars behave closer to the original.** A chase follows the roads without braking in every bend and does not freeze after a ram; a mission car that falls in water or a ditch is put back on the road; the police car of a mission is no longer taken for a real cop, so it cannot arrest you or start the fine loop, and a Hit & Run chase is not started or cleared by it.
- **Races and gauges.** The finish line lies across the road; the closeness bar fills as the distance shrinks and turns red; the damage and closeness gauges no longer sit under the timer.
- **Interiors.** Characters of a mission are no longer hidden inside buildings (Moe in the tavern); a room opens after its characters have loaded, so Grampa no longer stands up in front of you.
- **Pickups and entrances.** Mission pickups and collector cards flash and glint at the pick-up like the key; the glow of doors is clear in the middle and bright at the edges; the coin by the counter spins like the original and keeps spinning in pause and in the menus.
- **Cars.** A car that lies upside down is put back on its wheels by the rules of the original; a car-to-car ram gives damage and does not lift one car on the roof of the other; on foot you can stand on the roofs of cars, tall ones included; suspension travel, spring stiffness and the centre of mass follow the values of the original.
- **Traffic.** A traffic car that you hit stands as a solid object for 10 to 15 seconds and then drives off to its lane by itself, with no jump in front of your eyes.
- **Walking.** Homer rides moving platforms and inherits their speed in a jump; the slope limit and the fall of knocked props follow the original more closely.
- **Breakable props.** On levels 2 to 5 and 7 all breakable state props (vending machines, columns, gates, kiosks) follow their state machine from the disc, and the glass with Krusty is neutral instead of tinted for each level (this is in the data, so the levels must be rebuilt).
- **Sleep and resume.** Putting the PSP to sleep during play or loading no longer leaves a level, a gag, a mission car or a character broken after waking.
- **Level 5 loads faster.** The wait for characters under the newspaper, about 3 s on level 5, is gone: the level opens after 8.25 s (median) against 10.98 s (tour of the November 16 set, PSP-2000). The other levels wait 0.3 to 0.45 s longer under the newspaper, because the city blocks around you are now really waited for.
- **Everything is loaded when the screen opens.** After the screen opens, no city block around you is still missing (0 of 30 level loads against 28 of 30; same tour), and the cars of the first stage are already in place.
- **Mission cars come in without a hitch.** The car of a mission appears in 18 to 20 ms against 47 to 66 ms (same tour).
- **Getting into a car from afar.** If the walk round a car to its door would take longer than about a second, or the way is blocked, the screen closes with the iris and you are put into the car, for every character and every car.
- **Smaller things.** The sounds of the car shop play on the effects bus (they were silent in Store mode); answering Yes to cancelling a save no longer opens the save window again; the draw distance regulator can reach 200 m where memory allows; the boot screen shows build 108; many loops that ran every frame (props, lights, gags, city draw lists) are cheaper.
- **Setup on Fedora, Arch and Windows.** `setup.sh` now knows `dnf` and `pacman`; on Python 3.13 and 3.14, which have no wheels for the pinned numpy, it installs Python 3.12 or takes newer numpy and Pillow builds; when the build stops, the page shows the reason, the kind of failure (not this disc, build failed, not copied) and the path of the log (`build/app/build.log`); on Windows without WSL it says so instead of failing in silence. Fedora and Arch are not tested by us.
- **Speed and memory (hardware tours of build 108 test sets).** Scripted tours on a PSP-2000 of test sets of build 108 ran to the end: 159 scenes on November 14 and 164 scenes on November 15 (two sets) and 166 scenes on November 16, with 0 read errors, 0 memory failures and three sleep-and-wake cycles per tour that left nothing broken. On the 114 scenes shared with the build 106 tour (November 14 set): average frame rate per scene 25.0 fps against 24.4, frame time p95 35.0 ms against 35.6 (median over scenes), frames missed 12,550 against 16,749 (25 percent fewer), frames with a visible not yet loaded block 39 against 85, lowest free memory 3.2 MB against 3.3, level load 2.9 s against 3.2 s on average (8 to 14 percent shorter on every level). Level 2 had dipped on that intermediate set (23.2 fps median against 25.6 in build 106); that was fixed, and the measurement after the fix, on the November 15 sets: the first drive scene of level 2 runs at 33.3 ms frame time against 36.2 before the fix, and the mission-character step of the frame takes 0.3 ms against 4.2. The final build was not toured as such: a few later changes came after the November 16 set.

### Build 106

Changes since build 104 (the full list for builds 93 to 106 is in the [changelog](CHANGELOG.md)). The hardware numbers below are from the scripted tour on build 106 (November 11). An earlier tour on build 105, which differs from 106 only in the frame around the videos, came out slower still.

- **A car slides along a wall or fence again** at a shallow angle instead of stopping, as in builds 92 to 96, and still stays on four wheels in a hard hit. At 45 degrees and more it stops at the wall.
- **Traffic cars no longer swerve out of your way.** The swerve (in since build 90) is switched off; the code is still there.
- **A hit no longer moves the car in a jump on a long frame.** The shift is capped at 0.08 m per step; in the emulator a long frame (45 to 50 ms) used to move the car up to 0.33 m at once.
- **Videos started from the View Movies menu now play inside the TV frame like the others (they were drawn over the list).**
- **Slower than build 104, not faster.** On the 114-scene hardware tour of build 106 the median second was 24.5 fps against 26.2, frame time p95 34.9 ms against 34.5, and dropped frames 20 percent of scenes against 19 (26 percent of all frames against 21). Memory is a little tighter: lowest free memory 3.3 MB (build 104: 3.5), and the worst wait for a city block was 2.1 s (build 104: 1.9). There were no stalls, no read errors and no memory failures. The earlier build 105 tour was slower than the build 106 one (23.6 fps), mostly on level 2, and that did not repeat on 106; level 5 got worse instead (31 percent of dropped frames in its scenes against 19 in build 104). The tours were run once each and the cause of the slowdown against 104 has not been found.

### Build 104

The tour numbers in this part are from the build 104 hardware tour of November 11:

- **Doors and phone booths glow.** Entrances and phone booths pulse when you are near, drawn by the engine instead of a texture, as in the original.
- **Memory holds up.** On the 113-scene hardware tour of build 104 there were no memory failures and no city block that did not fit; the lowest free memory was 3.5 MB (build 93: 1.1 MB). On the 81 scenes the tours share, the count of frames with a visible, not yet loaded block fell from 77 to 15 (build 101 had 10).
- **Level loading is shorter.** The builder bakes the texture table of every level (`texbake.shr`), so the game skips about 1.5 to 2 seconds of texture analysis on each level load. It was verified key by key on the PSP; in the build 104 tour a level load took 3.3 s on average, against 5.0 s in builds 93 and 101.
- **Mission intro cameras use the original fly-ins from the disc again,** shortened to the last 40 m before the hero and stopped in front of walls, so the camera no longer passes through buildings.
- **You can get back into your car after it has exploded** (the wreck drives slowly), as in the original.
- **Fewer small glitches.** A mission failure no longer leaves a black screen, bets are paid at the card, and speech bubbles slide in.
- **Speed.** On the 81 scenes that the build 93 and build 104 hardware tours share, the 95th-percentile frame time went from 36.5 to 34.8 ms (median over scenes) and dropped frames from 36 to 19 percent (build 101: 14 percent; the median second is 26 fps against 27 in build 101).

Before the first build on a Mac, accept the Xcode license (`sudo xcodebuild -license accept`) or install the Command Line Tools (`xcode-select --install`). Firmware: tested on PRO-C and ARK-4; ME/LME and Adrenaline were not tried.

If the game stops, send `SHARVIEW/shar_report.txt` and `SHARVIEW/blackbox.txt` from the Memory Stick (copy them before you start the game again) and the map code from the bottom left of the pause screen, for example `L1 K9-7`. Please add the PSP model, the firmware and whether your disc is PAL or NTSC.

### Build 86

November 3, 2026. Since build 85 (the same day):

- Faster: on the full hardware tour of all 7 levels the p95 frame time (median over scenes) went from 39.2 to 35.8 ms and dropped frames from 31 to 18 percent; 49 of 55 scenes got faster. The mission start camera no longer rebuilds the street grid each time a cell unloads, the wasp's shot checks only nearby props, parked traffic and standing cars sleep instead of running full physics, and skid marks go to the graphics chip piece by piece
- Mission complete in a car: the hero cheers from the driver's seat, like the original, instead of sliding out through the door
- Mission failed: the red FAILED title is 1.5 times bigger
- Known: after about ten level changes in one session, a level's collectibles may not load (a new list fragments memory). Fix planned for the next build

## Build

### The easy way: one line, then your browser

Paste one line. It downloads this code into `~/SHAR-PSP`, sets up the tools and opens Hit & Run PSP Builder in your browser: a page with a PSP on it.
Drop the image of your own disc on the PSP screen (`.iso`, PAL or NTSC). The build takes about an hour, up to two on a slower machine;
when it is done, connect the PSP by USB and press **Put on PSP**: only changed files are copied, each one is checked

- Mac or Linux, in Terminal: `curl -fsSL https://raw.githubusercontent.com/robvagin/shar-psp-engine/main/app/get.sh | bash`
- Windows, in PowerShell: `irm https://raw.githubusercontent.com/robvagin/shar-psp-engine/main/app/get.ps1 | iex`
  (runs through Ubuntu inside Windows, WSL; the first time it installs WSL and asks for a restart. Not tested on Windows yet)

The page runs only on your computer (`127.0.0.1`), nothing is uploaded anywhere. Close the Terminal window and the build pauses;
run the same line again and it picks up where it stopped. Nothing from the game is inside the code: it builds only from your disc

### By hand

1. `./setup.sh` installs the PSPDEV toolchain into `./pspdev`
2. `export PSPDEV=$PWD/pspdev PATH=$PWD/pspdev/bin:$PATH`
3. `cd viewer && psp-cmake -S . -B build && cmake --build build` builds `EBOOT.PBP`

The exact build we test on a PSP-2000 (release switches; each `*_OLD=1` keeps an older behavior that won on hardware):

```
psp-cmake -S . -B build -DSHAR_RELEASE=ON -DSHAR_RELEASE_KEEP_DIAG=ON -DSHAR_RELEASE_KEEP_HUD=ON \
  -DCMAKE_C_FLAGS="-DJITTER_STRIPS_OLD=1 -DJITTER_CAR_OLD=1 -DFE_DEMO_PS2_OLD=1 -DZF932_OLD=1 -DCARFILL930_OLD=1 -DGAME_C7_TALKHIDE_OLD=1"
```

Game data: unpack your own disc with [hit-and-run-web](https://github.com/Vheissu/hit-and-run-web) into
`hit-and-run-web/source/`, run `tools/build_level.sh` or `tools/ship.sh`, and copy the result next to the EBOOT
in `PSP/GAME/SHARVIEW`. Needs custom firmware and a PSP-2000 or PSP-3000. The PSP-1000 has half the memory and is not tested yet. Tested on a PSP-2000.

- Space: the game folder takes about 3.3 GB on the Memory Stick (7 levels, cutscenes, sound, four languages). Building needs about 10 GB free on the computer
  (disc image, unpacked disc, tools and the finished game) and takes roughly an hour, up to two on a slower machine
- Computer: tested on macOS. Ubuntu 22.04/24.04 (x86_64 or arm64): the Linux path comes from noname8723, who built the game on Windows through Ubuntu;
  we have not run it on Linux ourselves yet. Fedora (dnf) and Arch (pacman) are set up by `./setup.sh` too, but not tested by us: if it stops, the page shows the reason
  and the path of the log (`build/app/build.log`). On Windows use WSL (Ubuntu inside Windows); the builder does not run in Windows itself
- Python: 3.11 or 3.12 is the tested pair. On 3.13/3.14 (Fedora 41+, Arch) `setup.sh` first tries to install 3.12, otherwise it uses newer numpy/Pillow builds;
  one tool package (`meshoptimizer`) is compiled on your computer, so the Python headers and a C++ compiler must be installed
  (Fedora: `sudo dnf install python3-devel gcc gcc-c++`, Ubuntu: `sudo apt install python3-dev build-essential`)
- Ubuntu notes: `./setup.sh` installs the apt packages (build-essential, cmake, 7zip or p7zip-full, ffmpeg and the libraries the PSP toolchain needs)
  and clones `hit-and-run-web` and `donut`. Then run `source tools/linux_env.sh`: it adds small `stat`/`cp`/`sed` wrappers for the macOS-style
  commands in the test scripts (`tools/ship.sh` does not need them). Python 3.11 or newer is required (Ubuntu 22.04 needs the deadsnakes PPA).
  The smoke test needs a headless PPSSPP: set `PPSSPP_HEADLESS=/path/to/PPSSPPHeadless`; without it `ship.sh` skips the smoke test and says so
  (`SHIP_SMOKE_REQUIRED=1` makes that an error). `--deploy` finds the Memory Stick in `/media/$USER/PSP`, `/run/media/$USER/PSP`, `/mnt/PSP`
  or any volume with `PSP/GAME` (`SHIP_PSP=/path` to force one). Instead of `caffeinate -i` use `systemd-inhibit tools/ship.sh`
- Fixed: on a fresh clone `tools/ship.sh` used to stop at `road934.shr` (a side file of the level export was dropped while staging)

- Discs: PAL `SLES_518.97` and US NTSC `SLUS_206.24`. The executable is read from `SYSTEM.CNF`; NTSC movies are 640x448 at 29.97 fps
  and are resampled to the engine's 25 fps
- `setup.sh` also creates `.venv` (Python 3.11+), installs the extra packages, the Homebrew libraries the macOS toolchain needs
  (`gmp libmpc mpfr zstd libusb`), ffmpeg (with the IPU decoder) and clones `donut` next to the engine
- `tools/ship.sh` builds level 1 by default. All levels: `SHIP_LEVELS="2 3 4 5 6 7" caffeinate -i tools/ship.sh`
- Sprites (coin, phone above the booth, wrench and its glow) are baked from your own disc by `tools/sprites_bake.py`.
  The coin needs Blender (`BLENDER=/path/to/blender`); without it, coins are drawn as 3D models
- In game, R+SELECT shows or hides the FPS counter

Thanks to the first outside builder for the NTSC report and the build notes, and to noname8723 for the Ubuntu build path (setup, Memory Stick and emulator lookup, Linux wrappers for the test scripts).

## Repository

- `viewer/` the PSP engine (C, PSPSDK). `viewer/me/` is the Media Engine module
- `tools/` the converter from the PS2 disc to PSP packages, plus build and test scripts (Python, shell)
- `tests/` regression checks, one folder per fix family
- `docs/` status, research and typography settings
- `bench/`, `metest/` small PSP test programs
- `assets/fonts/` pixel fonts (CC BY 3.0)

Comments in the code are mostly in Russian.

## How it was made

About six weeks, 108 test builds and dozens of full tours on a real PSP. More than 1,360 playtest notes, logged and worked through one by one.
Over 4,200 automated runs, with 130 recordings of the PS2 original as the reference. Fixes sit behind switches
that restore the old behavior, and a check counts only after it fails on the old code.

## Reporting bugs

The pause screen shows a map code in the bottom-left corner (like `L1 K9-7`). Put it in your issue,
so anyone can find the exact spot.

## Credits and references

- [donut](https://github.com/plowteam/donut), an open reimplementation of the game (GPL-3.0): formats and game logic reference
- [hit-and-run-web](https://github.com/Vheissu/hit-and-run-web) by Dwayne Charrington (MIT): PS2 disc format tools
- [psp-media-engine-safe-task](https://github.com/mcidclan/psp-media-engine-safe-task) and
  [psp-media-engine-custom-core](https://github.com/mcidclan/psp-media-engine-custom-core) by mcidclan (MIT):
  running our own code on the Media Engine (`viewer/me/vendor`, licenses kept)
- [The Simpsons: Hit & Run Wiki](https://simpsons-hit-and-run.fandom.com/wiki/The_Simpsons_Hit_and_Run_Wiki): game facts
- [Simpsons Wiki](https://simpsons.fandom.com/wiki/): every collectible card has a QR code that opens its episode page
- [@scenic_simpsons](https://www.instagram.com/scenic_simpsons/): frames from seasons 1–8 used to build the Cartoon mode palette
- PSP-2000 drawing on the cover: [PSP 2000 White](https://commons.wikimedia.org/wiki/File:PSP_2000_White.svg) by Paril (openclipart), CC0
- Pixel fonts by Patrick H. Lauke, [CC BY 3.0](http://creativecommons.org/licenses/by/3.0/):
  [Bart vs. the Space Mutants (16bit)](http://www.fontstruct.com/fontstructions/show/1363863),
  [The Simpsons](https://fontstruct.com/fontstructions/show/879452) (`assets/fonts/pixel`)

## License

GPL-3.0 for the code. Vendored Media Engine code keeps its MIT license, fonts keep CC BY 3.0.
The Simpsons and The Simpsons: Hit & Run are trademarks of their respective owners. Non-commercial fan project.
