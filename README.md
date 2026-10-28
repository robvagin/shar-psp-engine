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

## What's new in build 78

October 28, 2026. Since build 71 (October 26):

- Phone booth opens every time, even when memory is tight, and lists the default car of every level, so the car you pick is the car that arrives. L and R flip through cars (hold to repeat); Mission Select changes level with L and R
- On foot the camera turns the way the PS2 game turns it, and the stick keeps your heading when the camera changes (leaving the booth, mission starts, camera cuts)
- Level loading: the interface and the sound bank load first after a level is freed, and the newspaper loading screen no longer disappears when memory is short: the game frees the old level and tries again. Transitions between levels and the booth no longer need a big free block, and unloading cells during streaming no longer stalls a frame
- Tires are black on every car, near and far; the player's car gets its own wheel texture on levels 1 and 3 to 7
- Cartoon mode keeps ink only where you interact: the hero, pedestrians, every car, the phone booth and props you hit or use. Houses, roads and the asphalt edge are no longer outlined
- Settings: new row CURBS on the PICTURE page. Original draws the curb with the game's own texture; Tinted is the earlier painted curb (it applies from the next level load). Defaults for a fresh save now match the author's PSP menus: Textures On, Saturation 1.28, Contrast 1.30, Eye Lines Off, Texture Detail 80%, Old TV 20%, Draw Distance 250 m. Existing saves are untouched
- Far distance: Draw Distance 250 m raises the city limit and the haze that looked like a white wall is gone; a ring of baked panoramas beyond the city edge is back for a pilot set of zones (levels 1, 3 and 6)
- Mission cards glow with the original's soft round light instead of a texture square
- Police keep 27 m from a standing player like on the PS2, and the wanted meter falls at once during a chase (no 3 s pause)
- Ramps and jump zones: the entrance rails of the big level 5 ramp no longer stop the car dead; thin gate-style jump zones launch the car like the PS2 (the level 5 fire escape); the fence rule at roof edges follows a PS2 recording
- Jumps near fences on levels 2 to 7: fence links that used to count as the map edge are now inner fences, so a car in flight no longer hits an invisible wall
- Green road arrows follow the slope without glow; the "Vote Quimby" van speaks near you; poles and lamp posts fall from a bumper push
- Message bubbles appear in one step with their text; the camera cut at the start of a mission stays within 30 m of you

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

About four weeks, 69 test builds and 27 full tours on a real PSP. 985 playtest notes, logged and worked through one by one.
Over 3,000 automated runs, with 130 recordings of the PS2 original as the reference. Fixes sit behind switches
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

GPL-3.0 for the code. Vendored Media Engine code keeps its MIT license, fonts keep CC BY 3.0. The leaked source of the original game was never used.
The Simpsons and The Simpsons: Hit & Run are trademarks of their respective owners. Non-commercial fan project.
