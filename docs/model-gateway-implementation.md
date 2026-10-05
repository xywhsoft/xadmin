# 模型网关与计费插件实施记录

目标：原生 Chat Completions、Responses、Anthropic Messages 转发；模型目录；
服务端密钥；高精度预付费、会员额度、用量统计。部署目标为 home/host/xywhsoft_ai，
仅本地联调，不发布到线上。每个阶段独立 Git 提交，不执行压力或高负载测试。

## 顺序与验收

1. 通用流式 HTTP：增量 HTTP 分帧、受验证 TLS、显式 HTTP 配置、背压、取消、
   头/空闲/总超时、请求与插件生命周期。锁外网络等待，锁内插件回调。
2. billing：定点金额、唯一账本、余额迁移、额度批次、冻结与幂等结算、退款、
   套餐订阅。收费操作在 billing 单库事务中完成。
3. model-gateway：渠道与模型、价格版本、三种原生协议、SSE 增量计量、调用配额、
   用户归属、异常与重启恢复。无法确认上游接受状态时不自动重试。
4. 原生管理及用户页面：配置、密钥、套餐、余额与流水、用量与请求状态。
5. 正常与故障路径功能测试；部署本站、真实免费模型小额联调、保留本地服务。

自动支付需要支付平台配置，后续以独立适配器接入；本批先提供后台充值和退款。
mdo 模型选择界面接入属于客户端后续工作，本批交付它所需的服务端 API。

## 数据约束

- 金额为整数微元（1 元 = 1,000,000），报价按每百万 token 的微元保存；
  整次调用统一舍入。未知用量不能伪装为零。
- 一个用户只有一个可写余额事实源。启用 billing 后基础余额操作走服务接口；
  旧分余额一次性迁移。关闭服务不得静默回退到陈旧余额。
- 请求唯一编号与幂等键；计费结算可重试，模型生成不自动重试。
- 赠送额度保留来源与期限；退款回原来源。现金、冻结和消费流水禁止直接编辑。
- 价格、渠道与权益在请求开始时固定。密钥仅私有文件/环境变量，后台不可回显。
- 默认不保存提示词与回答；日志不记录 Authorization、密码或上游密钥。
- 暂不开放需要额外收费但没有计量能力的上游工具、音频和生成图片能力。

## 流式 SDK（host 4.4，ABI 4）

`XAdmin_HttpStream` 仅允许 deferred route 使用，复制请求线缆数据；HTTP/1
头与正文由 xrt 严格增量解析。回调视图仅在本次回调有效，回调在应用锁内执行。
最多 1 MiB 请求、32 MiB 响应、64 KiB 解析缓冲。不解压、不跟随重定向、不重试。
`allow_http` 只能来自可信服务器配置，HTTPS 从不跳过 CA/主机名验证。

`XAdmin_StreamBegin/Write/Finish` 负责 chunked 响应。每个写块有界并排空，
失败/取消 abort，不发送成功结束块。没有显式完成的流在 callback 返回时中断。
deferred job 延长 socket/server/plugin 生命周期；禁用/重载在在途 I/O 时被拒绝。

## 已完成的阶段验证

- 流式 SDK：真实 xs/TCC + 本地 chunked 上游，首块即时转发、跨块 UTF-8、
  上游错误状态、截断消息和空闲取消通过 `tests/stream_e2e.py`。
- 计费：`tests/billing_e2e.py` 覆盖旧余额迁移、分/微元桥接、幂等调整、
  冻结与余额不足回滚、微额结算、退款、赠送额度过期、待核实到期释放、
  会员周期及提前续期、重启恢复、停用后拒绝回退旧余额、账本不可改写。
- 原功能：`tests/smoke.py --functional-only` 全部通过，未执行并行负载或批量门禁。

会员周期额度按访问/新请求惰性发放；错过的历史周期不补发已经过期的额度。
同套餐续期从原到期时刻开始；更换套餐停止原套餐未来发放，不撤销已获额度。
待核实请求到期在访问/新请求时释放，默认 1 小时，平台承担无法核实的成本。

## 当前实现与文件边界

`plugin/billing` 拥有唯一可写钱包、赠送额度、会员和结算账本；
`plugin/model-gateway` 拥有模型与渠道配置、不可变报价版本、请求收据和统计。
二者通过 `plugin_sdk/billing_service.h` 的版本化服务协作。
`plugin_sdk/plugin_support.h` 只提供有界输入、SQL 和 JSON 边界工具。

网关拆分为：`catalog.c` 配置与快照；`credentials.c` 私有密钥；
`policy.c` 原生请求校验与结构化幂等指纹；`metering.c` 增量用量解析；
`requests.c` 准入、收据和恢复；`proxy.c` 网络生命周期；
`routes.c` 用户 API；`admin.c` 管理 API。管理及用户页面是原生 HTML/CSS/JS，
运行不依赖 Node、npm 或其他后台服务。

私有库版本为 2，拒绝打开更高版本；v1 到 v2 增加持久结算变更及消费者游标。
资金变化和结算变更写入同一事务。内存事件负责即时更新，持久变更负责补偿网关
停用期间的退款等操作；读取变更返回最新资金事实，不让旧事件覆盖新退款。

## 用户 API

以下接口要求有效前台用户 JWT（`Authorization: Bearer ...`）或同站会员 cookie。
cookie 写请求仍受主机 CSRF 保护。身份只取已验证 session，不接受请求内指定用户。

| 方法与路径 | 用途 |
| --- | --- |
| GET `/api/v1/ai/models` | OpenAI 形状的可调用模型列表 |
| GET `/api/v1/ai/catalog` | 名称、能力、协议、上下文/输出限制、报价、价格版本、会员可用性 |
| POST `/api/v1/ai/chat/completions` | 原生 Chat Completions；支持 JSON 和 SSE |
| POST `/api/v1/ai/responses` | 原生 Responses；支持 JSON 和 SSE |
| POST `/api/v1/ai/messages` | 原生 Anthropic Messages；支持 JSON 和 SSE |
| GET `/api/v1/ai/requests/{id}` | 本人请求收据、报价快照、用量、预留、实扣、退款和状态 |
| GET `/api/v1/ai/usage?days=30` | 1–90 天的本人总览、模型分组和最近 100 次请求 |
| GET `/api/v1/billing/account` | 可用现金/额度、冻结、会员权益 |
| GET `/api/v1/billing/transactions` | 最近 100 条本人资金流水 |
| GET `/api/v1/billing/credits` | 本人额度批次及期限 |
| GET `/api/v1/billing/plans` | 可开通套餐 |
| GET `/api/v1/billing/subscriptions` | 本人会员期记录 |

原生生成响应不包裹 xadmin `code/data`，错误为 `error` 对象；目录、收据和统计
使用 xadmin `code/message/data`。成功响应头 `X-Request-Id` 是网关收据编号。
标准 CORS 预检允许 `Idempotency-Key`，流式响应公开该收据头。

建议每次用户发起的生成使用新 `Idempotency-Key`（ASCII 字母、数字、`_.-`，
最多 96 字符）。同用户同键同请求再次提交返回 409 和原收据地址，不重新执行；
同键不同请求返回 422。JSON 对象键顺序与空白不改变指纹，数组顺序和标量类型
仍有意义。没有提供键时服务器生成操作编号，但客户端失联后无法按原键重试查询。

可附加 `xadmin_expected_price_version` 防止选择模型后价格变化；不匹配返回 409。
可附加 `xadmin_metadata: {project_id, session_id, task_id}`，仅作有界标签关联，
不能改变身份、权限和价格；这些字段不会发送给供应商。

## 管理与配置操作

启用顺序为 `billing` → `model-gateway`。后台菜单提供「账户与计费」和「模型网关」。
对应权限为 `billing.manage`、`model-gateway.manage`，管理写请求要求同源与 CSRF。

1. 渠道页配置协议、**完整请求 URL**、认证类型、超时与并发上限；
   密钥独立写入，不包含在普通配置或模型目录中。认证支持 Bearer、x-api-key、
   api-key 和 none。HTTPS 始终校验证书；HTTP 必须显式开启。
2. 模型页配置逻辑标识、上游模型名、上下文/输出上限、工具/视觉/思考能力、
   协议路由和销售/成本报价。逻辑标识不能含 `/`，供应商模型名可以。
   协议由原生路由决定，不将 Responses 或 Messages 强行转换成 Chat。
3. 报价表五项为普通输入、缓存读取、5 分钟缓存写入、1 小时缓存写入、输出。
   后台输入单位为「元 / 百万 token」，API 保存为「微元 / 百万 token」。
   成本未配置为 `null`，不能伪装成零。成本是配置报价计算的估计，非供应商账单。
4. 账户页按会员 ID 调整余额、发放额度、开通/续期/终止会员；可以全额退款，
   或按依据核实待结算请求。所有金额操作使用幂等编号。会员套餐修改不改变已开通权益。
5. 「模型网关 → 请求与用量」查看 token 分类、实扣、供应商成本和时延；
   免费售价仍记录可确认的 token 和成本。用户统计扣除退款，收据同时保留原收费和净收费。

私有密钥为 `plugin_data/model-gateway/credentials.json`。按渠道也可用环境变量
`MODEL_GATEWAY_<渠道标识大写且横线/点转下划线>_API_KEY`，环境变量优先。
采用环境变量时避免仅大小写、横线、点、下划线不同的渠道名，以免映射重合。
密钥文件损坏会停用依赖该文件的渠道，后台可明确确认重置后逐条重新配置。

用户页面 `/account/billing` 和 `/account/models` 使用前台会员 cookie，
分别显示余额/会员和在线模型/用量；没有会员登录时提示先登录，不提供后台认证旁路。

## 计量与故障约束

销售与成本按分离的 token 分类计算，整次请求统一向上取整到微元；会员折扣
只影响售价。OpenAI 缓存读取是输入 token 的子集，思考 token 是输出子集，
不重复计费。Anthropic 的缓存写入/读取不含于普通输入，message_delta 输出
是累积值，不按每块求和。适配规则参考供应商的
[Chat streaming usage](https://developers.openai.com/api/reference/resources/chat/subresources/completions/methods/create)、
[Claude streaming](https://platform.claude.com/docs/en/build-with-claude/streaming) 和
[prompt caching](https://platform.claude.com/docs/en/build-with-claude/prompt-caching)。

不依赖粗略 tokenizer 作为扣费事实。预留按配置的整个输入上下文上限与本次
最大输出计算保守费用，因此短输入也可能需要较多可用余额；最后按上游确认的
实际用量结算。超出配置上限、缺失或矛盾的用量进入待核实，而不是收取估计金额。

已确认没有发出的连接失败、HTTP 非 2xx 释放预留；可能被上游接收但缺少可信用量
的情况先保留额度。即使客户端断开，已经收到的完整最终用量仍可以结算。
计费服务暂时不可用时保留持久结算意图，恢复后重试结算；从不重新生成答案。
价格/权益/渠道在调用开始固定，锁外等待不保留任何插件服务租约或 SQLite 事务。

初版只覆盖文本生成、函数工具和可配置图片输入。拒绝共享供应商保存的 response/file
引用、background、store=true、n>1、音频、文档和供应商内建收费搜索/MCP工具。
这些能力需要独立的归属与计量接口，再逐项开放。明确设置输出上限，Chat stream
强制请求最终 usage；不压缩响应、不跟随重定向，不在模型发出后切换另一渠道重试。

## 本地验证命令

```powershell
python tests/stream_e2e.py --exe D:\GIT\home\xs.exe --tls
python tests/billing_e2e.py --exe D:\GIT\home\xs.exe
python tests/model_gateway_e2e.py --exe D:\GIT\home\xs.exe
python tests/smoke.py --exe D:\GIT\home\xs.exe --functional-only --port 19186
node --check plugin/model-gateway/static/ui.js
```

三个新增测试都使用一次性站点、数据库和小型本地模拟上游。真实 xs/TCC 运行，
没有生产密钥或在线费用，没有压力测试；原有功能测试确认源数据库未被修改。
流式 SDK 同时验证私有 CA、UTF-8 分片、即时首块、HTTP 错误、截断与空闲取消。
网关测试覆盖三协议流/非流、工具/思考透传、缓存/累积用量、幂等、身份隔离、
会员、免费售价与已知成本、在途改价、退款、客户端断开、插件停用恢复、进程重启。

`XADMIN_IDENTITY_CONFIG` 可为独立预览进程指定另一份私有 identity 配置。
读写都使用此路径，明确指定但文件不存在时初始化失败；没有环境变量时维持默认
`db/identity.json`。多站点正式进程不要设置站点专用覆盖。home 的 loopback
预览工具使用本地副本处理 cookie/CSRF 的 origin，不改线上域名及私有配置。
