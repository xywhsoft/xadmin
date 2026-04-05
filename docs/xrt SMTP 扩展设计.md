# xrt SMTP 扩展设计

## 目标

把 SMTP 发件能力从 `xadmin` 业务层下沉到 `xrt` 扩展层，形成可复用的基础设施：

- `xadmin` 可直接调用
- 其他 `xs` 项目也可直接复用
- 避免依赖外部 `curl`、PowerShell 或第三方发件工具

## 目录约定

新增目录：

- `tcc/extlibs/`

用于放置这类扩展：

- 不是 `xrt` 最核心的公共能力
- 但在特定场景里很有价值
- 适合以 header-only 形式被 `xsbase.h` 统一接入

当前 SMTP 扩展位置：

- `tcc/extlibs/xsmtp/inline_xsmtp.h`

接入方式：

- `tcc/inc_xs/xsbase.h`

## 当前能力

`xsmtp` 当前支持：

- SMTP 明文连接
- SMTPS/Implicit TLS
- STARTTLS
- `AUTH PLAIN`
- `AUTH LOGIN`
- UTF-8 邮件主题编码
- UTF-8 发件人名称编码
- `text/plain`
- `text/html`
- `multipart/alternative`
- `To/Cc/Bcc`
- 自定义头部

当前未做：

- 附件
- `AUTH CRAM-MD5`
- OAuth2
- DKIM 签名
- 连接池
- 批量投递优化

## xsmtp 接口

主要结构：

- `xsmtpaddr`
- `xsmtpconfig`
- `xsmtpmessage`
- `xsmtpresult`

主要入口：

- `xrtSmtpConfigInit(...)`
- `xrtSmtpMessageInit(...)`
- `xrtSmtpResultInit(...)`
- `xrtSmtpSendMail(...)`

## xadmin 接入

`xadmin` 当前不再自己实现 SMTP 协议，而是直接调用 `xsmtp`：

- 后台创建邮件任务后写入 `mail_task`
- 在邮件任务列表页手工执行“待发送任务”
- 执行器读取全局 SMTP 配置
- 调用 `xrtSmtpSendMail(...)`
- 回写：
  - `mail_task.status`
  - `mail_task.retryCount`
  - `mail_task.nextRetryAt`
  - `mail_task.sendTime`
  - `mail_task.errorMessage`
  - `mail_log`

## 后续建议

下一步建议继续补这几项：

1. 后台“测试 SMTP 配置”接口
2. 调度器自动执行 `mail_task`
3. 附件能力
4. 邮件模板变量渲染
5. TLS 校验开关与高级配置
