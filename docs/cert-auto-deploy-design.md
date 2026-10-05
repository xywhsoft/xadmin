# 证书自动部署功能设计方案

> 基础设施已就绪：xrt 的 xacme 扩展库（ACME RFC 8555 客户端，含 4 家 CA 预设 + 5 家 DNS 提供商）已编译进 xs.exe；
> xs 宿主已有 `tls_cert/tls_key/tls_ca` 配置字段和 SNI 多证书支持（`XS_TlsTableBuild` 按域名选择证书）。
> x-admin 侧零消费——本方案把这条链路接通。

## 一、架构总览

```
┌─ x-admin 管理后台 ─────────────────────────────────────────────┐
│  设置 → 证书管理                                                 │
│  ├── 账户配置（CA + 邮箱 + DNS 提供商凭证）                       │
│  ├── 域名列表（签发状态 / 到期时间 / 自动续签开关）                 │
│  ├── 手动签发 / 续签 / 撤销                                      │
│  └── 部署目标（xs.json 的 tls_cert/tls_key 写入 + 热重载通知）     │
├─ 计划任务（自动续签检测）                                          │
│  └── cron: 每 6h 扫描 → xrtAcmeStoreNeedRenew → xrtAcmeObtain    │
├─ x-acme 模块（modules/cert.h）                                   │
│  ├── 账户/域名/部署目标 CRUD（SQLite）                            │
│  ├── 签发引擎（调 xacme 的 xrtAcmeObtain / IssueStored）          │
│  ├── 证书存储（xacme store 格式 + x-admin 索引表）                 │
│  └── 部署引擎（写 PEM 文件 + 更新 xs.json + 触发 server reload）   │
├─ xrt xacme 扩展（已有，零改动）                                    │
│  ├── xrtAcmeObtain：签发+存储+续签一站式                           │
│  ├── xrtAcmeStoreSaveCert/LoadCert/NeedRenew                     │
│  └── DNS: alidns/cloudflare/tencent/aws/huawei                   │
└─ xs 宿主（已有，零改动）                                          │
   └── 启动时读 tls_cert/tls_key → XS_TlsTableBuild → SNI 按域名选证书  │
└──────────────────────────────────────────────────────────────────┘
```

## 二、数据模型

### 表 `cert_account`（ACME 账户）

```sql
CREATE TABLE IF NOT EXISTS cert_account (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,                    -- 账户别名
    directory_url TEXT NOT NULL,           -- CA 目录 URL（下拉选 4 家预设或自定义）
    contact_email TEXT NOT NULL DEFAULT '',
    dns_provider TEXT NOT NULL,            -- alidns / cloudflare / tencent / aws / huawei
    dns_credentials TEXT NOT NULL DEFAULT '{}',  -- JSON: {"access_key":"...","secret":"..."}（加密存储）
    eab_kid TEXT DEFAULT '',               -- ZeroSSL 等需 EAB
    eab_hmac_key TEXT DEFAULT '',
    store_root TEXT NOT NULL,              -- xacme store 目录（certs/<account_id>/）
    status TEXT DEFAULT 'active',
    create_time INTEGER NOT NULL,
    update_time INTEGER NOT NULL,
    isDelete INTEGER DEFAULT 0
);
```

### 表 `cert_domain`（签发的域名证书）

```sql
CREATE TABLE IF NOT EXISTS cert_domain (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    account_id INTEGER NOT NULL,           -- 归属账户
    primary_domain TEXT NOT NULL,          -- 主域名（CN）
    san_domains TEXT DEFAULT '',           -- SAN 列表（逗号分隔）
    status TEXT DEFAULT 'pending',         -- pending/issued/expired/revoked/error
    fullchain_path TEXT DEFAULT '',        -- 部署后的证书链路径
    key_path TEXT DEFAULT '',              -- 部署后的私钥路径
    not_before INTEGER DEFAULT 0,          -- 证书生效时间（µs）
    not_after INTEGER DEFAULT 0,           -- 证书到期时间（µs）
    auto_renew INTEGER DEFAULT 1,          -- 自动续签开关
    renewal_days INTEGER DEFAULT 30,       -- 剩余多少天时触发续签
    last_issue_time INTEGER DEFAULT 0,
    last_error TEXT DEFAULT '',
    create_time INTEGER NOT NULL,
    update_time INTEGER NOT NULL,
    isDelete INTEGER DEFAULT 0
);
```

### 表 `cert_deploy_target`（部署目标）

```sql
CREATE TABLE IF NOT EXISTS cert_deploy_target (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    domain_id INTEGER NOT NULL,
    target_type TEXT NOT NULL DEFAULT 'xs_server',  -- xs_server / file_copy
    -- xs_server 模式：
    server_name TEXT DEFAULT '',           -- xs.json 中的 server name
    host_name TEXT DEFAULT '',             -- xs.json 中的 host name
    cert_file TEXT DEFAULT '',             -- 证书链输出路径
    key_file TEXT DEFAULT '',              -- 私钥输出路径
    -- file_copy 模式（预留）：远程拷贝
    status TEXT DEFAULT 'pending',         -- pending/deployed/error
    last_deploy_time INTEGER DEFAULT 0,
    last_error TEXT DEFAULT '',
    create_time INTEGER NOT NULL,
    update_time INTEGER NOT NULL,
    isDelete INTEGER DEFAULT 0
);
```

### 表 `cert_issue_log`（签发/续签/部署日志）

```sql
CREATE TABLE IF NOT EXISTS cert_issue_log (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    domain_id INTEGER NOT NULL,
    action TEXT NOT NULL,                  -- issue / renew / revoke / deploy / error
    detail TEXT DEFAULT '',
    create_time INTEGER NOT NULL
);
```

## 三、路由设计

### 管理端（`/admin/cert/...`）

| 方法 | 路径 | 说明 |
|---|---|---|
| GET | `/admin/cert/accounts` | 账户列表（分页） |
| POST | `/admin/cert/account` | 创建账户 |
| PUT | `/admin/cert/account` | 更新账户 |
| DELETE | `/admin/cert/account?id=` | 删除账户（软删） |
| GET | `/admin/cert/domains` | 域名列表（含证书状态/到期时间） |
| POST | `/admin/cert/domain` | 添加域名 |
| PUT | `/admin/cert/domain` | 更新域名（自动续签开关等） |
| DELETE | `/admin/cert/domain?id=` | 删除域名 |
| POST | `/admin/cert/issue` | **手动签发** `{accountId, domain, sanDomains}` |
| POST | `/admin/cert/renew` | **手动续签** `{domainId}` |
| POST | `/admin/cert/revoke` | **撤销证书** `{domainId}` |
| GET | `/admin/cert/deploy/targets?domainId=` | 部署目标列表 |
| POST | `/admin/cert/deploy/target` | 添加部署目标 |
| PUT | `/admin/cert/deploy/target` | 更新部署目标 |
| DELETE | `/admin/cert/deploy/target?id=` | 删除部署目标 |
| POST | `/admin/cert/deploy/run` | **手动部署** `{domainId}` |
| GET | `/admin/cert/logs?domainId=&page=` | 操作日志 |
| GET | `/admin/view/cert` | 证书管理页面 |

### 计划任务（自动续签）

注册为 x-admin 内置计划任务（走 sched 体系）：
- 任务类型：`cert_auto_renew`
- 默认周期：每 6 小时
- 逻辑：遍历 `cert_domain WHERE auto_renew=1 AND status='issued'` → `xrtAcmeStoreNeedRenew(store_root, domain, renewal_days)` → 需要续签则调 `xrtAcmeObtain` → 成功后自动触发部署

## 四、核心流程

### 4.1 签发流程

```
POST /admin/cert/issue {accountId, domain, sanDomains}
    │
    ├─ 1. 查账户 → 构建 xacmeaccountconfig（directory_url + eab + email）
    ├─ 2. 构建 DNS provider（根据 dns_provider + dns_credentials）
    ├─ 3. 构建 xacmeobtainconfig（store_root + renewal_days）
    ├─ 4. 调 xrtAcmeObtain() —— 内部完成：
    │      账户注册/登录 → 生成 CSR → DNS-01 challenge → 等待传播 → 签发 → 存 store
    ├─ 5. 成功：更新 cert_domain（status=issued, not_before/after, last_issue_time）
    │      失败：status=error, last_error
    └─ 6. 自动触发部署（如有部署目标）
```

### 4.2 部署流程（xs_server 模式）

```
POST /admin/cert/deploy/run {domainId}
    │
    ├─ 1. 从 xacme store 读取证书链 + 私钥 PEM
    ├─ 2. 写入部署目标指定的文件路径（如 certs/xadmin/fullchain.pem + key.pem）
    ├─ 3. 更新 xs.json：在对应 server.host_default 写 tls_cert/tls_key 字段
    ├─ 4. 触发 xs server reload（POST /admin/tool/reload/server?name=<server>）
    │      → 新代加载新证书 → SNI 匹配新域名
    └─ 5. 更新 cert_deploy_target.status = deployed
```

### 4.3 自动续签流程（计划任务）

```
cron: cert_auto_renew (每 6h)
    │
    ├─ 遍历 cert_domain WHERE auto_renew=1 AND status='issued'
    ├─ 对每条：xrtAcmeStoreNeedRenew(store_root, primary_domain, renewal_days)
    ├─ 需要续签 → xrtAcmeObtain（同 4.1）→ 自动部署（同 4.2）
    └─ 记录日志到 cert_issue_log
```

## 五、UI 设计

### 证书管理页面（`page/cert/index.html`）

```
┌─────────────────────────────────────────────────────────────┐
│ 证书管理                                                      │
├─ Tab: 账户 | 证书 | 部署目标 | 操作日志                         │
├─                                                             │
│ [账户 Tab]                                                    │
│ ┌──────────────────────────────────────────────────────────┐│
│ │ 别名        CA           邮箱            DNS       状态   ││
│ │ ──────────────────────────────────────────────────────── ││
│ │ main       Let's Encrypt  admin@...     alidns    active ││
│ │ backup     ZeroSSL        ops@...       cloudflare active ││
│ │ [+ 新建账户]                                               ││
│ └──────────────────────────────────────────────────────────┘│
│                                                              │
│ [证书 Tab]                                                    │
│ ┌──────────────────────────────────────────────────────────┐│
│ │ 域名            状态    到期时间      剩余天数  自动续签   ││
│ │ ──────────────────────────────────────────────────────── ││
│ │ example.com    ✅已签发  2026-12-20     91      🔒 开     ││
│ │ *.example.com  ⚠ 即将到期 2026-10-15    25      🔒 开     ││
│ │ test.com       ❌ 失败   —             —       🔒 关     ││
│ │ [+ 签发新证书]  [批量续签即将到期的]                         ││
│ └──────────────────────────────────────────────────────────┘│
│                                                              │
│ [部署目标 Tab]（选中某证书后显示）                                │
│ ┌──────────────────────────────────────────────────────────┐│
│ │ 目标类型    服务器/主机    证书路径           状态          ││
│ │ xs_server  xadmin/xadmin  certs/xadmin/...    ✅ 已部署   ││
│ │ [+ 添加部署目标]  [立即部署]  [验证部署]                      ││
│ └──────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────┘
```

### 账户编辑弹窗（xform 属性面板）

- CA 目录 URL：下拉（Let's Encrypt / LE Staging / ZeroSSL / Buypass / 自定义）
- 联系邮箱：input
- DNS 提供商：下拉（alidns / cloudflare / tencent / aws / huawei）
- DNS 凭证：JSON textarea（access_key / secret / token 等按 provider 不同）
- EAB（ZeroSSL 需要）：kid + hmac_key

### 证书签发弹窗

- 选择账户：下拉
- 主域名：input（如 `example.com` 或 `*.example.com`）
- SAN 域名：textarea（逗号分隔）
- 自动续签：switch（默认开）
- 续签提前天数：number（默认 30）

## 六、安全设计

| 项 | 措施 |
|---|---|
| DNS 凭证存储 | AES 加密后存 `dns_credentials` 列（密钥从全局配置派生） |
| 私钥文件权限 | 写入后 `chmod 600`（Windows 下 ACL） |
| API 权限 | 所有 `/admin/cert/*` 路由 `bAdmin=true`（仅管理员） |
| 操作日志 | 签发/续签/撤销/部署全记 `cert_issue_log`（不可删） |
| EAB 凭证 | 加密存储，API 返回时脱敏 |
| 证书文件 | 部署到 `certs/` 目录（不入 git） |

## 七、技术要点

### 7.1 xacme 符号注入

xs.exe 已编译 xacme 扩展。需确认 TCC 符号表中是否已包含（`import_xrt.inc` 或类似注入表）。若未注入，需在构建时添加。

### 7.2 DNS 凭证加密

```c
// 加密：AES-256-CBC，密钥 = SHA256(global_secret + account_id)
// 解密：同密钥解密
// global_secret 存 options/global.json 的 cert_secret 字段（首次自动生成）
```

### 7.3 xs.json 写入

```c
// 读取 xs.json → 定位 services[i].host_default → 写 tls_cert/tls_key → 写回
// 路径相对于 xs.json 所在目录（如 "certs/xadmin/fullchain.pem"）
```

### 7.4 证书文件写入

```c
// 从 xacme store 读取 fullchain PEM + key PEM → 写入目标路径
// 目标路径：部署目标表的 cert_file / key_file 字段
// 原子写入：先写 .tmp → rename
```

### 7.5 计划任务注册

在 sched 体系注册内置任务：
```c
// Sched_Init 时检查 cert_auto_renew 任务是否存在，不存在则自动注册
// 类型: interval, 间隔: 6h, 动作: Cert_AutoRenewCheck()
```

## 八、门禁设计

| 测试 | 覆盖 |
|---|---|
| `tests/cert_e2e.py` | 账户 CRUD / 域名 CRUD / 部署目标 CRUD / 日志查询 |
| mock 签发 | xacme mock 模式（测试 CA）→ 签发→验证 store 产物→部署→文件存在 |
| 续签检测 | xrtAcmeStoreNeedRenew 触发→自动续签→证书更新 |
| 部署验证 | 部署后 xs.json tls_cert/tls_key 字段正确 / 证书文件存在且有效 |
| 权限 | 非 admin 访问 403 |
| 凭证加密 | 存储后非明文 / 读取时正确解密 |

## 九、实施排期

| 阶段 | 内容 | 量级 |
|---|---|---|
| P1 | 数据模型 + 账户/域名 CRUD + 管理页面 | ~800 行 |
| P2 | 签发引擎（xacme 接入 + DNS provider 构建） | ~400 行 |
| P3 | 部署引擎（文件写入 + xs.json 更新 + reload 通知） | ~300 行 |
| P4 | 自动续签（计划任务注册 + 检测逻辑） | ~150 行 |
| P5 | e2e 门禁 + GUI 验证 | ~200 行 |
| **合计** | | **~1850 行** |

## 十、依赖确认

| 依赖 | 状态 | 确认方式 |
|---|---|---|
| xacme 编译进 xs.exe | 已有 | `xs.exe --version` 列出 xacme |
| xrtAcmeObtain 符号可用 | 待确认 | TCC 符号表检查 |
| DNS provider 符号可用 | 待确认 | 同上 |
| xs TLS 热重载（证书不重启生效） | **无** | server reload 重建 TLS context（需验证） |

> **关键风险**：xs 宿主的 TLS context 在启动时构建（`XS_TlsLoadHost`），server reload 是否重建 TLS 表需验证。
> 若不重建，方案退化为：部署后需重启 xs 进程（或走 `tool/reload/xs` 级重启）。
