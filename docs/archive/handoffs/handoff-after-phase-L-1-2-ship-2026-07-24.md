# Handoff — Phase L 调整 1.2 v0.19.0.62 ship (2026-07-24)

> **承接 HEAD**: `fd5a644` (refactor(WeaselServer): v0.19.0.62 — QuickPanel UserDict 槽位彻底删除)
> **承接 handoff**: `handoff-after-phase-L-1-1-ship-2026-07-24.md`
> **状态**: ✅ v0.19.0.62 ship 完成
> **触发**: user 反馈 v0.19.0.61 button 2 槽位仍占空间,要求整槽删除

---

## 1. v0.19.0.62 ship artifacts

| Artifact | 值 |
|---|---|
| Commit | `fd5a644` refactor(WeaselServer): v0.19.0.62 — QuickPanel UserDict 槽位彻底删除 (5→4 buttons) |
| Tag | `v0.19.0.62` (lightweight) |
| Installer | `release/fluxing-0.19.0.62-installer.exe` (43,337,092 bytes) |
| WeaselServer.exe md5 | `02876AA73BE5FCF33527F8E13904C178` |
| FluxingPhrasesDialog.exe md5 | `C969AEB31E24D9EE1468F8869150970D` (未变) |
| Installer md5 | `64F9A3C8A9D47A03A81236EDD1761EB4` |
| Installer sha256 | `03a5e321a6b1ac291bf738a1a511a31f77c11a3fba9e6e36f991da28fbc8f02c` |
| SFTP | `www.aiec.fun/pinyin/` OK |

---

## 2. 关键改动

| 项 | 改动 |
|---|---|
| `kPanelW` | `289 → 245` (-44 px) |
| 新增 `kButtonCount` | `4` constexpr (paint / HitTest 循环统一用) |
| HitTest 循环 | `for (i = 0; i < 5; i++)` → `for (i = 0; i < kButtonCount; i++)` |
| paint 循环 | 同上 |
| paint case 重映射 | case 3 → case 2 (Settings); case 4 → case 3 (Account) |
| click routing | `hit == 3 → Shortcut` 改 `hit == 2 → Shortcut` |
| drag fallthrough click routing | 同步 `oldActiveIdx == 3` 改 `oldActiveIdx == 2` |

---

## 3. Phase L 调整链(回顾 + 剩余)

| 版本 | 调整 | 状态 |
|---|---|---|
| v0.19.0.59 | A5 (sftp_sync 通用化) | ✅ ship |
| v0.19.0.60 | Phase L 调整 1 (UserDictionary 模块下线 + Ctrl+Shift+U + click no-op) | ✅ ship |
| v0.19.0.61 | Phase L 调整 1.1 (button 2 paint no-op) | ✅ ship |
| **v0.19.0.62** | **Phase L 调整 1.2 (button 2 槽位删除 5→4)** | **✅ ship** |
| v0.19.0.63+ | Phase L 调整 2 (所有方案都支持中文标点) | ⏸ PENDING |
| v0.19.0.63+ | Phase L 调整 3 (切换火流猩弹浮动栏) | ⏸ PENDING |

3 个 ship,1 个 sftp_sync 复用,1 个清理链完整闭环。
UserDictionary 模块彻底下线(代码 + 热键 + 设置栏 + QuickPanel 槽位 + icon 函数)。

---

## 4. 整个项目 backlog 速览

**Phase L 剩余** (Phase L plan):
- ❌ 调整 2: 中文标点 (拼音 + 多个双拼方案 + 注音)
- ❌ 调整 3: 切换 IME 弹浮动栏 + Alt+, toggle

**Phase L 之外的活跃项**:
- ⏸ K3 T019 装机事故调查 (v0.19.0.55) — PENDING user 反馈
- ⏸ K7 GitHub push (HTTP 500) — 4 个 commit + 4 个 tag 累积未推
- ❌ 浮动设置栏其他 polish (button 0 Schema 跟 button 3 Account 仍是 no-op placeholder,可考虑补 ASCII mode toggle / 登录)

**Phase K 之外更早的 pending**:
- ❌ `installer.exe` 老版本链接 404 (Phase F 修了一半,download.php fallback 没修)
- ❌ `60+` 个 untracked 文件 (prior session 累积)
- ❌ librime submodule dirty

---

## 5. 加速建议 (给 user)

详见会话回复 (`plan-phase-L-2026-07-24.md` 之后)。

— END OF HANDOFF —