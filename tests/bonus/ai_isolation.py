#!/usr/bin/env python3
"""Bonus-race acceptance: rivals run every track without AI↔AI contact; DNF closes results."""
from __future__ import annotations

import json
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
B = ROOT / "build/bonus"
OUT = ROOT / "build/gates/bonus/ai-isolation"
EXE = OUT / "race_host"


def run_case(track: int, reverse: bool = False, collide: bool = False) -> tuple[int, dict]:
    tag = f"b{track:02d}-{'r' if reverse else 'f'}{'-red' if collide else ''}"
    report = OUT / f"{tag}.json"
    cmd = [str(EXE), str(B / f"b{track:02d}.shr"), str(B / f"b{track:02d}.bns"), str(B / "msncars-b.shr"),
           "--msndriver", "--laps", "1", "--secs", "240", "--json", str(report)]
    if reverse:
        cmd.append("--reverse")
    if collide:
        cmd.append("--npc-collide")
    p = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)
    if not report.is_file():
        raise RuntimeError(f"race_host produced no JSON for {tag}: {p.stderr[-500:]}")
    return p.returncode, json.loads(report.read_text())


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    # №1147: соперники теперь сталкиваются (удары между ними — tests/g03_1147); этот гейт держит прежнее решение g07b «сквозь друг друга» под AIAI1147_OLD=1
    subprocess.run([str(ROOT / "tools/bonus/build_host.sh"), str(EXE)], cwd=ROOT, check=True, env=dict(os.environ, EXTRA="-DAIAI1147_OLD=1"))
    for track in range(1, 8):
        for reverse in (False, True):
            rc, report = run_case(track, reverse)
            racers = report["racers"]
            finishers = sum(bool(r["finished"]) and r["laps"] >= 1 for r in racers)
            contacts = sum(r["cars"] for r in racers)
            if finishers < 1 or contacts:
                raise AssertionError(f"b{track:02d} {'reverse' if reverse else 'forward'}: finishers={finishers}, AI hits={contacts}, rc={rc}")
            print(f"b{track:02d} {'reverse' if reverse else 'forward'}: finisher={finishers}/4, AI hits={contacts}")

    _, red = run_case(3, collide=True)
    red_hits = sum(r["cars"] for r in red["racers"])
    if red_hits < 1:
        raise AssertionError("negative control --npc-collide did not create a detectable AI hit")
    print(f"negative control: caught {red_hits} AI collision events")

    collision = OUT / "collision_host"
    subprocess.run(["cc", "-std=c11", "-O2", "-DSKIN_HOST", "-I", str(ROOT / "viewer"),
                    str(ROOT / "tests/bonus/collision_host.c"), str(ROOT / "viewer/msncar.c"),
                    str(ROOT / "viewer/car.c"), str(ROOT / "viewer/col.c"), str(ROOT / "viewer/traffic.c"),
                    str(ROOT / "viewer/homer.c"), str(ROOT / "viewer/skin.c"), "-lm", "-o", str(collision)],
                   cwd=ROOT, check=True)
    subprocess.run([str(collision)], cwd=ROOT, check=True)
    msn = (ROOT / "viewer/msncar.c").read_text()
    if msn.index("if (e->player && e->driving") > msn.index("if (e->noNpcCollide) continue;"):
        raise AssertionError("bonus isolation guard moved ahead of player↔rival collision handling")

    dnf_src = ["cc", "-std=c11", "-O2", "-DSKIN_HOST", "-I", str(ROOT / "viewer"),
               str(ROOT / "tests/bonus/dnf_host.c"), str(ROOT / "viewer/bonus.c"), str(ROOT / "viewer/col.c"),
               str(ROOT / "viewer/car.c"), str(ROOT / "viewer/traffic.c"), str(ROOT / "viewer/homer.c"),
               str(ROOT / "viewer/skin.c"), str(ROOT / "viewer/props.c"), str(ROOT / "viewer/stateprops.c"),
               str(ROOT / "viewer/animobj.c"), str(ROOT / "viewer/coins.c"), "-lm"]
    dnf = OUT / "dnf_host"
    subprocess.run(dnf_src + ["-o", str(dnf)], cwd=ROOT, check=True)
    subprocess.run([str(dnf), str(B / "b01.bns")], cwd=ROOT, check=True)
    red_dnf = OUT / "dnf_host_red"
    subprocess.run(dnf_src[:2] + ["-DBR_DNF_S=1200.f"] + dnf_src[2:] + ["-o", str(red_dnf)], cwd=ROOT, check=True)
    red_run = subprocess.run([str(red_dnf), str(B / "b01.bns")], cwd=ROOT, capture_output=True, text=True)
    if red_run.returncode == 0:
        raise AssertionError("negative DNF control did not fail")
    print("negative DNF control: caught delayed result close")
    print("bonus AI isolation gate: PASS")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as exc:
        print(f"bonus AI isolation gate: FAIL: {exc}", file=sys.stderr)
        sys.exit(1)
