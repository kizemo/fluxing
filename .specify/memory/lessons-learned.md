# Lessons Learned · 经验教训沉淀

> 范围：Fluxing 项目（rime/weasel fork）从开发事故中提炼的可复用教训。
> 每条以"事故 → 根因 → 教训"格式记录；commit `c0951ca` 教训为本文件起源。

---

## L01 · 中文 UTF-8 文件读写 — PowerShell 5.1 GBK 代码页陷阱

**事故**：commit `c0951ca` 包含的 8 份 spec 文档（含中文 UTF-8）实际为"GBK 字节被错误 Unicode 化再 UTF-8 编码"的混合体。`git hash-object` 验证显示字节 hash 一致（因为 working 文件就是 commit 时写的字节），但中文内容已损坏。

**根因**（PowerShell 5.1 + chcp 936）：

1. `Get-Content -Raw path` 读 UTF-8 文件时，**按当前代码页 (936 = GBK) 解码 UTF-8 字节** → 返回"GBK 字节序列"字符串（即：原始字节被错误当 GBK 双字节字符解读后的 Unicode 码点序列）
2. 在 PowerShell 字符串层做 `$s.Substring(...)` / `-replace` / `+` 等操作 → 字符串里是错误码点
3. `[System.IO.File]::WriteAllText(path, $s, [UTF8Encoding]$false)` 把"GBK 字节序列"当 Unicode 码点写入 → 每个 GBK 字节 (0x00-0xFF) 当 1 个 Unicode 码点 → UTF-8 编码为 2 字节
4. `Get-Content` 读回验证时**走同样的 GBK 路径** → 看起来"自洽"，**未被发现损坏**

**为什么 `git hash-object` 显示一致**：git 算的是字节 hash，working 文件就是 commit 时写的字节，hash 当然一致 — **但内容已坏**。

**教训（必须遵守）**：

| 操作 | 允许 | 禁止 |
|---|---|---|
| 写中文 UTF-8 | `[System.IO.File]::WriteAllText(path, content, [System.Text.UTF8Encoding]::new($false))` | `Out-File` / `>` / `Set-Content` / `Get-Content \| Set-Content` |
| 读中文 UTF-8 | `[System.IO.File]::ReadAllText(path, [System.Text.Encoding]::UTF8)` 或 `ReadAllBytes` + 显式 `UTF8.GetString` | `Get-Content` / `cat` / `[IO.File]::ReadAllText(path)` (无 encoding) |
| 验证 UTF-8 完整性 | byte-level：`[System.IO.File]::ReadAllBytes(path)` 检查首字节 0xE0-0xEF、后续 0x80-0xBF；或 `git hash-object` + 与已验证源比较 | `Get-Content` 读回再 echo — **会再次 GBK 化** |
| 中文内容来源 | 优先从对话上下文取（已 LLM 处理为 Unicode 码点） | 从已损坏文件读再写 — **污染传染** |

**验证脚本模板**：

```powershell
# 写
$content = "中文内容"  # 从对话上下文
[System.IO.File]::WriteAllText($path, $content, [System.Text.UTF8Encoding]::new($false))

# 验证（byte-level）
$bytes = [System.IO.File]::ReadAllBytes($path)
$first30 = ($bytes[0..29] | ForEach-Object { $_.ToString("X2") }) -join " "
Write-Host "First 30 bytes: $first30"
# 期望: 23 20 ... (ASCII 头) 或 E4 ... (中文 UTF-8 头 E0-EF)
# 不期望: C0 C1 C2 C3 C4 ... (GBK 字节被错误 Unicode 化的 2 字节 UTF-8)
```

**commit 前自检**：

```powershell
git add path
git hash-object -w path  # 写 blob
git cat-file -p <hash> | git hash-object --stdin  # 验证可逆
```

---

## L02 · 中文内容修改必须用 byte-level Replace — 避开 PS 字符串层

**事故**：在 `lessons-learned.md` 起草时，多个 `Contains()` / `Replace()` 失败返回 `False`，即使字符串视觉上完全相同。

**根因**：PowerShell 5.1 + chcp 936 下，**[char]0x987A 形式的 Unicode 转义在 `here-string` 中会被错误编码**，导致 PS 解析器将字面量按 GBK 解读后再存为 Unicode 字符串 — 与文件实际 UTF-8 字节解析后的字符串不匹配。

**教训**：

- **能用 byte 操作就用 byte 操作**：`[System.IO.File]::ReadAllBytes(path)` + `[System.Text.Encoding]::UTF8.GetBytes(searchStr)` + byte-by-byte 比较
- **PS 字符串匹配不可靠**：当文件含中文且 PS 5.1 在 GBK 代码页时，**所有 PS 字符串层操作（`-match`、`-replace`、`.Contains()`、`.Replace()`、`.IndexOf()`）都可能给出错误结果**
- **测试 byte 匹配是否正确**：先 `Write-Host` 拼接 byte 数组的 hex，对照文件实际字节序列

**byte-level replace 模板**：

```powershell
$bytes = [System.IO.File]::ReadAllBytes($path)
$marker = [System.Text.Encoding]::UTF8.GetBytes("要找的字节模式")
$start = -1
for ($i = 0; $i -le $bytes.Length - $marker.Length; $i++) {
    $match = $true
    for ($j = 0; $j -lt $marker.Length; $j++) {
        if ($bytes[$i + $j] -ne $marker[$j]) { $match = $false; break }
    }
    if ($match) { $start = $i; break }
}
# 类似找 endMarker
# 拼接: $bytes[0..start] + newBytes + $bytes[end..end]
[System.IO.File]::WriteAllBytes($path, $combined)
```

---

## L03 · librime 1.13 key_binder 配置 — 哪些是支持的、哪些是"假阳性"

**事故**：spec 005 实施的"Shift_L 上屏第 2 候选" 单测 13/13 PASS，但**实际运行不生效**（按 Shift_L 直接上屏英文）。

**根因**（`librime/src/rime/gear/key_binder.cc` + `ascii_composer.cc` 源码验证）：

1. `key_binder` 的 `send:` 字段解析为 `KeyEvent::Parse(target)`，单字符字面量（如 `"2"`）走 `keycode_ = '2' = 0x32`，**合法** — 但仅当 key_binder **能收到**这个 event 时才生效
2. `engine.processors_` 顺序在 rime_ice schema 是 `ascii_composer → recognizer → key_binder → ...`
3. `ascii_composer.cc:80-103` 看到 `ch == XK_Shift_L` 时**无条件记录** `shift_key_pressed_=true` 并 return `kNoop` — key_binder 看不到单独 Shift_L press event
4. 松开 Shift_L 时 ascii_composer 调 `ToggleAsciiModeWithKey(XK_Shift_L)` → 因为 `Shift_L: commit_code` 触发了 `SwitchAsciiMode(true, commit_code)` → 上屏编码 + 切英文
5. `key_binder` 的 `KeyEvent::operator==` 严格比较 `keycode + modifier (含 release mask)` — `binding.accept: shift+l` 是 `(L, Shift)`，**不匹配** `Shift_L release event (Shift_L, RELEASE)` — librime 1.13 key_binder **不处理 release event**

**教训**：

| 假设 | 实际 |
|---|---|
| 单测 PASS = librime 引擎工作 | **错** — 当前 TestDefaultHotkeys.cpp 只测字符串包含，**不测 librime 行为** |
| `send: 2` 等数字 keyevent 让 key_binder 转发"选第 2 候选" | **对**（keycode 0x32 走 selector:146 `ch >= XK_0 && ch <= XK_9` 路径），**但前提是 key_binder 真能收到 send target 重定向 event** |
| `accept: shift+l` 匹配"按住 Shift + 按 L" | **对**（Shift modifier + L keycode 组合）|
| `accept: shift+l` 匹配"松开 Shift_L" | **错**（release event 不参与 binding 匹配）|
| `accept: Shift_L` 匹配"按下单独的 Shift_L" | **对**（keycode=Shift_L 数值、modifier=0）|
| `ascii_composer.switch_key.Shift_L: noop` 等于"完全屏蔽 Shift_L" | **部分对** — `load_bindings:37-38` 跳过 noop 不存 `bindings_` → `ToggleAsciiModeWithKey` 返回 false → **不切英文**；但 ascii_composer 仍会 record `shift_key_pressed_=true` 并 return kNoop → key_binder 仍能看到 Shift_L press event |

**修复 spec 005 rev3**（本仓库 c0e3f85 后 → 待 commit）：

- `ascii_composer.switch_key.Shift_L: noop` + `Shift_R: noop`（防止 ascii_composer 切英文）
- key_binder 加 4 个 binding（搜狗拼音风格兼容）：
  - `accept: Shift_L, send: 2, when: has_menu`（单 Shift_L 按下 → 选第 2 候选）
  - `accept: Shift_R, send: 3, when: has_menu`（单 Shift_R 按下 → 选第 3 候选）
  - `accept: shift+l, send: 2, when: has_menu`（组合键 Shift+L → 选第 2 候选）
  - `accept: shift+r, send: 3, when: has_menu`（组合键 Shift+R → 选第 3 候选）
  - `accept: shift+l, toggle: ascii_mode, when: always`（无候选时 Shift+L 切中英）
  - `accept: shift+r, toggle: ascii_mode, when: always`（无候选时 Shift+R 切中英）
  - `accept: Shift_L, toggle: ascii_mode, when: always`（无候选时单 Shift_L 切中英）
  - `accept: Shift_R, toggle: ascii_mode, when: always`（无候选时单 Shift_R 切中英）

**未来测试改进**：单测必须**实例化 librime engine + 加载 yaml + 模拟 KeyEvent** — 而不是只测 yaml 字符串包含。这需要 C++ 单测框架 + rime_api.h integration，估 4-6 小时工作量。

---

## L04 · librime 1.13 key_binder 支持的 action 类型

**事实**（`librime/src/rime/gear/key_binder.cc:185-220` 源码验证）：

key_binder binding 字段支持 4 类 action：

| 字段 | 作用 | 例子 |
|---|---|---|
| `send: <KeyEvent>` | 把 KeyEvent 注入 engine 事件流 | `send: Page_Up` / `send: 2` |
| `send_sequence: <KeySeq>` | 多按键序列 | `send_sequence: "ctrl+a"` |
| `toggle: <option>` | 切换 option 状态 | `toggle: ascii_mode` / `toggle: ascii_punct` / `toggle: traditionalization` |
| `set_option: <option>` / `unset_option: <option>` | 强制 set/unset | `set_option: simplification` |
| `select: <schema>` | 切换 schema | `select: .next` |

**不支持**：

- 直接调用 `Selector::SelectCandidateAt(ctx, N)` — selector 不暴露给 key_binder
- 自定义 lua callback
- release event binding
- mouse event binding（mouse 由 WeaselUI 处理，不进 RIME engine）

**间接实现"按数字选候选"**：

- 数字 0-9 key event 走 `Selector:146`：`ch >= XK_0 && ch <= XK_9` → `index = ((ch - XK_0) + 9) % 10` → `SelectCandidateAt(ctx, index)`
- 即 `1`→第 1 候选, `2`→第 2 候选, `9`→第 9 候选, `0`→第 10 候选
- binding `send: 2` 重定向按数字 2 即可

---

## L05 · Commit 前的最小验证清单

**标准 5 步验证**（每次 commit 前必做）：

1. **byte-level UTF-8 验证**（中文文件）：
   ```powershell
   $bytes = [System.IO.File]::ReadAllBytes($path)
   "First 30 bytes: " + ($bytes[0..29] | ForEach-Object { $_.ToString("X2") }) -join " "
   ```
   - ASCII 头: 期望 `0x20-0x7E` 范围
   - 中文 UTF-8: 期望 `0xE0-0xEF` 起始 + `0x80-0xBF` 后续
   - GBK 污染信号: 期望**没有** `0xC0/0xC1`（UTF-8 永不合法字节）

2. **git 字节 hash 一致性**：
   ```bash
   git add <files>
   git ls-files -s <path>          # 取出 staging blob hash
   # 写一个临时文件, 把 staging blob 倒出来, 再 hash
   git cat-file -p <hash> > /tmp/check.txt
   git hash-object /tmp/check.txt  # 重新算 hash
   ```
   - 倒出来重 hash 应一致（round-trip test）

3. **单测 PASS**（更新过的单测必须全部 PASS，**包括新加的 case**）：
   ```bash
   cd test/TestDefaultHotkeys
   ./TestDefaultHotkeys.exe ../../output/data/default.yaml
   ```
   - **0 failures 才是真 PASS**（不能"skip 几个 case"）

4. **diff 视觉检查**（中文 commit message / 中文文档）：
   - `git diff --cached` 看是否有"乱码"（如 `鐨 鏂规 椔`） — 这就是 GBK 污染信号

5. **commit message 简洁性**：Conventional Commits 格式 `feat(scope): ...` / `fix(scope): ...` / `docs(spec): ...`

---

## L06 · GitHub MCP 在 Codex 中的不可用场景

**事实**：

- `mcp__github__*` 工具需要 host MCP server 在 `~/.codex/config.toml` 的 `[mcp_servers]` 节注册
- 一次 Codex 会话**不会自动继承**另一次会话的 MCP 配置
- 当 host 配置缺失时，`mcp__github__*` 调用返回 `unsupported call`（不是"权限不足"，是"工具未注册"）

**绕道方案**：

- 给用户**预先写好** GitHub About / Description / Topics 文本，用户手动粘贴
- 用 `mcp__playwright__browser_navigate` 打开 GitHub 网页（playwright 工具**是**在 Codex desktop app 内置的）— 但仅适用于登录后的浏览器会话
- 走 GitHub API（直接 `Invoke-RestMethod` + PAT token）— 不走 MCP

**改进建议**：在 project-level `AGENTS.md` 中明确"在每次 Codex 会话开始时，先验证 `mcp__github__search_repositories` 是否返回 `unsupported call`；若是，提示用户重新配置 MCP"

---

## L07 · `Out-File -Encoding utf8` 与 `[UTF8Encoding]::new($false)` 的区别

**事实**：

- `Out-File -Encoding utf8` 写 **UTF-8 WITH BOM** (3 bytes EF BB BF 前缀)
- `Out-File -Encoding utf8BOM` 同样 BOM
- `Out-File -Encoding utf8NoBOM` 写 UTF-8 NO BOM
- `[System.IO.File]::WriteAllText(path, content, [System.Text.UTF8Encoding]::new($false))` 写 UTF-8 NO BOM
- `[System.IO.File]::WriteAllText(path, content, [System.Text.Encoding]::UTF8)` 写 UTF-8 WITH BOM（默认）

**使用规范**：

- RIME yaml / spec 文档 / 我们项目的所有文本文件 → **UTF-8 NO BOM**
- 工具：用 `[System.IO.File]::WriteAllText(path, content, [System.Text.UTF8Encoding]::new($false))`
- **不要用** `Out-File -Encoding utf8`（会污染 BOM）