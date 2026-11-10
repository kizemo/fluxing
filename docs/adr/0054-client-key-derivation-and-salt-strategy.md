# 0054. 客户端密钥派生：自实现 PBKDF2-HMAC-SHA256 + HKDF，salt 按邮箱确定性派生

- 状态：已接受 (accepted)
- 日期：2026-11-11
- 修订：ADR 0039 §「客户端加密」一栏（原写「经 CNG」）
- 关联：ADR 0039（云端账号与多端同步总体架构）、spec 100 `plan.md` §3.1 / §3.2、`docs/research/fluxing-cloud-2026-11/email-verify-channel.md`（T101）
- 代码：`WeaselServer/Cloud/CloudCrypto.{h,cpp}`（新建）、`WeaselServer/Cloud/CloudSyncService.{h,cpp}`（信封 AAD 复用）
- 测试：`test/TestFluxingCloud/TestFluxingCloud.cpp`（`TestCloudCryptoVectors` 等 8 个阶段 D 测试函数）

## Context

spec 100 `plan.md` §3.1 原式规定密钥派生**经 Windows CNG**：

```
master   = PBKDF2-HMAC-SHA256(password, salt, iters=600000)  // CNG: BCryptDeriveKeyPBKDF2
auth_key = HKDF-SHA256(master, info="fluxing-auth-v1")
enc_key  = HKDF-SHA256(master, info="fluxing-enc-v1")
```

阶段 D 实施时有三处「原式可行、实做必须改」：

1. **CNG 的 PBKDF2 默认 PRF 是 SHA-1。** `BCryptDeriveKeyPBKDF2` 允许指定 PRF，但**不显式指定即 SHA-1**。服务端（未来若换 WebCrypto 实现）与浏览器端 `crypto.subtle.deriveBits` 用的都是 PBKDF2-HMAC-**SHA256** ⇒ 客户端若不小心落到 SHA-1 PRF，跨实现就**派生不出同一个 `auth_key`**。
2. **CNG 没有 HKDF。** `BCryptDeriveKey` 的 counter 模式（`BCryptKdfCounter`）是 KDF 但**不是** HKDF 的 extract-then-expand（无 info 串、输出块数受限）；RFC 5869 语义无法用 CNG 表达。
3. **salt 的来源在协议上是个死结。** 服务端**从不用 salt 做校验**（只比 `HMAC(pepper, auth_key)`），而客户端必须在 `POST /v1/auth/login` **之前**算出 `auth_key` 才能发出请求 ⇒ 客户端**拿不到**服务端随机 salt。若把「服务端返回的 salt」当派生输入，新设备永远算不出同一 `auth_key`，**跨设备同步直接不成立**。

另有一条独立约束：**仓库零第三方依赖**（宪法 P3 / NFR-004）⇒ 上述缺口不能用 OpenSSL / libsodium 补。

## Decision

1. **PBKDF2 / HKDF / SHA-256 / HMAC 在 `CloudCrypto` 内自实现**，不引第三方库。AES-256-GCM **仍走 CNG** —— 复用 `include/FluxingCloudEnvelope.h` 的 `crypto::AesGcmSeal` / `crypto::AesGcmOpen`（宪法 R7 生产与单测同源），**不重写 GCM**。
2. **PBKDF2**：PRF 固定 HMAC-SHA256、`dkLen = 32`、`iters = 600000`（常量 `kPbkdf2Iterations`）。实现取自 RFC 8018 §5.2。
3. **HKDF-SHA256**：salt = **32 字节全零**，info 分别为 `"fluxing-auth-v1"` / `"fluxing-enc-v1"`，输出**单块 32 字节**（`PRK = HMAC(zeros32, ikm)`；`OKM = HMAC(PRK, info || 0x01)`）。
4. **salt = `SHA256("fluxing-enc-v1|" + lower(email))`** —— 确定性、可由客户端自算。服务端响应中的 `salt` 字段**只作诊断记录**，**不参与任何派生**。
5. **口令与密钥缓冲用后 `SecureZero`**（`SecureZeroString` / `SecureZeroBytes`）；派生路径**全程不写日志**。
6. **测试向量用标准来源，不自证自洽**：SHA-256 = FIPS 180-4；HMAC = RFC 4231 TC1/TC2/TC3/TC6；PBKDF2 = RFC 7914 §11（`c=1/2/4096`，取前 32 字节）；HKDF 无适用官方向量（固定 32B 零盐 + 单块输出使 RFC 5869 TC1/TC3 不适用）⇒ 改为「**按 RFC 5869 extract-then-expand 公式复算 + .NET `HMACSHA256` 独立实现**」双证，并在测试注释写明理由。

## Consequences

- ✅ **跨实现可互通**：PBKDF2-HMAC-SHA256 与 HKDF-SHA256 都是标准化原语，未来 JS / WebCrypto 客户端能算出同一组密钥。
- ✅ **可用官方向量钉死**：单测不依赖「自己和自己比」，改坏实现立刻变红。
- ✅ **服务端 salt 不再是阻塞项**：客户端不需要先拿服务端随机 salt 就能算 `auth_key` ⇒ 新设备首次登录即可工作。
- ✅ **不新增第三方依赖**（P3 / NFR-004），GCM 仍由 CNG 承担。
- ⚠️ **自实现密码学有风险**：PBKDF2 / HKDF 的正确性完全依赖标准向量 + 独立实现交叉验证；**任何改动都必须重跑 `tmp\build-fluxing-cloud-tests.cmd` 门禁**，且期望值不得凭记忆填写。
- ⚠️ **salt 不是熵**：它只提供**域分离**（防同一密钥跨用途复用），口令强度仍是唯一防线（`plan.md` §3.4 的补偿措施照旧：口令强度下限 + 可选独立「同步口令」）。
- ⚠️ **服务端侧 pepper 仍是抗离线猜测的唯一依赖**：salt 可公开计算 ⇒ 若 `verifier` 与 pepper 同时泄漏，攻击者可对弱口令做离线字典攻击。与 ADR 0039 的威胁模型一致（内容机密性 ≠ 账号隔离）。
- ⚠️ **60 万次迭代的实际耗时未单独测量**（单测用 1000/2000 次 + 常量断言；登录路径走 600000 次）⇒ 首次登录延迟需真机测量后再定 UI 反馈策略。
- ➕ **HKDF 目前不支持带 salt 参数的重载**：固定 32B 零盐是本协议的简化，若将来要对接「服务端随机 salt」需扩接口。

## Verification

- 单测 `test/TestFluxingCloud/TestFluxingCloud.cpp::TestCloudCryptoVectors`：
  SHA-256（空串 / `"abc"` / 100 万 `'a'`）· HMAC（RFC 4231 TC1/TC2/TC3/TC6）· PBKDF2（RFC 7914 §11，`c=1/2/4096`）· HKDF（公式复算 + .NET 独立实现）· `DeriveKeys`（迭代次数不同 ⇒ 密钥不同；`auth_key` 与 `enc_key` 分离；各 32 字节；空口令失败）· `EncodeAuthKey` 长度 44 · `GenerateDeviceId` 16 位十六进制且两次不同 · `SecureZeroString` / `SecureZeroBytes`。
- 整包门禁：`tmp\build-fluxing-cloud-tests.cmd` ⇒ `RESULT: PASS=297 FAIL=0` / `VERDICT: ALL PASS` / `TEST_FLUXINGCLOUD_EXIT=0`（日志 `tmp\stageD-build.log`）。
- AAD 绑定与「错密钥 / 篡改必解密失败」由 `TestCloudSyncPure` 覆盖（`SealEntries` / `OpenItems` 往返 + 反例）。
