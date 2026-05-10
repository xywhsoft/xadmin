# CMS 能力包收口执行 SPEC

本文是后续开发的唯一执行入口。旧的 `路由动态化与CMS能力包建设SPEC.md` 保留设计背景，`CMS能力包进度TRACKER.md` 保留历史状态；实际开发必须按本文勾选项顺序推进。

## 0. 状态标记

- `[ ]`：未开始。
- `[~]`：进行中，必须有正在收口的明确子任务。
- `[x]`：完成，代码、文档、静态门禁已通过；要求 live smoke 的任务还必须有 HTTP 验收记录。
- `[!]`：阻塞，必须写明阻塞原因和解除条件。
- `[F]`：冻结，不允许作为当前主线继续开发。

## 1. 执行硬规则

- [x] 当前横向补强主线冻结：后台统计、诊断表格化、零散 UI 补强不再作为主线继续扩大。
- [x] 后续开发入口切换到本文，旧 tracker 只保留历史状态。
- [ ] 每次开发开始前，先把当前子任务从 `[ ]` 改为 `[~]`。
- [ ] 每次开发结束前，只能把已通过验证的子任务改为 `[x]`。
- [ ] 不允许跳过前置 `CORE-*` 主线任务，除非是在修复阻塞性编译、门禁或运行错误。
- [ ] 请求热路径不得新增数据库扫描、正则编译、复杂冲突检查。
- [ ] 管理页必须遵守 xadmin 范式：表格管理数据，弹窗新增/编辑，看板独立页面，弹窗保存/取消按钮在底部横线下方。
- [ ] 未启用能力包必须不加载；过渡期允许 `#ifdef`、注册开关和运行时挂载检查，最终目标是不生成、不编译、不注册、不执行。

## 2. 当前冻结清单

- [F] `FRZ-001` 能力包后台统计、诊断、结果表格化横向补强
  - 冻结原因：该批次已形成可用基础，但继续横向追加不会关闭核心剩余任务。
  - 解冻条件：`CORE-001` 到 `CORE-004` 完成后，按单个能力包生产级任务重新排期。

- [F] `FRZ-002` 单个能力包体验细节增强
  - 冻结原因：目前缺少统一收口目标，容易扩大范围。
  - 解冻条件：进入 `CAP-*` 阶段后逐项解冻。

- [F] `FRZ-003` Hook/slot 彻底拆分实现
  - 冻结原因：必须先完成规则模型、纯净度边界和真实验收，否则拆分风险过高。
  - 解冻条件：`CORE-001` 到 `CORE-004` 全部完成。

## 3. 主线总清单

- [x] `CORE-001` 统一 URL 规则编辑模型
  - 完成标准：`content_route_rule` 从只读快照升级为可编辑管理模型，支持 slug/redirect/static 三类规则的列表、新增、编辑、启停、排序和保存期验证。
  - 验证：`check_cms_capability_workflow.ps1`；新增规则模型静态门禁；管理 API HTTP smoke。

- [x] `CORE-002` 动态路由规则刷新闭环
  - 完成标准：URL 规则变更后只在管理期重建动态路由正则组；失败保留旧表并返回错误；请求期不访问数据库、不编译正则。
  - 验证：动态路由门禁；刷新失败保旧表 marker；HTTP smoke 验证新规则命中。

- [x] `CORE-003` 基础内容插件纯净度收口
  - 完成标准：未启用 `content.category` 时不生成栏目业务 schema、字段、页面入口、导入导出语义；`content_item.category_id` 兼容镜像完成迁移方案并进入可删除状态。
  - 验证：base-only 生成物扫描；category disabled HTTP smoke；迁移/回填门禁。

- [x] `CORE-004` 真实生成物验收闭环
  - 收口记录：已通过受保护后台 live smoke；使用临时后台账号走真实登录，未绕过鉴权。
  - 完成标准：用固定内容模型生成插件、启用插件、执行基础和 enabled-only 能力包 HTTP smoke；不绕过鉴权。
  - 验证：`check_cms_capability_workflow.ps1 -RunLiveSmoke`。

- [x] `CORE-005` 后台任务队列基础
  - 完成标准：为 static、sitemap、import-export 建立统一后台任务表、执行入口、状态查询、失败重试边界；不要求分布式调度。
  - 验证：队列 schema/API/UI 门禁；三个能力包各一个任务 smoke。

- [x] `CORE-006` 能力包源码拆分第一批
  - 完成标准：选择 1 个低风险能力包和 1 个中风险能力包，从大模板迁出到声明式 source/include/template，并保持未启用时不复制、不编译。
  - 验证：生成物 source 列表扫描；disabled pack 编译扫描；能力包 acceptance smoke。

## 4. CORE-001 统一 URL 规则编辑模型

- [x] `CORE-001-A` 数据契约定稿
  - [x] 明确 `content_route_rule.rule_type`：允许 `slug`、`redirect`、`static`。
  - [x] 明确 `content_route_rule.source_pack`：记录来源能力包 ID，例如 `content.slug`；旧 `pack_id` 保留为兼容索引键。
  - [x] 明确 `content_route_rule.match_pattern`：保存动态路由正则或静态规则 pattern；旧 `pattern` 保留为兼容显示字段。
  - [x] 明确 `content_route_rule.target_path`：保存目标路径、目标 handler 或静态输出规则。
  - [x] 明确 `content_route_rule.priority`：只影响同组动态规则匹配顺序。
  - [x] 明确 `content_route_rule.status`：启用/停用边界。
  - [x] 明确 `content_route_rule.warning`：保存期风险提示，不进入请求热路径。
  - [x] 明确 `content_route_rule.compile_status` 和 `compile_message`：记录最近一次管理期编译结果。
  - [x] 明确 `content_route_rule.managed_flag`：区分系统生成快照和人工编辑规则。
  - [x] 更新本文状态和相关历史 tracker 摘要。

- [x] `CORE-001-B` 后端列表接口
  - [x] `route-rule/list` 支持 `ruleType` 筛选。
  - [x] `route-rule/list` 支持 `sourcePack` 筛选。
  - [x] `route-rule/list` 支持 `status` 筛选。
  - [x] `route-rule/list` 支持 `warningOnly` 筛选。
  - [x] `route-rule/list` 支持 `keyword` 筛选。
  - [x] `route-rule/list` 必须有 `limit` 上限。
  - [x] 返回稳定表格字段：ID、类型、来源、匹配规则、目标、优先级、状态、warning、编译状态、更新时间。
  - [x] 增加 contracts 标记。
  - [x] 增加静态门禁 marker。

- [x] `CORE-001-C` 后端保存接口
  - [x] 新增 `route-rule/save`。
  - [x] 支持新增规则。
  - [x] 支持编辑规则。
  - [x] 保存时校验 rule type、source pack、status、priority。
  - [x] 保存时编译正则。
  - [x] 保存时检查静态 route 直接冲突。
  - [x] 保存时生成宽泛规则 warning。
  - [x] 保存时生成后台/API/静态资源前缀 warning。
  - [x] 保存失败不刷新运行期动态路由表。
  - [x] 返回表格化保存结果。
  - [x] 增加 contracts 标记。
  - [x] 增加静态门禁 marker。

- [x] `CORE-001-D` 后端启停和排序接口
  - [x] 新增 `route-rule/status`。
  - [x] 新增 `route-rule/sort`。
  - [x] 启停接口必须有明确 ID 和目标状态。
  - [x] 排序接口必须有有界输入。
  - [x] 更新成功后触发管理期刷新。
  - [x] 更新失败不得影响旧动态路由表。
  - [x] 返回表格化更新结果。
  - [x] 增加 contracts 标记。
  - [x] 增加静态门禁 marker。

- [x] `CORE-001-E` 管理页表格
  - [x] 表格展示规则类型。
  - [x] 表格展示来源能力包。
  - [x] 表格展示匹配规则。
  - [x] 表格展示目标路径。
  - [x] 表格展示优先级。
  - [x] 表格展示启用状态。
  - [x] 表格展示 warning。
  - [x] 表格展示编译状态。
  - [x] 表格展示更新时间。
  - [x] 行操作包含编辑、启停、排序入口。
  - [x] 筛选区使用表格页常规输入控件。
  - [x] 增加页面静态门禁 marker。

- [x] `CORE-001-F` 新增/编辑弹窗
  - [x] 弹窗包含规则类型。
  - [x] 弹窗包含来源能力包。
  - [x] 弹窗包含匹配规则。
  - [x] 弹窗包含目标路径。
  - [x] 弹窗包含优先级。
  - [x] 弹窗包含状态。
  - [x] 保存前展示保存期 warning。
  - [x] 保存/取消按钮位于弹窗底部。
  - [x] 按钮区上方有横线分隔。
  - [x] 增加页面静态门禁 marker。

- [x] `CORE-001-G` 静态门禁
  - [x] 检查 `route-rule/list`、`save`、`status`、`sort` 接口存在。
  - [x] 检查管理页新增、编辑、启停、排序按钮存在。
  - [x] 检查保存期风险提示代码存在。
  - [x] 检查请求热路径没有新增 DB 查询。
  - [x] 检查请求热路径没有新增正则编译。
  - [x] 检查 contracts 与页面能力声明一致。
  - [x] `check_cms_capability_workflow.ps1` 通过。

## 5. CORE-002 动态路由规则刷新闭环

- [x] `CORE-002-A` 规则变更触发刷新
  - [x] `route-rule/save` 成功后触发刷新。
  - [x] `route-rule/status` 成功后触发刷新。
  - [x] `route-rule/sort` 成功后触发刷新。
  - [x] 刷新只在管理期执行。

- [x] `CORE-002-B` copy-on-success 保旧表
  - [x] 新规则表构建在临时结构中完成。
  - [x] 正则组编译失败时返回错误详情。
  - [x] 编译失败时旧动态路由表继续服务。
  - [x] 编译成功后原子替换活动表。
  - [x] 增加静态门禁 marker。

- [x] `CORE-002-C` 权限同步边界
  - [x] 动态规则 route-level 权限继续走统一 URI 权限缓存。
  - [x] 业务级权限继续由 handler 处理。
  - [x] 不在请求期访问数据库查询权限。
  - [x] 增加权限同步门禁 marker。

- [x] `CORE-002-D` HTTP 验收路径
  - [x] 通过生成插件注册动态路由，验证动态路由编译和命中链路。
  - [x] 通过 HTTP 验证新规则命中正确内容。
  - [x] 验证完成后临时后台账号已软删除。
  - [x] 记录 smoke 命令和结果：见 `13. 生产收口记录`。

## 6. CORE-003 基础内容插件纯净度

- [x] `CORE-003-A` base-only 生成物扫描清单
  - [x] 扫描无能力包生成物中的栏目残留。
  - [x] 扫描无能力包生成物中的 SEO 残留。
  - [x] 扫描无能力包生成物中的 slug 残留。
  - [x] 扫描无能力包生成物中的 redirect 残留。
  - [x] 扫描无能力包生成物中的 static 残留。
  - [x] 将残留位置写入本文。
  - 残留清单：
    - `managed_main.c.tpl` 基础 `content_item` schema 仍包含 `category_id`，这是 `CORE-003-B` 的迁移对象。
    - `managed_main.c.tpl` 仍包含 slug/SEO/redirect/static/category 业务函数；当前由能力包挂载检查控制运行，源码拆分留到 `CORE-006`。
    - `content_generation.h` 已按 `bCategoryPack` 条件生成 `generated/categories.html`，无能力包输出清单不包含栏目页。
    - `check_content_system.ps1` 已覆盖基础 schema 不创建能力包表、栏目页面条件生成和栏目能力边界扫描。

- [x] `CORE-003-B` `content_item.category_id` 迁移收口
  - [x] 确认绑定表回填路径覆盖保存、删除、导入、版本恢复。
  - [x] 确认公开列表、公开详情、聚合、搜索、sitemap、workflow 已优先读取绑定表。
  - [x] 确认漂移诊断和回填入口可用。
  - [x] 决定物理字段移除或兼容保留边界。
  - [x] 更新 schema 和迁移说明。
  - 迁移结论：`content_item.category_id` 当前作为 legacy mirror 兼容保留，写入语义已由 `content.category` 挂载检查收口；物理删除进入 `CORE-006` 后的源码拆分/迁移窗口，避免一次性破坏历史查询和旧数据升级路径。

- [x] `CORE-003-C` 栏目能力包 schema 条件化
  - [x] 栏目相关表只在 `content.category` 启用时生成。
  - [x] 栏目相关字段只在 `content.category` 启用时生成或迁移。
  - [x] 栏目相关索引只在 `content.category` 启用时生成。
  - [x] 增加 base-only schema 扫描门禁。
  - 边界说明：`content_item.category_id` 按 `CORE-003-B` 作为 legacy mirror 兼容保留，不再接受未启用栏目时的外部写入语义；栏目业务表、绑定表、栏目扩展字段和栏目索引已由 `content.category` 挂载检查和 pack schema 控制。

- [x] `CORE-003-D` 栏目能力包页面入口条件化
  - [x] 未启用栏目时不注册栏目页面。
  - [x] 未启用栏目时不注册栏目菜单。
  - [x] 未启用栏目时不注册栏目权限。
  - [x] 未启用栏目时不注册栏目 API。
  - [x] 增加 route/menu/auth 扫描门禁。

- [x] `CORE-003-E` base-only HTTP smoke
  - [x] 基础后台页面可访问。
  - [x] 基础后台 API 可访问。
  - [x] 公开列表 API 可用。
  - [x] runtime manifest、managed、contracts 均为 0 enabled pack。
  - [x] 响应不加载栏目能力包页面、API 和 manifest 项。

## 7. CORE-004 真实生成物验收闭环

- [x] `CORE-004-A` 本地账号/Cookie 输入方式固定
  - 收口记录：live smoke 支持 `CookieHeader`、明文密码、服务端 hash、安全后台入口和自启动服务后登录。
  - [x] 支持 `CookieHeader`。
  - [x] 支持 `AdminUsername/AdminPassword`。
  - [x] 支持 `AdminPasswordHash`。
  - [x] 支持安全后台入口：`AdminLoginPath` 用于登录换取 Cookie，`AdminBase` 继续用于 `/admin/*` 后台 API。
  - [x] 不绕过登录保护。

- [x] `CORE-004-B` 固定模型生成
  - [x] 使用 `tools/fixtures/content_smoke_model.json`。
  - [x] 调用真实生成接口。
  - [x] 获取实际 `pluginXid`。
  - [x] 定位 runtime 目录。

- [x] `CORE-004-C` 启用与重载
  - [x] 通过后台接口启用生成插件。
  - [x] 通过后台接口重载生成插件。
  - [x] 失败时输出明确错误。

- [x] `CORE-004-D` 基础 HTTP smoke
  - [x] 后台基础页面可访问。
  - [x] 后台基础 API 可访问。
  - [x] 公开列表 API 可访问。
  - [x] 公开详情 API 可访问。

- [x] `CORE-004-E` enabled-only 能力包 smoke
  - [x] 读取 runtime manifest。
  - [x] 只 smoke 已启用能力包。
  - [x] 每个启用包执行 acceptance API。
  - [x] 每个启用包执行 acceptance page。
  - [x] 记录 smoke 结果。

## 8. CORE-005 后台任务队列基础

- [x] `CORE-005-A` 任务模型
  - [x] 定义任务表 schema。
  - [x] 字段包含 task_type。
  - [x] 字段包含 target_type 和 target_id。
  - [x] 字段包含 status 和 progress。
  - [x] 字段包含 payload、result、error。
  - [x] 字段包含 retry_count。
  - [x] 字段包含 create/update/finish 时间。

- [x] `CORE-005-B` 管理 API
  - [x] 创建任务接口。
  - [x] 任务列表接口。
  - [x] 任务详情接口。
  - [x] 取消任务接口。
  - [x] 重试任务接口。
  - [x] 列表接口必须有 limit 和固定筛选。

- [x] `CORE-005-C` 管理页
  - [x] 独立任务看板页。
  - [x] 任务列表表格。
  - [x] 任务详情弹窗。
  - [x] 失败重试操作。
  - [x] 取消操作。

- [x] `CORE-005-D` static 接入
  - [x] 静态生成可进入任务队列。
  - [x] 失败批量重试可进入任务队列。
  - [x] 保留当前小任务同步路径。

- [x] `CORE-005-E` sitemap 接入
  - [x] sitemap 刷新可进入任务队列。
  - [x] 保留当前同步刷新路径。

- [x] `CORE-005-F` import-export 接入
  - [x] 大导入可进入任务队列。
  - [x] 大导出可进入任务队列。
  - [x] 保留当前小批量同步路径。

## 9. CORE-006 能力包源码拆分第一批

- [x] `CORE-006-A` 拆分候选选择
  - [x] 选择 1 个低风险能力包：`content.slug`，已有声明式 `source/include` 骨架，先作为复制/编译边界验证样板。
  - [x] 选择 1 个中风险能力包：`content.like`，业务面较小但有公开/后台 API、schema 和统计，适合验证第二个能力包源码声明链路。
  - [x] 写明选择理由：`content.slug` 验证低风险文件复制和 disabled 不编译；`content.like` 验证有前后台接口能力包的声明式源码边界。
  - [x] 写明回滚方式：移除候选 pack.json 的 `sourceFiles/includeFiles` 声明并删除对应 `source/include` 文件，生成器会恢复为只生成大模板。

- [x] `CORE-006-B` source/include 声明补齐
  - [x] pack manifest 声明源文件：`content.slug/source/content_slug_pack.c`、`content.like/source/content_like_pack.c`。
  - [x] pack manifest 声明头文件：`content.slug/include/content_slug_pack.h`、`content.like/include/content_like_pack.h`。
  - [x] pack manifest 声明 include 目录：生成器从 `includeFiles` 自动加入 includeDirs。
  - [x] 检查路径不越出能力包目录：继续使用 `Content_GeneratedRelativePathSafe` 和声明源码编译门禁覆盖。

- [x] `CORE-006-C` 大模板 `#ifdef` 过渡
  - [x] 未迁出的代码受 `XADMIN_CAP_*` 宏保护：候选声明源码 include/link 只在 `XADMIN_CAP_CONTENT_SLUG/LIKE` 下生效。
  - [x] 未迁出的注册逻辑受能力包挂载检查保护：运行期路由、菜单、schema 仍按 `Managed_AbilityPackMounted`/`b*Pack` 边界注册。
  - [x] 增加静态门禁 marker：`capability source macro boundary`。

- [x] `CORE-006-D` disabled pack 编译验证
  - [x] 禁用候选能力包时不复制源码：生成器在 `!bEnabled` 时跳过 `Content_AppendDeclaredPackFiles`。
  - [x] 禁用候选能力包时不 include 头文件：大模板 include 受 `XADMIN_CAP_CONTENT_SLUG/LIKE` 宏保护。
  - [x] 禁用候选能力包时不编译候选源码：build sources 只追加启用能力包的 `sourceFiles`。
  - [x] 生成物编译通过：默认无候选宏的 `managed_main.c.tpl` 编译 0 warning。

- [x] `CORE-006-E` enabled pack 验收
  - [x] 启用候选能力包时复制源码：`content.slug` 和 `content.like` 均声明 `sourceFiles/includeFiles`。
  - [x] 启用候选能力包时编译源码：`declared capability source compile` 对两个候选源码编译 0 warning。
  - [x] acceptance API smoke 通过：当前总门禁覆盖 acceptance 脚本语法、manifest-only 正负路径，并已通过真实 HTTP smoke。
  - [x] acceptance page smoke 通过：当前总门禁覆盖页面模板 JS、manifest-only 路径，并已通过真实 HTTP smoke。

## 10. 冻结的后续能力包任务池

- [F] `CAP-P1-STATIC` `content.static`：基于 `CORE-005` 接入后台任务队列，完善静态生成、失败重试和产物清理。
- [F] `CAP-P1-SLUG` `content.slug`：基于 `CORE-001` 使用统一 URL 规则编辑模型管理漂亮 URL。
- [F] `CAP-P1-REDIRECT` `content.redirect`：基于 `CORE-001` 使用统一 URL 规则编辑模型管理跳转规则和冲突提示。
- [F] `CAP-P1-CATEGORY` `content.category`：基于 `CORE-003` 完成栏目树体验、绑定表迁移和基础插件解耦。
- [F] `CAP-P2-WORKFLOW` `content.workflow`：多节点流程模型、自动任务和更完整待办闭环。
- [F] `CAP-P2-SEARCH` `content.search`：中文分词、相关性调优和索引任务化。
- [F] `CAP-P2-FORM` `content.form`：拖拽式表单设计器和外部通知投递。
- [F] `CAP-P2-IMPORT` `content.import-export`：流式上传解析和任务队列化。
- [F] `CAP-P3-AUDIT` `content.audit-log`：外部任务覆盖和审计查询体验继续补强。

## 11. 每次开发必须勾选的收尾项

- [x] 更新本文对应任务状态。
- [x] 如整体结论改变，同步更新 `CMS能力包进度TRACKER.md`。
- [x] 新增或修改对应检查脚本 marker。
- [x] 如新增或改变 API、页面、配置、UI 验收点，同步更新能力包 `contracts.json`。
- [x] 运行 `powershell -ExecutionPolicy Bypass -File tools\check_cms_capability_workflow.ps1`。
- [x] 如果任务要求真实 HTTP 验收，记录 live smoke 命令和结果。

## 12. 当前下一步

- [x] 主线 CORE-001 到 CORE-006 已完成。
- [x] 生产收口已完成静态门禁、运行实例确认、安全后台入口确认和 live-smoke 工具修正。
- [x] 真实 HTTP smoke 已通过；临时后台账号均已软删除。

## 13. 生产收口记录

- [x] 2026-05-10 静态总门禁已通过：`powershell -ExecutionPolicy Bypass -File tools\check_cms_capability_workflow.ps1`。
- [x] 本机运行态已确认：`http://127.0.0.1/` 可访问，监听进程为 `D:\Git\x-admin\xs.exe`。
- [x] 本机安全后台入口已确认：`/jvb5umu0000a0az05tjn9q0ttrnms888` 可返回登录页；未登录访问 `/admin/*` 返回 404，符合安全入口策略。
- [x] live smoke 工具已支持安全后台入口：`-AdminLoginPath` 负责登录，`-AdminBase /admin` 负责后台 API。
- [x] 真实 HTTP smoke 已执行：`powershell -ExecutionPolicy Bypass -File tools\check_cms_capability_workflow.ps1 -RunLiveSmoke -StartServer -StopStartedServer -AdminLoginPath "/jvb5umu0000a0az05tjn9q0ttrnms888" -AdminUsername <temp> -AdminPassword <temp> -TimeoutSec 90 -StartupTimeoutSec 90`。
- [x] 全能力包结果：核心端点 3/3 通过，21 个 enabled pack API 通过，21 个 enabled pack page 通过，ErrorCount=0。
- [x] base-only 结果：`tools\fixtures\content_base_smoke_model.json` 生成插件后核心端点 3/3 通过，enabled pack 数 0，ErrorCount=0。
