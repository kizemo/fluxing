#!/usr/bin/env python3
"""validate_custom_yaml.py — Rime 补靪文件静态校验器

用法:
    python validate_custom_yaml.py <file.custom.yaml> [更多文件...]
    python validate_custom_yaml.py <文件> --data-dir E:/path/to/output/data
    python validate_custom_yaml.py <文件> --json

检查项（error = 必定是错；warning = 高概率是错）:
  E1  YAML 解析失败
  E2  根节点没有 patch: 键（.custom.yaml 必须有）
  E3  同一文件出现多个 patch: 键（后者覆盖前者，静默）
  E4  patch: 不在根节点
  E5  补靪路径语法非法（空段、未知下标算子、坏的下标）
  E6  key_binder/bindings 条目不是 map，或缺 when/accept，或动作不唯一
  E7  when 取值不在已知集合内
  E8  accept/send 里的键名不是已知 X11 keysym（大小写错等）
  E9  修饰符顺序不是 Shift → Control → Alt
  W1  使用了 @before / @after / @next（上游明确不建议在补靪中使用）
  W2  补靪节点里带 __include:（T1 修复的正确写法）——提示其优先级语义
  W3  源 schema 里同一节点是 __include 引入的，但补靪没重述 include（patch 会静默失效）
  W4  .custom.yaml 里用了 __patch:（用户层应写 patch:）
  W5  找不到对应的基文件（<name>.schema.yaml / <name>.yaml）
  W6  命中已知的危险组合（patch engine/filters 但没提 uniquifier 等）

退出码: 0 = 无 error（可能有 warning）；1 = 有 error；2 = 用法错误

Windows 控制台若输出乱码：先执行 `chcp 65001`（PowerShell 控制台默认 GBK）。
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

try:
    import yaml
except ImportError:  # pragma: no cover
    print("需要 PyYAML：pip install pyyaml", file=sys.stderr)
    sys.exit(2)


# --------------------------------------------------------------------------- #
# 已知取值表
# --------------------------------------------------------------------------- #

KNOWN_WHEN = {"always", "composing", "has_menu", "paging", "predicting"}

ACTION_KEYS = {"send", "send_sequence", "toggle", "set_option", "unset_option", "select"}

# X11 keysym 常用键名（去 XK_ 前缀）——大小写敏感
KNOWN_KEYSYMS = {
    "comma", "period", "slash", "semicolon", "apostrophe", "backslash",
    "bracketleft", "bracketright", "grave", "numbersign", "minus", "equal",
    "space", "Return", "Tab", "Escape", "BackSpace", "Delete", "Home", "End",
    "Up", "Down", "Left", "Right", "Page_Up", "Page_Down", "space",
    "exclam", "quotedbl", "numbersign", "dollar", "percent", "ampersand",
    "apostrophe", "parenleft", "parenright", "asterisk", "plus", "comma",
    "minus", "period", "slash", "colon", "semicolon", "less", "equal",
    "greater", "question", "at", "bracketleft", "backslash", "bracketright",
    "asciicircum", "underscore", "grave", "braceleft", "bar", "braceright",
    "asciitilde", "Caps_Lock", "Shift_L", "Shift_R", "Control_L", "Control_R",
    "Alt_L", "Alt_R", "Super_L", "Super_R", "Meta_L", "Meta_R", "F1", "F2",
    "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12",
}

MODIFIER_ORDER = ["Shift", "Control", "Alt"]

# 路径里的合法算子
LIST_INDEX_RE = re.compile(r"^@\d+$")
LIST_LAST_RE = re.compile(r"^@last$")
LIST_BEFORE_RE = re.compile(r"^@before \d+$")
LIST_AFTER_RE = re.compile(r"^@after \d+$")
LIST_NEXT_RE = re.compile(r"^@next$")
LIST_OPS_RE = re.compile(r"^@(.+)$")


# --------------------------------------------------------------------------- #
# YAML 载入（检测重复键）
# --------------------------------------------------------------------------- #

class DuplicateKeyLoader(yaml.SafeLoader):
    """SafeLoader，额外把重复键记进 .duplicates 而不是静默丢弃。"""

    def __init__(self, stream):
        super().__init__(stream)
        self.duplicates: list[tuple[str, str]] = []


def _construct_mapping(loader, node, deep=False):
    mapping = {}
    for key_node, value_node in node.value:
        key = loader.construct_object(key_node, deep=deep)
        if key in mapping:
            loader.duplicates.append((str(key), str(key_node.start_mark.line + 1)))
        mapping[key] = loader.construct_object(value_node, deep=deep)
    return mapping


DuplicateKeyLoader.add_constructor(
    yaml.resolver.BaseResolver.DEFAULT_MAPPING_TAG, _construct_mapping
)


# --------------------------------------------------------------------------- #
# 报告
# --------------------------------------------------------------------------- #

class Report:
    def __init__(self) -> None:
        self.items: list[dict] = []

    def add(self, level: str, code: str, msg: str, where: str = "") -> None:
        self.items.append({"level": level, "code": code, "msg": msg, "where": where})

    @property
    def errors(self) -> int:
        return sum(1 for i in self.items if i["level"] == "error")

    @property
    def warnings(self) -> int:
        return sum(1 for i in self.items if i["level"] == "warning")

    def to_dict(self) -> dict:
        return {
            "errors": self.errors,
            "warnings": self.warnings,
            "items": self.items,
        }


# --------------------------------------------------------------------------- #
# 路径语法检查
# --------------------------------------------------------------------------- #

def check_patch_path(path: str, rep: Report, where: str) -> None:
    """检查一个补靪键的路径语法。"""
    raw = path
    p = path.strip()

    if not p:
        rep.add("error", "E5", "补靪路径为空", where)
        return

    # 末尾的 ? 表示可选目标
    optional = p.endswith("?")
    if optional:
        p = p[:-1]

    # 末尾的 /+ 或 /= 操作符
    merge = p.endswith("/+")
    replace = p.endswith("/=")
    if merge:
        p = p[:-2]
    elif replace:
        p = p[:-2]

    # 跨文件引用：config:/path 或 config.yaml:/path
    if ":" in p:
        head, _, tail = p.partition(":")
        if not tail:
            rep.add("error", "E5", f"跨文件引用缺少路径：{raw!r}", where)
            return
        if not head or head.endswith("/"):
            rep.add("error", "E5", f"跨文件引用的文件名无效：{raw!r}", where)
            return
        p = tail

    segments = p.split("/")
    for idx, seg in enumerate(segments):
        if seg == "":
            rep.add("error", "E5", f"路径有空段（连续或首尾的 /）：{raw!r}", where)
            return
        if seg.startswith("@"):
            # 只允许出现在非首段
            if idx == 0:
                rep.add("error", "E5", f"列表算子不能是路径首段：{raw!r}", where)
                return
            if LIST_INDEX_RE.match(seg) or LIST_LAST_RE.match(seg):
                continue
            if LIST_BEFORE_RE.match(seg) or LIST_AFTER_RE.match(seg):
                rep.add(
                    "warning", "W1",
                    f"@{seg.split()[0]} 在补靪中不被推荐（上游明确建议避免 @before/@after/@next）",
                    where,
                )
                continue
            if LIST_NEXT_RE.match(seg):
                rep.add(
                    "warning", "W1",
                    "@next 在补靪中不被推荐；追加列表建议用 /+ ",
                    where,
                )
                continue
            rep.add(
                "error", "E5",
                f"未知的列表算子 {seg!r}（合法：@0 / @last / @before N / @after N / @next）：{raw!r}",
                where,
            )
            return

    if merge and replace:  # pragma: no cover - 逻辑上不可能同时成立
        rep.add("error", "E5", f"路径同时带 /+ 与 /=：{raw!r}", where)


# --------------------------------------------------------------------------- #
# 键名与修饰符检查
# --------------------------------------------------------------------------- #

def check_key_expr(expr: str, rep: Report, where: str, field: str) -> None:
    """检查 accept/send 里的键表达式，如 'Shift+comma'、'Page_Up'。"""
    if not expr:
        return
    parts = [p.strip() for p in expr.split("+") if p.strip()]
    if not parts:
        rep.add("error", "E8", f"{field} 为空", where)
        return

    mods = parts[:-1]
    key = parts[-1]

    # 修饰符顺序
    known_mods = [m for m in mods if m in MODIFIER_ORDER + ["Super", "Meta", "Release"]]
    idxs = [MODIFIER_ORDER.index(m) for m in known_mods if m in MODIFIER_ORDER]
    if idxs != sorted(idxs):
        rep.add(
            "error", "E9",
            f"{field}={expr!r} 修饰符顺序应为 Shift → Control → Alt",
            where,
        )
    for m in mods:
        if m not in MODIFIER_ORDER + ["Super", "Meta", "Release"]:
            rep.add("error", "E9", f"{field}={expr!r} 未知修饰符 {m!r}", where)

    # 键名
    if key in KNOWN_KEYSYMS:
        return
    # 允许单字符字面量（字母键 "x"、数字 "1"、标点 "," 等都是合法键）
    if len(key) == 1:
        return
    # 允许数字（选字）
    if key.isdigit():
        return
    rep.add(
        "error", "E8",
        f"{field}={expr!r} 的键名 {key!r} 不是已知 X11 keysym"
        f"（检查大小写：comma ✅ / Comma ❌；或改用字面字符并加引号）",
        where,
    )


def check_bindings(bindings, rep: Report, where: str) -> None:
    """检查 key_binder/bindings / editor/bindings 的条目形状。"""
    if not isinstance(bindings, list):
        rep.add("error", "E6", f"bindings 必须是列表，实际是 {type(bindings).__name__}", where)
        return
    for i, item in enumerate(bindings):
        loc = f"{where}[{i}]"
        if not isinstance(item, dict):
            rep.add(
                "error", "E6",
                f"bindings 第 {i} 项是 {type(item).__name__}，必须是 map"
                f"（写成 - {{when:…, accept:…}}）",
                loc,
            )
            continue
        when = item.get("when")
        accept = item.get("accept")
        if when is None:
            rep.add("error", "E6", "binding 缺 when", loc)
        elif when not in KNOWN_WHEN:
            rep.add(
                "error", "E7",
                f"when={when!r} 不在已知集合 {sorted(KNOWN_WHEN)} 内（写错会被静默忽略）",
                loc,
            )
        if accept is None:
            rep.add("error", "E6", "binding 缺 accept", loc)
        else:
            check_key_expr(str(accept), rep, loc, "accept")

        actions = [k for k in item if k in ACTION_KEYS]
        if not actions:
            rep.add(
                "error", "E6",
                f"binding 没有动作字段（应为 {sorted(ACTION_KEYS)} 之一）",
                loc,
            )
        elif len(actions) > 1:
            rep.add("error", "E6", f"binding 的动作字段互斥，却给了多个：{actions}", loc)
        else:
            act = actions[0]
            val = item[act]
            if act in ("send", "send_sequence"):
                seqs = str(val).split() if act == "send_sequence" else [str(val)]
                for s in seqs:
                    check_key_expr(s, rep, loc, act)


# --------------------------------------------------------------------------- #
# 主检查
# --------------------------------------------------------------------------- #

def validate_file(path: Path, data_dir: Path | None, rep: Report) -> None:
    text = path.read_text(encoding="utf-8")
    rel = path.name

    loader = DuplicateKeyLoader(text)
    try:
        data = loader.get_single_data()
    except yaml.YAMLError as e:
        rep.add("error", "E1", f"YAML 解析失败：{e}", rel)
        return
    finally:
        duplicates = list(loader.duplicates)

    if not isinstance(data, dict):
        rep.add("error", "E1", f"根节点不是 map（是 {type(data).__name__}）", rel)
        return

    for key, line in duplicates:
        if key == "patch":
            rep.add("error", "E3", f"根节点出现重复的 patch: 键（第 {line} 行），后者会静默覆盖前者", rel)
        else:
            rep.add("warning", "E3", f"重复键 {key!r}（第 {line} 行）", rel)

    is_custom = path.name.endswith(".custom.yaml")

    if "patch" not in data:
        if is_custom:
            rep.add("error", "E2", "补靪文件根节点没有 patch: 键", rel)
    else:
        patch = data["patch"]
        if not isinstance(patch, dict):
            rep.add("error", "E2", f"patch: 的值必须是 map（是 {type(patch).__name__}）", rel)
        else:
            for pkey, pval in patch.items():
                loc = f"{rel} → patch: {pkey}"
                check_patch_path(str(pkey), rep, loc)

                if isinstance(pval, dict):
                    if "__include" in pval:
                        rep.add(
                            "warning", "W2",
                            "补靪节点带 __include:（这是 include 优先级陷阱 T1 的正确修法）"
                            "——确认源 schema 里该节点确实是 __include 引入的",
                            loc,
                        )
                    if "bindings" in pval and isinstance(pval["bindings"], list):
                        check_bindings(pval["bindings"], rep, f"{loc}.bindings")
                elif isinstance(pval, list) and str(pkey).split("/")[-1] == "bindings":
                    # 补靪直接作用在列表节点上：key_binder/bindings: [ ... ]
                    check_bindings(pval, rep, loc)

            # W3：源 schema 里是 include 引入的，补靪却没重述 include
            if data_dir:
                base = find_base_file(path, data_dir)
                if base:
                    include_nodes = collect_include_nodes(base)
                    for pkey in patch:
                        if str(pkey) in include_nodes:
                            rep.add(
                                "warning", "W3",
                                f"源文件 {base.name} 里该节点是 __include 引入的，"
                                f"而补靪没有重述 __include: —— patch 会静默失效（librime T1）",
                                f"{rel} → patch: {pkey}",
                            )

    if "__patch" in data and is_custom:
        rep.add("warning", "W4", ".custom.yaml 里用了 __patch:（用户层应写 patch:）", rel)

    # W5：基文件是否存在
    if data_dir:
        if find_base_file(path, data_dir) is None:
            rep.add(
                "warning", "W5",
                f"在 {data_dir} 找不到对应的基文件"
                f"（期望 {path.name[:-len('.custom.yaml')]}.schema.yaml 或 .yaml）",
                rel,
            )

    # W6：危险组合
    if "patch" in data and isinstance(data["patch"], dict):
        joined = " ".join(str(k) for k in data["patch"])
        if "engine/filters" in joined and "uniquifier" not in text:
            rep.add(
                "warning", "W6",
                "改了 engine/filters 但全文未见 uniquifier；"
                "simplifier 必须配 uniquifier，否则同字候选会重复",
                rel,
            )


def find_base_file(custom: Path, data_dir: Path) -> Path | None:
    stem = custom.name[: -len(".custom.yaml")] if custom.name.endswith(".custom.yaml") else custom.stem
    for cand in (f"{stem}.schema.yaml", f"{stem}.yaml"):
        p = data_dir / cand
        if p.exists():
            return p
    return None


def collect_include_nodes(base: Path) -> set[str]:
    """粗扫基文件，找出形如 `__include: default:/a/b` 的节点路径。"""
    found: set[str] = set()
    try:
        text = base.read_text(encoding="utf-8")
    except OSError:
        return found
    stack: list[str] = []
    for raw in text.splitlines():
        if not raw.strip() or raw.lstrip().startswith("#"):
            continue
        m = re.match(r"^(\s*)([A-Za-z0-9_\-]+):\s*(.*)$", raw)
        if not m:
            continue
        indent, key, val = len(m.group(1)), m.group(2), m.group(3).strip()
        while stack and stack[-1][0] >= indent:
            stack.pop()
        path = "/".join([k for _, k in stack] + [key])
        if val.startswith("__include:") and ":" in val:
            ref = val.split(":", 1)[1].strip()
            if ref.startswith("default:/") or ref.startswith("config:/"):
                found.add(path)
        stack.append((indent, key))
    return found


# --------------------------------------------------------------------------- #
# CLI
# --------------------------------------------------------------------------- #

def _setup_stdout() -> None:
    """Windows 控制台默认 GBK，直接打印中文/符号会 UnicodeEncodeError 或乱码。"""
    try:
        import ctypes

        ctypes.windll.kernel32.SetConsoleOutputCP(65001)
        ctypes.windll.kernel32.SetConsoleCP(65001)
    except Exception:
        pass
    for stream in (sys.stdout, sys.stderr):
        try:
            stream.reconfigure(encoding="utf-8", errors="replace")
        except Exception:
            pass


def main() -> int:
    _setup_stdout()
    ap = argparse.ArgumentParser(
        description="校验 Rime .custom.yaml 补靪文件",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    ap.add_argument("files", nargs="+", help="要校验的 .custom.yaml 文件")
    ap.add_argument("--data-dir", type=Path, default=None,
                    help="数据目录（用于查找基文件、检测 include 冲突）")
    ap.add_argument("--json", action="store_true", help="输出 JSON")
    args = ap.parse_args()

    rep = Report()
    for f in args.files:
        p = Path(f)
        if not p.exists():
            rep.add("error", "E1", f"文件不存在：{p}", str(p))
            continue
        validate_file(p, args.data_dir, rep)

    if args.json:
        print(json.dumps(rep.to_dict(), ensure_ascii=False, indent=2))
    else:
        for item in rep.items:
            mark = {"error": "✗", "warning": "!", "info": "·"}[item["level"]]
            loc = f"  [{item['where']}]" if item["where"] else ""
            print(f"{mark} {item['code']}  {item['msg']}{loc}")
        print(f"\n{rep.errors} error(s), {rep.warnings} warning(s)")

    return 1 if rep.errors else 0


if __name__ == "__main__":
    sys.exit(main())
