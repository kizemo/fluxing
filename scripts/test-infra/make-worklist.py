"""Generate the human-rewrite worklist for one damaged tracked document.

For every damaged line it emits: the line number, the section heading it
belongs to, the damaged text with ASCII markers for the lost characters, the
clean context around it, and the closest historical candidate (when the
scanner found one) so the rewriter can judge whether the candidate is the
same line.

Output is UTF-8 markdown with ASCII markers, so it survives the GBK console
and can be read by a downstream agent.
"""
import importlib.util
import sys
from pathlib import Path

scanner_path, repo, target, out_path = sys.argv[1:5]
baseline_sha = sys.argv[5] if len(sys.argv) > 5 else ""

spec = importlib.util.spec_from_file_location("scanmod", scanner_path)
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)

raw = Path(repo, target).read_bytes()
text = raw.decode("utf-8", "replace")
lines = text.split("\n")
stripped = [ln[:-1] if ln.endswith("\r") else ln for ln in lines]

revs = mod.revisions(repo, target)
corpus = []
for rev in revs:
    hist = mod.historical_lines(repo, target, rev)
    if hist:
        corpus.append((rev, hist))
corpus.reverse()
index = mod.build_index(corpus)


def marked(s: str) -> str:
    return s.replace("\ufffd", "[FFFD]").replace("\x00", "[NUL]").replace("\ufeff", "[BOM]")


def heading_for(n: int) -> str:
    for i in range(n - 1, -1, -1):
        s = stripped[i]
        if s.startswith("## ") or s.startswith("### "):
            return s
    return "(no heading above)"


out = [
    "# human-rewrite worklist - {}".format(target),
    "",
    "- total lines: {}".format(len(lines)),
    "- damaged lines: {}".format(sum(1 for ln in stripped if mod.is_damaged(ln))),
    "",
    "Markers: `[FFFD]` = a character that was destroyed (not recoverable from",
    "bytes), `[NUL]` = a character replaced by a zero byte, `[BOM]` = a stray",
    "U+FEFF that is an artefact, not content.",
    "",
    "Rewrite rules (from .specify/specs/078-repair-encoding-corruption/spec.md",
    "section 4):",
    "",
    "1. Never invent. Every rewrite is grounded in the surviving ASCII of the",
    "   line, the clean context around it, and (when present) the historical",
    "   candidate below.",
    "2. Keep English paragraphs, code identifiers, file names, version numbers,",
    "   commit ids, hashes and test counts exactly as they are.",
    "3. Do not add or remove lines, do not reorder.",
    "4. Use the repository's existing terminology (CONTEXT.md section 3).",
    "5. If the evidence is not enough to reconstruct the original wording,",
    "   write the most accurate statement the anchors support - and say so in",
    "   the audit table. Do not invent facts such as commit ids or counts.",
    "",
    "---",
    "",
]

damaged = [i for i, ln in enumerate(stripped) if mod.is_damaged(ln)]
for n, idx in enumerate(damaged, 1):
    cur = stripped[idx]
    out.append("## line {}".format(idx + 1))
    out.append("")
    out.append("- section: {}".format(heading_for(idx)))
    out.append("- damaged: `{}`".format(marked(cur)))
    lo = max(0, idx - 3)
    hi = min(len(stripped), idx + 4)
    out.append("- context:")
    for j in range(lo, hi):
        if j == idx:
            continue
        tag = ">" if mod.is_damaged(stripped[j]) else " "
        out.append("  {} [{}] {}".format(tag, j + 1, marked(stripped[j])))
    match = mod.best_match(cur, index)
    cand = None
    if match is not None and match[2] >= 0.99:
        cand = (match[0], match[1])
    else:
        partial = mod.partial_match(cur, index)
        if partial is not None:
            cand = (None, partial[0])
    if cand:
        rev, c = cand
        restored = mod.strip_bom_artifacts(mod.restore_candidate(c))
        out.append("- historical candidate{}:".format(
            " (rev {})".format(rev[:10]) if rev else ""))
        out.append("  - as stored: `{}`".format(marked(c)))
        if restored != c:
            out.append("  - after undoing GBK misread: `{}`".format(marked(restored)))
    out.append("")

Path(out_path).write_text("\n".join(out), encoding="utf-8")
print("worklist written: {} ({} damaged lines)".format(out_path, len(damaged)))
