# 联网搜索代理插件

`plugin/web-search` 是独立 xs/TCC 插件，首批接入博查和 z.ai。核心 xadmin 只增加通用的插件 HTTPS、异步路由和身份安全状态接口；没有 mdo 专用分支，也没有模型转发。

## 启用与配置

1. 使用本开发线的 xs 启动 xadmin，在后台「插件管理」启用 **联网搜索代理**。
2. 在后台菜单「联网搜索」设置两家的 API Key，或在启动 xs 的进程环境中设置 `BOCHA_API_KEY`、`ZAI_API_KEY`。环境变量优先，不会被后台文件配置覆盖；环境变量变更需要重新启动服务进程。
3. 在「插件管理 → 配置」设置默认平台、验证要求、每分钟/每天额度和超时。没有有效密钥的平台返回 503，不会悄悄使用其他平台。

默认策略：验证手机；博查为默认平台；每会员 5 次/分钟、100 次/天；全站 10,000 次/天；每次最多 10 条结果；总 I/O 超时 20 秒；全站最多 4 个上游请求，每会员最多 1 个。额度按 UTC 的自然分钟和自然日计算。

| 文件 | 内容 |
| --- | --- |
| `plugin/web-search/` | 可分发的插件源码、公开配置默认值、页面 |
| `options/plugin/web-search.json` | 平台开关、额度等不含密钥的策略 |
| `plugin_data/web-search/credentials.json` | 仅服务端读取的密钥，已加入 Git 忽略 |
| `db/plugin/web-search/plugin.db` | 会员/全站的时间窗口调用计数，已加入 Git 忽略 |

密钥文件对象只允许 `bocha`、`zai` 两个字符串字段。管理页面不回显密钥：留空保留，勾选删除清空；保存后新请求立即使用新密钥，已经开始的请求保留自己的快照。文件原子替换；POSIX 数据目录设为 0700、密钥文件为 0600，Windows 继承受控目录 ACL。部署时以专用服务用户运行，并确保这些路径不进入 Web 静态根、安装包或对外备份。

验证策略：`phone` 必须已验证手机；`any` 接受已验证手机或邮箱；`none` 仍必须会员登录。默认不会把管理员 Cookie 当作会员凭证。密钥管理另注册独立的后台权限「管理联网搜索」，未授权管理员不可读取或修改；非公开配置写入还校验管理员 CSRF token 和同源信息。

## 会员 API

沿用 `/api/v1`。支持会员 JWT `Authorization: Bearer <access_token>`，和同站会员 MSID Cookie；Cookie POST 必须携带 `X-CSRF-Token`。无效 Bearer 明确拒绝，不降级使用 Cookie。浏览器跨站使用需在身份服务 `cors_origins` 中配置确切来源。

### POST /api/v1/search

```json
{
  "query": "xs C 语言服务器框架",
  "provider": "bocha",
  "count": 5,
  "freshness": "oneWeek",
  "summary": true
}
```

| 字段 | 规则 |
| --- | --- |
| `query` | 必填，非空，最多 1024 个 UTF-8 字节，不接受控制字符 |
| `provider` | `bocha` / `zai`；省略使用后台默认平台 |
| `count` | 整数，1 至配置的 `max_results`；省略取 10 和上限中的较小值 |
| `freshness` | 博查支持 `noLimit` / `oneDay` / `oneWeek` / `oneMonth` / `oneYear`；省略 `noLimit` |
| `summary` | 布尔，默认 true；博查优先使用 summary，false 使用 snippet；z.ai 返回 content |

不接受客户端传入上游 URL、API Key、自定义请求头、搜索引擎或其他字段。请求 JSON 上限 8 KiB。

成功响应：

```json
{
  "code": 0,
  "message": "",
  "data": {
    "provider": "bocha",
    "request_id": "由服务端生成的32位随机十六进制标识",
    "count": 1,
    "truncated": false,
    "results": [{
      "title": "网页标题",
      "url": "https://example.com/page",
      "snippet": "网页摘要",
      "site": "网站名称",
      "published_at": "2026-10-04"
    }]
  }
}
```

结果按上游顺序输出，按 URL 精确去重，只保留 HTTP/HTTPS 网页。缺少有效标题/URL 的结果跳过；没有结果是成功的空数组。标题最多 512 字节、URL 2048 字节、摘要 2048 字节、网站名 256 字节、发布时间 64 字节；文本截断保持 UTF-8 字符边界。`truncated` 表示摘要或有效结果数被限制。发布时间保留平台原文，缺失时为空，不把抓取时间当发布时间。

结果属于外部网页数据，客户端显示应使用 textContent，Agent 不应把摘要当成系统指令。这个接口不生成答案，也不下载任意网页；后续网页阅读工具可以独立接入。

### GET /api/v1/search/providers

返回默认平台、结果上限以及两家平台的 `id`、`title`、`available`、`freshness_filter`。不包含平台密钥、服务器文件路径或原始配置。沿用同样的会员和联系方式验证规则。

### GET /api/v1/search/usage

返回本人 `minute_used`、`minute_limit`、`daily_used`、`daily_limit`、`daily_reset_at`（Unix 秒）。只展示自己的额度，不暴露其他会员或全站消费明细。

### 状态码

| HTTP / code | 意义 |
| --- | --- |
| 400 | 请求字段、长度、类型或平台能力不符合要求 |
| 401 | 未登录、凭证无效或账户不可用 |
| 403 | 联系方式未验证、权限不足、Cookie CSRF 验证失败 |
| 405 | 方法不支持 |
| 429 | 本站分钟/日额度或并发限制 |
| 502 | 上游非 2xx、业务错误、异常 JSON、TLS/网络错误；不回传供应商原始错误 |
| 503 | 平台未配置/被停用、工作线程或预算存储不可用 |
| 504 | 到达上游 I/O 总期限 |

429 不应立即重试；先查询个人 usage，分钟或日窗口重置后再发起。上游 429 归类为 502，与本站额度区分。没有自动重试、自动切换平台、对 uncertain 结果退款：所有已预留的转发尝试都会消耗额度，避免不确定计费和反复失败被滥用。请求无效、密钥缺失及并发拒绝不消耗额度。

## 平台契约与边界

- 博查：`POST https://api.bocha.cn/v1/web-search`，Bearer 鉴权，发送 query/freshness/summary/count，读取 `data.webPages.value`。参考 [官方 Web Search Skill 的 API 说明](https://github.com/Bocha-Labs/bocha-skills/blob/main/bocha-web-search/SKILL.md) 和 [官方 MCP 实现](https://github.com/Bocha-Labs/bocha-search-mcp)。
- z.ai：`POST https://api.z.ai/api/paas/v4/web_search`，Bearer 鉴权，发送 `search_engine=search-prime`、search_query/count/request_id，读取 search_result。参考 [官方 Web Search API](https://docs.z.ai/api-reference/tools/web-search)。使用 z.ai 平台密钥；不承诺国内 BigModel 的密钥可以通用。
- z.ai 文档对 search-prime 与某些筛选参数的支持描述不一致，本插件明确拒绝 z.ai 的非 noLimit freshness，避免静默忽略。结果数由本插件再次截断保证，未提供未经验证的站点/日期筛选能力。
- 上游响应最多 512 KiB，线缆数据另外限制为正文上限加 32 KiB，定长/chunked/关闭分帧均检查边界。HTTPS 使用系统 CA、验证域名，不跳过证书验证，不跟随重定向，不转发客户端 headers，不发送手机、邮箱或查询者账户资料。
- 配额在插件私有 SQLite 中用事务一次性预留会员分钟、会员日和全站日三种预算；失败回滚，成功落盘，插件/进程重启不重置。只保存聚合计数；不记录搜索关键词、网页内容、Bearer/JWT 或 API Key。过期窗口定期清理。
- 这是单个 xadmin 进程的插件，预算落在该实例的数据库。并发槽位在进程内；多实例部署需要先设计共享预算存储和全局并发协调，不能把多份私有数据库视为同一额度。

## 通用宿主能力

宿主版本提升为 4.1.0，ABI 结构仍为 v4。SDK 新增 `XAdmin_HttpPostJson`、`XAdmin_DeferRoute`、`XAdmin_MemberContactStatus`、管理员 CSRF helpers。搜索适配器代码不进入宿主。

`XAdmin_DeferRoute` 在原始路由中调用后立刻返回：宿主复制请求元数据/正文和 session，保留 server/stream 引用，在独立线程中执行回调，以 XS_TAKEOVER 接管一次响应，并最终 Close 连接。最多 16 个线程槽，已完成线程在后续请求回收，ServiceUnit 在释放插件、数据库和 TCC 代码前等待线程结束。动态参数路由暂不支持异步；回调只能使用 SDK 请求视图，不能借用原始 HTTP body reader。

回调先持有 xadmin 请求锁；`XAdmin_HttpPostJson` 用拥有的请求数据释放锁执行 I/O，完成后重新获得锁。不得跨调用保存活跃 SQLite statement/事务或配置借用指针。插件的活跃异步/I/O 计数在这些窗口阻止停用、重载和卸载；界面提示稍后重试。服务本身可安全发布新一代，旧请求完成后再销毁旧代。

同步调用 HTTPS 的普通网络回调仍可能阻塞 xs 监听线程；搜索使用异步路由专门解决这点。其他插件需要阻塞 I/O 时也应使用异步路由。API 不允许无限工作或脱离宿主生命周期的后台任务。

## 验证

```text
python tests/search_e2e.py
python tests/sms_unit.py
python tests/smoke.py --functional-only
```

搜索测试使用真实 xs/TCC 及隔离数据库，受控传输核对两个平台的请求契约，再用本地临时 CA 的 HTTPS 服务验证真实 TLS 转发、120 KiB 上游正文、UTF-8 截断和不可信证书拒绝。也验证 JWT/Cookie/CSRF、后台权限、手机/邮箱策略、分钟/日/全站预算、进程重启、密钥修复与环境优先级、慢请求时其他接口可用、客户端断开、插件重载及整个宿主换代。测试不会连接付费搜索平台，不修改根目录用户数据，不做压力或高负载测试。

2026-10-04：Windows 和 Linux 搜索端到端及原生 HTTPS 测试通过；共享传输的短信 192 项断言和 HTTPS 回归通过；Windows xadmin 功能 smoke 通过。实际厂商服务尚无可用凭据，**未完成真实博查/z.ai 的在线搜索或余额计费验证**。

部署到 `ai.xywhsoft.com` 时，需要目标服务器运行本开发线宿主、启用插件、配置服务端密钥，以及配置身份服务 public_origin 为 `https://ai.xywhsoft.com` 和正确 CORS 来源。本次仅修改本地 xadmin，未发布线上服务，也未改动 mdo 客户端。
