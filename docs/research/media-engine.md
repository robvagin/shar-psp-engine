# Using the PSP Media Engine in a game port

Notes from porting *The Simpsons: Hit & Run* to a PSP-2000 (custom firmware). Every number below was measured on a real PSP unless it says otherwise. The emulator (PPSSPP) does not execute code on the Media Engine, so nothing here could be checked there.

## What we did

The Media Engine (ME) is a second MIPS core in the PSP. Sony exposed it to games only through the firmware codecs (Atrac, MPEG). To run our own code on it we use **MIST** (MIT license) from mcidclan: the ME is handed a task through a slot in its kernel table and the firmware services (video decoding, sleep) keep working.

- **Sound mixing runs on the ME.** The main core prepares a job, the ME mixes the next buffer while the current one plays, the main core applies the limiter and outputs. This is on by default in the current build.
- **Our own function on the ME works:** reply in 5 microseconds, FPU available, sums of 16 KB buffers correct, about 0.3 ms per task (tests on October 4, 2026).
- **Particles and city culling were also moved to the ME and then switched off** (details below).

## What we learned

| Finding | Evidence |
|---|---|
| The ME cannot read kernel-module memory. Code and the DMA word must live in game (user) memory | Function inside a kernel module: slot not set (tests 10-12) or task accepted and never finished (test 13). Function in game memory: replies in 5 microseconds (test 15) |
| Cache is not shared. Every handoff needs a cache flush on one side and an uncached address (+0x40000000) on the other | Same rule reported by the Daedalus author (2007) and used in Perfect Dark PSP (`.uncached`); a mistake crashes the game |
| The ME has no GPU access, no file I/O, no VFPU | Draw lists are still built on the main core |
| PPSSPP does not run ME code | A stub on the main core runs the same path in the emulator; real timing only on hardware |

## Numbers from hardware

Sound on the ME, A/B on the same tour (everything on the main core vs. ME on), October 10, 2026:

| Measure | Main core only | ME on |
|---|---|---|
| Main-core time per 11.6 ms audio buffer, L1 | 700-990 us | 450-540 us |
| Mixing part of that | 420-700 us | 0-165 us |
| Share of the audio thread | 6.0-8.6 % | 4.0-5.0 % |
| Late audio buffers (crackle) | 0 | 0 |
| Skipped frames on the heavy level (load / drive / explosion) | 66 / 73 / 50 | 32 / 39 / 26 |
| p95 frame time on that level | 33.75-34.0 ms | 33.0-33.25 ms |

The sum of frame phases did not change (within 0.5 ms). The gain is that the audio thread no longer interrupts the frame, about 1 ms per frame. The ME-on run also had particles on the ME; they gave no gain (see below), so we attribute the change to sound.

Latest tour (November 3, 2026, build 86, 1818 s): 0 late buffers, worst buffer 1590 us. Per buffer, averages: cache flush 17 us, submit 197 us, wait for reply 4 us, prepare 74 us, finish 147 us.

## What did not work

- **Particles (smoke, fire, sparks, debris) on the ME.** Bit-exact against the main-core code over 900 frames on the Mac, but on the PSP no net gain: the update step got slower (one level: 48 to 341 us, cache flush for the task) while drawing got faster (202 to 81 us). Left on the main core.
- **City culling on the ME.** Bit-exact on 21,000 camera poses in a host test. On the developer's PSP it saved 0.11-0.26 ms per frame (about 1 %) and the ME missed its deadline in 18-27 % of frames. Off by default, code kept.
- **Not tried on hardware:** traffic AI and path planning (our estimates said not worth it), anything that needs the VFPU or system calls.

## Safeguards we use

1. No reply within 50 ms: the main core computes that buffer itself and the ME is left alone from then on.
2. During video playback (the JPEG decoder uses the ME) and on PSP sleep, sound is not given to the ME.
3. The task code is shared between the ME, the emulator stub and a host test, so one bit-exact test covers all three.
4. The ME gain is claimed only from hardware tours.

## References

- mcidclan, psp-media-engine-safe-task (MIST), MIT: https://github.com/mcidclan/psp-media-engine-safe-task
- mcidclan, psp-media-engine-custom-core, MIT: https://github.com/mcidclan/psp-media-engine-custom-core
- z2442, Perfect Dark PSP, MIT, sound mixer on the ME in `port/src/mixer_me.c`: https://github.com/z2442/perfect_dark-PSP
- DaedalusX64 1.1.8 release notes (audio on the ME became fully stable): https://github.com/DaedalusX64/daedalus/releases/tag/1.1.8
- Wololo, "Unlocking the PSP's Dual Core setup", June 16, 2026: https://wololo.net/2026/06/16/unlocking-the-psps-dual-core-setup/
- Unreal (1998) on PSP-2000, barttenbrinke/UE1, sound and music on the ME: https://github.com/barttenbrinke/UE1
