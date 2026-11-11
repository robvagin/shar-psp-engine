<p align="center"><img src="docs/social.png" width="100%" alt="PSP Engine: a native engine that runs a PS2 game on the PSP. Not an emulator"></p>

# SHAR-PSP

A native PSP engine for *The Simpsons: Hit & Run*. Not an emulator and not decompiled code: a new engine in C that reads the original game data, converted from your own PS2 disc.

No game files are included, and none will be.

- All 7 levels, on foot and behind the wheel: missions, traffic, pedestrians, phone booths, collectibles
- Built for the PSP screen: 3D in true 16:9, every menu and HUD element rebuilt from the original menu files, nothing stretched
- Cartoon mode: the look of the show's early seasons, with ink outlines, flat fills and the show's palette
- About 24 fps on a PSP-2000 (median second of a full hardware tour of all 7 levels; the tour was run on build 105, which build 106 matches except for the frame around the videos; build 104: 26)

## Read next

- [What works and what is missing](docs/status.md)
- [Using the PSP Media Engine](docs/research/media-engine.md): sound mixing on the second core, what worked and what did not
- [How this port is built and tested](docs/process.md): the rules and the path of a fix from a playtest note to the handheld
- [Tests](tests/README.md): how every fix is checked
- [Changelog](CHANGELOG.md): what changed from build 93 to build 106, with the numbers from the PSP tours
- [Build 86 in pictures](docs/slides/README.md): what was checked, what changed and how fast it runs, in five slides
- [All documents](docs/index.md)

## What's new in build 106

Changes since build 104 (the full list for builds 93 to 106 is in the [changelog](CHANGELOG.md)). **The hardware tour was run on build 105; build 106 differs from it only in the frame around the videos.**

- **A car slides along a wall or fence again** at a shallow angle instead of stopping, as in builds 92 to 96, and still stays on four wheels in a hard hit. At 45 degrees and more it stops at the wall.
- **Traffic cars no longer swerve out of your way.** The swerve (in since build 90) is switched off; the code is still there.
- **A hit no longer moves the car in a jump on a long frame.** The shift is capped at 0.08 m per step; in the emulator a long frame (45 to 50 ms) used to move the car up to 0.33 m at once.
- **Videos started from the View Movies menu now play inside the TV frame like the others (they were drawn over the list).**
- **Slower than build 104, not faster.** On the 114-scene hardware tour (build 105) the median second was 24 fps against 26, frame time p95 35.5 ms against 34.5, and dropped frames 21 percent of scenes against 19 (28 percent of all frames against 21). Memory is a little tighter: lowest free memory 3.3 MB (build 104: 3.5). There were no stalls, no read errors and no memory failures. The tour was run once and the cause of the slowdown has not been found.

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
  we have not run it on Linux ourselves yet. On Windows use WSL (Ubuntu inside Windows)
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

About six weeks, 106 test builds and dozens of full tours on a real PSP. More than 1,260 playtest notes, logged and worked through one by one.
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
