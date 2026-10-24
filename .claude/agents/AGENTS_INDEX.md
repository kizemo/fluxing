# rime_claude 项目级 Agent 索引

> 目录:`F:\soft\00selfmade\rime_claude\.claude\agents\`
> 项目:**Fluxing / 火流猩输入法**(rime/weasel 的 fork,品牌分支)
> 工作区:`F:\soft\00selfmade\rime_claude\`(cwd 启动 Claude Code 时以此为根)

**更新时间**: 2026-07-13
**总量**: 42 个 agent(其中项目原生 2 个 + 精选部署 40 个)

---

## 📦 Agent 来源

| 来源 | 数量 | 文件名前缀 |
|---|---|---|
| 项目原生(已存在)**绝对不覆盖** | 2 | `fluxing-bug-hunter.md`、`fluxing-build-engineer.md` |
| agency-agents/engineering 精选 | 14 | `engineering-*` |
| agency-agents/security 全量 | 10 | `security-*` |
| agency-agents/testing 全量 | 9 | `testing-*` |
| agency-agents/project-management 全量 | 7 | `project-*` |

> 项目级 agent 优先级**高于** `~/.claude/agents/` 全局;同名时优先使用此处的版本。

---

## 🏛️ 项目专属 Agent(rime_claude 上下文最强)

### 🐛 `fluxing-bug-hunter.md`(原生)
**特点**:深度绑定小狼毫/Fluxing 的 TSF + Win32 输入法上下文。能识别 WeaselServer、WeaselTSF、WeaselDeployer、librime 之间的 IPC 链;熟悉 leveldb user_dict 的 LOCK 语义;记得 `.specify/memory/lessons-learned.md` 中 15+ 个事故(L01–L15),能反向引用。
**适用**:`"activate fluxing-bug-hunter"`、`"@fluxing-bug-hunter"`。任何"TSF 不响应/WeaselServer 不退出/输入法卡死"等问题时,首选。

### 🔧 `fluxing-build-engineer.md`(原生)
**特点**:熟悉 `xbuild.bat`/`build.bat`/`xmake.lua`/`weasel.sln` 三套构建入口,知道何时跑哪个,知道 BOOST_ROOT 与 env.bat 的 release 标志(L10 §6)、NSIS BOM(L09)、silent install L13/L17 的 `/D=<path>` 与注册表预清理(L54)。
**适用**:改 `.props`、env.bat、`.nsi`、librime 子模块、提交前 build hygiene 检查。

---

## 🌟 engineering 精选 14 个(强相关 rime 栈)

> C++/Win32/TSF/librime 强相关,跨 IPC、LevelDB、NSIS、Python 测试脚本四个层。

| Agent | 用途 | 调用场景 |
|---|---|---|
| 👁️ `engineering-code-reviewer` | 通用代码评审(可维护性、安全、性能)| 每个 PR 提交前必跑 |
| 🚨 `engineering-incident-response-commander` | 事件响应、post-mortem 写作 | 跟 fluxing-bug-hunter 配合,L## 复盘 |
| 🧭 `engineering-codebase-onboarding-engineer` | 新会话/新人快速进入项目 | 新会话开篇,或新 spec 启动 |
| 🏛️ `engineering-software-architect` | 软件架构决策 review | 改 IPC、改构建、改 librime binding 前 |
| ✍️ `engineering-technical-writer` | CHANGELOG、ADR、文档写作 | 写 CHANGELOG.md、改 `docs/agents/` |
| 💻 `engineering-senior-developer` | 资深 C++/Python 开发 | 复杂 bug 修复 |
| ⚡ `engineering-rapid-prototyper` | 快速验证想法 | 试方案时不破坏主干 |
| 🖥️ `engineering-desktop-app-engineer` | Win32 GUI/TSF/输入法客户端 | WeaselUI/WeaselTSF 改 UI 时 |
| 🌐 `engineering-api-platform-engineer` | IPC/Pipe 通道 API 设计 | WeaselIPC 协议重构 |
| 🤖 `engineering-ai-engineer` | ML/Prompt 工程(略相关)| codebase-memory MCP 调优 |
| 🛠️ `engineering-devops-automator` | CI/构建流水线 | 改 `.github/workflows/`、`weasel.sln` |
| 🎯 `engineering-minimal-change-engineer` | 最小变更原则 | 修 bug 时避免引入新问题(L15) |
| 🧩 `engineering-multi-agent-systems-architect` | 多代理/多 IPC 进程协调 | WeaselServer ↔ TSF ↔ Deployer 三方同步 |
| 🔍 `engineering-search-relevance-engineer` | 检索/排序性能 | librime 候选排序优化 |

---

## 🛡️ security 全量 10 个

| Agent | 用途 |
|---|---|
| 🛡️ `security-application-security-engineer` | 应用安全代码评审 |
| 🔐 `security-authentication-engineer` | Auth/会话安全 |
| 🦠 `security-incident-responder` | 事故响应(Windows 字体/驱动层) |
| 🔬 `security-code-auditor` | 静态审计、SAST 工具解读 |
| 📜 `security-compliance-auditor` | 合规审计 |
| 💉 `security-penetration-tester` | 渗透测试 |
| 🔏 `security-secrets-reviewer` | secrets/env.bat 密钥检查 |
| 🪟 `security-windows-internals-expert` | Win32 内核/WinTcb 注册表/驱动 |
| 🔒 `security-incident-response-coordinator` | 协调多团队响应 |
| ⚔️ `security-threat-modeler` | 威胁建模 |

> **rime 强相关提示**:WeaselServer 注册表写 `HKLM\Software\Fluxing\Weasel`(silent install L54)+ librime-lua 插件注入面 — 用 `security-application-security-engineer`、`security-secrets-reviewer`、`security-penetration-tester` 三角验证。

---

## 🧪 testing 全量 9 个

| Agent | 用途 | rime 中的对应 |
|---|---|---|
| 🎯 `testing-test-architect` | 测试架构 | `test/TestWeaselIPC/*`、`TestDefaultHotkeys` |
| 🧪 `testing-test-automator` | 自动化测试 | `build-via-py.py` 流水 |
| 🐛 `testing-debugger` | 调试专项 | 跟 fluxing-bug-hunter 互补 |
| ⚡ `testing-performance-engineer` | 性能基准 | librime 候选排序耗时 |
| 🌐 `testing-api-tester` | IPC API 测试 | TestWeaselIPC |
| 🔒 `testing-security-tester` | 安全测试 | E2E 安全用例 |
| ♿ `testing-accessibility-tester` | 无障碍测试 | WeaselUI Section 508 |
| 🧬 `testing-test-data-engineer` | 测试数据 | ciku/librime 词典 |
| 🎬 `testing-workflow-optimizer` | 流水线优化 | GitHub Actions CI |

---

## 📋 project-management 全量 7 个

> rime_claude 当前有 11 个 active spec(`.specify/specs/NNN-*`)+ 15 个 lessons-learned + 大版本迭代 v0_19_0_10 → v0_19_0_22。这些 PM agent 适合做跨 spec 的排期/优先级。

| Agent | 用途 |
|---|---|
| 📋 `project-management-project-manager` | 项目总负责人 |
| 🎯 `project-management-sprint-prioritizer` | 排优先级 |
| 📅 `project-management-project-planner` | 计划制定 |
| 🚀 `project-management-product-launch-manager` | 发布管理(release 节奏)|
| 📊 `project-management-project-tracker` | 进度跟踪 |
| 💬 `project-management-stakeholder-communicator` | 利益相关方沟通 |
| 🔍 `project-management-risk-management-specialist` | 风险管理 |

---

## 🎯 rime_claude 典型调用范式

```bash
# 1) 报 bug 时自动调用 fluxing-bug-hunter
"TSF 在 alt+space 唤起 quick panel 时挂在 DPC 上,L08 上下文相关,trace 一下"

# 2) 提交前 hygiene
"切换到 fluxing-build-engineer 模式,跑 `xbuild.bat weasel` 看是否有警告"

# 3) PR review
"@engineering-code-reviewer 帮我看 WeaselTSF/WeaselTSF.cpp 的这次 diff"

# 4) 新 spec kickoff
"进入 engineering-codebase-onboarding-engineer 模式,我想开一个 NSIS 注册表清理的新 spec"

# 5) 安全 + 最小变更 双重保险
"先用 security-application-security-engineer 过一遍,再用 engineering-minimal-change-engineer 检查是否引入了新 bug"

# 6) 跨部门 release 复盘
"用 project-management-sprint-prioritizer 给我排一下当前 11 个 spec 的优先级"
```

---

## ⚠️ 与全局 `~/.claude/agents/` 的关系

- 全局共 259 个 agent(其中 `engineering-code-reviewer` 与此处同名但**内容不同**——全局版是通用,此处不存在同名歧义)。
- 加载顺序:Claude Code 启动时,**先加载 cwd 下的 `.claude/agents/`,再合并全局**。同名 agent 时项目级胜出。
- 现有全局同名 agent(均在全局 AGENTS_INDEX.md):`code-reviewer`(用户原始版)、`engineering-code-reviewer`(agency-agents 全局版)、`engineering-incident-response-commander` 等 — 在 rime_claude 项目里**优先用项目级**的同等角色。
- **唯一例外**:`engineering-software-architect`、`engineering-technical-writer` 这类**只有全局有的**,在 rime_claude 也会被自动加载(因为同名规则不触发,直接全局补齐)。

---

## 📋 附录:完整 42 条 Agent 索引

## 📋 完整索引(共 42 个 Agent,按 division 排序)

1. 🏗️ **Backend Architect** (`engineering-backend-architect.md`) — Senior backend architect specializing in scalable system design, database architecture, API development, and cloud infrastructure. Builds robust, secure, performant server-side applications and microservices
2. 👁️ **Code Reviewer** (`engineering-code-reviewer.md`) — Expert code reviewer who provides constructive, actionable feedback focused on correctness, maintainability, security, and performance — not style preferences.
3. 🧭 **Codebase Onboarding Engineer** (`engineering-codebase-onboarding-engineer.md`) — Expert developer onboarding specialist who helps new engineers understand unfamiliar codebases fast by reading source code, tracing code paths, and stating only facts grounded in the code.
4. 💻 **Desktop App Engineer** (`engineering-desktop-app-engineer.md`) — Expert desktop application engineer for Electron and Tauri — secure IPC and process isolation, code signing and notarization, auto-update pipelines, native OS integration, and resource-footprint discipline.
5. ⚙️ **DevOps Automator** (`engineering-devops-automator.md`) — Expert DevOps engineer specializing in infrastructure automation, CI/CD pipeline development, and cloud operations
6. 🚨 **Incident Response Commander** (`engineering-incident-response-commander.md`) — Expert incident commander specializing in production incident management, structured response coordination, post-mortem facilitation, SLO/SLI tracking, and on-call process design for reliable engineering organizations.
7. 🪡 **Minimal Change Engineer** (`engineering-minimal-change-engineer.md`) — Engineering specialist focused on minimum-viable diffs — fixes only what was asked, refuses scope creep, prefers three similar lines over a premature abstraction. The discipline that prevents bug-fix PRs from becoming 
8. 🕸️ **Multi-Agent Systems Architect** (`engineering-multi-agent-systems-architect.md`) — Systems architect specializing in the design, coordination, and governance of multi-agent AI pipelines — covering topology selection, context management, inter-agent trust, failure recovery, human-in-the-loop gating, a
9. 🧬 **Prompt Engineer** (`engineering-prompt-engineer.md`) — Specialist in crafting, testing, and systematically optimizing prompts for LLMs — turning vague instructions into reliable, production-grade AI behaviors.
10. ⚡ **Rapid Prototyper** (`engineering-rapid-prototyper.md`) — Specialized in ultra-fast proof-of-concept development and MVP creation using efficient tools and frameworks
11. 🔎 **Search Relevance Engineer** (`engineering-search-relevance-engineer.md`) — Expert search engineer for Elasticsearch and OpenSearch — index and analyzer design, BM25 query tuning, hybrid lexical+vector retrieval, and judgment-based relevance evaluation with nDCG and online experiments.
12. 💎 **Senior Developer** (`engineering-senior-developer.md`) — Premium implementation specialist - Masters Laravel/Livewire/FluxUI, advanced CSS, Three.js integration
13. 🏛️ **Software Architect** (`engineering-software-architect.md`) — Expert software architect specializing in system design, domain-driven design, architectural patterns, and technical decision-making for scalable, maintainable systems.
14. 📚 **Technical Writer** (`engineering-technical-writer.md`) — Expert technical writer specializing in developer documentation, API references, README files, and tutorials. Transforms complex engineering concepts into clear, accurate, and engaging docs that developers actually read 
15.  **fluxing-bug-hunter** (`fluxing-bug-hunter.md`) — Fluxing / 火流猩输入法 的 bug 猎人 + 修复者。在用户报告 bug、症状、或要求"检查代码/修 bug/排查为什么 XXX 不工作"时主动触发。基于系统性调试方法论 + 项目事故库 (L0
16.  **fluxing-build-engineer** (`fluxing-build-engineer.md`) — Fluxing / 火流猩输入法 的构建工程师。在用户要求"编译/构建/打包/跑测试/构建 installer/重编 rime 引擎/clean build"时主动触发。专精 MSVC v143 + xmake + msbuild + cmake + NSIS + libri
17. 🧪 **Experiment Tracker** (`project-management-experiment-tracker.md`) — Expert project manager specializing in experiment design, execution tracking, and data-driven decision making. Focused on managing A/B tests, feature experiments, and hypothesis validation through systematic experimentat
18. 📋 **Jira Workflow Steward** (`project-management-jira-workflow-steward.md`) — Expert delivery operations specialist who enforces Jira-linked Git workflows, traceable commits, structured pull requests, and release-safe branch strategy across software teams.
19. 📋 **Meeting Notes Specialist** (`project-management-meeting-notes-specialist.md`) — Extract structured decisions, action items, and open questions from meeting transcripts or rough notes into a clean 4-section summary.
20. 🐑 **Project Shepherd** (`project-management-project-shepherd.md`) — Expert project manager specializing in cross-functional project coordination, timeline management, and stakeholder alignment. Focused on shepherding projects from conception to completion while managing resources, risks,
21. 🏭 **Studio Operations** (`project-management-studio-operations.md`) — Expert operations manager specializing in day-to-day studio efficiency, process optimization, and resource coordination. Focused on ensuring smooth operations, maintaining productivity standards, and supporting all teams
22. 🎬 **Studio Producer** (`project-management-studio-producer.md`) — Senior strategic leader specializing in high-level creative and technical project orchestration, resource allocation, and multi-project portfolio management. Focused on aligning creative vision with business objectives w
23. 📝 **Senior Project Manager** (`project-manager-senior.md`) — Converts specs to tasks and remembers previous projects. Focused on realistic scope, no background processes, exact spec requirements
24. 🔐 **Application Security Engineer** (`security-appsec-engineer.md`) — AppSec specialist who secures the software development lifecycle through threat modeling, secure code review, SAST/DAST integration, and developer security education that makes secure code the default.
25. 🛡️ **Security Architect** (`security-architect.md`) — Expert security architect specializing in threat modeling, secure-by-design architecture, trust-boundary analysis, defense-in-depth, and risk-based security reviews across web, API, cloud-native, and distributed systems.
26. 🛡️ **Blockchain Security Auditor** (`security-blockchain-security-auditor.md`) — Expert smart contract security auditor specializing in vulnerability detection, formal verification, exploit analysis, and comprehensive audit report writing for DeFi protocols and blockchain applications.
27. ☁️ **Cloud Security Architect** (`security-cloud-security-architect.md`) — Cloud-native security specialist designing zero trust architectures, implementing defense-in-depth across AWS, Azure, and GCP, and securing infrastructure-as-code pipelines from day one.
28. 📋 **Compliance Auditor** (`security-compliance-auditor.md`) — Expert technical compliance auditor specializing in SOC 2, ISO 27001, HIPAA, and PCI-DSS audits — from readiness assessment through evidence collection to certification.
29. 🚨 **Incident Responder** (`security-incident-responder.md`) — Digital forensics and incident response specialist who leads breach investigations, contains active threats, coordinates crisis response, and writes post-mortems that prevent recurrence.
30. 🗡️ **Penetration Tester** (`security-penetration-tester.md`) — Offensive security specialist conducting authorized penetration tests, red team operations, and vulnerability assessments across networks, web applications, and cloud infrastructure.
31. 🛡️ **Senior SecOps Engineer** (`security-senior-secops.md`) — Defensive application security specialist who scans every code submission for secrets and sensitive data exposure before anything else, then implements or audits security controls following the organization's security st
32. 🎯 **Threat Detection Engineer** (`security-threat-detection-engineer.md`) — Expert detection engineer specializing in SIEM rule development, MITRE ATT&CK coverage mapping, threat hunting, alert tuning, and detection-as-code pipelines for security operations teams.
33. 🔍 **Threat Intelligence Analyst** (`security-threat-intelligence-analyst.md`) — Cyber threat intelligence specialist who tracks adversary groups, maps attack campaigns to MITRE ATT&CK, produces actionable intelligence reports, and builds detection rules that catch real threats.
34. ♿ **Accessibility Auditor** (`testing-accessibility-auditor.md`) — Expert accessibility specialist who audits interfaces against WCAG standards, tests with assistive technologies, and ensures inclusive design. Defaults to finding barriers — if it's not tested with a screen reader, it'
35. 🔌 **API Tester** (`testing-api-tester.md`) — Expert API testing specialist focused on comprehensive API validation, performance testing, and quality assurance across all systems and third-party integrations
36. 📸 **Evidence Collector** (`testing-evidence-collector.md`) — Screenshot-obsessed, fantasy-allergic QA specialist - Default to finding 3-5 issues, requires visual proof for everything
37. ⏱️ **Performance Benchmarker** (`testing-performance-benchmarker.md`) — Expert performance testing and optimization specialist focused on measuring, analyzing, and improving system performance across all applications and infrastructure
38. 🧐 **Reality Checker** (`testing-reality-checker.md`) — Stops fantasy approvals, evidence-based certification - Default to "NEEDS WORK", requires overwhelming proof for production readiness
39. 🎭 **Test Automation Engineer** (`testing-test-automation-engineer.md`) — Expert end-to-end test automation engineer for Playwright and Cypress — resilient selectors, flake elimination, isolated test data, CI parallelization, and trace-driven failure debugging.
40. 📋 **Test Results Analyzer** (`testing-test-results-analyzer.md`) — Expert test analysis specialist focused on comprehensive test result evaluation, quality metrics analysis, and actionable insight generation from testing activities
41. 🔧 **Tool Evaluator** (`testing-tool-evaluator.md`) — Expert technology assessment specialist focused on evaluating, testing, and recommending tools, software, and platforms for business use and productivity optimization
42. ⚡ **Workflow Optimizer** (`testing-workflow-optimizer.md`) — Expert process improvement specialist focused on analyzing, optimizing, and automating workflows across all business functions for maximum productivity and efficiency
