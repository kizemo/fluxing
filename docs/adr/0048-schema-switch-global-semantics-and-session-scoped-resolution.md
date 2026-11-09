# ADR 0048 — 显式选方案 = 全局语义;会话解析统一走 _ResolveTargetSession

- 状态: 已采纳 (2026-11-09)
- 影响范围: `RimeWithWeasel/RimeWithWeasel.cpp`、`include/RimeWithWeasel.h`、`WeaselServer/WeaselServerApp.cpp`(调用点不变)
- 决策类型: 架构决策(前端会话/schema 状态层)+ 缺陷根因结论
- 关联: spec 045 批次 S 之后的新缺陷(用户报告「中文状态只能全拼, 切换到搜狗双拼仍全拼」)

## Context

用户报告: 在本分支做过编辑之后, 火流猩输入法在中文状态下**只能全拼输入**;
用快捷面板槽 0 切到「搜狗双拼」后, **仍然是全拼输入**(切了没反应)。

根因调查(证据全部落盘 `tmp/probe-double-pinyin.cpp` / `tmp/probe2-session-default.cpp` /
`tmp/dbl-*.txt` / `tmp/dbl-log*.txt`):

**数据层全部证伪** —— 不是 schema / 代数 / prism / 键位问题:
- 探针在 `double_pinyin_sogou` 下 `vs` → 中/种/重, `vl` → 摘, `vd` → 装 ⇒
  schema、代数规则、prism 二进制全部正常, 双拼本身可用;
- 仓库 `output/data/radical_pinyin.schema.yaml` 合法包含 `algebra_double_pinyin` 块
  (rime-prelude 上游布局, 非串写);
- 装机 `build/radical_pinyin.schema.yaml` 是 rime_radical_pinyin 包自带的**全拼** schema,
  与双拼方案无关, 不是污染源;
- 装机 `build/default.yaml` 的 `schema_list = [rime_ice, double_pinyin_sogou]` 正常;
- 服务端日志可见确实加载过 `double_pinyin_sogou.prism.bin`(double array1024) + `cn_en_sogou.txt`,
  并发生过 rime_ice → double_pinyin_sogou → rime_ice 的切换(2.4 秒内三次)。

**librime 侧行为是正常的, 不是缺陷**:
- `Service::CreateSession` 不设置任何 schema; `switcher_->RestoreSavedOptions()` 也不恢复 schema
  ⇒ **新建会话总是回落 `schema_list` 第一项 = rime_ice**(全拼)。这是 librime 的既定行为。

**真正的缺陷在我们自己的前端会话/schema 状态层**, 共 4 处:

1. `SwitchToNextSchema()` 用 `to_session_id(m_active_session)` 解析目标会话;
   `to_session_id` 用 `operator[]` 查 map, **键不存在时会插入一个假条目**;
   而 `m_active_session` 会被 `FocusOut` 清零(`RimeWithWeasel.cpp:627`)
   ⇒ `select_schema(0, ...)` 落到假会话, 返回 False, **全程静默无任何反馈**
   ⇒ 用户观感就是「点了没反应」。
2. `m_current_schema_id` 是**全局单值**, 却会被 `AddSession` 用新会话的默认方案(rime_ice)
   无条件覆盖 ⇒ 面板显示的方案名与实际会话脱节, 且「下一个方案」的起点可能算成当前方案本身。
3. 切换只作用于**单个会话**。用户显式选方案在商业输入法里是**全局语义**
   ⇒ A 窗口切到双拼后, B 窗口(新会话默认 rime_ice)仍然全拼 ⇒ 表现为「切了也没用」。
4. `SelectSchema(id)` 内部 `create_session()` 造一个新会话再切, 对正在输入的窗口没有可见效果
   (全仓无调用点, 属死代码但会误导后续维护)。
5. 放大器: `_ResolveTargetSession()` 的唯一会话回退条件是 `m_session_status_map.size() == 1`,
   而历史上的 `operator[]` 假条目(key=0)会把 size 变成 2 ⇒ 回退永久失效。

## Decision

1. **目标会话一律走 `_ResolveTargetSession(0)`**, 禁止在新代码里使用 `to_session_id()`
   (它带 `operator[]` 插入副作用)。既有 `to_session_id` 暂留, 但不得再用于会话解析。
2. **「当前方案」一律取目标会话的 live 值**(`rime_api->get_status` 读内存中的 `status.schema_id`,
   无文件 I/O, 符合 constitution P2), 全局 `m_current_schema_id` 降级为**兜底镜像**,
   且**不再被新会话覆盖**(只在为空时初始化)。
3. **用户显式选方案 = 全局语义**: 切换成功后**广播到所有活动会话**; 新建会话**继承用户已选方案**
   (仅当镜像值非空且与新会话默认方案不一致时才下发 `select_schema`, 避免每次聚焦编辑框都重载组件)。
4. **切换失败必须给用户可见的中文提示**(走既有「部署完成」提示同一条通道):
   区分「无活动会话」「无可用方案」「切换失败」三种原因, 不再静默。
   满足 spec 099 不变量: 用户可感知的动作必须可感知其结果。
5. per-session 方案追踪表 `m_session_schema_ids` 落库, 供「当前方案」回写与后续逐会话展示使用。

## Consequences

- 新会话默认仍是 rime_ice(librime 行为不变), 但**只要用户显式选过方案, 之后新建的会话一律继承该方案**,
  用户不再感知「换窗口就退回全拼」。
- 当前机器的方案集合为 `rime_ice` + `double_pinyin_sogou` 两项, 广播语义与商业输入法一致;
  若将来支持「按窗口独立选方案」的深需求, 需要另立 ADR 推翻本决策。
- `SelectSchema()` 不再创建会话 ⇒ 消除了「切了没反应」的第二条路径。
- 副作用: 面板显示的方案名现在反映**目标会话**的实际方案, 而不是最后一个新建会话的默认值。
- 未决(不在本 ADR 范围): `output/data/double_pinyin_mspy.schema.yaml` 标注「微软双拼」却使用搜狗键位,
  属次要数据缺陷, 后续单独处理。
