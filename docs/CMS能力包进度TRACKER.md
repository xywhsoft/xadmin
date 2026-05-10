# CMS 能力包进度 TRACKER

本文是历史进度表，记录已经完成和仍有缺口的能力包状态。后续开发的唯一执行入口已切换为 `CMS能力包收口执行SPEC.md`；本文不再作为“继续开发”的主线任务表。

当前主线冻结说明：

- 能力包后台统计、诊断、结果表格化横向补强已经收口，后续不再作为主线继续扩大。
- 后续必须先按 `CMS能力包收口执行SPEC.md` 的 `CORE-*` 顺序推进；新 SPEC 使用 `[ ] / [~] / [x] / [!] / [F]` 勾选状态跟踪。
- 单个能力包生产级细节增强进入冻结任务池，等核心主线完成后再逐项解冻。

状态约定：

- `done`：代码、文档和当前静态门禁均已通过。
- `partial`：已有可运行 V1，但仍有明确生产级缺口。
- `pending`：尚未实现或只停留在设计口径。
- `blocked`：依赖外部系统、基础设施或尚未定稿的上游设计。

常用验证：

```powershell
powershell -ExecutionPolicy Bypass -File tools\check_capability_packs.ps1
powershell -ExecutionPolicy Bypass -File tools\check_content_system.ps1
git diff --check
```

## P0 当前结论

| ID | 领域 | 状态 | 当前结论 | 下一步 |
| --- | --- | --- | --- | --- |
| P0-001 | 能力包总数 | done | 当前内置能力包为 21 个，`check_capability_packs` 为 0 error / 0 warning。 | 后续新增能力包时同步更新本表。 |
| P0-002 | 基础可运行 | partial | 无能力包时基础内容插件生成路径已被多项静态门禁覆盖，但 `content_item.category_id` 等历史字段仍待拆分。 | 进入代码拆分阶段时迁出遗留字段和模板片段。 |
| P0-003 | 能力包可运行 | partial | 启用能力包时 schema、权限、菜单、路由、配置表单、acceptance path 已有静态门禁。 | 接入生成后真实 HTTP smoke。 |
| P0-004 | 文档可跟踪性 | partial | 原 SPEC 设计上下文充分，但 `[~]` 大段任务过多。 | 使用本文作为后续执行入口。 |

## P1 动态路由系统

| ID | 任务 | 状态 | 已完成 | 剩余缺口 | 验证 |
| --- | --- | --- | --- | --- | --- |
| RTE-001 | 静态优先、动态兜底 | done | 请求链路为 static route -> dynamic route -> plugin static -> site static。 | 无。 | `check_content_system` route hot path scan |
| RTE-002 | xrt 正则集合预编译 | done | 动态路由使用 xrt 正则 API，copy-on-success 重编译，失败保留旧表。 | 无。 | `check_content_system` xrt regex scans |
| RTE-003 | 动态路由 capture 参数 | done | capture 0 不暴露，仅暴露用户捕获组，数量有固定上限。 | 无。 | `check_content_system` capture scans |
| RTE-004 | 动态路由权限同步 | done | 后台/前台 URI 权限同步支持静态优先、动态兜底。 | 无。 | `check_content_system` auth sync scans |
| RTE-005 | 提交期风险 warning | partial | 已覆盖静态 route key 冲突、宽泛规则、后台/API/静态资源前缀风险；slug/redirect 能力页已补规则说明，可查看公开前缀、动态正则、样例 URL 和提交期 warning；新增 `route-rule/validate` 管理期批量验证接口，汇总 slug/redirect/static warning；统一 URL 规则页已可表格查看规则快照 warning。 | 更完整规则编辑模型仍未完成。 | `dynamic route risk warning wiring` |
| RTE-006 | 伪静态规则体系 | partial | slug/redirect 已支持实例配置公开前缀、注册期正则字面量转义、风险 warning、能力页规则预览与冲突说明；统一规则计划已在管理侧汇总 slug、redirect、static 规则来源，并新增后台 `route-rule/plan` 只读接口读取实际 `static_rule` 持久化规则；跨能力包批量验证已通过 `route-rule/validate` 只读接口接入；相关能力包启用时会创建 `content_route_rule` 并在管理期同步规则快照，`route-rule/list` 支持表格化查看、按能力包/规则类型/状态/warningOnly 筛选，`route-rule/stats` 支持后台规则聚合统计，`route-rule/refresh` 支持显式刷新快照并表格化展示同步结果。 | 更完整规则编辑模型仍未完成。 | `dynamic route risk warning wiring` |

## P2 能力包功能状态

| ID | 能力包 | 状态 | 已完成 V1 | 剩余生产级缺口 | 建议优先级 |
| --- | --- | --- | --- | --- | --- |
| CAP-001 | `content.comment` | partial | 评论树、审核、批量审核、单条审核状态边界、通知、审计、带内容标题/长度/链接数/积压时长/风险分/风险原因的专用待审队列，待审队列支持内容/作者/正文/IP 筛选，审核日志视图、审核日志和通知记录筛选、审核统计表格化、待审风险分布统计、批量审核结果表格化、单条审核/隐藏结果表格化、基础反垃圾、昵称黑白名单、正文/UA 禁用片段、请求边界、计数刷新可观测。 | 更细反垃圾策略、人工审核工作台继续强化。 | P2 |
| CAP-002 | `content.sensitive` | partial | 敏感词、scope、策略、日志、清理、导入、状态边界、ASCII 词边界、`cjkLoose` 中文宽松匹配、零宽字符绕过识别、命中上限、词库分组与后台分组筛选、命中日志筛选、词库/命中统计表格化、近期高频命中词和高频分组统计；导入、检测和日志清理结果已表格化。 | 更完整中文分词和语义级匹配策略。 | P2 |
| CAP-003 | `content.static` | partial | 规则、规则状态边界、规则路径/状态筛选、规则 warning 列、管理期 pathPattern 展开预览、任务、产物、预览、清理、任务状态/规则/内容筛选、产物内容/路径筛选、后台静态化统计表格化、受限内容 noindex、自动生成上限、产物持久化失败时任务状态回写失败、单条失败任务重试、有界失败任务批量重试；静态生成、规则预览、产物清理、单条任务重试、失败任务批量重试和 URL 规则快照刷新结果已表格化。 | 后台任务队列、完整伪静态规则编辑模型继续增强。 | P1 |
| CAP-004 | `content.tag` | partial | 标签列表、状态边界、绑定、后台关联筛选、即时创建、数量上限、合并、公开输出上限、有界批量启用/停用、后台标签统计表格化；标签合并和批量状态结果已表格化。 | 更完整可视化拖拽/批量运营体验。 | P3 |
| CAP-005 | `content.topic` | partial | 专题列表、状态边界、绑定模式、后台关联筛选、批量绑定上限、排序辅助、公开输出上限、有界批量启用/停用、后台专题统计表格化；专题内容排序和批量状态结果已表格化。 | 更完整专题编排 UI。 | P3 |
| CAP-006 | `content.like` | done | 点赞前后台接口、明细/排行/统计表格、访客边界和配置隔离已完成。 | 暂无必须项。 | P4 |
| CAP-007 | `content.view-stat` | done | 访问记录、排行、日统计、统计表格、看板页和服务端 IP 采集已完成。 | 暂无必须项。 | P4 |
| CAP-008 | `content.seo` | done | 内容/栏目 SEO、公开 head、模板配置、后台管理、状态边界、有效预览、能力页模板变量清单、内容模板变量取值预览、栏目模板变量取值预览、未替换变量 warning、内容/栏目预览结果表格化。 | 暂无必须项。 | P3 |
| CAP-009 | `content.slug` | partial | slug 字段、检测、预览、历史、历史内容/旧值/新值/状态筛选、批量修复、动态漂亮 URL、可配置公开前缀、能力页规则说明、slug/redirect/static 统一规则计划、批量验证、统一 URL 规则表格筛选、URL 规则统计、静态规则 warning/预览协同、启用时物理 slug 列和索引；slug 检测/预览、批量修复结果以及统一 URL 规则计划、验证、统计和快照刷新结果已表格化。 | 完整伪静态规则编辑模型继续增强。 | P1 |
| CAP-010 | `content.redirect` | partial | 301/302、命中统计、批量导入、跳转环 warning、动态跳转入口、可配置公开前缀、跳转规则来源/目标/状态筛选、能力页规则说明、slug/redirect/static 统一规则计划、批量验证、规则快照持久化、统一 URL 规则表格筛选视图、URL 规则统计和静态规则 warning/预览协同；跳转规则导入预检/确认结果以及统一 URL 规则计划、验证、统计和快照刷新结果已表格化。 | 完整伪静态规则编辑模型继续增强。 | P1 |
| CAP-011 | `content.category` | partial | 栏目树 API、排序、拖拽迁移、状态边界、关键词过滤、搜索上下文保留、表格展开/收起、树状态摘要、删除保护、删除拒绝依赖计数、SEO、模板布局、maxDepth、树行 `childCount/contentCount` 可观测；未启用栏目能力包时后端忽略 `categoryId` 过滤、保存、导入和导出语义；启用栏目能力包时新增 `content_category_bind` 绑定表，新保存/删除内容、导入提交和版本恢复都会同步绑定表，栏目计数、公开栏目内容列表、tag/topic 内容聚合、related 同栏目规则与公开列表、搜索结果/重建候选、sitemap 预览/刷新、JSON 导出和工作流发布后的派生同步已优先走绑定表，schema ensure 会从旧 `content_item.category_id` 幂等回填绑定表；后台新增 `category.bind.status` 诊断 legacy mirror、active bind、missing bind 和 mismatch 计数，并返回缺失/不一致漂移样本表格，提供显式 `category.bind.backfill` 修复入口，能力页迁移状态和回填结果已表格化。 | 更完整树控件和 `content_item.category_id` 物理字段彻底迁出。 | P1 |
| CAP-012 | `content.media` | done | 媒体表、引用表、后台 MIME/状态筛选、封面/正文引用、尺寸识别、删除保护、批量操作结果表格化、附件上传入口、按附件 XID 读取并回填元信息、上传完成后跨窗口自动打开预填弹窗。 | 暂无必须项。 | P3 |
| CAP-013 | `content.revision` | partial | 版本快照、按内容/动作/状态过滤列表、版本统计表格化、字段 diff、恢复、长文本/媒体/结构化 diff、服务端类型化 diff 元数据、变更类型和长度摘要；diff row 新增 `diffMode`、行数统计、数值前后值和 `numberDelta`，后台与编辑器恢复预览均可显示数值差异；布尔 diff 已输出 `beforeBool/afterBool`，日期时间字段已输出 `datetime` 模式和时间文本；枚举类字段已输出 `enum` 模式和 before/after label；`categoryId` 已输出栏目标题/路径 relation label；自定义内容关联字段可按 content title 输出 relation label；版本恢复会同步 `content_category_bind`，恢复结果已表格化。 | 更复杂外部关联字段专用 diff。 | P3 |
| CAP-014 | `content.workflow` | partial | 提交/审核/发布/下线/定时发布、日志、待办、通知、工作流统计表格化、工作流动作结果表格化、到期定时发布执行结果表格化、日志按内容/审核人/动作筛选、待办按审核人/状态筛选、通知按内容/审核人/动作/已读筛选、派生同步；`requiredApprovals` 次数型多级审批 V1 已接入，未达到通过次数前只记录审核动作，达到后再发布；工作流日志已记录 session operatorId，并支持 `requireDistinctApprovers` 阻止同一操作者重复凑审批次数；工作流统计已输出审批配置和有界定时发布到期诊断。 | 多级节点模型、自动任务。 | P2 |
| CAP-015 | `content.search` | partial | 索引、分片重建结果表格化、索引健康统计表格化、后台 q 筛选、栏目收窄筛选、权重、摘要、高亮、边界、CJK 标点归一化、CJK 连续字宽松 LIKE、标题精确命中加权、标题前缀加权、完整短语命中加权、查询词覆盖度加权、CJK 二元词覆盖加权、新鲜度加权，能力页搜索测试结果和排序解释已表格化展示分项得分。 | 更完整中文分词、更复杂相关性。 | P2 |
| CAP-016 | `content.sitemap` | partial | sitemap/RSS/robots、缓存、分片、dirty 标记、TTL、统计、后台内容/状态筛选、只读刷新计划预检表格化、手动刷新结果表格化、刷新上限 UI、dirty 原因/内容/时间元数据读取、缓存文件存在性/大小/更新时间诊断。 | 后台任务队列。 | P2 |
| CAP-017 | `content.related` | partial | 手工关联、状态边界、后台来源/关联/类型/状态筛选、同栏目规则、标签/专题加权、有界刷新和输出、只读规则分值预览、公式和原因解释表格、规则重建结果表格化、后台关联统计表格化、规则配置只读诊断；同栏目规则的候选、重建和发布刷新已优先读取 `content_category_bind`，旧 `category_id` 仅保留为老数据兜底。 | 更完整复杂规则计算器。 | P3 |
| CAP-018 | `content.form` | partial | 表单定义、定义状态边界、提交处理状态边界、通知已读状态边界、字段辅助设计、字段新增/更新/删除/排序、字段预览/点选回填、空字段名/重复字段名/required 未命中/required 重复提示与保存校验、校验、提交、提交记录筛选、提交统计表格化、提交处理结果表格化、通知记录筛选、通知统计表格化、通知已读/未读/重放结果表格化、导出结果表格化、通知日志、通知已读状态、本地通知统计、本地通知重发、反垃圾边界。 | 完整拖拽设计器、外部通知投递。 | P2 |
| CAP-019 | `content.access` | partial | public/login/level/group/password/paid/private、模式边界、规则状态边界、后台规则筛选、后台权限统计表格化、栏目继承、哈希升级。 | 正式订单插件接入。 | blocked |
| CAP-020 | `content.audit-log` | partial | 核心和能力包写操作审计、字段差异、IP 来源、清理、后台过滤 UI、只读详情视图、后台审计统计表格化和操作者分布诊断；审计清理结果已表格化。 | 外部任务覆盖。 | P3 |
| CAP-021 | `content.import-export` | partial | 预检、确认、失败行、冲突策略、分片导入导出、回放边界、字段计划预览、导出字段可观测、JSON 导出结果表格化、字段计划结果表格化、导入预检/确认结果表格化、分片导入结果表格化、任务状态筛选、导入结果状态细分、最近失败导入任务样本、导入提交同步 `content_category_bind` 和导入导出任务统计表格化。 | 流式上传解析、后台任务队列。 | P2 |

## P3 生成边界与拆分

| ID | 任务 | 状态 | 已完成 | 剩余缺口 | 验证 |
| --- | --- | --- | --- | --- | --- |
| GEN-001 | 未启用能力包不注册 | done | 路由、菜单、权限、schema、runtime manifest 均尊重 enabled。 | 无。 | `managed route capability boundary scan` |
| GEN-002 | 未启用能力包不复制源码 | partial | source/include/template/assets 声明复制已只遍历启用能力包。 | 真实业务源码仍大量留在大模板。 | `capability manifest generation wiring` |
| GEN-003 | `#ifdef XADMIN_CAP_*` 过渡宏 | partial | build.defines 已生成。 | 大模板逐块包裹和迁出未完成。 | 待新增 |
| GEN-004 | 基础内容插件纯净度 | partial | 大部分能力包表和页面已迁到能力包条件；栏目未启用时不再接受外部栏目过滤/保存/导入/导出语义；栏目关系已开始迁入 `content_category_bind` 能力包表，并有后台迁移状态诊断和显式回填入口辅助后续拆除 legacy mirror。 | `content_item.category_id` 兼容物理字段与更细模板代码仍待拆。 | `managed category schema boundary scan` / `base content write guard` / `import/export paging UI wiring` |
| GEN-005 | 能力包代码桩和 Hook | partial | 已识别 slot/hook 清单和 manifest/provider discovery。 | 尚未真正按 hook 拆出能力包业务源码。 | 待新增 |

## P4 验收与质量门禁

| ID | 任务 | 状态 | 已完成 | 剩余缺口 | 验证 |
| --- | --- | --- | --- | --- | --- |
| QA-001 | 能力包 manifest/contract/schema 门禁 | done | 21 包 0 error / 0 warning。 | 新增包时持续维护。 | `check_capability_packs.ps1` |
| QA-002 | 内容系统静态汇总门禁 | done | JS、模板、C 编译、路由边界、access 边界、能力页中文等已覆盖。 | 新增功能同步补 marker。 | `check_content_system.ps1` |
| QA-003 | 最小 acceptance path | done | 每个能力包已有 API 和页面 smoke 路径。 | 真实生成物服务未固定跑 HTTP smoke。 | `smoke_capability_acceptance.ps1` |
| QA-004 | 生成后真实 HTTP smoke | partial | smoke 脚本已支持 `ManifestPath`、`RuntimeDir` 和 enabled-only 子集；`smoke_generated_runtime.ps1` 固定 runtime 清单校验、基础内容核心 API/页面探测和能力包 smoke 串联入口；`smoke_generated_runtime_live.ps1` 已提供可选启动 `xs.exe`、等待 HTTP 可访问、按 `GenerateXid` 调用真实内容生成接口、自动定位 runtime、启用/重载生成插件、执行 runtime smoke、按需停止自启动进程的 live 包装；`tools/fixtures/content_smoke_model.json` 和 `tools/smoke_content_generation_live.ps1` 已固定一组全能力包验收模型并串联保存、生成、启用、HTTP smoke；live smoke 已支持 `AdminBase` 和 `CookieHeader`，可带自定义后台入口与已有登录态执行；`tools/get_admin_cookie.ps1` 可按真实后台登录流程生成 `XSID=...`，总入口可通过 `AdminUsername/AdminPassword/AdminPasswordHash` 自动获取 CookieHeader；`tools/check_cms_capability_workflow.ps1` 已提供本地总入口，默认跑静态门禁和 diff 检查，显式传 `-RunLiveSmoke` 时才跑受保护后台链路；`.github/workflows/cms-capability.yml` 已接入静态门禁 CI。 | 真实受保护后台 live smoke 需要提供有效账号密码或已有 Cookie，不能在 CI 中绕过鉴权。 | `smoke acceptance script syntax` |
| QA-005 | 文本和本地化门禁 | done | manifest、xform、能力页、模板 mojibake 均有门禁。 | 新页面继续补可见文本回归项。 | `check_capability_packs.ps1` / `check_content_system.ps1` |

## P5 下一批建议执行顺序

| 顺序 | ID | 目标 | 原因 |
| --- | --- | --- | --- |
| 1 | CAP-015 | 搜索复杂相关性 V1 | 已完成标题精确命中、标题前缀加权、完整短语命中、查询词覆盖度加权、栏目收窄筛选、能力页搜索测试表格、排序解释表格和索引分片重建结果表格；下一步再评估中文分词。 |
| 2 | CAP-009 / CAP-010 / RTE-006 | slug/redirect/伪静态规则统一 | 已完成前缀配置、正则字面量转义、能力页规则说明、联合前缀检查、slug/redirect/static 统一规则计划、`route-rule/plan` 后台只读计划接口、`route-rule/validate` 管理期批量验证、`content_route_rule` 规则快照持久化、`route-rule/list` 表格视图，以及计划/验证/统计表格化诊断；下一步做更完整规则编辑模型。 |
| 3 | CAP-011 / GEN-004 | 栏目遗留字段迁出 | 已完成第一步：新增栏目绑定表，接入内容保存/删除、导入提交、版本恢复、栏目计数、公开栏目内容列表、tag/topic 内容聚合、related 同栏目规则与公开列表、搜索结果/重建候选、sitemap 预览/刷新与公开 fallback、slug/detail 输出、JSON 导出和工作流派生同步，并增加旧 `category_id` 到绑定表的幂等回填、后台迁移状态诊断、缺失/不一致样本表、显式回填入口和表格化诊断 UI；下一步继续迁出遗留物理字段。 |
| 4 | QA-004 | 生成后真实 HTTP smoke | 已新增固定验收模型、一键 live smoke 脚本、自定义后台入口、`CookieHeader` 鉴权传递、`get_admin_cookie.ps1` 登录态获取、本地总入口和静态门禁 CI；下一步在具备真实账号时运行完整 live smoke。 |
| 5 | GEN-005 | 能力包源码拆分和 Hook | 第三阶段核心，但应在能力包功能继续稳定后推进。 |

