83 任务清单

> 勾选框与 [plan.md](plan.md) 的任务编号一一对应。
> `[x]` = 已完成；`[ ]` = 下一阶段待办。
>
> 进度（2026-10-28）：**M0（调查与决策）+ M1（共享单元）+ M2（抓键改造）
> + M3（第二条路径）+ M5（门禁复跑）已完成**；**M4、M6 全部待做**。
> 决策见 [ADR 0011](../../../docs/adr/0011-capture-keys-by-produced-character.md)（`Proposed`）。

## M0 · 调查与决策

- [x] T0.1 死绑定坐实（对照实验）：把 `output/data/default.yaml` 副本里的 `Control+Shift+numbersign` 改成 `Control+Shift+9` 后跑门禁 ⇒ `DEAD_KEYCODE`（前端发 `0x0028 parenleft`，不是 `0x0039`）+ `RESULT|FAIL`，`bindings=27 reachable=26 dead=1`；同文件 `Control+Shift+numbersign` = `REACHABLE`
- [x] T0.2 F1 核验：librime `keys_by_keyval` **只有大写字母 A–Z**，`RimeGetKeycodeByName` 是纯 `strcmp` ⇒ `Control+f` 连解析都失败，只有 `Control+Shift+F` 可表达（`librime/src/rime/key_table.cc:2026-2033`）
- [x] T0.3 F2 核验：前端 `ToUnicodeEx` 前清掉 `VK_CONTROL` / `VK_MENU` ⇒ 字符大小写只由 Shift 决定（`WeaselTSF/KeyEvent.cpp:44-51`）
- [x] T0.4 F3 记为疑点：CapsLock 打开时前端额外带 `LOCK_MASK`（`WeaselTSF/KeyEvent.cpp:18-19`），而 librime 精确比较 modifier ⇒ **疑似全部失配** ⇒ 记为清单 **T20**，**先实测再修**
- [x] T0.5 两条抓键路径漂移坐实：`KeyRecorder.cpp` 把 `VK_OEM_PLUS` 映射成 `plus`，`ShortcutSettings.cpp:622-699` 映射成 `equal`；`KeyRecorder.cpp:118` 把 `win` 错拼成 `Control+`
- [x] T0.6 拍板 ADR 0011 的 D1–D6，写出 spec 083 的 R1–R7、验收表与实施步骤

## M1 · 共享单元 `include/KeyNameMapper.h`

- [x] T1.1 新增 header-only `include/KeyNameMapper.h`（**不新建 .vcxproj**）
- [x] T1.2 键名表与 `test/TestHotkeyBindings/keysyms_generated.inc` **同源同生成器**
- [x] T1.3 提供「字符 → 键名」「组合键 → 字符」两个方向的纯函数（无 GUI / 无 rime_api 依赖）
- [x] T1.4 复刻 F2（清 Ctrl/Alt、Shift 定大小写），与 `WeaselTSF/KeyEvent.cpp` 逐条对齐
- [x] T1.5 librime 无名的字符返回「不可表达」，**绝不产出死绑定**

> M1 落点（2026-10-28）：共享单元落在 `include/KeyNameMapper.h`（`namespace fluxing`），
> 键名表迁到 `include/keysyms_generated.inc`（include guard + `fluxing::keysyms` 命名空间 +
> 函数内 `static` 表，C++11 安全，不依赖生产工程的 C++ 标准），生成器仍是
> `test/TestHotkeyBindings/gen-keysyms.ps1`（输出路径改到 `include\`，并**保留 librime 原表序**，
> 使「首个同键值条目」== `RimeGetKeyName`）。
> `WeaselTSF/KeyEvent.cpp` 改为调用 `fluxing::ResolveCharacter`，掩码逻辑逐字保留。
> 门禁 `TestHotkeyBindings` 删掉本地副本改吃共享单元，并新增 `--selfcheck-mapper`
> （5 类交叉验证：表名回环 / accept 串回环 / 18 个固定和弦 / 14 个 librime 解析边界 /
> 前端 (vkey, ext, shift, ctrl, alt, keyup) 全组合枚举）。
> 两条坐实的事实：**librime 键表只有大写 A–Z** ⇒ `Control+a` 解析不了（F1 坐实）；
> **修饰键本身不能构成 `accept:`**，必须拒绝而不是改写（D6）。

## M2 · 抓键改造 `FluxingConfigEditor/KeyRecorder.cpp`

- [x] T2.1 `MapKeyCodeToLibrimeName`（:22-88）换成共享单元
- [x] T2.2 删掉 `:118` 的 `win → Control+` 误拼，改为**拒绝 + 提示**
- [x] T2.3 修饰键按前端掩码精确输出（Shift / Control / Alt）
- [x] T2.4 `Ctrl`+小写字母 ⇒ 拒绝并提示「请按住 Shift」

> M2 落点（2026-10-28）：决策全部收进共享单元 `fluxing::CaptureAccept()`
> （`include/KeyNameMapper.h`），`KeyRecorder.cpp` 只负责取快照 + 显示结果。
>
> - **T2.1**：`MapKeyCodeToLibrimeName` 整段删除；键名改由 librime 键表
>   （`fluxing::KeyName` / `fluxing::KeycodeByName`）给出，虚拟键不再自造名字。
> - **T2.2**：`win` 不再错拼成 `Control+`：`CaptureAccept` 见到 Win 键
>   （按下或按住）一律返回 `kAcceptWinKey`，对话框显示
>   「不支持 Win 键，请改用 Ctrl / Shift / Alt 组合」；实时预览里显示 `Win+`。
> - **T2.3**：修饰键只取 **Shift / Control / Alt**（与前端掩码同源：
>   `keyState[VK_SHIFT|VK_CONTROL|VK_MENU] & 0x80`）；**不输出 Lock**
>   （CapsLock 维度属 T20 / ADR 0011 Q2，未决，不在本次预判）。
> - **T2.4**：`Ctrl`+小写字母 ⇒ librime 键表只有大写 `A–Z`（F1/F4）⇒
>   `kAcceptNoName` ⇒ 拒绝并提示「该组合键无法表达，请按住 Shift 再试」。
>
> 字符与修饰键取自**同一次** `GetKeyboardState()` 快照，否则可能出现
> 「掩码带 Shift、字符却是小写」的死绑定。
> 附带修正：对话框占位提示原本是乱码（「亟慑组合键...」），已改回「请按组合键...」。


## M3 · 第二条路径 `WeaselServer/ShortcutSettings.cpp`

- [x] T3.1 `VKeyToName`（:622-699）切到共享单元
- [x] T3.2 `NormalizeCombo`（:701-718）切到共享单元，消除 `equal` vs `plus` 矛盾
- [x] T3.3 **不动**内置表（:339-402，硬编码 accept 串，`Control+Shift+F` 保持原样）

> M3 落点（2026-10-28）：第二条路径**删掉**自造的 `VKeyToName` / `NormalizeCombo`
> （不再有第二套键名 / 组合键拼法），改走共享单元 `fluxing::CaptureAccept()`
> ⇒ 两条路径产出的 accept 串同源。
>
> - **T3.1 / T3.2**：低阶键盘钩子（`LowLevelKeyboardProc`）用 `KBDLLHOOKSTRUCT`
>   造 `BYTE[256]` 快照（`state[vk]=0x80` + Shift / Ctrl / Alt / Win 位），
>   `KeyInfo` 由 `scanCode<<16 | LLKHF_EXTENDED` 构造，HKL 取 `GetKeyboardLayout(0)`，
>   再调 `fluxing::CaptureAccept()`。顺带消掉三处漂移：`Shift`+`=` 过去永远写
>   `equal`（永不触发），现在写 `Shift+plus`；`Ctrl`+小写字母过去写出 librime
>   解析不了的名字（死绑定），现在**拒绝**；`Super+`（Win）过去写出伪绑定，
>   现在**拒绝**并提示。
> - **T3.3**：内置表**一个字没动**。但共享单元按 librime 位序产出
>   `Shift+Control+F`，内置表写的是 `Control+Shift+F` —— 拼写不同、语义相同。
>   ⇒ 冲突检测的两处字符串精确比较改成**语义比较** `fluxing::AcceptEquals()`
>   （解析成 keycode + modifier 掩码再比），顺带让旧 yaml 里的 `Control+Shift+F`
>   与新抓的 `Shift+Control+F` 也能判成重复。
> - 拒绝原因文案收进共享单元 `fluxing::AcceptErrorMessage()`（两条路径共用，
>   宪法 P2 简体中文），落在 popover + 面板状态行；**拒绝时清空 pending chord**
>   （所见即所存）。
> - 门禁新增第 8 类自检「第二路径语义一致性」：14 条内置表用例 + `equal` /
>   `Shift+plus` 矛盾用例，输出 `MAPPER|builtin_equivalents=15`。
>
> M3 验收数字（本机实跑）：源门禁 `bindings=39|reachable=39|dead=0` +
> `RESULT|PASS`；部署产物重编 12 个 schema 后 `bindings=337|reachable=337|dead=0` +
> `RESULT|PASS`；`MAPPER-SUMMARY|failures=0`；全套件 `=== ALL TESTS PASSED ===`；
> 完整解决方案 Release/Win32 编译 exit 0；5 个改动文件 clang-format 整文件干净。

## M4 · 测试（抓键产物专项）

- [ ] T4.1 `test/TestHotkeyBindings/fixtures/recorder-output.yaml`
- [ ] T4.2 同目录 `.expect.txt`（逐条 `REACHABLE`）
- [ ] T4.3 负控：`Control+Shift+9` 必须仍判 `DEAD_KEYCODE`（防止门禁被改瞎）

## M5 · 门禁复跑（仓库源 + 部署产物）

- [x] T5.1 仓库源：`SUMMARY|bindings=39|reachable=39|dead=0` + `RESULT|PASS`（**数字不得变差**）
- [x] T5.2 部署产物：`bindings=337|reachable=337|dead=0` + `RESULT|PASS`
- [x] T5.3 全套件 `=== ALL TESTS PASSED ===`；`TestDefaultHotkeys` `Passed: 89 / 89`
- [x] T5.4 clang-format 整文件 clean

> M5 实跑数字（2026-10-28，全部本机可复现）：
>
> - 仓库源（`output\data\default.yaml` + 3 个 `user-custom\*.custom.yaml`）：
>   `SUMMARY|bindings=39|reachable=39|dead=0|known_dead=0|known_dead_keys=0|expectations=0|mismatches=0|unused=0`
>   + `RESULT|PASS` —— 与 M1 改动前的基线**逐字相同**。
> - 部署产物源 YAML（装器载荷 `pkg_100`）：同上 `39/39/0` + `RESULT|PASS`。
> - 部署产物**重新编译后**（`rime_deployer --build`，先去掉载荷里陈旧的 `data\build\` 再全量重编，
>   12 个 schema 全覆盖）：`SUMMARY|bindings=337|reachable=337|dead=0` + `RESULT|PASS`。
>   原定 `120` 那组是用户实机 `user1\fluxing\build\` 的口径，该目录本机已不存在；
>   本次覆盖面更大（12 个 schema 全部编译后产物）。
> - 负控（`fixtures/negative-controls.yaml`）：`bindings=9|reachable=3|dead=6` + `RESULT|PASS`
>   （门禁没被改瞎）。
> - mapper 自检：`MAPPER-SUMMARY|failures=0` + `RESULT|PASS`。
> - 全套件：`=== ALL TESTS PASSED ===`。
> - clang-format：3 个 C++ 文件整文件 `--dry-run --Werror` 干净。
> - 完整解决方案 Release/Win32 编译 exit 0（含 `output\weasel.dll`、`output\WeaselSetup.exe`）。
>
> **顺带查出一条既有遗留**：装器载荷里带着**陈旧的** `data\build\*.schema.yaml`
> （还是 ADR 0009 之前的 8 条死绑定）。它不参与本轮判定（用户目录由 librime 重新部署时重编），
> 但属于「装器多带了一份可能误导的编译产物」，留给后续打包里程碑处理。

## M6 · 打包与实机验收

- [ ] T6.1 版本号升 `0.21.0.10`（`env.bat`，gitignored）
- [ ] T6.2 `build.bat weasel installer` ⇒ `output\archives\fluxing-0.21.0.10-installer.exe`
- [ ] T6.3 交用户实机验收（抓键可用性 + 原有快捷键不回归）
- [ ] T6.4 实测通过后 ADR 0011 `Proposed` → `Accepted`

## T20 联动（CapsLock 疑点）

- [ ] T7.1 **实测**：CapsLock 打开时，已配置的快捷键是否真的全部失配
- [ ] T7.2 依据实测结果，在 ADR 0011 Q2 的三个候选中选一个并落到 spec / 清单
- [ ] T7.3 若选「前端清 `LOCK_MASK`」⇒ 独立 `fix(fluxing):` 提交 + `style(fluxing):` 前置提交 + 单测
