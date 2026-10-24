# Handoff — Fluxing v0.21.0.0 milestone release (2026-08-10)

> **状态**: v0.21.0.0 已 ship + commit + tag (annotated, local only).
> 装机验证全 PASS:input works, no crash, race fix deployed, no new dumps.
> 旧 bug 收敛到 spec 076 真因 + 5 个 incident L##。

---

## 0. 大白话总结

| 项 | 值 |
|---|---|
| **installer** | `release/fluxing-0.21.0.0-installer.exe` (42.8 MB, md5 `a43c72e7a8bf66b270ad34bef8846030`) |
| **5 binary md5** | 与 v0.20.0.4 完全一致 — 仅 NSIS version metadata 变 |
| **git commits** | `29f0c61` (source+test+7 cleanup) + `54909ae` (ship+50 cleanup) + `feac4f5` (docs) |
| **git tag** | `v0.21.0.0` (annotated, points to 54909ae) |
| **装机验证** | weaselx64.dll md5 `a289525b...` ✓ (race fix 真部署) + HKCU\CTF binding EXISTS ✓ + 0 new dumps ✓ |
| **Known issue defer** | v0.21.0.1:start_service.bat mislabel + key_binder send Chinese chars parse error |

---

## 1. 已经做的

### 1.1 Phase M (v0.21.0.0 milestone) 5 incident 收敛

| L## | Version | Root cause | Fix in v0.21.0.0 |
|---|---|---|---|
| ThreadSpecificPtr-Race | v0.20.0.2 | `boost::thread_specific_ptr` + TOCTOU → 0xc0000374 | (已 commit ce9383b) include/PipeChannel.h 改 `static thread_local std::unique_ptr` |
| Weaselx64Dll-SilentMiss | v0.20.0.3 | install.nsi Rename-then-File fallback 静默吞 TSF lock 失败 | F3 (output/install.nsi line 500-535): Rename AND File 都失败 → 写 `.stage2` marker + Stage 2 At-startup swap |
| HKCU-CtfBinding-SilentMiss | v0.20.0.3 | WriteRegStr HKCU\..\CTF\Assemblies 失败无 retry/verify | F4 (output/install.nsi line 777-794): retry x2 + ReadRegStr verify + loud log |
| TestPipeChannelRace-BuildBroken | v0.20.0.3 | cpp 缺 `using namespace weasel` + 中文注释搅乱 cp936 line tracking + vcxproj 缺 PipeChannel.cpp ClCompile + boost serialization lib | test/TestPipeChannelRace/TestPipeChannelRace.{cpp,vcxproj}: 加 namespace + 删中文 + 加 ClCompile + boost lib path + MultiThreaded static |
| StartServiceBat-Mislabel | v0.20.0.0 source | `start_service.bat` 内容 = `weaselserver.exe /q` (STOP) | **defer to v0.21.0.1** |

### 1.2 TestPipeChannelRace (ship-gate per spec 076)

`test/TestPipeChannelRace/Release/TestPipeChannelRace.exe` 12 PASS / 0 FAIL:
- 2 threads × 1000 round trips: per-thread stable + distinct ctx + distinct pipe handle ✓
- 8 threads × 1000 round trips: ✓
- 32 threads × 1000 round trips: ✓
- 100 threads × 1000 round trips: ✓

### 1.3 User verification (装机后 2026-08-10)

| Check | Result |
|---|---|
| weaselx64.dll md5 | `a289525bb18511568785e9ab2e7ffec0` (race fix deployed) ✓ |
| weasel.dll md5 | `863bfdbc3c88e6a9046c72d3026f5ff5` ✓ |
| WeaselServer.exe md5 | `a622cbd076405c23371aa3bcdbf70f49` ✓ |
| Uninstall DisplayVersion | `0.20.0.4` (user 装的是 F3+F4 ship) |
| HKCU\..\CTF\Assemblies\0x00000804\{3D02CAB6-...} | EXISTS with `{A3F4CDED-...}` ✓ (F4 真生效) |
| HKLM\..\Run WeaselServer | `D:\Program Files\fluxing\weasel\WeaselServer.exe` ✓ |
| C:\fluxing-dumps since install | 0 new (last Aug 7 12:13) |
| %LOCALAPPDATA%\fluxing\crash | empty |
| EventLog Errors since install | 0 new (last Aug 7) |
| WeaselServer PID 7160 | Responding, 7.7 min uptime |
| librime init | 24382 bytes / 265 lines, complete |
| User report | "可以输入中文,未出现 crash" ✓ |

### 1.4 v0.21.0.0 ship

- env.bat: FLUXING_VERSION 0.21.0 + WEASEL_BUILD 0
- _nsis_v065.cmd rebuild: installer md5 `a43c72e7a8bf66b270ad34bef8846030`
- 7z extract 5 binary md5 (与 v0.20.0.4 一致, 仅 NSIS metadata 变)
- Copy `output/archives/fluxing-0.21.0.0-installer.exe` → `release/fluxing-0.21.0.0-installer.exe`
- git add + commit + tag (annotated)

### 1.5 dirty state 清理

- 49 `release/fluxing-0.19.0.*-installer.exe` 0-byte placeholders → git rm + commit (L##-Release-Output-CleanupRule)
- release/ 保留: v0.20.0.0, v0.20.0.1, v0.20.0.3, v0.20.0.4, v0.21.0.0 (release history)
- env.bat (gitignored) 不 commit
- weasel.props / *.obj / *.pdb (per CLAUDE.md) 都没误提交

---

## 2. 文件位置

| 文件 | 路径 | 状态 |
|---|---|---|
| v0.21.0.0 installer | `release/fluxing-0.21.0.0-installer.exe` | ✅ shipped, md5 verified |
| F3 + F4 installer fix | `output/install.nsi` line 500-535 + 777-794 + 800-870 | ✅ committed (29f0c61) |
| TestPipeChannelRace | `test/TestPipeChannelRace/{TestPipeChannelRace.cpp,TestPipeChannelRace.vcxproj}` | ✅ committed, 12/12 PASS |
| setlocale fix | `include/StringAlgorithm.hpp` | ✅ committed (29f0c61, was uncommitted from v0.20.0.x) |
| Lessons-learned L##-PhaseM | `.specify/memory/lessons-learned.md` line 9999-10085 | ✅ committed (feac4f5) |
| Handoff (本文) | `handoff-v0.21.0.0-release-2026-08-10.md` | 新建 (untracked) |
| Build test cmd | `_build_test.cmd` (test rebuild helper) | untracked, 留作 utility |
| Env.bak | `env.bat` (v0.21.0.0) | gitignored |
| User machine | `D:\Program Files\Fluxing\weasel\` | ✅ verified |

---

## 3. 下一步 (defer to v0.21.0.1)

| Item | Description | Estimated |
|---|---|---|
| start_service.bat mislabel | `output/start_service.bat` 内容 = `weaselserver.exe /q` (STOP); `output/stop_service.bat` 内容 = `start "" WeaselServer.exe` (START). 改名 / 改内容 | trivial (1 line change each) |
| key_binder send Chinese chars | `output/data/user-custom/*.custom.yaml` 里 `send: "《"` 等中文, librime 报 `parse error: unrecognized key '《'`, Shift+标点 binding 失效。Fix: 用 `send_text` 字段 (新 librime) 或 改写为 punctuator half_shape patch (但 Shift+ 不响应) | medium (research librime API) |
| 装机二次压测 | v0.21.0.0 装机后并发 Claude + Explorer + dopus + 微信 + WeaselServer 重启 5+ min, 看 C:\fluxing-dumps 是否新增 | manual |
| Push to kizemo/Fluxing | local 944 ahead / 831 behind, divergence 大,需要 handoff session 处理 push + rebase | future session |

---

## 4. branch divergence 状态

```
## Fluxing...kizemo/Fluxing [ahead 944, behind 831]
```

本会话没 push (per CLAUDE.md "不在主机上直接跑 installer / push 待接力 session")。
下次接力 session 第一件事: 处理 divergence + push tag v0.21.0.0 + commits 29f0c61 / 54909ae / feac4f5。

---

## 5. 给下次会话的 prompt

```
Fluxing v0.21.0.0 已 ship (local commits 29f0c61 + 54909ae + feac4f5, tag v0.21.0.0 annotated on 54909ae)。
Branch Fluxing vs kizemo/Fluxing: 944 ahead / 831 behind — divergence 极大, 不能直接 push。
下一步 (3 选 1):
  (a) 处理 divergence + push v0.21.0.0 tag (需要 git rebase / merge / force-push)
  (b) v0.21.0.1 ship: start_service.bat mislabel fix + key_binder send Chinese chars fix + 并发压测验证
  (c) 接用户新需求 (如有)
详见 F:\soft\00selfmade\rime_claude\handoff-v0.21.0.0-release-2026-08-10.md
```

---

**作者**: Claude (本会话) · **日期**: 2026-08-10
**session 状态**: v0.21.0.0 milestone ship 完成, 装机验证 PASS, 待用户选 (a)/(b)/(c)

---

## 6. v0.21.0.0 SFTP ship — continuation (2026-08-10 上午)

**用户 explicit ask**: "请把新版本exe文件，上传到www.aiec.fun对应的目录中，同时修改版本相关文件"

### 6.1 版本相关文件修改 (4 file)

| File | Path | 修改 |
|---|---|---|
| `_check_install_v2.ps1` | `F:\soft\00selfmade\rime_claude\_check_install_v2.ps1` | line 70-73: `$expectedMd5` / `$expectedFluxingMd5` / `$expectedInstallerMd5` / `$expectedBuildTime` 全部更新到 v0.21.0.0 + Phase M 注释。**committed `792cb47` docs(check-install): v0.21.0.0 expected md5 — Phase M milestone** |
| `version.json` | `E:\办公文件\L1网站\pinyin\version.json` | version="0.21.0.0", filename="fluxing-0.21.0.0-installer.exe", updated="2026-08-10", notes=Phase M 总结 (含 5 incident + 5 binary md5 + known issue defer) |
| `appcast.xml` | `E:\办公文件\L1网站\pinyin\appcast.xml` | prepend v0.21.0.0 item (title / sparkle:releaseNotesLink / pubDate RFC 2822 / enclosure url+version+length+type)。现 54 items, v0.21.0.0 在 top |
| `release-notes.html` | `E:\办公文件\L1网站\pinyin\release-notes.html` | prepend `<h2>0.21.0.0</h2>` section (5 incident 列表 + 5 binary md5 装机 verified + User verification + Commit/Tag + Defer to v0.21.0.1) |

`common.php` 无 hardcoded version (读 `version.json` `VERSION_FILE`), 不用手动改。

### 6.2 SFTP atomic upload (sftp_sync.py --version=0.21.0.0)

```
config: sftp_config.json
local_base: E:\办公文件\L1网站\pinyin
remote_subdir: pinyin
host=47.120.26.175 port=22 user=kizemo remotePath=/var/www/wordpress
known_hosts: C:\Users\Duanyi/.ssh/known_hosts (loaded)

Local md5 pre-check:
  OK fluxing-0.21.0.0-installer.exe: A43C72E7A8BF66B270AD34BEF8846030 (cross-source)
  OK version.json: 3A41905EAD18104F02E757BBAD7554B0 (self-consistent)
  OK appcast.xml: FD8CEAC09CE9D98AC223A7C527BA21F7 (self-consistent)
  OK release-notes.html: E3BC40DD7A0713597C233A877279E739 (self-consistent)

Atomic upload (run-id=0.21.0.0-1786328321):
  snapshot version.json / appcast.xml / release-notes.html → .pre-ship-{runid}
  stage 4 files → .stage-{runid}
  server-side md5sum verify: 4/4 OK
  rename stage → final: 4/4 OK
  final md5sum verify: 4/4 OK
  cleanup pre-ship: auto-ran

[DONE] v0.21.0.0 SFTP 上传 + cross-source verify OK
```

**Public download**: https://www.aiec.fun/pinyin/download.php?file=fluxing-0.21.0.0-installer.exe
**Appcast feed**: https://www.aiec.fun/pinyin/appcast.xml
**Release notes**: https://www.aiec.fun/pinyin/release-notes.html

### 6.3 Git commits (v0.21.0.0 milestone + SFTP ship continuation)

```
792cb47 docs(check-install): v0.21.0.0 expected md5 — Phase M milestone
feac4f5 docs(memory): L##-PhaseM v0.21.0.0 milestone — spec 076 race fix deploy + installer reliability
54909ae chore(release): v0.21.0.0 installer ship + drop 49 stale v0.19.0.x
29f0c61 fix(WeaselIPC+installer): v0.21.0.0 spec 076 reliability milestone
```

Local only — branch Fluxing vs kizemo/Fluxing 944 ahead / 831 behind divergence 没 push。

### 6.4 后续任务 (优先序)

| 优先级 | Task | Est. | 备注 |
|---|---|---|---|
| P0 | 处理 branch divergence + push v0.21.0.0 tag + 4 commits (792cb47/feac4f5/54909ae/29f0c61) | 30 min | git rebase / merge / force-push。Push 是 public 行为, **必须 user 二次确认** |
| P1 | v0.21.0.1 ship: `output/start_service.bat` 内容反 fix + `output/data/user-custom/*.custom.yaml` key_binder `send: "《"` 等中文 parse error fix | 4 h | librime `Send` field 期望 key sequences not literal text。Fix 思路: 改用 `send_text` (新 librime) 或 改写为 `punctuator/half_shape` patch (Shift+ 不响应时备用) |
| P1 | 并发压测: 装 v0.21.0.0 后同时开 Claude Code + Explorer + dopus + 微信 + WeaselServer 重启, 来回切火流猩 5+ min, 看 `C:\fluxing-dumps\` 是否新增 0xc0000374 dump | 30 min manual | spec 076 race fix 真不触发才算 PASS |
| P2 | sandbox-verify 跑 `rime-verify.ps1 -InstallerPath` | 15 min | 推荐在沙箱里跑一遍再让 user 装机 |

### 6.5 环境 / 分支 / 基线状态

| 项 | 值 |
|---|---|
| 工作目录 | `F:\soft\00selfmade\rime_claude` |
| Git branch | `Fluxing` |
| Git status | clean (m librime submodule -dirty 是历史 noise) |
| HEAD | `792cb47` |
| Tag | `v0.21.0.0` (annotated, points to `54909ae`) |
| kizemo/Fluxing remote | `[ahead 944, behind 831]` — divergence 极大 |
| env.bat | `FLUXING_VERSION=0.21.0` `WEASEL_BUILD=0` `PRODUCT_VERSION=0.21.0.0` `FILE_VERSION=0.21.0.0` |
| Boost | `F:\b183` (msvc-14.3, x32 static) |
| 装机 base | `D:\Program Files\Fluxing\weasel\` (verified v0.20.0.4 → v0.21.0.0 same binaries) |
| Server | `47.120.26.175` (kizemo@, `/var/www/wordpress/pinyin/`) |
| 已 ship versions on server | v0.21.0.0 (current) |

### 6.6 避坑提示 (per next session)

1. **不要** 重新跑 `_nsis_v065.cmd` rebuild v0.21.0.0 installer (source 不变, 只会生成相同 md5 + 浪费时间)
2. **不要** 改 `output/install.nsi` (spec 076 race fix + F3/F4 reliability 都验证 PASS, 不能动)
3. **不要** 删 `release/fluxing-0.20.0.{0,1,3,4}-installer.exe` (release history, per `feedback_release_output_rule.md`)
4. **不要** 改 `librime` submodule (untracked deps noise, 等下次正式 upgrade 再处理)
5. **不要** echo `sftp.json` password 到 log (脚本本身已隐藏, 但 transform/analyze 时注意)
6. **不要** 在 v0.21.0.0 装机路径 `D:\Program Files\Fluxing\weasel\` 跑 `uninstall.exe` (会破坏装机 state, 装机 verification 已 PASS)
7. **不要** 直接 `git push` (divergence 944/831, 需先 rebase 或 force-with-lease — 必须 user 二次确认)

### 6.7 必读顺序 (新会话第一件事)

1. **本文档** (`handoff-v0.21.0.0-release-2026-08-10.md`) — 上下文 + 必交付 + 避坑
2. `.specify/memory/lessons-learned.md` §L##-PhaseM-v0.21.0.0 (line 9999-10085) — 5 incident 完整复盘
3. `output/install.nsi` line 500-535 (F3) + 777-794 (F4) — 装机可靠性 fix 的具体代码
4. `test/TestPipeChannelRace/TestPipeChannelRace.cpp` + `.vcxproj` — regression test wiring
5. `E:\办公文件\L1网站\pinyin\appcast.xml` + `version.json` + `release-notes.html` — 当前 server 端 4 file 状态

### 6.8 接力 prompt

详见 `prompt-v0.21.0.0-next.md` (项目根, ≤30 行)。新会话第一句话直接引用即可, **不要重读整个对话历史** — 本文 + lessons-learned + 上列 5 个文件 是真相源。