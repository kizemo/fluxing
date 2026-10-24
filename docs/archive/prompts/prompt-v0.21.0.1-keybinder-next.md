承接 `handoff-v0.21.0.1-keybinder-sendtext-2026-08-10.md`(项目根,必读 §6 列了 8 个真相源)。

**基线**: Fluxing HEAD `3ab771c6fa...`,divergence 0/0,librime 1.13.1 pin `1c233581`,installer `release/fluxing-0.21.0.0-installer.exe` md5 `a43c72e7a8bf66b270ad34bef8846030`。

**必交付 (P1)**: v0.21.0.1 ship — librime 1.13.1 fork patch 加 `send_text` 字段 → rebuild librime → rebuild Fluxing weasel → installer → md5 dual-verify → sandbox-verify → SFTP → tag `v0.21.0.1` → push via SSH kizemo-ssh。
**必交付 (P2)**: lessons-learned L109 (fork pattern) + L110 (upgrade path reference) + ADR `docs/adr/NNNN-librime-fork-strategy.md`。

**禁止事项**: 改 output/install.nsi · push kizemo HTTPS · commit dirty librime 引用 · git add -A · 删 release 历史 installer · 装机 uninstall.exe · sftp password to log · silent failure 任何路径 · librime 升级不做 TestPipeChannelRace 12 PASS · echo sftp.json 密码。

**加速模式**: 任何 silent-failure-prone 代码 → IfErrors 检测 + retry + 备用路径 · ship 前 md5 dual-verify · bash output 不可靠 → git ls-remote ground truth · librime submodule 改动必同步 Fluxing main commit 引用。

**第一步**: 用 brainstorming skill 决定 §3.1 三个决策点 (A/B × i/ii × a/b),推荐组合 A+i+a。然后 writing-plans 写 phased plan,然后实现。