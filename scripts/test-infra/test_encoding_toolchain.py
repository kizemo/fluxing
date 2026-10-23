#!/usr/bin/env python3
"""Regression tests for the encoding-damage toolchain.

Run:  python scripts/test-infra/test_encoding_toolchain.py
Exit 0 = every assertion held.

The cases below pin the three behaviours that changed on 2026-10-23:

1. The GBK round-trip detector must not report a clean Chinese line as
   damaged.  ``状态`` round trips to ``״̬`` purely by coincidence, and
   before the repertoire check it was reported as damage -- and
   ``--apply-roundtrip`` would have rewritten the correct text into
   nonsense.
2. A real GBK misread must still be reported, so the fix above does not
   silently disable the detector.
3. A CR byte can itself be the damage (the ``\\r`` escape was un-escaped
   into a raw CR that swallowed the letter forming it).  The applier must
   accept such a rewrite and the verifier must accept the resulting CR
   count change.
"""
from __future__ import annotations

import importlib.util
import json
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

failures = []


def check(name, cond, detail=""):
    if cond:
        print("  PASS: {}".format(name))
    else:
        failures.append(name)
        print("  FAIL: {}{}".format(name, ("  <- " + detail) if detail else ""))


print("1. GBK round trip must not fire on clean Chinese")
# the repertoire a PRD.md-like document would publish
scan.set_repertoire(set("### 10.5 spec 037 spec / plan / tasks 状态 ASCII text\n"))
clean = "### 10.5 spec 037 spec / plan / tasks 状态"
check("clean Chinese line is not damaged", not scan.is_damaged(clean))
check("clean Chinese line has no round trip", scan.gbk_misread(clean) is None)

print("2. a real GBK misread must still be detected")
# the original text was "§" (section sign); it was read as GBK, which turned
# it into the CJK character "搂" and wrote that back as UTF-8
misread = "搂"
scan.set_repertoire(set("§ Chinese text\n"))
check("real misread is detected", scan.is_damaged(misread))
check("real misread round trips to the original",
      scan.gbk_misread(misread) == "§")

print("3. the control-character classes are unchanged")
scan.set_repertoire(set("accept ascii_composer add_shflags vcvars32 aiec.fun\n"))
check("BEL is damage", scan.is_damaged("\x07dd_shflags"))
check("plain ASCII is not damage", not scan.is_damaged("add_shflags"))
check("U+FFFD is damage", scan.is_damaged("a�b"))
check("NUL is damage", scan.is_damaged("a\x00b"))

print("4. the applier accepts a rewrite that removes a CR byte")
with tempfile.TemporaryDirectory() as td:
    target = Path(td) / "sample.md"
    target.write_bytes(b"- \x0drelease/x.exe\r\n- second\r\n")
    m = Path(td) / "m.json"
    m.write_text(json.dumps({"lines": {"1": "- release/x.exe"}}), encoding="utf-8")
    r = subprocess.run(
        ["python", str(REPO / "scripts" / "test-infra" / "apply-line-rewrites.py"),
         "--repo", str(td), "--target", "sample.md", "--map", str(m)],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    out = r.stdout.decode("utf-8", "replace")
    check("applier accepted the CR removal", r.returncode == 0, out.strip())
    check("applier wrote the repaired line",
          target.read_bytes() == b"- release/x.exe\r\n- second\r\n",
          repr(target.read_bytes()))

print("5. the applier still refuses a rewrite that changes the line count")
with tempfile.TemporaryDirectory() as td:
    target = Path(td) / "sample.md"
    target.write_bytes(b"- one\n- two\n")
    m = Path(td) / "m.json"
    m.write_text(json.dumps({"lines": {"1": "- one\n- injected"}}), encoding="utf-8")
    r = subprocess.run(
        ["python", str(REPO / "scripts" / "test-infra" / "apply-line-rewrites.py"),
         "--repo", str(td), "--target", "sample.md", "--map", str(m)],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    check("applier refused the line-count change", r.returncode != 0,
          r.stdout.decode("utf-8", "replace").strip())

print("6. the TAB class detector finds an un-escaped backslash-t")
TAB = chr(9)
check("inline TAB damage is detected",
      scan.inline_tab_damage(["- build " + TAB + "est/x.cpp now"]) == [0],
      repr(scan.inline_tab_damage(["- build " + TAB + "est/x.cpp now"])))
check("leading TAB damage is detected",
      scan.inline_tab_damage([TAB + "est/TestX.cpp pins it down"]) == [0],
      repr(scan.inline_tab_damage([TAB + "est/TestX.cpp pins it down"])))
check("a repaired line is clean",
      scan.inline_tab_damage(["- build test/x.cpp now"]) == [])

print("7. the TAB class detector does not fire on legitimate TABs")
check("a TSV data block is exempt",
      scan.inline_tab_damage(["#@/db_name" + TAB + "rime_ice",
                               "#@/db_type" + TAB + "userdb"]) == [])
check("an inline code span is exempt",
      scan.inline_tab_damage(["- OLD: `PASS" + TAB + "` transcript"]) == [])
check("a fenced code block is exempt",
      scan.inline_tab_damage(["```", "int x = a" + TAB + "b;", "```"]) == [])
check("indentation-only TAB is exempt",
      scan.inline_tab_damage([TAB + "indented list item"]) == [])
check("the - OLD: transcript marker is exempt",
      scan.inline_tab_damage(["- OLD: `abc`" + TAB + " def"]) == [])

print("8. the applier repairs a TAB class line and keeps the line ending")
for label, eol in (("LF", b"\n"), ("CRLF", b"\r\n")):
    with tempfile.TemporaryDirectory() as td:
        target = Path(td) / "sample.md"
        target.write_bytes(b"- build " + TAB.encode() + b"est/x.cpp now" + eol
                           + b"- second" + eol)
        m = Path(td) / "m.json"
        m.write_text(json.dumps({"lines": {"1": "- build test/x.cpp now"}}),
                     encoding="utf-8")
        r = subprocess.run(
            ["python", str(REPO / "scripts" / "test-infra" / "apply-line-rewrites.py"),
             "--repo", str(td), "--target", "sample.md", "--map", str(m)],
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        out = r.stdout.decode("utf-8", "replace")
        check("applier accepted the TAB repair in a {} file".format(label),
              r.returncode == 0, out.strip())
        want = b"- build test/x.cpp now" + eol + b"- second" + eol
        check("the {} file kept its own line ending".format(label),
              target.read_bytes() == want, repr(target.read_bytes()))

print("9. the applier refuses a replacement that still carries damage")
with tempfile.TemporaryDirectory() as td:
    target = Path(td) / "sample.md"
    target.write_bytes(b"- one\n- two\n")
    m = Path(td) / "m.json"
    m.write_text(json.dumps({"lines": {"1": "- on�e"}}), encoding="utf-8")
    r = subprocess.run(
        ["python", str(REPO / "scripts" / "test-infra" / "apply-line-rewrites.py"),
         "--repo", str(td), "--target", "sample.md", "--map", str(m)],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    check("applier refused the U+FFFD replacement", r.returncode != 0,
          r.stdout.decode("utf-8", "replace").strip())

print("10. the verifier baselines on HEAD while the repair is uncommitted")
with tempfile.TemporaryDirectory() as td:
    subprocess.run(["git", "init", "-q", td], check=True)
    subprocess.run(["git", "-C", td, "config", "user.email", "t@t"], check=True)
    subprocess.run(["git", "-C", td, "config", "user.name", "t"], check=True)
    # keep the bytes exactly as they are: on Windows a text mode write turns
    # the LF into CRLF, which would make every line differ from the blob
    subprocess.run(["git", "-C", td, "config", "core.autocrlf", "false"],
                   check=True)
    doc = Path(td) / "doc.md"
    doc.write_bytes(b"line one\nline two\n")
    subprocess.run(["git", "-C", td, "add", "-A"], check=True)
    subprocess.run(["git", "-C", td, "commit", "-qm", "x"], check=True)
    # the repair: the working tree now differs from HEAD
    doc.write_bytes(b"line one fixed\nline two\n")
    m = Path(td) / "m.json"
    m.write_text(json.dumps({"lines": {"1": "line one fixed"}}), encoding="utf-8")
    r = subprocess.run(
        ["python", str(REPO / "scripts" / "test-infra" / "verify-encoding-repair.py"),
         "--repo", str(td), "--target", "doc.md", "--map", str(m)],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    out = r.stdout.decode("utf-8", "replace")
    check("the baseline is HEAD for an uncommitted repair",
          "before rev : HEAD" in out, out[:500])
    check("the verifier passes on the repaired file",
          r.returncode == 0 and "ALL CHECKS PASSED" in out, out[-500:])

print("11. the verifier finds the scanner even when it checks another tree")
with tempfile.TemporaryDirectory() as td:
    subprocess.run(["git", "init", "-q", td], check=True)
    subprocess.run(["git", "-C", td, "config", "user.email", "t@t"], check=True)
    subprocess.run(["git", "-C", td, "config", "user.name", "t"], check=True)
    subprocess.run(["git", "-C", td, "config", "core.autocrlf", "false"],
                   check=True)
    (Path(td) / "doc.md").write_bytes(b"line one\nline two\n")
    subprocess.run(["git", "-C", td, "add", "-A"], check=True)
    subprocess.run(["git", "-C", td, "commit", "-qm", "x"], check=True)
    m = Path(td) / "m.json"
    m.write_text(json.dumps({"lines": {"1": "line one"}}), encoding="utf-8")
    r = subprocess.run(
        ["python", str(REPO / "scripts" / "test-infra" / "verify-encoding-repair.py"),
         "--repo", str(td), "--target", "doc.md", "--map", str(m)],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    out = r.stdout.decode("utf-8", "replace")
    check("the scanner was found in the tool repository",
          "scanner exit code 2" not in out, out[-600:])

print("")
if failures:
    print("FAILED: {}".format(", ".join(failures)))
    raise SystemExit(1)
print("ALL TESTS PASSED")
