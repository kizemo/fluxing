# rewrite provenance report

- target: `.specify/memory/lessons-learned.md`
- map: `rewrite-final.json` (180 entries)
- baseline `0c0716c36a`: clean parent before FFFD corruption (1872 lines)
- baseline `879cb2e914`: last fully pristine revision (861 lines)

| class | count | meaning |
|---|---|---|
| EXACT | 71 | verbatim line found in a clean baseline |
| PARTIAL | 4 | proposed text is a substring of a clean baseline line |
| SELF | 36 | de-coded from the damaged line itself |
| WITNESS | 0 | quoted verbatim in a parallel repo document |
| EMPTY | 50 | deliberately blanked fragment |
| NEW | 19 | no source at all -- needs a manual context decision |

## detail

### line 862 [SELF]

- OLD: `**Context**: spec 005 design.md wrote <07>ccept: shift+l (lowercase`
- NEW: `**Context**: spec 005 design.md wrote accept: shift+l (lowercase`
- provenance: derived from the damaged line itself (control-char / escape de-coding)

### line 884 [SELF]

- OLD: `- In any key_binder / key_sequence / <07>ccept: field of`
- NEW: `- In any key_binder / key_sequence / accept: field of`
- provenance: derived from the damaged line itself (control-char / escape de-coding)

### line 889 [SELF]

- OLD: `  different way: <07> means "the key that produces lowercase a"`
- NEW: `  different way: a means "the key that produces lowercase a"`
- provenance: derived from the damaged line itself (control-char / escape de-coding)

### line 898 [SELF]

- OLD: `  <07>ccept: Shift+l, not whether librime actually accepted it.`
- NEW: `  accept: Shift+l, not whether librime actually accepted it.`
- provenance: derived from the damaged line itself (control-char / escape de-coding)

### line 905 [SELF]

- OLD: `  two in <07>lways), replacing the four <07>ccept: Shift_L`
- NEW: `  two in always), replacing the four accept: Shift_L`
- provenance: derived from the damaged line itself (control-char / escape de-coding)

### line 1053 [EXACT]

- OLD: `# [F] L19 [F][F][F]<0E> (2026-07-01): L18 [F]<07>p[F] `always: Shift+Shift_L/R` ascii_mode toggle<0C>F[F]Y[F] `has_menu: Shift+Shift_L/R send 2/3` binding<02>0.18.5.0 (7[F]K: `shift+Enter` / `shift+<letter>` release event [F][F][F][F] ascii_mode <07>b<02>L18 [F]`
- NEW: `# 被 L19 替代说明 (2026-07-01): L18 仅切走了 `always: Shift+Shift_L/R` ascii_mode toggle，但保留了 `has_menu: Shift+Shift_L/R send 2/3` binding。0.18.5.0 用户实测: `shift+Enter` / `shift+<letter>` release event 仍可触发 ascii_mode 切换。L18 修复未真实安装验证，是因为字符串断言（TestDefaultHotkeys 25/25 PASS）不能证明运行时 binding 表行为。L19 防御性原则：`default.yaml` 中 `keycode=Shift_L/R` 的所有 binding 全部不存在；候选选择改用 RIME 社区默认 `Control+1/2/3..9`。`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1054 [EMPTY]

- OLD: `*<1F>[F][F]Ō[F]<0C>/[F]:W&2[F][N]<08>TestDefaultHotkeys 25/25 PASS	`
- NEW: `(blank)`
- provenance: deliberate blank (fragment absorbed by a recovered neighbour)

### line 1055 [EMPTY]

- OLD: `[F][F]<0E>[F]L[F] binding hL:<02>L19 2[F]'[F]<19><1A>`default.yaml` - `keycode=Shift_L/R` [F]@	 binding h[F]`
- NEW: `(blank)`
- provenance: deliberate blank (fragment absorbed by a recovered neighbour)

### line 1056 [EMPTY]

- OLD: `X(<1B><19>		[F]9( RIME >:ؤ `Control+1/2/3..9`<02>`
- NEW: `(blank)`
- provenance: deliberate blank (fragment absorbed by a recovered neighbour)

### line 1064 [EXACT]

- OLD: `other key that has a `Shift+symbol` layout <14> the `+` keystroke never reaches the`
- NEW: `other key that has a `Shift+symbol` layout — the `+` keystroke never reaches the`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1070 [EXACT]

- OLD: `<14> but the actual match also fires on the **release** portion of compound keystrokes`
- NEW: `— but the actual match also fires on the **release** portion of compound keystrokes`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1087 [EXACT]

- OLD: `- Kept the 2 `has_menu: send N, accept: Shift+Shift_L/R` bindings (select 2nd/3rd  candidate) <14> these are useful and the user confirmed they want them to stay.`
- NEW: `- Kept the 2 `has_menu: send N, accept: Shift+Shift_L/R` bindings (select 2nd/3rd  candidate) — these are useful and the user confirmed they want them to stay.`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1095 [EXACT]

- OLD: `  AND `modifier==Shift` <14> but the **release event** of any `shift+<other>` keystroke`
- NEW: `  AND `modifier==Shift` — but the **release event** of any `shift+<other>` keystroke`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1098 [EXACT]

- OLD: `  event** <14> i.e. a key that is only ever pressed together with a real character. `space`  is ideal because `Shift+space` is a deliberate user action, not a release artifact.`
- NEW: `  event** — i.e. a key that is only ever pressed together with a real character. `space`  is ideal because `Shift+space` is a deliberate user action, not a release artifact.`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1115 [EXACT]

- OLD: `  (was 24/24 before L18; +1 for `Shift+space` assertion, +4 negative assertions for  the removed `Shift+Shift_L/R` ascii_mode bindings, -3 for the removed  `Shift_L/R U. always <07>-[F]` duplicates; net +1).`
- NEW: `  (was 24/24 before L18; +1 for `Shift+space` assertion, +4 negative assertions for  the removed `Shift+Shift_L/R` ascii_mode bindings, -3 for the removed  `Shift_L/R 单键 always 切中英` duplicates; net +1).`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1124 [EXACT]

- OLD: `**q[F]H,**: 0.18.5.0 (librime 1.13)`
- NEW: `**影响版本**: 0.18.5.0 (librime 1.13)`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1125 [EMPTY]

- OLD: `**[F]`
- NEW: `(blank)`
- provenance: deliberate blank (fragment absorbed by a recovered neighbour)

### line 1126 [EXACT]

- OLD: `H,**: 0.18.6.0 ([F][F])`
- NEW: `**修复版本**: 0.18.6.0 (待发)`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1133 [EMPTY]

- OLD: `MW[F]&[F][F]<0C>e<07>b[F]-[F]-[F]<07>[F]<02>`
- NEW: `(blank)`
- provenance: deliberate blank (fragment absorbed by a recovered neighbour)

### line 1136 [EMPTY]

- OLD: `L18 [F]`
- NEW: `(blank)`
- provenance: deliberate blank (fragment absorbed by a recovered neighbour)

### line 1139 [EXACT]

- OLD: `### 9[F]`
- NEW: `### 根因`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1142 [EMPTY]

- OLD: `<08>.[F] release event [F]9M<02>`
- NEW: `(blank)`
- provenance: deliberate blank (fragment absorbed by a recovered neighbour)

### line 1143 [EMPTY]

- OLD: `2. L18 [F]`
- NEW: `(blank)`
- provenance: deliberate blank (fragment absorbed by a recovered neighbour)

### line 1145 [EMPTY]

- OLD: `***<1F>[F][F]Ō[F]**<02>`
- NEW: `(blank)`
- provenance: deliberate blank (fragment absorbed by a recovered neighbour)

### line 1146 [EXACT]

- OLD: `3. W&2[F][N][F][F][F]<0E> yaml [F],[F][F]a binding X(/`
- NEW: `3. 字符串断言只能证明 yaml 文本里某条 binding 存在/不存在，不能证明运行时 binding 表的行为。`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1147 [EMPTY]

- OLD: `X(<0C>`
- NEW: `(blank)`
- provenance: deliberate blank (fragment absorbed by a recovered neighbour)

### line 1148 [EMPTY]

- OLD: `[F][F]<0E>[F]L[F] binding h[F]L:<02>`
- NEW: `(blank)`
- provenance: deliberate blank (fragment absorbed by a recovered neighbour)

### line 1150 [EXACT]

- OLD: `### 2[F]'[F]<19>`
- NEW: `### 防御性原则`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1152 [EXACT]

- OLD: `- `default.yaml` - `keycode=Shift_L/R` [F]@	 binding **h[F]`
- NEW: `- `default.yaml` 中 `keycode=Shift_L/R` 的所有 binding **全部不存在**（包括 `always` 与 `has_menu` 两条路径）。`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1153 [EMPTY]

- OLD: `X(**<08><05>[F] `always` <0E> `has_menu` $a[F]	<02>`
- NEW: `(blank)`
- provenance: deliberate blank (fragment absorbed by a recovered neighbour)

### line 1155 [EMPTY]

- OLD: `[F][F]<02>`
- NEW: `(blank)`
- provenance: deliberate blank (fragment absorbed by a recovered neighbour)

### line 1160 [EXACT]

- OLD: `### [F]`
- NEW: `### 修复`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1163 [EXACT]

- OLD: `1. `output/data/default.yaml`: [F]d `has_menu: Shift+Shift_L, send: 2` <0E> `has_menu: Shift+Shift_R, send: 3`<0C>[F][F] `has_menu: Control+1, send: 2` <0E> `has_menu: Control+2, send: 3`<02>`
- NEW: `1. `output/data/default.yaml`: 移除 `has_menu: Shift+Shift_L, send: 2` 与 `has_menu: Shift+Shift_R, send: 3`，新增 `has_menu: Control+1, send: 2` 与 `has_menu: Control+2, send: 3`。`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1165 [EXACT]

- OLD: `3. z[F][F][F]9( `Control+1` ( `Shift+space` KM<02>`
- NEW: `3. 顺序检查改用 `Control+1` 在 `Shift+space` 之前。`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1167 [EXACT]

- OLD: `### [F][F]`
- NEW: `### 验证`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1170 [EMPTY]

- OLD: `	<02>`
- NEW: `(blank)`
- provenance: deliberate blank (fragment absorbed by a recovered neighbour)

### line 1172 [EXACT]

- OLD: `- ***[F][F]**<1A><1F>[F][F][F]<05>L:<08>librime submodule a[F]<0C>0.18.6 [F] build	<1B>(7[F]ŞK<02>`
- NEW: `- **未验证**：真实安装包行为（librime submodule 污染，0.18.6 待 build）；用户重装实测。`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1174 [EXACT]

- OLD: `### 2 L19 `
- NEW: `### 防 L19 复发`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1175 [EMPTY]

- OLD: `[F]`
- NEW: `(blank)`
- provenance: deliberate blank (fragment absorbed by a recovered neighbour)

### line 1178 [EXACT]

- OLD: `- L19 ( `TestDefaultHotkeys` [F] 6 *<1F>[F][N]<0C>[F][F]@	 `Shift_L/R` b<01>[F] binding<02>`
- NEW: `- L19 在 `TestDefaultHotkeys` 加 6 个负断言，覆盖所有 `Shift_L/R` 形态的 binding。`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1179 [EXACT]

- OLD: `- <0E>[F] 0.18.6 release [F]{( AGENTS.md [F]2.5 silent-install smoke test K<16><0C>**[F]<16>**[F] `shift+Enter` / `shift+<letter>` / `Control+1` / `Control+2` K[F][F]6<02>`
- NEW: `- 后续 0.18.6 release 必须在 AGENTS.md §2.5 silent-install smoke test 之外，**额外**跑 `shift+Enter` / `shift+<letter>` / `Control+1` / `Control+2` 手动验收。`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1181 [EXACT]

- OLD: `### [F]s`
- NEW: `### 相关`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1184 [EXACT]

- OLD: `- L16<1A>spec 012 `
- NEW: `- L16：spec 012 上 `Shift+l/r` 组合键 ascii_mode toggle 已移除。`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1185 [EMPTY]

- OLD: ` `Shift+l/r` [F]<08>. ascii_mode toggle [F][F]d<02>`
- NEW: `(blank)`
- provenance: deliberate blank (fragment absorbed by a recovered neighbour)

### line 1190 [EXACT]

- OLD: `**q[F]H,**: 0.18.5.0 / 0.18.6.0 release build`
- NEW: `**影响版本**: 0.18.5.0 / 0.18.6.0 release build`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1191 [EMPTY]

- OLD: `**[F]`
- NEW: `(blank)`
- provenance: deliberate blank (fragment absorbed by a recovered neighbour)

### line 1192 [EXACT]

- OLD: `H,**: 0.18.6.0 build script [F]9( cmd /c wrapper`
- NEW: `**修复版本**: 0.18.6.0 build script 已改用 cmd /c wrapper`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1196 [EXACT]

- OLD: ``Start-Process -FilePath installer.exe -ArgumentList '/S /D=...' -Wait -PassThru` ( PowerShell 5.1 `
- NEW: `跑 AGENTS.md §2.5 silent-install smoke test 时，`Start-Process -FilePath installer.exe -ArgumentList '/S /D=...' -Wait -PassThru` 在 PowerShell 5.1 下 5 分钟仍不返回。改用 `Start-Process cmd.exe -ArgumentList '/c', 'start /B ...' -PassThru` 同样 60s 仍卡住。直接 `cmd /c` 同步执行 40.3s 完成，exit 0。`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1197 [EMPTY]

- OLD: `[F][F]<02>9( `Start-Process cmd.exe -ArgumentList '/c', 'start /B ...' -PassThru` <0C>7 60s [F]aO<02>[F][F] `cmd /c` <0C>egL 40.3s [F]<10><0C>exit 0<02>`
- NEW: `(blank)`
- provenance: deliberate blank (fragment absorbed by a recovered neighbour)

### line 1199 [EXACT]

- OLD: `### 9[F]`
- NEW: `### 根因`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1202 [EMPTY]

- OLD: `[F][F]<02>`
- NEW: `(blank)`
- provenance: deliberate blank (fragment absorbed by a recovered neighbour)

### line 1206 [EXACT]

- OLD: `### D&[F] 0.18.6.0 build )Q<08>[F][N]v[F]U	`
- NEW: `### 附带的 0.18.6.0 build 踩坑（应一并记录）`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1208 [EXACT]

- OLD: `1. **librime build.bat CMAKE_GENERATOR z<<06>[F] bug**<1A>env.bat - `set "CMAKE_GENERATOR=Visual Studio 17 2022"`<0C>F librime/build.bat , 78 L `set common_cmake_flags=%common_cmake_flags% -G%CMAKE_GENERATOR%` [F][F]<15>[F]<0C>cmd [F] "Visual Studio 17 2022" [F]<10> 4 * token [F]e common_cmake_flags<0C>cmake 60 -GVisual + Studio + 17 + 2022 [F]<19><02>**workaround**<1A>[F][F]<03> cmake <0C>`
- NEW: `1. **librime build.bat CMAKE_GENERATOR 空格分词 bug**：env.bat 中 `set "CMAKE_GENERATOR=Visual Studio 17 2022"`，但 librime/build.bat 第 78 行 `set common_cmake_flags=%common_cmake_flags% -G%CMAKE_GENERATOR%` 没加引号，cmd 把 "Visual Studio 17 2022" 拆成 4 个 token 写入 common_cmake_flags，cmake 收到 -GVisual + Studio + 17 + 2022 报错。**workaround**：直接调 cmake 而不通过 build.bat；用 -G "Visual Studio 17 2022" 显式引号。`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1209 [EMPTY]

- OLD: `<1A>[F] build.bat<1B>( -G "Visual Studio 17 2022" ><0F><15>[F]<02>`
- NEW: `(blank)`
- provenance: deliberate blank (fragment absorbed by a recovered neighbour)

### line 1211 [EMPTY]

- OLD: `2. **WinSparkle.lib stub**<1A>lib64/WinSparkle.lib (10460 bytes) /*`
- NEW: `(blank)`
- provenance: deliberate blank (fragment absorbed by a recovered neighbour)

### line 1217 [EMPTY]

- OLD: `4. **xmake x86 build ؤ build_dir = build\windows\x86\release**<0C>`
- NEW: `(blank)`
- provenance: deliberate blank (fragment absorbed by a recovered neighbour)

### line 1218 [EXACT]

- OLD: `/ xbuild.bat [F][F]̙[F] output\Win32<02>@	 weasel.dll / WeaselSetup.exe [F][F]E[F]i( build\windows\x86\release\WeaselTSF\ / build\windows\x86\release\WeaselSetup\<0C>xmake.lua [F] after_build hook [F]Z[F]* cp [F]\<0C>FSM hook [F] cp weasel*.dll <1A>M&0 output\<0C>W<0F>[F] WeaselSetup vB[F]<07><02>`
- NEW: `4. **xmake x86 build 默认 build_dir = build\windows\x86\release**，不是 xbuild.bat 注释里写的 output\Win32。所有 weasel.dll / WeaselSetup.exe 的实际产物在 build\windows\x86\release\WeaselTSF\ / build\windows\x86\release\WeaselSetup\，xmake.lua 的 after_build hook 应做这个 cp 动作，但当前 hook 只 cp weasel*.dll 通配符到 output\，遗漏了 WeaselSetup 顶层目标。`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1220 [EXACT]

- OLD: `### [F][F]`
- NEW: `### 验证`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1223 [PARTIAL]

- OLD: `- L19 [F]`
- NEW: `- L19 修复验证：装出的`
- provenance: substring of a baseline line in 0c0716c36a (substring of a baseline line)

### line 1224 [PARTIAL]

- OLD: `[F][F]<1A>[F][F][F] default.yaml + Control+1, send: 2 + Control+2, send: 3<0C>`
- NEW: `default.yaml 含 Control+1, send: 2 + Control+2, send: 3，不含`
- provenance: substring of a baseline line in 0c0716c36a (substring of a baseline line)

### line 1225 [PARTIAL]

- OLD: `+ Shift+Shift_L, send: 2<0C>+ Shift+space toggle ascii_mode<02>`
- NEW: `Shift+Shift_L, send: 2，含 Shift+space toggle ascii_mode。`
- provenance: substring of a baseline line in 0c0716c36a (substring of a baseline line)

### line 1228 [EXACT]

- OLD: `### 2 L20 `
- NEW: `### 防 L20 复发`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1229 [EMPTY]

- OLD: `[F]`
- NEW: `(blank)`
- provenance: deliberate blank (fragment absorbed by a recovered neighbour)

### line 1232 [EXACT]

- OLD: `- [F] librime/build.bat , 78 L<15>[F] bug [F]\: spec Ф[F] librime `
- NEW: `- 修 librime/build.bat 第 78 行引号 bug 应作为 spec 提交给 librime 上游。`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1233 [EMPTY]

- OLD: `8<02>`
- NEW: `(blank)`
- provenance: deliberate blank (fragment absorbed by a recovered neighbour)

### line 1234 [EXACT]

- OLD: `- [F] xmake.lua [F] after_build hook [F] weasel.dll / WeaselSetup.exe cn cp 0 output\Win32\ / weasel [F][F] PR<02>`
- NEW: `- 修 xmake.lua 的 after_build hook 让 weasel.dll / WeaselSetup.exe 正确 cp 到 output\Win32\ 是 weasel 自己的 PR。`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1236 [EXACT]

- OLD: `### [F]s`
- NEW: `### 相关`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1241 [EMPTY]

- OLD: `<08>[F] L20 [F][F][F][F]cn default.yaml	<02>`
- NEW: `(blank)`
- provenance: deliberate blank (fragment absorbed by a recovered neighbour)

### line 1245 [EMPTY]

- OLD: `## L21 - L19 / over-correction: key_binder [F]d keycode=Shift_L/R [F]`
- NEW: `(blank)`
- provenance: deliberate blank (fragment absorbed by a recovered neighbour)

### line 1246 [NEW]

- OLD: `<06> modifier=Shift (binding b<0F>) <0E> modifier=0 (release-event [F]9M) - cn[F]b<0F>/ <07>ccept: Shift+Shift_L/R`
- NEW: `## L21 - L19 是 over-correction: key_binder 移除 keycode=Shift_L/R 时不分 modifier=Shift (binding 形式) 与 modifier=0 (release-event 误匹配) - 正确的形式是 accept: Shift+Shift_L/R`
- provenance: NO SOURCE -- needs a manual context decision

### line 1249 [EXACT]

- OLD: `**q[F]H,**: 0.18.6.0 / 0.18.7.0`
- NEW: `**影响版本**: 0.18.6.0 / 0.18.7.0`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1250 [EXACT]

- OLD: `**[F]`
- NEW: `**修复版本**: 0.18.8.0 (spec 014)`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1251 [EMPTY]

- OLD: `H,**: 0.18.8.0 (spec 014)`
- NEW: `(blank)`
- provenance: deliberate blank (fragment absorbed by a recovered neighbour)

### line 1257 [EXACT]

- OLD: `### 9[F]`
- NEW: `### 根因`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1262 [NEW]

- OLD: `1. <07>ccept: Shift_L (modifier=0) - [F]: {keycode=Shift_L, modifier=0} - **<1A>**<0E> TSF release event [F]9M (release event _/ {keycode=Shift_L, modifier=Release} F[F] keycode <0C><0C>[F]Л librime [F][F]<04><06>)`
- NEW: `1. accept: Shift_L (modifier=0) - 解析为 {keycode=Shift_L, modifier=0} - **会**与 TSF release event 误匹配 (release event 也是 {keycode=Shift_L, modifier=Release} 但因 keycode 同而被某些 librime 路径误处理)`
- provenance: NO SOURCE -- needs a manual context decision

### line 1263 [EMPTY]

- OLD: `2. <07>ccept: Shift+Shift_L (modifier=Shift) - [F]: {keycode=Shift_L, modifier=Shift} - **`
- NEW: `(blank)`
- provenance: deliberate blank (fragment absorbed by a recovered neighbour)

### line 1264 [NEW]

- OLD: `<1A>**<0E> TSF release event [F]9M (release event / modifier=Release, <0E> modifier=Shift %<`
- NEW: `2. accept: Shift+Shift_L (modifier=Shift) - 解析为 {keycode=Shift_L, modifier=Shift} - **不会**与 TSF release event 误匹配 (release event 是 modifier=Release, 与 modifier=Shift 严格不等)`
- provenance: NO SOURCE -- needs a manual context decision

### line 1265 [EMPTY]

- OLD: `I)`
- NEW: `(blank)`
- provenance: deliberate blank (fragment absorbed by a recovered neighbour)

### line 1267 [EXACT]

- OLD: `librime 1.13.1 KeyEvent::operator== (librime/src/rime/key_event.h:64) %<ԃ keycode_ <0E> modifier_, $[F]b<0F>([F]L[F]L:*6`
- NEW: `librime 1.13.1 KeyEvent::operator== (librime/src/rime/key_event.h:64) 严格比较 keycode_ 与 modifier_, 两种形式在运行时行为截然不同。L19 一刀切全部移除, 等于把 spec 005 v1.1 承诺的 Shift 选候选能力废掉。`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1268 [EMPTY]

- OLD: `<0C><02>L19 [N][N]<07>h[F][F]d, I[F][F] spec 005 v1.1 [F][F] Shift 	<19>	[F][F][F][F]<02>`
- NEW: `(blank)`
- provenance: deliberate blank (fragment absorbed by a recovered neighbour)

### line 1270 [EXACT]

- OLD: `### [F]`
- NEW: `### 修复`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1273 [EXACT]

- OLD: `1. output/data/default.yaml: ( key_binder/bindings has_menu [F], ( Control+1/2 KM[F][F] 2 L binding:`
- NEW: `1. output/data/default.yaml: 在 key_binder/bindings has_menu 段, 在 Control+1/2 之前加回 2 行 binding:`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1278 [EXACT]

- OLD: `   [F]E/:`
- NEW: `   实际是:`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1286 [SELF]

- OLD: `   - <07>ccept: Shift+Shift_L, send: 2 / <07>ccept: Shift+Shift_R, send: 3 X(`
- NEW: `   - accept: Shift+Shift_L, send: 2 / accept: Shift+Shift_R, send: 3 X(`
- provenance: derived from the damaged line itself (control-char / escape de-coding)

### line 1287 [NEW]

- OLD: `   - [F]	 bare <07>ccept: Shift_L/R, (modifier=0) - L19 2[F][F]Y`
- NEW: `   - 没有 bare accept: Shift_L/R, (modifier=0) - L19 防御保留`
- provenance: NO SOURCE -- needs a manual context decision

### line 1288 [SELF]

- OLD: `   - <07>scii_composer.switch_key.Shift_L/R: noop 保持`
- NEW: `   - ascii_composer.switch_key.Shift_L/R: noop 保持`
- provenance: derived from the damaged line itself (control-char / escape de-coding)

### line 1290 [EXACT]

- OLD: `   - 4 * ordering [F][N] (Shift+Shift_L/R ( Shift+space KM, ( Control+1/2 KM)`
- NEW: `   - 4 个 ordering 断言 (Shift+Shift_L/R 在 Shift+space 之前, 在 Control+1/2 之前)`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1291 [EXACT]

- OLD: `4. weasel.sln: [F] Project [F] + 16 L ProjectConfigurationPlatforms`
- NEW: `4. weasel.sln: 新 Project 注册 + 16 行 ProjectConfigurationPlatforms`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1298 [EXACT]

- OLD: `### [F][F]`
- NEW: `### 验证`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1300 [EXACT]

- OLD: `- TestDefaultHotkeys.exe output\data\default.yaml -> Passed: 35 / 35 (4 * L19 <1F>[F]c + 4 *[F]c)`
- NEW: `- TestDefaultHotkeys.exe output\data\default.yaml -> Passed: 35 / 35 (4 个 L19 负翻正 + 4 个新正)`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1303 [EXACT]

- OLD: `- 7z [F]<05>$* installer [F][F]: /[N][F]<02>/ data\default.yaml (16607 -> 17200 bytes, +593 bytes /[F][F]e[F] 4 L[F][F] + 2 L binding)<02>@	 binary 100% [F]<0C> (rime.dll, WeaselServer.exe I 23 *[F][F] SHA256 hI)`
- NEW: `- 7z 解包两个 installer 对比: 唯一差异是 data\default.yaml (16607 -> 17200 bytes, +593 bytes 是新插入的 4 行注释 + 2 行 binding)。所有 binary 100% 相同 (rime.dll, WeaselServer.exe 等 23 个文件 SHA256 全等)`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1304 [EXACT]

- OLD: `- silent install 0.18.8.0 -> exit 0, HKLM InstallDir = C:\Program Files\fluxing, HKCU RimeUserDir = C:\Program Files\fluxing\user1\fluxing, default.yaml + spec 014 [F]`
- NEW: `- silent install 0.18.8.0 -> exit 0, HKLM InstallDir = C:\Program Files\fluxing, HKCU RimeUserDir = C:\Program Files\fluxing\user1\fluxing, default.yaml 含 spec 014 修复`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1308 [EXACT]

- OLD: `- L19 [F]<11>[F][F]: "W&2[F][N][F][F][F]<0E> yaml [F],[F][F]a binding X(/`
- NEW: `- L19 自我审查: "字符串断言只能证明 yaml 文本里某条 binding 存在/不存在, 不能证明运行时 binding 表的行为"`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1310 [EMPTY]

- OLD: `[F][F]<0E>[F]L[F] binding h[F]L:"`
- NEW: `(blank)`
- provenance: deliberate blank (fragment absorbed by a recovered neighbour)

### line 1311 [EXACT]

- OLD: `- L19 ͽ: c/ L19 ( TestDefaultHotkeys 31/31 PASS e"[F][F]"[F]`
- NEW: `- L19 反讽: 正是 L19 用 TestDefaultHotkeys 31/31 PASS 来"验证"修复, 但修复**本身**就是 over-correction, 把 spec 005 v1.1 承诺的 Shift 选候选能力废掉。**字符串测试既不能验证修复有效, 也不能验证修复没引入新 bug**。`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1312 [EMPTY]

- OLD: `, F[F]`
- NEW: `(blank)`
- provenance: deliberate blank (fragment absorbed by a recovered neighbour)

### line 1313 [EMPTY]

- OLD: `**,[F]**1/ over-correction, [F] spec 005 v1.1 [F][F] Shift 	<19>	[F][F][F][F]<02>**W&2K[F][F]`
- NEW: `(blank)`
- provenance: deliberate blank (fragment absorbed by a recovered neighbour)

### line 1314 [EMPTY]

- OLD: `[F][F][F][F]`
- NEW: `(blank)`
- provenance: deliberate blank (fragment absorbed by a recovered neighbour)

### line 1315 [EMPTY]

- OLD: `	H, _`
- NEW: `(blank)`
- provenance: deliberate blank (fragment absorbed by a recovered neighbour)

### line 1316 [EMPTY]

- OLD: `[F][F][F][F]`
- NEW: `(blank)`
- provenance: deliberate blank (fragment absorbed by a recovered neighbour)

### line 1317 [EMPTY]

- OLD: `[F]<15>e[F] bug**<02>`
- NEW: `(blank)`
- provenance: deliberate blank (fragment absorbed by a recovered neighbour)

### line 1318 [EMPTY]

- OLD: `- spec 014 `
- NEW: `(blank)`
- provenance: deliberate blank (fragment absorbed by a recovered neighbour)

### line 1319 [EXACT]

- OLD: `[F][F]VU[N] yaml W&2K[F], [F] TestShiftSelectBinding \:,[F]*[F][F] runtime Q[F]K[F], b<10>"$*K[F]q<0C>[F][F][N]*[F]`
- NEW: `- spec 014 不再依赖单一 yaml 字符串测试, 加 TestShiftSelectBinding 作为第二个独立 runtime 契约测试, 形成"两个测试共同验证一个修复"的交叉验证模式。`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1320 [EMPTY]

- OLD: `"[F][F]Ɍ[F]!<0F><02>`
- NEW: `(blank)`
- provenance: deliberate blank (fragment absorbed by a recovered neighbour)

### line 1321 [EMPTY]

- OLD: `- s.[F][F]'Ӻ: [F]U yaml-only [F]`
- NEW: `(blank)`
- provenance: deliberate blank (fragment absorbed by a recovered neighbour)

### line 1322 [EXACT]

- OLD: `[F][F][F]	**[F]<11> 2 *[F][F]K[F]** [F][F] (M L18/L19 [F]"passing test, regressed behavior" w1):`
- NEW: `- 关键架构性结论: 任何 yaml-only 修复都应该有**至少 2 个独立测试** 覆盖 (避免 L18/L19 的"passing test, regressed behavior" 陷阱):`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1323 [EXACT]

- OLD: `  1. TestDefaultHotkeys.exe (string-level yaml Q[F])`
- NEW: `  1. TestDefaultHotkeys.exe (string-level yaml 契约)`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1324 [EXACT]

- OLD: `  2. TestShiftSelectBinding.exe (yaml Q[F] + [F]t key L: mock, 赌[F])`
- NEW: `  2. TestShiftSelectBinding.exe (yaml 契约 + 完整 key 行为 mock, 跨段验证)`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1326 [EXACT]

- OLD: `### 2 L19 `
- NEW: `### 防 L19 复发`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1327 [EMPTY]

- OLD: `[F]`
- NEW: `(blank)`
- provenance: deliberate blank (fragment absorbed by a recovered neighbour)

### line 1329 [EMPTY]

- OLD: `luxing scope, spec 014 在 tasks.md T001 明确加注: "<07>ccept: Shift+Shift_L/R 形式 (modifier=Shift) 确保 keycode=Shift_L, modifier=0 的 TSF release event 不会误匹配"`
- NEW: `(blank)`
- provenance: deliberate blank (fragment absorbed by a recovered neighbour)

### line 1330 [NEW]

- OLD: `<1A>[F]9M"`
- NEW: `- AGENTS.md 已在 §3.2 标记 Fluxing scope, spec 014 在 tasks.md T001 明确加注: "accept: Shift+Shift_L/R 形式 (modifier=Shift) 确保 keycode=Shift_L, modifier=0 的 TSF release event 不会误匹配"`
- provenance: NO SOURCE -- needs a manual context decision

### line 1335 [EMPTY]

- OLD: `<1A> release-match)`
- NEW: `(blank)`
- provenance: deliberate blank (fragment absorbed by a recovered neighbour)

### line 1337 [EXACT]

- OLD: `### [F]s`
- NEW: `### 相关`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1339 [SELF]

- OLD: `- L18 (被 L19 替代): 只切走了 <07>lways: Shift+Shift_L/R, 未切 has_menu: Shift+Shift_L/R`
- NEW: `- L18 (被 L19 替代): 只切走了 always: Shift+Shift_L/R, 未切 has_menu: Shift+Shift_L/R`
- provenance: derived from the damaged line itself (control-char / escape de-coding)

### line 1342 [EXACT]

- OLD: `- spec 005 v1.1 design.md [F]2.2 (Shift+Shift_L/R binding b<0F>[F][F][F][F]c)`
- NEW: `- spec 005 v1.1 design.md §2.2 (Shift+Shift_L/R binding 形式的设计文档)`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1343 [EXACT]

- OLD: `- spec 012 (L16): Shift+Shift_L/R binding b<0F>[F][F][F][F], F*<1F>[F] ship`
- NEW: `- spec 012 (L16): Shift+Shift_L/R binding 形式的实现路径, 但未真实 ship`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1345 [EMPTY]

- OLD: `): ( spec 012 [F]@`
- NEW: `(blank)`
- provenance: deliberate blank (fragment absorbed by a recovered neighbour)

### line 1346 [EMPTY]

- OLD: ` + KՆ[F] + <1F>[F] ship`
- NEW: `(blank)`
- provenance: deliberate blank (fragment absorbed by a recovered neighbour)

### line 1351 [EXACT]

- OLD: `**q[F]H,**: 0.18.6.0 / 0.18.7.0 / 0.18.8.0 (all test projects affected; pre-existing bug)`
- NEW: `**影响版本**: 0.18.6.0 / 0.18.7.0 / 0.18.8.0 (all test projects affected; pre-existing bug)`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1352 [EXACT]

- OLD: `**[F]`
- NEW: `**修复版本**: 0.18.9.0 (spec 015)`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1357 [NEW]

- OLD: `spec 015 ( build + [F][F]L TestResponseParser / TestWeaselIPC [F]Ѱ<1A>$* .cpp +>[F]	 system("pause"); <03>(<0C>'[F]( return KM<02>[F]/ Windows [F]6[F]<0F> UX !<0F><08>[F][F] .exe [F];^I[F]	.	<0C>F(^[F][F][F][F]<08>CI<01><1A>,<01>stdin ͚<11>	<0B><0C>s[F] stdin <1A>[F] system("pause") [F][F] 0xC0000005 ACCESS_VIOLATION<0C>K[F][F][F] crash<0C>CI h[F][F]`
- NEW: `spec 015 在 build + 试运行 TestResponseParser / TestWeaselIPC 时发现：两个 .cpp 末尾都有 system("pause"); 调用，紧跟在 return 之前。这是 Windows 控制台交互式 UX 模式（双击 .exe 时阻塞等待按键），但在非交互环境（CI、脚本、stdin 重定向）下，关闭 stdin 会让 system("pause") 触发 0xC0000005 ACCESS_VIOLATION，测试直接 crash，CI 全绿过不了。`
- provenance: NO SOURCE -- needs a manual context decision

### line 1358 [EMPTY]

- OLD: `[F]<02>`
- NEW: `(blank)`
- provenance: deliberate blank (fragment absorbed by a recovered neighbour)

### line 1364 [NEW]

- OLD: `spec 015 <16>[F] scripts\run-tests.bat [F]Ѱ<1A> d system(pause) <0E><0C>TestResponseParser [F]: test_4 [F] BOOST_ASSERT(2 == c.candies.size()) 1%<0C><03>( <07>bort()<0C>abort ( Windows Release [F][F][F]l<10> 0xC0000005 STATUS_FATAL_APP_EXIT<0C>s -1073741819<02>`
- NEW: `spec 015 编写 scripts\run-tests.bat 时发现：删除 system(pause) 后，TestResponseParser 因为 test_4 的 BOOST_ASSERT(2 == c.candies.size()) 失败，调用 abort()，abort 在 Windows Release 构建里转成 0xC0000005 STATUS_FATAL_APP_EXIT，即 -1073741819。`
- provenance: NO SOURCE -- needs a manual context decision

### line 1366 [PARTIAL]

- OLD: `F -1073741819 \: signed 32-bit /<1F>p`
- NEW: `但 -1073741819 作为 signed 32-bit 是负数，< 1，结果 batch 误判为 "no error"，脚本继续报告 === ALL TESTS PASSED === + exit 0。`
- provenance: substring of a baseline line in 0c0716c36a (substring of a baseline line)

### line 1368 [EXACT]

- OLD: `### [F]`
- NEW: `### 修复`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1370 [NEW]

- OLD: `1. **TestResponseParser.cpp + TestWeaselIPC.cpp**:  d   system("pause"); L<08>byte-level <16>[F]<0C>UTF-8 [F] BOM<0C>CRLF	<02>`
- NEW: `1. **TestResponseParser.cpp + TestWeaselIPC.cpp**: 删除 system("pause"); 行（byte-level 编辑，UTF-8 无 BOM，CRLF）。`
- provenance: NO SOURCE -- needs a manual context decision

### line 1371 [EXACT]

- OLD: `2. **scripts\run-tests.bat**: 9( setlocal enableextensions enabledelayedexpansion + if !errorlevel! NEQ 0 set "FAIL=1" + set "FINAL_RC=!FAIL!" + endlocal & set "OUTER_RC=%FINAL_RC%" !<0F>cn <12> -1073741819 I<1F><[F]<16>B<02>`
- NEW: `2. **scripts\run-tests.bat**: 改用 setlocal enableextensions enabledelayedexpansion + if !errorlevel! NEQ 0 set "FAIL=1" + set "FINAL_RC=!FAIL!" + endlocal & set "OUTER_RC=%FINAL_RC%" 模式正确传递 -1073741819 等负值给外层。`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1372 [EXACT]

- OLD: `3. **scripts\run-tests.bat**: ( set "SOL_DIR=%CD%"<08>[F][F][F][F]>͜`	 [F] msbuild [F] standalone-build SolutionDir [F][F]<02>`
- NEW: `3. **scripts\run-tests.bat**: 用 set "SOL_DIR=%CD%"（绝对路径无尾反斜杠）传给 msbuild 解决 standalone-build SolutionDir 解析问题。`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1375 [EXACT]

- OLD: `### [F][F]`
- NEW: `### 验证`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1377 [EXACT]

- OLD: `- scripts\run-tests.bat (SM HEAD<08>TestResponseParser test_4 fail	`
- NEW: `- scripts\run-tests.bat 在当前 HEAD（TestResponseParser test_4 fail）下：exit 1，输出 === TESTS FAILED ===。`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1381 [EXACT]

- OLD: `- TestResponseParser: test_4 1%<08>spec 015 sec 5 [F]U: out-of-scope WeaselIPC bug<0C>[N][F] spec 016+ [F]`
- NEW: `- TestResponseParser: test_4 失败（spec 015 sec 5 记录为 out-of-scope WeaselIPC bug，需要 spec 016+ 修复）`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1385 [EMPTY]

- OLD: `[F]`
- NEW: `(blank)`
- provenance: deliberate blank (fragment absorbed by a recovered neighbour)

### line 1388 [EMPTY]

- OLD: `[F] system("pause") <16>I[F]i<08>getchar<01>_getch I	<02>`
- NEW: `(blank)`
- provenance: deliberate blank (fragment absorbed by a recovered neighbour)

### line 1389 [EXACT]

- OLD: `- [F]U[F][F] test project [F]{( scripts\run-tests.bat [F]{[F]<1B>`
- NEW: `- 任何新增 test project 必须在 scripts\run-tests.bat 里登记；不允许"test by hand"。`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1390 [EMPTY]

- OLD: `A[F]"test by hand"<02>`
- NEW: `(blank)`
- provenance: deliberate blank (fragment absorbed by a recovered neighbour)

### line 1391 [EXACT]

- OLD: `- [F]U( exit code Z gate [F] .bat <1A>,<0C>[F]{( if !errorlevel! NEQ 0 !<0F><0C>`
- NEW: `- 任何使用 exit code 做 gate 的 .bat 脚本，必须用 if !errorlevel! NEQ 0 模式，不用 if errorlevel 1。`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1392 [EMPTY]

- OLD: `( if errorlevel 1<02>`
- NEW: `(blank)`
- provenance: deliberate blank (fragment absorbed by a recovered neighbour)

### line 1395 [EXACT]

- OLD: `### [F]s`
- NEW: `### 相关`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1398 [EXACT]

- OLD: `- L11: BOM double rule<02>`
- NEW: `- L11: BOM double rule。`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 1400 [EXACT]

- OLD: `- spec 014 / 015: "$*[F][F] test + [N]a lesson" pattern<02>`
- NEW: `- spec 014 / 015: "两个独立 test + 一条 lesson" pattern。`
- provenance: verbatim line in 0c0716c36a (exact line match)

### line 3619 [NEW]

- OLD: `- **Attempt 1 (L50, v0.18.27.2)**: `D2D1::RenderTargetProperties()` default dpi=96, backing store = child logical size (no DPI scaling), `D2D1::RectF` = logical. On 144 DPI child logical 170x17 [F][F] backing store 170x17, D2D treats DIP as physical (dpi=96 [F][F] no scale) [F][F] text and rounded rects drawn at 170x17 logical into a 170x17 backing store. **Result**: text overflows the smaller child HWND (logical 26 = physical 17 [F][F] 17pt text = 22.7 physical pixels rendered into 17-pixel child). No D2D `Clear()` call, so D2D backing store is opaque black by default and shows through anywhere a pixel is not overdrawn.`
- NEW: `- **Attempt 1 (L50, v0.18.27.2)**: `D2D1::RenderTargetProperties()` default dpi=96, backing store = child logical size (no DPI scaling), `D2D1::RectF` = logical. On 144 DPI child logical 170x17 → backing store 170x17, D2D treats DIP as physical (dpi=96 → no scale) → text and rounded rects drawn at 170x17 logical into a 170x17 backing store. **Result**: text overflows the smaller child HWND (logical 26 = physical 17 → 17pt text = 22.7 physical pixels rendered into 17-pixel child). No D2D `Clear()` call, so D2D backing store is opaque black by default and shows through anywhere a pixel is not overdrawn.`
- provenance: NO SOURCE -- needs a manual context decision

### line 3621 [NEW]

- OLD: `- **Attempt 2 (spec 041 v0.18.28.0 first pass)**: `D2DRenderer::GetPhysicalClientRect` converts logical [F][F] physical via `MulDiv(rc, 96, dpi)`. Backing store pixelSize = logical [F][F] 96 / dpi (i.e. `physical_w = logical * 96 / dpi` shrink). `D2D1::RectF` = physical. On 144 DPI child logical 170x17 [F][F] physical 113x11 [F][F] backing store 113x11, `D2D1::RectF(0, 0, 113, 11)`. D2D rt dpi still default 96 [F][F] D2D treats DIP=physical, so 113x11 written 1:1. **Result**: backing store now smaller than HWND physical surface (170x17), and text rendered at physical pixel size is too small to read.`
- NEW: `- **Attempt 2 (spec 041 v0.18.28.0 first pass)**: `D2DRenderer::GetPhysicalClientRect` converts logical → physical via `MulDiv(rc, 96, dpi)`. Backing store pixelSize = logical × 96 / dpi (i.e. `physical_w = logical * 96 / dpi` shrink). `D2D1::RectF` = physical. On 144 DPI child logical 170x17 → physical 113x11 → backing store 113x11, `D2D1::RectF(0, 0, 113, 11)`. D2D rt dpi still default 96 → D2D treats DIP=physical, so 113x11 written 1:1. **Result**: backing store now smaller than HWND physical surface (170x17), and text rendered at physical pixel size is too small to read.`
- provenance: NO SOURCE -- needs a manual context decision

### line 3623 [NEW]

- OLD: `- **Attempt 3 (the one that works)**: `D2DRenderer::GetPhysicalClientRect` returns the raw `GetClientRect` value (== HWND physical size on V1 PerMonitor DPI aware process; child logical = child physical on V1 [F][F] verified empirically with `GetWindowDpiAwarenessContext` returning V1, `GetDpiForWindow` returning 144, and the child physical size exactly matching `logical [F][F] 96/144`). Backing store = HWND physical size. D2D rt dpi = 96 (default). `D2D1::RectF` = physical (1:1 to backing store). All 4 controls add `rt->Clear(D2D1::ColorF(GetSysColor(COLOR_WINDOW), 1.0f))` after `BeginDraw` so non-text pixels do not show the opaque-black D2D backing store.`
- NEW: `- **Attempt 3 (the one that works)**: `D2DRenderer::GetPhysicalClientRect` returns the raw `GetClientRect` value (== HWND physical size on V1 PerMonitor DPI aware process; child logical = child physical on V1 — verified empirically with `GetWindowDpiAwarenessContext` returning V1, `GetDpiForWindow` returning 144, and the child physical size exactly matching `logical × 96/144`). Backing store = HWND physical size. D2D rt dpi = 96 (default). `D2D1::RectF` = physical (1:1 to backing store). All 4 controls add `rt->Clear(D2D1::ColorF(GetSysColor(COLOR_WINDOW), 1.0f))` after `BeginDraw` so non-text pixels do not show the opaque-black D2D backing store.`
- provenance: NO SOURCE -- needs a manual context decision

### line 3627 [NEW]

- OLD: `1. **`D2D1_HwndRenderTargetProperties.pixelSize` is in PHYSICAL pixels**, not logical. It must match the HWND physical client area. Passing logical coords here shrinks the backing store below the HWND; passing logical [F][F] dpi/96 makes it larger than the HWND and Windows clips on EndDraw.`
- NEW: `1. **`D2D1_HwndRenderTargetProperties.pixelSize` is in PHYSICAL pixels**, not logical. It must match the HWND physical client area. Passing logical coords here shrinks the backing store below the HWND; passing logical × dpi/96 makes it larger than the HWND and Windows clips on EndDraw.`
- provenance: NO SOURCE -- needs a manual context decision

### line 3635 [NEW]

- OLD: `5. **On V1 PerMonitor DPI aware process, child HWND physical size = logical size passed to `CreateWindowExW` [F][F] 96/dpi** (i.e. **inverted** from the top-level behavior where physical = logical [F][F] dpi/96). Verified on a 144 DPI monitor: title_rc logical (10, 4, 268, 30) width 258 [F][F] child physical 171 (= 258 [F][F] 96/144), height 26 [F][F] child physical 17. Top-level dialog logical 300[F][F]150 [F][F] physical 200[F][F]100 (= 300 [F][F] 144/96) follows the top-level pattern. This asymmetry is **the single most confusing thing** about V1 DPI and is undocumented in the Microsoft Per-Monitor DPI whitepaper; you have to discover it empirically with `GetWindowDpiAwarenessContext` + `GetClientRect` + `GetDpiForWindow` on each HWND.`
- NEW: `5. **On V1 PerMonitor DPI aware process, child HWND physical size = logical size passed to `CreateWindowExW` × 96/dpi** (i.e. **inverted** from the top-level behavior where physical = logical × dpi/96). Verified on a 144 DPI monitor: title_rc logical (10, 4, 268, 30) width 258 → child physical 171 (= 258 × 96/144), height 26 → child physical 17. Top-level dialog logical 300×150 → physical 200×100 (= 300 × 144/96) follows the top-level pattern. This asymmetry is **the single most confusing thing** about V1 DPI and is undocumented in the Microsoft Per-Monitor DPI whitepaper; you have to discover it empirically with `GetWindowDpiAwarenessContext` + `GetClientRect` + `GetDpiForWindow` on each HWND.`
- provenance: NO SOURCE -- needs a manual context decision

### line 3642 [NEW]

- OLD: `- The second attempt assumed `GetClientRect` in a V1 process returns logical coords and you must scale to physical. For **top-level** windows on V1 that is true, but for **child** windows on V1 it is false (the child physical size is `logical [F][F] 96/dpi`, not `logical [F][F] dpi/96`). The spec 041 plan was written for top-level and applied to child, which is why it "made the backing store 1.5x too large" [F][F] the comment in the code was technically right for top-level and wrong for child.`
- NEW: `- The second attempt assumed `GetClientRect` in a V1 process returns logical coords and you must scale to physical. For **top-level** windows on V1 that is true, but for **child** windows on V1 it is false (the child physical size is `logical × 96/dpi`, not `logical × dpi/96`). The spec 041 plan was written for top-level and applied to child, which is why it "made the backing store 1.5x too large" — the comment in the code was technically right for top-level and wrong for child.`
- provenance: NO SOURCE -- needs a manual context decision

### line 3647 [NEW]

- OLD: `- `D:\TEMP\qp-trace9.ps1` posts Alt+, (WM_HOTKEY id 9001) to the WeaselIPC HWND, then `EnumChildWindows` on the `FluxingQuickPanel_v0` class to print each child's logical + physical rect, then `PrintWindow` with `PW_RENDERFULLCONTENT` to a PNG. Running this after every code change gave a 5-second visual diff loop. Without this, we would have shipped black bars to production (this is what v0.18.27.2 did [F][F] the L50 fallback existed in source but was never tested with a real 144 DPI monitor).`
- NEW: `- `D:\TEMP\qp-trace9.ps1` posts Alt+, (WM_HOTKEY id 9001) to the WeaselIPC HWND, then `EnumChildWindows` on the `FluxingQuickPanel_v0` class to print each child's logical + physical rect, then `PrintWindow` with `PW_RENDERFULLCONTENT` to a PNG. Running this after every code change gave a 5-second visual diff loop. Without this, we would have shipped black bars to production (this is what v0.18.27.2 did — the L50 fallback existed in source but was never tested with a real 144 DPI monitor).`
- provenance: NO SOURCE -- needs a manual context decision

### line 3652 [NEW]

- OLD: `- Use `D2D1::RenderTargetProperties()` with default dpiX/dpiY (= 0 [F][F] D2D1_DEVICE_CONTEXT_DEFAULT_DPI = 96). Do NOT pass `static_cast<float>(dpi)`.`
- NEW: `- Use `D2D1::RenderTargetProperties()` with default dpiX/dpiY (= 0 → D2D1_DEVICE_CONTEXT_DEFAULT_DPI = 96). Do NOT pass `static_cast<float>(dpi)`.`
- provenance: NO SOURCE -- needs a manual context decision

### line 3660 [NEW]

- OLD: `- Do not assume `GetClientRect` returns the same coordinate system on top-level vs child HWNDs in V1 PerMonitor DPI. Top-level: `physical = logical [F][F] dpi/96`. Child: `physical = logical [F][F] 96/dpi`. The two are opposite. Verify with `GetDpiForWindow(hwnd)` on the specific HWND before writing the size calc.`
- NEW: `- Do not assume `GetClientRect` returns the same coordinate system on top-level vs child HWNDs in V1 PerMonitor DPI. Top-level: `physical = logical × dpi/96`. Child: `physical = logical × 96/dpi`. The two are opposite. Verify with `GetDpiForWindow(hwnd)` on the specific HWND before writing the size calc.`
- provenance: NO SOURCE -- needs a manual context decision

### line 3664 [NEW]

- OLD: `- Do not modify `WeaselServer/QuickPanelDialog.cpp` (QP geometry) to "fix" a DPI rendering bug [F][F] the QP physical 200[F][F]100 in 144 DPI is intentionally small (spec 038 anti-pattern AP-041-B). The fix belongs in the 4 control WM_PAINT handlers, not the dialog.`
- NEW: `- Do not modify `WeaselServer/QuickPanelDialog.cpp` (QP geometry) to "fix" a DPI rendering bug — the QP physical 200×100 in 144 DPI is intentionally small (spec 038 anti-pattern AP-041-B). The fix belongs in the 4 control WM_PAINT handlers, not the dialog.`
- provenance: NO SOURCE -- needs a manual context decision

### line 3666 [NEW]

- OLD: `**Related L##**: L48 (FluxingD2DRenderer atexit crash [F][F] also rt lifecycle), L51 (D2D fallback for unavailable D2D [F][F] orthogonal to this; L52's fix does not regress L51), L47 (BOM/line ending damage chain [F][F] same PCH include order constraint, but D2D-specific). L52 is the **first instance** of a v0 DPI handling post-mortem; future PerMonitor V2 / multi-monitor work in spec 044+ should reference L52's empirical proof + pattern.`
- NEW: `**Related L##**: L48 (FluxingD2DRenderer atexit crash — also rt lifecycle), L51 (D2D fallback for unavailable D2D — orthogonal to this; L52's fix does not regress L51), L47 (BOM/line ending damage chain — same PCH include order constraint, but D2D-specific). L52 is the **first instance** of a v0 DPI handling post-mortem; future PerMonitor V2 / multi-monitor work in spec 044+ should reference L52's empirical proof + pattern.`
- provenance: NO SOURCE -- needs a manual context decision

### line 3690 [SELF]

- OLD: `The L43 cure (/LTCG:OFF per-target) was already applied to WeaselTSF/xmake.lua in spec 031 (0.18.22.0). The global xmake.lua line 64 also has <07>dd_ldflags("/LTCG:OFF /INCREMENTAL:NO", {force = true}). So **WeaselServer.exe does NOT have the L43 dead-strip problem** — all referenced symbols ARE linked. The verification error was in the *search tool*, not the binary.`
- NEW: `The L43 cure (/LTCG:OFF per-target) was already applied to WeaselTSF/xmake.lua in spec 031 (0.18.22.0). The global xmake.lua line 64 also has add_ldflags("/LTCG:OFF /INCREMENTAL:NO", {force = true}). So **WeaselServer.exe does NOT have the L43 dead-strip problem** — all referenced symbols ARE linked. The verification error was in the *search tool*, not the binary.`
- provenance: derived from the damaged line itself (control-char / escape de-coding)

### line 3735 [SELF]

- OLD: `Add L53 to the systematic-debugging checklist in AGENTS.md §6 and <0B>erification-before-completion skill: "When verifying Windows PE binary contents, use UTF-16 string decode + dumpbin /DISASM + byte-pattern count. Never rely on ASCII strings alone."`
- NEW: `Add L53 to the systematic-debugging checklist in AGENTS.md §6 and verification-before-completion skill: "When verifying Windows PE binary contents, use UTF-16 string decode + dumpbin /DISASM + byte-pattern count. Never rely on ASCII strings alone."`
- provenance: derived from the damaged line itself (control-char / escape de-coding)

### line 4525 [SELF]

- OLD: `  cd C:\Program Files<0C>luxing\weasel`
- NEW: `  cd C:\Program Files\fluxing\weasel`
- provenance: derived from the damaged line itself (control-char / escape de-coding)

### line 4563 [SELF]

- OLD: `!define FLUXING_ROOT $INSTDIR<0C>luxing`
- NEW: `!define FLUXING_ROOT $INSTDIR\fluxing`
- provenance: derived from the damaged line itself (control-char / escape de-coding)

### line 4566 [SELF]

- OLD: ``ForceFluxingSuffix` (line 568-580) appends `<0C>luxing` to `$INSTDIR` if the last 7 chars are not 'fluxing'. So on a default install:`
- NEW: ``ForceFluxingSuffix` (line 568-580) appends `\fluxing` to `$INSTDIR` if the last 7 chars are not 'fluxing'. So on a default install:`
- provenance: derived from the damaged line itself (control-char / escape de-coding)

### line 4567 [SELF]

- OLD: `- `/D=D:\Program Files<0C>luxing` -> $INSTDIR = `D:\Program Files<0C>luxing``
- NEW: `- `/D=D:\Program Files\fluxing` -> $INSTDIR = `D:\Program Files\fluxing``
- provenance: derived from the damaged line itself (control-char / escape de-coding)

### line 4568 [SELF]

- OLD: `- ForceFluxingSuffix sees no 'fluxing' suffix -> appends -> $INSTDIR = `D:\Program Files<0C>luxing<0C>luxing``
- NEW: `- ForceFluxingSuffix sees no 'fluxing' suffix -> appends -> $INSTDIR = `D:\Program Files\fluxing\fluxing``
- provenance: derived from the damaged line itself (control-char / escape de-coding)

### line 4569 [SELF]

- OLD: `- WEASEL_ROOT = `$INSTDIR\weasel` = `D:\Program Files<0C>luxing<0C>luxing\weasel` (DOES NOT EXIST)`
- NEW: `- WEASEL_ROOT = `$INSTDIR\weasel` = `D:\Program Files\fluxing\fluxing\weasel` (DOES NOT EXIST)`
- provenance: derived from the damaged line itself (control-char / escape de-coding)

### line 4571 [SELF]

- OLD: `  -> expanded to: `regsvr32 /s "D:\Program Files<0C>luxing<0C>luxing\weaselx64.dll"` (DOES NOT EXIST)`
- NEW: `  -> expanded to: `regsvr32 /s "D:\Program Files\fluxing\fluxing\weaselx64.dll"` (DOES NOT EXIST)`
- provenance: derived from the damaged line itself (control-char / escape de-coding)

### line 4572 [SELF]

- OLD: `- The actual files are at: `D:\Program Files<0C>luxing\weasel\weaselx64.dll``
- NEW: `- The actual files are at: `D:\Program Files\fluxing\weasel\weaselx64.dll``
- provenance: derived from the damaged line itself (control-char / escape de-coding)

### line 4583 [SELF]

- OLD: `This guarantees the path exists: `$R3\weasel\weaselx64.dll` is always `D:\Program Files<0C>luxing\weasel\weaselx64.dll` regardless of whether ForceFluxingSuffix ran.`
- NEW: `This guarantees the path exists: `$R3\weasel\weaselx64.dll` is always `D:\Program Files\fluxing\weasel\weaselx64.dll` regardless of whether ForceFluxingSuffix ran.`
- provenance: derived from the damaged line itself (control-char / escape de-coding)

### line 4589 [SELF]

- OLD: `3. silent install runs `regsvr32 /s "D:\Program Files<0C>luxing\weasel\weaselx64.dll"` (now correct path)`
- NEW: `3. silent install runs `regsvr32 /s "D:\Program Files\fluxing\weasel\weaselx64.dll"` (now correct path)`
- provenance: derived from the damaged line itself (control-char / escape de-coding)

### line 4597 [SELF]

- OLD: `- **AP-L63-A**: NSIS variable scoping. `$INSTDIR` is the install path with L14-suffix `<0C>luxing` appended, but the actual file layout is `<install>\weasel\`. **The relationship is `<install>\weasel\<file>`, not `<install with fluxing suffix>\<file>`.** I should have looked at the actual file layout to determine the right path variable.`
- NEW: `- **AP-L63-A**: NSIS variable scoping. `$INSTDIR` is the install path with L14-suffix `\fluxing` appended, but the actual file layout is `<install>\weasel\`. **The relationship is `<install>\weasel\<file>`, not `<install with fluxing suffix>\<file>`.** I should have looked at the actual file layout to determine the right path variable.`
- provenance: derived from the damaged line itself (control-char / escape de-coding)

### line 4954 [SELF]

- OLD: `%LOCALAPPDATA%<0C>luxing\crash\YYYYMMDD-HHMMSS-<code>.dmp. This is the`
- NEW: `%LOCALAPPDATA%\fluxing\crash\YYYYMMDD-HHMMSS-<code>.dmp. This is the`
- provenance: derived from the damaged line itself (control-char / escape de-coding)

### line 4961 [SELF]

- OLD: `  %LOCALAPPDATA%<0C>luxing\crash\<timestamp>-<code>.dmp`
- NEW: `  %LOCALAPPDATA%\fluxing\crash\<timestamp>-<code>.dmp`
- provenance: derived from the damaged line itself (control-char / escape de-coding)

### line 4968 [SELF]

- OLD: `from %LOCALAPPDATA%<0C>luxing\crash\ for analysis.`
- NEW: `from %LOCALAPPDATA%\fluxing\crash\ for analysis.`
- provenance: derived from the damaged line itself (control-char / escape de-coding)

### line 5102 [SELF]

- OLD: `  - %LOCALAPPDATA%<0C>luxing\crash\ does NOT accumulate new dumps`
- NEW: `  - %LOCALAPPDATA%\fluxing\crash\ does NOT accumulate new dumps`
- provenance: derived from the damaged line itself (control-char / escape de-coding)

### line 5444 [SELF]

- OLD: `  `GetModuleFileNameW(NULL, ...)` (executable directory) + `"<0C>luxing-logo.png"`.`
- NEW: `  `GetModuleFileNameW(NULL, ...)` (executable directory) + `"\fluxing-logo.png"`.`
- provenance: derived from the damaged line itself (control-char / escape de-coding)

### line 9184 [SELF]

- OLD: `- 三件套 MD5 全 match installer (<08>2d1236c / <08>1a74c6f / 14dfdfb0) → 看似`
- NEW: `- 三件套 MD5 全 match installer (b2d1236c / b1a74c6f / 14dfdfb0) → 看似`
- provenance: derived from the damaged line itself (control-char / escape de-coding)

### line 9196 [SELF]

- OLD: `- <08>uild.log 显示 v0.19.0.54 ship 走 **MSBuild 路径** (WeaselServer.vcxproj ->`
- NEW: `- build.log 显示 v0.19.0.54 ship 走 **MSBuild 路径** (WeaselServer.vcxproj ->`
- provenance: derived from the damaged line itself (control-char / escape de-coding)

### line 9198 [SELF]

- OLD: `- v0.19.0.55 (commit 8559ed4 + 823bc1f) 改走 **<08>uild_v055.ps1 → xbuild.bat`
- NEW: `- v0.19.0.55 (commit 8559ed4 + 823bc1f) 改走 **build_v055.ps1 → xbuild.bat`
- provenance: derived from the damaged line itself (control-char / escape de-coding)

### line 9199 [SELF]

- OLD: `  weasel installer → xmake 路径**。WeaselServer/xmake.lua 的 <07>fter_build`
- NEW: `  weasel installer → xmake 路径**。WeaselServer/xmake.lua 的 after_build`
- provenance: derived from the damaged line itself (control-char / escape de-coding)

### line 9212 [SELF]

- OLD: `1. **F106-A build infra guardrail**: 在 WeaselServer/xmake.lua 的 <07>fter_build`
- NEW: `1. **F106-A build infra guardrail**: 在 WeaselServer/xmake.lua 的 after_build`
- provenance: derived from the damaged line itself (control-char / escape de-coding)

### line 9218 [SELF]

- OLD: `2. **F106-B build_v0XX.ps1 wrapper guard**: 在每个 <08>uild_v0XX.ps1 末尾强制 verify:`
- NEW: `2. **F106-B build_v0XX.ps1 wrapper guard**: 在每个 build_v0XX.ps1 末尾强制 verify:`
- provenance: derived from the damaged line itself (control-char / escape de-coding)

### line 9250 [SELF]

- OLD: `- <08>uild.xmake.after_build.no-verify (主要) — xmake 的 os.cp(target, output)`
- NEW: `- build.xmake.after_build.no-verify (主要) — xmake 的 os.cp(target, output)`
- provenance: derived from the damaged line itself (control-char / escape de-coding)

### line 9252 [SELF]

- OLD: `- <08>uild.false-positive-md5-on-zero-file (主要) — 当 source build 和 installed`
- NEW: `- build.false-positive-md5-on-zero-file (主要) — 当 source build 和 installed`
- provenance: derived from the damaged line itself (control-char / escape de-coding)

### line 9264 [SELF]

- OLD: `  <08>uild_v0XX.ps1 / xbuild.bat 的提交, PR 必带「关键 EXE size + MZ signature`
- NEW: `  build_v0XX.ps1 / xbuild.bat 的提交, PR 必带「关键 EXE size + MZ signature`
- provenance: derived from the damaged line itself (control-char / escape de-coding)

### line 9979 [SELF]

- OLD: `**Symptom**: 装机 v0.20.0.0/0.20.0.1 后,切到火流猩输入法,微信/Claude Code/dopus/ToDesk/explorer 全部 0xc0000374 HEAP_CORRUPTION 崩溃。dump 在 `C:<0C>luxing-dumps\`。`
- NEW: `**Symptom**: 装机 v0.20.0.0/0.20.0.1 后,切到火流猩输入法,微信/Claude Code/dopus/ToDesk/explorer 全部 0xc0000374 HEAP_CORRUPTION 崩溃。dump 在 `C:\fluxing-dumps\`。`
- provenance: derived from the damaged line itself (control-char / escape de-coding)
