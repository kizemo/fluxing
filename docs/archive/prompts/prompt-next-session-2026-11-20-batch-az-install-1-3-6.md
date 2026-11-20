# 接力 prompt · 批次 AZ 之后（2026-11-20）

> 本文件**只给全路径指针**，不复述 handoff 内容。背景 / 目标 / 进度 / 下阶段计划请自行从下列文档读取。

## 必读（按序）

1. `F:\soft\00selfmade\rime_claude\CLAUDE.md` — 进站指引与最新交接指针
2. `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-20-batch-az-install-1-3-6.md` — **本批真源**（1.3.6 装机结果、TSF 注册真值、判据口径表、未闭环清单）
3. `F:\soft\00selfmade\rime_claude\docs\release\restart-checklist-1.3.6.md` — 待用户在场执行的验收清单（§1 换钥提示 / §2 孤儿回收）
4. `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-20-batch-ay-release-1-3-6.md` — 上一批（出包 1.3.6 与包内令牌判据）

## 按需再读

- `F:\soft\00selfmade\rime_claude\.specify\memory\lessons-learned.md`（L13 / L09 / L20 / L112 / L120 / L126-I）
- `F:\soft\00selfmade\rime_claude\.specify\memory\project-knowledge.md`（§10 NSIS 陷阱；绝对禁止清单；PPL；重装配方）
- `F:\soft\00selfmade\rime_claude\output\install.nsi`（silent 分支、shim 降级路径、Stage2 marker、注册段）
- `F:\soft\00selfmade\rime_claude\docs\release\regression-checklist-cloud-sync.md`（G-3 / G-5 反向判据）
- `F:\soft\00selfmade\rime_claude\docs\adr\0067-*.md`（孤儿会话回收）· `0062`（维护准入闸门）· `0063`（换钥提示）
- `F:\soft\00selfmade\rime_claude\Rime-设置说明书-重点快捷键.md`（改 `output/data/**/*.yaml` 时必读）

## 下阶段起点

AZ 未闭环清单按 handoff §6 优先级执行：先请用户重开使用输入法的应用并在场跑 restart-checklist §1/§2；再申请停服窗口补 `TestWeaselIPC.exe /verify-client-pid`；本批文档改动尚未提交，push 需用户授权。
