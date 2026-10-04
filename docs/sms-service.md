# 可注册短信服务

短信平台只负责投递。验证码生成、MAC 保存、有效期、尝试次数、一次性消费和账号变更事务由现有身份服务统一负责。全部实现使用 C 和 xs/xrt，不新增运行时 SDK 或外部命令依赖。

## 标准接口

`include/xadmin/sms.h` 定义 `XASmsProvider` 接口表、配置字段、消息、配置快照和回执；实现位于 `src/sms/`，每个平台一个 `providers/*.c` 文件。

平台通过 `XA_SmsRegister(&provider)` 注册，提供配置字段 `fields`、可选 `validate`、必选 `build/parse`。`build` 构造请求，框架统一发送；`parse` 映射业务状态。后台字段由注册信息生成，新增平台不修改账号 API 或设置页面。

注册在 xadmin `ServiceInit` 之前完成，初始化时冻结；重复 ID、未知 ABI、超限、非法字段、带密钥默认值和冻结后的注册被拒绝。最多 16 个平台。描述符必须是静态对象，存活到 `ServiceUnit`。当前是应用 C 组合接口，不是现有插件 SDK 的跨动态库 ABI。自定义站点可用自己的 C 入口包含 xadmin，在调用 xadmin 初始化前注册。

自定义入口的组合方式如下。`my_sms.c` 定义静态 `MySmsProvider`；可以参考 `src/sms/providers/webhook.c` 实现其回调，不需要修改核心或后台页面。

```c
#define ServiceInit XAdmin_ServiceInit
#include "../main.c" /* 按自定义入口位置调整相对路径 */
#undef ServiceInit
#include "my_sms.c"

void ServiceInit(XS_HostInfo* host)
{
    if (!XA_SmsRegister(&MySmsProvider)) {
        printf("[site] SMS provider registration failed\n");
        return;
    }
    XAdmin_ServiceInit(host);
}
```

build 只通过 `XA_SmsHttpHeader/Body` 构造请求，不自行发送；返回 false 表示构造失败。parse 收到 2xx 的有效 JSON 对象，初始回执为 UNKNOWN，只有平台明确受理或拒绝时才改为 ACCEPTED/FAILED。所有配置、参数和响应均为借用对象，不得释放或留存；密钥字段设置 secret=true。

## 内置平台

| ID | 实现 | 官方资料 |
|---|---|---|
| `webhook` | HTTPS 表单 + Bearer | 下文协议 |
| `aliyun` | Dysmsapi 2017-05-25 / ACS3 | [发送](https://api.aliyun.com/document/Dysmsapi/2017-05-25/SendSms)、[签名](https://help.aliyun.com/zh/sdk/product-overview/v3-request-structure-and-signature) |
| `tencent` | Sms 2021-01-11 / TC3 | [发送](https://cloud.tencent.com/document/api/382/55981) |
| `huawei` | batchSendSms/v1 / X-WSSE | [发送](https://support.huaweicloud.com/api-msgsms/sms_05_0001.html) |
| `baidu` | api/v3/sendSms / BCE v1 | [发送](https://cloud.baidu.com/doc/SMS/s/lkijy5wvf)、[签名](https://cloud.baidu.com/doc/Reference/s/njwvz1yfu-en) |
| `yunpian` | v2 指定模板单发 | [发送](https://global.yunpian.com/official/document/sms/zh_cn/domestic_tpl_single_send) |
| `chuanglan` | 国内 v2 模板接口 / HMAC | [发送](https://doc.chuanglan.com/docs/PA4N31T5HDWA1768) |
| `ronglian` | TemplateSMS / MD5 + Base64 | [发送](https://console.yuntongxun.com/doc/rest/sms/3_2_2_2.html) |
| `submail` | 国内 v4 sms/xsend / App Key，HTTPS | [发送](https://www.submail.cn/documents/OOVyh) |

阿里、云片、创蓝、容联、SUBMAIL 当前适配国内接口，接受 `+86` 加 11 位手机号；其余适配器使用 E.164，境外发送仍需平台开通和相应模板。底座一次一个接收人，不提供群发或营销入口。

华为部分语言示例的 WSSE 摘要格式不同：默认 `wsse_digest=raw`，按接口公式对原始 SHA256 摘要 Base64；旧网关若要求 hex 后 Base64，可明确选 `hex`，不自动回退或重发。始终校验证书和主机名。

创蓝将有序参数转换为 `param1/param2/...`。独立通知账号可配置 `notification_account/notification_password`，必须成对填写，验证码仍使用 `account/password`。

## 配置

后台 → 用户管理 → 账户与登录设置 → 短信服务。私有配置位于 `db/identity.json` 的 `sms` 字段，密钥 GET 仅返回 `secrets_configured`。省略密钥保留，空字符串清除，切换平台清除旧 options；templates 整体替换。保存后重载应用生效，在途请求使用配置快照。

启用新服务时优先使用它；停用时回到原 `sms_webhook_url/sms_webhook_token`。彻底停用短信须同时清除原有 Webhook 地址或 token。

```json
{
  "enabled": true,
  "provider": "tencent",
  "options": {
    "secret_id": "YOUR_SECRET_ID",
    "secret_key": "YOUR_SECRET_KEY",
    "sdk_app_id": "YOUR_APP_ID",
    "sign_name": "网站签名",
    "region": "ap-guangzhou"
  },
  "templates": {
    "verification": {
      "type": "verification",
      "id": "YOUR_VERIFICATION_TEMPLATE",
      "parameters": ["code", "minutes"]
    },
    "contact_changed": {
      "type": "notification",
      "id": "YOUR_CONTACT_CHANGED_TEMPLATE",
      "parameters": []
    },
    "order_update": {
      "type": "notification",
      "id": "YOUR_ORDER_TEMPLATE",
      "parameters": ["order", "status"]
    }
  }
}
```

最多 16 个模板，每个最多 16 个参数，名称最多 64 字符，仅字母、数字、下划线、连字符。启用平台 ID 模板时必须填写已审核 ID；Webhook 可省略 ID。可选 `text` 留给自定义适配器，内置平台不使用自由文本发送。

验证码固定使用 `verification`；参数可选 `code`（必须）、`minutes`、`expires_in`（秒）、`purpose`（bind/login/recover）。只发模板选择的参数。通知参数必须完整匹配，不得缺少或额外传入。腾讯、华为、容联、创蓝按 parameters 顺序；其他平台按参数名。

注册式 Webhook 发送 form：`phone/type/template/parameters/challenge_id/code/purpose/expires_in`，其中 parameters 为 JSON 字符串，type 为 verification 或 notification，Authorization 为 Bearer。2xx JSON `{sent:true}` 表示受理，`{sent:false}` 表示拒绝；消息 ID 可返回 `message_id`。原 Webhook 的 security_notice 协议保持原样。

## 验证码与业务通知

用户填写验证码的界面沿用账户页：

1. 登录/找回：`POST /api/v1/auth/challenges`，channel=phone，服务器生成并发送。
2. 填写后：`POST /api/v1/auth/challenges/verify`，传 challenge_id/code；找回另传 newPassword。
3. 绑定/换绑：`POST /api/v1/profile/contacts/challenge`，需登录和近期证明；填写后提交 `POST /api/v1/profile/contacts/confirm`。

有效期 300 秒、最多 5 次尝试、一次性消费、频率限制和作用域隔离保持。数据库保存验证码 MAC，正式 API 不回显验证码。不增加手机号自动注册行为。

业务代码用统一接口发送通知，接收人授权、业务权限和发送预算由调用方负责：

```c
/* 在请求锁下捕获快照，业务事件/接收人已校验。 */
XASmsConfig config = G_Identity.sms;
xvalue* params = ValueObject();
ValueSetText(params, "order", "202610040001");
ValueSetText(params, "status", "已发货");
XASmsMessage message = {
    XA_SMS_NOTIFICATION, "+8613800138000", "order_update", params, "event-123"
};
XASmsContext context = {req->raw->server->Engine, NULL, NULL, NULL};
XASmsReceipt receipt;
xrtMutexUnlock(G_RequestLock);
XASmsStatus status = XA_SmsSend(&config, &context, &message, &receipt);
xrtMutexLock(G_RequestLock);
xrtSecureZero(&config, sizeof(config));
xrtValueRelease(params);
```

换绑时已自动用 contact_changed 通知旧的已验证手机；没有该模板则跳过，不影响已完成的变更。

ACCEPTED 是平台受理，不是手机送达；FAILED 是本地校验/构造失败或明确拒绝；UNKNOWN 是网络异常、超时、服务端错误或无法确认的响应。腾讯平台自身的下发超时也按 UNKNOWN 处理；创蓝必须确认单个接收人的成功/失败数量。UNKNOWN 不自动重试或切换平台重发。request_id 是关联标识，不承诺跨平台幂等；业务通知由业务事件去重。

HTTPS 使用借用的 xs Engine，单次 I/O 总期限 15 秒，正文上限 64 KiB、线缆响应 96 KiB，不跟随重定向。可用 `XASmsContext.ca_pem` 指定私有 CA，始终校验。回调同步借用对象，不允许留存或启动脱离生命周期的工作。

`XA_SmsSetTransport()` 在冻结前安装受控应用传输，身份投递会使用它；单独调用也可在 XASmsContext.transport 注入传输。回调正文须 xrt 分配、最多 64 KiB，框架负责释放。正式入口没有测试模式、匿名发送 API、验证码长期队列或正文/密钥/原始响应日志。

## 测试与实际送达

```text
python tests/sms_unit.py
python tests/sms_identity_e2e.py
python tests/identity_challenge_e2e.py
python tests/identity_management_e2e.py
```

真实 xs/TCC 测试覆盖扩展注册、配置/密钥、用途参数、各平台正反响应，并由 Python 独立验算签名与编码。原生 TLS 测试用临时本地 CA，验证证书拒绝、定长/chunked、截断/超限和重定向。生成证书需要测试依赖 cryptography；`--skip-native` 只跑适配器契约，不影响程序运行时依赖。

2026-10-04 已在 Windows 和 Linux 的最新 xs 测试构建上通过 192 项底座断言、独立签名/编码验算及原生 TLS 测试；两个系统均通过短信账号端到端测试。Windows 上另通过账号/JWT、安全问题、验证码与后台身份管理回归，并在浏览器验证平台切换及配置保存。

账号端到端测试在单独 test host 注入模拟传输，覆盖用户输入验证码、绑定、登录、失败/未知结果、通知和后台配置，不发真实短信、不改真实库。不做压力或高负载测试。

尚未配置真实平台账户及审核模板，未做运营商送达实测；上线前需要使用自己授权的手机号验证真实签名、模板、白名单和手机送达。契约测试通过不等于真实短信送达已验证。
