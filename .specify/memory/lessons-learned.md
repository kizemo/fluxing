# Lessons Learned 路 缁忛獙鏁欒娌夋穩

> 鑼冨洿锛欶luxing 椤圭洰锛坮ime/weasel fork锛変粠寮€鍙戜簨鏁呬腑鎻愮偧鐨勫彲澶嶇敤鏁欒銆?> 姣忔潯浠?浜嬫晠 鈫?鏍瑰洜 鈫?鏁欒"鏍煎紡璁板綍锛沜ommit `c0951ca` 鏁欒涓烘湰鏂囦欢璧锋簮銆?
---

## L01 路 涓枃 UTF-8 鏂囦欢璇诲啓 鈥?PowerShell 5.1 GBK 浠ｇ爜椤甸櫡闃?
**浜嬫晠**锛歝ommit `c0951ca` 鍖呭惈鐨?8 浠?spec 鏂囨。锛堝惈涓枃 UTF-8锛夊疄闄呬负"GBK 瀛楄妭琚敊璇?Unicode 鍖栧啀 UTF-8 缂栫爜"鐨勬贩鍚堜綋銆俙git hash-object` 楠岃瘉鏄剧ず瀛楄妭 hash 涓€鑷达紙鍥犱负 working 鏂囦欢灏辨槸 commit 鏃跺啓鐨勫瓧鑺傦級锛屼絾涓枃鍐呭宸叉崯鍧忋€?
**鏍瑰洜**锛圥owerShell 5.1 + chcp 936锛夛細

1. `Get-Content -Raw path` 璇?UTF-8 鏂囦欢鏃讹紝**鎸夊綋鍓嶄唬鐮侀〉 (936 = GBK) 瑙ｇ爜 UTF-8 瀛楄妭** 鈫?杩斿洖"GBK 瀛楄妭搴忓垪"瀛楃涓诧紙鍗筹細鍘熷瀛楄妭琚敊璇綋 GBK 鍙屽瓧鑺傚瓧绗﹁В璇诲悗鐨?Unicode 鐮佺偣搴忓垪锛?2. 鍦?PowerShell 瀛楃涓插眰鍋?`$s.Substring(...)` / `-replace` / `+` 绛夋搷浣?鈫?瀛楃涓查噷鏄敊璇爜鐐?3. `[System.IO.File]::WriteAllText(path, $s, [UTF8Encoding]$false)` 鎶?GBK 瀛楄妭搴忓垪"褰?Unicode 鐮佺偣鍐欏叆 鈫?姣忎釜 GBK 瀛楄妭 (0x00-0xFF) 褰?1 涓?Unicode 鐮佺偣 鈫?UTF-8 缂栫爜涓?2 瀛楄妭
4. `Get-Content` 璇诲洖楠岃瘉鏃?*璧板悓鏍风殑 GBK 璺緞** 鈫?鐪嬭捣鏉?鑷唇"锛?*鏈鍙戠幇鎹熷潖**

**涓轰粈涔?`git hash-object` 鏄剧ず涓€鑷?*锛歡it 绠楃殑鏄瓧鑺?hash锛寃orking 鏂囦欢灏辨槸 commit 鏃跺啓鐨勫瓧鑺傦紝hash 褰撶劧涓€鑷?鈥?**浣嗗唴瀹瑰凡鍧?*銆?
**鏁欒锛堝繀椤婚伒瀹堬級**锛?
| 鎿嶄綔 | 鍏佽 | 绂佹 |
|---|---|---|
| 鍐欎腑鏂?UTF-8 | `[System.IO.File]::WriteAllText(path, content, [System.Text.UTF8Encoding]::new($false))` | `Out-File` / `>` / `Set-Content` / `Get-Content \| Set-Content` |
| 璇讳腑鏂?UTF-8 | `[System.IO.File]::ReadAllText(path, [System.Text.Encoding]::UTF8)` 鎴?`ReadAllBytes` + 鏄惧紡 `UTF8.GetString` | `Get-Content` / `cat` / `[IO.File]::ReadAllText(path)` (鏃?encoding) |
| 楠岃瘉 UTF-8 瀹屾暣鎬?| byte-level锛歚[System.IO.File]::ReadAllBytes(path)` 妫€鏌ラ瀛楄妭 0xE0-0xEF銆佸悗缁?0x80-0xBF锛涙垨 `git hash-object` + 涓庡凡楠岃瘉婧愭瘮杈?| `Get-Content` 璇诲洖鍐?echo 鈥?**浼氬啀娆?GBK 鍖?* |
| 涓枃鍐呭鏉ユ簮 | 浼樺厛浠庡璇濅笂涓嬫枃鍙栵紙宸?LLM 澶勭悊涓?Unicode 鐮佺偣锛?| 浠庡凡鎹熷潖鏂囦欢璇诲啀鍐?鈥?**姹℃煋浼犳煋** |

**楠岃瘉鑴氭湰妯℃澘**锛?
```powershell
# 鍐?$content = "涓枃鍐呭"  # 浠庡璇濅笂涓嬫枃
[System.IO.File]::WriteAllText($path, $content, [System.Text.UTF8Encoding]::new($false))

# 楠岃瘉锛坆yte-level锛?$bytes = [System.IO.File]::ReadAllBytes($path)
$first30 = ($bytes[0..29] | ForEach-Object { $_.ToString("X2") }) -join " "
Write-Host "First 30 bytes: $first30"
# 鏈熸湜: 23 20 ... (ASCII 澶? 鎴?E4 ... (涓枃 UTF-8 澶?E0-EF)
# 涓嶆湡鏈? C0 C1 C2 C3 C4 ... (GBK 瀛楄妭琚敊璇?Unicode 鍖栫殑 2 瀛楄妭 UTF-8)
```

**commit 鍓嶈嚜妫€**锛?
```powershell
git add path
git hash-object -w path  # 鍐?blob
git cat-file -p <hash> | git hash-object --stdin  # 楠岃瘉鍙€?```

---

## L02 路 涓枃鍐呭淇敼蹇呴』鐢?byte-level Replace 鈥?閬垮紑 PS 瀛楃涓插眰

**浜嬫晠**锛氬湪 `lessons-learned.md` 璧疯崏鏃讹紝澶氫釜 `Contains()` / `Replace()` 澶辫触杩斿洖 `False`锛屽嵆浣垮瓧绗︿覆瑙嗚涓婂畬鍏ㄧ浉鍚屻€?
**鏍瑰洜**锛歅owerShell 5.1 + chcp 936 涓嬶紝**[char]0x987A 褰㈠紡鐨?Unicode 杞箟鍦?`here-string` 涓細琚敊璇紪鐮?*锛屽鑷?PS 瑙ｆ瀽鍣ㄥ皢瀛楅潰閲忔寜 GBK 瑙ｈ鍚庡啀瀛樹负 Unicode 瀛楃涓?鈥?涓庢枃浠跺疄闄?UTF-8 瀛楄妭瑙ｆ瀽鍚庣殑瀛楃涓蹭笉鍖归厤銆?
**鏁欒**锛?
- **鑳界敤 byte 鎿嶄綔灏辩敤 byte 鎿嶄綔**锛歚[System.IO.File]::ReadAllBytes(path)` + `[System.Text.Encoding]::UTF8.GetBytes(searchStr)` + byte-by-byte 姣旇緝
- **PS 瀛楃涓插尮閰嶄笉鍙潬**锛氬綋鏂囦欢鍚腑鏂囦笖 PS 5.1 鍦?GBK 浠ｇ爜椤垫椂锛?*鎵€鏈?PS 瀛楃涓插眰鎿嶄綔锛坄-match`銆乣-replace`銆乣.Contains()`銆乣.Replace()`銆乣.IndexOf()`锛夐兘鍙兘缁欏嚭閿欒缁撴灉**
- **娴嬭瘯 byte 鍖归厤鏄惁姝ｇ‘**锛氬厛 `Write-Host` 鎷兼帴 byte 鏁扮粍鐨?hex锛屽鐓ф枃浠跺疄闄呭瓧鑺傚簭鍒?
**byte-level replace 妯℃澘**锛?
```powershell
$bytes = [System.IO.File]::ReadAllBytes($path)
$marker = [System.Text.Encoding]::UTF8.GetBytes("瑕佹壘鐨勫瓧鑺傛ā寮?)
$start = -1
for ($i = 0; $i -le $bytes.Length - $marker.Length; $i++) {
    $match = $true
    for ($j = 0; $j -lt $marker.Length; $j++) {
        if ($bytes[$i + $j] -ne $marker[$j]) { $match = $false; break }
    }
    if ($match) { $start = $i; break }
}
# 绫讳技鎵?endMarker
# 鎷兼帴: $bytes[0..start] + newBytes + $bytes[end..end]
[System.IO.File]::WriteAllBytes($path, $combined)
```

---

## L03 路 librime 1.13 key_binder 閰嶇疆 鈥?鍝簺鏄敮鎸佺殑銆佸摢浜涙槸"鍋囬槼鎬?

**浜嬫晠**锛歴pec 005 瀹炴柦鐨?Shift_L 涓婂睆绗?2 鍊欓€? 鍗曟祴 13/13 PASS锛屼絾**瀹為檯杩愯涓嶇敓鏁?*锛堟寜 Shift_L 鐩存帴涓婂睆鑻辨枃锛夈€?
**鏍瑰洜**锛坄librime/src/rime/gear/key_binder.cc` + `ascii_composer.cc` 婧愮爜楠岃瘉锛夛細

1. `key_binder` 鐨?`send:` 瀛楁瑙ｆ瀽涓?`KeyEvent::Parse(target)`锛屽崟瀛楃瀛楅潰閲忥紙濡?`"2"`锛夎蛋 `keycode_ = '2' = 0x32`锛?*鍚堟硶** 鈥?浣嗕粎褰?key_binder **鑳芥敹鍒?*杩欎釜 event 鏃舵墠鐢熸晥
2. `engine.processors_` 椤哄簭鍦?rime_ice schema 鏄?`ascii_composer 鈫?recognizer 鈫?key_binder 鈫?...`
3. `ascii_composer.cc:80-103` 鐪嬪埌 `ch == XK_Shift_L` 鏃?*鏃犳潯浠惰褰?* `shift_key_pressed_=true` 骞?return `kNoop` 鈥?key_binder 鐪嬩笉鍒板崟鐙?Shift_L press event
4. 鏉惧紑 Shift_L 鏃?ascii_composer 璋?`ToggleAsciiModeWithKey(XK_Shift_L)` 鈫?鍥犱负 `Shift_L: commit_code` 瑙﹀彂浜?`SwitchAsciiMode(true, commit_code)` 鈫?涓婂睆缂栫爜 + 鍒囪嫳鏂?5. `key_binder` 鐨?`KeyEvent::operator==` 涓ユ牸姣旇緝 `keycode + modifier (鍚?release mask)` 鈥?`binding.accept: shift+l` 鏄?`(L, Shift)`锛?*涓嶅尮閰?* `Shift_L release event (Shift_L, RELEASE)` 鈥?librime 1.13 key_binder **涓嶅鐞?release event**

**鏁欒**锛?
| 鍋囪 | 瀹為檯 |
|---|---|
| 鍗曟祴 PASS = librime 寮曟搸宸ヤ綔 | **閿?* 鈥?褰撳墠 TestDefaultHotkeys.cpp 鍙祴瀛楃涓插寘鍚紝**涓嶆祴 librime 琛屼负** |
| `send: 2` 绛夋暟瀛?keyevent 璁?key_binder 杞彂"閫夌 2 鍊欓€? | **瀵?*锛坘eycode 0x32 璧?selector:146 `ch >= XK_0 && ch <= XK_9` 璺緞锛夛紝**浣嗗墠鎻愭槸 key_binder 鐪熻兘鏀跺埌 send target 閲嶅畾鍚?event** |
| `accept: shift+l` 鍖归厤"鎸変綇 Shift + 鎸?L" | **瀵?*锛圫hift modifier + L keycode 缁勫悎锛墊
| `accept: shift+l` 鍖归厤"鏉惧紑 Shift_L" | **閿?*锛坮elease event 涓嶅弬涓?binding 鍖归厤锛墊
| `accept: Shift_L` 鍖归厤"鎸変笅鍗曠嫭鐨?Shift_L" | **瀵?*锛坘eycode=Shift_L 鏁板€笺€乵odifier=0锛墊
| `ascii_composer.switch_key.Shift_L: noop` 绛変簬"瀹屽叏灞忚斀 Shift_L" | **閮ㄥ垎瀵?* 鈥?`load_bindings:37-38` 璺宠繃 noop 涓嶅瓨 `bindings_` 鈫?`ToggleAsciiModeWithKey` 杩斿洖 false 鈫?**涓嶅垏鑻辨枃**锛涗絾 ascii_composer 浠嶄細 record `shift_key_pressed_=true` 骞?return kNoop 鈫?key_binder 浠嶈兘鐪嬪埌 Shift_L press event |

**淇 spec 005 rev3**锛堟湰浠撳簱 c0e3f85 鍚?鈫?寰?commit锛夛細

- `ascii_composer.switch_key.Shift_L: noop` + `Shift_R: noop`锛堥槻姝?ascii_composer 鍒囪嫳鏂囷級
- key_binder 鍔?4 涓?binding锛堟悳鐙楁嫾闊抽鏍煎吋瀹癸級锛?  - `accept: Shift_L, send: 2, when: has_menu`锛堝崟 Shift_L 鎸変笅 鈫?閫夌 2 鍊欓€夛級
  - `accept: Shift_R, send: 3, when: has_menu`锛堝崟 Shift_R 鎸変笅 鈫?閫夌 3 鍊欓€夛級
  - `accept: shift+l, send: 2, when: has_menu`锛堢粍鍚堥敭 Shift+L 鈫?閫夌 2 鍊欓€夛級
  - `accept: shift+r, send: 3, when: has_menu`锛堢粍鍚堥敭 Shift+R 鈫?閫夌 3 鍊欓€夛級
  - `accept: shift+l, toggle: ascii_mode, when: always`锛堟棤鍊欓€夋椂 Shift+L 鍒囦腑鑻憋級
  - `accept: shift+r, toggle: ascii_mode, when: always`锛堟棤鍊欓€夋椂 Shift+R 鍒囦腑鑻憋級
  - `accept: Shift_L, toggle: ascii_mode, when: always`锛堟棤鍊欓€夋椂鍗?Shift_L 鍒囦腑鑻憋級
  - `accept: Shift_R, toggle: ascii_mode, when: always`锛堟棤鍊欓€夋椂鍗?Shift_R 鍒囦腑鑻憋級

**鏈潵娴嬭瘯鏀硅繘**锛氬崟娴嬪繀椤?*瀹炰緥鍖?librime engine + 鍔犺浇 yaml + 妯℃嫙 KeyEvent** 鈥?鑰屼笉鏄彧娴?yaml 瀛楃涓插寘鍚€傝繖闇€瑕?C++ 鍗曟祴妗嗘灦 + rime_api.h integration锛屼及 4-6 灏忔椂宸ヤ綔閲忋€?
---

## L04 路 librime 1.13 key_binder 鏀寔鐨?action 绫诲瀷

**浜嬪疄**锛坄librime/src/rime/gear/key_binder.cc:185-220` 婧愮爜楠岃瘉锛夛細

key_binder binding 瀛楁鏀寔 4 绫?action锛?
| 瀛楁 | 浣滅敤 | 渚嬪瓙 |
|---|---|---|
| `send: <KeyEvent>` | 鎶?KeyEvent 娉ㄥ叆 engine 浜嬩欢娴?| `send: Page_Up` / `send: 2` |
| `send_sequence: <KeySeq>` | 澶氭寜閿簭鍒?| `send_sequence: "ctrl+a"` |
| `toggle: <option>` | 鍒囨崲 option 鐘舵€?| `toggle: ascii_mode` / `toggle: ascii_punct` / `toggle: traditionalization` |
| `set_option: <option>` / `unset_option: <option>` | 寮哄埗 set/unset | `set_option: simplification` |
| `select: <schema>` | 鍒囨崲 schema | `select: .next` |

**涓嶆敮鎸?*锛?
- 鐩存帴璋冪敤 `Selector::SelectCandidateAt(ctx, N)` 鈥?selector 涓嶆毚闇茬粰 key_binder
- 鑷畾涔?lua callback
- release event binding
- mouse event binding锛坢ouse 鐢?WeaselUI 澶勭悊锛屼笉杩?RIME engine锛?
**闂存帴瀹炵幇"鎸夋暟瀛楅€夊€欓€?**锛?
- 鏁板瓧 0-9 key event 璧?`Selector:146`锛歚ch >= XK_0 && ch <= XK_9` 鈫?`index = ((ch - XK_0) + 9) % 10` 鈫?`SelectCandidateAt(ctx, index)`
- 鍗?`1`鈫掔 1 鍊欓€? `2`鈫掔 2 鍊欓€? `9`鈫掔 9 鍊欓€? `0`鈫掔 10 鍊欓€?- binding `send: 2` 閲嶅畾鍚戞寜鏁板瓧 2 鍗冲彲

---

## L05 路 Commit 鍓嶇殑鏈€灏忛獙璇佹竻鍗?
**鏍囧噯 5 姝ラ獙璇?*锛堟瘡娆?commit 鍓嶅繀鍋氾級锛?
1. **byte-level UTF-8 楠岃瘉**锛堜腑鏂囨枃浠讹級锛?   ```powershell
   $bytes = [System.IO.File]::ReadAllBytes($path)
   "First 30 bytes: " + ($bytes[0..29] | ForEach-Object { $_.ToString("X2") }) -join " "
   ```
   - ASCII 澶? 鏈熸湜 `0x20-0x7E` 鑼冨洿
   - 涓枃 UTF-8: 鏈熸湜 `0xE0-0xEF` 璧峰 + `0x80-0xBF` 鍚庣画
   - GBK 姹℃煋淇″彿: 鏈熸湜**娌℃湁** `0xC0/0xC1`锛圲TF-8 姘镐笉鍚堟硶瀛楄妭锛?
2. **git 瀛楄妭 hash 涓€鑷存€?*锛?   ```bash
   git add <files>
   git ls-files -s <path>          # 鍙栧嚭 staging blob hash
   # 鍐欎竴涓复鏃舵枃浠? 鎶?staging blob 鍊掑嚭鏉? 鍐?hash
   git cat-file -p <hash> > /tmp/check.txt
   git hash-object /tmp/check.txt  # 閲嶆柊绠?hash
   ```
   - 鍊掑嚭鏉ラ噸 hash 搴斾竴鑷达紙round-trip test锛?
3. **鍗曟祴 PASS**锛堟洿鏂拌繃鐨勫崟娴嬪繀椤诲叏閮?PASS锛?*鍖呮嫭鏂板姞鐨?case**锛夛細
   ```bash
   cd test/TestDefaultHotkeys
   ./TestDefaultHotkeys.exe ../../output/data/default.yaml
   ```
   - **0 failures 鎵嶆槸鐪?PASS**锛堜笉鑳?skip 鍑犱釜 case"锛?
4. **diff 瑙嗚妫€鏌?*锛堜腑鏂?commit message / 涓枃鏂囨。锛夛細
   - `git diff --cached` 鐪嬫槸鍚︽湁"涔辩爜"锛堝 `閻?閺傝 妞擿锛?鈥?杩欏氨鏄?GBK 姹℃煋淇″彿

5. **commit message 绠€娲佹€?*锛欳onventional Commits 鏍煎紡 `feat(scope): ...` / `fix(scope): ...` / `docs(spec): ...`

---

## L06 路 GitHub MCP 鍦?Codex 涓殑涓嶅彲鐢ㄥ満鏅?
**浜嬪疄**锛?
- `mcp__github__*` 宸ュ叿闇€瑕?host MCP server 鍦?`~/.codex/config.toml` 鐨?`[mcp_servers]` 鑺傛敞鍐?- 涓€娆?Codex 浼氳瘽**涓嶄細鑷姩缁ф壙**鍙︿竴娆′細璇濈殑 MCP 閰嶇疆
- 褰?host 閰嶇疆缂哄け鏃讹紝`mcp__github__*` 璋冪敤杩斿洖 `unsupported call`锛堜笉鏄?鏉冮檺涓嶈冻"锛屾槸"宸ュ叿鏈敞鍐?锛?
**缁曢亾鏂规**锛?
- 缁欑敤鎴?*棰勫厛鍐欏ソ** GitHub About / Description / Topics 鏂囨湰锛岀敤鎴锋墜鍔ㄧ矘璐?- 鐢?`mcp__playwright__browser_navigate` 鎵撳紑 GitHub 缃戦〉锛坧laywright 宸ュ叿**鏄?*鍦?Codex desktop app 鍐呯疆鐨勶級鈥?浣嗕粎閫傜敤浜庣櫥褰曞悗鐨勬祻瑙堝櫒浼氳瘽
- 璧?GitHub API锛堢洿鎺?`Invoke-RestMethod` + PAT token锛夆€?涓嶈蛋 MCP

**鏀硅繘寤鸿**锛氬湪 project-level `AGENTS.md` 涓槑纭?鍦ㄦ瘡娆?Codex 浼氳瘽寮€濮嬫椂锛屽厛楠岃瘉 `mcp__github__search_repositories` 鏄惁杩斿洖 `unsupported call`锛涜嫢鏄紝鎻愮ず鐢ㄦ埛閲嶆柊閰嶇疆 MCP"

---

## L07 路 `Out-File -Encoding utf8` 涓?`[UTF8Encoding]::new($false)` 鐨勫尯鍒?
**浜嬪疄**锛?
- `Out-File -Encoding utf8` 鍐?**UTF-8 WITH BOM** (3 bytes EF BB BF 鍓嶇紑)
- `Out-File -Encoding utf8BOM` 鍚屾牱 BOM
- `Out-File -Encoding utf8NoBOM` 鍐?UTF-8 NO BOM
- `[System.IO.File]::WriteAllText(path, content, [System.Text.UTF8Encoding]::new($false))` 鍐?UTF-8 NO BOM
- `[System.IO.File]::WriteAllText(path, content, [System.Text.Encoding]::UTF8)` 鍐?UTF-8 WITH BOM锛堥粯璁わ級

**浣跨敤瑙勮寖**锛?
- RIME yaml / spec 鏂囨。 / 鎴戜滑椤圭洰鐨勬墍鏈夋枃鏈枃浠?鈫?**UTF-8 NO BOM**
- 宸ュ叿锛氱敤 `[System.IO.File]::WriteAllText(path, content, [System.Text.UTF8Encoding]::new($false))`
- **涓嶈鐢?* `Out-File -Encoding utf8`锛堜細姹℃煋 BOM锛?---

## L08 路 GitHub API PATCH repository endpoint 鐨勪腑鏂囧鐞?quirk

**浜嬫晠**锛氱敤 GitHub REST API PATCH /repos/{owner}/{repo} 淇敼 description 瀛楁鏃讹紝鍏ㄤ腑鏂?description 琚浛鎹负闂彿锛孴opics 涔熻涓㈠純锛坱opics 蹇呴』鐢?/repos/{owner}/{repo}/topics 绔偣锛夈€?
**鏍瑰洜**锛?
- GitHub API PATCH /repos/{owner}/{repo} 绔偣鍦ㄦ煇浜涘満鏅笅浼氭妸鍏ㄩ潪 ASCII 鎻忚堪閲岀殑涓枃瀛楃鏇挎崲涓洪棶鍙?鈥?宸茬煡琛屼负锛屼笉鏄瓧绗︾紪鐮侀棶棰橈紙request body 鏄共鍑€ UTF-8锛?- 淇锛氭妸 description 鍐欐垚鑻辨枃涓轰富 + 涓枃鎷彿鐨勫舰寮忥紝GitHub 鏈嶅姟绔細淇濈暀浣滀负鏁翠綋鐨?description 鍐呯殑闈?ASCII 鐗囨
- Topics 瀛楁鍦?PATCH /repos/{owner}/{repo} 绔偣涓嶄細淇敼锛堝嵆浣?body 閲屽啓 topics:[...] 涔熻蹇界暐锛?- 蹇呴』鐢ㄧ嫭绔嬬鐐?PUT /repos/{owner}/{repo}/topics + accept 澶?application/vnd.github.mercy-preview+json + body {"names":[...]}

**鏁欒**锛?
- GitHub About 鏀?Description 鏃讹細鍏ㄨ嫳鏂囨垨鑻辨枃涓轰富 + 涓枃鎷彿锛屼笉瑕佺敤鍏ㄤ腑鏂?description
- Topics 蹇呴』鐢ㄧ嫭绔?/topics 绔偣 (PUT)锛屼笉鏄?/repos/{owner}/{repo} 鐨?PATCH 閲岀殑 topics 瀛楁
- 楠岃瘉锛氭敼瀹屽悗 GET 浠撳簱淇℃伅锛宐yte-level 妫€鏌?description UTF-8 瀛楄妭搴忓垪鏄惁瀹屾暣 (0xE0-0xEF 璧峰锛屾棤 0x3F 鏇夸唬)
- 涓嶈兘鐢?Invoke-RestMethod | Select-Object 楠岃瘉 鈥?PS 5.1 GBK 鍖栦細鎶婁腑鏂囨樉绀烘垚涔辩爜锛岃鍒?API 澶辫触

## L09 - NSIS install.nsi: BOM + OutFile hard-coded + line endings

**Incident**: Building fluxing-0.18.1.0-installer.exe failed with:

`
makensis.exe : Bad text encoding: output\install.nsi:69
`

After fixing, the installer built and copied to rchives/fluxing-0.18.0.0-installer.exe (overwriting the previous release silently) instead of luxing-0.18.1.0-installer.exe.

**Root causes** (3 distinct NSIS pitfalls discovered simultaneously):

1. **NSIS Unicode true requires UTF-8 BOM**. Without BOM, NSIS decodes bytes as ANSI (system codepage). When the script contains non-ASCII bytes (Chinese, emoji), NSIS errors out on the first non-ASCII line. PowerShell's [System.IO.File]::ReadAllBytes + WriteAllBytes (byte-level) does NOT add BOM; must prepend  xEF 0xBB 0xBF manually after byte-level edits.

2. **OutFile was hard-coded**: OutFile "archives\fluxing-0.18.0.0-installer.exe". Every build silently overwrote the previous release file at the same path. Fix: OutFile "archives\fluxing-\.\-installer.exe".

3. **NSIS tolerates lone CR ( x0D without  x0A) but should be CRLF**. Byte-level LF鈫扖RLF conversion (PowerShell) must check ytes[i] == 0x0A { prepend 0x0D } BEFORE appending  x0A. Reversing the order produces  x0A 0x0D (LF-CR, Mac classic) which is technically valid NSIS line ending but inconsistent.

**Lesson**:
- Before NSIS build: verify output/install.nsi has BOM (ytes[0..2] == EF BB BF), 100% CRLF (CRLF count == LF count + 1 for BOM-less file, == for BOM file), no  xC0/0xC1 overlong bytes.
- OutFile must always use NSIS variable interpolation: \.\.
- Installer artifact copy: NSIS doesn't copy to elease/; xbuild.bat/uild.bat also don't. Add a manual Copy-Item output/archives/<name> release/<name> step at the end of any release build.
- Verification: after build, Test-Path release/fluxing-\.0-installer.exe and Get-Item ... | Length (44 MB order of magnitude).

**NSIS install-path bug for user data**: \ is reset to \\weasel (${WEASEL_ROOT}) inside the install section (line 217 of upstream weasel install.nsi). Any reference to \ after that point gives the *engine* install path, not the user-visible root. To use the user-visible root, save it BEFORE the reset: StrCpy \ "\" then StrCpy \ "\".

**Avoid WeaselSetup /userdir:<path>** for paths that end in user1 etc. 鈥?WeaselSetup.cpp::Run() does EnsureFluxingUserDataSuffix on the path and appends \fluxing if the last segment isn't luxing. So /userdir:foo\user1 becomes oo\user1\fluxing in the registry. If you need the exact path, write the registry key directly from NSIS: WriteRegStr HKCU "Software\Fluxing\Weasel" "RimeUserDir" "<path>" (and pre-create the dir with CreateDirectory).
---

## L10 - librime-lua integration: cmake plugin auto-discovery, MSBuild import lib quirk, and Win32-only librime

**Symptom**: After upgrading from 0.18.1.0 to a build with rime_ice schema, typing Chinese produced no candidates. WeaselServer log showed: error creating processor/translator/filter: 'lua_*'. The rime_ice schema is heavily lua-dependent (6 lua_translator, 6 lua_filter, 1 lua_processor); without lua it silently degrades.

**Root cause** (3 layered issues discovered while fixing):

1. **librime is a git submodule; cmake build auto-discovers librime/plugins/* for plugin DLLs**. If no plugin is present, rime builds without lua_processor/lua_translator/lua_filter symbols. rime.dll ends up 2.3 MB (no lua) instead of 3.0 MB (with lua).
   - **Fix**: vendor hchunhui/librime-lua at 	hirdparty/librime-lua/, then on every uild.bat rime, run scripts/prepare-librime-lua.bat to copy 	hirdparty/librime-lua/ to librime/plugins/lua/. CMake's dd_subdirectory(plugins) finds the plugin, links it static into rime.dll.

2. **MSBuild's incremental link does NOT regenerate rime.lib when the link re-executes but the dll's exported-symbol set is *perceived* as unchanged**. Symptoms:
   - dist_x64/lib/rime.lib keeps old mtime (10:53) even after ime.vcxproj re-links and produces a fresh 3.0 MB dist_x64/lib/rime.dll (mtime 11:11).
   - When the Weasel xmake build then links lib64/rime.lib, it fails with LNK2001: unresolved external symbol rime_get_api or similar (because the .lib is the 2.3 MB-era file, no lua symbols; or worse, the .lib is the empty 1496-byte MSBuild stub that MSBuild writes when it skips import-lib generation).
   - **Fix**: in :build_librime_platform, after cmake --build build --target install + stash_build push, do a manual copy /Y librime\build_%1\src\Release\rime.lib librime\dist_%1\lib\rime.lib. The cmake install(TARGETS rime) for SHARED library on Windows is **not** reliable for the import .lib (it does install the .dll, but .lib is sometimes skipped with "Up-to-date: rime.lib" even when the dll was re-linked).
   - **Alternative verification**: dumpbin /EXPORTS librime\build\src\Release\rime.dll | findstr luaL_newstate luaL_openlibs should print both symbols. If yes, lua is linked in; you can rebuild the .lib by running msbuild librime\build\src\rime.vcxproj /t:Rebuild /p:Configuration=Release /p:Platform=Win32 (the rime.vcxproj only has Release|Win32 config after cmake configure with -AWin32).

3. **The project is Win32-only even though installer copies output\rime.dll for x64 OS**. xmake build runs both xmake f -a x64 and xmake f -a x86. The x64 build links lib64/rime.lib (32-bit, because librime is built with -AWin32) and fails with LNK1104 rime_get_api (machine-type mismatch). The x86 build succeeds.
   - **Why this is OK in production**: rime.dll is 32-bit. WeaselServer.exe is 32-bit. The installer copies output\rime.dll (32-bit) which works on x64 OS via WoW64. The historical lib64\rime.lib was 64-bit and let xmake x64 build "succeed" (exit 0) by linking against the wrong-machine lib (which the linker then can't use, producing no usable x64 WeaselServer.exe — the existing output\WeaselServer.exe is leftover from a much older native-64-bit build).
   - **Fix in xbuild.bat**: skip the xmake x64 step. Add a comment explaining the constraint so the next agent doesn't try to "fix" it by adding an x64 librime build. A true x64 build is a separate task that requires switching librime\env.bat set ARCH=x64 (submodule change, needs its own PR).

4. **NSIS MUI_ICON path is resolved relative to cwd, not install.nsi**. output\install.nsi has !define MUI_ICON ..\resource\weasel.ico. When xbuild.bat invokes makensis from WEASEL_ROOT (project root), NSIS looks for ..\resource\weasel.ico (i.e. F:\soft\resource\weasel.ico) which doesn't exist. NSIS errors: can't open file then Error in macro MUI_INTERFACE on macroline 87 then Error in script "output\install.nsi" on line 51 -- aborting.
   - **Fix**: xbuild.bat cd /d %WEASEL_ROOT%\output before invoking makensis. Then ..\resource\weasel.ico resolves to esource\weasel.ico from the project root. Use the bare install.nsi argument (not output\install.nsi) since cwd is now output/.

5. **NSIS /D=path /userdir=otherpath silent install: NSIS concatenates all unknown CLI args into **. The /userdir= switch is NOT a standard NSIS option. When passed, NSIS treats it as a path fragment and the result is something like C:\TEMP\Fluxing userdir=C:\TEMP\UserData\fluxing\user1\fluxing written to HKCU\Software\Fluxing\Weasel\RimeUserDir.
   - **Fix**: silent install users should only pass /S and /D=path. The user-data path is forced internally by the NSIS script via WriteRegStr HKCU "Software\Fluxing\Weasel" "RimeUserDir" "\fluxing\user1\fluxing" (this is the 0.18.1.0 fix that bypasses WeaselSetup.exe /userdir: and its EnsureFluxingUserDataSuffix mangling).

6. **env.bat must pin FLUXING_VERSION=0.18.2 + RELEASE_BUILD=1 for installer to use the right name**. Without RELEASE_BUILD=1, uild.bat falls through to a git tag --sort=-creatordate lookup + git rev-list for the commit count, producing PRODUCT_VERSION=0.17.4.57.4506a32 (where 57 is commit count, 4506a32 is the short hash). The installer file is then named luxing-0.17.4.57-installer.exe and silently overwrites the previous release (the L09 OutFile-interpolation fix is still in effect, but the interpolated value is wrong).
   - **Fix**: env.bat template should default to RELEASE_BUILD=1 and FLUXING_VERSION=0.18.2. For non-release builds (CI, dev), comment them out to opt into the git-hash suffix.

**Verification checklist before declaring librime-lua integration done**:
- dumpbin /EXPORTS librime\dist_x64\lib\rime.dll | findstr luaL_newstate → must show the symbol.
- ime_deployer --build <user_dir> <shared_dir> <staging_dir> → must produce ime_ice.table.bin ≈ 60 MB (vs 0 bytes / error when lua is missing).
- xmake f -a x86 -m release && xmake → linking.release WeaselServer.exe must succeed; output\Win32\WeaselServer.exe must be 32-bit (machine 14C).
- Silent install to fresh C:\TEMP\fluxing-0182-test\Fluxing:
  - HKLM\SOFTWARE\WOW6432Node\Fluxing\Weasel\InstallDir = C:\TEMP\fluxing-0182-test\Fluxing
  - HKCU\Software\Fluxing\Weasel\RimeUserDir = C:\TEMP\fluxing-0182-test\Fluxing\fluxing\user1\fluxing
  - output\weasel\data\build\rime_ice.table.bin ≈ 60 MB

**Files changed** (commit 60e04ab):
- uild.bat (+6 lines): prepare-librime-lua.bat hook + manual rime.lib copy from uild\src\Release\
- xbuild.bat (+5 lines): skip xmake x64 + cd output before makensis
- env.bat (rewritten): pin FLUXING_VERSION + RELEASE_BUILD for installer naming
- scripts/prepare-librime-lua.bat (new, 36 lines): idempotent copy of vendored librime-lua
- scripts/fetch-librime-lua.bat (new, 22 lines): one-shot git clone for refreshing vendored source
- 	hirdparty/librime-lua/ (new, 1.06 MB, 100 files): hchunhui/librime-lua vendored source
- elease/fluxing-0.18.2.0-installer.exe (new, 43.8 MB): built and silent-install tested