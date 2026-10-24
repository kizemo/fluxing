# Handoff — v0.21.0.1 librime key_binder send_text patch (2026-08-10)

> **状态**:v0.21.0.0 已 ship + verified + L108 push 收尾完成 (commit `3ab771c`,divergence 真 0/0)。下一棒:**v0.21.0.1 = librime 升级 + 本地 fork patch 加 `send_text` 字段**,修 `output/data/user-custom/*.custom.yaml` 中 `key_binder/bindings` 的 `send: "《"` 等中文 parse error。
>
> `start_service.bat` mislabel 已修过(主 handoff §3 描述的是 fix **前** 状态),**v0.21.0.1 不再处理**。

---

## 0. 大白话总结

| 项 | 值 |
|---|---|
| 基线 commit | `3ab771c6fadc928e492af48d52efbc64aa6e9418` (Fluxing HEAD) |
| 基线 librime pin | `1c23358157934bd6e6d6981f0c0164f05393b497` (librime 1.13.1) |
| upstream librime HEAD | `4901c8a4a5dc4de4bf186e16527458baf478281b` (librime 1.17.0) |
| upstream 1.13.1..1.17.0 commits | **94** (增量 + fix + perf,无破坏性重构) |
| librime 1.17.0 `key_binder.cc` | **仍无 `send_text` 字段** — fix 必须本地 patch |
| Fluxing RimeApi C 调用 | 1.13.1 → 1.17.0 兼容(`include/` 无 commit 修改) |
| **next step 关键决策** | **(i) 只 patch librime 1.13.1 / (ii) 升级 librime 1.17.0 + patch** — 见 §3 |
| 关键文件 | `librime/src/rime/gear/key_binder.cc:186-194` (1.13.1) |
| 影响 yaml | `output/data/user-custom/{rime_ice,radical_pinyin,double_pinyin_sogou}.custom.yaml` |

---

## 1. 现状核实 (本 session verified)

### 1.1 librime 1.13.1 key_binder.cc 当前 send 解析逻辑

`librime/src/rime/gear/key_binder.cc:186-194`(1.13.1):

```cpp
if (auto target = map->GetValue("send")) {
  KeyEvent key;
  if (key.Parse(target->str())) {                // KeyEvent::Parse 只识别 ASCII key name
    binding.target.push_back(std::move(key));
  } else {
    LOG(WARNING) << "invalid key binding #" << i
                 << ", with invalid send pattern: " << target->str() << ".";
    continue;                                     // 中文 send 直接被 continue 跳过,binding 失效
  }
} else if (auto target = map->GetValue("send_sequence")) {
  if (!binding.target.Parse(target->str())) { ... } // 同样 KeySequence::Parse
} else if (auto option = map->GetValue("toggle")) { ... }
// set_option / unset_option / select 三个 option 分支
```

**所有非 KeyEvent 字符串(`《》？：""` 中文) 必 fail 报 "invalid send pattern"**,binding 被 silently drop,Shift+标点 完全无效。

### 1.2 upstream 1.17.0 的 key_binder.cc 完全相同(同分支结构,无 send_text)

```bash
$ git -C librime log --oneline 1c233581..origin/HEAD -- src/rime/gear/key_binder.cc
# (空 — 94 个 commit 没改这个文件)
```

→ **升级 librime 不解决问题**,必须本地 fork patch。

### 1.3 Fluxing RimeApi C 调用兼容性(重要)

- librime 1.13.1 → 1.17.0 的 94 个 commit 里 **`include/` 目录无 commit 修改**
- 意味着 `rime.h` 公共 C API 100% 不变 → WeaselServer / WeaselTSF 重 build 不需改 source
- 唯一风险:librime 内部行为变化(无测试覆盖就暴露)

### 1.4 user-custom yaml 中中文 send 实测

`output/data/user-custom/rime_ice.custom.yaml:52-59`:
```yaml
key_binder/bindings/+:
  - { when: always, accept: "Shift+comma", send: "《" }
  - { when: always, accept: "Shift+period", send: "》" }
  - { when: always, accept: "Shift+slash", send: "？" }
  - { when: always, accept: "Shift+semicolon", send: "：" }
  - { when: always, accept: "Shift+apostrophe", send: '"' }
  - { when: always, accept: "Shift+backslash", send: "|" }
  - { when: always, accept: "Shift+bracketleft", send: "{" }
  - { when: always, accept: "Shift+bracketright", send: "}" }
```

8 条 binding,**前 6 条全中文 send → librime parse 全 fail**。装 v0.21.0.0 后 rime_ice 用户的 Shift+标点 → 中文映射是 **dead binding**(silent)。

### 1.5 start_service.bat mislabel 已修(无需再动)

`output/start_service.bat` source = `cd "%~dp0" + start "" WeaselServer.exe` (正确 START);装机路径 `D:\Program Files\Fluxing\weasel\start_service.bat` 同;installer archive 2025-06-04 build 内容一致。主 handoff §3 描述的是 fix 前的状态,**已修过**。

---

## 2. 必须强制 (per CLAUDE.md)

继承 handoff-github-push-2026-08-10.md §5 + §6 的所有强制规则,新增:

1. **librime submodule 改动需谨慎**:改 `librime/src/` 内任意文件后,submodule 引用 hash 会变,Fluxing main commit 必须同步指向新 librime commit(否则 submodule 引用 pin 死旧版本)
2. **绝对不能** commit dirty librime submodule 引用 (当前 `1c233581-dirty` 已存在,只能 add 已 commit 的 librime 引用,不能 add dirty)
3. **librime patch 必须 commit 到 librime submodule 自己的 history**(作为 Fluxing fork 的本地 commit,不是 patch 文件)
4. **改 librime source 必 build test**:rebuild librime + Fluxing weasel + TestPipeChannelRace 12 PASS(per spec 076 ship-gate)
5. **md5 dual-verify 是 ship-gate**:extract archive md5 == user dir md5,差异 > 0 必 block
6. **silent failure 必带 loud log**(per CLAUDE.md)

---

## 3. 必交付(分层:决策 → 实现 → verify → ship)

### 3.1 P1 决策 (用 brainstorming skill 决定)

| 决策点 | 选项 | 评估 |
|---|---|---|
| **librime 升级策略** | (A) 仅 patch librime 1.13.1 (改动最小,API 已验) | 风险低,scope 最小 |
| | (B) 升级 librime 1.13.1 → 1.17.0 + patch (拿 94 commit 改进) | scope 大,需回归测 |
| **send_text 字段设计** | (i) 新分支 `else if send_text` + `engine->context()->CommitText(s)` | 干净,需 verify Context::CommitText API |
| | (ii) 扩 `send` 分支:识别 "Shift+comma" 等无法 parse 时 fallback commit_text | hack,但 schema 端零改动 |
| **librime fork vs upstream** | (a) librime submodule 加 commit hash,push 到 kizemo/Fluxing remote(submodule 不存独立 commit) | Fluxing repo 内可见,需 backup tag |
| | (b) librime 推到独立 kizemo/librime fork repo,submodule URL 改 fork | 长期清晰,需另开 librime repo + push |

**推荐组合**: A + i + a — patch 1.13.1(最小改动)+ send_text 新分支(干净)+ librime commit 留在 Fluxing submodule 引用里。

### 3.2 P1 实现 (按 writing-plans skill 写 plan)

1. **brainstorm** v0.21.0.1 send_text 字段 design(命名、默认值、commit_text 调用、unit test)
2. **patch** `librime/src/rime/gear/key_binder.cc:194` 后加 `send_text` 分支
3. **unit test** (librime/test/ 内加 test_key_binder_send_text.cc) — Shift+key send "《" → assert Context::commit_text() 含 "《"
4. **commit librime** (librime/ 目录下 git commit + cd .. + 同步 submodule 引用)
5. **rebuild librime** (Windows build chain per `env.bat` + `RELEASE_BUILD=1`)
6. **rebuild Fluxing stack** (weasel + weaselpanel + installer)
7. **md5 dual-verify** (extract installer archive + check file md5 == user dir md5)
8. **commit** Fluxing main commit message: `feat(Fluxing+librime): v0.21.0.1 key_binder send_text field`
9. **push via SSH** (per L108): `git push kizemo-ssh <sha>:Fluxing` (单 commit,无需 chunked)
10. **sandbox-verify** `rime-verify.ps1 -InstallerPath release\fluxing-0.21.0.1-installer.exe -NoWait`
11. **SFTP** + 装机验证 (主 handoff §6 流程)
12. **create tag** `v0.21.0.1` annotated on Fluxing commit

### 3.3 P2 记录

- **lessons-learned L109**: librime key_binder send_text fork pattern + patch 持久化策略
- **lessons-learned L110**: librime 1.13.1 → 1.17.0 upgrade 路径(为将来升级做 reference,即使 v0.21.0.1 不升级)
- **ADR** `docs/adr/NNNN-librime-fork-strategy.md`(决策 (B) vs (A),(b) vs (a) 的最终选择)

---

## 4. 环境 / 分支 / 基线状态

| 项 | 值 |
|---|---|
| 工作目录 | `F:\soft\00selfmade\rime_claude` |
| Git branch | `Fluxing` (HEAD = `3ab771c6fa...`) |
| Git status | clean modified (除 librime dirty + 181 untracked) |
| Divergence | **0/0** (L108 `65557d8` + 补遗 `3ab771c` 都已 push) |
| Remotes | `kizemo` (HTTPS) + `kizemo-ssh` (SSH) |
| librime submodule | `1c233581-dirty` (dirty = build artifacts,**不要 commit 引用**) |
| 装机 base | `D:\Program Files\Fluxing\weasel\` (v0.21.0.0 verified) |
| env.bat FLUXING_VERSION | `0.21.0`(需改 `0.21.0.1`) |
| WEASEL_BUILD | (待 grep env.bat / weasel.props 确认格式) |
| installer md5 baseline | `a43c72e7a8bf66b270ad34bef8846030` (v0.21.0.0) |

---

## 5. 避坑提示(继承自 push handoff + 新增)

1. **不要** `git push kizemo Fluxing:Fluxing`(走 HTTPS,触发 chunked limit)— **用** `git push kizemo-ssh <sha>:Fluxing`
2. **不要** 改 `output/install.nsi`(F3/F4 verified PASS,不能动)
3. **不要** 删 `release/fluxing-0.20.0.{0,1,3,4}-installer.exe` + `release/fluxing-0.21.0.0-installer.exe`(per `feedback_release_output_rule`)
4. **不要** 在 `D:\Program Files\Fluxing\weasel\` 跑 `uninstall.exe`(破坏装机 state)
5. **不要** commit dirty librime 引用(`1c233581-dirty` → 必须先 `cd librime + git commit + cd ..` 后 `git add librime` 引用新 SHA)
6. **不要** `git add -A` 或 `git add .`(会误 add 大量 untracked noise)— 只 add 实际改的 tracked 文件
7. **不要**改 librime 1.13.1 → 1.17.0 不做 TestPipeChannelRace 12 PASS 测试(spec 076 ship-gate)
8. **不要** send_text 字段命名跟现有 `send` / `send_sequence` 冲突(yaml 必须能 dry-run 验证 schema)
9. **不要** echo `sftp.json` password 到 log
10. **bash stdout 不可靠**(per L108)— 用 `git ls-remote` + md5 命令 ground truth

---

## 6. 必读顺序(新会话第一件事)

1. **本文档** (`handoff-v0.21.0.1-keybinder-sendtext-2026-08-10.md`) — 本次任务的 scope + 决策点
2. **handoff-github-push-2026-08-10.md** §0 + §8 + §5 + §6 — push 教训 + L108 教训
3. **handoff-v0.21.0.0-release-2026-08-10.md** §3 + §6.4 — v0.21.0.0 ship + SFTP 全流程
4. **lessons-learned L108** — GitHub 2.00 GiB pack limit + chunked push
5. **lessons-learned PhaseM v0.21.0.0**(line 9999-10085)— 5 incident 复盘(竞态 fix + F3/F4 install reliability)
6. **librime/src/rime/gear/key_binder.cc:186-225** — 当前 send 分支结构(1.13.1)
7. **librime/src/rime/gear/key_binder.h** — KeyBinder class signature
8. **output/data/user-custom/*.custom.yaml** — 中文 send 实际 schema 用法

---

## 7. prompt-next 文件

详见同目录 `prompt-v0.21.0.1-keybinder-next.md`(≤30 行)。新会话第一句话直接引用。

---

**作者**:Claude(本 session) · **日期**:2026-08-10 · **session 状态**:(d) 完成,(a) 等新会话 brainstorming + writing-plans + 实现