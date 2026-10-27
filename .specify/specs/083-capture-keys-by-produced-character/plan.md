# 083 实施计划：设置界面抓键按「实际产生的字符」生成（ADR 0011）

> 配套：[spec.md](spec.md)、[ADR 0011](../../../docs/adr/0011-capture-keys-by-produced-character.md)、
> ADR 0009 / ADR 0010、lessons L149 / L150、清单 T2 / T19 / T20。
>
> 本轮（2026-10-28）**只做完了调查与决策**；**M1–M6 全部待做**。

## 0. 本轮已交付

| 交付物 | 位置 | 状态 |
|---|---|---|
| 死绑定坐实（对照实验） | `F:\soft\00selfmade\tmp-dllcmp\probe_t2_binding.py` + `.log` | **[已做]** `Control+Shift+9` ⇒ `DEAD_KEYCODE`，`RESULT\|FAIL` |
| 决策 | `docs/adr/0011-capture-keys-by-produced-character.md` | **[已做]** `Proposed`（实现落地并实测后才转 `Accepted`） |
| 需求 | `.specify/specs/083-capture-keys-by-produced-character/spec.md` | **[已做]** R1–R7 |
| 源码事实 | spec.md §0 的 F1 / F2 / F3 | **[已做]** 逐条核验（含推翻子代理的两处错误归因） |
| 实施计划 | 本文件 | **[已做]** |
| 任务清单 | [tasks.md](tasks.md) | **[已做]** |
| 共享单元 / 抓键改造 / 测试 | — | **[未做]** M1–M6 全部待做 |

## 1. 里程碑

### M0 · 调查与决策（已完成）

| # | 任务 | 验收 |
|---|---|---|
| T0.1 | 用对照实验坐实「写错键名 ⇒ 死绑定」 | 门禁输出 `DEAD_KEYCODE` + `RESULT\|FAIL` |
| T0.2 | 核验 librime 的键名表只有大写字母（`keys_by_keyval` 纯 `strcmp`） | `librime/src/rime/key_table.cc:2026-2033` |
| T0.3 | 核验前端在算字符前清 Ctrl / Alt | `WeaselTSF/KeyEvent.cpp:44-51` |
| T0.4 | 核验两条抓键实现已经漂移（`equal` vs `plus`） | `KeyRecorder.cpp` vs `ShortcutSettings.cpp:622-718` |
| T0.5 | 拍板 ADR 0011 的 D1–D6，写出 R1–R7 | ADR `Proposed` + spec.md |

**依赖**：无。**风险**：低（只读调查 + 文档）。

---

### M1 · 共享单元 `include/KeyNameMapper.h`

**目标**：唯一一份「字符 ⇄ Rime 键名」映射，抓键与门禁共用。

| # | 任务 | 验收 |
|---|---|---|
| T1.1 | 新增 header-only `include/KeyNameMapper.h`（**不新建 .vcxproj**，R7） | 门禁工程可直接 `#include` |
| T1.2 | 键名表与 `test/TestHotkeyBindings/keysyms_generated.inc` **同源同生成器** | 两侧逐条一致 |
| T1.3 | 提供「字符 → 键名」「组合键 → 字符」两个方向的纯函数 | 无 GUI / 无 rime_api 依赖 |
| T1.4 | 复刻 F2（清 Ctrl/Alt、Shift 定大小写） | 与 `WeaselTSF/KeyEvent.cpp` 逐条对齐 |
| T1.5 | 对 librime 无名的字符返回「不可表达」，**不产出死绑定** | R3 + F1 |

**依赖**：M0。**风险**：中（表同源是正确性根）。

---

### M2 · 抓键改造 `FluxingConfigEditor/KeyRecorder.cpp`

| # | 任务 | 验收 |
|---|---|---|
| T2.1 | `MapKeyCodeToLibrimeName` 换成共享单元 | `KeyRecorder.cpp:22-88` 重写 |
| T2.2 | 删掉 `:118` 的 `win → Control+` 误拼，改为**拒绝 + 提示** | R3 / D6 |
| T2.3 | 输出按前端掩码精确给出 Shift / Control / Alt | R3 |
| T2.4 | `Ctrl`+小写字母 ⇒ 拒绝并提示「请按住 Shift」 | F1 + R3 |

**依赖**：M1。**风险**：中。

---

### M3 · 第二条路径 `WeaselServer/ShortcutSettings.cpp`

| # | 任务 | 验收 |
|---|---|---|
| T3.1 | `VKeyToName`（:622-699）切到共享单元 | 与 M2 同源 |
| T3.2 | `NormalizeCombo`（:701-718）切到共享单元 | 消除 `equal` vs `plus` 矛盾 |
| T3.3 | **不动**内置表（:339-402，硬编码 accept 串） | `Control+Shift+F` 保持原样 |

**依赖**：M1。**风险**：中。

---

### M4 · 测试（抓键产物专项）

| # | 任务 | 验收 |
|---|---|---|
| T4.1 | `test/TestHotkeyBindings/fixtures/recorder-output.yaml` | 抓键产物样本 |
| T4.2 | 同目录 `.expect.txt` 期望集 | 逐条 `REACHABLE` |
| T4.3 | 负控：`Control+Shift+9` 必须判 `DEAD_KEYCODE` | 门禁仍能抓到死绑定（防止门禁被改瞎） |

**依赖**：M2 / M3。**风险**：低。

---

### M5 · 门禁复跑（仓库源 + 部署产物）

| # | 任务 | 验收 |
|---|---|---|
| T5.1 | 仓库源门禁 | `SUMMARY\|bindings=39\|reachable=39\|dead=0` + `RESULT\|PASS`（**数字不得变差**） |
| T5.2 | 部署产物门禁 | `bindings=120\|reachable=120\|dead=0` + `RESULT\|PASS` |
| T5.3 | 全套件 + 默认快捷键 | `=== ALL TESTS PASSED ===`；`Passed: 89 / 89` |
| T5.4 | clang-format 整文件 clean | CI lint 按整文件判 |

**依赖**：M4。**风险**：低。

---

### M6 · 打包与实机验收

| # | 任务 | 验收 |
|---|---|---|
| T6.1 | 版本号升 `0.21.0.10`（`env.bat`，gitignored） | `PRODUCT_VERSION` 同步 |
| T6.2 | `build.bat weasel installer` | `output\archives\fluxing-0.21.0.10-installer.exe` |
| T6.3 | 交用户实机验收（抓键可用性 + 原有快捷键不回归） | 用户确认 |
| T6.4 | ADR 0011 `Proposed` → `Accepted`（仅在实测通过后） | ADR 状态迁移记录 |

**依赖**：M5。**风险**：中。

## 2. 未决问题（ADR 0011 Open questions）

| # | 问题 | 处置 |
|---|---|---|
| Q1 | 共享单元放哪 | **已定**：`include/KeyNameMapper.h`，header-only，不新建构建目标（R7） |
| Q2 | CapsLock 怎么办（F3 / 清单 T20） | **未决**：三个候选（前端清 `LOCK_MASK` / 抓键产出 `Lock+` / 门禁纳入 caps 维度）——**先实测再选** |
| Q3 | `Ctrl`+字母要不要在 UI 里教学 | **未决**：先按「拒绝 + 提示」实现，教学文案后置 |

## 3. 提交纪律

- 每步都要 **clang-format 整文件 clean**（CI lint 按整文件判）。
- **`style(fluxing):` 与功能改动分两个提交**（先例 `4ab52db` / `a023ee1`）。
- 只改 `.custom.yaml` 的 `patch:`；一个文件只能一个 `patch:`（rime-skill 铁律）。
- `librime/` **只读**（宪法 P3）。
- 提交消息里断言的数字必须真实落地；未实跑的结论不得写死。
