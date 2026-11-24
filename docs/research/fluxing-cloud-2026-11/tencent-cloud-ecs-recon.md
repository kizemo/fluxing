# 腾讯云现机只读勘查报告（`124.223.115.201`）

- **勘查时间**：2026-11-24（北京时间），两轮 SSH 实测
- **通道来源**：`~/.ssh/config` 条目 `Host 124.223.115.201`（`User ubuntu` / `IdentityFile E:\Users\Duanyi\.ssh\id_ed25519` / `IdentitiesOnly yes` / `StrictHostKeyChecking accept-new`）——该 config 在 `E:\storedata-upload` **仓库之外**，仓库内**没有**任何腾讯云控制台/API 凭据
- **动作边界**：**全程只读**（信息类命令 + `grep` 配置目录 + `du`）。**未**重启服务、**未**改配置、**未**装包、**未**写文件、**未**动数据库；远端零写入
- **本报告用途**：为「用户量不大时是否复用现成腾讯云服务器自建 fluxing-sync」补机型 / 带宽 / nginx / 备案 / 负载证据（`docs/research/fluxing-cloud-2026-11/endpoint-reachability.md` 拍板条款 3 保留了「自定义端点口子供自建部署」，本报告是那条口子的可行性底稿）

---

## 一、结论（先给判定）

**可以承载，但不建议现在切；建议维持「先桶化止血撑住 Cloudflare Free，自建列为协议冻结后的独立批次（暂称 BR-⑤）」。**

三条理由，全部来自本次实测：

1. **基建确实现成，这条削弱了「自搭 Nginx + TLS + 进程守护成本高」的原有论据**：该机已有 nginx 1.18（系统托管）+ certbot 定时器 + 一个正在跑 Cloudflare Tunnel 的 systemd 服务 + Docker Compose 化的 Grafana/Loki/Prometheus 栈 + 宝塔面板 + 免密 sudo。**新增一个 server block 或一个容器的边际工作量接近于零。**
2. **但磁盘只剩 14 G（已用 89%）是最硬的近期约束**：`/dev/vda2` ext4 118 G 中已用 100 G（`/var` 单独占 57 G）。fluxing-sync 若自建，要新增 PostgreSQL schema + 密文对象存储 + 日志，**在不清理磁盘的前提下没有安全余量**；而 `/var` 57 G 里主要是宝塔与 Loki/日志数据，清理属变更动作、需另行授权。
3. **该机负载已经很重，云同步是新的「延迟敏感 + 高频小请求」型流量**：WordPress 站点 `jury.xin`（php-fpm 多进程 + mysqld）、6 个 `sales-bi-*` 容器、`evidence` 常驻进程、`promtail`/`loki`/`prometheus`/`grafana`/`alertmanager`、宝塔 `BT-Task`、腾讯云 `YDService`/`barad_agent`，**并且 `sales-bi-update.service` 已处于 `failed` 状态**（该机既有部署链路本身就带病）。把输入法云同步的可用性 SLA 挂在这台已经带病的多租户机器上，是**用一个新的可用性风险换掉一个额度风险**。

---

## 二、实测证据（两轮 SSH 取数，逐条可复核）

### 2.1 机器与系统

| 项 | 实测值 | 取数命令 |
|---|---|---|
| 主机名 | `VM-0-3-ubuntu` | `hostnamectl` |
| 虚拟化 | `kvm` / `Chassis: vm`（腾讯云 CVM 典型指纹） | `hostnamectl` |
| 内核 | `5.15.0-190-generic` x86_64（Ubuntu 22.04 系） | `hostnamectl` |
| vCPU | **4** | `nproc` |
| 内存 | total `7745 MB` / used `2641` / **available `4655`** | `free -m` |
| Swap | `2047 MB` 中 used `822` | `free -m` |
| 根盘 | `/dev/vda2 ext4 **118G used 100G avail 14G = 89%**` | `df -h` |
| 目录占用 | `/var 57G`、`/data 3.7G`、`/tmp 1.3G`、`/boot 259M`；`/opt/sales-bi 2.5G`、`/opt/node-v20.18.0 168M`、`/opt/chromium-extracted 594M`、`/opt/chromium.zip 174M`、`/opt/jst_etl 988K` | `du -sh` |
| 特权 | `sudo -n true` ⇒ **SUDO_NOPASSWD=yes**；`docker` 不带 sudo 报 permission denied ⇒ 必须 `sudo -n docker` | 实测 |

### 2.2 网络与可达性

- `eth0 = 10.0.0.3/22` ⇒ **公网 IP `124.223.115.201` 是 NAT / 弹性 IP，不绑定在网卡上**（自建方案里「域名直接 A 记录指到本机网卡」的直觉不成立，需走弹性 IP + 安全组）。
- 腾讯云元数据 `169.254.169.254` **两次探测均 `curl` 28 超时 ⇒ 元数据被挡**（脚本内无法自助读取实例规格、带宽、计费方式）。
- 出站探测 `https://api.ipify.org` ⇒ **`curl (7) Connection refused after 37 ms`**。⚠️ 这与「该机 `cloudflared` Tunnel 在线且 sales-bi 可访问」互相矛盾 ⇒ **出站 443 的真实可用性未判定**（很可能是该单一目标的问题，而非出站整体被封），换 egress 目标复测仍未做（属下一轮）。

### 2.3 监听面（`ss -lntp`）

`0.0.0.0:443 / 80 / 22 / 8080 / 5432`（5432 同时监听 IPv6）、`*:3306`、`0.0.0.0:9100`、`127.0.0.1:{9090, 9093, 3000(node pid 605541), 3001, 3002, 3100, 46341, 20241}`、`:631`（CUPS）、`127.0.0.53:53`（systemd-resolved）。

> ⚠️ **安全观察（与本项目无关，但已上报）**：`5432`（PostgreSQL）与 `3306`（MySQL）**直接对公网监听**，`9100` 同样公网可达。若腾讯云安全组未做收口，这是暴露面。本次勘查**只读**，未验证安全组规则、未尝试从外部连接。

### 2.4 服务与容器

- Docker 6 容器（全部 `sales-bi-*` 栈）：`sales-bi-core`（`127.0.0.1:3002->3000`，Up 4 weeks）、`sales-bi-grafana` 10.4.5（3001）、`sales-bi-prometheus` v2.53.0（9090）、`sales-bi-alertmanager` v0.27.0（9093）、`sales-bi-loki` 2.9.8（3100）、`sales-bi-promtail` 2.9.8。
- 常驻进程：`barad_agent`、`YDService`（腾讯云主机安全）、`promtail`、`loki`、`evidence`（pid 919103，uid 10001，MEM 2.6%）、`cloudflared`、`mysqld`、`BT-Task`（宝塔任务）、大量 `php-fpm(www)`。
- systemd：`sales-bi-evidence.service` **active**、`sales-bi-exporter.service` **active**、**`sales-bi-update.service` = loaded / failed**；enabled 含 `BT-FirewallServices`、`docker`、`containerd`、`cloudflared`、cups/lxd snaps。

### 2.5 反向代理与 TLS（自建方案最关心的一节）

- nginx **由系统托管**（`/etc/nginx`）；`/www/server/panel/vhost/nginx` **不存在**（宝塔装了但**没有用它托管 vhost**）；`/etc/caddy` **不存在**。
- `/etc/nginx/sites-enabled/default`：`listen 80 default_server` / `server_name _`。
- `/etc/nginx/sites-enabled/lawyer_wp.conf`：**承载 WordPress 站点 `jury.xin`** —— `server_name jury.xin www.jury.xin`，`:80 → 301 https://jury.xin$request_uri`，`:443 ssl` 证书 `/etc/letsencrypt/live/jury.xin/{fullchain,privkey}.pem`。另有 `server_name 124.223.115.201` 的 443 server，用 `/www/server/nginx/conf/ssl/ip_redirect/ip.crt|key`，行为是 301 到 `jury.xin`。
- `/etc/nginx/sites-enabled/sales-bi`：`listen 8080 default_server` + 大量 `proxy_pass http://127.0.0.1:3002`（Evidence 核心）与 `http://127.0.0.1:3000/<页面>`（`data-anomaly` / `net-sales-vs-refund` / `refund-by-category` / `refund-by-sku` …）。
- `certbot.timer` **active/enabled**；`/etc/letsencrypt` 含 `accounts archive cli.ini csr keys live` ⇒ **ACME 续签链路可用且自动化**（自建 fluxing-sync 域名可直接复用这条链路）。
- **Cloudflare Tunnel 已在这台机上运行**：`cloudflared.service`（enabled，`Description=Cloudflare Tunnel for Sales BI`，`ExecStart=/usr/bin/cloudflared tunnel --no-autoupdate --config /etc/cloudflared/config.yml run`，`Restart=always`）；`/etc/cloudflared/config.yml` **926 B 权限 0666（全局可写）**，同目录 `credentials.json`（179 B, 0600）+ 3 份 bak（含一份 `config.yml.bak-wrong-uuid`）；`/home/ubuntu/.cloudflared` 为空。

> ⚠️ **两个新发现的安全/健康问题**（属该机既有栈，与 fluxing-sync 无关，本次**未做任何修改**）：
> ① `/etc/cloudflared/config.yml` 权限 **0666 = 任何本机用户可改隧道路由**；
> ② `sales-bi-update.service` 处于 `failed` ⇒ 该机自动部署链路当前是坏的。

### 2.6 运行时与工具链

`nginx 1.18.0-0ubuntu14.20`；node 系统版 `v22.23.2` + `/opt/node-v20.18.0-linux-x64`；npm；python3；**psql client 14.24 / libpq5 14.24**；mysql；sqlite3；git；certbot 1.21.0；openssl；docker。**`pm2` 未装**（若自建，进程守护要么复用 systemd unit，要么补装 pm2）。
`/opt/sales-bi` 内含 `.env`（979 B）、`dbt_project.yml`、`evidence/`、`evidence-core/`、`.evidence-backup/`（75 个子目录）、`deploy/`、`docs/`、`.github/`、若干 `compare-teams-v5*.png`。

---

## 三、未取到的 4 项（本报告不得据此下结论）

1. **PostgreSQL server 实际大版本未证实** —— `find / -maxdepth 4 -name postgresql.conf` 无输出、`pg_lsclusters` 未安装 ⇒ 只知道 psql **客户端** 14.24。自建要选 PG 版本，这条必须补。
2. **实例规格里的带宽 / 计费方式 / 机型代号** —— 元数据被挡、腾讯云控制台与 API 凭据在 `E:\storedata-upload` **全仓零命中**（无 `SecretId`/`SecretKey`/`AKID`、无 `.pem`/`.ppk`）⇒ 只能 SSH，规格需用户从控制台确认。
3. **ICP 备案状态未知** —— 该机与远端配置里 `beian`/`备案` 关键词**零命中**，`jury.xin` 与 `aiec.fun` 均无记录可查 ⇒ 若用国内机对外提供 80/443 服务，备案是合规硬前置，**未证实 = 未通过**。
4. **出站 443 实际可用性未判定** —— 见 §2.2 的矛盾（`api.ipify.org` refused vs `cloudflared` 在线）。

---

## 四、对「自建 vs Cloudflare」论据的双向影响（如实登记，不据此改判）

**削弱原论据（对自建有利）**：

- 「自搭 Nginx + TLS + 进程守护 + 监控的工作量」被实测**大幅削弱**：nginx/certbot/systemd/docker/Prometheus-Grafana-Loki 全都在跑，加一个 vhost 或一个容器的边际成本接近零。
- 「自建能顺带解决境内可达性」有现成落点：该机已经在给 `jury.xin` 做 443 反代，弹性 IP + 已续签的 ACME 链路是通的。

**新增反证（对自建不利）**：

- **磁盘 89% / 剩 14 G**（`/var` 57 G）—— 自建前**必须**先清盘，而清盘是变更动作。
- **负载已重且带病**：WordPress + mysqld + php-fpm×N + PG + 6 容器 + cloudflared + evidence 常驻 + 宝塔 + 主机安全 agent，且 `sales-bi-update.service` **failed**。
- **`/etc/cloudflared/config.yml` 0666**：与 sales-bi 共用一台机时，fluxing-sync 的路由安全依赖别人的文件权限。
- **5432 / 3306 / 9100 公网监听**（安全组未证实）：把云同步数据库放到这台机 = 把新的数据主权面挂到已存在的暴露面上。
- **`server_seq` / D1 强耦合仍在**：现有服务端代码（`cloud/fluxing-sync/**`）绑 D1 与 R2，自建 = 一次**存储层重写**，不是换台机（这条与本次勘查无关，但它是自建的真实成本）。
- **备案与规格未证实**：两条都可能一票否决。

---

## 五、建议（口径）

1. **不改变当前路线**：先让 ADR 0077 桶化上线（服务端已实施 + 门禁 13 文件/164 用例全绿 + 已 push），把 D1 从「逐条索引」降为「设备 × 桶一行」，读侧止血收益到账后，再看 Cloudflare Free 是否还顶不住。
2. **本报告作为「BR-⑤ 自建」可行性底稿登记**，不进实现批次：待云同步**协议冻结**（老逐条端点下线、桶端点真机判据 AC-09 PASS）之后，再决定是否立项。
3. 若将来立项，**开工前置四项**：① 清盘并给出磁盘余量承诺；② 补证 PG server 版本与 `postgresql.conf` 位置；③ 备案与实例规格/带宽由用户在控制台确认；④ 修 `/etc/cloudflared/config.yml` 的 0666 权限并确认 `sales-bi-update.service` failed 的成因。
4. **与本项目无关但应尽快处理的两项安全观察**：cloudflared 配置全局可写、`5432`/`3306` 公网监听（需安全组核实）。
