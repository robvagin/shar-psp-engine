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

## What's new in build 86

November 3, 2026. Since build 85 (the same day):

- Faster: on the full hardware tour of all 7 levels the p95 frame time (median over scenes) went from 39.2 to 35.8 ms and dropped frames from 31 to 18 percent; 49 of 55 scenes got faster. The mission start camera no longer rebuilds the street grid each time a cell unloads, the wasp's shot checks only nearby props, parked traffic and standing cars sleep instead of running full physics, and skid marks go to the graphics chip piece by piece
- Mission complete in a car: the hero cheers from the driver's seat, like the original, instead of sliding out through the door
- Mission failed: the red FAILED title is 1.5 times bigger
- Known: after about ten level changes in one session, a level's collectibles may not load (a new list fragments memory). Fix planned for the next build

### Build 85

November 3, 2026. Since build 83 (November 2):

- Four languages: English, French, German and Spanish, text and voices
- Saves keep best times and attempts for every mission, like the original. Saves from earlier builds are read and converted on load, progress included
- Mini Game: the full set of bonus cars, rivals drive the track roads and bump into each other instead of passing through, the countdown is the same as in street races, the HUD and the pause menu use the main game's fonts, the pause stops the music, and Select cycles the race views plus the top view
- Pause menu: a new CAMERA section with six camera settings; the backdrop and frames no longer blink between OPTIONS sections
- Scrapbook: every page, with its transitions; small captions in the Bart font, Accept and Back on every page
- The original cheat codes, including the four extra cameras
- Cars: physics in even sub-steps, with corner grip checked against PS2 recordings, so cars stay on their wheels at speed
- Collectibles: from the car, cards and gags are picked up with the original reach; the wasp's shot now pushes props, knocks down pedestrians and dents traffic
- Level 7: solid bushes, no invisible fences or poles after a booth is knocked down, windows drawn as plain textures without blinking
- Ink outlines only on characters and cars
- Look-back in the car is gone; the radar is back to its earlier look, with the blinking dot kept inside its edge

## Build

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
- Computer: tested on macOS. On Windows it should run in WSL, but that is not tested yet
- Known issue (November 3): on a fresh clone `tools/ship.sh` can stop at `road934.shr`. A fix is on the way

- Discs: PAL `SLES_518.97` and US NTSC `SLUS_206.24`. The executable is read from `SYSTEM.CNF`; NTSC movies are 640x448 at 29.97 fps
  and are resampled to the engine's 25 fps
- `setup.sh` also creates `.venv` (Python 3.11+), installs the extra packages, the Homebrew libraries the macOS toolchain needs
  (`gmp libmpc mpfr zstd libusb`), ffmpeg (with the IPU decoder) and clones `donut` next to the engine
- `tools/ship.sh` builds level 1 by default. All levels: `SHIP_LEVELS="2 3 4 5 6 7" caffeinate -i tools/ship.sh`
- Sprites (coin, phone above the booth, wrench and its glow) are baked from your own disc by `tools/sprites_bake.py`.
  The coin needs Blender (`BLENDER=/path/to/blender`); without it, coins are drawn as 3D models
- In game, R+SELECT shows or hides the FPS counter

Thanks to the first outside builder for the NTSC report and the build notes.

## Repository

- `viewer/` the PSP engine (C, PSPSDK). `viewer/me/` is the Media Engine module
- `tools/` the converter from the PS2 disc to PSP packages, plus build and test scripts (Python, shell)
- `tests/` regression checks, one folder per fix family
- `docs/` status, research and typography settings
- `bench/`, `metest/` small PSP test programs
- `assets/fonts/` pixel fonts (CC BY 3.0)

Comments in the code are mostly in Russian.

## How it was made

About five weeks, 86 test builds and 43 full tours on a real PSP. 1,159 playtest notes, logged and worked through one by one.
Over 3,400 automated runs, with 130 recordings of the PS2 original as the reference. Fixes sit behind switches
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
