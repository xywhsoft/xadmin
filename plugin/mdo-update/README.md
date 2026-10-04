# 墨斗更新

后台启用 mdo-update 后出现“墨斗更新”菜单。管理员上传 Windows x86_64 的
mdo.exe 或单体 Android ARM64 APK，可选填写说明；插件计算实际落盘文件的
SHA-256，发布后立即生效。无需填写版本号或 hash。

需要 xadmin 4.2 SDK（ABI 仍为 v4），尤其是 XAdmin_ReplyBinary 和
src/net/plugin_async.c。不能仅把插件复制给旧宿主后强行跳过兼容检查。

宿主 HTTP 服务需配置 recv_limit: 34603008、body_limit: 33562624
（字段放在 service 层，不是 custom 子对象）；反向代理也须允许至少 33 MiB
请求体。插件自身每个包限制 32 MiB。默认小请求限制无法上传 EXE/APK。

- GET /admin/mdo-update：管理页面，复用管理员权限。
- GET /admin/api/mdo-update：当前包、上传上限、会话 CSRF token。
- POST /admin/api/mdo-update/upload：multipart platform、file、可选 notes；
  必须携带 X-CSRF-Token，同源提交。
- GET /update/version?platform=windows-x86_64 或 android-arm64-v8a：
  无需登录。省略 platform 默认 Windows；没有包为 404。
- GET/HEAD /update/download/{platform}/{sha256}：原样二进制下载，
  不重定向、不压缩；下载分块发送，单次 120 秒期限。

数据位于 plugin_data/mdo-update/current.json 和 packages/，不要公开静态
映射这个目录。updated_at 为 Unix 秒；hash 为小写 SHA-256。当前和前一包保留，
更旧包发布成功后清理；进行中的下载拥有独立字节，不受后续发布影响。
相同包上传不改变说明或时间。元数据损坏时拒绝发布，避免静默覆盖已发布内容。

插件只做格式检查，不执行文件；APK 包名、签名、versionCode 由客户端和安卓系统
进一步验证。发布前须自行验证产物可用。同签名 APK 每次发布建议递增 versionCode。

验证：python tests/mdo_update_e2e.py --exe <mdo.exe> --apk <mdo-arm64-v8a.apk>。
测试创建独立站点、账户和数据，只验证功能，不发布线上包、不做压力测试。
