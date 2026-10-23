---
paths:
  - "output/data/**/*.yaml"
  - "output/data/**/*.custom.yaml"
  - "output/data/**/default.yaml"
  - "weasel.yaml"
  - "squirrel.yaml"
  - "ibus_rime.yaml"
  - "fcitx_rime.yaml"
---

# Rime 输入法设置规则

改 Rime 设置文件(schema / custom patch / weasel / squirrel / ibus-rime / fcitx-rime / default.yaml)时,**必须先读**:

[`Rime-设置说明书-重点快捷键.md`](Rime-设置说明书-重点快捷键.md)(项目根目录,1105 行,基于 librime 源码校准)

## 按需跳转章节

| 任务 | 必读章节 |
|---|---|
| 写 `patch:` 补靪 | §3 (YAML 编译指令、列表下标、/+) |
| 改 `key_binder/bindings` | §6 全文(when / accept / send / action / ascii_composer) |
| 改 `editor/bindings` | §7 (12 种 editor action + char_handler) |
| 改 `switches` / `menu` / `recognizer/patterns` / `punctuator` / `translator` / `app_options` / `switcher/hotkeys` | §5 + §8 |
| ascii_mode 切换问题 | §6.8 + §9.3 (WeaselTSF `RELEASE_MASK` 坑) |
| 改完不生效 / 部署报错 | §10 (日志路径 + 9 个常见坑) |
| 看本机 rime_ice 现状 | §9 (key_binder 22 条 + editor 默认 + ascii_composer 修复) |
| 改完想找例子 | 附录 B (10 个最常见定制场景,直接抄) |
| 不知道键名怎么拼 | 附录 A (X11 keysym 完整列表) |

## Why

`key_binder` 的 6 种 `when`、6 种 action(§6.3/§6.4)与 `editor` 的 12 种 action(§7.1)直接从 librime 源码 `key_binder.cc:21-28, 161-226` 与 `editor.cc:19-32` 校准。直接靠记忆写 binding 容易踩坑:

- **小狼毫 Shift_L release 事件 bug**: `WeaselTSF/KeyEvent.cpp:27-28` 强制加 `RELEASE_MASK`,librime `key_event.h:43` 的 `operator==` 比较 modifier 含 `RELEASE_MASK` → `accept: Shift_L` (modifier=0) 永远不匹配 release。`key_binder` 走不通,改走 `ascii_composer/switch_key: commit_text`(§9.3 fix)。
- **`bindings` 不写 `/+` 是替换,不是追加**: rime_ice default 的 8 条 `Shift_L/R` binding 在 patch 时要么整段继承,要么整段重写,不能用 `bindings/+` 合并。
- **`when: paging` 不会自己触发**: 必须先有 `Page_Down` 之类的翻页动作。误用 `paging` 想覆盖 `has_menu` 场景的 binding 会静默失效。

完整溯源在 `Rime-设置说明书-重点快捷键.md` 附录 C。

## How to apply

1. 改 Rime 设置前先 Read 说明书对应章节(不要凭印象写 binding / patch)。
2. 写完 patch 后,部署一次,看 `<用户文件夹>/build/<name>.schema.yaml` 是否如预期。
3. 涉及 key_binder 时,日志 `%TEMP%\rime.weasel.*` 看有没有 `invalid key binding #N` 警告。
4. 不要在 `output/data/` 下复制 default 文件再改 — 用 `default.custom.yaml` / `<name>.custom.yaml` 的 `patch:` 补靪。