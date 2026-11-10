// TestFluxingCloud.cpp — 火流猩 · 用户资料云同步 · 阶段 B 单测
//
// spec 100 / tasks.md：
//   T201 信封编解码   （往返 / AAD 篡改 / nonce 长度 / base64 畸形 / 空与超长载荷）
//   T202 条目合并     （同键取大 / 同键同版本确定 / 异键并集 / 幂等结合律 / 两端收敛）
//   T203 HLC 定序     （严格弱序 / 单调 / 同毫秒自增 / 抬升 / 双端同序）
//   T204 共享生产头   （本文件直接 include include/FluxingCloudEnvelope.h）
//
// T204 判据：生产与单测**同源**。本文件不定义任何信封编解码 / HLC / 条目合并
// 的第二份实现，只调用 include/FluxingCloudEnvelope.h 里的 inline 函数
// （constitution R7「生产与单测共用一套实现」）。

// NOMINMAX 必须在 windows.h 之前：否则 <algorithm> 里的 std::min/std::max
// 会被 windows.h 的 min/max 宏污染（本文件用到 std::sort / std::swap）。
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include <windows.h>
#include <bcrypt.h>
#include <winhttp.h>
#include <winreg.h>

// 生产头（T204）。用相对路径而不是 $(SolutionDir)\include，
// 这样本文件被任何工程单独打开/编译都能解析到同一个生产头。
#include "../../include/FluxingCloudEnvelope.h"

namespace fc = fluxing::cloud;

namespace {

int g_pass = 0;
int g_fail = 0;

void Check(bool ok, const char* name) {
  if (ok) {
    ++g_pass;
    std::printf("  [PASS] %s\n", name);
  } else {
    ++g_fail;
    std::printf("  [FAIL] %s\n", name);
  }
}

// 把 N 次循环折叠成一条结果：既保留「穷举了 N 项」的信息，又不刷爆输出。
void CheckMany(bool ok, const char* name, int n) {
  if (ok) {
    ++g_pass;
    std::printf("  [PASS] %s（%d 项）\n", name, n);
  } else {
    ++g_fail;
    std::printf("  [FAIL] %s（%d 项）\n", name, n);
  }
}

// 错误码断言：失败时打印期望值 / 实际值的**名字**，
// 这样「kBadBase64 vs kAuthFailed」这类分类错位能一眼看出。
void CheckErr(fc::EnvelopeError got, fc::EnvelopeError want, const char* name) {
  if (got == want) {
    ++g_pass;
    std::printf("  [PASS] %s\n", name);
    return;
  }
  ++g_fail;
  std::printf("  [FAIL] %s（期望 %s，实际 %s）\n", name,
              fc::EnvelopeErrorName(want), fc::EnvelopeErrorName(got));
}

// ---------------------------------------------------------------------------
// 测试夹具
// ---------------------------------------------------------------------------

const char* const kItemKey = "userdict/custom_phrase.txt";

std::vector<uint8_t> TestKey() {
  std::vector<uint8_t> k(fc::kKeyBytes);
  for (size_t i = 0; i < k.size(); ++i)
    k[i] = static_cast<uint8_t>(0x10 + i * 7);
  return k;
}

fc::Hlc TestHlc() {
  fc::Hlc h;
  h.ms = 1762700000000ull;
  h.counter = 7;
  h.device = "0123456789abcdef";
  return h;
}

// 一行一个字段的线格式基线（8 行）。合法基线先自证可解析，
// 后面每条反例的失败才能归因到「那处改动」而不是「基线本来就坏」。
std::vector<std::string> MakeBaseLines() {
  const std::vector<uint8_t> nonce(12, 0x11);
  const std::vector<uint8_t> cipher(16, 0x22);
  std::vector<std::string> L;
  L.push_back("v1");
  L.push_back(std::string("item_key\t") + kItemKey);
  L.push_back("type\tphrases");
  L.push_back("hlc_ms\t1762700000000");
  L.push_back("hlc_counter\t7");
  L.push_back("device\t0123456789abcdef");
  L.push_back("nonce\t" + fc::Base64Encode(nonce));
  L.push_back("cipher\t" + fc::Base64Encode(cipher));
  return L;
}

std::string JoinWire(const std::vector<std::string>& lines) {
  std::string s;
  for (size_t i = 0; i < lines.size(); ++i) {
    s += lines[i];
    s += '\n';
  }
  return s;
}

fc::Entry MakeEntry(const char* key, const char* payload, uint64_t ms,
                    uint16_t counter, const char* device) {
  fc::Entry e;
  e.key = key;
  e.payload = payload;
  e.hlc.ms = ms;
  e.hlc.counter = counter;
  e.hlc.device = device;
  return e;
}

// 条目合并的输出契约：key 严格升序（因而必然唯一）。
bool KeysAscendingUnique(const std::vector<fc::Entry>& v) {
  for (size_t i = 1; i < v.size(); ++i) {
    if (!(v[i - 1].key < v[i].key))
      return false;
  }
  return true;
}

// ===========================================================================
// [1]–[4] 工程骨架冒烟（T104 遗留，防止「链接断了却以为是逻辑错」）
// ===========================================================================

void TestHarnessAlive() {
  std::printf("[1] 冒烟：测试工程可运行\n");
  Check(1 + 1 == 2, "算术基线");
  Check(std::strlen("火") == 3, "UTF-8 字面量为 3 字节（/utf-8 生效）");
  Check(std::strlen(fc::EnvelopeErrorName(fc::EnvelopeError::kOk)) > 0,
        "生产头 FluxingCloudEnvelope.h 已被 include（非空错误名）");
}

void TestWinHttpLinked() {
  std::printf("[2] 链接：winhttp.lib\n");
  HINTERNET s = ::WinHttpOpen(L"Fluxing-TestFluxingCloud/1.0",
                              WINHTTP_ACCESS_TYPE_NO_PROXY,
                              WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  Check(s != nullptr, "WinHttpOpen 返回有效会话句柄");
  if (s != nullptr)
    ::WinHttpCloseHandle(s);
}

void TestBCryptLinked() {
  std::printf("[3] 链接：bcrypt.lib\n");
  BCRYPT_ALG_HANDLE alg = nullptr;
  NTSTATUS st = ::BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM,
                                             nullptr, 0);
  Check(NT_SUCCESS(st) && alg != nullptr, "BCryptOpenAlgorithmProvider(SHA256)");
  if (alg != nullptr) {
    DWORD obj_len = 0;
    ULONG got = 0;
    st = ::BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH,
                             reinterpret_cast<PUCHAR>(&obj_len),
                             sizeof(obj_len), &got, 0);
    Check(NT_SUCCESS(st) && obj_len > 0, "BCryptGetProperty(OBJECT_LENGTH) 返回正数");
    ::BCryptCloseAlgorithmProvider(alg, 0);
  }
}

void TestAdvApiLinked() {
  std::printf("[4] 链接：advapi32.lib\n");
  HKEY key = nullptr;
  const LONG rc = ::RegOpenKeyExW(HKEY_CURRENT_USER, L"Software", 0, KEY_READ, &key);
  Check(rc == ERROR_SUCCESS && key != nullptr, "RegOpenKeyExW(HKCU\\Software) 成功");
  if (key != nullptr)
    ::RegCloseKey(key);
}

// ===========================================================================
// [5] base64 严格模式
// ===========================================================================

void TestBase64() {
  std::printf("[5] base64 严格模式\n");

  std::vector<uint8_t> all(256);
  for (size_t i = 0; i < all.size(); ++i)
    all[i] = static_cast<uint8_t>(i);
  std::vector<uint8_t> back;
  Check(fc::Base64Decode(fc::Base64Encode(all), &back) && back == all,
        "256 字节全量往返一致");

  bool lens_ok = true;
  for (size_t len = 0; len <= 4; ++len) {
    std::vector<uint8_t> src(len);
    for (size_t i = 0; i < len; ++i)
      src[i] = static_cast<uint8_t>(0xA5 + i);
    std::vector<uint8_t> dst;
    if (!fc::Base64Decode(fc::Base64Encode(src), &dst) || dst != src)
      lens_ok = false;
  }
  CheckMany(lens_ok, "长度 0/1/2/3/4 字节往返一致（三种 padding 分支都覆盖）", 5);

  // 正例必须先成立：否则下面 5 条反例可能只是「解码器一律拒绝」造成的假绿。
  std::vector<uint8_t> tmp;
  Check(fc::Base64Decode("QQ==", &tmp) && tmp.size() == 1 && tmp[0] == 0x41,
        "严格模式接受合法 padding「QQ==」");
  tmp.clear();
  Check(fc::Base64Decode("QUI=", &tmp) && tmp.size() == 2 && tmp[0] == 'A' &&
            tmp[1] == 'B',
        "严格模式接受合法 padding「QUI=」");

  tmp.clear();
  Check(!fc::Base64Decode("abc", &tmp), "反例：长度非 4 的倍数「abc」被拒");
  Check(!fc::Base64Decode("ab$d", &tmp), "反例：非法字符「ab$d」被拒");
  Check(!fc::Base64Decode("QR==", &tmp), "反例：填充位非 0「QR==」被拒");
  Check(!fc::Base64Decode("Q=Q=", &tmp), "反例：'=' 出现在组内前两位「Q=Q=」被拒");
  Check(!fc::Base64Decode("====", &tmp), "反例：以 '=' 打头「====」被拒");
  tmp.clear();
  Check(fc::Base64Decode("", &tmp) && tmp.empty(), "空串解出空内容");
}

// ===========================================================================
// [6] 条目类型
// ===========================================================================

void TestItemType() {
  std::printf("[6] 条目类型与错误名\n");
  Check(std::strcmp(fc::ItemTypeName(fc::ItemType::kConfig), "config") == 0,
        "ItemTypeName(kConfig) == config");
  Check(std::strcmp(fc::ItemTypeName(fc::ItemType::kPhrases), "phrases") == 0,
        "ItemTypeName(kPhrases) == phrases");
  Check(std::strcmp(fc::ItemTypeName(fc::ItemType::kUserDb), "userdb") == 0,
        "ItemTypeName(kUserDb) == userdb");

  fc::ItemType t = fc::ItemType::kConfig;
  Check(fc::ParseItemType("userdb", &t) && t == fc::ItemType::kUserDb,
        "ParseItemType(userdb) 往返");
  Check(!fc::ParseItemType("bogus", &t), "反例：ParseItemType(bogus) 被拒");

  Check(!fc::IsEntryMergedType(fc::ItemType::kConfig),
        "config 整份覆盖（不逐条合并）");
  Check(fc::IsEntryMergedType(fc::ItemType::kPhrases), "phrases 逐条合并");
  Check(fc::IsEntryMergedType(fc::ItemType::kUserDb), "userdb 逐条合并");
}

// ===========================================================================
// [7] 信封往返（T201 ①）
// ===========================================================================

void TestEnvelopeRoundTrip() {
  std::printf("[7] 信封往返（T201 ①）\n");
  const std::vector<uint8_t> key = TestKey();
  const fc::Hlc hlc = TestHlc();
  // 明文刻意含 CRLF 与 TAB：证明密文走 base64、不靠转义
  const std::string plain = "词条一\r\n词条二\t词条三\n";

  fc::Envelope env;
  std::string err;
  CheckErr(fc::SealEnvelope(key, kItemKey, fc::ItemType::kPhrases, hlc, plain,
                            &env, &err),
           fc::EnvelopeError::kOk, "SealEnvelope 返回 kOk");
  Check(env.nonce.size() == fc::kNonceBytes, "nonce 为 12 字节");
  Check(env.cipher.size() == plain.size() + fc::kTagBytes,
        "cipher 长度 == 明文 + 16B tag");
  Check(env.item_key == kItemKey && env.type == fc::ItemType::kPhrases &&
            fc::HlcEqual(env.hlc, hlc),
        "信封携带的 item_key / type / hlc 与入参一致");

  std::string wire;
  CheckErr(fc::SerializeEnvelope(env, &wire), fc::EnvelopeError::kOk,
           "SerializeEnvelope 返回 kOk");
  Check(wire.compare(0, 3, "v1\n") == 0, "线格式首行为 magic「v1」");

  fc::Envelope parsed;
  CheckErr(fc::ParseEnvelope(wire, &parsed), fc::EnvelopeError::kOk,
           "ParseEnvelope 返回 kOk（正向基线）");
  Check(parsed.item_key == env.item_key, "解析后 item_key 一致");
  Check(parsed.type == env.type, "解析后 type 一致");
  Check(fc::HlcEqual(parsed.hlc, env.hlc), "解析后 hlc 一致");
  Check(parsed.nonce == env.nonce, "解析后 nonce 一致");
  Check(parsed.cipher == env.cipher, "解析后 cipher 一致");

  std::string out;
  CheckErr(fc::OpenEnvelope(key, parsed, &out, &err), fc::EnvelopeError::kOk,
           "OpenEnvelope 返回 kOk");
  Check(out == plain, "解密后明文逐字节一致");
}

// ===========================================================================
// [8] 字段转义
// ===========================================================================

void TestFieldEscape() {
  std::printf("[8] 字段转义（反斜杠 / TAB / LF）\n");
  const std::vector<uint8_t> key = TestKey();
  fc::Hlc hlc = TestHlc();
  hlc.device = "dev\\with\ttab";
  const std::string item_key = "a\\b\tc\nd";
  const std::string plain = "payload";

  fc::Envelope env;
  std::string err;
  CheckErr(fc::SealEnvelope(key, item_key, fc::ItemType::kUserDb, hlc, plain,
                            &env, &err),
           fc::EnvelopeError::kOk, "含反斜杠 / TAB / LF 的字段可封装");

  std::string wire;
  fc::SerializeEnvelope(env, &wire);
  // 行首必须仍是 "device\t"；字段体内 '\' ⇒ "\\"、TAB ⇒ "\t"，
  // 这样行结构不会被字段内容里的 TAB / LF 破坏。
  Check(wire.find("\ndevice\tdev\\\\with\\ttab\n") != std::string::npos,
        "device 里的 '\\' 与 TAB 被转义（未污染行结构）");

  fc::Envelope parsed;
  CheckErr(fc::ParseEnvelope(wire, &parsed), fc::EnvelopeError::kOk,
           "含转义字段的线格式可解析");
  Check(parsed.item_key == item_key, "item_key 转义往返一致");
  Check(parsed.hlc.device == hlc.device, "device 转义往返一致");

  std::string out;
  CheckErr(fc::OpenEnvelope(key, parsed, &out, &err), fc::EnvelopeError::kOk,
           "转义字段仍能解密");
  Check(out == plain, "转义字段明文一致");
}

// ===========================================================================
// [9] 信封反例：AAD 篡改 / nonce / 密文 / 载荷边界（T201 ②③④⑤）
// ===========================================================================

void TestEnvelopeTamper() {
  std::printf("[9] 信封反例：AAD 篡改 / 密文 / nonce / 载荷边界（T201 ②③④⑤）\n");
  const std::vector<uint8_t> key = TestKey();
  const fc::Hlc hlc = TestHlc();
  const std::string plain = "自定义短语：你好\t世界";

  fc::Envelope env;
  std::string err;
  CheckErr(fc::SealEnvelope(key, kItemKey, fc::ItemType::kPhrases, hlc, plain,
                            &env, &err),
           fc::EnvelopeError::kOk, "基线信封已封好");

  // ② AAD 的 4 个组成项逐一篡改，都必须认证失败
  {
    fc::Envelope t = env;
    t.hlc.ms += 1;
    std::string o;
    CheckErr(fc::OpenEnvelope(key, t, &o, &err), fc::EnvelopeError::kAuthFailed,
             "反例②：hlc_ms +1 ⇒ kAuthFailed");
  }
  {
    fc::Envelope t = env;
    t.hlc.counter = static_cast<uint16_t>(t.hlc.counter + 1);
    std::string o;
    CheckErr(fc::OpenEnvelope(key, t, &o, &err), fc::EnvelopeError::kAuthFailed,
             "反例②：hlc_counter +1 ⇒ kAuthFailed");
  }
  {
    fc::Envelope t = env;
    t.type = fc::ItemType::kUserDb;
    std::string o;
    CheckErr(fc::OpenEnvelope(key, t, &o, &err), fc::EnvelopeError::kAuthFailed,
             "反例②：type phrases→userdb ⇒ kAuthFailed");
  }
  {
    fc::Envelope t = env;
    t.item_key = "userdict/custom_phrase.yaml";
    std::string o;
    CheckErr(fc::OpenEnvelope(key, t, &o, &err), fc::EnvelopeError::kAuthFailed,
             "反例②：item_key 换文件名 ⇒ kAuthFailed");
  }

  // 密文 / tag / 密钥位翻转
  {
    fc::Envelope t = env;
    t.cipher[0] = static_cast<uint8_t>(t.cipher[0] ^ 0xFF);
    std::string o;
    CheckErr(fc::OpenEnvelope(key, t, &o, &err), fc::EnvelopeError::kAuthFailed,
             "反例：密文首字节翻转 ⇒ kAuthFailed");
  }
  {
    fc::Envelope t = env;
    const size_t last = t.cipher.size() - 1;
    t.cipher[last] = static_cast<uint8_t>(t.cipher[last] ^ 0x01);
    std::string o;
    CheckErr(fc::OpenEnvelope(key, t, &o, &err), fc::EnvelopeError::kAuthFailed,
             "反例：tag 末字节翻转 ⇒ kAuthFailed");
  }
  {
    std::vector<uint8_t> wrong = key;
    wrong[0] = static_cast<uint8_t>(wrong[0] ^ 0xFF);
    std::string o;
    CheckErr(fc::OpenEnvelope(wrong, env, &o, &err), fc::EnvelopeError::kAuthFailed,
             "反例：错误 enc_key ⇒ kAuthFailed");
  }

  // ③ nonce 长度
  {
    fc::Envelope t = env;
    t.nonce.pop_back();
    std::string w;
    CheckErr(fc::SerializeEnvelope(t, &w), fc::EnvelopeError::kBadNonce,
             "反例③：nonce 11 字节 ⇒ SerializeEnvelope 报 kBadNonce");
    std::string o;
    CheckErr(fc::OpenEnvelope(key, t, &o, &err), fc::EnvelopeError::kBadNonce,
             "反例③：nonce 11 字节 ⇒ OpenEnvelope 报 kBadNonce");
  }
  {
    // 线格式层：base64 合法但只解出 8 字节
    std::vector<std::string> L = MakeBaseLines();
    Check(L.size() == 8, "线格式基行为 8 行");
    L[6] = "nonce\tAAAAAAAAAAA=";
    fc::Envelope p;
    CheckErr(fc::ParseEnvelope(JoinWire(L), &p), fc::EnvelopeError::kBadNonce,
             "反例③：线格式 nonce 解出 8 字节 ⇒ kBadNonce");
  }

  // ④ base64 畸形
  {
    std::vector<std::string> L = MakeBaseLines();
    L[7] = "cipher\t%%%%";
    fc::Envelope p;
    CheckErr(fc::ParseEnvelope(JoinWire(L), &p), fc::EnvelopeError::kBadBase64,
             "反例④：cipher 含非法 base64 字符 ⇒ kBadBase64");
  }
  {
    std::vector<std::string> L = MakeBaseLines();
    const std::string prefix = "cipher\t";
    const std::string good = L[7].substr(prefix.size());
    L[7] = prefix + good.substr(0, good.size() - 1);
    fc::Envelope p;
    CheckErr(fc::ParseEnvelope(JoinWire(L), &p), fc::EnvelopeError::kBadBase64,
             "反例④：cipher base64 被截断一个字符 ⇒ kBadBase64");
  }

  // ⑤ 空载荷
  {
    fc::Envelope e2;
    CheckErr(fc::SealEnvelope(key, kItemKey, fc::ItemType::kPhrases, hlc,
                              std::string(), &e2, &err),
             fc::EnvelopeError::kOk, "边界⑤：空载荷可封装（kOk）");
    Check(e2.cipher.size() == fc::kTagBytes, "边界⑤：空载荷 cipher 恰为 16B tag");
    std::string o;
    CheckErr(fc::OpenEnvelope(key, e2, &o, &err), fc::EnvelopeError::kOk,
             "边界⑤：空载荷可解封");
    Check(o.empty(), "边界⑤：空载荷解出空串");
  }
  // ⑤ 超长 / 恰好到上限
  {
    const std::string over(fc::kMaxPayloadBytes + 1, 'x');
    fc::Envelope e3;
    CheckErr(fc::SealEnvelope(key, kItemKey, fc::ItemType::kPhrases, hlc, over,
                              &e3, &err),
             fc::EnvelopeError::kTooLong, "边界⑤：kMaxPayloadBytes + 1 ⇒ kTooLong");
  }
  {
    const std::string exact(fc::kMaxPayloadBytes, 'y');
    fc::Envelope e4;
    CheckErr(fc::SealEnvelope(key, kItemKey, fc::ItemType::kPhrases, hlc, exact,
                              &e4, &err),
             fc::EnvelopeError::kOk,
             "边界⑤：恰好 kMaxPayloadBytes ⇒ kOk（上限判定不能写成 >=）");
    std::string o;
    CheckErr(fc::OpenEnvelope(key, e4, &o, &err), fc::EnvelopeError::kOk,
             "边界⑤：上限载荷可解封");
    Check(o == exact, "边界⑤：上限载荷逐字节还原");
  }
}

// ===========================================================================
// [10] 信封线格式反例
// ===========================================================================

void TestEnvelopeWire() {
  std::printf("[10] 信封线格式反例\n");
  {
    fc::Envelope p;
    CheckErr(fc::ParseEnvelope(JoinWire(MakeBaseLines()), &p), fc::EnvelopeError::kOk,
             "基线 8 行线格式可解析（正向基线，下面每条失败才可归因）");
  }
  {
    fc::Envelope p;
    CheckErr(fc::ParseEnvelope("", &p), fc::EnvelopeError::kEmpty,
             "反例：空输入 ⇒ kEmpty");
  }
  {
    std::vector<std::string> L = MakeBaseLines();
    L[0] = "v2";
    fc::Envelope p;
    CheckErr(fc::ParseEnvelope(JoinWire(L), &p), fc::EnvelopeError::kBadMagic,
             "反例：magic v2 ⇒ kBadMagic");
  }
  {
    std::vector<std::string> L = MakeBaseLines();
    L.resize(7);  // 丢掉 cipher 行
    fc::Envelope p;
    CheckErr(fc::ParseEnvelope(JoinWire(L), &p), fc::EnvelopeError::kBadField,
             "反例：缺 cipher 行（7 行）⇒ kBadField");
  }
  {
    std::vector<std::string> L = MakeBaseLines();
    L.push_back("zzz\t1");  // 多出一行
    fc::Envelope p;
    CheckErr(fc::ParseEnvelope(JoinWire(L), &p), fc::EnvelopeError::kBadField,
             "反例：多出一行（9 行）⇒ kBadField");
  }
  {
    std::vector<std::string> L = MakeBaseLines();
    std::swap(L[1], L[2]);
    fc::Envelope p;
    CheckErr(fc::ParseEnvelope(JoinWire(L), &p), fc::EnvelopeError::kBadField,
             "反例：item_key / type 两行交换 ⇒ kBadField（行序严格）");
  }
  {
    std::vector<std::string> L = MakeBaseLines();
    L[2] = "type\tbogus";
    fc::Envelope p;
    CheckErr(fc::ParseEnvelope(JoinWire(L), &p), fc::EnvelopeError::kBadType,
             "反例：type=bogus ⇒ kBadType");
  }
  {
    std::vector<std::string> L = MakeBaseLines();
    L[3] = "hlc_ms\tabc";
    fc::Envelope p;
    CheckErr(fc::ParseEnvelope(JoinWire(L), &p), fc::EnvelopeError::kBadNumber,
             "反例：hlc_ms=abc ⇒ kBadNumber");
  }
  {
    std::vector<std::string> L = MakeBaseLines();
    L[4] = "hlc_counter\t70000";
    fc::Envelope p;
    CheckErr(fc::ParseEnvelope(JoinWire(L), &p), fc::EnvelopeError::kBadNumber,
             "反例：hlc_counter=70000 溢出 u16 ⇒ kBadNumber");
  }
  {
    std::vector<std::string> L = MakeBaseLines();
    L[1] = "item_key\ta\\qyaml";
    fc::Envelope p;
    CheckErr(fc::ParseEnvelope(JoinWire(L), &p), fc::EnvelopeError::kBadEscape,
             "反例：未知转义 \\q ⇒ kBadEscape");
  }
}

// ===========================================================================
// [11] HLC 全序（T203 ①⑤）
// ===========================================================================

void TestHlcOrder() {
  std::printf("[11] HLC 全序（T203 ①⑤）\n");

  const uint64_t ms_set[3] = {1000, 1001, 1002};
  const uint16_t ct_set[3] = {0, 1, 2};
  const char* dev_set[2] = {"aaaa", "bbbb"};
  std::vector<fc::Hlc> all;
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      for (int k = 0; k < 2; ++k) {
        fc::Hlc h;
        h.ms = ms_set[i];
        h.counter = ct_set[j];
        h.device = dev_set[k];
        all.push_back(h);
      }
    }
  }
  Check(all.size() == 18, "构造 3×3×2 = 18 个版本号（三维度全组合）");

  bool irreflexive = true;
  for (size_t i = 0; i < all.size(); ++i) {
    if (fc::HlcLess(all[i], all[i]))
      irreflexive = false;
  }
  CheckMany(irreflexive, "① 不可自反：!less(x,x)", static_cast<int>(all.size()));

  bool antisym = true;
  int n_antisym = 0;
  for (size_t i = 0; i < all.size(); ++i) {
    for (size_t j = 0; j < all.size(); ++j) {
      ++n_antisym;
      if (fc::CompareHlc(all[i], all[j]) != -fc::CompareHlc(all[j], all[i]))
        antisym = false;
    }
  }
  CheckMany(antisym, "① 反对称：cmp(a,b) == -cmp(b,a)", n_antisym);

  bool distinct = true;
  for (size_t i = 0; i < all.size(); ++i) {
    for (size_t j = i + 1; j < all.size(); ++j) {
      if (fc::CompareHlc(all[i], all[j]) == 0)
        distinct = false;
    }
  }
  CheckMany(distinct, "① 18 个版本两两可区分（无并列）",
            static_cast<int>(all.size() * (all.size() - 1) / 2));

  bool transitive = true;
  int n_trans = 0;
  for (size_t i = 0; i < all.size(); ++i) {
    for (size_t j = 0; j < all.size(); ++j) {
      for (size_t k = 0; k < all.size(); ++k) {
        ++n_trans;
        if (fc::HlcLess(all[i], all[j]) && fc::HlcLess(all[j], all[k]) &&
            !fc::HlcLess(all[i], all[k]))
          transitive = false;
      }
    }
  }
  CheckMany(transitive, "① 传递：less(a,b) && less(b,c) ⇒ less(a,c)", n_trans);

  {
    fc::Hlc a;
    a.ms = 5;
    a.counter = 0;
    a.device = "d";
    fc::Hlc b = a;
    b.ms = 6;
    fc::Hlc c = a;
    c.counter = 1;
    fc::Hlc d = a;
    d.device = "e";
    Check(fc::HlcLess(a, b), "① 仅 ms 不同也能定序");
    Check(fc::HlcLess(a, c), "① 仅 counter 不同也能定序");
    Check(fc::HlcLess(a, d), "① 仅 device 不同也能定序");
  }

  std::vector<fc::Hlc> asc = all;
  std::sort(asc.begin(), asc.end(), fc::HlcLess);
  bool nondecreasing = true;
  for (size_t i = 1; i < asc.size(); ++i) {
    if (fc::HlcLess(asc[i], asc[i - 1]))
      nondecreasing = false;
  }
  Check(nondecreasing, "① std::sort(HlcLess) 结果非降");

  std::vector<fc::Hlc> desc = all;
  std::sort(desc.begin(), desc.end(),
            [](const fc::Hlc& x, const fc::Hlc& y) { return fc::HlcLess(y, x); });
  bool mirrored = true;
  for (size_t i = 0; i < asc.size(); ++i) {
    if (!fc::HlcEqual(asc[i], desc[asc.size() - 1 - i]))
      mirrored = false;
  }
  Check(mirrored, "① 正序与逆序排序互为镜像（全序无并列）");

  // ⑤ 两台设备对同一组版本算出的顺序，与各自收到的排列无关
  std::vector<fc::Hlc> perm;
  for (size_t i = 0; i < all.size(); ++i)
    perm.push_back(all[(i * 7 + 3) % all.size()]);  // gcd(7,18)=1 ⇒ 双射
  std::sort(perm.begin(), perm.end(), fc::HlcLess);
  bool same_order = perm.size() == asc.size();
  for (size_t i = 0; same_order && i < asc.size(); ++i) {
    if (!fc::HlcEqual(perm[i], asc[i]))
      same_order = false;
  }
  Check(same_order, "⑤ 两台设备对同一组版本算出同一顺序（与到达排列无关）");
}

// ===========================================================================
// [12] HLC 本机时钟（T203 ②③④）
// ===========================================================================

void TestHlcClock() {
  std::printf("[12] HLC 本机时钟（T203 ②③④）\n");
  const uint64_t t0 = 1762700000000ull;

  fc::HlcClock c;
  c.device = "0000aaaa";
  const fc::Hlc h1 = fc::HlcTick(&c, t0);
  Check(h1.ms == t0 && h1.counter == 0, "② 首次写入 ms = now_ms，counter = 0");
  Check(h1.device == "0000aaaa", "② 写入携带本机 device");
  Check(h1.ms >= c.last_ms, "② 写入后 ms >= last_ms");

  const fc::Hlc h2 = fc::HlcTick(&c, t0);
  Check(h2.ms == t0 && h2.counter == 1, "③ 同毫秒再写 counter 自增到 1");
  Check(fc::HlcLess(h1, h2), "③ 同毫秒第二次写入严格大于第一次");

  {
    // 墙钟抖动序列：正常 → 回拨 1s → 前进 5ms → 回拨近 1000s → 回到 t0
    const uint64_t wall[5] = {t0, t0 - 1000, t0 + 5, t0 - 999999, t0};
    fc::HlcClock c2;
    c2.device = "0000bbbb";
    bool increasing = true;
    bool ms_never_back = true;
    fc::Hlc prev;
    uint64_t max_ms = 0;
    for (int i = 0; i < 5; ++i) {
      const fc::Hlc cur = fc::HlcTick(&c2, wall[i]);
      if (i > 0 && !fc::HlcLess(prev, cur))
        increasing = false;
      if (cur.ms < max_ms)
        ms_never_back = false;
      if (cur.ms > max_ms)
        max_ms = cur.ms;
      prev = cur;
    }
    CheckMany(increasing, "③ 墙钟抖动（含两次回拨）下版本号仍严格递增", 5);
    Check(ms_never_back, "② ms 永不回退（回拨时沿用 last_ms）");
  }

  {
    fc::HlcClock c3;
    c3.device = "0000cccc";
    c3.last_ms = 500;
    c3.counter = fc::kHlcCounterMax;
    fc::Hlc before;
    before.ms = 500;
    before.counter = fc::kHlcCounterMax;
    before.device = "0000cccc";
    const fc::Hlc after = fc::HlcTick(&c3, 500);
    Check(after.ms == 501 && after.counter == 0,
          "③ counter 打满 0xFFFF ⇒ 推进 1ms 并归零");
    Check(fc::HlcLess(before, after), "③ counter 打满后新版本仍严格大于旧版本");
  }

  {
    fc::HlcClock c4;
    c4.device = "0000dddd";
    c4.last_ms = t0;
    c4.counter = 3;
    fc::Hlc remote;
    remote.ms = t0 + 100000;
    remote.counter = 0;
    remote.device = "ffffffff";
    fc::HlcObserve(&c4, remote);
    Check(c4.last_ms == remote.ms, "④ 收到更远的远端 ms 后本机 last_ms 抬升");
    const fc::Hlc after = fc::HlcTick(&c4, t0);
    Check(after.ms >= remote.ms, "④ 抬升后本机写入的 ms >= 远端 ms");
    Check(fc::HlcLess(remote, after), "④ 抬升后本机新版本严格大于远端版本");
  }

  {
    fc::HlcClock c5;
    c5.device = "0000eeee";
    c5.last_ms = t0;
    c5.counter = 3;
    fc::Hlc remote;
    remote.ms = t0;
    remote.counter = 9;
    remote.device = "ffffffff";
    fc::HlcObserve(&c5, remote);
    Check(c5.counter == 9, "④ 同 ms 下远端 counter 更大 ⇒ 本机 counter 抬到远端值");
    fc::Hlc older = remote;
    older.counter = 5;
    fc::HlcObserve(&c5, older);
    Check(c5.counter == 9, "④ 远端 counter 更小时本机 counter 不下调");
  }
}

// ===========================================================================
// [13] 条目合并（T202 ①–⑤）
// ===========================================================================

void TestMergeEntries() {
  std::printf("[13] 条目合并（T202 ①–⑤）\n");
  typedef std::vector<fc::Entry> Entries;

  const Entries a = {
      MakeEntry("a.txt", "v1", 100, 0, "0000aaaa"),
      MakeEntry("b.txt", "v1", 100, 0, "0000aaaa"),
      MakeEntry("c.txt", "v1", 100, 0, "0000aaaa"),
  };
  const Entries b = {
      MakeEntry("a.txt", "v2", 200, 0, "0000bbbb"),
      MakeEntry("d.txt", "v1", 200, 0, "0000bbbb"),
  };

  const Entries m = fc::MergeEntries(a, b);
  Check(m.size() == 4, "③ 异键取并集：3 + 2 ⇒ 4 个键");
  Check(KeysAscendingUnique(m), "输出恒按 key 升序且每键只出现一次");
  {
    bool a_took_v2 = false;
    for (size_t i = 0; i < m.size(); ++i) {
      if (m[i].key == "a.txt")
        a_took_v2 = (m[i].payload == "v2");
    }
    Check(a_took_v2, "① 同键不同版本取 HLC 更大者（a.txt 取 v2，ms 200 > 100）");
  }
  Check(fc::MergeEntries(b, a) == m, "⑤ 交换输入顺序结果相同（两端收敛）");

  {
    const Entries x = {MakeEntry("k.txt", "aaa", 100, 0, "0000aaaa")};
    const Entries y = {MakeEntry("k.txt", "zzz", 100, 0, "0000aaaa")};
    const Entries xy = fc::MergeEntries(x, y);
    const Entries yx = fc::MergeEntries(y, x);
    Check(xy.size() == 1, "② 同键同版本只保留一条");
    Check(xy == yx, "② 结果与输入顺序无关（确定性）");
    Check(xy.size() == 1 && xy[0].payload == "zzz",
          "② 兜底规则确定：payload 字典序大者胜");
  }

  Check(fc::MergeEntries(m, m) == m, "④ 幂等：merge(x,x) == x");
  Check(fc::MergeEntries(a, a) == a, "④ 幂等：对已升序集合仍成立");

  {
    const Entries c = {
        MakeEntry("b.txt", "v3", 300, 0, "0000cccc"),
        MakeEntry("e.txt", "v1", 50, 0, "0000cccc"),
    };
    const Entries left = fc::MergeEntries(fc::MergeEntries(a, b), c);
    const Entries right = fc::MergeEntries(a, fc::MergeEntries(b, c));
    Check(left == right, "④ 结合律：merge(merge(a,b),c) == merge(a,merge(b,c))");
    Check(KeysAscendingUnique(left), "④ 结合律结果仍是规范形态（升序唯一）");
  }

  {
    const Entries empty;
    Check(fc::MergeEntries(empty, empty).empty(), "空 ⊕ 空 = 空");
    Check(fc::MergeEntries(a, empty) == a, "a ⊕ 空 = a");
    Check(fc::MergeEntries(empty, a) == fc::MergeEntries(a, empty),
          "a ⊕ 空 == 空 ⊕ a");
  }

  {
    // ⑤ 两端各自「本地状态 + 对方全量」后收敛到同一份内容
    const Entries dev_a = fc::MergeEntries(a, b);
    const Entries dev_b = fc::MergeEntries(b, a);
    Check(dev_a == dev_b, "⑤ 两台设备交换全量后状态逐条一致");
  }
}

// ===========================================================================
// [14] 条目表序列化
// ===========================================================================

void TestEntriesWire() {
  std::printf("[14] 条目表序列化\n");
  const char* const kMagic = "fluxing-entries-v1";

  std::vector<fc::Entry> v;
  {
    fc::Entry e;
    e.key = "userdict/custom_phrase.txt";
    e.payload.assign("A\tB\nC\r\nD\0E", 9);  // TAB / LF / CR / NUL 全齐
    e.hlc = TestHlc();
    v.push_back(e);
  }
  {
    fc::Entry e;
    e.key = "userdict/other.txt";
    e.payload = "";  // 空 payload
    e.hlc.ms = 1;
    e.hlc.counter = 0;
    e.hlc.device = "0000aaaa";
    v.push_back(e);
  }

  const std::string wire = fc::SerializeEntries(v);
  Check(wire.compare(0, std::strlen(kMagic), kMagic) == 0,
        "首行为 magic「fluxing-entries-v1」");
  Check(!wire.empty() && wire[wire.size() - 1] == '\n', "每行（含末行）以 LF 结束");

  std::vector<fc::Entry> back;
  CheckErr(fc::ParseEntries(wire, &back), fc::EnvelopeError::kOk,
           "条目表往返解析 kOk");
  Check(back == v, "含 TAB/LF/CR/NUL 的 payload 与空 payload 都逐字节往返");

  {
    std::vector<fc::Entry> rev(v.rbegin(), v.rend());
    std::vector<fc::Entry> rev_back;
    CheckErr(fc::ParseEntries(fc::SerializeEntries(rev), &rev_back),
             fc::EnvelopeError::kOk, "逆序写入可解析");
    Check(rev_back == rev, "序列化不做隐式排序（顺序如实保留）");
  }

  {
    const std::string empty_wire = fc::SerializeEntries(std::vector<fc::Entry>());
    Check(empty_wire == std::string(kMagic) + "\n", "空表序列化为只有 magic 一行");
    std::vector<fc::Entry> empty_back;
    CheckErr(fc::ParseEntries(empty_wire, &empty_back), fc::EnvelopeError::kOk,
             "空表可解析");
    Check(empty_back.empty(), "空表解析为空");
  }

  {
    std::vector<fc::Entry> out;
    CheckErr(fc::ParseEntries("", &out), fc::EnvelopeError::kEmpty,
             "反例：空输入 ⇒ kEmpty");
  }
  {
    std::vector<fc::Entry> out;
    CheckErr(fc::ParseEntries("fluxing-entries-v2\n", &out),
             fc::EnvelopeError::kBadMagic, "反例：magic v2 ⇒ kBadMagic");
  }
  {
    std::vector<fc::Entry> out;
    CheckErr(fc::ParseEntries("fluxing-entries-v1\nonly\tthree\n", &out),
             fc::EnvelopeError::kBadField, "反例：字段不足 5 个 ⇒ kBadField");
  }
  {
    std::vector<fc::Entry> out;
    CheckErr(fc::ParseEntries("fluxing-entries-v1\n\n", &out),
             fc::EnvelopeError::kBadField, "反例：空行 ⇒ kBadField（不静默跳过）");
  }
  {
    std::vector<fc::Entry> out;
    CheckErr(fc::ParseEntries("fluxing-entries-v1\nk.txt\t!!!!\t1\t0\td\n", &out),
             fc::EnvelopeError::kBadBase64, "反例：payload 非 base64 ⇒ kBadBase64");
  }
  {
    std::vector<fc::Entry> out;
    CheckErr(fc::ParseEntries("fluxing-entries-v1\nk.txt\tAAAA\t1\t0\td\\q\n", &out),
             fc::EnvelopeError::kBadEscape, "反例：device 未知转义 ⇒ kBadEscape");
  }
  {
    std::vector<fc::Entry> out;
    CheckErr(fc::ParseEntries(
                 "fluxing-entries-v1\nk.txt\tAAAA\t99999999999999999999999\t0\td\n",
                 &out),
             fc::EnvelopeError::kBadNumber, "反例：ms 溢出 u64 ⇒ kBadNumber");
  }
}

}  // namespace

int main() {
  ::SetConsoleOutputCP(CP_UTF8);
  std::printf("==== TestFluxingCloud · spec 100 阶段 B（契约与客户端纯逻辑）====\n");
  std::printf("T201 信封编解码 / T202 条目合并 / T203 HLC 定序 / T204 共享生产头\n");

  TestHarnessAlive();
  TestWinHttpLinked();
  TestBCryptLinked();
  TestAdvApiLinked();
  TestBase64();
  TestItemType();
  TestEnvelopeRoundTrip();
  TestFieldEscape();
  TestEnvelopeTamper();
  TestEnvelopeWire();
  TestHlcOrder();
  TestHlcClock();
  TestMergeEntries();
  TestEntriesWire();

  std::printf("\nRESULT: PASS=%d FAIL=%d\n", g_pass, g_fail);
  if (g_fail == 0) {
    std::printf("VERDICT: ALL PASS\n");
  } else {
    std::printf("VERDICT: FAILED\n");
  }
  return g_fail == 0 ? 0 : 1;
}