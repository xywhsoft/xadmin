# 前后台两步验证（2FA）

前台会员和后台管理员使用独立的验证器绑定。当前支持 TOTP：SHA-1、六位动态码、30 秒周期，以及十个一次性恢复码。不依赖短信、邮件或第三方二维码服务。

现有账户默认不强制绑定。用户确认绑定后，所有登录入口执行两步验证；前台密码、短信/邮箱验证码、GitHub、微信，后台原登录入口和受保护入口均生效。尚未提供按角色强制绑定或 WebAuthn。

## 界面

- 前台：从网站导航“账户安全”进入 /account/index.html#two-factor-auth，登录会员账户后定位到“两步验证（2FA）”卡片。
- 后台：右上角用户菜单“账户安全 / 两步验证”通过 Layui 弹层打开设置，可在窗口内完成绑定、更换验证器、关闭绑定和恢复码管理；也可直接访问 /admin/view/auth/mfa。普通后台用户也可管理自己的绑定，不需要用户管理权限。
- 绑定：确认当前身份，扫描二维码或手动录入密钥，再输入新验证器动态码。确认前不会替换原绑定。
- 登录：原登录方式验证成功后，输入验证器动态码或未使用的恢复码，才获得正式会话。后台通过独立弹层完成验证，保留原登录页布局；可切换恢复码，关闭弹层则取消本次验证并返回密码登录。
- 更换验证器、关闭绑定和重新生成恢复码需要五分钟内的原登录方式及 2FA 验证。密码账户可输入当前密码；无密码的第三方账户可先重新登录。
- 绑定确认和重新生成时，恢复码只显示一次，提供下载。新恢复码替代全部旧码，每个仅使用一次。
- 绑定、更换、关闭和重新生成恢复码均撤销全部旧会话并签发新的当前凭据。前台包括 Cookie、Bearer 和 Refresh Token；后台包括 XSID。

同一动态码成功验证后不能重复使用，刚确认绑定的动态码也已消费。若立即退出再登录，请等待下一周期或使用恢复码。验证器与服务器应保持时间同步，允许前后各一个周期的时钟偏差。

## 密钥和存储

首次启动执行独立的 mfa 数据库迁移，保留原密码哈希流程。验证器密钥用 AES-256-GCM 加密，随机 nonce，并绑定权限域、账户 ID 和绑定版本。恢复码、登录挑战和绑定挑战只保存摘要；密钥和恢复码不写入日志。

加密主密钥默认生成在私有 db/mfa.key，是 32 字节随机二进制内容。进程环境变量 XADMIN_MFA_KEY_FILE 可指定既有密钥文件或首次创建位置；父目录须预先存在。文件使用独占创建，不覆盖既有文件，不跟随符号链接。Linux 创建权限为 0600；Windows 部署应为服务账户和运维管理员设置文件及目录 ACL。

**备份主库时必须另行备份原 mfa.key。** 建议将密钥备份放在受控的独立位置。数据库保存密钥指纹；文件缺失、损坏或替换后，服务拒绝初始化，不能通过删除文件自动更换密钥。数据库备份本身不包含加密主密钥。

新部署需使用支持 xrt AES-GCM、SHA-1、随机数和安全文件接口的 xs。仓库当前 xs 已通过测试；应用内置二维码编码源码并保留 MIT 许可。

## 接口约定

所有 MFA JSON 响应使用现有 code/msg/data 格式，敏感回复设置 Cache-Control: no-store。后台最终登录仍保持 result/message 格式。管理接口仅操作当前账户，不接受外部账户 ID。

前台第一步登录在已启用 MFA 时返回：

~~~json
{"code":0,"msg":"signed in","data":{"mfa_required":true,"challenge_id":"随机挑战标识","expires_in":300}}
~~~

此时不返回 Access/Refresh Token，不创建正式会员会话，并清除原会员 Cookie。客户端必须判断 mfa_required 后再处理登录结果。

| 接口 | 用途 |
| --- | --- |
| POST /api/v1/auth/mfa/verify | 请求 challenge_id 和 code，支持动态码或恢复码；也支持 recovery_code 字段。成功返回正常登录数据及 Cookie |
| GET /api/v1/auth/mfa/pending | 第三方回调后通过 HttpOnly 的短期 MMFA Cookie 获取待验证挑战，供账户页接续流程 |
| POST /api/v1/auth/mfa/cancel | 请求 challenge_id，取消登录挑战 |
| GET /api/v1/profile/mfa | 绑定状态、剩余恢复码数量、近期身份验证状态 |
| POST /api/v1/profile/mfa/setup | 请求可选 password/code；返回 setup_id/secret/otpauth_uri/qr_svg/expires_in |
| POST /api/v1/profile/mfa/confirm | 请求 setup_id/code，确认新验证器；返回新登录数据和十个恢复码 |
| POST /api/v1/profile/mfa/reauth | 验证原登录方式和已绑定因子，更新近期验证状态 |
| POST /api/v1/profile/mfa/recovery-codes | 重新生成恢复码并轮换会话，返回新登录数据和新恢复码 |
| POST /api/v1/profile/mfa/disable | 关闭绑定并轮换会话，返回新登录数据 |

前台 password 为原始密码；Cookie 写请求需要现有 X-CSRF-Token，Bearer 请求沿用现有规则。刷新 Token 和原生应用 PKCE 授权只继承来源会话已有的 MFA 状态，不把它变成新的因子验证。

后台同名管理操作位于 /admin/auth/mfa 和其 setup/confirm/reauth/recovery-codes/disable 子路径。先 GET 状态取得 csrf_token，写请求携带 X-CSRF-Token。后台 password 延续 SHA256(username + "_xywhsoft_" + 原始密码)，服务端仍执行原加盐密码校验。管理操作后的 Cookie 会更新，应重新取得 CSRF Token。

后台第一步在原登录路径返回 result:false,mfa_required:true,challenge_id。第二步仍向**同一个登录路径** POST challenge_id/code，成功才设置 XSID。取消请求使用 challenge_id/action:"cancel"。配置受保护入口时不开放额外后台登录入口，“记住登录”设置在第二步继承。

登录挑战有效期五分钟，绑定挑战十分钟且绑定当前会话；每个最多五次尝试。账户连续五次因子验证失败后暂停五分钟，状态持久化，重启不会解除。已使用或超时挑战不能再次登录。原密码、账户状态、角色或绑定版本变化会使未完成登录挑战失效。

## 本地应急恢复

验证器丢失时优先用已保存的恢复码登录，然后更换验证器。恢复码也丢失时，由有服务器数据库权限的运维人员执行本地工具；不提供远程“跳过 2FA”接口。

1. 停止 xadmin，按运维流程备份数据库和原密钥。
2. 核对数据库中的后台 user.id 或前台 member.id。
3. 预览指定账户，再执行恢复。示例 ID 仅为示例：

~~~powershell
python tools/mfa_recover.py --db db/main.db --realm admin --account-id 1
python tools/mfa_recover.py --db db/main.db --realm admin --account-id 1 --apply --reason "验证器和恢复码丢失，已核实账户归属"
~~~

工具在事务内关闭该账户绑定、增加版本、删除恢复码及未完成挑战、撤销会员持久会话，并记录操作者和原因。必须停服，后台内存会话才同时清除。保留原账号和密码；恢复后按原登录方式登录并重新绑定。--realm member 用于前台账户。

该工具用于账户因子丢失，不能代替找回原密码或恢复丢失的主密钥。mfa.key 丢失时应恢复与数据库对应的原密钥备份，禁止随机生成文件替换。

## 验证

~~~powershell
python tests/mfa_e2e.py
python tests/mfa_e2e.py --protected-entry --port 19215
python tests/identity_challenge_e2e.py
python tests/identity_oauth_e2e.py
python tests/identity_application_e2e.py
node tests/admin_password_ui.js
node tests/mfa_ui.js
~~~

测试使用 tests/.runtime 下的隔离数据库和测试账户，覆盖 RFC 6238 向量、加密隔离、CSRF、旧凭据撤销、动态码重放、恢复码单次消费、失败事务、重启与锁定、本地恢复及密钥不匹配拒绝启动；不更改真实数据库或创建真实部署密钥。
