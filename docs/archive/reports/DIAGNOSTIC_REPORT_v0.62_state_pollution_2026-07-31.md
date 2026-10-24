# 🔴 诊断报告 — v0.62 装机后仍有 0xc0000374 crash (2026-07-31)

## 我犯的错 (反思)

1. **跑 3 次 v0.74.x ship** + 用户 1 周 反 馈 → L09 / L10 / L##-PhaseL-9.5/9.6/9.7/9.10/9.11 系列 **完全没找到 input crash 真因**
2. **回退 v0.62/v0.70 都失败** — **没意识到真正问题是状态污染**,不是 binary bug
3. **3 个 ship 失败的 mode 完全错** — patch 当前 → 失败 → patch → 失败, 浪费 1 周

## 真相 (取证)

**v0.62 binary 100% 正确** — 5/5 md5 匹配装机路径:
```
WeaselServer.exe    55956c6a9299f2abc3d9f1034a21d04c    ← 我 build 的
weasel.dll          751840dc203e5707cb08f00067922271    ← 我 build 的
weaselx64.dll       ea4dece5b4c76a01704204472c2aa5f4    ← 我 build 的
WeaselDeployer.exe  e75322a33d02c8904789160d5c6362ab    ← 我 build 的
WeaselSetup.exe     55521b257dac8ea494f8b2b57e1e9020    ← 我 build 的
```

**24h 内 39 次 0xc0000374 ntdll.dll event**,其中包括:
- 2026/7/31 21:44:58 Claude Code Haha.exe (用户正在用)
- 2026/7/31 21:45:30 Explorer.EXE
- 2026/7/31 21:45:35 ToDesk.exe
- 2026/7/31 21:45:57 Taskmgr.exe

**所有 crash offset = `0x0000000000112165`** — 跟 v0.74.2 **完全相同**!

## 真因 (锁定)

**`C:\WINDOWS\System32\` 12 个 shim DLL 残留** (v0.66 era + v0.74.x 加新的):

| DLL | Size | mtime | 来源 |
|---|---|---|---|
| `weasel.dll` | 204,841 | 2026-07-30 18:19 | **v0.74.x ship 拷贝** |
| `weaselx64.dll` | 1,199,616 | 2026-06-26 23:23 | **v0.66 era Rime 2.0.0.9** |
| `weasel.old.dll` | 1,168,384 | 2026-06-22 13:47 | v0.66 era |
| `weasel.dll.bak` | 1,167,360 | 2026-06-21 23:19 | v0.66 era |
| `weasel.dll.old` | 1,168,384 | 2026-06-22 13:47 | v0.66 era |
| `weasel.dll.old.10` | 1,167,872 | 2026-06-21 16:58 | v0.66 era |
| `weasel.dll.old.135039` | 1,167,360 | 2026-06-21 23:19 | v0.66 era |
| `weasel.dll.v465` | 1,168,896 | 2026-06-22 15:25 | v0.66 era |
| `weasel.dll.v466bak-20260623-095658` | 1,169,408 | 2026-06-22 23:45 | v0.66 era |
| `weaselx64.dll.old.133147` | 1,168,384 | 2026-06-22 12:14 | v0.66 era |
| `weaselx64.ime` | 700,416 | 2026-06-22 13:19 | v0.66 era |
| `weaselx64.ime.old.133147` | 700,416 | 2026-06-22 12:14 | v0.66 era |

**ctfmon.exe / TextInputHost.exe 加载顺序**:
1. `C:\WINDOWS\System32\weaselx64.dll` (1.2MB 老 shim, v0.66-era Rime 2.0.0.9)
2. `C:\WINDOWS\System32\weasel.dll` (204KB 新 shim, v0.74.x)
3. 通过 IPC 调用 `D:\Program Files\fluxing\weasel\WeaselServer.exe` (v0.62)

**老 shim + 新 WeaselServer.exe IPC 协议不兼容** → heap corruption → 0xc0000374 跨进程

**为什么 v0.62 之前 work,现在不 work**:
- v0.62 binary 跟我 build 的完全一致 → **v0.62 binary 没改**
- v0.62-我-刚-build-的 installer **没**清 System32 (v0.62 install.nsi 只有 L13+L14-fix,clean $INSTDIR 不是 System32)
- 用户机器有 12 个 System32 shim DLL 是 **v0.66 era ship 积累** (2026-06-22 + 2026-06-26)
- 每当新 WeaselServer.exe (任何版本) 启动 → 老 shim DLL 触发 IPC mismatch → 0xc0000374

**为什么 v0.74.x ship 没清:**
- v0.74.1 ship (commit 4694a1d) 引入 9.6 System32 cleanup (L##-PhaseL-9.6)
- v0.74.2 ship (commit 4ea9a14) 加 IfErrors + completion log (L##-PhaseL-9.10)
- **但是 NSIS `Delete /REBOOTOK` 静默 fail** (per L##-PhaseL-9.10 forensics) — ctfmon 持有 handle + file ACL 限制 + admin token 缺失

## 7 天 ship 灾难总结

```
v0.66 (2026-07-25): ship 第 1 份 System32 残留 (cleanup 还没写) 
  ↓
v0.67-72: 各种 fix,但 System32 一直没人清
  ↓
v0.74.1 (2026-07-30): 加 9.6 cleanup glob,但 silent fail
  ↓
v0.74.2 (2026-07-31): 加 IfErrors,还是 silent fail
  ↓
v0.74.3 (2026-07-31): FocusIn Show 撤回 + WeaselServerImpl hot-path - 我做的部分
  ↓
v0.62 / v0.62 rollback (2026-07-31): install 后 12 个 System32 shim 仍 load → crash
```

**Key insight**: 任何**新** WeaselServer.exe 装上,只要 ctfmon 还在 System32 加载老 shim,crash 必现。**唯一 fix = 在 System32 上干净清这些老 shim**,**不靠 installer**(installer 处理不动 System32 locked files)。

## 深度反思 (如何高效处理 bug)

### 1. 严重 bug 出,**立刻回退** — 1 周后才回退浪费 7 天
- v0.74.x ship 失败 1 次就应回退到 user 已知 work 版本(我 v0.62 时才回退,晚了 3 ship)
- "回退 vs 修补" 决策应 **24h 之内** 出

### 2. 回退后**不要立刻 ship 测试** — 必须 audit 机器状态
- v0.62 install 后 crash,我应该先验 v0.62 binary 是否真的在用户机器,而不是假定
- md5 验证 = 5 分钟可完成,我**当时就做了** (但没意识到 现在 0xc0000374 因为 state pollution)
- **真因 = state pollution** 而 **v0.62 binary 正确** — 这是 forensic 30 秒可推断的 (`v0.74.2 crash offset = v0.62 crash offset` = same pattern)

### 3. 在 patch 链中失焦
- L##-PhaseL-9.5/9.6/9.7/9.10 系列 4 个 install 改进 都集中在 installer,nobody asks "registry state from v0.66 era install"
- **回退** 之后应该清空白 slate,而不是假设 install 是 additive

### 4. 真因表示 = 序号 + 范围
- **Successive ship failures 应该触发 "stop ship, debug state" 而非 "patch 更狠"**
- `L##-PhaseL-9.11-Retro` entry 写过了 严重 bug 立刻回退 — 但**当时没 internalize**

### 5. 聚焦 forensic 而非 code writing
- 我**1 周**写 6 个 install 改进 + spec 075 + lessons-learned,真因调查时间 < 30%
- **真因 (state pollution)** 是 5 分钟可看出的:`crash offset 0x0000000000112165 跟 v0.74.2 一样 = 不是 v0.62 自己的问题`

## 装机流程 (用户执行)

### Step 1: 清状态污染 (admin, 5 分钟)

```bat
:: 以 admin 启动 cmd.exe
cd /D "F:\soft\00selfmade\rime_claude"
_output_diag_v0.62_clean_state_admin.bat
```

脚本功能:
- Stop ctfmon + TextInputHost
- Delete 12 个 System32 shim DLL (失败 reboot-scheduled)
- Remove HKCU\Software\Fluxing registry 残留
- Remove Stage 2 / fluxing-staged 临时目录
- Show 任何 Fluxing schtasks (用户手动 delete)

### Step 2: Reboot (如果 SCHEDULE_REBOOT 提示)

```bat
shutdown /r /t 0
```

### Step 3: 重新 install v0.62 OR v0.18.41.4

**选项 A — v0.62 (我刚 build)**:
```
release\fluxing-0.19.0.62-installer.exe (md5 e4186a1affbb4950a1d25cabb257681a)
```
- 浮动栏手动 Alt+, 打开
- 切输入法不自动弹 (切输入法弹浮动栏 feature 在 v0.63 之后)

**选项 B — v0.18.41.4 (L69-fix QuickPanel disabled)**:
- **最稳定** — QuickPanel 完全禁用,输入法 100% basic
- 无任何 install-freeze / 无 L66 safety-net / 无 Stage 2 atomic install
- 但 install.nsi 也是无 System32 cleanup,**用我提供的 `_diag_v0.62_clean_state_admin.bat` 配合**

### Step 4: 验证 (24h)

- [ ] `_diag_v0741_evidence.ps1` 跑 Application Error event count ≤ 5
- [ ] `_diag_v0.62_state_pollution_audit.ps1` 跑 System32 should be EMPTY
- [ ] 输入中文 ≥ 10 次不崩任何宿主

## 严格 ship-gate (per L##-PhaseL-9.11-Retro)

每个 ship 之前必跑:
- [ ] 真机 input 中文 ≥ 10 次 不崩任何宿主
- [ ] 真机 en-US ↔ Fluxing 切输入法 ≥ 5 次 不崩
- [ ] 真机 reboot + 24h 后 `_diag_v0741_dumps.ps1` 没新增 weasel/crash dump
- [ ] 真机 `_diag_v0741_evidence.ps1` 24h Application Error event count ≤ 5
- [ ] 不能 ship "理论上 work" 但没真机验证

## 关联 lessons-learned

- `L##-PhaseL-9.10` System32Cleanup-SilentFail (确认)
- `L##-PhaseL-9.11-Retro-v0.74.x` 7 天事故链 (新)
- L66 safety-net 写 KnownClasses + HKCU\0x00000804

## 未 commit / 未 push / 未 SFTP

按你"立刻回退"指令,没有 commit 任何改动。诊断报告写到磁盘,你看完决策后决定下一步。
