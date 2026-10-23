#!/usr/bin/env python3
"""Acceptance gate for the TAB class damage detector.

TAB is the one C0 control that can be legitimate -- it indents every
C/C++ source file and separates the fields of the rime userdb header.  A
``\\t`` escape that was un-escaped into a real TAB therefore leaves a line
that looks like ordinary text to a byte scan, which is why this detector
needs document context.

The gate checks both directions:

  forward   every tracked markdown in the tree must report nothing;
  reverse   the damaged revisions of the known affected files must report
            exactly the lines that were repaired -- no more, no less.

Run:  python scripts/test-infra/verify-tab-detector.py
Exit 0 = the detector is exact on this repository.
"""
from __future__ import annotations

import importlib.util
import os
import subprocess
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

spec = importlib.util.spec_from_file_location(
    "scan", REPO / "scripts" / "test-infra" / "scan-encoding-damage.py")
scan = importlib.util.module_from_spec(spec)
spec.loader.exec_module(scan)

# The revision that still carries the damage: the parent of the commit that
# landed the spec 079 repair.  It is pinned on purpose -- the gate has to
# keep working no matter how many commits land after it.
DAMAGED_REV = "93b61c0"

# file -> (TAB class lines, total damaged lines) in the damaged revision
DAMAGED = {
    ".specify/specs/014-restore-shift-select-2nd-3rd-candidate/plan.md": (5, 11),
    ".specify/memory/lessons-learned.md": (5, 5),
    ".specify/specs/015-fix-test-infrastructure/tasks.md": (1, 1),
    ".specify/specs/037-fluxing-components-v0/tasks.md": (4, 4),
    "CHANGELOG.md": (4, 4),
}

failures = []


def check(name, cond, detail=""):
    if cond:
        print("  PASS: {}".format(name))
    else:
        failures.append(name)
        print("  FAIL: {}{}".format(name, ("  <- " + detail) if detail else ""))


def tracked_markdown():
    raw = subprocess.run(["git", "-C", str(REPO), "ls-files"],
                         stdout=subprocess.PIPE).stdout.decode("utf-8", "replace")
    out = []
    for rel in raw.split("\n"):
        if not rel or not rel.lower().endswith(".md"):
            continue
        if rel.startswith("librime/") or rel.startswith("output/data/"):
            continue
        out.append(rel)
    return out


print("1. forward: no TAB class damage in the current tree")
files = tracked_markdown()
hits = 0
for rel in files:
    raw = (REPO / rel).read_bytes()
    if b"\x00" in raw[:4096]:
        continue
    try:
        text = raw.decode("utf-8")
    except UnicodeDecodeError:
        continue
    lines = [ln[:-1] if ln.endswith("\r") else ln for ln in text.split("\n")]
    for n in scan.inline_tab_damage(lines):
        hits += 1
        print("     {} L{}".format(rel, n + 1))
check("{} tracked markdown files report nothing".format(len(files)), hits == 0,
      "{} hit(s)".format(hits))

print("2. reverse: the damaged revisions report exactly their damage")
tmp = tempfile.mkdtemp(prefix="tabdetector_")
subprocess.run(["git", "init", "-q", tmp], check=True)
# keep the bytes exactly as they are committed
subprocess.run(["git", "-C", tmp, "config", "core.autocrlf", "false"], check=True)
for rel in DAMAGED:
    blob = subprocess.run(["git", "-C", str(REPO), "show",
                           "{}:{}".format(DAMAGED_REV, rel)],
                          stdout=subprocess.PIPE).stdout
    if not blob:
        raise SystemExit("revision {} does not carry {}".format(
            DAMAGED_REV, rel))
    dst = os.path.join(tmp, rel.replace("/", os.sep))
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    with open(dst, "wb") as fh:
        fh.write(blob)
subprocess.run(["git", "-C", tmp, "add", "-A"], check=True)
subprocess.run(["git", "-C", tmp, "-c", "user.email=t@t", "-c", "user.name=t",
                "commit", "-qm", "damaged snapshot"], check=True)

for rel, (want_tab, want_all) in sorted(DAMAGED.items()):
    r = subprocess.run(
        ["python", str(REPO / "scripts" / "test-infra" / "scan-encoding-damage.py"),
         "--repo", tmp, "--path", rel],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    out = r.stdout.decode("utf-8", "replace")
    summary = [ln for ln in out.splitlines() if ln.startswith("damage-scan:")]
    line = summary[0] if summary else ""
    got_tab = int(line.split("tab=")[1].split()[0]) if "tab=" in line else -1
    got_all = int(line.split("damaged_lines=")[1].split()[0]) \
        if "damaged_lines=" in line else -1
    check("{} reports tab={} damaged={}".format(rel, want_tab, want_all),
          got_tab == want_tab and got_all == want_all and r.returncode != 0,
          line)

print("")
if failures:
    print("FAILED: {}".format(", ".join(failures)))
    raise SystemExit(1)
print("ALL CHECKS PASSED")
