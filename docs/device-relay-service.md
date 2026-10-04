# 会员设备中继插件

`device-relay` 是通用 xadmin 插件，不绑定墨斗模型、搜索平台或网站域名。依赖宿主 4.3 的托管通道 API；所有已登录且状态正常的会员可使用，无手机号/邮箱认证附加要求。

## HTTP API

统一使用 `/api/v1` 和身份服务的 Bearer 或会员 Cookie；Cookie 写请求仍要求 CSRF。响应沿用 `{code,message,data}`，不返回设备密钥或散列。除单次握手外，接口先检查会员登录。

| 接口 | 请求 | 行为 |
| --- | --- | --- |
| GET `/devices` | 无 | 自己的设备、在线状态、控制占用及查看连接数 |
| POST `/devices/register` | `device_id,device_secret,name,platform,app_version,allow_remote`，可选 `reactivate` | 注册或更新自己有密钥证明的设备；首次返回 201，其余 200 |
| POST `/devices/revoke` | `device_id` | 关闭远控，撤销该设备的所有连接及待使用票据 |
| POST `/devices/remove` | `device_id` | 删除自己的设备登记并撤销连接和票据 |
| POST `/devices/ticket` | `device_id,role`；device 角色需要 `device_secret`，controller 角色需要 `mode=control/view` | 生成单次连接票据 |
| GET `/devices/connect` | WebSocket Upgrade，见下文 | 消费票据并接管连接 |

实际 URL 在上述路径前加 `/api/v1`。设备 ID 是 16 字节安全随机数的小写 hex，设备密钥是 32 字节安全随机数的小写 hex。客户端应保存在自己的受保护凭据区，不能用名称、时间或硬件编号替代。服务端只存密钥 SHA-256，不存原文。

已经撤销的设备不能在重连时悄悄恢复。必须由明确的开启动作调用登记接口，提交原设备密钥及 `allow_remote=true,reactivate=true`。重连只获取票据，不重复注册或重置远控状态。

403 表示设备证明或授权无效；其他账号的设备在查询、取票据及撤销时返回 404。409 表示离线、控制占用、登记上限或重复设备连接；429 表示待用票据达到上限；503 表示服务关闭或通道资源不足。客户端不能将这些错误解释为“切换本机”。

## 握手和路由

票据响应含 `ticket,protocol,path,expires_at,payload_limit`。通过 `Sec-WebSocket-Protocol: xadmin.device-relay.v1, xadmin.ticket.<ticket>` 交付证明，不带 URL 查询串。服务器只在响应中选择公开协议名，不回显票据。票据默认有效 30 秒，每个会员最多 8 张未过期票据，全插件最多 128 张，仅存内存；使用一次或插件换代即失效。即使升级格式错误，已识别的票据也被消费。

每设备一个原生连接、一个独占控制连接，默认最多三个只读连接。原生连接收到 `ready`；控制端收到包含版本、设备 ID、随机 `peer_id`、模式和最大 payload 的 `ready`。原生端先收到 `peer_open`，包含由服务端决定的模式。控制端关闭时原生端收到 `peer_close`。

控制端可发送文本或二进制消息。中继不解释业务正文，将其封装为二进制：

```text
MDR1 (4 字节) | peer_id (16 字节) | kind (1 字节：1 文本 / 2 二进制) | payload
```

原生端按同一封装回复。服务器只向绑定该原生连接的 peer 转发，忽略已断开的 peer 的迟到结果；控制端不能自己指定外层 peer。只读模式必须由原生端根据可信 `peer_open` 模式检查业务动作，不能依赖前端禁用按钮。协议、请求确认、去重、分块及版本能力协商由原生应用负责。

完整通道消息最多 256 KiB；转发 payload 最多 262123 字节，输出队列 1 MiB。达到队列限制关闭连接，让应用按请求执行状态恢复。原生设备断开会关闭其所有控制/查看连接；其他设备不受影响。平台账号注销、封禁或失效由宿主会话检查关闭相关通道。

## 存储和部署

私有库位于 `db/plugin/device-relay/plugin.db`，schema 版本 1，只包含设备元数据、所有者和密钥散列。消息、附件、票据、JWT 和会话正文不会写入该库或日志。首版通过 TLS 保护传输，服务器仍可见转发正文；没有端到端加密承诺。

后台使用已有插件启停、配置和健康检查页面。配置包括服务开关、每位会员登记上限、查看连接上限、票据有效秒数，不包含业务平台特例。宿主全局托管通道上限仍为 32 个；开放给所有用户不意味着资源无限。生产应观察正常使用中的资源状态，另行扩展容量，不执行压力测试。

验证命令：

```powershell
python tests/device_relay_e2e.py --exe D:/GIT/x-admin/xs.exe --tls
```

测试使用临时数据库和真实 TCC 插件，覆盖两个账号、登记证明、单次票据及到期、可信 peer 路由、独占控制/查看、二进制、撤销/重新开启、插件换代、错误帧隔离以及正文不落库/日志。墨斗原生客户端、前端切换和生产站点仍需后续阶段完成。

2026-10-05 验证：Windows xs `48fc5fd-dirty` 的 TLS 测试通过；服务器 Ubuntu/glibc 2.35、GCC 动态链接 xs `fee65c24759a28d95bba0f1396bebc5748649d63` 的通道和设备中继 TLS 测试通过。隔离源码位于 `/home/xs/codex-build/device-relay-d4ad297`，监听测试端口 39181/39191；使用临时数据库，未部署生产站点或重启线上服务。构建/验证记录已经同步到本地文档，服务端 `/opt/www` 的已跟踪文件没有修改。
