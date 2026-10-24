# Prompt — v0.21.0.1 closure · install.nsi 修复 + T20 完结

**承接 handoff**: `handoff-v0.21.0.1-closure-2026-08-17.md`(请先读完 §5 非技术语言目标 + §2.2 install.nsi 根因)

**基线**:
- rime_claude Fluxing HEAD = `0184481`(已 push 远程 kizemo/Fluxing,本地/远程 SHA 匹配)
- sandbox-verify main HEAD = `1364128`(L54 cleanup 扩展已 commit)
- 工作树脏文件(`M CLAUDE.md` + `m librime` + ~150 untracked)**不属于本任务,严禁纳入 commit**

**下阶段必交付(按优先级)**:
1. 修 `output/install.nsi:243-291` 强制覆盖逻辑(删除 `StrCpy $INSTDIR $R0` 或加 guard 让 /D= 始终生效)
2. 本地 rebuild(`_build_v02101.ps1` 或 `_build_v0742.ps1` 加 `/DFLUXING_VERSION=0.21.0.2`)
3. 用新 installer 重跑 `rime-verify.ps1 -NoWait`,等 verify.log 稳定
4. 8 场景 §a-§h 全绿(只 §9 GUI install 会 fail,预期)+ 写 spec 077 completion 报告
5. 新 commit(`fix(WeaselSetup): honor /D= override`,per CLAUDE.md §2 module prefix)+ 推送

**禁止事项**:
- 不动 `librime/` 子模块(P3)
- 不把 `weasel.props` / `env.bat` / `*.obj` 纳入 commit
- 不在 hotfix_v0.67_patch_AB/ 下做改动(历史工作目录)
- 不开新功能开发(spec 077 已 ship)
- 不改 `.gitignore` 把 installer 文件纳入(沿用 release/ 目录约定)

**Win11 LogonCommand flake 应对**(每 5 分钟无 log 即触发):
```powershell
Stop-Service vmcompute -Force
Get-Process -Name vmmemWindowsSandbox -ErrorAction SilentlyContinue | ForEach-Object {
    cmd /c "taskkill /F /PID $($_.Id)" 2>$null
}
Start-Service vmcompute; Start-Sleep 5
# 重 invoke wrapper
```

**加速模式**:无(本次涉及 install.nsi 源码改 + rebuild,需 TDD + 验证节奏,不豁免).

**新会话第一句话**: 直接引用本 prompt 即可,不需要重读历史对话.
