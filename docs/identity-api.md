# 网站会员身份服务

面向任何使用 xadmin 的网站，不含模型、搜索、配额或特定域名规则。会员与后台管理员属于独立权限域，会员 Cookie/JWT 无法登录后台。统一使用 `/api/v1`；旧会员客户端需要同步修改登录请求，不提供旧的浏览器密码摘要协议或 `/api/v2`。

用户页面为 `/account/index.html`。后台“前台用户”工具栏新增“账户与登录设置”，也可通过 `/admin/view/member/identity` 打开。未完整配置的第三方登录及验证码入口隐藏。原生 HTML/CSS/JS，无需 Node/npm 或前端编译。

用户资料与后台用户列表提供只读 `phone_verified`、`email_verified`、相应 `*_verified_at` 和 `security_questions_configured`。未绑定、已填写但未验证、已验证分开显示；验证状态始终以验证码流程为准，不能通过普通资料或后台用户编辑设置。后台列表的 `security_recovery_pending` 表示有未过期的密保恢复申请，每行“安全”打开详情。

## 启用和配置

使用仓库内更新后的 `xs.exe` 和 `xs.json` 启动；Linux 需相应扩展的 xs。运行时来源见 `tools/xs-runtime.lock.json`，SDK 来源见 xserver 的 `lib/xrt_sources.lock.json`。必需扩展为 sqlite、xjwt、xoauth2、xsmtp、md4c。

首次启动执行带校验登记的迁移。历史账号名不合法或大小写冲突时启动失败并打印会员 ID；在维护窗口明确处理冲突，不自动改名或合并。历史手机、邮箱保留为未验证资料，不能登录。

后台可保存配置，或按 `install/identity.example.json` 创建私有 `db/identity.json`。该文件不在静态目录、不进入普通配置编辑器，并被 Git 忽略。无文件时：本地注册开放，第三方登录/短信关闭，国家区号空，CORS 列表空。

`public_origin` 为网站自己的 HTTPS 协议 + 域名/端口，不含路径、尾部斜杠、查询或用户信息。开发允许 `http://127.0.0.1:端口` / `http://localhost:端口`。反向代理也配置外部 HTTPS 地址，它同时决定 Secure Cookie；不盲信 `X-Forwarded-*`。配置每个脚本代内不可变，保存返回 `reload_required:true`，重载或重启后生效。重载清除后台会话及未完成授权，保留会员持久会话。

JWT 随机签名密钥保存在主库 `identity_key`，不进入浏览器/客户端。配置和数据库备份应仅网站服务账号/管理员可读；Linux 私有目录建议 0700、文件 0600，Windows 使用服务账号 ACL。测试不修改真实主库、不生成真实网站凭据。

### GitHub 和微信

GitHub 注册自己的 OAuth App，callback 固定 `public_origin + /api/v1/auth/oauth/github/callback`。启用 state、S256 PKCE、`read:user`，每次服务端换 Token 后读取 `/user`，以不可变数值 ID 关联，不请求仓库访问权限。[GitHub 官方说明](https://docs.github.com/en/apps/oauth-apps/building-oauth-apps/authorizing-oauth-apps)。

微信 `mode:"website"` 使用开放平台网站应用的 AppID/AppSecret、网站扫码、`snsapi_login`。可选 `mode:"web"` 使用对应网页授权应用凭据和授权域名、`snsapi_userinfo`。一个配置选择一种模式，不按 User-Agent 切换，不用网站应用冒充网页授权应用。callback 固定 `public_origin + /api/v1/auth/oauth/wechat/callback`；在官方控制台注册对应域名和权限。

微信 token 和 userinfo 的 OpenID 必须一致，身份依据为 **AppID + OpenID**。UnionID 仅保存元数据，`union_scope` 不自动填写，首版不按 UnionID 合并跨应用账号。GitHub 邮箱、微信昵称或头像也不用于自动合并/验证联系方式。禁用或删除会员仍保留第三方归属，回调不会重新创建它。

`oauth_create_member` 控制首次未绑定授权是否创建会员。新会员可以没有账号和密码，之后通过凭据设置/联系方式验证补齐。

## 标识与密码

| 标识 | 规则 |
| --- | --- |
| 账号 | ASCII 3–64 字节，字母、数字、点、下划线、短横线，至少一个字母；大小写不敏感，显示保留原形式 |
| 手机 | E.164；可按配置区号补齐，不猜测本地拨号前缀；前导零的国内号码请改为国际格式 |
| 邮箱 | 本地部分大小写敏感，ASCII 域名转小写；不删除加号/点；EAI/IDN 由上层明确转换 |

仅验证后的手机/邮箱能登录。普通资料及后台编辑不能修改这些字段或验证状态。账号、已验证联系方式、第三方身份由数据库保障唯一，软删除仍保留标识，不静默释放。

新密码 8–128 **UTF-8 字节**，PBKDF2-HMAC-SHA256、600000 次、随机 16 字节盐。HTTP 接收原始密码，网站应部署 HTTPS。成功登录时验证并升级旧会员密码记录；会员 HTTP 不再接受旧前端 SHA256 证明。后台管理员协议未在此批变更。

## API 契约

正文 `application/json` 对象，最大 8192 字节、深度 4，拒绝重复键/NUL/越界字符串。成功 `{code:0,msg,data}`；错误使用实际 HTTP 状态及 `{code:状态,msg,data:null}`。400 参数，401 未认证，403 禁止/近期证明不足，404 不存在，409 唯一性/版本冲突，429 限流，500 本地故障，502 送达/第三方故障，503 未配置。

身份响应禁止缓存。余额、通知、附件等既有业务继续原契约和权限。会员资料 `createTime`、`phone_verified_at`、`email_verified_at` 沿用 Unix 微秒；新会话/授权/密保 `*_at` 为 Unix 秒，`expires_in` 为秒。

### 密码、资料与会话

| 方法与路径 | JSON / 返回 |
| --- | --- |
| `POST /api/v1/register` | `{username,password,nickname?}`；201 `{id}`；不接受联系方式 |
| `POST /api/v1/login` | `{identifier,password}`；Token 集及会员 Cookie |
| `GET /api/v1/session` | `{id,session_id,reauth_until,access_token,token_type,expires_in,csrf_token?}`；不返回 refresh token |
| `GET /api/v1/profile` | 自己资料、联系方式、验证布尔状态、组/余额 |
| `PUT /api/v1/profile` | 只允许 `{nickname?,avatar?}`，省略保留原值 |
| `POST /api/v1/profile/password` | `{oldPassword,newPassword}`；撤销其他会话 |
| `POST /api/v1/profile/reauth` | `{password}`；当前会话获得 5 分钟近期证明 |
| `POST /api/v1/profile/credentials` | `{username,password}`；近期证明，第三方会员首次设置或更换账号/密码，撤销其他会话 |
| `POST /api/v1/logout` | 无需正文；撤销当前会话并清 Cookie |
| `GET /api/v1/sessions` | 自己最多 5 个活跃会话，`id,current,created_at,last_used,expires_at,ip,user_agent` |
| `DELETE /api/v1/sessions` | `{session_id}` 或 `{others:true}` |
| `POST /api/v1/token/refresh` | `{refresh_token}`；新 access/refresh，不延长会话绝对期限 |

Token 集：`id,access_token,refresh_token,token_type:"Bearer",expires_in:900,csrf_token`。JWT 固定 HS256，校验 issuer/audience/member realm/subject/sid/jti/iat/exp/kid；每次请求仍检查数据库撤销及最新会员/用户组状态。无效或重复 Authorization 不回退到 Cookie。

会话绝对 30 天，每账号最多 5 个活跃会话。refresh 随机生成、摘要落库、单次轮换；旧 refresh 重放撤销整个会话族。客户端必须协调串行刷新。退出、禁用/删除、密码重置、设备撤销跨进程重启/重载保持失效。

浏览器使用 HttpOnly `MSID` 和可读 `MCSRF`，均 SameSite=Lax，写请求每次读取当前 MCSRF 作为 `X-CSRF-Token`，不把 Token 存 localStorage。`GET /session` 在 MCSRF 丢失时补发/更新，匹配时不轮换，可多标签共享。原生客户端使用 Bearer，无 Cookie CSRF。Cookie 写只允许本站 origin；Bearer CORS 采用 `cors_origins` 精确列表、最多 8 项，不允许 `*`，不开放 credentialed CORS。

`last_used` 每分钟最多更新一次，每 5 分钟清理过期/撤销超过 7 天的会话及其刷新历史；活跃族保留完整重放记录。正常签名轮换保留旧验证密钥；紧急轮换删除旧密钥并撤销全部会员会话及旧密钥保护的未完成验证码。

### 验证码与联系方式

| 方法与路径 | JSON / 行为 |
| --- | --- |
| `POST /api/v1/auth/challenges` | `{purpose:"login"或"recover",channel:"phone"或"email",target}`；202 `{challenge_id,expires_in:300,delivery:"sent"或"unknown"}` |
| `POST /api/v1/auth/challenges/verify` | `{challenge_id,code,newPassword?}`；login 签会话；recover 必须新密码，撤销全部旧会话 |
| `POST /api/v1/profile/contacts/challenge` | `{channel,target}`；会员近期证明，向新目标发绑定验证码 |
| `POST /api/v1/profile/contacts/confirm` | `{challenge_id,code}`；同会员同会话，事务内验证/换绑，撤销其他会话 |
| `DELETE /api/v1/profile/contacts` | `{channel}`；近期证明且保留另一种当前可用登录方式 |

六位码、5 分钟、最多 5 次校验；按用途/目标/会员/会话/挑战 ID 带密钥摘要，不明文保存或打印。每目标 60 秒冷却，每 IP 每小时 20 次，记录有容量上限。写入失败回滚挑战消费，可重试；用途混用、另一会话、过期和成功重放均拒绝。

公开发送不直接透露目标是否注册，收到码不等于存在会员归属。明确送达失败 502，未配置 503，未知送达 202/unknown，无自动无限重试。换绑成功后尝试通知旧验证目标，通知失败不回滚。解绑时不会把停用 OAuth 或无法送达的纯验证码联系方式算作可用替代。

### 第三方身份

| 方法与路径 | 行为 |
| --- | --- |
| `GET /api/v1/auth/providers` | `{providers:[{id,name}],registration,phone_verification,email_verification,default_country_code}`，无 secret |
| `POST /api/v1/auth/oauth/{provider}/start` | `{}`；`{authorization_url,expires_in:600}`，HttpOnly MOB 浏览器绑定 |
| `GET /api/v1/auth/oauth/{provider}/callback` | 官方 `state,code` 或 `error`；成功 Cookie、303 固定 `/account/index.html` |
| `GET /api/v1/profile/identities` | 自己 `{id,provider,app_namespace,subject,created_at}` |
| `POST /api/v1/profile/identities/{provider}/bind` | `{}`；近期证明，显式绑定，不转移其他会员的身份 |
| `POST /api/v1/profile/identities/{provider}/reauth` | `{}`；已绑定身份确认当前会话，5 分钟证明 |
| `DELETE /api/v1/profile/identities/{identityId}` | 本人、近期证明、剩余可用登录方式；撤销其他会话 |

provider 为 github/wechat。独立 xoauth2client、最多 128 个待完成尝试、10 分钟有效；state/browser 摘要与消费记录落库，verifier 仅内存。先一次领取再外部 I/O，取消/失败也不可重放。重启/重载取消未完成尝试。第三方 Token 请求内使用后释放，不写库/日志/跳转 URL；本地 Token 不使用第三方 Token。反向代理应屏蔽 callback query 以避免记录 code/state。不支持任意 `return_url`。

浏览器直接打开失败/取消的 callback 时返回带固定账户页链接的中文错误页，保留错误 HTTP 状态，不回显 code/state。Accept 不含 `text/html` 的 API 客户端仍接收 JSON 契约。

## 后台管理接口

沿用管理员 Cookie/URI/RBAC，不接受会员 Bearer。

| 方法与路径 | 行为 |
| --- | --- |
| `GET /admin/member/identity/config` | 脱敏配置、`secret_configured`/`sms_token_configured`、`revision`、后台会话 `csrf_token`、`reload_required` |
| `PUT /admin/member/identity/config` | `{revision,config:{...部分字段...}}`，上述后台 CSRF；省略密钥保留，显式空值清除；旧 revision 409；校验后原子保存 |
| `POST /admin/member/identity/keys/rotate` | `{invalidate_existing:false或true}`，相同后台 CSRF，不返回密钥 |

所有字段有名称/类型/长度/回调验证，错误编辑不修改文件或服务。后台读取已保存配置，保存和生效分开。

## 送达适配

支持注册式短信平台、统一验证码/通知模板和后台平台配置，见 [短信服务接口](sms-service.md)。未启用注册式服务时，继续使用原短信 HTTPS webhook，Bearer，`application/x-www-form-urlencoded`：

```text
challenge_id=<随机ID>&phone=<E.164>&code=<六位码>&purpose=bind|login|recover&expires_in=300&type=verification
```

2xx JSON `{sent:true}` 表示受理送达，明确 4xx 失败，其余超时/未知响应为 unknown。换绑通知 `type=security_notice,purpose=contact_changed,code="",expires_in=0`，适配器须区分验证码与通知。

邮件复用已有 SMTP，要求 `smtp_secure=ssl` 或 `starttls`、系统证书校验，不降级明文；默认 PLAIN 认证，请确认发件服务支持。身份验证码不进入保存明文的长期邮件队列。

C 适配接口在 `include/xadmin/identity_delivery.h`。组合根就绪前调用 `XA_SetIdentityDelivery`；回调同步借用消息/Engine、应用请求锁已释放，返回 SENT/FAILED/UNKNOWN。不能保存借用对象跨异步生命周期或打印验证码。测试 mock 只编入单独 test host；正式入口无万能验证码、回显或任意 OAuth endpoint 开关。

## 验证与回退

Python 3、标准库，Windows：

```powershell
python tests/identity_unit.py
python tests/identity_e2e.py
python tests/identity_challenge_e2e.py
python tests/identity_oauth_e2e.py
python tests/identity_management_e2e.py
python tests/install_wizard_e2e.py
python tests/smoke.py --functional-only --protected-entry
```

Linux 对各测试传 `--exe /path/to/xs`。测试各自使用 `tests/.runtime` SQLite backup 夹具，关闭继承的自动插件、计划任务和邮件；不修改真实主库。验证码/OAuth 是注入传输功能验证，无真实发送/线上授权，无压力/高负载/浸泡。用户页面检查 390px/1280px、登录/资料保存/刷新会话；后台配置 HTTP/RBAC/CSRF/脱敏、JS 语法及浏览器加载已验证，手机布局无横向溢出；Win/Linux 全新安装 16 项功能检查通过。

迁移登记 `xadmin_migration`，不覆盖业务 `PRAGMA user_version`。已发布组件 SQL 不原地改校验，新 schema 追加版本/组件。首次备份分别为 `db/identity-before-v1.db`、`identity-before-auth-v1.db`、`identity-before-challenge-v1.db`、`identity-before-oauth-v1.db`，已有不匹配备份不自动覆盖。

部署前整体备份数据库、私有配置和匹配源码。首次维护窗口需整体回退时先停服务，恢复该窗口主库及旧代码；不要拿旧备份覆盖已产生新业务数据的线上库，也不要用旧会员客户端写入已升级密码。阶段备份用于诊断/恢复，不是自动线上回退。

当前 Win/Linux 隔离功能验收通过。**真实 GitHub/微信授权和真实 SMTP/短信未验证**，需要网站自己的应用注册与送达凭据后验证授权/取消/换绑/恢复。未配置时保持相关入口关闭，不影响本地账号密码及既有后台业务。


## 安全问题与管理员辅助恢复

`security_questions` 为站点级布尔配置，默认 `true`，可在“账户与登录设置”关闭；保存后重载生效。关闭不删除现有密保，用户仍可查看和移除，管理员仍可处理此前有效的申请。

安全问题不是登录凭据，不授予近期身份证明，不能单独重置密码。它只帮助管理员筛选恢复申请；重置之前必须通过其他途径独立核实申请人身份。[OWASP 说明了安全问题的局限](https://cheatsheetseries.owasp.org/cheatsheets/Choosing_and_Using_Security_Questions_Cheat_Sheet.html)。没有经过核实的申请应拒绝，而不是仅凭答案正确放行。

| API | 行为 |
| --- | --- |
| `GET /api/v1/auth/security-questions` | 公共问题目录 `{id,text}[]`，不泄露指定账号的问题 |
| `GET /api/v1/profile/security-questions` | 当前会员的 `configured`、`questions`（ID 数组）、`updated_at`、`recovery_mode:"administrator_review"` |
| `PUT /api/v1/profile/security-questions` | `question1/answer1` 至 `question3/answer3`，需要近期身份证明及会员 CSRF |
| `DELETE /api/v1/profile/security-questions` | 空对象 `{}`，需要近期证明；删除密保，关闭恢复申请并撤销其他设备 |
| `POST /api/v1/auth/security-questions/recover` | `identifier` 与上述三组问题答案；符合语法的请求统一返回 202，无令牌、无 Cookie、无答案对错提示 |
| `GET /admin/member/user/security?id=会员ID` | 验证状态、问题文本（无答案或哈希）、最近 20 条恢复申请及后台 CSRF |
| `POST /admin/member/user/security` | 下述管理员操作，沿用后台权限与同源 CSRF |

问题只能选固定目录中的三个不同 ID，答案经去除首尾 ASCII 空白和 ASCII 大小写折叠后须为不同的 4–128 UTF-8 字节字符串；控制字符、NUL 无效，非 ASCII 内容不自动折叠。恢复时按问题 ID 匹配，不要求保持设置顺序。各答案分别使用 PBKDF2-SHA256 600000 次、独立随机盐；设置后不回显，不让管理员读取。建议使用不公开、不与其他网站重复的答案，不能把安全问题当作多因素认证。

设置或移除密保会撤销其他设备；管理员清除密保会退出所有设备。更新密码/账号凭据或密保会使未处理的恢复申请失效。匿名恢复按 IP 每小时 20 次、标识每小时 5 次限流，不提供按账号查询结果的接口；只有三个答案均匹配且账号有效才登记申请。申请有效期 24 小时，重复成功提交关闭旧申请，最多保留 10000 条记录，新申请时清理过期超过 7 天的记录。用户需主动联系站点管理员，本功能不自动发送短信/邮件。

管理员操作正文包含整数 `member_id`、`action` 和 4–256 字节 `reason`：

- `clear_questions`：清除密保、关闭申请、撤销会员全部会话。
- `reject_recovery`：还需 `request_id`，拒绝有效申请。
- `reset_password`：还需 `request_id`、8–128 字节 `newPassword`、严格布尔 `independently_verified:true`；管理员确认独立核实后重置，不允许恢复禁用/删除账号。

申请消费、密码写入、会话撤销和处理记录在同一事务中，失败全部回滚。已处理、过期或版本变更的申请返回 409，不可重放。`member_security_review` 记录处理人、会员、申请、动作、原因和时间，不存答案或密码；既有路由日志屏蔽敏感正文。该记录用于说明管理员做了什么，不替代站点的人工核实流程。

新增 `security` 迁移组件在首次启动前生成 `db/identity-before-security-v1.db`。回退旧代码之前应停止服务并恢复该备份；不要在运行中的数据库上手动删除安全表。功能回归使用隔离库（Windows 和 Linux 均验证通过）：

```powershell
python tests/identity_security_e2e.py --port 19261
```

此回归覆盖状态字段、输入限制、近期证明、CSRF/RBAC、答案非明文、乱序问题匹配、匿名响应与限流、事务故障回滚、申请过期/失效/重放、会话和 JWT 撤销、站点关闭及迁移重启。只运行功能测试，不运行压力或高负载测试。

Windows 正在打开主库时，WSL 不应直接对其使用 SQLite backup；先由 Windows 的 SQLite 生成一致性副本，再向本回归传入 `--source-db /mnt/d/.../副本.db`。测试副本与服务数据分开，不能拿原始文件复制替代活动 WAL 数据库的一致性备份。
