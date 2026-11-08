# 批次 N · spec 045 T107 正文级真合并（勘察定稿 + 实施设计）

> 生成：2026-11-08 23:01 · 会话 `mvs_5a90523976724e12aede113520ad36b2`
> 上一批：[批次 M · spec 045 快捷键设置 P1 收口（T101/T102/T103/T106）](handoff-2026-11-08-batch-m-shortcut-settings-p1.md)
> **状态：T107 未实现。** 本轮只做勘察与设计定稿，未改一行业务代码，未跑构建、未跑单测、未做真机验收。

---

## 1. 本轮做了什么 / 没做什么

**已做**（evidence 见 §2）：
- 进站阅读：`CLAUDE.md`、批次 M 交接、`.specify/specs/045-shortcut-settings/tasks.md`、`design.md` §1–§7
- 源码勘察：`WeaselServer/ShortcutSettings.cpp`（L400–860 精读）、`WeaselServer/ShortcutSettings.h`、
  `test/TestShortcutSettings/TestShortcutSettings.cpp` 前 390 行
- 依赖勘察：确认 `WeaselServer` **未链接 yaml-cpp**（yaml-cpp 只在 `WeaselDeployer` / `FluxingConfigEditor` / `test/TestYamlRoundTripE2E`）
- 工作树核对：`git status -sb` + `git diff --stat`
- 把批次 M 交接 §2.4 的「写盘语义 9 条」逐条对着代码验证，并新发现一处**数据丢失 bug**（§2.3）

**没做**：T107 编码、`tasks.md` 勾选、构建、单测、真机验收。

---

## 2. 已验证事实（带证据）

### 2.1 工作树（动工前必须先决策）

- 分支 `Fluxing`，`## Fluxing...kizemo/Fluxing [ahead 10]`
- 批次 L + M 的改动**全部未提交**：12 个已跟踪文件，`+706 / -87`
- 未跟踪：`.specify/specs/045-shortcut-settings/tasks.md`、`docs/adr/0045-*.md`、两份 handoff、
  `qp-dump.bmp` / `qp-logo.bmp` / `qp-state.txt`
- `librime` 子模块 dirty（`m`）

⇒ T107 动工前先定：这批（L+M）要不要先提交。否则 T107 的 diff 会和前两批混在同一次 `git diff` 里，
commit 时无法按批次拆分（CLAUDE.md「Conventional Commits + 模块前缀」要求单次提交语义单一）。
`qp-*.bmp` / `qp-state.txt` 是探针产物，**不应入库**。

### 2.2 `SaveHotkeys` 现状锚点（`WeaselServer/ShortcutSettings.cpp`，截至本次勘察）

| 行号 | 内容 |
|---|---|
| 583–644 | 匿名 namespace：`SaveStats`（`written` / `skipped_no_action` / `backed_up` / `refused`）、`g_saveStats`、`kManagedBegin`(L605) / `kManagedEnd`(L607)、`HasTopLevelPatch()`(L611)、`BackupFile()` |
| 646–781 | `bool SaveHotkeys(const std::wstring& path, const Data& data, bool overwrite_file)`（签名尾部在 L648） |
| L687–738 | 块体拼装：`appendAscii(kManagedBegin)` → `patch:` → `key_binder/bindings:` → 每行 → `appendAscii(kManagedEnd)` |
| L712 | 行筛选 `if (!edited && h.is_default) continue;` |
| L719 | `++g_saveStats.skipped_no_action;` |
| L742–751 | `existing.find(kManagedBegin)` 命中 ⇒ 替换 begin..end |
| L758–762 | `else if (HasTopLevelPatch(existing))` ⇒ `refused = true; return false;` ← **T107 要改的就是这里** |
| ~1731 起 | `SaveAndDeploy()`；L1742 用 `g_saveStats.refused` 组提示；L1756–1757 `skipped_no_action` 提示 |
| ~1769 起 | `ExportYamlPath()` |

### 2.3 确认 bug：既有 override 在保存时被静默丢弃（数据丢失）

证据链（全部为源码实读）：

1. **L303**：构造内置默认时 `h.is_default = true;`
2. **L561–562**：`merged` 先把 `m_defaults` **整份拷贝** ⇒ 每条都带 `is_default = true`
3. **L564–578**（合并循环）：自定义行命中 `d.accept == c.accept` 时只做
   `d.accept = c.accept; d.action_yaml = c.action_yaml; d.is_custom = true;`
   —— **没有把 `d.is_default` 复位为 false**
4. **L712**：写盘筛选 `if (!edited && h.is_default) continue;`
   ⇒ 这条「用户既有的 override」不进输出

**后果**：用户 `default.custom.yaml` 里已有 `- { when: always, accept: Control+Shift+1, send: 1 }`，
而内置表里也存在 `Control+Shift+1` ⇒ 加载时被合并成 `is_default=true` 的行 ⇒ 用户没在界面上改它，
保存时就被跳过 ⇒ **保存动作把用户已有的自定义绑定抹回内置默认**。这不是理论风险，是最常见的旧用户形态。

**同处第二个缺陷**：合并只比 `accept`、**不比 `when`** ⇒ 同 `accept` 不同 `when` 的多行会塌缩成一条。
（`when` 是 RIME key_binder 的一等语义，`always` / `predict` / `composing` 互斥不同。）

⇒ 这两个必须与 T107 同批修：T107 是「能把文件写进去」，§2.3 是「写进去的东西是对的」。
只做 T107 会让数据丢失从「写不进去」升级成「写进去且丢配置」。

### 2.4 T107 的硬约束：托管块形态会导致重复 `patch:` 键

现实现块体是**顶层形态**：

```
# >>> fluxing-shortcuts (spec 045) begin
patch:
  key_binder/bindings:
    - { when: always, accept: Control+Shift+1, send: 1 }
# <<< fluxing-shortcuts (spec 045) end
```

若把它塞进「文件里**已有**的 `patch:` 段内部」，二次保存时 L742 命中标记、替换出的仍是**顶层形态**
⇒ 文件里出现第二个顶层 `patch:` 键 ⇒ YAML 重复键。librime 的 Patcher 取后者，
**用户原有 patch 项（schema_list / menu / switcher…）被吞**。

⇒ 结论：**块体必须与容器分离**，否则 T107 只是把「拒写」换成「写坏」。

---

## 3. 设计定稿（待实施）

### 3.1 块体只含子块，不含 `patch:`

- 托管块固定包裹 **`key_binder/bindings:` 子块**：
  - 标记行：缩进 2（与 `key_binder/bindings:` 同级，注释行不影响 YAML 结构）
  - `  key_binder/bindings:`：缩进 2
  - 每条 `    - { ... }`：缩进 4
- **`patch:` 行由「容器层」负责**：不存在才新建，已存在就复用
- **幂等**：二次保存把 `begin..end`（含）整段换成同样的子块；容器不在块内 ⇒ 永远不产生第二个 `patch:`
- 提取为纯函数 `std::string BuildManagedSubBlock(const Data&)`（便于单测直接断言字符串，不必落盘）

### 3.2 三种输入形态的处理（T107 的核心）

| 形态 | 旧文件状况 | 处理 |
|---|---|---|
| **A** | 无顶层 `patch:` | 追加 `patch:` + 托管子块（沿用现行为，首次 `BackupFile()` 失败即不写） |
| **B** | 有顶层 `patch:`，其内**无** `key_binder` / `bindings` | **本轮新增能力**：把托管子块插到 `patch:` 行之后；原有其它键、缩进、注释原样保留 |
| **C** | 有顶层 `patch:`，其内**已有**手写 `key_binder/bindings`（无标记） | **仍然拒写**，但换精确文案 + 独立计数 |

**C 为什么还拒**：文本级合并若要吞掉用户手写段，就得先解析它；而现解析器只认 inline
`- { k: v }`（L501–502 的判断），用户写 block style / 带注释 / 用 anchor 时解析不到
⇒ 吞掉 = 静默丢配置。C 的正确处置是明确告诉用户「删掉/改名这一段再保存」。

> 若用户后期要求 C 也可写：退路是把用户手写项**原样保留**、托管子块作为**第二个**
> `key_binder/bindings` 键追加 —— 但重复键后者胜 ⇒ 用户手写项失效。
> **这条退路必须用户拍板，不能默认做。**

### 3.3 `SaveStats` 扩展

- 保留 `refused`
- 新增 `int refused_has_manual_bindings`（C 形态专用）
- `SaveAndDeploy()` 的提示文案按计数分流：
  - A/B 成功：报写入条数（现有文案）
  - C：`"未写入：default.custom.yaml 的 patch: 下已有手写 key_binder/bindings，请先删除该段再保存（避免覆盖你的手动配置）"`

### 3.4 实施顺序

| 步 | 内容 | 验收 |
|---|---|---|
| **S1** | 修 §2.3：合并循环置 `d.is_default = false` + 改判据为 `(accept, when)` 双等 | 新增单测 `TestLoadHotkeysKeepsExistingOverride`：写一份带 override 的 yaml → `LoadHotkeys` → 该行 `is_default == false`；再 `SaveHotkeys` 后文件里该行仍在 |
| **S2** | 拆块体/容器：`BuildManagedSubBlock(data)` 纯函数；`SaveHotkeys` 只负责「找容器 + 放块」 | 纯字符串断言，无需落盘 |
| **S3** | 落地 **B 形态**插入 + 幂等 | `TestSaveHotkeysInsertsIntoExistingPatch` + `TestSaveHotkeysIdempotent` |
| **S4** | C 形态文案与计数 | `TestSaveHotkeysRefusesManualBindings` |
| **S5** | 改测试语义：`TestSaveHotkeysRefusesExistingPatch`（断言「有顶层 `patch:` ⇒ 返回 false」）**已失效**，拆成 S3/S4 的三条 | — |
| **S6** | 构建 + 全量单测 | `BUILD_EXIT=0`、`FAIL=0` |
| **S7** | 真机验收（与 T101/T102/T103/T106 合并做） | 见 §5 |

**S3 的插入算法（具体）**：
1. 按行切分 `existing`（保留原行尾形态）
2. 找行首无缩进、去空白后以 `patch:` 开头的行 → 记 `patchLine`
3. 从 `patchLine+1` 起向后扫：第一个「非空且非注释且行首无缩进」的行 = patch 段结束边界
4. 在该区间内查 `key_binder`（含 `key_binder/bindings:` 两种写法）⇒ 命中即形态 C，`return false`
5. 未命中 ⇒ 把 `BuildManagedSubBlock` 的各行插入到 `patchLine` 之后（紧跟一行后可加空行保持可读）
6. 输出统一 `"\xEF\xBB\xBF"` + 正文、CRLF（L09）

**回归断言（必须写进测试，防止重蹈 §2.4）**：
- `CountOf(text, "patch:") == 1`（连存两次后仍为 1）
- 原有 `schema_list` / `menu` 等段文本**逐字节**保留
- 文件头 BOM 恰好一份

---

## 4. 构建与验证命令

> 以下命令来自批次 M 交接 §4，**本轮未执行**，下一轮直接复用。

```bat
rem 主构建（内部 xbuild.bat weasel；不重建测试工程）
F:\soft\00selfmade\rime_claude\tmp\build-batchM.cmd

rem L0 门禁
pwsh -NoProfile -File F:\soft\00selfmade\rime_claude\scripts\gate\L0-static.ps1 -Mode source

rem 测试工程（必须 MSBuild 直编）
msbuild test\TestShortcutSettings\TestShortcutSettings.vcxproj /t:Build ^
  /p:Configuration=Release /p:Platform=Win32 ^
  /p:SolutionDir="F:\soft\00selfmade\rime_claude" /m:1 /nologo /v:minimal
```

**坑（已踩过，勿重犯）**：
- `SolutionDir` 必须**不带尾反斜杠**的绝对路径，否则 OutDir 嵌到 `test\<name>\Release\`（L31）
- `xbuild.bat weasel` **不重建测试工程**；`release\*.exe` 单测产物可能是过期二进制
  ⇒ 判 PASS 前先核 mtime
- `cmd /c "F:\...\tmp\build-test-shortcut.cmd"` 形态可用；`cd /f/... && cmd /c ...` 会被安全策略拦
- PowerShell 里 `"$i: $_"` 报「变量引用无效」，要写 `${i}`

---

## 5. 未验证缺口（沿用批次 M，一条都没消除）

1. **真机界面零验收**：T101 / T102 / T103 / T106 只有单测 + 静态门禁证据，没人在真机上点过一次
2. **真实 `default.custom.yaml` 从未被本代码写过**：测试全部落在 `%TEMP%\FluxingTestUserData`
3. **`WeaselUserDataPath()` 实际返回目录未坐实**：批次 M 交接称
   `E:\Users\Duanyi\AppData\Roaming\Fluxing` **目测不存在**。
   ⇒ T107 的 A/B 形态落盘验证**前**必须先确认这个路径（含「目录不存在时谁负责创建」），
   否则「单测全绿」与「用户点保存没反应」可以同时成立
4. **锁屏态限制**：原始输入注入（`SendInput` / `PostMessage`）与 `CopyFromScreen` 抓屏会失败；
   UIA `InvokePattern.Invoke()` 与 UIA 树枚举仍可用 ⇒ 锁屏不必然阻塞验收，但要把「点控件」改成 UIA 路径
5. **`weaselx64.dll` 被 17 个进程占用**：批次 L 的清理 `CLEAN_FAIL`；静默安装 `EXIT=0` 也可能没替换成功
   ⇒ 真机验收必须核**装机副本的大小 + mtime**，不能只看安装器退出码

---

## 6. 下阶段计划（优先级，与原交接 §5 一致）

1. **决策**：批次 L+M 是否先提交（§2.1）；`qp-*.bmp` / `qp-state.txt` 清出工作区
2. **T107**：S1 → S6（§3.4），S1 不可跳过
3. **真机界面验收**：T101 / T102 / T103 / T106 + T107 落盘（先坐实 §5.3 路径）
4. **T104「立即验证」**：必须先过 P2 / AP-045-B 审查（`WH_KEYBOARD_LL` hook 内禁止 `SendInput`）
5. **T105** 自定义快捷键子 modal
6. **P2 T201–T203** / 测试缺口 **T301–T304**

---

## 7. 累计工具坑（批次 L / M / N）

- 探针类产物（`*.bmp` / `*-state.txt`）不入库；写进 `.gitignore` 或放 `tmp/`
- 装机 / 重启后立刻跑桌面自动化探针会**假失败**；`WHOLE_PANEL_DIFF=0` ⇒ 点击未送达，不是逻辑错，
  同一二进制不重装重跑往往即绿
- UIA 里自绘控件的 `Name` 常是 tooltip 或空串 ⇒ 判据别用字形，用 tooltip 文案或同类按钮计数
- 「搜索 X 得 0 个」不能当「X 已消失」的证据；先证明判据本来能搜到 X（拿改前基线跑同一判据）
- 抓屏前先 `SetProcessDpiAwarenessContext(-4)`，否则拿到逻辑分辨率截错区域