# 阶段 1 · 确认环（relay-01of06）

- 日期：2026-11-27
- 会话：`mvs_e9d60ea4bfe343d7932a3ec3212b437b`（链标识 `relay-01of06`）
- 性质：**只读复核轮**。零产品代码改动、零出包、零装机、零生产写（未动 D1 / R2 / Cloudflare / 用户词典）。
- 依据：`docs/plan-relay.md` §2 阶段 1 + `docs/relay/stage-2-rulings-20261127.md` §7.3。

## §0 结论

**三项判据 3/3 PASS ⇒ 阶段 1 完成，链条推进到阶段 2。**

| 判据 | 目标 | 实测 | 判定 |
|---|---|---|---|
| ① | `1d86d8c` 是 HEAD 的祖先 | `git merge-base --is-ancestor` 退出码 **0** | **PASS** |
| ② | 门禁四行复跑全绿 | `PASS=1014` `FAIL=0` `UFFFD_COUNT=0` `CLANGFORMAT_HUNK_DRIFT=0` | **PASS** |
| ③ | 阶段 1 结论仍成立 | `1d86d8c..HEAD` 在结论面内改动条数 **0** | **PASS** |

## §1 状态机与锁（执行前基线）

- `docs/relay.json`：`status=running` `current_stage=1` `max_stages=6` `stages=0` `sessions=0` `notes=0`，`sha256=BF32966C58EEA0DC8EC084BAF0B785B75E65B19D28235A37483F085D9829EF78`（bootstrap 态）。
- 互斥锁：`state=LIVE holder=mvs_e9d60ea4bfe343d7932a3ec3212b437b prev_holder=start-relay:308052 stage=1 ttl_s=1800`；长构建前已 `renew`（`heartbeat_at=2026-11-27T14:14:44+08:00`）。
- 分支 `feat/100-cloud-sync`，HEAD = `572de94d6d5bc6b255b632b3fc4cacf43777977b`。

## §2 判据① —— 祖先检查

```
git merge-base --is-ancestor 1d86d8c HEAD   ->  ANCESTOR_EXIT=0
git log -1 1d86d8c -> 1d86d8c 2026-11-27 11:10:49 +0800
                      feat(WeaselServer): 规则 1 v2 改用桶空间库存证据 + F3 桶端点 HTTP 状态诊断行
```

**VERDICT：PASS。** 上一修复轮的成果提交确实在 HEAD 的祖先链上。

## §3 判据② —— 门禁四行复跑

执行口径（严格按 `docs/RELAY-ADAPTATION.md` §4）：

1. `$env:FLUXING_CLOUD_SYNC_BUCKET_PHASE = '0'`（实测 `PHASE_ENV=0`）—— 防止测试进程继承 User 级桶相位造成成批假失败。
2. 单独 msbuild 测试工程并显式带 `SolutionDir`（`weasel.sln` 不含 `TestFluxingCloud`，漏带会跑上一轮旧 exe = 假绿）：
   `MSBuild.exe test\TestFluxingCloud\TestFluxingCloud.vcxproj /p:Configuration=Release /p:Platform=x64 /p:SolutionDir=F:\soft\00selfmade\rime_claude\ /m /v:m /nologo`
   → `MSBUILD_EXIT=0`，耗时 4 s，输出 `Release\TestFluxingCloud.exe`。
3. `pwsh -NoProfile -ExecutionPolicy Bypass -File reports\fluxing-br-1g\gate-p0-run.ps1`

实测四行（判定只看这四行，**不看** `$LASTEXITCODE`）：

```
PASS=1014 FAIL=0
OUT| RESULT: PASS=1014 FAIL=0
UFFFD_COUNT=0
CLANGFORMAT_HUNK_DRIFT=0
GATE_EXIT=0
EXE_EXIT=0
```

判绿条件 `PASS >= 1014` 且 `FAIL = 0` 且 `UFFFD_COUNT = 0` 且 `CLANGFORMAT_HUNK_DRIFT = 0` —— **四条全部满足**。

**防假绿三项交叉核验**：

| 检查 | 实测 | 含义 |
|---|---|---|
| exe 时间戳 | `Release\TestFluxingCloud.exe` mtime `2026-11-27T11:08:11`，size `1,072,128`，sha256 `B4255B67C3104B8F33EB728F76B4BB36F5F000E9E09B7931DE657BBD9A146E93` | 未在 14:15 重建 ⇒ msbuild 判定 up-to-date |
| 增量判定有效性 | msbuild 自身的时间戳比较通过（若任一源新于 exe 必然重编并刷新 mtime） | 不是「旧 exe 静默复用」 |
| 结论面脏文件 | `git status --porcelain --untracked-files=no -- WeaselServer/Cloud test/TestFluxingCloud` → **0 行**；最后一次触及该面的提交即 `1d86d8c` | 磁盘源码 == 生成该 exe 的源码 |

旁证：`CLANGFORMAT_WHOLEFILE_DRIFT=3`（`CloudSyncService.cpp` / `CloudRimeUserDictBackend.cpp` / `TestFluxingCloud.cpp` 三份**整文件**漂移）。该值属 HEAD 存量，**不进判据**（判据只认 hunk 级漂移 = 0），本轮不重排、不整文件格式化。

**VERDICT：PASS。**

## §4 判据③ —— 阶段 1 结论面的零改动复核

```
git log --oneline 1d86d8c..HEAD -- WeaselServer/Cloud test/TestFluxingCloud
->  LOG_LINES=0
```

`1d86d8c` 之后到 HEAD 之间，**没有任何提交触及** `WeaselServer/Cloud` 与 `test/TestFluxingCloud`；工作树在这两个路径上也无 tracked 修改。

结论面零改动 ⇒ `docs/relay/stage-1-inventory.md`（含 §9 修复轮新事实）的结论**在结构上不可能被推翻**。

**VERDICT：PASS。**

## §5 本轮明确未做 / 未验证

- 未跑真机四判据（①4xx/5xx、②全分页、③桶推执行证明、④桶侧不得旧于本地）—— 属阶段 2。
- 未出包、未装机、未升版；未改任何产品代码或测试代码。
- 未读写生产 D1 / R2 / Cloudflare；未动 `RimeUserDir` / `InstallDir`。
- `CLANGFORMAT_WHOLEFILE_DRIFT=3` 未处理（见 §3 旁证）。
- **判据性质说明**：判据③ 是「零改动 ⇒ 不被推翻」的结构性核验，**不是**对结论正确性的独立复验。若要求在源码不变的前提下重新证伪结论，需另立批次。

## §6 回退触发条件核查（对照 `stage-1-inventory.md` §3）

三条回退触发条件**均未触发**：结论面零改动、门禁四行全绿、祖先关系成立。

## §7 阶段 2 的前置条件（交给 relay-02of06）

1. 桶相位必须保持 `0`（本机已复位；**阶段 3/4 永不启用** —— 与接力阶段编号不同）。
2. 真机取证必须落在 **`kBoth` 轮**：进程启动后第 1 轮恒为 `kPullOnly`，第 2 轮才是 `kBoth`，轮间隔实测 ≈62 分钟 ⇒ 起跑后需等待，不能取首轮。
3. 判据口径以 `docs/relay/stage-2-rulings-20261127.md` 为准：①`4xx=0` 硬红 + 5xx 逐条登记；②取最近一个「全分页 2xx + 末页 `has_more=0`」完整轮；③须见 `bucket quota ` 行且 `pushed + skipped >= 1` 且 `rows_pushed <= quota_rows`；④`unmapped=0` 且 `bucket_older=0`。
4. 写预算阈值 20,000 的落盘属**阶段 3**，需**单独人工授权**（一次生产写：改 Worker env + deploy 换 Version），本轮不动。
5. 红线不变：**AC-09 仍 FAIL，不得改记**（只有日志出现 `AC09_VERDICT=PASS` 才可改）；`AC-212` / `AC-215②` 未判定；`AC-213` / `AC-214` 永久退役；规则 1 盲区未修前不得切读。

## §8 证据路径（`reports/**` 不入库）

| 文件 | 作用 |
|---|---|
| `reports\fluxing-br-1g\stage1-confirm-checks.ps1` | 只读基线检查序列（锁 / 台账 / 祖先 / 写集 / 门禁产物） |
| `reports\fluxing-br-1g\stage1-checks-out.txt` | 上述脚本完整输出（含 `STATUS_LINES=4105` 全量 porcelain） |
| `reports\fluxing-br-1g\stage1-gate-run.ps1` | 门禁执行装置（设相位 0 + renew 锁 + msbuild + gate + 四行提取） |
| `reports\fluxing-br-1g\stage1-gate-out.txt` | 门禁转录全文（`=== GATE P0 RUN END ===` 至 `=== GATE END ===`） |