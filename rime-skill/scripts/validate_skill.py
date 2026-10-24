#!/usr/bin/env python3
"""validate_skill.py - 自检 rime-skill 是否符合 skill-forge 的 frontmatter / body 原则。

用法:
    python scripts/validate_skill.py                    # 自检技能自身（默认取本脚本上级目录）
    python scripts/validate_skill.py <技能目录>          # 自检任意技能目录

依赖:
    skill-forge 的 scripts/_lib/frontmatter.py 必须在 sys.path 上。
    默认从 SKILL_FORGE_HOME 环境变量或 F:/soft/00selfmade/skill-optimizer/skill-forge 找。

退出码:
    0 = 无告警
    1 = 有告警（frontmatter 或 body）
"""
import os
import sys
from pathlib import Path

SKILL_FORGE_HOME = os.environ.get(
    "SKILL_FORGE_HOME", r"F:/soft/00selfmade/skill-optimizer/skill-forge"
)
sys.path.insert(0, SKILL_FORGE_HOME)

try:
    from scripts._lib.frontmatter import (  # noqa: E402
        load,
        body_of,
        validate_skill_frontmatter,
        validate_skill_body,
    )
except ImportError as exc:  # pragma: no cover - 环境问题直接暴露
    print(f"[FAIL] 无法导入 skill-forge 校验器 ({SKILL_FORGE_HOME}): {exc}")
    raise SystemExit(2)

LIMITS = {
    "description_words": 50,
    "description_chars": 1024,
    "body_lines": 150,
    "body_words": 1000,
}


def main() -> int:
    if len(sys.argv) > 1:
        skill_dir = Path(sys.argv[1]).resolve()
    else:
        skill_dir = Path(__file__).resolve().parent.parent

    skill_md = skill_dir / "SKILL.md"
    if not skill_md.exists():
        print(f"[FAIL] 找不到 {skill_md}")
        return 2

    fm = load(skill_md)
    fw = validate_skill_frontmatter(fm, skill_dir)
    bw = validate_skill_body(skill_md, fm)

    body = body_of(skill_md).strip()
    desc = str(fm.get("description", ""))
    triggers = fm.get("triggers", []) or []

    print(f"技能目录            : {skill_dir}")
    print(f"frontmatter warnings: {fw if fw else 'NONE'}")
    print(f"body warnings       : {bw if bw else 'NONE'}")
    print(f"name                : {fm.get('name')}")
    print(
        f"description words   : {len(desc.split())} / {LIMITS['description_words']}"
    )
    print(
        f"description chars   : {len(desc)} / {LIMITS['description_chars']}"
    )
    print(f"triggers count      : {len(triggers)}")
    print(f"mutating            : {fm.get('mutating')} ({type(fm.get('mutating')).__name__})")
    print(f"brain_first         : {fm.get('brain_first')}")
    print(f"version             : {fm.get('version')}")
    print(f"when_to_use         : {'present' if fm.get('when_to_use') else 'MISSING'}")
    print(f"body lines          : {len(body.splitlines())} / {LIMITS['body_lines']}")
    print(f"body words(wc -w)   : {len(body.split())} / {LIMITS['body_words']}")
    print(f"body chars          : {len(body)}")

    ok = not fw and not bw
    print("")
    print("结论: " + ("PASS — 无告警" if ok else "WARN — 见上方告警"))
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
