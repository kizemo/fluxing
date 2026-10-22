"""Top-N largest blobs across all refs, with their path hints.

Usage: python top-blobs.py <repo> [N]
Runs: git rev-list --objects --all | git cat-file --batch-check
Parses in Python (the batch-check output contains spaces in the path part,
which makes it un-sortable with naive PowerShell field splitting).
"""
import subprocess
import sys
from collections import defaultdict

repo = sys.argv[1]
top_n = int(sys.argv[2]) if len(sys.argv) > 2 else 30

rev_list = subprocess.run(
    ["git", "-C", repo, "rev-list", "--objects", "--all"],
    capture_output=True, check=True).stdout.decode("utf-8", "replace")

# object-id -> first path seen (rev-list lists each object once)
oid_path = {}
for line in rev_list.splitlines():
    parts = line.split(" ", 1)
    if len(parts) == 2:
        oid_path.setdefault(parts[0], parts[1])

oids = "\n".join(oid_path.keys()) + "\n"
proc = subprocess.run(
    ["git", "-C", repo, "cat-file", "--batch-check=%(objecttype) %(objectname) %(objectsize)"],
    input=oids.encode(), capture_output=True, check=True).stdout.decode("utf-8", "replace")

rows = []
for line in proc.splitlines():
    parts = line.split(" ")
    if len(parts) != 3:
        continue
    otype, oid, size = parts
    if otype != "blob":
        continue
    rows.append((int(size), oid, oid_path.get(oid, "?")))

rows.sort(reverse=True)

# aggregate by top-level directory for a per-directory view
agg = defaultdict(lambda: [0, 0])
for size, _oid, path in rows:
    key = path.split("/")[0] if "/" in path else path
    agg[key][0] += size
    agg[key][1] += 1

print("=== top {} blobs ===".format(top_n))
total = 0
for size, oid, path in rows[:top_n]:
    total += size
    print("{:>12,}  {}  {}".format(size, oid[:10], path))
print("top-{} sum = {:,} bytes ({:.1f} MB)".format(
    top_n, total, total / 1048576.0))

print()
print("=== bytes by top-level path (all blobs) ===")
agg_rows = sorted(agg.items(), key=lambda kv: kv[1][0], reverse=True)
grand = 0
for key, (size, count) in agg_rows[:25]:
    grand += size
    print("{:>12,}  {:>5} files  {}".format(size, count, key))
print("top-25 dirs sum = {:,} bytes ({:.1f} MB)".format(grand, grand / 1048576.0))
