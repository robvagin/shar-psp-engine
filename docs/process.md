# How this port is built and tested

This is a port of The Simpsons: Hit & Run to the PSP-2000 (custom firmware). It is not an emulator and not a recompile. The engine is written from scratch in C (`viewer/`) and reads assets that the owner converts offline from their own PS2 disc into a compact format. No game data is published or committed.

The owner is an art director, not a programmer. That shapes the process: the owner says what looks or feels wrong, and everything else (code, build, tests, deploy, analysis) is done by AI coding sessions working under fixed rules. This page describes those rules and the path a complaint takes from a sentence to a build on the handheld.

## Rules

**Hard rules**
- References are limited to an open-source re-implementation (donut, GPL-3.0) and MIT-licensed PS2 format tooling.
- Game data is never published or committed. Only the engine, tools and documents are.
- Only one game folder lives on the PSP card. Test builds go there only when the owner says so.
- Publishing (code, posts) is done by one person after a review pass, never by a worker session.

**Owner priorities**
- Completeness over peak frame rate: all missions, textures, sounds and gags of the original. Frame rate should be steady; a flat 30 FPS beats a jumpy 40 to 60.
- The port should look native on the wide screen: 3D at a true 16:9, UI built from the original resources with correct proportions, each element anchored to its own screen edge.

**Working rules**
- Every step ends with one line in a progress log.
- At most three attempts per problem. If it fails, write down what was tried, roll back, move on.
- A step that needs a check on the handheld ends with a stop. The next step does not start blind.
- No new libraries or repositories outside the plan without an explanation.

**Tuning principle**
- A complaint about one spot means checking the whole family on every level. Behavior comes from object type, material and state, not from a texture that happens to match.
- No patching coordinates of one place and no deleting collision to make a test green.
- Every fixed dead end becomes a reproducible check: the old code shows the problem, the new code passes it and its neighbors.
- Acceptance runs in order: logic on a desktop, repeatable gameplay scenarios, the real handheld, then the owner's verdict on feel. A fast desktop does not prove handheld speed.
- What is not covered is written as "not covered". We do not claim a complete port.

## Path of a fix

```
owner's words
  -> numbered line in the request list (time, words)
  -> conductor assigns it to a worker session; the worker takes its own
     git worktree and branch from the current base
  -> check against the original: disc scripts, PS2 recordings, reference data
  -> reproduce in a headless emulator with a scripted scenario
  -> fix the whole family, behind a switch that restores the old behavior
  -> acceptance script: a green run on the fix, a red run on the old behavior
  -> log line: ticket, branch, commit, PASS, what changed
  -> conductor merges into build N: one build, build scripts,
     frame gate, texture-count check on all levels
  -> tour of build N in the emulator to the end marker,
     plus a player-path pass without any service files
  -> deploy to the handheld on the owner's word, verified by SHA-256
  -> owner's tour on hardware (a scripted tour that plays itself),
     logs copied off the card
  -> analysis: tour comparison against the previous tour,
     A/B by switch on identical data
  -> new items go into the next build
```

**Roles**
- Owner: reports what is wrong and accepts builds on the handheld.
- Conductor (one session): keeps the numbered request list (single writer of statuses), writes assignments, merges branches into each build, deploys, analyzes tours, publishes.
- Worker sessions (five in the last rounds, each with its own area): take their lines, work in their own worktree, hand in with a log line.
- Journal: one shared file where every handoff, finding and decision gets a timestamped line signed by its author.

**Why every fix has a switch.** The old behavior stays in the code behind a named switch. It is the red control for the acceptance script and the rollback if the owner dislikes the result.

## Why the measurements can be trusted

- A green check without a red one counts for nothing. Each check must fail at least once on the old behavior, otherwise it proves nothing.
- Handheld cost is estimated from desktop runs with a factor of about 5.8 (the handheld is CPU bound). Half a millisecond on the desktop is about three on the PSP. The cause of a regression is proven by an A/B switch on identical data, not by correlation in a log.
- A test is not the game. Service files switch parts of the game off (fly-by cameras, hints, movies). Before every deploy the player path is checked separately without them: menu, new game, on foot, in a car, save and resume.
- Tour artifacts are real. Writing the log and teleporting the scenario cost frames by themselves, so verdicts use frame work time, and every new diagnostic carries its own cost.
- Example: in one hardware tour the city phase grew by 0.41 ms on average. Bisecting by switches on a still camera pinned about 0.85 ms (converted to PSP) on one merge, which was then made cheaper. A separate depth-pass change added about 0.49 ms on one level and let the sea hide the distant view on the pier, so the following builds ship with it off.

## Numbers (build 71, October 26, 2026)

| What | Count | Source |
|---|---|---|
| Numbered requests | 997 | request list and the loading-screen counter |
| Builds | 71 | loading-screen counter |
| Runs (emulator and hardware) | 3543 | loading-screen counter: reference catalog, smoke runs, emulator runs, hardware tours |
| Logged play in run logs | 51 h | sum of the last timestamp of 2287 run logs still on disk; older logs were deleted, so this is a lower bound |
| Hardware tour log folders | 29 | full tours plus repeats |
| Commits on the integration branch | 3358 | `git rev-list --count` |
| Real PS2 recordings used for comparison | 130 | reference library index |
| Named rollback switches in the code | 1068 | unique `*_OLD` names in `viewer/` (not all independent) |
| Test folders | 272, including 55 acceptance scripts and 60 gate scripts | `tests/`, files `accept*.sh` and `*gate*.sh` |
| AI tokens | 18.7 billion over 66 sessions | Claude Code logs, September 26 to October 26, 2026; 18.5 billion of them are re-reads of cached context, 34.5 million were written; older logs were deleted, so this is a lower bound |

Not measured: total hours people spent on the project.
