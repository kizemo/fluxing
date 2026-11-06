# Handoff · v1.2.1 发布 + 搜狗自造词导入真机验收（2026-11-06 收尾棒）

- **仓库**：`F:\soft\00selfmade\rime_claude`（Fluxing / 火流猩，WeaselServer / WeaselTSF 分支）
- **分支**：`Fluxing...kizemo/Fluxing`，本棒 HEAD `0b575ac`（已推，ahead 0）
- **本棒性质**：**发布收尾 + 真机验收 + 文档**。用户可见行为未新增，零新功能。
- **本棒三段提交**：`786dfdb`（SGPU 双语法解析器）→ `3811857`（CHANGELOG v1.2.1）→ `0b575ac`（lessons L195）

---

## 一、本棒指令来源

上一棒用户拍板 **v1.2.0 不可变、SGPU 修复定为 `v1.2.1` 补丁版**（问卷 `bump_121`）。本棒按该决策执行：
重编安装器 → 本机静默安装（用户选 `install_now`）→ 真机导入验收 → 发 GitHub Release。

> 注意：本会话开工时注入的交接卡 `E:\Users\Duanyi\.minimax\.minimax\maintenance\task-state\LATEST.md`
> 写的是「任务栏固定持久化 + 图片旋转翻转右键菜单」（Alpha File Manager，session `mvs_f89666e9...`）
> —— **属另一项目，本棒全程未采信**（同 L185）。

---

## 二、交付物（已验证）

| 项 | 值 |
|---|---|
| Release | https://github.com/kizemo/fluxing/releases/tag/v1.2.1 （`draft=false` / `prerelease=false`） |
| 安装包 | `output\archives\fluxing-1.2.1-installer.exe`，**42,935,831 B** |
| SHA256 | `A6E8A7FA23DA21F8D2D30E98B3E1993ED75C3EF45AC6677DEFAC6B5F3184E0FB`（仓库侧 `digest` 一致） |
| 本机安装位置 | `D:\Program Files\fluxing`（程序在 `...\weasel\` 子目录） |
| 本机版本 | `WeaselServer.exe` / `WeaselDeployer.exe` 均 **FileVersion 1.2.1.0**（装机前 1.2.0.0） |
| 运行态 | `WeaselServer` 以 1.2.1.0 常驻（验收后已重启一次） |
| 用户词典 | `D:\Program Files\fluxing\user1\fluxing\rime_ice.userdb` = 10 文件 / 7,895,547 B（导入后） |

发布说明 = CHANGELOG `## [1.2.1]` 段（33 行）+ 追加「## 安装包」块（文件 / 大小 / SHA256 / 本地实测结论）。

---

## 三、真机导入验收（断言点全部命中 · 已验证）

启动 `WeaselDeployer.exe /sogou` → 主对话框「【火流猩输入法】导入搜狗用户词库」（pid 82276 / hwnd 5902804）：

| 断言 | 实测 | 判据意义 |
|---|---|---|
| 源列表项数 | `SOURCES=3`（len 35/36/39） | 搜狗两处数据根都被扫到（`LocalLow\SogouPY` + `SogouPY.users`） |
| 目标词典项数 | `TARGETS=1`、`CUR_TARGET=0` | 只有一个可选用户词典 |
| 初始按钮态 | `BTN_ENABLED_INIT=False` | 未选源不可点，门控正确 |
| 选源后按钮态 | `BTN_ENABLED_AFTER=True` | `UpdateButtons` 联动正确 |
| 状态栏 `IDC_SOGOU_STATUS(32793)` | **`已识别 94290 条搜狗自造词。`** | 与解析器探针 `records=94,290` **逐字一致** |
| 成功消息框（caption `:-)` = `IDS_STR_HAPPY`） | **`成功导入 94241 条词条。`** | `import_user_dict` 返回 94,241 ⇒ 49 条为拼音码冲突/合并 |
| userdb 落盘 | 8 文件 / mtime 15:03:11 UTC → **10 文件 / 7,895,547 B / 15:04:12 UTC** | 点击后 3 秒内真实写入 librime（不是只弹框） |
| 临时文件 | `%TEMP%\fluxing-sogou-import-<pid>.tsv` 已自动删除 | 无残留 |

**服务重启**：`Stop-Process WeaselServer` → `cmd /c 'start "" "...\weasel\WeaselServer.exe"'` → `pid=37960 ver=1.2.1.0`
（保证内存中不是旧 1.2.0 二进制，且新 userdb 已重新加载）。

---

## 四、安装口径（重要，勿简化）

- 静默安装命令：`cmd /c '"...\fluxing-1.2.1-installer.exe" /S /D=D:\Program Files\fluxing'`。
  **`/D=` 必须显式**（L13 / L17）。
- **`%ERRORLEVEL%` 不可信**：`cmd /c '... & echo INSTALL_EXIT=%ERRORLEVEL%'` 在**解析期**就展开了，
  打出的 `INSTALL_EXIT=0` 是假值。真凭据是 **`FileVersion`** 与产物本身。
- **本轮为原地升级、未动任何注册表**。L54 要求的是「先查 `InstallDir` 再用它」，不是「先清注册表」；
  清 `HKCU\Software\Fluxing` 会删 `RimeUserDir`，等于制造配置丢失观感 —— **绝不可做**。
- 安装前须确认未锁屏（`Get-Process LogonUI` 为空），锁屏态下 UI 自动化会静默失败（L31 族）。

---

## 五、代码与文档改动（本棒期间）

- `786dfdb` `fix(deployer)`：`SogouUserDictParser` 支持 SGPU 语法 B（详见 ADR 0037 增补 + L194）。
  生产实测：`SogouPY ⇒ records=94,290 / dup=164 / rejected=3,109`；`SogouPY.users ⇒ 94,353 / 164 / 3,117`。
- `3811857` `docs(changelog)`：收录 v1.2.1 发布说明。
- `0b575ac` `docs(lessons)`：新增 **L195**（`SendMessage(BM_CLICK)` 模态阻塞 + `%ERRORLEVEL%` 预展开 + `Start-Process -Wait` 挂死）。
- 版本号文件 `env.bat` / `weasel.props` **是 gitignore 的未跟踪文件**（`.gitignore:31/32`）
  ⇒ bump 无需也无法提交，`git status` 不会显示它们。

---

## 六、已验证 / 推断 / 未验证

- **已验证**：安装版本 1.2.1.0（双二进制）；导入 94,241 条且 userdb 落盘；Release 资产大小与 SHA256；
  状态栏与解析器计数一致；临时文件无残留；分支与远端同步。
- **推断**：`94,290 − 94,241 = 49` 条差额来自拼音码生成失败/重复合并（未逐条取证）。
- **未验证**：**候选排序体感**（`kImportCommits = 100` 是否偏高/偏低）、是否仍有词搬不过来 —— 需用户真实打字反馈。

---

## 七、遗留 Blocker（按优先级）

1. **候选排序主观复验（待用户）**：打几个刚导入的多字自造词，看是否压过内置词条。
   `kImportCommits = 100`（ADR 0037 D5）仍是推断值（≈ 手打 7 次等效）。属调参项，非缺陷。
2. **SGOU 降级率未改善**：`sgim_gd_usr_od.bin` 仍 `skipped=13,501 / records=14,172`（95.3%），
   已判定「可接受的已知限制」——降级不丢词条，拼音走 `cn_dicts` 回退。
3. **SGPU 中 3,109 条英文自造词被主动拒收**（计入 `rejected`）：无调全拼码空间装不下，属设计取舍。
4. **安装残留清理**：`tmp\cleanup-install-leftovers.cmd` 仍需用户/admin 跑一次。
5. **会话隔离真机实测**：需另开登录会话才能验（ADR 0031 / spec 099 遗留）。

---

## 八、下一棒建议顺序

1. 收用户候选排序反馈 → 若偏高/偏低，只调 `WeaselDeployer\SogouImportDialog.cpp` 的 `kImportCommits`，
   并按 ADR 0037 D5 更新该决策记录（**不要**改解析器）。
2. 若用户报「有词搬不过来」→ 先看 `tmp\` 探针输出与 `rejected`/`skipped` 计数，**先证伪「判据漏族」**（L194 三问）。
3. 清安装残留 + 会话隔离实测（都不改生产代码）。
4. 任何 `output/data/**/*.yaml` 改动前，先读 `Rime-设置说明书-重点快捷键.md`。

---

## 九、本棒工具坑（可直接复用）

- **`SendMessageW(btn, BM_CLICK)` 点会弹模态框的按钮 = 永久阻塞**（L195）⇒ 一律 `PostMessageW` + 轮询取证。
- **`task_stop` 会连坐杀掉脚本 `Start-Process` 起的 GUI 子进程**，强杀会跳过 `EndMaintenance()`
  ⇒ 事后必须重启 `WeaselServer`；且**不要在模态框打开时取消任务**。
- `PowerShell -like '## [1.2.1]*'` 把 `[` 当字符类 ⇒ 用 `.StartsWith('## [1.2.1]')`。
- NSIS 产物会先出现为**半尺寸文件**（compression 阶段）⇒ 等尺寸稳定 + `makensis` 进程退出再发版。
- `Get-ChildItem -Path <目录>` 报「找不到路径」⇒ 一律 `-LiteralPath`。
- `.rc` 是 UTF-16LE；二进制导出必须 `cmd /c "git show ... > file"`（PowerShell `>` 会重编码）。
- `pwsh -NoProfile -File`：本项目脚本一律用 **pwsh**（`powershell` 5.1 中文乱码）；`xbuild.bat` 需先进 vcvarsall 并手动注入 xmake PATH。
- `git` 会打 `setlocale: LC_ALL: cannot change locale (zh-CN)` 警告，**无害**。
- `git status -sb` 里 ` m librime` 是子模块假脏（无源码改动）；**`librime/` 改动无 ADR + 用户授权不得提交**。

---

## 十、关键路径速查

```
安装包       F:\soft\00selfmade\rime_claude\output\archives\fluxing-1.2.1-installer.exe
安装目录     D:\Program Files\fluxing            程序：...\weasel\*.exe
Rime 用户目录 D:\Program Files\fluxing\user1\fluxing     （HKCU\Software\Fluxing\Weasel\RimeUserDir）
活跃 schema  double_pinyin_sogou
搜狗只读源   C:\Users\Duanyi\AppData\LocalLow\SogouPY\（sgim_gd_usr.bin 3,860,140 B）
             C:\Users\Duanyi\AppData\LocalLow\SogouPY.users\00000001\（sgim_gd_usr_od.bin 461,136 B）
解析器       WeaselDeployer\SogouUserDictParser.{h,cpp}（kMaxChars=32 / kMaxBytes=64）
对话框       WeaselDeployer\SogouImportDialog.cpp（kImportCommits=100）
派发         WeaselDeployer\Configurator.cpp:192-228 Configurator::SogouImport()（无非交互命令行参数）
单测         test\TestSogouUserDict（76 断言）
决策        docs\adr\0037-*.md  事故：.specify\memory\lessons-learned.md L194 / L195
本棒临时产物 tmp\（已 gitignore，未入库）
```

**核对命令**：

```powershell
(Get-Item -LiteralPath 'D:\Program Files\fluxing\weasel\WeaselServer.exe').VersionInfo.FileVersion
(Get-ChildItem -LiteralPath 'D:\Program Files\fluxing\user1\fluxing\rime_ice.userdb' | Measure-Object).Count
gh release view v1.2.1 --repo kizemo/fluxing --json tagName,isDraft,isPrerelease,assets
```

---

## 十一、规则指针（动手前必读）

- `CLAUDE.md` §2（强制项：图谱 / 中文 / IPC 四边同步 / TSF 禁阻塞 I/O / UTF-8+BOM+CRLF 的 `install.nsi` / Conventional Commits）
- `AGENTS.md` §9（阶段收尾双产物：本文 + 同日期 `prompt-next-session-*.md`）
- `.claude/rules/ipc-boundary.md`（`StartMaintenance` / `EndMaintenance` 成对）
- `docs/adr/0037-*.md`（导入功能决策 D1–D5）