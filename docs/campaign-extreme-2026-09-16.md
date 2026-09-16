# 7 小时混合极端压力战役终报（2026-09-16）

**TLDR**：86.2 万请求 **0 个 5xx**、306 次热重载**零真实崩溃**（4 次"GDB 崩溃"全部坐实为探针开发期外部 `taskkill //IM xs.exe` 误杀）、渗透载荷（SQLi/XSS/穿越/类型混淆/深嵌套）无一得手；两项真实发现均已闭环——①管理写接口无长度校验致数据膨胀（本仓已修，E1）②xrt block 层每代 ~2-4MB 换代泄漏（数据移交 xserver 仓）。性能劣化全部集中在内存熔断邻域，正常期基线亚毫秒。

## 战役配置与终态

- 编排器：`tests/campaign_extreme7.py`（GDB batch 启动 + 崩溃归档 + 2.6GB 内存熔断干净重启）
- 负载：reader/writer/fuzzer/reload 风暴(30-75s)/session churn/plugin beater/uploader 七类混合，6.5h
- 终态：`req=862004 ok=797150 err4xx=60775 err5xx=0 connerr=4079 reload_prox_err=285 reloads=306 crashes=4 boots=54 leak_cycles=49`
- 工件：`tests/.runtime/extreme7/`（findings.json 444 条 / final.json / metrics.jsonl 779 样本 / crash_00{1-4} 上下文）

## 发现裁定（444 条 findings 全分类）

| 类别 | 条数 | 裁定 |
|---|---|---|
| CRASH | 4 | **非应用缺陷**。03:53-03:56 集中发生（探针开发窗口），GDB 记录 `exited with code 01`、无信号无栈——外部 `taskkill //IM xs.exe` 按名误杀签名；第 5 次启动后 6.5h 零崩溃 |
| LEAK/MEM | 49+49 | 熔断轮转（每轮 ~2.6GB 触发干净重启）。归因=**数据膨胀为主**（见下）+ 真实换代泄漏 |
| PERF | 170 | 高压期（RSS≥1.8GB 邻域）p95>5s 告警；正常期基线健康（见下表），根因同内存压力 |
| WRT-D | 172 | **战具噪声**。重载窗口连接失败（resp=None）后 DB 查无记录——请求本就未成功，非假成功 |

## 泄漏归因（探针差分三重实证，中期已交付）

1. **数据膨胀（主因）**：管理写接口只查非空、无长度上限、无频控；fuzzer 的 `default=str` 把怪载荷转成合法字符串直接落库。终态战役夹具 DB 112MB、logs 41 万行（战役自身 xlogpush 流量）、RSS≈10×DB（页缓存+行缓冲）。
2. **真实换代泄漏 ~2-4MB/代**：xrt block 层 `blockAlloc 3.5MB/代 vs blockFree 1.65MB/代`——属 xserver/xrt 仓，本仓无修复面，数据移交。

## 性能基线（30s 分桶，正常期 vs 高压期中位数）

| 类型 | 正常期 p50/p95 (ms) | 高压期 p50/p95 (ms) |
|---|---|---|
| api | 1.2 / 41 | 143 / 608 |
| public | 0.6 / 37 | 128 / 511 |
| page | 0.6 / 34 | 125 / 410 |
| write | 10.7 / 70 | 121 / 584 |
| upload | 195 / 502 | 278 / 271 |
| plugin | 25 / 5726（TCC 编译固有） | 194 / 16506 |

劣化 100× 全部出现在熔断邻域（换代编译停顿 + 内存压力页错误）；无内存压力时全部路由亚毫秒~毫秒级。

## 修复：E1 管理写接口字段长度上限

战役数据膨胀主因的代码面闭环（公开 API 注册已有 3-32 先例，管理侧此前完全缺失；v1 同样无校验，属定向加固非保真回填）：

- 上限档位：username/name/nickname 64、password 128（客户端 SHA-256 hex=64）、email 128、phone 32、avatar 512、desc 1024、authList 4096
- 覆盖 18 站点：`route_http/auth.h` 9 处（用户 POST/repwd、角色 POST/PUT、权限分类 POST/PUT、权限组 POST/PUT、接口 PUT）+ `route_http/member.h` 9 处（用户 POST/PUT/repwd、用户组 POST/PUT、权限分类 POST/PUT、权限分组 POST/PUT）
- 门禁：write_regression 新增 E1 块（超限 8 组拒写且 DB 快照不变 + 恰好等于上限的边界值放行 + member PUT 超限 email/avatar 拒写）；smoke 41 项全绿零警告

## 遗留建议（本轮不落地）

1. **登录失败锁定/管理写频控**：仓库已有 R2（注册 1/min/IP）先例可循；管理写全部在 `/admin/` 会话门内，属纵深防御项
2. **xrt block 层换代释放缺口**：xserver 仓跟进（blockAlloc/blockFree 差值数据在探针报告）
3. **战具改进**：PERF 高压期抑制阈值在熔断邻域仍有漏网（rss<1200 才报），WRT-D 应以 resp=None 直接归噪声类

## 崩溃上下文存档

crash_001~004 的 gdb log 尾部均为线程批量 `exited with code 0/1` + `No stack`——进程被外部终止特征，与 2026-09-14"静默崩溃"结案（外部构建任务按名击杀）同签名。
