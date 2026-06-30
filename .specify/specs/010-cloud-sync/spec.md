# 010 · 火流猩输入法 v2 · 跨设备云同步（v2 P2 仅设计）

> 元 spec 004 拆分。v2 阶段 P2 仅交付 design；v2.1+ 实施。

## 0. 上下文

- v2 同步范围：用户词典 / 常用短语 / 自定义方案 / default.yaml / weasel.yaml。
- 不含：输入历史（隐私 + 数量巨大 + 无业务价值）、临时状态、OS 相关设置。
- 后端：Vercel + 邮箱 + WebAuthn Passkey（用户拍板 Q3=A）。

## 1. 产品视角

### 1.1 目标

让用户在多设备间共享"自造的输入法资产"（用户词典 / 短语 / 自定义方案 / 配置），不共享输入历史。

### 1.2 用户故事

- US7-A [P2]：用户在公司电脑 A 首次输入"测试短语"加入常用短语 + 用户词典。
- US7-B [P2]：用户在家里电脑 B 安装 Fluxing v2.1.0 → 登录同一账户 → 自动拉取 → 输入"ceshi"出现"测试短语"候选。
- US7-C [P2]：两台电脑离线时均能正常工作；恢复网络后自动同步。

### 1.3 验收

- Given Fluxing v2.1.0 已实施 spec 010，
- When 用户在 A 设备添加 3 条常用短语并输入停止 5s，
- Then A 设备的 WeaselServer 自动 push sync-package.json 到 Vercel。
- And 当 B 设备启动时拉取 → 3 条短语出现在 B 设备的 phrases.json。
- And B 设备的 use_count 仍为 0（push 时不带 use_count）。

## 2. 范围限制（v2 不实施）

- **不含输入历史**（spec 004 §1.1）。
- **不含临时状态**、**不含 OS 相关设置**。
- **频率限制**：每用户每日 push 100 次。
- **单端 sync-package 大小**：约 200KB - 2MB（压缩后 50-500KB）。

## 3. 依赖

- spec 009 短语 store（提供 phrases.json 读写 API）。
- spec 002 已落地的 `%LocalAppData%\Fluxing` 数据目录。
- spec 006 托盘面板"立即同步"按钮（v2 占位；v2.1 接入）。
- 后端：Vercel（已有部署经验）；webauthn-cpp 库（task 评估 vendored）。