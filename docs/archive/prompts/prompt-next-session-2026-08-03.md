# 下一会话 Prompt

承接 v0.20.0.1 hotfix 后续任务。

**必读 handoff**: `F:\soft\00selfmade\rime_claude\handoff-v0.20.0.1-final-2026-08-03.md`(11 节,含技术 + 非技术总结 + Git 状态 + 必读顺序 + Prompt)

## 当前状态
- ✅ v0.20.0.1 installer 已 ship 本地: `release/fluxing-0.20.0.1-installer.exe`(md5 `b63b9b6de3d8e2a88a0ab9b843fa3847`)
- ✅ 4 commits 本地: `a429156` / `7145056` / `e3fa89f` / `e2b0f00`
- ✅ Tag `v0.20.0.1`(local,未 push)
- ✅ 5-binary mtime sync verified + 3-schema 8+8 patch verified + registry 6 项 NSIS writes verified
- ⚠️ 8/2 12:30 Claude Code 干净退出事件**未根因**(老实记录在 L##)

## 必做(按优先级)
1. **`git push kizemo v0.20.0.1`**(939 commits divergence,可能需 `--no-verify`,或 force push 解决)
2. **如果 12:30 类事件仍复现**:
   - Process Monitor 实时抓 trigger
   - 查 TSF unregister/re-register 时机
   - 不可 disable 火绒(user 之前拒绝)
3. (可选) `gh release create v0.20.0.1` + SFTP sync aiec.fun
4. (可选) 写 `_diag_5binary_mtime_check.ps1`(L##-FluxingPhrasesDialog-PathFix Future Work 提的)

## 禁止
- ❌ `git stash drop`(`stash@{0}` 是 v0.74.3 WIP,per CLAUDE.md 保留)
- ❌ Ship v0.20.0.0(broken,已被 v0.20.0.1 替代)
- ❌ Commit `env.bat`(gitignored)
- ❌ Commit `output/_extract_*/`、`*.pdb`、`*.ilk`(build artifacts)

## 关键文件
- Handoff: `handoff-v0.20.0.1-final-2026-08-03.md`
- L## 段: `lessons-learned.md` line 9800+ (PathFix) + 9900+ (12:30)
- 修复位置: `output/install.nsi` line 584
- Auto-memory: `feedback_install_nsi_path_mismatch.md`
- Build pipeline: `env.bat` (gitignored, version=v0.20.0.1)

## 上下文背景
- 这是个 5-input 中文输入法项目,叫 Fluxing(RIME 引擎的 Windows 移植 + 自家 UI)
- v0.20.0.0 是从 v0.19.0 直接升大版本号,加 Rime 用户配置 default 化功能
- 出问题的 install.nsi line 578 缺 `Win32\` 前缀,导致 NSIS bundle 旧 binary
- 修复后 v0.20.0.1 让所有 5 个 binary 都是新版本,无版本错配,无 heap corruption

session 撞墙 9h+,下次开会话严格 <2h,撞墙立即写 handoff 接力。
