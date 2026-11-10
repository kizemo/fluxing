#pragma once
// spec 099 P0-7 (2026-11-04): 快捷设置栏位置持久化 —— 落盘格式 + 钳制算法
// (header-only 单一事实源)。
//
// 为什么单独成文件(与 include/FluxingAlwaysShow.h 同一条理由):位置读写与钳制
// 要被**两个**模块同时用到 ——
//   - WeaselServer/QuickPanelDialog.cpp:建窗时按持久化位置落面板、拖动结束时
//     写回、DPI 变化时把当前位置钳回可视区;
//   - test/TestQuickPanelDialog:判别性测试(跨重启保留、分辨率/DPI 变化后钳制、
//     多屏负坐标)。
// 逻辑若留在 .cpp 里就只能靠间接断言覆盖 ⇒ 下沉成共享头,生产代码与测试同源
// (constitution R7 的精神)。
//
// 关键约束:**不得污染 quickpanel.state 的单一真相源语义(R7)**。
// quickpanel.state 的格式是「单行 always_show=0|1」,其读盘判据
// (always_show::ResolveAlwaysShowFromFiles)依赖「第一行内容精确匹配」——
// 把位置塞进同一个文件会改变该判据的语义 ⇒ 位置必须落在**独立文件**
// quickpanel.position(ADR 0035)。
//
// 依赖面:<filesystem> + <fstream> + <optional> + <string> + <vector> +
// windows.h(经 FluxingUserPaths.h)+ user32(枚举显示器工作区)。

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

#include <FluxingUserPaths.h>

namespace fluxing {
namespace panel_position {

// ===== 类型 =====

// 屏幕矩形。**允许负值** —— 左侧副屏的工作区 left/top 是负数(多屏负坐标)。
// 不直接用 RECT:共享头不额外引 windows.h 之外的依赖,且 Rect 要能进
// std::vector 做纯函数计算。
struct Rect {
  int left;
  int top;
  int right;
  int bottom;
};

struct Point {
  int x;
  int y;
};

inline int RectWidth(const Rect& r) {
  return r.right - r.left;
}

inline int RectHeight(const Rect& r) {
  return r.bottom - r.top;
}

// inner 是否完整落在 outer 内(含边界)。
inline bool RectContains(const Rect& outer, const Rect& inner) {
  return inner.left >= outer.left && inner.top >= outer.top &&
         inner.right <= outer.right && inner.bottom <= outer.bottom;
}

// D0176 (2026-11-06):面板矩形与任一工作区是否有**正面积**交集。
// 与 RectContains 的区别:那是「完整可见」,这是「至少露出像素」。
// 用途 —— 落盘与复用路径的硬门槛:与所有工作区零交集的窗口,在用户眼里
// 等于不存在;此时任何「显示成功」都是假的成功。
// 注意:这里是**必要条件**判断,不做钳制;钳制仍走 ClampToVisibleArea。
inline bool RectIntersectsVisibleArea(int x,
                                      int y,
                                      int w,
                                      int h,
                                      const std::vector<Rect>& work_areas) {
  if (w <= 0 || h <= 0)
    return false;
  const Rect panel{x, y, x + w, y + h};
  for (const Rect& wa : work_areas) {
    const int iw = (panel.right < wa.right ? panel.right : wa.right) -
                   (panel.left > wa.left ? panel.left : wa.left);
    const int ih = (panel.bottom < wa.bottom ? panel.bottom : wa.bottom) -
                   (panel.top > wa.top ? panel.top : wa.top);
    if (iw > 0 && ih > 0)
      return true;
  }
  return false;
}

// ===== 落点 =====

// 位置文件名(不含目录)。**独立于 quickpanel.state**(ADR 0035)。
inline const wchar_t* PositionFileName() {
  return L"quickpanel.position";
}

// 默认落点:<用户数据目录>\quickpanel.position(与 quickpanel.state 同源目录,
// 但**不同文件** —— 见文件头 R7 说明)。
inline std::wstring DefaultPositionPath() {
  std::filesystem::path dir = ResolveFluxingUserDataPath();
  if (dir.empty())
    return L"quickpanel.position";
  return (dir / std::filesystem::path(PositionFileName())).wstring();
}

// 测试注入的落点(镜像 always_show::StatePathOverrideRef)。
inline std::wstring& PositionPathOverrideRef() {
  static std::wstring path;
  return path;
}

inline void SetPositionPathOverride(const std::wstring& path) {
  PositionPathOverrideRef() = path;
}

inline bool HasPositionPathOverride() {
  return !PositionPathOverrideRef().empty();
}

inline std::wstring ResolvePositionPath() {
  const std::wstring& ov = PositionPathOverrideRef();
  if (!ov.empty())
    return ov;
  return DefaultPositionPath();
}

// ===== 序列化 =====

// 文件内容:两行 ASCII "pos_x=<int>\npos_y=<int>\n"。
inline std::string FormatPosition(int x, int y) {
  return "pos_x=" + std::to_string(x) + "\npos_y=" + std::to_string(y) + "\n";
}

// ===== 位置记录(v2:带锚点) =====
//
// v1(旧):只有 pos_x / pos_y 两行 ⇒ 只有绝对坐标 ⇒ 分辨率变化时相对位置会漂。
// v2(2026-11-10 · 批次 V P0):追加「保存时所在工作区」+「保存时面板物理尺寸」
//   ⇒ 读回时可还原锚点(fx/fy)⇒ 分辨率变化后按比例重映射。
//   ★ 旧文件**仍然认**(缺锚点字段 ⇒ 按 v1 处理)⇒ 不需要迁移脚本,
//     首次拖动即自动升级为 v2。
struct PositionRecord {
  Point pos{0, 0};
  bool has_anchor = false;
  Rect work_area{0, 0, 0, 0};
  int panel_w = 0;
  int panel_h = 0;
};

// v2 文本:前两行与 v1 完全一致(旧解析器照样能读),其后追加锚点字段。
inline std::string FormatPositionRecord(const PositionRecord& rec) {
  std::string s = FormatPosition(rec.pos.x, rec.pos.y);
  if (rec.has_anchor) {
    s += "wa_left=" + std::to_string(rec.work_area.left) + "\n";
    s += "wa_top=" + std::to_string(rec.work_area.top) + "\n";
    s += "wa_right=" + std::to_string(rec.work_area.right) + "\n";
    s += "wa_bottom=" + std::to_string(rec.work_area.bottom) + "\n";
    s += "pw=" + std::to_string(rec.panel_w) + "\n";
    s += "ph=" + std::to_string(rec.panel_h) + "\n";
  }
  return s;
}

// 严格解析十进制整数:允许前导 '-',不允许空格 / 小数 / 尾巴多余字符。
// 超出 int 范围 ⇒ 不认(避免溢出进后续几何计算)。
inline std::optional<int> ParseIntStrict(const std::string& s) {
  if (s.empty())
    return std::nullopt;
  size_t i = 0;
  bool neg = false;
  if (s[0] == '-' || s[0] == '+') {
    neg = (s[0] == '-');
    i = 1;
  }
  if (i >= s.size())
    return std::nullopt;
  long long v = 0;
  const long long limit = neg ? 2147483648LL : 2147483647LL;
  for (; i < s.size(); ++i) {
    if (s[i] < '0' || s[i] > '9')
      return std::nullopt;
    v = v * 10 + (s[i] - '0');
    if (v > limit)
      return std::nullopt;
  }
  return std::optional<int>(static_cast<int>(neg ? -v : v));
}

// 坐标合理性上限:±100000 物理像素。远超任何真实多屏布局(4K 4 屏拼接也只到
// 几万像素),超界即视为文件损坏 ⇒ 不认(顺带杜绝 x + w 的 int 溢出)。
constexpr int kMaxCoordAbs = 100000;

// 解析位置文件内容。
//   - 必须**同时**有 pos_x 与 pos_y 两项才认(缺一即损坏);
//   - 重复键以**第一次**出现为准(损坏的尾巴不能覆盖掉好的头部);
//   - 其它行(注释/空白/未知键)忽略。
inline std::optional<PositionRecord> ParsePositionRecord(const std::string& raw) {
  std::optional<int> x;
  std::optional<int> y;
  std::optional<int> wa_left, wa_top, wa_right, wa_bottom, pw, ph;
  size_t pos = 0;
  while (pos <= raw.size()) {
    const size_t eol = raw.find('\n', pos);
    std::string line =
        raw.substr(pos, eol == std::string::npos ? std::string::npos : eol - pos);
    if (!line.empty() && line.back() == '\r')
      line.pop_back();  // 容忍 CRLF 写盘
    if (!line.empty()) {
      auto take = [&](const char* key, std::optional<int>& slot) {
        if (slot.has_value())
          return;
        const size_t klen = std::strlen(key);
        if (line.rfind(key, 0) != 0)
          return;
        auto v = ParseIntStrict(line.substr(klen));
        if (v.has_value() && *v <= kMaxCoordAbs && *v >= -kMaxCoordAbs)
          slot = v;
      };
      take("pos_x=", x);
      take("pos_y=", y);
      take("wa_left=", wa_left);
      take("wa_top=", wa_top);
      take("wa_right=", wa_right);
      take("wa_bottom=", wa_bottom);
      take("pw=", pw);
      take("ph=", ph);
    }
    if (eol == std::string::npos)
      break;
    pos = eol + 1;
  }
  if (!x.has_value() || !y.has_value())
    return std::nullopt;
  PositionRecord rec;
  rec.pos = Point{*x, *y};
  // 锚点字段必须**齐全**才认(缺一项 ⇒ 按 v1 绝对坐标处理)。
  if (wa_left.has_value() && wa_top.has_value() && wa_right.has_value() &&
      wa_bottom.has_value() && pw.has_value() && ph.has_value() &&
      *pw > 0 && *ph > 0 && *wa_right > *wa_left && *wa_bottom > *wa_top) {
    rec.has_anchor = true;
    rec.work_area = Rect{*wa_left, *wa_top, *wa_right, *wa_bottom};
    rec.panel_w = *pw;
    rec.panel_h = *ph;
  }
  return rec;
}

// 旧接口(只取绝对坐标)—— 保留给既有调用点与单测。
inline std::optional<Point> ParsePosition(const std::string& raw) {
  auto rec = ParsePositionRecord(raw);
  if (!rec.has_value())
    return std::nullopt;
  return rec->pos;
}

// ===== 读写 =====

// 读位置文件。文件缺失 / 打不开 / 内容损坏 ⇒ nullopt(调用方保持现状)。
inline std::optional<PositionRecord> ReadPositionRecordFile(
    const std::wstring& path) {
  if (path.empty())
    return std::nullopt;
  std::ifstream f(path, std::ios::binary);
  if (!f)
    return std::nullopt;
  const std::string raw((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());
  return ParsePositionRecord(raw);
}

// 旧接口(只取绝对坐标)。
inline std::optional<Point> ReadPositionFile(const std::wstring& path) {
  auto rec = ReadPositionRecordFile(path);
  if (!rec.has_value())
    return std::nullopt;
  return rec->pos;
}

// 写位置文件并**回读自证**(本仓铁律:0 字节坏文件比没文件更危险)。
// 写失败 / 回读不一致 ⇒ 删掉坏文件并返回 false。
// 与 always_show::WriteStateFile 同款(截断写 + 回读断言),不引入原子 rename。
inline bool WritePositionRecordFile(const std::wstring& path,
                                     const PositionRecord& rec) {
  if (path.empty())
    return false;
  const std::wstring dir = path.substr(0, path.find_last_of(L"\\/"));
  if (!dir.empty()) {
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
  }
  {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f)
      return false;
    f << FormatPositionRecord(rec);
    f.flush();
    if (!f)
      return false;
  }
  auto back = ReadPositionRecordFile(path);
  bool same = back.has_value() && back->pos.x == rec.pos.x &&
              back->pos.y == rec.pos.y && back->has_anchor == rec.has_anchor;
  if (same && rec.has_anchor) {
    same = back->work_area.left == rec.work_area.left &&
           back->work_area.top == rec.work_area.top &&
           back->work_area.right == rec.work_area.right &&
           back->work_area.bottom == rec.work_area.bottom &&
           back->panel_w == rec.panel_w && back->panel_h == rec.panel_h;
  }
  if (!same) {
    std::error_code ec;
    std::filesystem::remove(path, ec);
    return false;
  }
  return true;
}

// 旧接口(只写绝对坐标,无锚点 ⇒ 落盘文本与 v1 完全一致)。
inline bool WritePositionFile(const std::wstring& path, int x, int y) {
  PositionRecord rec;
  rec.pos = Point{x, y};
  return WritePositionRecordFile(path, rec);
}

// ===== 钳制 =====

// 把 (x,y) 钳制回「可见区」。
// work_areas = 所有显示器的工作区(屏幕坐标,**允许负值**)。
// 规则(ADR 0035):
//   1) 面板矩形完整落在**某个**工作区内 ⇒ 原样返回。
//      —— 这一条就是「多屏负坐标」用例:左侧副屏的工作区 left/top 是负数,
//         面板放在那里时位置必须**原样保留**,不能被当成「越界」拉走。
//   2) 否则 ⇒ 选「面板中心点最近、且尺寸容得下面板」的工作区,在其中钳制:
//        x ∈ [wa.left, wa.right - w],y ∈ [wa.top, wa.bottom - h]
//      —— 这一条就是「分辨率/DPI 变化后钳回可视区」用例。
//   3) 一个工作区都容不下面板 ⇒ (0,0)。
inline Point ClampToVisibleArea(int x,
                                int y,
                                int w,
                                int h,
                                const std::vector<Rect>& work_areas) {
  if (work_areas.empty())
    return Point{0, 0};
  if (w <= 0 || h <= 0)
    return Point{x, y};  // 尺寸非法 ⇒ 不动(防御,不该发生)
  // 规则 1:面板矩形完整落在**某个**工作区内 ⇒ 原样保留。
  //   这一条就是「多屏负坐标」用例 —— 左侧副屏的工作区 left/top 是负数,
  //   面板放在那里时位置必须原样保留,不能被当成「越界」拉走。
  const Rect panel{x, y, x + w, y + h};
  for (const Rect& wa : work_areas) {
    if (RectContains(wa, panel))
      return Point{x, y};
  }
  // 规则 2:否则 ⇒ 选「面板中心点最近、且尺寸容得下面板」的工作区,在其中钳制。
  //   这一条就是「分辨率/DPI 变化后钳回可视区」用例。
  const long long cx = static_cast<long long>(x) + (w / 2);
  const long long cy = static_cast<long long>(y) + (h / 2);
  const Rect* best = nullptr;
  long long best_dist = 0;
  for (const Rect& wa : work_areas) {
    if (RectWidth(wa) < w || RectHeight(wa) < h)
      continue;  // 容不下面板的工作区不参与候选
    const long long mx = static_cast<long long>(wa.left) + (RectWidth(wa) / 2);
    const long long my = static_cast<long long>(wa.top) + (RectHeight(wa) / 2);
    const long long dx = cx - mx;
    const long long dy = cy - my;
    const long long dist = dx * dx + dy * dy;
    if (!best || dist < best_dist) {
      best = &wa;
      best_dist = dist;
    }
  }
  // 规则 3:一个工作区都容不下面板 ⇒ (0,0)。
  if (!best)
    return Point{0, 0};
  int nx = x;
  int ny = y;
  const int lo_x = best->left;
  const int hi_x = best->right - w;  // 已保证 hi_x >= lo_x(该工作区容得下面板)
  if (nx < lo_x)
    nx = lo_x;
  if (nx > hi_x)
    nx = hi_x;
  const int lo_y = best->top;
  const int hi_y = best->bottom - h;
  if (ny < lo_y)
    ny = lo_y;
  if (ny > hi_y)
    ny = hi_y;
  return Point{nx, ny};
}

// ===== 位置锚点(2026-11-10 · 批次 V P0:分辨率变化时相对位置不漂) =====
//
// 病灶(用户真机复验,2026-11-10):面板位置按**绝对物理像素**落盘 ⇒ 分辨率
// 变化时工作区变大而坐标不变 ⇒ 面板相对屏幕**向左上偏移**(用户原话)。
// 根因不是 DPI、不是钳制算法(钳制只在出屏时兜底),而是「位置语义」:
// 绝对坐标 ≠ 相对位置。
//
// 修法:位置改按「相对所在工作区的比例」锚定(anchor)。分辨率变化时按比例
// 重映射 ⇒ 相对位置不变(右下角还是右下角,贴右边的间距同比放大)。
//
// 锚点定义(两轴对称):
//   fx = (x - wa.left) / (wa.right  - wa.left  - panel_w)
//   fy = (y - wa.top)  / (wa.bottom - wa.top   - panel_h)
// 分母 = 工作区**减去面板**后的可摆放空间 ⇒ 贴右边缘时,右边距随分辨率同比。
// 分母 <= 0(工作区恰好等于面板)⇒ 该轴比例取 0(贴左/贴上)。
// 比例一律钳到 [0,1] —— 越界的位置由 ClampToVisibleArea 负责,锚点不越界。

// 相对比例锚点。
struct Anchor {
  double fx;
  double fy;
};

// 四舍五入(.5 远离 0)—— 与 include/FluxingPanelDpi.h 的就近舍入口径一致。
inline int RoundHalfAwayFromZero(double v) {
  return static_cast<int>(v >= 0 ? v + 0.5 : v - 0.5);
}

// 由「绝对位置 + 所在工作区 + 面板尺寸」算出锚点。
inline Anchor ComputeAnchor(const Rect& wa,
                             int x,
                             int y,
                             int panel_w,
                             int panel_h) {
  Anchor a{0.0, 0.0};
  const int span_x = RectWidth(wa) - panel_w;
  const int span_y = RectHeight(wa) - panel_h;
  if (span_x > 0) {
    const double fx =
        static_cast<double>(x - wa.left) / static_cast<double>(span_x);
    a.fx = fx < 0.0 ? 0.0 : (fx > 1.0 ? 1.0 : fx);
  }
  if (span_y > 0) {
    const double fy =
        static_cast<double>(y - wa.top) / static_cast<double>(span_y);
    a.fy = fy < 0.0 ? 0.0 : (fy > 1.0 ? 1.0 : fy);
  }
  return a;
}

// 锚点 → 绝对位置(在给定工作区内)。
inline Point AnchorToPosition(const Rect& wa,
                               const Anchor& a,
                               int panel_w,
                               int panel_h) {
  const int span_x = RectWidth(wa) - panel_w;
  const int span_y = RectHeight(wa) - panel_h;
  int x = wa.left;
  int y = wa.top;
  if (span_x > 0)
    x = wa.left + RoundHalfAwayFromZero(a.fx * static_cast<double>(span_x));
  if (span_y > 0)
    y = wa.top + RoundHalfAwayFromZero(a.fy * static_cast<double>(span_y));
  return Point{x, y};
}

// 锚点状态:锚点 + 它所依据的工作区 + 当时的面板尺寸。
// work_area 用于**认显示器**(分辨率变化后按原点/中心找同一块屏)。
struct AnchorState {
  bool valid = false;
  Anchor anchor{0.0, 0.0};
  Rect work_area{0, 0, 0, 0};
  int panel_w = 0;
  int panel_h = 0;
};

// 找出与给定矩形**交集最大**的工作区(没有交集 ⇒ nullptr)。
inline const Rect* FindBestWorkArea(const std::vector<Rect>& areas,
                                    int x,
                                    int y,
                                    int w,
                                    int h) {
  const Rect* best = nullptr;
  long long best_inter = 0;
  for (const Rect& wa : areas) {
    const long long iw =
        (static_cast<long long>(x + w) < wa.right
             ? static_cast<long long>(x + w)
             : wa.right) -
        (x > wa.left ? x : wa.left);
    const long long ih =
        (static_cast<long long>(y + h) < wa.bottom
             ? static_cast<long long>(y + h)
             : wa.bottom) -
        (y > wa.top ? y : wa.top);
    const long long inter = (iw > 0 && ih > 0) ? iw * ih : 0;
    if (inter > best_inter) {
      best = &wa;
      best_inter = inter;
    }
  }
  return best;
}

// 由当前几何生成锚点状态(工作区列表为空 ⇒ 无效锚点)。
inline AnchorState MakeAnchorState(const std::vector<Rect>& areas,
                                    int x,
                                    int y,
                                    int panel_w,
                                    int panel_h) {
  AnchorState st;
  const Rect* wa = FindBestWorkArea(areas, x, y, panel_w, panel_h);
  if (!wa)
    return st;
  st.valid = true;
  st.anchor = ComputeAnchor(*wa, x, y, panel_w, panel_h);
  st.work_area = *wa;
  st.panel_w = panel_w;
  st.panel_h = panel_h;
  return st;
}

// 在当前工作区列表里找出「与参考工作区同一块屏」的那块:
//   1) 原点相同(同一块屏,只是分辨率变了)⇒ 直接命中;
//   2) 否则按中心距离最近 ⇒ 命中(屏幕被换位置/副屏顺序变化时的兜底);
//      ★ 因此「原屏被拔出」**不会**返回 nullptr:只要还有别的屏,面板就会被
//        重定位到最近的那块屏上 —— 这比「留在已不存在的屏上」对用户更有用
//        (面板不会消失在桌面外)。
//   3) 工作区列表为空 ⇒ nullptr(枚举失败,调用方走原路径)。
inline const Rect* FindMatchingWorkArea(const Rect& ref,
                                         const std::vector<Rect>& areas) {
  if (areas.empty())
    return nullptr;
  for (const Rect& wa : areas) {
    if (wa.left == ref.left && wa.top == ref.top)
      return &wa;
  }
  const long long rcx = static_cast<long long>(ref.left) + (RectWidth(ref) / 2);
  const long long rcy = static_cast<long long>(ref.top) + (RectHeight(ref) / 2);
  const Rect* best = nullptr;
  long long best_dist = 0;
  for (const Rect& wa : areas) {
    const long long cx = static_cast<long long>(wa.left) + (RectWidth(wa) / 2);
    const long long cy = static_cast<long long>(wa.top) + (RectHeight(wa) / 2);
    const long long dx = cx - rcx;
    const long long dy = cy - rcy;
    const long long dist = dx * dx + dy * dy;
    if (!best || dist < best_dist) {
      best = &wa;
      best_dist = dist;
    }
  }
  return best;
}

// 把锚点映射到「当前工作区」下的绝对位置。
//   - 锚点无效 / 工作区列表为空 / 原屏已拔出 ⇒ false(调用方走原路径)。
//   - 映射结果再钳回该工作区(比例取整可能差 1px)。
inline bool ResolveAnchoredPosition(const AnchorState& st,
                                     const std::vector<Rect>& areas,
                                     int panel_w,
                                     int panel_h,
                                     Point* out,
                                     Rect* out_work_area = nullptr) {
  if (!st.valid || areas.empty() || !out)
    return false;
  const Rect* wa = FindMatchingWorkArea(st.work_area, areas);
  if (!wa)
    return false;
  const Point p = AnchorToPosition(*wa, st.anchor, panel_w, panel_h);
  std::vector<Rect> one{*wa};
  const Point clamped = ClampToVisibleArea(p.x, p.y, panel_w, panel_h, one);
  *out = clamped;
  if (out_work_area)
    *out_work_area = *wa;
  return true;
}

// ===== 显示器工作区枚举 =====

// 枚举所有显示器的工作区。左侧/上方副屏的工作区坐标为负 —— 钳制必须能处理。
inline BOOL CALLBACK EnumWorkAreaProc(HMONITOR mon,
                                      HDC,
                                      LPRECT,
                                      LPARAM lp) {
  MONITORINFO mi = {};
  mi.cbSize = sizeof(mi);
  if (!GetMonitorInfoW(mon, &mi))
    return TRUE;
  std::vector<Rect>* out = reinterpret_cast<std::vector<Rect>*>(lp);
  out->push_back(
      Rect{mi.rcWork.left, mi.rcWork.top, mi.rcWork.right, mi.rcWork.bottom});
  return TRUE;
}

inline std::vector<Rect> EnumerateWorkAreas() {
  std::vector<Rect> out;
  ::EnumDisplayMonitors(nullptr, nullptr, EnumWorkAreaProc,
                        reinterpret_cast<LPARAM>(&out));
  return out;
}

// ===== 运行期缓存(启动期读一次,显示路径零 I/O) =====

// 已持久化的面板位置(屏幕坐标)。函数内 static ⇒ 跨 TU 只有一份。
inline Point& SavedPositionRef() {
  static Point p{0, 0};
  return p;
}

inline bool& HasSavedPositionRef() {
  static bool has = false;
  return has;
}

// 运行期锚点缓存(2026-11-10 · 批次 V P0):面板相对工作区的比例锚点。
// 落盘锚点(v2 文件)与运行期锚点共用这一个结构 —— 读盘时还原、放置时补算、
// 分辨率变化时按它重映射。函数内 static ⇒ 跨 TU 只有一份。
inline AnchorState& AnchorStateRef() {
  static AnchorState st;
  return st;
}

}  // namespace panel_position
}  // namespace fluxing
