# 031 · Plan · 候选字屏蔽（stage 3 of spec 008 — 客户端过滤）

## 1. 技术上下文

- **C++ 17 / WTL / ATL**（与 WeaselUI 同栈）。
- **修改文件**（2 个）：
  - `WeaselUI/WeaselPanel.h` — 加 `m_ignoreList` + `m_ignoreFilePath` 私有成员 + `_IgnoreCurrentCandidate` / `_FilterIgnoredCandidates` 私有方法声明。
  - `WeaselUI/WeaselPanel.cpp` — 加 impl + `OnRButtonDown` 扩展 + `OnCreate` / `Update` 末尾调 load / filter。
- **新建**（3 个）：
  - `test/TestCandidateIgnoreFilter/TestCandidateIgnoreFilter.cpp` — 5 真实 assertions。
  - `test/TestCandidateIgnoreFilter/TestCandidateIgnoreFilter.vcxproj` — 9 号 test project。
  - `weasel.sln` 加 `TestCandidateIgnoreFilter` 节点。
- **不引入新依赖**；weasel.sln 加 1 单元。
- **不修改** `output/install.nsi`（不 release installer）。
- **不修改** `env.bat` / `weasel.props`（bookkeeping sub-release）。

## 2. Architecture

```
User right-clicks candidate[index=hover] with text.length < 2 (or is_user_dict false)
       |
       v
Windows OS -> WM_RBUTTONDOWN -> WeaselPanel::OnRButtonDown (spec 030 impl, extended)
       |
       v
判定 text.length >= 2 -> delete (spec 030 走 m_deleteCallback)
判定 text.length <  2 -> ignore (本 spec 走 _IgnoreCurrentCandidate)
       |
       v
_IgnoreCurrentCandidate:
  1. m_ignoreList.insert(text)
  2. CreateFileW(<user_ignore.txt>, FILE_APPEND_DATA) + WriteFile(text + "\r\n")
  3. RedrawWindow() -> Update() 末尾调 _FilterIgnoredCandidates -> m_ctx.cinfo.candies 过滤
```

**端到端数据流只走 WeaselPanel 一个类**。IPC / WeaselTSF / RimeWithWeasel 完全不参与。

## 3. Constitution Check

| Rule | Status | Notes |
|---|---|---|
| I. Intent | OK | spec 031 §0 明确 fallback 方案 B |
| II. Test | OK | TestCandidateIgnoreFilter 5 真实 assertions |
| III. Spec-Artifact | OK | 3 件套齐全 |
| IV. Clarification | OK | spec 008 §1.1 简化规则 (text.length 判定) 用户已接受 (spec 028 L35) |
| V. Incremental | OK | spec 008 stage 3 独立 ship，零引擎依赖 |
| R1-R9 | OK | 引用 L22/L23/L25/L28/L31/L35/L37 |
| P1-P8 | OK | P8 brand-fork scope |
| L36 | OK | LoadIgnoreList 同时 verify 已有 text file reader 是否也用 byte-level + BOM |
| L37 | OK | 写文件用 CreateFileW + WriteFile (byte-level)，不用 ofstream<< (LF trap) |

## 4. 风险

- **R1**：spec 008 §1.1 规则简化（text.length 判定）已在 spec 028 接受。本 spec 不改变。
- **R2**：user_ignore.txt 持久化但**无 UI 入口编辑** — 卸载/换 user dir 会丢。spec 008 T008 "恢复" 按钮 deferred。
- **R3**：schema 切换后路径更新 deferred 到 spec 008 T007（暗色主题订阅一起做）。
- **R4**：运行时 schema 切换不重 load ignore list — 同 input session 有效，跨 session 失效（除非重启 WeaselPanel）。可接受。

## 5. 验证步骤

1. `cmd /c xbuild.bat weasel` → 0 errors。
2. `cmd /c scripts\test-infra\run-test-suite.bat` → 9/9 PASS。
3. `cmd /c scripts\test-infra\verify-test-binaries-fresh.bat` → 9/9 binaries fresh。
4. AGENTS.md sec 5 五步 pre-commit gate。
5. 不做 NSIS smoke test（不改 install.nsi）。

## 6. Release

- **version bump**：无（bookkeeping sub-release）。
- **tag**：无。
- **commit**：`feat(fluxing): spec 031 - candidate ignore filter (stage 3 of 008)`。
- **push**：`git push kizemo Fluxing`。

## 7. References

- spec 008 / 028 / 030 / TDD §3.1
- `WeaselUI/WeaselPanel.cpp:285-345` (OnMouseWheel / OnLeftClickedUp 模板)
- `WeaselUI/WeaselPanel.cpp:1147,1156` (m_hoverIndex = -1 重置点)
- L35 (librime is_user_dict 不可达)
- L37 (PowerShell line-based trap)
- AGENTS.md sec 5 pre-commit gate