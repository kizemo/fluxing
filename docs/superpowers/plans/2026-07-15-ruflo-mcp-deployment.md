# Ruflo MCP 部署实施计划

> **For agentic workers:** 本计划已由主 agent 在当前会话内按步骤执行；不启动 daemon，不执行 Ruflo full init。

**Goal:** 将 Ruflo `v3.30.2` 固定安装到本机，接入 Claude Desktop MCP，完成 stdio smoke test，并为 `rime_claude` 提供按需调用方法。

**Architecture:** Ruflo 使用仓库内 `ruflo/` wrapper，通过本机绝对路径 Node 启动 `@claude-flow/cli@3.30.2` 的 stdio MCP server。Claude Desktop 配置同时保留已安装的 `codebase-memory-mcp`，Ruflo 使用 `claude-flow` key；项目状态锚定到 `F:/soft/00selfmade/rime_claude`，不执行项目级 `init`、hooks 或 daemon。

**Tech Stack:** Node.js `v24.18.0`、npm、Ruflo `v3.30.2`、Claude Desktop JSON MCP 配置、MCP stdio JSON-RPC、Git for Windows、HTTP proxy `127.0.0.1:7897`。

## Global Constraints

- Node.js 必须满足 `>=20`；本机使用 `D:/Program Files/nodejs/node.exe`。
- Ruflo 固定为 Git tag `v3.30.2`，`@claude-flow/cli` 固定为 `3.30.2`。
- npm 只通过 `https://registry.npmjs.org` 和 `127.0.0.1:7897` 访问网络。
- 不执行仓库 `scripts/install.sh`。
- 不执行 `ruflo init`、`init --start-daemon`、`init --start-all`，不启动 Ruflo daemon。
- 不修改 `F:/soft/00selfmade/rime_claude` 中现有源码、submodule、`.claude` 文件或项目规则。
- 修改 Claude Desktop 配置前创建备份，只做最小 JSON 增量。
- smoke test 必须确认 stdout 为合法 JSON-RPC，stderr 与 stdout 分离。
- 不运行 `npm audit fix`，只记录 audit 结果。

---

### Task 1: 安装固定版本 Ruflo runtime

**Files:**
- Create: `C:/Users/Duanyi/Tools/ruflo`（Git checkout）
- Create: `C:/Users/Duanyi/Tools/ruflo/ruflo/node_modules`（npm 依赖）
- Modify: `C:/Users/Duanyi/Tools/ruflo/ruflo/package.json`（仅在安装时将 CLI dependency 固定为 `3.30.2`）

**Interfaces:**
- Produces: `C:/Users/Duanyi/Tools/ruflo/ruflo/bin/ruflo.js`
- Produces: local executable command `node .../ruflo/bin/ruflo.js --version`

- [ ] **Step 1: Check destination and clone tag**

Run:

```bash
if test -e "C:/Users/Duanyi/Tools/ruflo"; then
  printf 'ERROR: destination already exists\n' >&2
  exit 1
fi
mkdir -p "C:/Users/Duanyi/Tools"
HTTP_PROXY=http://127.0.0.1:7897 \
HTTPS_PROXY=http://127.0.0.1:7897 \
ALL_PROXY=http://127.0.0.1:7897 \
git clone --branch v3.30.2 --depth 1 https://github.com/ruvnet/ruflo.git "C:/Users/Duanyi/Tools/ruflo"
```

Expected: checkout completes at tag `v3.30.2`; if network or destination check fails, stop before writing any config.

- [ ] **Step 2: Install the published wrapper dependencies with exact CLI version**

Run from the cloned `ruflo` subpackage:

```bash
cd "C:/Users/Duanyi/Tools/ruflo/ruflo"
HTTP_PROXY=http://127.0.0.1:7897 \
HTTPS_PROXY=http://127.0.0.1:7897 \
ALL_PROXY=http://127.0.0.1:7897 \
npm install --registry=https://registry.npmjs.org --save-exact @claude-flow/cli@3.30.2
```

Expected: `node_modules/@claude-flow/cli/package.json` reports version `3.30.2`; no project `rime_claude` files are touched.

- [ ] **Step 3: Verify local CLI version before configuring Desktop**

Run:

```bash
"D:/Program Files/nodejs/node.exe" \
  "C:/Users/Duanyi/Tools/ruflo/ruflo/bin/ruflo.js" --version
```

Expected:

```text
ruflo v3.30.2
```

- [ ] **Step 4: Record dependency audit without auto-fixing**

Run:

```bash
cd "C:/Users/Duanyi/Tools/ruflo/ruflo"
npm audit --omit=dev --registry=https://registry.npmjs.org
```

Expected: audit may report dependency advisories; record the exact result, do not run `npm audit fix`.

---

### Task 2: Configure Claude Desktop MCP

**Files:**
- Read/backup: `C:/Users/Duanyi/AppData/Roaming/Claude/config.json`
- Modify: `C:/Users/Duanyi/AppData/Roaming/Claude/config.json`
- Create: `C:/Users/Duanyi/AppData/Roaming/Claude/config.json.ruflo-backup-20260715`

**Interfaces:**
- Consumes: local CLI from Task 1 and existing `codebase-memory-mcp.exe` at `C:/Users/Duanyi/.local/bin/codebase-memory-mcp.exe`.
- Produces: `mcpServers.claude-flow` with absolute Node path and project-scoped `CLAUDE_FLOW_CWD`.

- [ ] **Step 1: Back up the current Desktop config**

Run:

```bash
cp "C:/Users/Duanyi/AppData/Roaming/Claude/config.json" \
   "C:/Users/Duanyi/AppData/Roaming/Claude/config.json.ruflo-backup-20260715"
```

Expected: backup exists and has the same byte count as the original.

- [ ] **Step 2: Write the minimum MCP server entries**

The resulting JSON must preserve `locale` and contain exactly these two MCP entries:

```json
{
  "locale": "zh-CN",
  "mcpServers": {
    "codebase-memory-mcp": {
      "command": "C:/Users/Duanyi/.local/bin/codebase-memory-mcp.exe"
    },
    "claude-flow": {
      "command": "D:/Program Files/nodejs/node.exe",
      "args": [
        "C:/Users/Duanyi/Tools/ruflo/ruflo/bin/ruflo.js",
        "mcp",
        "start"
      ],
      "env": {
        "CLAUDE_FLOW_CWD": "F:/soft/00selfmade/rime_claude",
        "HTTP_PROXY": "http://127.0.0.1:7897",
        "HTTPS_PROXY": "http://127.0.0.1:7897",
        "ALL_PROXY": "http://127.0.0.1:7897"
      }
    }
  }
}
```

Expected: no `ruflo`-keyed duplicate; `claude-flow` is the only Ruflo server key.

- [ ] **Step 3: Validate JSON and file preservation**

Run:

```bash
python -c "import json; p=r'C:/Users/Duanyi/AppData/Roaming/Claude/config.json'; d=json.load(open(p, encoding='utf-8')); assert d['locale']=='zh-CN'; assert set(d['mcpServers'])=={'codebase-memory-mcp','claude-flow'}; assert d['mcpServers']['claude-flow']['args'][-2:]==['mcp','start']; print('Desktop MCP config: OK')"
```

Expected: `Desktop MCP config: OK`.

---

### Task 3: Run MCP stdio smoke test

**Files:**
- Create: `C:/Users/Duanyi/ruflo-mcp-smoke.py` (temporary test harness)
- Create: `C:/Users/Duanyi/ruflo-mcp-smoke-stdout.jsonl` (temporary captured stdout)
- Create: `C:/Users/Duanyi/ruflo-mcp-smoke-stderr.log` (temporary captured stderr)

**Interfaces:**
- Consumes: `node .../ruflo/bin/ruflo.js mcp start`.
- Produces: parsed MCP `initialize` response and `tools/list` response.

- [ ] **Step 1: Send MCP JSON-RPC handshake**

Run a Python subprocess harness that sends these newline-delimited messages:

```json
{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"ruflo-smoke","version":"1.0.0"}}}
{"jsonrpc":"2.0","method":"notifications/initialized","params":{}}
{"jsonrpc":"2.0","id":2,"method":"tools/list","params":{}}
```

The harness must use the absolute Node path and `cwd=F:/soft/00selfmade/rime_claude`, parse every stdout line with `json.loads`, and redirect stderr separately.

- [ ] **Step 2: Verify protocol invariants**

Expected:

- response `id=1` contains `result.serverInfo` and `result.capabilities.tools`;
- response `id=2` contains `result.tools` as an array;
- every non-empty stdout line is valid JSON;
- no `Loading model`, npm warning, or human-readable log appears on stdout;
- process exits cleanly after stdin closes.

- [ ] **Step 3: Verify no daemon was started**

Run:

```bash
"D:/Program Files/nodejs/node.exe" \
  "C:/Users/Duanyi/Tools/ruflo/ruflo/bin/ruflo.js" daemon status --all
```

Expected: no Ruflo daemon owned by this deployment is running. Do not run `daemon start`.

---

### Task 4: Provide rime_claude on-demand Ruflo workflow

**Files:**
- No source or project configuration changes.
- Output: user-facing operating guide in the final response.

**Interfaces:**
- Consumes: `claude-flow` MCP server and the existing project `CLAUDE.md`, `CONTEXT.md`, `AGENTS.md`, constitution, and codebase-memory index.
- Produces: repeatable prompt recipes for read-only reconnaissance, impact analysis, implementation review, and verification.

- [ ] **Step 1: Define the default invocation boundary**

Use Ruflo only on demand through Claude Desktop/Claude Code MCP. Do not run `ruflo init`, `init --full`, `init --start-all`, or daemon commands. Keep the existing project agents/skills/rules as the authority.

- [ ] **Step 2: Define a read-only reconnaissance prompt**

Use:

```text
先不要修改代码。

请在 rime_claude 中完成只读侦察：
1. 阅读 CLAUDE.md、CONTEXT.md、AGENTS.md 和 .specify/memory/constitution.md；
2. 使用 codebase-memory 查找目标符号的定义、调用者、被调用者和相关测试；
3. 检查当前 git status -sb 与 git diff --stat；
4. 输出“已确认 / 推测 / 待验证”；
5. 列出最小影响文件和验证命令。

禁止：修改代码、修改 librime/、启动 daemon、运行安装器或改变用户数据路径。
```

- [ ] **Step 3: Define a one-writer implementation handoff**

Only after the reconnaissance report is accepted, use one implementation agent with explicit intent, acceptance criteria, exact files, and project constraints. Keep independent review agents read-only and run project tests after implementation.

---

### Task 5: Final verification and handoff

**Files:**
- Read-only verification of all paths above.
- No changes to `F:/soft/00selfmade/rime_claude` source or existing `.claude` configuration.

- [ ] **Step 1: Confirm installed version, MCP config, and smoke output**

Run the version command, JSON validation, and smoke harness again after Desktop config is written.

- [ ] **Step 2: Confirm project cleanliness boundary**

Run:

```bash
git -C "F:/soft/00selfmade/rime_claude" status --short --branch
```

Expected: only the pre-existing project changes plus any explicitly documented deployment artifacts; no Ruflo-generated `.claude-flow`, `CLAUDE.md`, `.mcp.json`, or source changes.

- [ ] **Step 3: Report exact result**

Report installed tag, CLI version, Desktop config path, smoke-test result, audit result, daemon status, and the on-demand prompt method. Do not claim Desktop UI availability until the application is restarted and the MCP server is visible.
