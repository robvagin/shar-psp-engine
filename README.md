<p align="center"><img src="docs/social.png" width="100%" alt="PSP Engine: a native engine that runs a PS2 game on the PSP. Not an emulator"></p>

# SHAR-PSP

A native PSP engine for *The Simpsons: Hit & Run*. Not an emulator and not decompiled code: a new engine in C that reads the original game data, converted from your own PS2 disc.

No game files are included, and none will be.

- All 7 levels, on foot and behind the wheel: missions, traffic, pedestrians, phone booths, collectibles
- Built for the PSP screen: 3D in true 16:9, every menu and HUD element rebuilt from the original menu files, nothing stretched
- Cartoon mode: the look of the show's early seasons, with ink outlines, flat fills and the show's palette
- About 28 fps on a PSP-2000 (median of a full hardware tour of all 7 levels)

## Read next

- [What works and what is missing](docs/status.md)
- [Using the PSP Media Engine](docs/research/media-engine.md): sound mixing on the second core, what worked and what did not
- [How this port is built and tested](docs/process.md): the rules and the path of a fix from a playtest note to the handheld
- [Tests](tests/README.md): how every fix is checked
- [Build 86 in pictures](docs/slides/README.md): what was checked, what changed and how fast it runs, in five slides
- [All documents](docs/index.md)

## What's new in build 93

- **Nobody appears before the ground under them.** At a level start, a door, a room exit or a mission move, the hero is placed only when the city block and the floor under the spot are loaded (up to 6 seconds). The screen transition stays closed until he stands on it. Traffic pedestrians and mission characters follow the same rule.
- **A watchdog.** Every frame the game checks for a hero or a car with nothing underneath, a body below the world, or a body stuck inside a wall, and puts it back on solid ground. The player sees nothing; every case goes to the black box.
- **A memory ladder.** When the city, a menu or a mission needs memory that is not free, the game releases it in steps (read-ahead, far blocks, a shorter draw distance for a while, a simpler menu) and brings it back later, instead of failing. When a city block on level 4 cannot fit, the ladder now runs before giving up on it. Not verified on a real PSP yet.
- **The game checks its own files.** The builder stamps them (`set.txt`). Files from another build show "Game files are from another build. Run the builder again."; a missing level names the missing file instead of silently skipping the level or returning to the menu.
- **Police and cars.** With the Hit & Run meter at zero the police give up after about 6 seconds; wheelies are limited, so the ATV and the red brick car no longer flip on ramps.

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

About six weeks, 93 test builds and dozens of full tours on a real PSP. 1,220 playtest notes, logged and worked through one by one.
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
