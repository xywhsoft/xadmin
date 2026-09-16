# 交接：xs 宿主换代内存泄漏（~2MB/代，净增不复收）

> 来源：x-admin 2026-09-16 7h 混合极端战役 + leakprobe 探针差分实验。
> 交接物：复现过程 / 计数器读数指南 / 已排除项 / 根因假设面 / 一步到位的定位工具链 / 验收门禁。
> 所有脚本与数据在 x-admin 仓 `tests/.runtime/`（leakprobe 系列 + extreme7 系列）。

## 1. 问题陈述

每次热重载（换代）后，xrt 堆活集合净增 ~1.9MB 且跨代不复收：

- **静止重载（无任何流量）**：每代 `BlockAllocBytes +~3.5MB`，`BlockFreeBytes +~1.65MB` → 净增 ~1.85MB/代，进程 RSS 同步线性上涨；
- **混合负载下**放大到 ~2-4MB/代；
- 战役全量口径（含流量与数据膨胀）：~9-10MB/代 RSS 增长，306 次重载累计触发 49 轮 2.6GB 内存熔断重启（`tests/.runtime/extreme7_campaign_stdout.log` 尾部 LEAK-49 终态）。

数据膨胀主因已在 x-admin 侧修复（E1，commit a505ced）；本问题是剥离流量后的**纯换代泄漏**。

## 2. 复现过程

### 2.1 探针架构（最小复现环境）

`tests/.runtime/leakprobe_launch.py` 构建夹具（端口 18700）：

- 完整 x-admin 业务链复制到 `tests/.runtime/leakprobe/`（db 副本建 `migration_smoke` 测试号、`plugin_runtime enabled=0`、`cp_url` 置空）；
- `xs.json` 的 `host_default.devfile` 指向 `leakprobe/host.c`——该文件 `#define ServiceInit XAdmin_ServiceInit` 后 `#include "../../main.c"`，再追加三条探针路由：
  - `GET /__probe/stats` → `xrtMemStatsGet` 全计数器 JSON（malloc/calloc/realloc/free + blockAlloc/blockFree + pooled/direct/backing 子计）；
  - `POST /__probe/reset` → `xrtMemStatsReset()` 清零（此后只测增量）；
  - `POST /__probe/reload` → `xsReloadHostSubmit` 提交换代（202 + reload id）；
  - `ServiceInit` 尾部 `xrtMemStatsEnable(true)`。

### 2.2 最小复现步骤

```bash
cd D:/GIT/x-admin
python tests/.runtime/leakprobe_launch.py     # 起夹具（前台或后台均可）
python tests/.runtime/leakprobe_diff.py       # 8 代静止重载差分
```

`leakprobe_diff.py` 每代流程：reset 基线 → `POST /__probe/reload` → 轮询 `/admin/login==200`（0.5s 间隔，最多 30s）+ 额外 3s 收割等待 → 重新登录取新 cookie → 读 stats 与 RSS，打印逐代 delta。

**判定标准**（与战役一致即为复现）：
- 每代 `blockAllocBytes - blockFreeBytes` ≈ +1.5~2MB，连续 8 代无回收趋势；
- RSS 每代 +1~3MB；
- `mallocBytes/freeBytes` 基本平衡（泄漏在 xrt 堆语义层，不是 libc 层裸漏）。

### 2.3 分层归因脚本（已在 x-admin 侧跑过，结论供参考）

| 脚本 | 实验 | 结论 |
|---|---|---|
| `leakprobe_diff.py` | 静止重载 ×8 | 纯换代泄漏 ~1.85MB/代（上面的基线数字） |
| `leakprobe_load.py` | 只读流量 + 重载 | 增量与静止重载同量级（读路径不放大泄漏） |
| `leakprobe_churn.py` | 插件启停/上传churn | 上传/churn 引入额外增长但主体仍是每代常数项 |
| `leakprobe_fuzz.py` | 渗透载荷 | 放大项=数据膨胀（已由 E1 修复），非泄漏本体 |
| `leakprobe_bisect.py` | 单类型载荷×单端点 60s 一轮 | 用于把"膨胀"与"泄漏"分开的二次定位 |

### 2.4 战役级复现（全量）

`python tests/campaign_extreme7.py`（GDB batch 启动 + 七类混合负载 + 2.6GB 熔断重启）。终态见 `tests/.runtime/extreme7/final.json`：306 reloads / 49 leak_cycles / 0 真实崩溃。

## 3. 计数器语义（读数指南，防误判）

以下均在 xrt 源码核实（`D:/GIT/xrt`）：

- **Block\* 计数的是 xrt 堆的全部分配**（不分用途）：pooled 尺寸类路径 `heap.c:686`（`__xrtHeapAllocPooled`）与 backing 大块路径 `heap.c:743`（`__xrtHeapAllocBacking`）都调 `__xrtMemStatsBlockAlloc`；释放对称在 `heap.c:825/846`。
- **`blockFree < blockAlloc` ≠ 自动是泄漏**——它表示活集合增长；`blockFree` 包含归还到池空闲链的块（`__xrtHeapReturn`）。**跨多代持续单向增长才是泄漏签名**（探针 8 代单调 + 战役 306 代单调已坐实）。
- 子计数器分工：`pooledAllocBytes`（池化类）/`directAllocBytes`/`backingAllocBytes`（底层大块）——先看泄漏落在哪一层；
- 每尺寸类分桶：stats 内部有 `ClassCalls/ClassBytes[XRT_MEM_STATS_CLASS_COUNT]`（`stats.c:318-321` 记账），可按类缩小泄漏块尺寸；
- `Malloc*/Free*` 是更底层分配计数，与 Block 层分开读。

## 4. 已排除项（勿重复排查）

1. **应用层数据膨胀**（战役主因）：管理写接口无长度校验 → 已在 x-admin E1 修复（a505ced）+ write_regression 门禁。
2. **TCC 解析器状态跨编译泄漏**：xserver `5f1a5b6` 已修（那是编译状态污染，F3），与本问题无关。
3. **F6 旧缓存同步销毁**：x-admin 侧已建延迟退役 256 槽修复（2026-09-11 战役）——但**退役槽的收割完整性**建议在 xs 侧顺手复核（见 §5-3）。
4. **崩溃类干扰**：战役 4 次"GDB 崩溃"全部是探针开发期外部 `taskkill //IM xs.exe` 按名误杀（`exited code 01` 无栈签名），与内存问题无关。

## 5. 根因假设面（按可疑度排序，待 xs 侧定位）

换代销毁链上的 xrt 堆对象，每代有分配、旧代未（全）释放的候选：

1. **每代线程栈**：xs 换代采用新线程栈（换代线程栈不同——x-admin 侧记忆库有此架构事实）。旧代请求线程退出时其栈若是 xrt 堆分配，回收路径是否覆盖异常/强制终止分支？
2. **旧代宿主侧对象**：TCC 上下文里挂在每代的串/符号/路由描述（`xsCreateTCC` 预置表相关）、reload worker 自身的临时结构。
3. **延迟退役收割**：x-admin 侧 CacheRetire 延迟槽的收割由宿主换代时序驱动——若收割回调在旧代销毁后仍触发或漏触发，槽内缓存（页面/模板/xvalue 树）滞留。
4. **xrt 分配器内部滞留**：池空闲链跨代只增不减（类碎片化）——若是这类，表现为 blockFree 已计但 RSS 不降，需与 Block 计数差分开看（探针两者都记了，对照即可分辨）。

## 6. 定位工具链（推荐路径：一步到位）

**首选：`XRT_FEATURE_MEMORY_DEBUG` 构建 + `xrtMemDebugReport` 前后差分**

- xrt 已带完整站点级调试设施：`__xrtMemDebugAlloc/Free` 在 `heap.c` 每笔分配记 file:line；公开导出 `xrtMemDebugReport`（`memory/debug_report.c:409`，text/JSON 双格式）与 `__xrtMemDebugCaptureLive`（程序化捕获活分配表）。
- 步骤：Debug 构建 xs+xrt → 探针起服 → reload 前捕获/导出一次 → 单次 reload + 收割等待 → 再导出 → 按 site 差分。**差分多出来的 file:line 就是滞留分配的精确归属**，直接判定 §5 中哪个假设成立（宿主 teardown 漏释放 vs 分配器滞留）。
- 建议在探针 host.c 加一条 `/__probe/memdebug` 路由调 `xrtMemDebugReport` 落盘，避免改动仓库脚本。

**备选（不想开 Debug 构建时）**：stats 子计数 + 尺寸类分桶先缩小范围（§3），再对可疑尺寸类 grep 调用点。

**操作警示**（探针开发期实测踩过）：
- 杀服务必须 **按 PID**，`taskkill //IM xs.exe` 会误杀同名全部实例（战役 4 次假崩溃的来源）；
- 探针头文件里 `xmemstats` 结构体字段须与链接内声明一致（uint64 vs uint64_t 混用编译错；`BackingFreeBytes` 字段不存在）；
- Windows 下 C 源走脚本文件打补丁，勿 heredoc（CRLF 拆断字符串字面量）。

## 7. 修复验收门禁建议

1. **单元级**：`leakprobe_diff.py` 静止重载 8 代，`blockAlloc-blockFree` 逐代净增 ≈ 0（允许首代预热），RSS 曲线走平；
2. **矩阵级**：xserver `reload_matrix` 套件加内存断言（N 代后 RSS 增量 < 阈值）；
3. **战役级**：重跑 `campaign_extreme7.py`，期望 `leak_cycles` 从 49 显著下降（数据膨胀已由 E1 消除，剩余熔断应基本消失）；
4. **xrt 回归**：`build.py --suite` 全绿。

## 8. 工件索引（x-admin 仓）

- 探针：`tests/.runtime/leakprobe/`（host.c）+ `leakprobe_launch/diff/load/churn/fuzz/bisect.py`
- 战役：`tests/campaign_extreme7.py` + `tests/.runtime/extreme7/`（findings/final/metrics/crash 上下文）
- 终报：`docs/campaign-extreme-2026-09-16.md`
