# 托管 WebSocket 通道（宿主 4.3）

插件 ABI 描述符仍为 v4，新增函数见 `plugin_sdk/xs_plugin.h`。这是通用会员实时通道，适用于通知、协作和设备中继等插件。

路由调用 `XAdmin_ChannelAccept` 并立即返回，传入已验证会员会话和 `XAdminChannelConfig`。宿主拥有连接、线程、输出队列和回调生命周期，不借用 HTTP 请求头、正文或 session 指针。接受成功后即接管请求；握手发送失败也会调用一次 `on_close`。

`on_open`、`on_message` 和 `on_close` 与其他插件回调一样，在宿主应用锁内运行。消息内存仅在回调期间有效。`XAdmin_ChannelSend` 拷贝正文后立即返回，0 表示进入队列，-2 表示背压，-1 表示无效或已关闭；进入队列不代表对端已收到。插件应在背压时关闭连接并让应用协议重连/恢复，不能无限重试。插件自己负责协议级确认和去重。

默认要求同站或站点明确允许的 Origin。`allow_cross_origin` 仅供已经核验单次握手凭据的插件使用，不能直接用于 Cookie 登录握手。宿主拒绝带查询串、正文及无效协议的升级请求。插件应在独立 HTTP 请求中完成敏感凭据交换，不能把认证凭据放 URL。

限制：全宿主最多 32 个连接，每连接最多 64 条待发送消息；完整消息上限由插件指定，最高 256 KiB；队列最高 4 MiB。大文件必须分块并由应用协议控制总大小和顺序。单次发送最多等待 2 秒，20 秒 WebSocket Ping，60 秒无收到帧则关闭；每 5 秒复核会员会话，交付每条消息前再次复核。注销/撤销会话立即请求关闭。用户封禁或数据库直接撤销也能在复核时关闭。

线程保留 xs 的流事件表，遵守 pull / TAKEOVER 契约。插件停用、重载先标记停止，释放应用锁并等待通道线程结束，然后调用 OnStop / OnUnload、释放 TCC 镜像。通道回调内的插件启停/重载请求被拒绝，避免等待自身。宿主新代发布后，旧通道通过公开 server lease 查询识别退役，在 250 ms 检查周期内关闭；不能让长连接无限保活旧编译代。

正文只在有界内存里存放，释放前清零，通道不记录正文或认证材料。

功能验证：

```powershell
python tests/channel_e2e.py --exe D:/GIT/x-admin/xs.exe
python tests/channel_e2e.py --exe D:/GIT/x-admin/xs.exe --tls
```

夹具使用临时数据库和真实 xs/TCC 插件，覆盖握手拒绝、文本/二进制、分片、Ping/Pong、两个连接、协议错误、队列背压、关闭回调计数、注销、插件和宿主换代。不会启动压力或高负载测试，也不会触碰线上服务。

2026-10-05：Windows xs `48fc5fd-dirty` 的明文及 TLS 通道测试通过；`identity_unit.py`（63 条断言）、`identity_security_e2e.py` 和 `audit_sdk.py` 通过。SDK 审计发现已有共享工具头漂移，已单独提交合并两边辅助函数。通用 smoke 在执行前被既有 `page/attachment/list.html` 资产哈希差异阻止，本阶段未修改该页面或基线清单，不能将整套 smoke 记为通过。
