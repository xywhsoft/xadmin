# 内容系统静态编译原则符合性审计与整改清单（2026-09-16）

> 原则（用户定义）：依托 TCC 动态编译能力，把配置好的模型**直接编译为静态操作的插件代码**——
> 没有运行时的动态判断，和手写插件一样直接操作数据库。配置变更 = 重新生成。

## 一、本轮已落地的静态烘焙（缺口补全）

| 配置 | 烘焙形态 | 验证 |
|---|---|---|
| policies.softDelete=false | 生成 `DELETE FROM content_item WHERE id=?`（true=软删 UPDATE；SQL 语句整体烘焙，无分支） | 删除后行消失 ✓ |
| policies.auditTime=false | `#define MANAGED_CONTENT_TIME(now) (0)`，六处 INSERT/UPDATE 时间绑定走宏（true=(now)） | 时间戳 0 ✓ |
| policies.createRole/manageRole | 生成期解析角色名→authLevel（主库查询），烘焙 `#define MANAGED_*_AUTH_LEVEL <n>` + 守卫代码块（0=不设门） | 200/999 落盘 ✓；低权用户先被路由层 403（纵深防御），守卫在处理器内二线生效 |
| pages.detailFields | 允许清单烘焙为 `static const char* sAllowed[]`，公开详情按清单投影（未配置=空函数） | 公开 data 仅 title/price ✓ |
| pages.listColumns | 列配置+字段标题烘焙为 admin 页 JS 常量数组 | `listColumns:[{field:title...},{field:price...}]` ✓ |
| pages.fieldGroups | 生成期分配字段 group 键 → spec.presentation.groups → 编辑器表单实际分组 | form-meta groups=[主要内容,发布] ✓ |
| pages.displayGroups | 解析后进 presentation.displayGroups（透传完成；渲染消费见整改 R6） | 透传 ✓ |
| SEO 栏目模板×4 | categoryTitleTemplate 等在栏目公开 detail/contents 输出 `seo` 对象（变量 {categoryTitle}/{siteName}/{pluginXid} 等替换） | `测试栏目 - 栏目频道 - CMS 演示模型` ✓ |

**顺带修复的潜伏缺陷**（本轮暴露）：
1. 模板本地 `xrtReplace` 对无 NUL 的 xbuffer 直接 strdup → 越界读堆垃圾 → 任何已配置的 SEO 模板都会毒化 JSON 输出（**整个 SEO 模板特性自迁移起未真正可用过**；与 form.h 渲染器同族缺陷）。补 `xrtBufferAppendByte(buf, 0)`。
2. 栏目 detail/contents 处理器 `ValueSetOwn(data, xrtValueArrayGet(arr,0))` 借用引用后释放数组 → 悬垂（靠释放内存未复用侥幸存活）。改深拷贝（两处）。

## 二、现状架构与原则的差距（整改清单）

当前生成模型：**通用解释器模板 + spec.json 运行时解释**。字段定义/校验/取值/能力配置全部在运行时从 JSON 读取并解释——这正是原则要求消除的"动态判断"。按影响排序：

| 编号 | 违反项 | 现状 | 整改方向 | 量级 |
|---|---|---|---|---|
| R1 | **payload_json JSON 行存储** | 全部自定义字段存单列 JSON，每行每请求 parse/stringify | 字段→**类型化真实列**（schema.sql 按字段生成列+索引，SQL 直读直写）——用户点名的核心项 | 大（生成器数据层重写） |
| R2 | **运行时字段解释** | Managed_LoadSpec + ValidateData/CoerceFieldValues/ExtractTitle/Slug 每请求解释 spec | 生成期烘焙：校验代码/标题提取 SQL 常量/列清单静态化 | 大（随 R1 一并） |
| R3 | **162 个能力配置运行时读取** | Managed_AbilityPackConfig* 每次调用走 contracts.json 装载+查找 | 生成期烘焙为 `#define`/静态常量（变更配置=重新生成，符合"编译固化"语义；运行期调参场景可保留少数白名单键走配置） | 中（生成器+模板批量） |
| R4 | 页面配置运行时读取 | pageSize/maxScanRows/defaultSort 从 spec 读 | 同上烘焙常量 | 小（随 R3） |
| R5 | 编辑器表单动态 schema | form-meta API 动态返回字段 schema 驱动编辑器 | 字段表单定义直接烘焙进生成的 editor 页 | 中 |
| R6 | displayGroups 渲染消费 | 已透传 presentation.displayGroups，静态详情页未按分组渲染 | 静态化详情生成按分组输出段落 | 小 |
| R7 | 已对齐项 | 路由注册/菜单/权限绑定/本轮策略与投影均已静态 | — | ✅ |

## 三、建议实施顺序

R3+R4（低风险高确定性，收益立现）→ R6（小）→ R1+R2（大工程：数据层类型化，需迁移既有 payload 数据，建议单独排期+全链路门禁）→ R5。

每步验收标准：生成产物 grep 无对应运行时读取调用；e2e 断言生成代码包含烘焙常量；既有 phase1/fullchain/smoke/cms 门禁全绿。
