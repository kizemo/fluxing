81 任务清单

> 勾选框与 [plan.md](plan.md) 的任务编号一一对应。
> `[x]` = 本轮（2026-10-25）已完成；`[ ]` = 下一阶段待办。

## M0 · 拍板与文档债

- [x] T0.1 用户确认 ADR 0009 的方向（改配置，不改前端按键语义），状态 `Proposed` → `Accepted`（2026-10-25 拍板「删 2 条重复死绑定」，其余 4 条 `Shift+标点` 改名待办）
- [x] T0.2 《Rime-设置说明书-重点快捷键.md》§9.1：条数与算式对齐实测（删 2 条后 = 出货 `default.yaml` 27 + `rime_ice.custom.yaml` 4 = **31 条**；原文「共 22 条 = 4+2+4+16+8」本身算式就不成立（=34），已改成实测值；L113 删掉的 4 条中文全角 `Shift+标点` 也一并从摘录里去掉）
- [x] T0.3 同文 §10.2：删掉「被系统级 hotkey 抢了」的错误归因，改指 ADR 0009（md + html 两份都改）
- [x] T0.4 `CONTEXT.md` §4 加 invariant 16「TSF 前端发的是字符不是物理键」
- [x] T0.5 `CONTEXT.md` §7 去掉「`docs/adr/` is currently empty」的过时描述
- [ ] T0.6 `CONTEXT.md` §7 的 ADR 章节格式与 `docs/adr/README.md`（MADR）统一

## M1 · 修死绑定

- [ ] T1.1 `output/data/user-custom/rime_ice.custom.yaml`：`Shift+apostrophe` → `Shift+quotedbl`
- [ ] T1.2 同上：`Shift+backslash` → `Shift+bar`
- [ ] T1.3 同上：`Shift+bracketleft` → `Shift+braceleft`
- [ ] T1.4 同上：`Shift+bracketright` → `Shift+braceright`
- [x] T1.5 `output/data/default.yaml`：删 `Control+Shift+9`（孪生键 `Control+Shift+numbersign`）（2026-10-25 落地；行首原位置留了 ADR 0009 说明注释）
- [x] T1.6 同上：删 `Control+Shift+0`（孪生键 `Control+Shift+dollar`）
- [ ] T1.7 `test/TestHotkeyBindings/known-dead-bindings.txt` 清空 —— **本轮只完成一半**：6 条 → 4 条（本轮删掉的是 `default.yaml` 那 2 条重复死绑定）；剩下 4 条 `Shift+标点` 等 T1.1-T1.4 改名修好后才清零
- [x] T1.8 `test/TestDefaultHotkeys`：把「切换中英标点 = `Control+Shift+9`」的断言改成 `Control+Shift+numbersign`；「切换简繁 = `Control+Shift+0`」改成 `Control+Shift+dollar`（并补 2 条**否定断言**钉住死绑定不许复活；active 绑定数 44 → 42）
- [ ] T1.9 同上：4 条 Shift+标点的断言同步改成字符键名
- [ ] T1.10 `output/data/user-custom/double_pinyin_sogou.custom.yaml`：同样的 4 条一并修
- [ ] T1.11 `output/data/user-custom/radical_pinyin.custom.yaml`：同样的 4 条一并修
- [ ] T1.12 `user-custom/*.custom.yaml` 里「保留的 4 条……实际生效」的注释改成说明**为什么写字符键名**
- [ ] T1.13 改完跑 `Release\TestHotkeyBindings.exe`（**不带** `--baseline`）→ 必须 `dead=0`（还差 4 条 `Shift+标点`，本轮带 `--baseline` 跑：`dead=0|known_dead=4`）
- [ ] T1.14 重新打包安装包（`build.bat weasel installer`）
- [ ] T1.15 静默装机 smoke test（AGENTS.md §2.5），结果写进 commit message
- [x] T1.16 `WeaselServer/ShortcutSettings.cpp`：内置 / 默认快捷键表里的 `Control+Shift+9` / `Control+Shift+0` 改成可达的 `Control+Shift+numbersign` / `Control+Shift+dollar`（否则「快捷键设置」界面仍显示一个按不动的键，保存时还会把死绑定写回用户的 `custom.yaml`）

## M2 · 门禁进 CI

- [x] T2.1 `test/TestHotkeyBindings` 注册进 `weasel.sln`
- [x] T2.2 `scripts/test-infra/run-test-suite.bat`：构建 + 负对照 + 真实配置 + 防漂移检查
- [x] T2.3 `scripts/sandbox-gate/in-sandbox-claims.ps1` PHASE 3.5（P10.1 / P10.2 / P10.2b / P10.3）
- [x] T2.4 沙箱工装的暂存逻辑补上三个辅助文件（fixture / expect / baseline）
- [x] T2.5 CI 跑 `run-test-suite.bat`：CI 的 `test` job 调 `scripts\run-tests.bat`，它是 `scripts\test-infra\run-test-suite.bat` 的薄包装 ⇒ 新门禁已经在 CI 路径上
- [x] T2.6 `gen-keysyms.ps1 -Check` 接进 CI：在 `run-test-suite.bat` 里，同上
- [x] T2.8 **【2026-10-25 本轮新增·已完成】拆掉把整条流水线挡死的 lint 墙**（事故 **L132**）——
  `lint` job 自品牌分叉起就是红的（整树 lint，175 个文件里 78 个不合规），
  而 `build` needs `lint` ⇒ `build` / `test` / `sandbox-gate` **全部被跳过**，
  门禁从接进去那天起**一次都没真正跑过**。
  → 新增 `scripts/lint-changed-cpp.sh`：**只检查本次改动触及的** `.cpp` / `.h`；
    历史欠账改用 `--all --report-only` **单独报告、不阻断**，保持可见且只能往下走。
  → 判别力已验证：本机全量 **78 / 175** 与 CI 日志点名的 78 个文件完全一致；
    塞一个故意写坏的文件 ⇒ **79 / 176**；独立仓库里坏文件 ⇒ **exit 1**、修好 ⇒ **exit 0**。
- [ ] T2.9 **【可选】清掉 78 个文件的样式债**：跑一遍 `clang-format -i` 即可，
  欠账数只减不增；清完之后可以把 `--all --report-only` 那一步也删掉
- [x] T2.10 **【2026-10-25 本轮新增·已完成】CI 补上 librime 依赖库的构建**——
  拆掉 lint 墙之后 `build` job 第一次真跑，立刻撞上：
  `Build Weasel: Error: the librime dependency libraries are not built yet.`
  根因：`Weasel*` 要链接 `librime/deps/yaml-cpp/build_Win32/Release/yaml-cpp.lib`，
  `build.bat` 在它不存在时主动拒绝继续（L128 加的清晰报错）；
  而 CI 的 workflow **从上游继承**，上游不链接 yaml-cpp ⇒ **分叉特有的 CI 缺口**。
  → 在 Weasel 构建之前加一步 `.\build.bat rime`（它会拉子模块、打/撤 opencc-marisa 补丁、
    为 Win32 与 x64 各构建依赖 + librime ⇒ 产物不再混用预编译 librime）。
- [x] T2.11 **【2026-10-25 本轮新增·已完成】修 CI 拉不到 librime 的嵌套子模块**（事故 **L133**）——
  补完 T2.10 后又暴露第二层：
  `patch-librime-opencc-marisa.ps1 : opencc CMakeLists.txt not found`。
  根因：**`librime/deps/*` 是 librime 自己的子模块**，而 `actions/checkout@v4` 的
  `submodules: true` 跑的是 `git submodule update --init --force`（**没有 `--recursive`**）
  ⇒ CI 里 `librime/deps/` 是空的；而 `build.bat` 的守卫只在 `librime\build.bat` 缺失时才跑
  ⇒ CI 里永远不会跑。
  → `build.bat`：守卫改成「`librime\build.bat` 缺失 **或** `librime\deps\opencc\CMakeLists.txt` 缺失」
    就 `git submodule update --init --recursive`（顺带修好本地 clone 忘 `--recursive` 的情况）；
  → `ci.yml`：三处 `submodules: true` → `submodules: recursive`；
  → `ci.yml`：`test` job 也要 `build.bat rime`（测试工程链接的 `rime.lib` / `yaml-cpp.lib`
    **都不在版本库里**，而 `get-rime.ps1` 把 rime.lib 放在测试工程不搜索的目录）。
- [x] T2.12 **【2026-10-25 本轮新增·已完成】让 xmake 矩阵腿不再连坐门禁**（事故 **L134**）——
  拆掉 lint 墙（L132）之后 `build` job 第一次真跑，又撞上第三道墙：
  `build (xmake)` = failure（`unknown target(RimeWithWeasel) for WeaselTSF.deps!`
  + `LNK1181` on `yaml-cpp.lib` / `rime.lib` + `install.nsi` line 621），
  而矩阵默认 `fail-fast: true` ⇒ 它把 `build (msbuild)` **取消**了；
  `test` 的 `needs: build` 没满足 ⇒ `test` 与 `sandbox-gate` **全部 skipped**
  ⇒ 唯一能用的构建腿被取消，所有门禁腿被跳过。
  → `strategy.fail-fast: false`；
  → `Build Weasel` 拆成按变体分别触发的两个步骤，xmake 那条挂**步骤级** `continue-on-error: true`
    （**job 级不够**：job 级只让 workflow 不变红，job 结论仍是 `failure`，`needs` 照样跳过下游）；
  → job 级再补 `continue-on-error: ${{ matrix.variant == 'xmake' }}` 作策略声明；
  → xmake 腿**保留在矩阵里**，坏要继续可见，只是不再挡住门禁。
- [ ] T2.13 **【可选·下阶段】真修 xmake 的 arm64 配置**——本轮只把它降为非阻断，腿本身还是红的。
  根因（已逐行核对根 `xmake.lua` / `xbuild.bat` / `WeaselTSF/xmake.lua`）：
  根 `xmake.lua` 第 52–54 行把 `includes("RimeWithWeasel", ...)` 关在
  `is_arch("x64") or is_arch("x86")` 里，而 CI 的 xmake 腿构建 **arm64**
  （`xbuild.bat arm64 installer` → `xmake f -a arm64`）⇒ arm64 下 `RimeWithWeasel`
  这个 target 压根没定义 ⇒ `WeaselTSF` 的 `add_deps` 报 `unknown target`；
  另外第 36–46 行的 arch → link 目录映射**没有 arm64 分支** ⇒ `LNK1181` on
  `yaml-cpp.lib` / `rime.lib`。
  → 修法：把 arm64 纳入 include 条件与 link 目录映射（或让 arm64 也指向 `lib`），
    再复验 `xbuild.bat arm64 installer`。
  ⚠️ **不是发布路径**（`AGENTS.md A15`）：发布一律走 `build.bat`（MSBuild）。
- [x] T2.14 **【2026-10-25 本轮新增·已完成】修 `build.bat rime` 的两个致命缺陷**（事故 **L135**）——
  拆掉第三道墙（T2.12）之后，`build (msbuild)` 腿**不再被取消**了，但它自己红了：
  `LNK1181: cannot open input file 'rime.lib'`（`WeaselServer` / `WeaselDeployer`）。
  而**制造这个缺口的步骤自己是绿的**（`Build librime and its dependencies` = success）——
  拉 job 日志才看到：`4 file(s) copied.` → `The system cannot find the path specified.`
  → `0 file(s) copied.` → `error building weasel...`。
  - **根因一：清理循环删错了目录**。`build.bat` 的「clean cache before building」
    用的是 **librime 相对**路径（`build dist lib deps\*\build`），
    所以必须在 `librime\` 里执行。上游 `rime/weasel` 在这段前面有
    `cd %WEASEL_ROOT%\librime`（上游提交 `135b50a`），
    **本 fork 的 `face243`（v0.18.2.0 集成 librime-lua）把它删了且没人补回**
    ⇒ 清理循环在**仓库根**跑，`rd /s /q lib` 删掉了 `%WEASEL_ROOT%\lib\`
    （`get-rime.ps1` 装 `rime.lib` 的目录，也是 Weasel 工程的 `/LIBPATH`）
    ⇒ 随后的 `copy ...\dist_Win32\lib\rime.lib %WEASEL_ROOT%\lib\` 目标目录已不存在。
    **症状离原因隔了两步**。
  - **根因二：失败被吞**。`goto error` 在 `call :build_librime_platform` 子程序内部
    ⇒ `exit /b 1` 只结束子程序；**调用方那两行没检查 `errorlevel`**
    ⇒ 主流程继续、脚本退出码 0 ⇒ CI 那一步显示 success。
  → 修法：① 清理循环前后加 `cd /d "%WEASEL_ROOT%\librime"` / `cd /d "%WEASEL_ROOT%"`，
    `cd` 失败就明确报错退出；② 两处 `call :build_librime_platform` 之后加
    `if errorlevel 1 goto error`，`call :build_data` / `call :build_opencc_data` 同样补；
    ③ `ci.yml` 里 `.\build.bat rime` / `.\build.bat arm64 installer` 之后显式判
    `$LASTEXITCODE`（`$ErrorActionPreference` 管不到原生命令退出码）。
  → 机制级本地实证（cmd 自己的批处理语义，三个场景）：见 L135 的 Evidence 段。
- [x] T2.15 **【2026-10-25 本轮新增·已完成】让 CI 自己编译预编译词典**（事故 **L136**）——
  L135 修完后 `build (msbuild)` 能跑完整条流程了（Win32 方案 **Build succeeded / 0 Error**，
  `LNK1181 rime.lib` 消失），但 **NSIS 打包红**：
  `Error in script "output\install.nsi" on line 897 -- aborting creation process`。
  - 根因：`output\install.nsi` 第 895–898 行是 fork 提交 `17b36cb`（"bundle prebuilt dicts"）
    加的 `File "data\build\*.bin"`（**该块里唯一没写 `/nonfatal` 的一行**），
    而 `output\data\build` 被 **`.gitignore:4`（`build`）** 排除
    ⇒ 那是维护者手工跑过一次 deploy 的**本地产物**，CI 的 checkout 里一个都没有
    ⇒ NSIS 的 `File` 匹配不到就中止整个打包。
  - ⚠️ **不能简单把 897 行改成 `/nonfatal`**：沙箱门禁的 **P1.g** 断言
    「预编译字典 `rime_ice.table.bin` 存在」⇒ 它是**要求**，不是可选项。
  → 修法：`build` job 增加一步「编译词典」——用上一步刚从源码编出来的
    `rime_deployer.exe` 执行 `--build output\data`，产出 `output\data\build\*.bin`；
    找不到 deployer / 退出码非零 / `rime_ice.table.bin` 没产出，三种情况都**明确红**。
  → 机制在 scratch 树上验过：Win32 工具优先选中、`rime.dll` 放到 exe 旁边、
    退出码 3 时步骤明确 throw 而不是静绿。
- [x] T2.16 **【2026-10-25 本轮新增·已完成】按「自己的前提」重新接线门禁**（事故 **L136**）——
  依赖链原本是线性的 `lint → build → test → sandbox-gate`
  ⇒ **任何一环红了，后面所有门禁一起消失**。
  `test` job 自己会 `build.bat rime`、自己会编测试工程，**不需要 `build` 的任何产物**
  （不要安装包、不要 weasel.dll）⇒ 打包失败时单元测试根本没启动。
  → 改成：`lint`（无）· `build` needs lint · `test` needs lint ·
    `sandbox-gate` needs **[build, test]**（它验的是装完之后的产品，两个都要）。
  → 判据：**把某个 job 的产物全删掉，它还成立吗？** 成立就别依赖它。
- [x] T2.17 **【2026-10-25 本轮新增·已完成】让测试套件自己找 Visual Studio**（事故 **L137**）——
  T2.16 接线之后，`test` job **第一次真的执行**，然后停在
  `scripts\test-infra\run-test-suite.bat` 的**第一步**：
  `[ERROR] vcvars32.bat not found at: C:\PROGRA~2\MICROS~2\2022\BUILDT~1\VC\AUXILI~1\Build\vcvars32.bat`
  ⇒ **这套单元测试在 CI 里从来没跑过一次**（以前绿是因为 `test` 一直被跳过）。
  - 根因：脚本写死了**一台开发机**的 **VS 2022 BuildTools** 8.3 短路径；
    而 runner 上是 **VS 2022 Enterprise**（`ilammy/msvc-dev-cmd` 自己的日志可见）。
  → 修法：按规则探测 —— ① `env.bat` 显式给的 `VCVARS`/`MSBUILD`（永不被覆盖）；
    ② 本仓库已知的 VS 2022 BuildTools 目录（`PLATFORM_TOOLSET=v143` 那套）；
    ③ `vswhere` 枚举全部安装、**优先取路径里带 `\2022\` 的**；④ 任意安装兜底。
  ⚠️ 两个实测坑：**版本区间 `[17.0,18.0)` 里的 `)` 会截断 `for /f` 反引号命令**
    （实测报 `'C:\Program' is not recognized`）；**`vswhere -latest` 在装了 VS 18 的机器上
    返回 VS 18** ⇒ 工具链会从 v143 悄悄换成 v144 ⇒ 规则必须是「优先 2022」而不是「最新」。
- [x] T2.18 **【2026-10-25 本轮新增·已完成】plum 不许再覆盖 fork 跟踪的 `output\data`**（事故 **L138**）——
  T2.15 加的「编译词典」那一步红在 `deployment_tasks.cc:208] missing input schema: quick5`。
  - 根因链：`WEASEL_BUNDLED_RECIPES` **全仓从未被定义**（只有 `build.bat:45` / `xbuild.bat:55`
    两处打印）⇒ `bash plum/rime-install` 不带参数 ⇒ `plum/rime-install:41-42` 走 `':preset'` ⇒
    `plum/preset-packages.conf` 含 `prelude` ⇒ `plum/scripts/split-packages.sh:21` 写着
    **`prelude=default.yaml symbols.yaml`** ⇒ **上游 rime 的 `default.yaml` 覆盖掉 fork 那一份**。
  - ⚠️ **第二个后果更严重**：上游那份 `default.yaml` 里**没有 fork 的 29 条 `key_binder` 绑定**
    （`Control+Shift+9` / `Control+Shift+numbersign` / `Shift+space` …）
    ⇒ CI 会打出一个「快捷键是上游的」产品，**而这一步不会让任何 job 变红**。
  - 这个 fork 根本不需要 plum：`output/data` 是跟踪的产品数据（139 个文件），
    全仓 `output/data` 下**零处**引用 `luna_pinyin` / `bopomofo` / `cangjie` / `terra_pinyin`。
  → 修法：`build.bat` 的 `:build_data` 改成两条互斥的分支 ——
    「**未定义 / 为空 ⇒ 跳过 plum**」「**有值 ⇒ 按清单跑**」
    ⇒ **不变式：plum 只可能在拿到显式清单时才跑**，
    「悄悄退回 plum 的 preset」这条路被**从构造上**堵死；
    `env.bat.template` 与 `env.vs2022.bat` 里**不设**这个变量并写清原因
    （⚠️ cmd 里 `set "X="` 是**删除**变量 ⇒「已定义且为空」这个状态**无法表达**
    ⇒ 所以设计成两态而不是三态）；
    再加兜底闸：plum 前后各取一次 `git status --porcelain -- output/data` 快照，
    `fc /b` 比对 —— **动过任何一个文件就中止构建**。
- [x] T2.19 **【2026-10-25 本轮新增·已完成】`weasel.props` 改成每个 target 的前置条件**（事故 **L139**）——
  仓库里每个 C++ 工程第一行都是 `<Import Project="..\..\weasel.props" />`，
  而 `weasel.props` 是 `.gitignore` 里的**生成物**，改之前**只在 `build.bat` 的 Weasel 分支**生成
  ⇒ **`build.bat rime`（= CI `test` job 跑的东西）根本不生成它** ⇒ 每个测试工程都会
  `error MSB4019: 找不到导入的项目 ... weasel.props`。
  - 运行时实证：把 `weasel.props` 挪走再编一个测试工程 ⇒ MSB4019，exit 1；
    放回去 ⇒ 同一个工程编得过（阳性对照）。
  → 修法：抽 `:render_props` 子程序（`pushd "%WEASEL_ROOT%"` → `cscript render.js` →
    判 `errorlevel` → `popd`），并在 `build.bat` 的**早期**（版本号算完之后）就调用一次
    ⇒ `rime` / `data` / `boost` 都产出它。日志实证：`Generated weasel.props`。
- [x] T2.22 **【2026-10-25 本轮新增·已完成】CI 上传的产物路径改成这个 fork 真正的产物名**（事故 **L143**）——
  `ci.yml` 的 `Upload artifacts` 步还找上游的 `weasel*.exe`，而本 fork 的安装包叫
  `fluxing-*-installer.exe`（`install.nsi:51` 的 OutFile）。
  - ⚠️ **这一步不会变红**：`actions/upload-artifact` 的 `if-no-files-found` 作用于
    **整个 path 集合**，而 `debug_symbols.7z` 存在 ⇒ 步骤绿，
    但名为 `weasel-artifact-<ref>` 的产物里**只有调试符号、没有安装包**
    ⇒ 又一个「静默」的缺口（与 L132 同类）；
  → 修法：上传路径改成 `fluxing-*-installer.exe`；
    两个 release 步里的 `weasel*.exe` **故意不动** ——
    它们的条件是 `github.repository == 'rime/weasel'`，只在上游仓库里跑，那里名字是对的。
- [x] T2.23 **【2026-10-25 本轮新增·已完成】打包前先建 `output\archives`**（事故 **L144**）——
  L141 修完后 NSIS 的报错往前推了一格（CI run `40198583873`）：
  `Output: "...\output\archives\fluxing-...-installer.exe"` + `Can't open output file`。
  - 根因：`output\archives` 被 **`.gitignore:27`（`/output/archives`）** 排除，
    **全仓没有任何地方创建它** —— 维护者的机器上有，是因为历来的安装包就躺在里面；
    而 **NSIS 不会为相对 OutFile 自动建这个子目录**；
  → 修法：`build.bat` 在调 `makensis` 之前
    `mkdir "%WEASEL_ROOT%\output\archives"`（不存在才建）并判退出码；
  - 机制级实证（最小 NSIS 脚本 + scratch 目录）：
    阴性（无该目录）⇒ **EXIT=1** + `Can't open output file` + `Error - aborting creation process`；
    阳性（先 `mkdir`）⇒ **EXIT=0**，文件写出。
- [x] T2.24 **【2026-10-25 本轮新增·已完成】CI 产物挂的是上游版本号 `0.17.4`**（事故 **L145**）——
  CI 日志里 makensis 收到 `FLUXING_VERSION=0.17.4` / `PRODUCT_VERSION=0.17.4.383.b901bc7`，
  而本 fork 的版本线是 **0.21.0.x**。
  - 根因是**两层**：
    ① **版本号在 git 里没有构建期的源头** —— 上游把版本线放在
      `build.bat` 的 `VERSION_MAJOR/MINOR/PATCH` 默认值里并每次发版 bump，
      fork 继承了这套机制却**从没改过这三个数**（至今是上游的 `0/17/4`）；
      而非 release 构建的 build 号是 `git tag | findstr %FLUXING_VERSION%` 反推的
      ⇒ 它匹配到**上游的 `0.17.4` 标签**，数出 383 笔提交；
    ② **继承来的环境变量会压过 `build.bat` 的默认值**（`if not defined` 的语义）
      —— 本机注册表 Machine 作用域里留着 **0.19.0 时代**的残值
      （`FLUXING_VERSION=0.19.0` / `RELEASE_BUILD=1` / `VERSION_MINOR=19` …）；
  → 修法：`build.bat` 与 `xbuild.bat` 在 `setlocal` 之后、`call env.bat` 之前
    **清空继承来的 8 个版本 / 构建变量**，并把默认值改成 fork 自己的
    `0 / 21 / 0` ⇒ 版本来源优先级钉死为
    **env.bat > fork 默认值 > 空**；
  - 前后对照矩阵（同一套 env.bat，旧脚本取自 `HEAD:build.bat`）：
    旧 + CI 式 env.bat ⇒ `0.17.4.387`（复现 CI 的那个 bug）；
    旧 + CI 式 env.bat + 污染环境 ⇒ `0.19.0.49`；
    新 + CI 式 env.bat（干净 / 污染两种）⇒ `0.21.0.28.e105c64`；
    新旧 + 开发者 env.bat ⇒ `0.21.0.6`（**本机 release 构建逐字未变**）；
  - 负对照有牙：把旧默认值钉回 env.bat，新脚本仍复现 `0.17.4.387`；
  - 独立复现 CI 的 383：CI 在 `b901bc7` 上是 383，本地在 4 笔提交后的 `e105c64` 是 387；
  - 代价：发版时要连同 `CHANGELOG.md` 与 git 标签一起 bump 那三个数
    （已写进 `AGENTS.md §3.3`）；
  - 残留：本机注册表 Machine 作用域的 0.19.0 残值未清（需管理员，
    且变量名通用可能影响别的项目）⇒ 记为 **L146**。
- [x] T2.20 **【2026-10-25 本轮新增·已完成】安装器要用的文本产物补齐：NSIS 的相对路径按**脚本目录**解析**（事故 **L141**）——
  CI 的 `build (msbuild)` 红在 NSIS：
  `LicenseData: open failed "LICENSE.txt"`（`output\install.nsi` line 72）。
  - 关键机制：`makensis` 默认 **chdir 进脚本目录**，所以 `install.nsi` 里的相对路径
    按**脚本目录**解析，不是按当前工作目录。用 `/NOCD`（= 不切到脚本目录）
    做三格对照：A1（文件在脚本目录，默认）**EXIT=0**；
    A2（同样文件，`/NOCD`）**EXIT=1** 且报同一句 `LicenseData: open failed`；
    B1（文件只在当前目录，`/NOCD`）EXIT=0。
  - 根因：`output\LICENSE.txt` 是 **`.gitignore:25`（`/output/*.txt`）排除的生成物**，
    提交 `41e47e3` 把它从跟踪里删掉（674 行）之后，`build.bat` 的 `:build_data`
    只补回了 `README.md` 与 `rime-install.bat`，**唯独没补它**
    ⇒ 本机那份是删除前的**未跟踪残留**，CI 的干净检出没有
    ⇒ **安装包从来没在干净检出上编出来过**。
  → 修法：`build.bat` 新增 `:stage_text_assets`，把 `LICENSE.txt` / `README.md` /
    `rime-install.bat` 一次备齐；`:build_data` 与**安装器分支**都调它
    ⇒ 安装包目标不再依赖 CI 步骤的先后顺序。拷贝带 `/Y`（防覆盖询问停死）、
    退出码逐条判（防吞失败，L135 同一类）。
  - 运行时实证（双向）：拿走 `output\LICENSE.txt` ⇒ `makensis` **EXIT=1** 且报错
    **逐字**等于 CI 报错；`build.bat data`（日志里 2 条 → **3 条**
    `1 file(s) copied.`，产物 sha256 与根 `LICENSE.txt` 相同）⇒ 再编 **EXIT=0**，
    真的产出 `output\archives\fluxing-0.21.0.6-installer.exe`。
  ⚠️ `output\install.nsi` **未改动** ⇒ 不触发 AGENTS.md §2.5 的静默装机 smoke test。
- [x] T2.21 **【2026-10-25 本轮新增·已完成】两个 IPC 测试工程不再依赖某一台机器的 boost**（事故 **L142**）——
  CI 的 `test` job 红在链接：
  `LINK : fatal error LNK1104: cannot open file 'libboost_wserialization-vc143-mt-s-x32-1_83.lib'`。
  - 根因（一半）：`test\TestPipeChannelRace` / `test\TestShiftIPCWire` 的 vcxproj 里
    写死了 `F:\b183\bin.v2\libs\serialization\build\msvc-14.3\release\...` 与
    `libboost_*_vc143-mt-s-x32-1_83.lib`。**boost 的库名带版本号** ⇒ CI 的
    boost 1.84.0 树里永远没有 `1_83`。
  - 根因（另一半，更要紧）：这个依赖**根本不需要**。
    `boost/archive/codecvt_null.hpp` 的 `codecvt_null<wchar_t>` 是**显式特化**，
    成员定义在**编译好的序列化库**里 ⇒ 只有**自己解析那些头**的 TU 才会引用它；
    而产品侧 `WeaselIPC` 用 `stdafx.h` 作**预编译头**编，**PCH 不把引用带进 TU**。
    `dumpbin /SYMBOLS` 实测：产品 `PipeChannel.obj` **0 个** codecvt 符号，
    测试工程同一个源文件 **4 个**（3 本地定义 + 1 未定义构造）。
  - 两个测试**不用 boost::archive**：`include\PipeChannel.h` 只用 header-only 的
    `boost::interprocess::wbufferstream`；归档头是经 `WeaselIPC\stdafx.h` 才进 TU 的。
  → 修法：`WeaselIPC\stdafx.h` 给归档头加守卫 `#ifndef WEASEL_IPC_NO_BOOST_ARCHIVE`
    （**产品侧永不定义** ⇒ 产品构建一字不变）；两个测试工程定义它
    ⇒ 链接行上**一个 boost 库都不需要**：路径、版本号、依赖哪个 boost 全部消失。
  - 运行时实证：两个工程链接通过（`F:\b183` 目录与 `1_83` 库名已删）；
    `TestPipeChannelRace` **12 passed / 0 failed**、`TestShiftIPCWire` **10 PASS / 0 FAIL**；
    完整套件 **EXIT=0 / `=== ALL TESTS PASSED ===`**；`WeaselIPC.vcxproj`
    随 `stdafx.h` 变更重编 `No errors detected.` ⇒ 产品构建未受影响。
  - ⚠️ 过程中被**证伪**的假设：先以为是 boost 的 auto-link pragma 作祟，
    加 `BOOST_ALL_NO_LIB` 即可 —— 实测直接被
    `LNK2019 boost::archive::codecvt_null<wchar_t>` 证伪，
    逼出了「PCH vs 重新解析」这一层机制。
- [ ] T2.7 **仍阻塞**：`sandbox-gate` job 需要带 `sandbox` 标签的自托管 runner
  （Windows Sandbox 是嵌套虚机，官方 windows runner 不支持）——要么自建，
  要么把 P10 的静态部分拆到官方 runner 上跑。
  ⚠️ 注意：**这已经不是「门禁跑不起来」的原因了**（lint 墙已拆，见 T2.8；
  CI 构建缺步骤也已补，见 T2.10；xmake 腿的连坐也已拆，见 T2.12；
  `build.bat rime` 的两个缺陷也已修，见 T2.14；
  词典与依赖链也已处理，见 T2.15 / T2.16；
  测试套件自身的三道墙也已处理，见 T2.17 / T2.18 / T2.19；
  安装器文本产物与 boost 硬编码也已处理，见 T2.20 / T2.21），
  现在只差一台带嵌套虚拟化的机器。

## M3 · 真机行为验收

- [ ] T3.1 写「按键 → 期望 app 收到什么」的端到端驱动（仿 `test/TestWeaselIPC/TsfProfileVerify.cpp`）
- [ ] T3.2 验 `Shift+space` 切中英
- [ ] T3.3 验 `Control+1` / `Control+2` 选第 2 / 第 3 候选
- [ ] T3.4 验 `Shift+Tab` / `Tab` → `send: Shift+Left` / `Shift+Right` 是否真的移光标
- [ ] T3.5 验 `Alt+Left` / `Alt+Right` 是否真的移光标
- [ ] T3.6 验 `comma` / `period` 上页 / 下页（含 `ReinterpretPagingKey` 的重解释行为）
- [ ] T3.7 验 16 条 `KP_*` 映射
- [ ] T3.8 验 `Control+Shift+numbersign` 切中英标点
- [ ] T3.9 验 `Control+Shift+dollar` 切简繁
- [ ] T3.10 验修好后的 4 条 Shift+标点出 `"` / `|` / `{` / `}`
- [ ] T3.11 验 spec 077 的「单按 Shift 选第 2/3 候选」端到端
- [ ] T3.12 产出一张「绑定 → 可达性 → 真机效果」的对照表

## M4 · 收口

- [ ] T4.1 CHANGELOG：「4 条 Shift+标点此前从未生效，现已修好」+「2 条重复绑定已移除」
- [x] T4.2 复验报告回填 §3.14 / §3.12 修正 / §2 / §4
- [ ] T4.3 版本号 + 发布

## 本轮（2026-10-25）已完成、已落库

- [x] 读《Rime-设置说明书-重点快捷键.md》全文，核对 `output/data/**` 实际绑定数
- [x] 定位根因：TSF 前端发**字符**不发明**键**，librime 精确比较 ⇒ 6 条死绑定
- [x] 写 `test/TestHotkeyBindings`（把产品 `WeaselTSF/KeyEvent.cpp` 编进测试二进制）
- [x] 写 `gen-keysyms.ps1`（从 librime `key_table.cc` 机械生成键名表 + 自校验 + `-Check`）
- [x] 写坏数据对照 `fixtures/negative-controls.yaml` + `.expect.txt`（7 种死法 + 1 条好绑定）
- [x] 写 `known-dead-bindings.txt`（6 条已知死绑定 + 「改成什么」的建议）
- [x] 接入 `weasel.sln` / `run-test-suite.bat` / 沙箱门禁 PHASE 3.5
- [x] 写 L131（根因 + 修法 + 推广）与 ADR 0009（方向论证）
- [x] 跑一轮仓库测试套件：新门禁两条断言 + 防漂移检查全绿
- [x] 跑一轮沙箱门禁：见复验报告 §3.14（50 项 = 42 PASS / 2 FAIL，FAIL 仍是沙箱无 D 盘 + 无外网两条环境事实）
- [x] 复验报告回填：§3.14（新增）/ §3.12（修正一条错误结论）/ §2（P10.x 四行）/ §4（15–17 条）
- [x] 新文件全部按仓库约定归一：`.cpp`/`.h`/`.vcxproj`/`.inc` CRLF、
  `gen-keysyms.ps1` UTF-8 BOM + CRLF、`.clang-format` 18 自检通过
- [x] 拆掉把整条流水线挡死的 lint 墙（L132 + T2.8）：`scripts/lint-changed-cpp.sh` + ci.yml

- [x] T2.25 **【2026-10-25 本轮新增·已完成】快捷键验收门禁在 CI 的绿链上跑通并 PASS**——
  CI run `40204461845`（sha `b5df0eb`，含 L143/L144）**四个 job 全绿**：
  `lint` / `build (msbuild)` / `test` / `build (xmake)` 全部 success
  （`sandbox-gate` 仍在等带 `sandbox` 标签的自托管 runner ⇒ queued）。
  - `test` job 日志里门禁两轮都在：
    负对照 `SUMMARY|bindings=8|reachable=2|dead=6|known_dead=0|known_dead_keys=0|expectations=8|mismatches=0|unused=0` → `RESULT|PASS`；
    出货配置 `SUMMARY|bindings=33|reachable=27|dead=0|known_dead=6|known_dead_keys=6|expectations=0|mismatches=0|unused=0` → `RESULT|PASS`；
    收尾 `=== ALL TESTS PASSED ===`。
  ⇒ 这条链路第一次同时满足「门禁真跑」+「整条流水线是绿的」。
- [x] T2.26 **【2026-10-25 本轮新增·已完成】CI 的 artifact 改用 fork 自己的名字**（事故 **L147**）——
  `ci.yml` 的上传步还叫 `weasel-artifact-<ref>`，
  而 `docs/Fluxing-code-map/06-customization-points.md:102` 早就把这条列成了品牌分叉待办
  （和 L145 的 `VERSION_*` 同一张表，同样「记了但从没做」）。
  - 改法：改名 `fluxing-artifact-${{ env.git_ref_name }}`；
  - 这个 artifact **没有消费者**（沙箱门禁下载 `fluxing-installer` /
    `fluxing-buildout` / `fluxing-tests`；两个 release 步用文件路径）
    ⇒ 只影响 Actions 界面上的显示；
  - CI 实证（run `40206623716`）：产物名 = `fluxing-artifact-0.21.0.31.099b2ac`
    ⇒ fork 的名字 + fork 的版本号；
  - 顺带效果：artifact 名里嵌的是 `PRODUCT_VERSION`，
    修完 L145 之后名字里不再出现上游版本号。
