# 新版 xs/xrt 接口适配（2026-10-08）

应用现要求带 xrt `8e9623c4` 或同一时间/等待契约的 xs。核心、扩展原生实现和 TCC SDK 必须来自同一批源码，不能只替换头文件。

数据库和既有 JSON 的时间字段继续使用 Unix 微秒；JWT、会话过期和验证码的协议字段仍使用 Unix 秒。新 `xtime` 则是公元 UTC 毫秒，二者不可直接相互赋值。

`include/xadmin/time.h` 统一提供显式转换：`XAdmin_UnixNowUs()` 用于持久记录和已有业务时间计算，`XAdmin_TimeFromUnixUs()` 用于旧记录的日期显示。调用 xrt 日期格式化、短信签名或本地日期 API 时使用原生 xtime；腾讯短信签名的 timestamp 用 `xrtTimeUnix()` 转成 Unix 秒。文件元数据为 Unix 毫秒，插件文件列表显示处单独转换。

HTTP、TLS、异步回复和 SMTP 在一项操作内共享单调截止预算，每次底层调用传入剩余毫秒。已删除所有旧 `xdeadline`/`WaitUntil` 调用，计划任务条件等待和后台休眠也改为毫秒。WebSocket 的活跃判断使用单调时钟，不受系统日期校正影响。

插件 SDK 公开同一个时间头；内置插件和内容插件生成模板也改为显式 Unix 时间，保持现有数据库可读，不需要改写线上历史记录。

使用待发布的完整 xs 程序执行隔离回归：

```sh
python tests/time_contract.py --exe ../xserver/.build/xrt-latest-native/xs.exe
python tests/smoke.py --functional-only --exe ../xserver/.build/xrt-latest-native/xs.exe
python tests/identity_application_e2e.py --exe ../xserver/.build/xrt-latest-native/xs.exe --port 19087
python tests/stream_e2e.py --exe ../xserver/.build/xrt-latest-native/xs.exe --port 19085
python tests/stream_e2e.py --tls --exe ../xserver/.build/xrt-latest-native/xs.exe --port 19086
```

Linux 使用对应 ELF 路径。本次时间契约测试覆盖 Unix 纪元、负值向下取整、历史日期、真实当前时间和短单调预算；业务测试使用独立数据库，覆盖身份授权、CSRF、一次性回调、续期/撤销、插件重载、计划任务、异步回复及流式超时。没有运行压力或高负载测试。
