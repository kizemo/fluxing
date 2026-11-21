# 批次 BB · B-5 孤儿 IPC 会话回收「解析层」单测（ADR 0067）

> 目标：闭环重启清单 §B-5「**旧客户端不写 `session.client_pid=` ⇒ 服务端不得回收、
> 更不得误杀**」中此前**零覆盖**的那一层 —— **解析层**。
>
> 性质：**纯测试批次 + 一次等价重构**。未改 `librime/`、未改 IPC 协议消息类型、
> 未新增第三方依赖、未改任何用户可见中文文案、未 commit、未 push。

---

## 0. 一句话

把「从握手文本里解析客户端自报 pid」这件事从 `RimeWithWeasel.cpp` 的内联实现抽成
共享纯函数 `fluxing::session_reclaim::ParseClientPid`（`include/FluxingSessionReclaim.h`），
并新增单测 `TestClientPidParse`（19 条断言）覆盖「旧客户端握手里没有这一行 ⇒ 解析结果
确实是未知(0) ⇒ `SelectReclaimableSessionIds` 不选中它」。

---

## 1. 缺口（改前状态）

| 层 | 覆盖情况（改前） |
|---|---|
| 判据层 | `TestSessionReclaimCriteria()` 已覆盖「pid 未知 ⇒ 永不回收」「混合表只回收已死且 pid 已知那条」 |
| 日志行 | `TestSessionReclaimLogLine()` 已覆盖 B-2 的 `reclaim orphan session ipc_id=… client_pid=… live=…` 逐字格式 |
| **解析层** | **零覆盖** —— 没有任何用例证明「握手里没有 `session.client_pid=` 这一行时，解析结果真的是 0（未知）」 |

风险形态：解析层一旦把「缺这一行」错当成某个数（例如 0 被当成「可探测的 pid」或
被回绕成别的数），判据层再正确也保不住「不误杀旧客户端」这条底线。

---

## 2. 改动清单（**只动了这 3 个文件**）

### 2.1 `include/FluxingSessionReclaim.h`（共享头，**必须全 ASCII**）

| 位置 | 改动 |
|---|---|
| `:35` | 新增 `#include <algorithm>`（新函数用 `std::equal` 做 key 前缀比对） |
| `:67`–`:135` | **新增一个**共享纯函数 `ParseClientPid`（下方全文） |

```cpp
inline bool ParseClientPid(const std::wstring& body,
                            unsigned long* out_pid) {
  static const wchar_t kKey[] = L"session.client_pid=";
  const size_t kKeyLen = sizeof(kKey) / sizeof(kKey[0]) - 1;
  bool found = false;
  unsigned long last_valid = 0;
  size_t pos = 0;
  while (pos <= body.size()) {
    size_t eol = body.find(L'\n', pos);
    const bool last_line = (eol == std::wstring::npos);
    if (last_line)
      eol = body.size();
    const std::wstring line = body.substr(pos, eol - pos);
    pos = eol + 1;
    if (line.size() >= kKeyLen &&
        std::equal(kKey, kKey + kKeyLen, line.begin())) {
      const std::wstring value = line.substr(kKeyLen);
      unsigned long parsed = 0;
      bool valid = !value.empty();
      for (wchar_t ch : value) {
        if (ch < L'0' || ch > L'9') { valid = false; break; }
        // Accumulate in 64 bits so the DWORD range check below is live:
        // 'unsigned long' is 32 bit on Windows, so accumulating there
        // would wrap around instead of rejecting an out of range value.
        const unsigned long long next =
            static_cast<unsigned long long>(parsed) * 10ull +
            static_cast<unsigned long long>(ch - L'0');
        if (next > 0xFFFFFFFFull) { valid = false; break; }   // out of DWORD range
        parsed = static_cast<unsigned long>(next);
      }
      if (valid) { last_valid = parsed; found = true; }
    }
    if (last_line) break;
  }
  if (found && out_pid) *out_pid = last_valid;
  return found;
}
```

语义约定（与头文件既有安全条款一致）：
- 返回 `true` = 握手里**至少有一行合法** pid ⇒ `*out_pid` = **最后一个合法值**；
- 返回 `false` = 该行**缺失**或**每一行都非法** ⇒ **`*out_pid` 原样不动**（调用方保持「pid 未知」）；
- 绝不猜数：非空纯 ASCII 十进制、且必须落在 DWORD 范围内，否则判非法。

### 2.2 `RimeWithWeasel/RimeWithWeasel.cpp`（生产侧，**单点调用**）

| 位置 | 改动 |
|---|---|
| `:1093`–`:1095` | 新增局部变量 `std::wstring handshake_body;`（握手文本逐行原样收集） |
| `:1106`–`:1107` | 循环内每行补回一个 `L'\n'` 收进 `handshake_body`（**收集范围与改前逐行扫描完全一致：到 `"."` 行为止**） |
| `:1113`–`:1123` | 删掉改前的内联解析块（原 `:1109`–`:1129`），换成**一次**调用：<br>`fluxing::session_reclaim::ParseClientPid(handshake_body, &parsed_pid); owner_pid = static_cast<DWORD>(parsed_pid);` |

`session.client_app=` 的解析、`app_name`、会话状态设置等**一行未动**。
`owner_pid` 初值仍是 0，解析失败时 `parsed_pid` 保持 0 ⇒ 与改前「非法就不改写」的写法同值。

### 2.3 `test/TestFluxingCloud/TestFluxingCloud.cpp`（单测）

| 位置 | 改动 |
|---|---|
| `:3565`–`:3745` | **新增用例 `TestClientPidParse()`**（19 条断言，见 §4.2） |
| `:3749`–`:3751` | `main()` 里新增调用 `TestClientPidParse();`（紧随 `TestSessionReclaimLogLine()`） |

覆盖矩阵：

| # | 场景 | 断言 |
|---|---|---|
| ① | 旧客户端握手（**没有** `session.client_pid=` 行） | 解析不出来 |
| ① | 空握手文本 | 解析不出来 |
| ② | 解析失败时出参 | **原样不动**（调用方保持未知） |
| ③ | 旧客户端的 pid | 保持 0，且 `OwnerPidKnown` 为假 |
| ④ | `session.client_pid=`（空值） | 非法 |
| ⑤ | 非纯十进制 9 值（`abc` / `12x` / 前导空格 / 尾随空格 / `-1` / `1 2` / `0x10` / `+7` / 全角 `１２３`） | **一律非法**（`CheckMany`，9 项） |
| ⑥ | 纯十进制 `4321` | 解析出 4321 |
| ⑥ | `0` | 解析出 0，且判据层仍判「未知 pid（不可探测）」 |
| ⑥ | `4294967295` | 合法上界 |
| ⑦ | `4294967296` / `99999999999` | **非法**（> 0xFFFFFFFF） |
| ⑧ | 多个 pid 行 | 最后一个**合法**值生效；后一个非法 ⇒ 保留前一个；前一个非法 ⇒ 取后一个 |
| ⑨ | key 不在行首 | 不认 |
| ⑩ | **关键**：旧客户端会话（pid 未知、探测说「已死」） | `SelectReclaimableSessionIds` **不选中**（绝不误杀） |
| ⑩ | **关键**：混表（旧客户端 + pid 已知且已死） | **只**回收「pid 已知且已死」那条，旧客户端那条不动 |

---

## 3. 「语义与改前等价」是怎么核对的

### 3.1 逐条规则对照（改前内联实现 vs 改后共享函数）

| 输入 | 改前（内联） | 改后（共享函数） | 等价 |
|---|---|---|---|
| 没有 `session.client_pid=` 行（旧客户端） | `owner_pid` 保持 0 | 返回 false、出参不动 ⇒ 调用方 0 | ✅ |
| 空值 `session.client_pid=` | `valid=false` ⇒ 0 | 返回 false ⇒ 0 | ✅ |
| `session.client_pid=0` | 合法 ⇒ 0（判据层仍算未知） | 返回 true、出参 0 | ✅（取值同） |
| 非十进制（`abc`/`12x`/空格/`-1`/`+7`/`0x10`/全角） | 非法 ⇒ 0 | 非法 ⇒ 0 | ✅ |
| 纯十进制 `4321` | 4321 | 4321 | ✅ |
| `0xFFFFFFFF` | 4294967295 | 4294967295 | ✅ |
| **> 0xFFFFFFFF**（如 `99999999999`） | **32 位 `unsigned long` 回绕 ⇒ 1215752191** | **拒绝 ⇒ 0（未知）** | ❌ **唯一分歧，见 §3.3** |
| 多个 pid 行 | 最后一个**合法**值生效（非法行不改写） | 同 | ✅ |
| key 不在行首 | 不认 | 不认 | ✅ |
| CRLF 结尾（`4321\r`） | value 含 `\r` ⇒ 非法 ⇒ 0 | 同 | ✅ |
| `"."` 行之后的内容 | 扫描在 `"."` 处停止 ⇒ 不认 | body 只收集到 `"."` ⇒ 不认 | ✅ |
| 空 body / 无 `\n` 的末行 | 逐行扫描同样结果 | 同 | ✅ |

### 3.2 差分实测（仓库外的一次性夹具）

在 `%TEMP%\fluxing-b5-diff\diff_parse.cpp` 里把**改前的内联实现逐字抄一份**
（连同它原先的「逐行扫描 + `.` 终止」口径），与**改后的共享头函数**对同一批
27 条握手语料逐条对拍（`cl.exe` 编译，x86）：

```
DIFF  corpus[08]  old=1215752191  new=0
SAME=26 DIFF=1 TOTAL=27
WRAP_PROOF old(4294967296)=0 new(4294967296)=0
```

⇒ **27 条里 26 条逐值相同**，唯一不同的一条就是 §3.3 那一类。
（夹具在仓库外，未入库；`corpus[08]` = `session.client_pid=99999999999`。）

### 3.3 唯一的一处分歧（**故意，且方向是安全的**）

改前的内联代码：

```cpp
parsed = parsed * 10 + static_cast<unsigned long>(ch - L'0');
if (parsed > 0xFFFFFFFFul) {  // 超出 DWORD 范围 => 非法
  valid = false;
  break;
}
```

**MSVC/Windows 上 `unsigned long` 恒为 32 位**（x86/x64 都一样）⇒ `parsed > 0xFFFFFFFFul`
**恒为假** ⇒ 这条「超出 DWORD 范围 ⇒ 非法」的检查在 Windows 上是**死代码**，
超范围值会**回绕**成一个看起来合法的数。实测：`99999999999` 回绕成 `1215752191`
⇒ 改前会把它当作**已知 pid** 去探测 ⇒ 若该 pid 不存在，这条**活会话会被回收**（误杀方向）。

改后累加器用 `unsigned long long`，范围检查**真正生效** ⇒ 超范围值判非法 ⇒ pid 未知 ⇒
永不回收 ⇒ 只泄漏、不误杀，正落在本头文件自己写明的安全条款上：

> The only allowed failure direction is a LEAK (an orphan session that is not reclaimed).

取舍说明：
- 任务书把「`> 0xFFFFFFFF` ⇒ 非法」写进了**函数规格**与**用例清单**两处；
- 回绕方向是**误杀**方向，与 ADR 0067 的安全条款相反；
- 触发条件是客户端上报 **>10 位**十进制 pid，正常客户端
  （`std::to_wstring(GetCurrentProcessId())`）**永远产生不出来** ⇒ 现实路径零影响。

> 若上游坚持「连回绕也逐字照抄」，只需把累加器改回 `unsigned long` 并删掉 §4.2 的两条
> 超范围断言；但那样「`> 0xFFFFFFFF` ⇒ 非法」这条验收项就不成立，且把失败方向换成误杀。
> **本批按任务书口径实现（拒绝）**，此处显式登记以免被当成「悄悄改语义」。

### 3.4 生产侧「收集 body 后单点调用」是否等价

改前：循环里逐行判断 `starts_with(line, "session.client_pid=")`；
改后：循环里**逐字**把每行收进 `handshake_body`（补回 `'\n'`），循环外一次性交给共享函数按 `'\n'` 切分。

- 切分结果与改前的 `std::getline(bs, line)` **逐行相同**（getline 也是按 `'\n'` 切、保留 `\r`）；
- 收集范围与改前扫描范围**同边界**（都在 `"."` 行 `break`）；
- 多个 pid 行的「最后一个合法值生效」口径改前改后一致（§3.1 已逐条比对 + §3.2 差分实测）。

---

## 4. 门禁（**全部实跑**，原始数字如下）

### 4.1 构建 `TestFluxingCloud`（**不在 `weasel.sln` 里 ⇒ 必须显式构建**）

```
& "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe" `
  "F:\soft\00selfmade\rime_claude\test\TestFluxingCloud\TestFluxingCloud.vcxproj" `
  /t:Build /p:Configuration=Release /p:Platform=Win32 /p:SolutionDir=F:\soft\00selfmade\rime_claude\
```

结果：**`MSBUILD_EXIT=0`**，**错误 0**，**警告 3**（均为既有 `C4996 'fopen'`，
在 `TestFluxingCloud.cpp:3164/3175/3215` 的旧用例代码里，**与本批无关**）。

**假绿自检（exe mtime ≥ 所有被改源文件 mtime）**：

```
EXE  : F:\soft\00selfmade\rime_claude\Release\TestFluxingCloud.exe  mtime=2026-11-21 09:08:07.732
FluxingSessionReclaim.h    mtime=2026-11-21 09:06:31.612  src_le_exe=True
TestFluxingCloud.cpp       mtime=2026-11-21 09:07:39.928  src_le_exe=True
```

⇒ **非假绿**（改动确实被编译进 exe）。

### 4.2 跑测试（cwd = 仓库根 `F:\soft\00selfmade\rime_claude`，exe 在仓库根 `Release\`）

```
RESULT: PASS=611 FAIL=0
VERDICT: ALL PASS
EXE_EXIT=0
```

- **基线 592 → 611（+19）**，**FAIL 仍为 0**；
- +19 = 18 条 `Check` + 1 条 `CheckMany`（内含 9 个坏值）= 本批新增用例 `TestClientPidParse`；
- 新用例输出段（实跑原文）：

```
[ADR 0067 / B-5] 握手里的 client pid 解析
  [PASS] B-5：旧客户端（无 client_pid 行）⇒ 解析不出来
  [PASS] B-5：空握手文本 ⇒ 解析不出来
  [PASS] B-5：解析失败 ⇒ 不改写出参（调用方保持 pid 未知）
  [PASS] B-5：旧客户端 ⇒ pid 保持 0（未知）
  [PASS] B-5：旧客户端 ⇒ pid 未知 ⇒ 不可探测
  [PASS] B-5：空值 ⇒ 非法
  [PASS] B-5：非纯十进制值（abc/12x/空格/-1/0x10/+7/全角…）⇒ 一律非法（9 项）
  [PASS] B-5：纯十进制 ⇒ 解析出 4321
  [PASS] B-5：0 ⇒ 解析出 0
  [PASS] B-5：0 ⇒ 判据层仍判「未知 pid」（不可探测）
  [PASS] B-5：0xFFFFFFFF ⇒ 合法上界
  [PASS] B-5：> 0xFFFFFFFF ⇒ 非法
  [PASS] B-5：远超 DWORD 范围 ⇒ 非法
  [PASS] B-5：两个合法 pid 行 ⇒ 取最后一个
  [PASS] B-5：后一个非法 ⇒ 保留前一个合法值
  [PASS] B-5：前一个非法 ⇒ 取后一个合法值
  [PASS] B-5：key 不在行首 ⇒ 不认
  [PASS] B-5：旧客户端会话 ⇒ 不回收（绝不误杀）
  [PASS] B-5：混表 ⇒ 只回收「pid 已知且已死」那条，旧客户端那条不动
```

### 4.3 编译生产侧 `RimeWithWeasel`（**解析改动必须在这里被编译到**）

```
& $msb "F:\soft\00selfmade\rime_claude\RimeWithWeasel\RimeWithWeasel.vcxproj" `
  /t:Rebuild /p:Configuration=Release /p:Platform=Win32 /p:SolutionDir=F:\soft\00selfmade\rime_claude\
```

结果：**`MSBUILD_EXIT=0`，错误 0**；`RimeWithWeasel.cpp` **本文件 0 警告**
（输出里唯一一条警告来自依赖工程 `WeaselUI\WeaselPanel.cpp:167` 的既有 `C4267`，与本批无关）。

产物 mtime（证明改动确实进了库）：

```
RimeWithWeasel.obj  mtime=2026-11-21 09:09:16.036
RimeWithWeasel.lib  mtime=2026-11-21 09:09:17.087
src RimeWithWeasel.cpp        mtime=2026-11-21 09:06:50.234
hdr FluxingSessionReclaim.h   mtime=2026-11-21 09:06:31.612
```

⇒ obj/lib 比两个被改源文件都新 ⇒ 解析改动**确实**被编译进静态库。

---

## 5. 共享头 **全 ASCII** 自证（`RimeWithWeasel.vcxproj` 无 `/utf-8` ⇒ 非 ASCII 字节会让整个进程加载失败）

逐字节读 `include/FluxingSessionReclaim.h`：

```
HEADER_ASCII_PROOF bytes=8484 maxByte=0x7D nonAsciiCount=0 bom=False
HEADER_SHA16=BB957544A339CB45
```

⇒ **无任何字节 > 0x7F**（最大字节 0x7D = `}`）、**无 BOM**。

行尾 / 编码三件套（改动前后一致，未被工具改写）：

| 文件 | 行尾 | 编码 |
|---|---|---|
| `include/FluxingSessionReclaim.h` | LF only（219 LF / 0 CRLF） | ASCII，无 BOM |
| `RimeWithWeasel/RimeWithWeasel.cpp` | **CRLF**（3133 CRLF / 0 LF-only） | UTF-8，无 BOM |
| `test/TestFluxingCloud/TestFluxingCloud.cpp` | LF only | UTF-8，无 BOM（工程带 `/utf-8`，见 `TestFluxingCloud.vcxproj:124/145/167/189`） |

---

## 6. 假设

1. **共享函数取「整段握手文本」为入参**（`const std::wstring& body`），而不是父任务示例里的
   「value 片段」：要求是「**从握手文本里解析**」，且关键场景是「**没有这一行的旧客户端**」——
   只有以 body 为入参，该场景才能被单测直接覆盖（否则「缺行」仍留在调用方，测不到解析层）。
2. **生产侧把「逐行判断」换成「收集 body + 循环外单点调用」**：收集范围与改前逐行扫描
   **同边界**（到 `"."` 行为止）、行内容**逐字**保留（每行补回 `'\n'`）。
3. `unsigned long` 在 MSVC/Windows 上恒为 32 位（x86/x64 均是）——§3.3 的结论据此成立。

---

## 7. 未闭环 / 残留风险

1. **B-5 的真机端到端仍未做**：本批闭环的是「解析层 + 判据层」的**单元级**证据；
   「旧版客户端连真实服务端 ⇒ 不被回收」的真机链路未验（真机项属下一批范围）。
2. **唯一语义分歧已显式登记**（§3.3）：`> 0xFFFFFFFF` 从「回绕成已知 pid」改为「判非法」。
   若需严格「连回绕也照抄」，回退路径见 §3.3 末段。
3. **未 commit / 未 push**（按任务要求，改动全部留在工作区）。
   `git status -sb` 里另有 `?? docs/design/quickpanel-v6/...`、`?? scripts/verify/orphan-*.ps1`、
   `?? test/TestOrphanReclaimE2E/...`、` m librime` —— **均为本批之前就存在的其它批次产物，
   本批未碰**。
4. `test/TestWeaselIPC/TestWeaselIPC.cpp` 里另有一份**测试侧**的 pid 解析副本
   （`record_client_pid`，只把值写进记录文件），本批**未动**（不在授权文件范围内），
   它与服务端解析口径无关。
5. 本批**没有**新增任何中文用户可见文案，**没有**改 `librime/`，**没有**改
   `include/WeaselIPCData.h` 或消息类型 ⇒ 不触发 IPC 四边同步红线。

---

## 8. 证据路径

| 证据 | 路径 |
|---|---|
| 共享纯函数（新增） | `F:\soft\00selfmade\rime_claude\include\FluxingSessionReclaim.h:88` |
| 生产侧单点调用 | `F:\soft\00selfmade\rime_claude\RimeWithWeasel\RimeWithWeasel.cpp:1120` |
| 新单测用例 | `F:\soft\00selfmade\rime_claude\test\TestFluxingCloud\TestFluxingCloud.cpp:3574` |
| `main()` 调用点 | `F:\soft\00selfmade\rime_claude\test\TestFluxingCloud\TestFluxingCloud.cpp:3751` |
| 单测 exe | `F:\soft\00selfmade\rime_claude\Release\TestFluxingCloud.exe`（mtime 2026-11-21 09:08:07.732） |
| 生产静态库 | `F:\soft\00selfmade\rime_claude\msbuild\Release\Win32\RimeWithWeasel.lib`（mtime 2026-11-21 09:09:17.087） |
| 差分夹具（仓库外，未入库） | `%TEMP%\fluxing-b5-diff\diff_parse.cpp` + `diff_parse.exe` |
