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
- [All documents](docs/index.md)

## What's new in build 83

November 2, 2026. Since build 81 (October 31):

- A full behavior pass against the original game: driving and on-foot physics, traffic and pedestrians, police and mission cars, the camera, missions and the moves between them, menus, sounds and loading. More than 900 points compared, each change behind a switch that restores the old behavior. What is ours for the PSP stays ours: fonts, layout, 16:9, camera angle, buttons
- Every level has its own hero (Homer, Bart, Lisa, Marge, Apu, Bart, Homer), and each one now has the dodge, cheer and look-back moves
- Police and the wanted meter follow the original timing on every level; mission cars, races and their catch-up behave like the PS2
- Burnouts throw light dust on the road and dirt, grass or water spray by surface; the burning-car smoke comes only after a long burnout
- Rooms: the gap next to the TV in the Simpsons' house is gone (rooms are drawn relative to the camera, like the city, so the PSP's graphics chip keeps its precision), and the first room camera after a load or a door is a straight cut, no fly-in from behind
- The level 7 camera tilt stays outside, it no longer leans the room and talk cameras
- Any car can push and break street props, traffic and mission cars included, and it costs almost nothing: only moving cars and props nearby are checked
- Level loading is faster again: the baked traffic data is rebuilt with every data set, so the PSP no longer recomputes it at each load
- In the car, look back with D-pad down

### Build 81

October 31, 2026. Since build 78 (October 28):

- Mission Select inside the same level now works like a real mission change: the music restarts with the level intro, and the loading screen shows every time and stays up until the move is done, even when you switch fast
- The mission car waits at its own spot (usually by the phone booth). It is moved next to you only when you stand close on the same ground or are far away, so it no longer lands on a ledge it cannot drive off (level 6, mission 4 by the observatory)
- The loading screen stays up until the characters of the mission, the bonus characters nearby and the mission cars are loaded. Nothing pops in after the picture opens (Grandpa in the Simpsons' house); the dark screen at a house door waits the same way
- The level sound bank has one fixed place in memory for the whole game, sized for the largest bank, so a level never loses its sound after many level changes (level 5 used to go silent)
- The visibility table of levels 1 and 4 loads in small pieces when memory is tight, so the level always gets it

## Build

1. `./setup.sh` installs the PSPDEV toolchain into `./pspdev`
2. `export PSPDEV=$PWD/pspdev PATH=$PWD/pspdev/bin:$PATH`
3. `cd viewer && psp-cmake -S . -B build && cmake --build build` builds `EBOOT.PBP`

The exact build we test on a PSP-2000 (release switches; each `*_OLD=1` keeps an older behavior that won on hardware):

```
psp-cmake -S . -B build -DSHAR_RELEASE=ON -DSHAR_RELEASE_KEEP_DIAG=ON -DSHAR_RELEASE_KEEP_HUD=ON \
  -DCMAKE_C_FLAGS="-DJITTER_STRIPS_OLD=1 -DJITTER_CAR_OLD=1 -DFE_DEMO_PS2_OLD=1 -DZF932_OLD=1 -DCARFILL930_OLD=1"
```

Game data: unpack your own disc with [hit-and-run-web](https://github.com/Vheissu/hit-and-run-web) into
`hit-and-run-web/source/`, run `tools/build_level.sh` or `tools/ship.sh`, and copy the result next to the EBOOT
in `PSP/GAME/SHARVIEW`. Needs custom firmware and a PSP-2000 or later (the PSP-1000 has too little RAM). Tested on a PSP-2000.

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

About five weeks, 83 test builds and 40 full tours on a real PSP. 1,086 playtest notes, logged and worked through one by one.
Over 4,500 automated runs, with 130 recordings of the PS2 original as the reference. Fixes sit behind switches
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
