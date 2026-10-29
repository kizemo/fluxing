# 0018. 常驻面板的 idle 透明度地板与裸 Shift 部分选词的 flush 判据

- **状态**: Proposed
- **日期**: 2026-10-29
- **关联**: spec `087-shift-side-and-quickpanel-persist` / ADR 0013 D3 / ADR 0016 D3 / ADR 0012 / ADR 0013 D7 / lessons-learned L03 / L04
- **相关**: `include/QuickPanelAlpha.h` · `WeaselServer/QuickPanelDialog.cpp` · `WeaselServer/WeaselServerApp.cpp` · `include/ShiftStateMachine.h` · `RimeWithWeasel/RimeWithWeasel.cpp`

## 背景（Context）

v1.1.0 真机验收（2026-10-29）回报两条缺陷，都是 spec 087 / 016 已「收口」后又被用户撞到的：

1. **快捷面板在有输入动作或点击后仍然消失**。ADR 0016 已把 FocusOut、100ms 轮询器、
   `OnKillFocus`、`WM_ACTIVATEAPP` 四类被动收起路径全部加了 `AlwaysShowMode()` 守卫，
   用户仍报「会消失」。
2. **裸 Shift 选第 2/3 候选时连带上屏**。方向问题已由 ADR 0017 修好；新症状是：输入
   两个字以上的双拼，第 1 候选是词、第 2/3 候选是单字时，按 Shift 上屏该单字的**同时
   把第 1 候选词的第二个字也带上屏**；而数字键 2/3 没有这个现象（数字键上屏单字后
   继续为剩余音节出候选）。

## 决策（Decision）

### D1（缺陷 1 · 「点击」支）把 Hide() 守卫补到 `onPhrases` 调用点，而不是 `Hide()` 本体

全仓取证：`WeaselServer/QuickPanelDialog.cpp:1438` 的 `Hide()` 本体仍无条件
`ShowWindow(SW_HIDE)`（ADR 0013 D7「`Alt+,` / ESC 明确收起手势」必须继续生效，
不能吞掉用户意图）。真正的漏网点在 `WeaselServer/WeaselServerApp.cpp` 的
`QuickPanelDialog::registerCallbacks()` 里 `onPhrases` 的 `WM_LBUTTONUP` 处理：
v0.19.0.39 为了让子 `PhrasesDialog` 拿到键盘焦点，刻意做了 `Hide() → Show() → BringToTop()`
的 workaround，这是**当时唯一未守卫的生产 Hide() 调用点**——点「词库」按钮正是那次「点击」。
修法：加 `!QuickPanelDialog::AlwaysShowMode()` 前置条件，非常驻态行为逐字节不变。

### D2（缺陷 1 · 「输入动作」支）idle alpha factor 由 51（20%）抬到 153（60%）

面板是 `WS_EX_LAYERED` + `UpdateLayeredWindow` **逐像素 alpha**（`QuickPanelDialog.cpp:1415`
明确写明 v0.19.0.10 起不再用 `SetLayeredWindowAttributes(LWA_ALPHA)`）。
`ApplyAlphaGradient()` 已把逐像素 alpha 压到 140→82，再乘 `s_alphaFactor`；
100ms 轮询器只要**光标不在面板矩形内**就把常驻态 park 到 `fluxing::kAlphaFactorIdle`。
打字时光标按定义在目标应用的编辑框里 ⇒ 有效 alpha ≈ 28→16，约桌面上的 **6-11%**：
面板从未被 Hide，只是淡到看不见，且每次 `Show()` 复用都从那个残留值起步。
故 ADR 0013 D3 的 20% 是「消失感」的直接成因，本 ADR 把地板抬到 **60%**（仍可辨地退到 hover 态之后），
并让 fade 步进注释与实际刻度一致（255→153 约 5 tick，153→255 约 5 tick）。
不新增开关：这是同一处 ADR 0013 D3 设计值的修订，不是新配置面。

### D3（缺陷 2）flush 判据 = 选词后是否还剩候选，纯函数 `ShouldCommitAfterSelect`

librime 侧（只读参照，P3）用**尾段**区分两种选词：

- **整词**：`Context::Select` → `ConcreteEngine::OnSelect`
  （`librime/src/rime/engine.cc:259-282`）发现 `seg.end == input.length()`，
  走 `composition().Forward()`（`segmentation.cc:114-120`）追加**零长度段、无 menu**
  ⇒ `get_context().menu.num_candidates == 0`，此时不 commit 就什么都不落屏 —— 这正是
  ADR 0012 要 `commit_composition()` 的场景。
- **部分选词**：`seg.end < end` ⇒ `Segment::Close`（`segmentation.cc:17-24`）截短并打
  `partial` tag，剩余音节被重新 `Compose` 成新的已翻译段 ⇒ menu **非空**。
  此时 `commit_composition()` 走 `Context::Commit`（`context.cc:18-26`）提交的是**整个
  open composition** = 选中的单字 **+ 剩余段第 1 候选词** ⇒ 用户看到的连带上屏。

数字键路径 `SelectCandidate()` 本来就只做 `select_candidate + _UpdateUI`、不 commit，
所以没有该缺陷。修法：`ShiftUp()` 的 `kSelectCandidate` 分支在 `select_candidate` 之后
`get_context`，`ShouldCommitAfterSelect(context_available, menu.num_candidates)` 为真才
`commit_composition()`——即**镜像数字键**。取不到 context 时保守返回 true，维持 ADR 0012
不出现「整词选完不落屏」的回归。判定逻辑放进 `include/ShiftStateMachine.h` 作纯函数，
可脱机在 `TestShiftIPCStateMachine` 断言（R2）。

## 后果（Consequences）

**正面**
- 「常驻」语义闭合：四类被动路径（ADR 0016）+ 调用点 Hide（D1）+ 可见性地板（D2）。
- 裸 Shift 与数字键语义一致：整词落屏、部分选词继续为剩余音节出候选。
- 新增 5 条机构建回归断言（`Test #22`），`ShouldCommitAfterSelect` 三个分支全覆盖。

**负面 / 需承担**
- 常驻态 idle 60% 比 20% 更显眼，长时间打字时面板残留在视觉场里的干扰上升；若用户反馈
  过吵，调 `include/QuickPanelAlpha.h:41` 的常数即可，**默认值改动须先补进本 ADR**。
- `ShiftUp()` 每次选词多一次 `get_context`/`free_context` IPC 往返（P2 非 TSF 阻塞路径，
  server 侧进程内调用，可接受）。
- 「输入动作」支只有代码取证、无 Hide() 调用点证据 ⇒ **装机复验是完成条件**（R6）。
- `TestWeaselIPC` 被本机运行中的 WeaselServer 占管道跑不起来（已知限制），
  沙箱权威门禁本机不可跑（T019）；判 CI 终态用 `gh api .../jobs`。

## 备选方案（Alternatives）

- **把守卫放进 `Hide()` 本体** —— 否决：违反 ADR 0013 D7，`Alt+,` / ESC 收起会被吞。
- **常驻态下把面板 alpha 钉在 255（完全不淡出）** —— 否决：抹掉 hover/非 hover 的层次，
  且与 ADR 0013「退到背景但仍在」的意图不符。
- **用 `get_input()` / caret 位置变化判定整词 vs 部分选词** —— 否决：取证表明 `OnSelect`
  两条分支都不改 `input_` 与 `caret_pos`，量不出差别。
- **`peek` commit 队列 / `get_commit` 判定** —— 否决：`get_commit` 会**消费**文本，
  不消费就没有信号，等于把用户该拿的字吞掉。
- **给 Shift 选词也开一个「不 flush」开关** —— 否决：这是缺陷修复不是偏好，加配置面
  违反 AGENTS §13「禁止随意新增配置项」。

## 参考

- `include/QuickPanelAlpha.h` · `WeaselServer/QuickPanelDialog.cpp`（`ApplyAlphaGradient` / 轮询器 / `Hide()`）
- `WeaselServer/WeaselServerApp.cpp`（`onPhrases` 的 `WM_LBUTTONUP` workaround）
- `include/ShiftStateMachine.h`（`ShouldCommitAfterSelect`）· `RimeWithWeasel/RimeWithWeasel.cpp`（`ShiftUp` / `SelectCandidate`）
- `librime/src/rime/{engine.cc,segmentation.cc,context.cc}`（只读参照，P3 不改子模块）
- ADR 0012（整词需 commit）· ADR 0013 D3/D7 · ADR 0016 D3 · ADR 0017（Shift 左右方向）
- `.specify/memory/lessons-learned.md` L03 / L04（TSF 回调内阻塞）
