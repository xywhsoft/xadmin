# 路由动态化与 CMS 能力包建设 SPEC

## 最新进度补充

- [x] `content.redirect` 跳转规则筛选：后台 `redirect/list` 新增 sourcePath、targetUrl、status 筛选，能力页跳转规则视图同步提供来源路径、目标地址和启用状态输入框；筛选只影响后台有界列表查询，不改变公开 resolve、HTTP 301/302 和动态路由热路径。
- [x] `content.static` 任务规则/内容筛选：后台 `static/task/list` 新增 ruleId 和 targetId 筛选，能力页任务视图同步提供规则 ID、内容 ID 输入框；筛选只影响后台有界列表查询，不改变静态生成、重试和请求热路径。
- [x] `content.static` 产物路径筛选：后台 `static/artifact/list` 新增 path 子串筛选，能力页产物视图同步提供产物路径输入框；筛选只影响后台有界列表查询，不改变静态文件服务、生成、清理和路由热路径。
- [x] `content.form` 表单字段名结构边界：能力页字段预览会提示空字段名和重复字段名，校验结构按钮同步复用该检查；后端 `form.save` 保存 schema 时拒绝非对象字段、空字段名和重复字段名，避免公开提交路径出现字段覆盖或不可解释的 required 行为。
- [x] `content.form` required 引用边界：能力页字段预览会提示 `required[]` 引用了不存在的字段或重复字段；后端 `form.save` 保存 schema 时要求 required 为数组且元素为字段名，并拒绝不存在字段名和重复 required 项，避免公开提交阶段出现永远无法满足或语义重复的必填规则。
- [x] `content.form` 字段预览和点选回填：能力页从 `schemaJson` 本地解析 `fields[]`，以表格预览字段、类型和必填状态，选择字段可回填设计器输入框继续编辑；只改本地编辑态，不新增后端写入路径。
- [x] `content.form` 后台提交统计：新增只读 `form/submission/stats` 聚合接口和能力页“提交统计”入口，返回提交总数、覆盖表单数、已处理/未处理数量、最近提交时间、statusStats 和 formStats；统计只读 `content_form_submission`，不改变公开提交、处理和通知流程。
- [x] `content.static` 后台静态化统计：新增只读 `static/stats` 聚合接口和能力页“静态化统计”入口，返回规则、任务、产物总量，任务成功/失败/待处理数量，产物覆盖对象数，以及规则/任务状态分布；统计只读 `static_rule/static_task/static_artifact`，不改变生成、清理和公开路径。
- [x] `content.revision` 后台版本统计：新增只读 `revision/stats` 聚合接口和能力页“版本统计”入口，返回版本总数、覆盖内容数、最大版本号、最近快照时间、actionStats 和 statusStats；统计只读 `content_revision`，不改变快照保留、diff 和恢复流程。
- [x] `content.access` 后台权限统计：新增只读 `access/rule/stats` 聚合接口和能力页“权限统计”入口，返回规则总数、启用/停用数量、内容/栏目规则数量和 accessMode 分布；统计只读 `content_access_rule`，不改变运行期权限判断。
- [x] `content.audit-log` 后台审计统计：新增只读 `audit-log/stats` 聚合接口和能力页“审计统计”入口，返回日志总数、最近时间、去重操作者/对象数量、actionStats 和 targetTypeStats；统计只读 `content_audit_log`，不改变审计写入、清理和请求热路径。
- [x] `content.related` 后台关联统计：新增只读 `related/stats` 聚合接口和能力页“关联统计”入口，返回 total/enabled/disabled、manual/rule 数量、最近更新时间和 relationType 分布；统计只走后台固定聚合 SQL，不改变公开推荐和规则计算热路径。
- [x] `content.comment` 专用待审队列：新增后台只读 `comment/moderation-queue`，按 `maxAdminListRows` 返回 pending 评论、内容标题、作者、IP、正文长度、链接数、积压秒数、审核次数和最近审核时间，并支持 contentId/authorName 绑定筛选；能力页“审核”视图改用该队列，仍通过原有单条/批量审核接口处理。
- [x] `content.sensitive` 后台统计：新增只读 `sensitive/stats` 聚合接口和能力页“词库统计”入口，返回词条总数、启用/停用数量、分组/作用域数量、命中日志总量、近期命中数和最近命中时间；近期窗口由 `statsRecentDays` 配置控制，统计只走固定聚合 SQL，不改变敏感词扫描热路径。
- [x] `content.revision` 枚举和栏目关联 diff：select/radio/checkbox/checklist/combobox 字段的版本差异会解析字段 `list/options`，diff row 输出 `diffMode=enum`、`beforeLabel/afterLabel` 和 `optionLabelResolved`；内置 `categoryId` 在栏目包启用时输出 `diffMode=relation` 和栏目标题/路径 label；后台能力页和编辑器版本弹窗都显示原始值与可读 label。
- [x] `content.sensitive` / `content.related` / `content.seo` / `content.category` 状态边界：敏感词保存和导入、手工相关推荐保存、SEO 元信息保存、栏目保存都会把 `status` 归一化为 enabled(1) 或 disabled(0)，避免后台写入口把未知状态持久化到能力包表；契约新增 `statusValidation`，检查脚本新增函数级静态门禁。
- [x] `content.tag` / `content.topic` 后台关联筛选：能力页内容关联视图新增 tag/topic ID 和 content ID 筛选，复用后端已有有界查询，结果继续受 `maxAdminLinkRows` 限制；契约新增 `adminLinkFilters`，不改变公开列表和路由热路径。
- [x] `content.audit-log` 后台过滤 UI：能力页审计日志表新增 targetType/action/targetId 筛选，复用后端固定 SQL 和 `maxListRows` 上限；契约新增 `adminFilterUi`，筛选只影响后台表格查询。
- [x] `content.access` 后台规则筛选：访问规则列表新增 targetType/targetId/accessMode/status 固定条件筛选，能力页同步提供筛选控件，继续受 `maxListRows` 限制；运行期权限检查路径不增加额外扫描。
- [x] `content.media` 后台资源筛选：媒体资源列表新增 MIME 和状态固定条件筛选，能力页同步提供筛选控件，继续受 `maxListRows` 限制；公开媒体列表仍只走启用状态和 access 引用过滤。
- [x] `content.form` 通知记录筛选：后台 `form/notification/list` 新增 formId/contentId/status/event 固定条件筛选，能力页通知记录视图同步提供筛选控件，继续受 `maxListRows` 限制。
- [x] `content.form` 本地通知重发：后台新增 `form/notification/replay`，可基于已有通知复制出新的本地通知事件 `form.notification.replay`，能力页通知记录支持行内重发；该能力只写本地通知日志，不接外部投递。
- [x] `content.static` 失败任务批量重试：后台新增 `static/task/retry-failed`，按 `maxTaskRetryRows` 有界扫描失败任务并重新生成，能力页提供重试上限输入和结果面板；该操作只在后台触发，不进入请求热路径。
- [x] `content.sitemap` 刷新上限 UI：能力页新增刷新上限输入，`sitemap/refresh-plan` 和 `sitemap/refresh` 共用该 limit，并继续受 `maxRefreshRows` 服务端封顶保护。
- [x] `content.comment` 待审风险诊断：`comment/moderation-queue` 额外返回 `riskScore/riskFlags`，风险原因由现有反垃圾配置派生，能力页审核表格直接展示；该字段只服务人工审核，不改变公开提交行为。
- [x] `content.import-export` 任务统计：后台新增 `import-export/stats` 只读接口，汇总导入/导出任务总量、行数、失败数和状态分布，能力页提供“任务统计”按钮。
- [x] `content.workflow` 工作流统计：后台新增 `workflow/stats` 只读接口，汇总待办、通知、日志和动作分布，能力页提供“工作流统计”按钮。

## 0. 当前口径

本文是 2026-05-09 起 xAdmin 路由系统与内容系统能力包建设的当前口径。若旧文档与本文冲突，以本文为准。

核心结论：

- xAdmin 路由系统演进为“高性能静态路由 + 正则动态路由”双模式。
- 静态路由仍然是第一快路径，动态路由只在静态路由未命中后执行。
- 请求路径上只做必要、静态、可预测的检查；复杂校验前移到注册、保存、编译阶段。
- 动态路由权限按“路由级权限 + 业务级权限”分层。
- 内容插件必须在无能力包时也能生成可用的基础内容管理系统。
- 能力包是按需增强，不启用的能力最终不生成、不编译、不加载、不注册、不执行。
- 过渡阶段允许用 `#ifdef`、注册开关和运行时开关屏蔽能力代码，但最终目标是按能力包拆分生成。

## 1. 路由系统设计原则

### 1.1 性能原则

- 静态路由继续使用 exact path 字典查找。
- 动态路由只作为 static miss 后的第二层。
- 动态路由规则只在系统加载、插件加载、规则变更时编译。
- 请求时不编译正则、不扫描数据库、不解析规则配置。
- 动态规则合法性、冲突、遮挡、静态资源路径吞噬风险只在提交或编译阶段检查。
- 对“可能吞掉静态资源路径”的规则，默认做后台提示和风险标记，不在运行时追加排除判断。

### 1.2 路由访问顺序

目标请求链路：

```text
request path normalize
-> static exact route
-> dynamic route group
-> plugin static files
-> site static files
-> 404
```

动态路由不得替代静态路由快路径。即使未来支持复杂 URL 规则，也必须保持静态路由 O(1) 或接近 O(1) 的访问特性。

### 1.3 动态路由编译

动态路由注册后按以下维度分组：

```text
method
scope
prefix group
owner type
```

推荐结构：

```text
GET public /c/*
GET public /article/*
GET public /category/*
GET admin /admin/*
POST api /api/*
```

每组维护一个预编译正则集合。动态路由变更时使用 copy-on-write：

```text
load dynamic route rows
build route descriptors
compile regex set
compile capture descriptors
validate success
atomic swap active dynamic route table
old table released after no request references it
```

编译失败不得影响旧路由表继续服务。

### 1.4 xrt 正则使用边界

xAdmin 代码不得直接使用 bbre API。xrt 已封装正则库，动态路由必须通过 `xrtRegexCreate`、`xrtRegexSetBuilderCreate`、`xrtRegexSetMatches` 和 `xrtRegexCaptures` 这类 xrt 正则 API 完成编译、批量候选规则命中和参数提取：

```text
path -> matched pattern index
```

xrt 正则集合命中只负责定位候选规则，不直接负责路由参数提取。命中后应进入第二步：

```text
matched pattern index -> capture extraction -> RouteParams
```

简单参数路由可以优先使用 path segment/trie。正则动态路由用于复杂约束、伪静态、slug 规则、redirect 规则等场景。

## 2. 动态路由数据模型

建议在现有 URI/路由权限表基础上扩展动态路由字段，或新增动态路由表后同步到权限资源。

建议字段：

```text
id
route_key
uri
method
is_dynamic
pattern
prefix
priority
scope
owner_type
owner_id
handler_key
need_auth
auth_id
enabled
compile_status
compile_message
create_time
update_time
```

字段语义：

- `uri`：展示和权限绑定用的稳定路由标识。
- `pattern`：动态路由正则表达式。
- `prefix`：静态前缀分组键，用于减少运行时正则集合规模。
- `priority`：动态路由命中顺序。
- `scope`：`public`、`member`、`admin`、`api` 等。
- `owner_type`：`core`、`plugin`、`content`、`capability`。
- `handler_key`：动态路由命中后转发到的处理器标识。
- `auth_id`：路由级权限资源。

## 3. 权限模型

动态路由权限采用两层模型：

```text
route-level auth
object-level auth
```

路由级权限：

- 静态路由按 exact URI 绑定权限。
- 动态路由按 `route_key` 或 `pattern uri` 绑定权限。
- 请求命中动态路由后，先执行 xAdmin 统一权限缓存判断。

业务级权限：

- 由 handler 自行处理。
- 内容系统可接入 `content.access`、阅读权限级别、会员组、付费状态、文章状态等判断。
- 若插件未提供业务级权限，则只能使用 xAdmin 路由级权限。

性能要求：

- 请求时权限判断必须走缓存。
- 权限同步在注册、保存、编译阶段完成。
- 不允许请求时访问数据库查询权限。

## 4. 动态路由提交期检查

以下检查在保存或编译阶段执行，不进入请求路径：

- 正则语法检查。
- capture 参数名重复检查。
- 与静态 exact 路由冲突检查。
- 与同组动态路由优先级遮挡检查。
- 可能吞掉插件静态文件或站点静态文件路径的风险提示。
- 过宽规则提示，例如 `^/.*$`、`^/([^/]+)$`。
- owner 权限边界检查。

阻止保存的错误：

- 正则无法编译。
- route key 重复。
- handler 不存在。
- method/scope 非法。
- 与核心静态 exact 路由直接冲突。

仅提示不阻止的风险：

- 可能覆盖 `/css/*`、`/js/*`、`/img/*`、`/uploads/*`。
- 规则过宽。
- 低优先级规则永远无法命中。

## 5. CMS 能力包总原则

### 5.1 基础内容插件边界

无任何能力包时，生成插件仍必须可用，至少包括：

- 后台内容列表。
- 后台内容新增、编辑、删除。
- 草稿/发布基础状态。
- 基础前台列表 API。
- 基础前台详情 API。
- 插件私有数据库。
- 插件私有后台页面。
- 插件私有权限和菜单。

基础内容插件不内置栏目、SEO、评论、标签、搜索、静态化、访问控制等增强功能。

### 5.2 能力包加载边界

能力包 id 必须是稳定英文，例如：

```text
content.seo
content.slug
content.category
```

显示名称可以是中文，例如：

```text
SEO 优化
固定链接
栏目
```

未启用能力包：

- 最终不生成该能力代码。
- 不复制该能力模板和静态资源。
- 不生成该能力 schema。
- 不注册该能力菜单、路由、权限。
- 不进入该能力的运行逻辑。

过渡阶段可接受：

- 模板中存在能力代码，但必须通过 `#ifdef`、注册条件或运行时挂载检查保证不加载、不注册、不执行。

## 6. 第一阶段能力包清单

现有能力包：

```text
content.comment      评论
content.tag          标签
content.topic        专题
content.sensitive    敏感词
content.static       静态化
content.like         点赞
content.view-stat    浏览统计
```

新增目标能力包：

```text
content.seo           SEO 优化
content.slug          固定链接
content.redirect      跳转规则
content.category      栏目
content.media         媒体资源
content.revision      内容版本
content.workflow      审核流程
content.search        内容搜索
content.sitemap       站点地图
content.related       相关推荐
content.form          内容表单
content.access        阅读权限
content.audit-log     操作审计
content.import-export 导入导出
```

### 6.1 Slug 与 Redirect

`content.slug` 负责当前内容的规范 URL：

- slug 字段。
- URL 规则。
- 唯一性检查。
- 当前 URL 解析。
- 与栏目路径组合。

`content.redirect` 负责旧 URL 到新 URL 的跳转：

- 手工跳转规则。
- slug 修改后的历史 URL。
- 301/302 策略。
- 命中统计。

二者不合并：

- `content.slug` 是内容地址能力。
- `content.redirect` 是迁移和兼容能力。
- `content.redirect` 可独立存在。
- 启用 `content.slug` 后可选择自动写入 redirect 历史记录。

路由实现要求：

- 不为每篇内容注册全局静态路由。
- 不破坏 xAdmin 静态 exact 路由快路径。
- 公开 URL 通过少量动态入口和插件内部解析完成。
- 复杂 URL 规则可使用动态路由正则组。
- `content.slug.slugRoutePrefix` 和 `content.redirect.redirectRoutePrefix` 提供公开前缀配置；注册动态路由时必须先做静态合法性检查，再对前缀做正则字面量转义，避免插件 ID 或路径中的 `.` 等字符被误当作正则语义。

### 6.2 Category

`content.category` 从核心能力调整为可选能力包。

原因：

- 博客、公告、单页、知识片段等模型可以不需要栏目。
- 栏目树、栏目路由、栏目 SEO、栏目权限会显著增加生成插件复杂度。
- 不需要栏目时，代码不应生成或加载。

能力边界：

- 栏目树。
- 栏目排序。
- 栏目 slug。
- 栏目模板。
- 栏目 SEO。
- 栏目封面和描述。
- 栏目状态。
- 栏目内容列表。
- 栏目访问权限。
- 栏目静态化入口。

栏目仍归属于当前内容插件，不做宿主级全局栏目表。

### 6.3 Access

`content.access` 负责内容阅读权限，不替代 xAdmin 路由级权限。

至少支持：

```text
public
login
member_level
group
paid
password
private
```

必须接入现有会员 `authLevel`：

```text
content.required_read_level <= member.authLevel
```

覆盖点：

- 前台详情 API。
- 前台详情页。
- 静态化输出策略。
- 受限内容列表展示策略。

若同时启用 `content.static` 和 `content.access`，受限内容不得生成公开裸静态详情页；当前生成入口和自动静态化会为受限内容生成 `noindex,nofollow` 的受限壳页面，保留静态路径和 artifact 记录，但不输出正文内容。

## 7. 三阶段路线图

### Phase 1：能力覆盖与动态路由基础

- [x] 新建动态路由设计与数据结构。
- [x] 支持动态路由注册 API。
- [x] 支持动态路由正则组预编译：动态路由注册/删除会触发 xrt 正则集合重建，重建采用先构建新集合、成功后替换旧集合的方式，编译失败时保留旧集合和旧编译计数。
- [x] 支持 static route miss 后进入 dynamic route。
- [x] 支持动态路由命中后的参数提取。
- [x] 支持动态路由权限资源同步：动态路由注册会进入 `uris` 同步；权限缓存加载会先查静态路由、再查动态路由；后台 URI 权限编辑保存后也会按静态优先、动态兜底同步内存路由权限状态。
- [~] 支持动态路由提交期风险检查和提示：动态路由注册时已使用 xrt 正则库完成编译校验，静态 route key 冲突会拒绝注册，宽泛规则和可能覆盖静态资源路径的规则只在注册期记录 warning，不进入请求热路径；`/admin/trace` 已输出 `dynamicLastWarning` 供后台提示；`content.redirect` 保存/批量导入预检、`content.slug` 检查/预览/批量修复和 `content.static` 规则保存已对站点根路径、后台/API 前缀、静态资源前缀做提交期 warning，能力包管理页已显示这些 warning；`content.slug` 已接入 `^/{pluginXid}/([^/]+)$` 动态漂亮 URL，`content.redirect` 已接入可配置的 `^/{pluginXid}/r/[^?#]+$` 动态跳转入口，避免全站 catch-all 和多插件 `/r` 前缀冲突；slug/redirect 能力页已补“规则说明”和联合前缀检查，展示实例公开前缀、动态正则、样例 URL/path、预检结果、静态优先的冲突解释、匹配优先级和跨能力包前缀 warning，并已扩展为 slug/redirect/static 统一规则计划，汇总三个能力包的规则来源、启用状态、样例路径和热路径顺序；`tools/check_content_system.ps1` 已加入静态门禁，并锁定动态漂亮 URL 必须同时受能力包启用状态和实例开关控制；更完整伪静态规则配置仍待后续补强。
- [~] 补强 `content.comment` 能力包：评论列表、提交、审核、批量审核、隐藏、删除、计数和线程计数刷新已接入；能力包管理页已增加专用 `comment/moderation-queue` 待审核队列、批量通过/驳回入口、审核日志视图、按评论 ID 筛选审核日志和通知视图，后台新增 `comment/moderation-stats` 只读统计接口并在能力页显示待审/通过/驳回/隐藏/删除数量；待审队列已输出 `riskScore/riskFlags` 辅助人工审核，并支持 contentId、authorName、body、ip 后台筛选；通用能力包列表 API 支持存在 `status` 列时按状态过滤；公开评论列表保留平铺 `data` 并额外返回 `tree/children` 回复树，评论回复提交会校验父评论属于同一内容且已公开，并可通过 `maxReplyDepth` 在提交期限制回复层级；公开评论列表已支持 `maxPublicRows` 输出上限并使用绑定 `LIMIT ?`；公开评论提交已接入 honeypot、`minSubmitSeconds` 最短提交耗时、`minBodyLength/maxBodyLength`、`maxLinks` 正文链接数量上限、`duplicateWindowSeconds` 短窗口重复提交、`ipWindowSeconds/ipWindowLimit` IP 窗口限频、`allowedAuthorNames/blockedAuthorNames` 昵称黑白名单，以及 `blockedBodyPhrases/blockedUserAgentPhrases` 正文和 UA 禁用片段基础反垃圾校验；审核/隐藏/删除动作已写入 `comment_audit_log`，通知事件已写入 `comment_notification`，并提供后台 `comment/audit-log/list`、`comment/notification/list` 查询接口；更细反垃圾策略仍待补强。
- [~] 补强 `content.sensitive` 能力包：敏感词保存、删除、命中日志、内容保存前扫描和命中策略已接入；实例配置已中文化并声明 `strategy/fields/logRetentionDays/statsRecentDays/matchMode/maxWordLength/maxHitsPerScan`，扫描时按词库 `scope=all|content|comment|check` 过滤，并支持默认子串匹配、ASCII 词边界匹配和 `cjkLoose` 中文宽松匹配；`cjkLoose` 会忽略常见零宽字符、BOM 类格式字符、CJK/全角标点和 ASCII 分隔符后再匹配，覆盖低成本中文绕过写法；敏感词保存和批量导入可按 `maxWordLength` 拒绝异常长词条，单次扫描会按 `maxHitsPerScan` 停止继续写命中日志和返回数组；词库已增加 `group_key` 分组字段、迁移和后台分组筛选，分组只服务后台运营，不改变请求期按 scope 扫描的热路径；评论提交路径已接入 `comment.beforeCreate` 扫描，`block` 拦截、`replace` 替换正文、`mark` 强制进入待审核、`log` 仅记录；后台已提供 `sensitive/log/cleanup` 按保留天数清理命中日志，并提供 `sensitive/stats` 只读统计词库、分组、作用域、命中日志和近期命中数；验收 API 已改为现有 `/pack/list?pack=content.sensitive` 路径；更完整中文分词和语义级匹配仍待补强。
- [~] 补强 `content.static` 能力包：静态化规则、任务、产物列表、生成/预览/清理、保存期 URL 风险 warning、受限内容 noindex 壳页和删除后自动清理已接入；发布后自动生成已支持 `autoGenerateRuleLimit`，规则遍历 SQL 使用 `LIMIT ?` 并封顶 200，避免规则数放大发布写入成本；规则/任务/产物列表已支持 `maxListRows` 输出上限，任务列表支持状态、规则 ID 和内容 ID 筛选，产物列表支持内容 ID 和路径子串筛选，产物清理已支持 `maxCleanRows` 单次清理上限并使用绑定 `LIMIT ?`；任务运维已支持单条失败任务重试和 `maxTaskRetryRows` 有界失败任务批量重试；后台静态化统计已覆盖规则、任务和产物聚合；后台异步任务队列和更完整伪静态规则仍待补强。
- [~] 补强 `content.tag` / `content.topic` 能力包：标签和专题实例表单已中文化；标签已支持 `maxTags` 和 `allowCreateInline` 配置，并提供标签合并接口与能力页操作区；专题已支持 `mode` 与 `maxBindContents`，专题批量绑定内容和内容批量绑定专题都会校验单次绑定数量并封顶 1000，避免一次请求写入过多关联；标签/专题公开列表、公开内容列表、详情附带标签/专题字段和后台绑定关系列表已支持 `maxPublicListRows/maxDetailTags|maxDetailTopics/maxPublicContentRows/maxAdminLinkRows` 配置并使用绑定 `LIMIT ?`；标签/专题后台批量启用/停用已接入 `maxBatchRows` 有界接口和能力页入口；专题内容排序已提供最多 500 条的有界排序接口、JSON 编排入口和能力页 Up/Down 编排辅助；更细的可视化拖拽体验仍待补强。
- [~] 补强 `content.like` / `content.view-stat` 能力包：点赞和访问统计已按能力包启用状态注册前台记录/状态/排行接口、后台计数/明细/日统计接口；本轮补充后台 `like/stats` 与 `view/stats` 汇总 API，并在能力包管理页提供 Stats 入口；启用点赞或访问统计时会额外生成并注册独立 `generated/dashboard.html` 统计看板页，看板已接入 `/view/daily/list` 日趋势表和 Canvas 折线趋势图；点赞 create/cancel/status 的 contentId 与 actor key 校验已对齐，IP 去重和访问日志 IP 优先使用服务端 `xsReqRemote(objReq)`，访问 visitor key 已增加长度/控制字符校验；点赞支持 `content.like.allowGuest/dedup/maxAdminListRows`，访问统计支持 `content.view-stat.enableViewLog/rankEnabled/uniqueWindowSeconds` 配置并已在契约中声明，点赞后台明细/排行列表已与访问统计配置隔离。
- [x] 新增 `content.seo` 能力包：已补充清单/契约，并完成编辑页 SEO 字段按能力包注入、公开 `seo/meta` API 按能力包注册、后台 `seo/list|save|delete` 管理 API、独立 `content_seo_meta` 表、内容保存/导入/删除同步、后台 SEO 元信息长度与 canonical 保存期校验、公开展示页 head 注入、列表接口批量输出 `seoMeta`、栏目 SEO 字段按能力包启用显示、公开页栏目筛选时消费栏目 SEO 更新 head、栏目 title/keywords/description/canonical 模板变量，内容模型生成期的栏目 SEO 模板配置入口，内容级 title/keywords/description/canonical 运行期模板配置入口、能力包页独立 SEO 管理表格/弹窗、有效元信息预览、模板变量清单、内容模板变量取值预览、栏目模板变量取值预览和未替换变量 warning。
- [~] 新增 `content.slug` 能力包：已补充清单/契约，并完成编辑页 slug 字段按能力包注入、详情接口 slug 查询按能力包启用、公开 `slug/resolve` API、后台 `slug/check|preview|history|repair` API、保存时唯一性校验、批量填充/去重修复和 `content_slug_history` 历史表按能力包注册；历史记录支持 contentId、oldSlug、newSlug 和 status 筛选；启用 slug 且 `enablePrettySlugRoute=true` 时生成插件会注册带插件 XID 前缀的动态漂亮 URL `^/{pluginXid}/([^/]+)$` 并由公开页按 path 解析 slug；能力页已提供 slug/redirect/static 统一规则计划；启用能力包时会迁移 `content_item.slug_value` 物理列和索引，新保存、导入和修复会同步该列，公开 slug 解析优先走物理列并对旧 payload 数据保留有界回退扫描；更完整伪静态规则配置后续补强。
- [~] 新增 `content.redirect` 能力包：已补充清单/契约，并完成启用时生成 `content_redirect` 表、公开 `redirect/resolve` API 按能力包注册、后台列表/保存/删除/批量导入 API、能力包管理页表格与新增/编辑弹窗、命中统计回写；跳转规则列表支持来源路径、目标地址和启用状态筛选；批量导入已支持 preview 与 `confirm=true` 写入，slug 历史自动写入和 `redirect=1` 真实 301/302 响应已接入；启用能力包且 `enablePrettyRedirectRoute=true` 时会注册 `^/{pluginXid}/r/[^?#]+$` 动态跳转入口，直接按请求路径匹配 `content_redirect.source_path` 并返回 301/302；保存和批量导入确认时已对目标也是跳转源、两步跳转环、最多 8 跳的有界链式跳转环和链深过长做提交期 warning，能力页已提供 slug/redirect/static 统一规则计划，后续更完整伪静态规则配置仍待补强。
- [~] 新增 `content.category` 能力包：已补充清单/契约，并完成栏目后台路由、菜单、列表页栏目筛选、编辑页栏目字段和 `generated/categories.html` 文件按能力包挂载/生成；已补公开 `category/list|detail|contents` API、后台 `category/tree` API、树形标题与 breadcrumb 路径输出、父级 path/level 计算、子树 path/level 重算、栏目描述/封面/模板键字段、栏目 slug 保存期安全校验、父栏目下拉、表格内排序保存、拖拽排序弹窗、拖拽弹窗内跨父级迁移、管理页关键词过滤、删除保护、公开页栏目筛选、栏目 SEO head 消费，以及公开列表按栏目 `templateKey` 切换 default/compact/grid/feature 布局；实例配置 `maxDepth` 已接入保存和拖拽迁移检查；更完整树控件仍待后续补强。
- [~] 新增 `content.media` 能力包：已补充清单/契约/schema，并完成启用时生成 `content_media`、`content_media_ref` 表，公开 `media/list|detail` API、后台 `media/list|save|delete|batch|ref/list` API、内容保存时同步封面/正文资源引用、详情/列表返回 `mediaList` 与 `coverMediaUrl`、媒体列表返回 `refCount`、能力包管理页资源/引用双视图、批量启用/停用/删除、URL 扩展名与常见 MIME 推导、资源宽高字段展示/保存、媒体 URL/大小/尺寸元数据保存校验、浏览器侧图片尺寸识别、编辑页按能力包注入封面资源 ID/正文资源 ID 字段并提供媒体资源选择器，媒体管理页已展示/编辑 `attachmentXid` 并提供宿主附件上传入口；实例配置 `maxAssetSizeBytes`、`maxImageDimension`、`serverDetectImageSize`、`allowedMimeTypes`、`maxListRows`、`maxRefListRows` 与 `maxDetailMediaRows` 已接入保存校验、媒体列表输出上限、详情附带媒体输出上限和本地插件静态图片 PNG/JPEG/GIF 文件头识别；更深的宿主附件上传后回填流程仍待补强。
- [~] 新增 `content.revision` 能力包：已补充 schema，并完成启用时生成 `content_revision` 表、内容保存自动写入版本快照、后台 `revision/list|detail|diff|restore|restore-preview|stats` API 按能力包注册、能力管理页版本表格、Diff 入口、恢复前当前内容对比确认、版本统计，以及编辑页版本弹窗、Diff 和 Restore 入口；能力管理页和编辑页的 Diff/Restore Preview 已从 JSON 面板升级为字段级 Before/After 表格，并对长文本/多行字段提供逐行高亮显示，对图片资源和 URL 字段提供资源预览，对对象/数组字段提供结构化 JSON diff 表格；diff 行已返回 `changeKind/typeChanged/beforeLength/afterLength` 便于快速判断新增、删除、修改和长度变化；布尔 diff 已输出 `beforeBool/afterBool`，日期时间字段已输出 `datetime` 模式和 `beforeTimeText/afterTimeText`，枚举字段已输出 `enum` 模式和 `beforeLabel/afterLabel`，内置栏目字段已输出 `relation` 模式和栏目标题/路径；实例配置 `maxSnapshotsPerContent` 已接入按内容保留新版本、剪裁旧版本；更细的自定义关联字段专用 diff 仍待补强。
- [~] 新增 `content.workflow` 能力包：已修复能力包 effects 描述，补充 `content_workflow_log` 与 `content_workflow_notification` schema，并完成启用时注册后台 `workflow/action`、`workflow/log/list`、`workflow/todo/list`、`workflow/notification/list`、`workflow/scheduled-publish` 与 `workflow/stats` API，支持 submit、approve/publish、reject、offline、schedule 基础动作、工作流日志表格、审核待办表格、通知表格、工作流统计、审批配置只读诊断、定时发布到期只读诊断、能力包页动作面板、到期定时发布有界执行入口、编辑页 submit/approve/reject/offline 动作入口、动作指派人记录、到期定时发布处理、基础状态流转校验和发布/下线后的派生能力同步；到期定时发布默认批量数量已支持 `content.workflow.defaultScheduledLimit` 生成期配置，后台列表上限已支持 `content.workflow.maxListRows` 配置，审核原因长度上限已支持 `content.workflow.maxReasonLength` 配置；次数型多级审批 V1 已通过 `requiredApprovals` 接入，未达到通过次数前只记录审核动作，达到后再发布；工作流日志已记录 session operatorId，并支持 `requireDistinctApprovers` 阻止同一操作者重复凑审批次数；多级节点模型和自动任务仍待补强。
- [~] 新增 `content.search` 能力包：已补充 `content_search_index` schema，并完成启用时注册公开 `/search`、后台 `/search|search/rebuild|search/stats` API；当前搜索覆盖标题、slug、摘要和 DSL 中标记 `searchable` 的字段，已补独立索引重建、保存/导入/删除同步、索引统计、管理页有界搜索探针、排序解释、公开页无额外筛选时走真实搜索接口、搜索分数、命中附近摘要和关键词高亮展示、常见分隔符归一化、常见中文/全角标点分隔、连续 CJK 字符宽松 LIKE 匹配、空白分隔查询词的有序多词 LIKE 匹配、查询 `%/_/~` 通配符转义、多词命中摘要来源选择，以及 `exactTitleWeight/prefixTitleWeight/titleWeight/keywordWeight/summaryWeight/bodyWeight/termCoverageWeight` 可配置权重、查询词覆盖度加权、`minQueryLength/maxResultLimit` 查询边界、`maxRebuildRows` 重建分片边界、契约 `configKeys` 和 `rankingExplain` 声明；启用 `content.category` 时 `search.query` 支持显式 `categoryId` 收窄筛选，未传栏目时保留原 SQL 路径；更完整中文分词仍待补强。
- [~] 新增 `content.sitemap` 能力包：已补充 `content_sitemap_entry` schema，并完成启用时注册公开 `sitemap.xml`、`sitemap-index.xml`、`rss.xml`、`robots.txt` API，以及后台 `sitemap/entry/list|refresh-plan|refresh|stats` API；当前已补条目刷新、只读刷新计划预检、保存/导入/删除同步、统计入口、公开 XML/RSS 优先读取条目缓存、`content.sitemap.siteUrl` 绝对 URL 输出配置、`sitemap.xml?page=N` 分片和 `sitemap-index.xml` 索引，以及写入侧刷新 `sitemap/sitemap.xml|sitemap-index.xml|rss.xml|robots.txt` 落盘缓存，并写入 `sitemap/cache.json` 记录 write-through 策略、生成时间、条目数、TTL 和过期时间；写入侧会先标记 `sitemap/dirty.json`，缓存文件采用临时文件完整写入后替换，完整写入后清 dirty，后台统计页已表格化展示缓存策略、缓存文件、dirty 文件、缓存条目数、TTL、dirty/过期状态、更新时间和具体缓存文件存在性/大小/更新时间，且 `cacheDirty` 会读取 `sitemap/dirty.json` 元数据；`siteUrl/cacheTtlSeconds` 已在契约 `configKeys` 声明；真正后台任务队列仍待补强。
- [~] 新增 `content.related` 能力包：已补充 `content_related` schema，并完成启用时注册公开 `related/list` API、后台 `related/list/save/delete/rebuild`、`related/rule/preview` 与 `related/stats` API，以及内容详情 `relatedList` 附带输出；当前支持手工关联，并已补同栏目规则重建生成 `relationType=rule` 关联、保存/导入后按 `content.related.ruleLimit` 同步刷新当前内容规则关联和最近同栏目内容的反向推荐，工作流发布/定时发布后会按 `content.related.publishRefreshLimit` 有界刷新同栏目邻近内容规则关联，公开推荐输出按 `content.related.maxPublicRelated` 封顶，能力包页规则重建已增加 1-20 有界输入和独立结果面板，规则分值预览可在不写入 `content_related` 的情况下输出同栏目候选、共享标签/专题数量、分值公式、原因解释和最终权重，能力页以表格展示候选分值拆解并保留 JSON 原文，后台关联统计可只读查看 total/enabled/disabled、manual/rule、relationType 分布和当前规则权重/执行上限配置，规则重建会跳过已存在的手工关联以避免重复推荐；规则权重已按同栏目基础分叠加共同标签和共同专题加分，并支持 `categoryWeight/tagWeight/topicWeight` 调参，更完整复杂规则计算器仍待补强。
- [~] 新增 `content.form` 能力包：已补充 `content_form`、`content_form_submission` 与 `content_form_notification` schema，并完成启用时注册公开 `form/submit` API、后台 `form/list/save/delete`、`form/submission/list|export|status|stats` 与 `form/notification/list|status|stats|replay` API；当前支持基础 JSON schema 记录、schema textarea 管理、字段追加/删除/上下移动式可视化辅助，设计器可写入 placeholder、minLength/maxLength、min/max、pattern 和 options，支持从当前 schemaJson 预览 fields[] 并点选回填设计器输入框，字段结构会提示并拒绝非对象字段、空字段名、重复字段名、required 未命中字段和 required 重复项，JSON 格式化/校验、保存期 `maxFields` 字段数量上限、`required` 字段校验、`fields[].type/format/minLength/maxLength/min/max/minItems/maxItems/pattern/options|list` 校验、email/url/date/phone 格式校验、honeypot、`minSubmitSeconds` 提交耗时和 `ipWindowSeconds/ipWindowLimit` 基础反垃圾校验、能力包级 `maxSubmissionBytes` 默认提交大小上限和单表单 schema 覆盖、提交数据入库、request/session 字段来源 IP 记录、JSON 导出、提交处理状态更新、处理备注/处理人/处理时间记录、提交统计，以及本地通知日志、已读/未读状态、统计和本地重发记录；后台列表上限和提交导出上限已支持 `maxListRows/maxExportRows` 配置；完整拖拽式设计器和外部通知仍待补强。
- [~] 新增 `content.access` 能力包：已补充 `content_access_rule` schema，并完成启用时注册公开 `access/check` API、后台 `access/rule/list/save/delete` 与 `access/rule/stats` API；内容公开列表会过滤未授权内容，详情读取会返回访问拒绝，当前支持 public、login、level、group、password、paid、private 基础模式，paid 模式会明确返回 `payRequired/price`，并允许外部订单系统把可信已购内容 ID 注入 `paidSessionKey` 指定的 session 数组后放行，返回 `orderValidated`；权限检查响应已返回 `ruleMatched/ruleSource`，可区分无规则、内容规则和栏目继承规则；password 模式新写入已使用带 salt 与可配置轮次的 `xsha256i:` 迭代哈希，并兼容旧 `xsha256:`、`xrt64:` 与明文；新增 password 规则必须提供密码，编辑已有 password 规则时空密码会保留原哈希；后台权限统计可查看规则总数、内容/栏目规则数量和 accessMode 分布；受限内容静态化已生成无正文的 `noindex,nofollow` 壳页面；正式订单插件仍待后续独立接入。
- [~] 新增 `content.audit-log` 能力包：已补充 `content_audit_log` schema，并完成启用时按 `content.audit-log` 独立注册后台 `audit-log/list`、`audit-log/detail`、`audit-log/cleanup` 与 `audit-log/stats` API；内容保存、删除、版本恢复、工作流动作、阅读权限规则变更及评论、标签、专题、栏目、静态化、敏感词、推荐、表单、导入导出、媒体等能力包写操作会自动写入基础审计日志，已支持从真实 `xsReqRemote(objReq)` 和 session 的 `ip/clientIp/remoteAddr` 字段采集 IP，内容更新会记录字段级差异摘要，后台详情视图可只读查看单条日志并解析 `detail_json`，后台统计可查看日志总数、最近时间、去重操作者/对象数量、actionStats 和 targetTypeStats，搜索重建、sitemap 刷新和定时发布手动运行已写入系统任务审计，并支持按保留天数清理旧日志；清理 API 默认保留天数已支持 `content.audit-log.defaultKeepDays` 生成期配置，后台列表上限已支持 `content.audit-log.maxListRows` 配置，后台列表已支持 `targetType/action/targetId` 绑定参数过滤，单次清理行数已支持 `maxCleanupRows` 配置并使用绑定 `LIMIT ?`；更完整的外部任务覆盖仍待补强。
- [~] 新增 `content.import-export` 能力包：已补充 `content_import_job` 与 `content_export_job` schema，并完成启用时注册后台导入预检、确认导入、导入任务列表、失败行回放、JSON 导出、导出任务列表、导出下载与任务统计 API；当前支持字段白名单、失败行报告、显式 `confirm=true` 的受控导入、`insert/update/skip` 导入冲突策略、按导入任务取回失败行并填回 JSON 编辑区、已完成导出任务 JSON 附件下载，JSON 导出 `limit/offset` 分片、`hasMore/nextOffset` 响应和管理页“下一片”连续导出，以及管理页按切片连续调用 preview/commit 的分片导入入口；导入成功计数已压实到单行校验、写库和派生同步全部完成之后，导入任务状态会按最终结果写为 `imported/partial_failed/failed`；服务端 preview/commit 已按 `content.import-export.maxBatchRows` 强制单批上限并封顶 1000 行，导出默认和显式分片上限已支持 `content.import-export.maxExportRows` 并封顶 5000 行；真正流式上传解析和后台任务队列仍待补强。
- [x] 所有能力包在后台可见中文显示名。
- [~] 未启用能力包时，生成插件仍可运行：核心后台内容管理页面已从 `content.view-stat` 路由条件中解耦，栏目入口、栏目页面文件、栏目筛选/编辑字段、旧 7 个能力包公开/后台 API 路由已按能力包开关不注册/不生成/不渲染；`content_category` 表和栏目索引已从基础 schema 迁入 `content.category/schema.sql`，`content_item(category_id, status, is_draft, delete_time)` 辅助索引也已改为仅在 `content.category` 启用时创建，并命名为 `G_CategoryPostMigrationIndexSql` 以避免被当成通用索引；启用 access 但不启用 category 时访问规则列表不再联表栏目表，栏目规则保存会明确拒绝；`generated/categories.html` 和 `generated/dashboard.html` 已通过生成期条件与检查脚本锁定为按能力包生成；schema、权限、菜单、路由、build.defines、声明 source/include/template/assets、runtime manifest 和 contracts 均已通过检查脚本锁定 `enabled=false` 跳过，`runtime/contracts.json.capabilities` 与 `runtime/managed.json.capabilitySlots` 也已改为只写启用能力包，能力挂载 registry/provider discovery 已改为从 `runtime/managed.json.capabilitySlots` 读取 slot、从 `runtime/contracts.json.abilityPacks` 读取包契约，且 `capabilitySlots` 固定输出数组，未启用能力包时为空数组；`tools/check_content_system.ps1` 已加入静态/动态路由能力边界、通用基础 schema 边界和 `content_category` 引用边界扫描，避免能力包专属路由/表漏出或基础后台入口被能力包误包裹；`content_item.category_id` 字段和更细模板代码仍待后续拆分。
- [~] 启用能力包时，对应菜单、权限、路由、schema 可运行：已新增 `tools/check_capability_packs.ps1`，覆盖 21 个能力包的 manifest/contract/effects/schema/advisor 验收路径一致性检查，并对能力包 `schema.sql` 做静态结构门禁，确认声明表与建表一致、语句形态可预期、索引名不跨包重复；检查脚本已锁定能力包默认权限、菜单标题、模板静态 auth 变量与 `Content_IsBuiltinAbilityPack()` 的边界，避免新增通用生成包被误归入模板固定路由包，当前 `ErrorCount=0`、`WarningCount=0`；内容模型编辑器已能渲染能力包 `instance.xform.json` 的基础生成期配置，目前 21 个能力包均已有实例配置表单；已新增 `tools/smoke_capability_acceptance.ps1` 可按能力包 acceptance path 对生成插件做 HTTP smoke，脚本会检查 API 响应为 JSON、页面响应为 xAdmin HTML 形态，并保留 `-SkipContentCheck` 便于临时定位鉴权/网络问题；后续还需接入生成结果页/CI。

### Phase 2：能力生产级补强

- [x] `content.seo` 支持内容级 SEO 元信息：编辑页字段、公开 meta API、后台 SEO 管理 API/表格/弹窗、独立 `content_seo_meta` 表、内容保存/导入/删除同步、后台 SEO 元信息长度与 canonical 保存期校验、公开详情页 head 注入、列表接口批量 `seoMeta` 输出、栏目 SEO 字段保存/返回、公开页栏目筛选 head 消费、栏目 SEO 模板变量、栏目模板配置入口、内容级 title/keywords/description/canonical 运行期模板配置入口、能力页有效元信息预览、模板变量清单、内容模板变量取值预览、栏目模板变量取值预览和未替换变量 warning 已接入。
- [~] `content.slug` 支持冲突检测、规则预览、历史 URL：后台冲突检测、规则预览、历史 URL 表格和历史筛选、保存时唯一性校验、redirect 自动联动、批量修复 preview/confirm、启用能力包时带插件 XID 前缀且可配置开关的动态漂亮 URL，以及能力页规则说明和 slug/redirect 联合前缀检查已接入；启用能力包时新增 `content_item.slug_value` 物理列和索引，新数据优先走列查询，旧数据继续按配置上限扫描 payload；更完整伪静态规则配置仍待补强。
- [~] `content.redirect` 支持 301/302、命中统计、批量导入：基础解析 API、301/302 状态码数据、命中统计、后台管理表格、来源/目标/状态筛选、新增/编辑弹窗、批量导入 preview/confirm、slug 历史自动写入、`redirect=1` 真实 HTTP 跳转、可配置 `/{pluginXid}/r/...` 动态路由跳转入口、能力页规则说明、slug/redirect 联合前缀检查、统一 URL 规则统计，以及提交期跳转链、两步环、最多 8 跳有界链式环和链深过长 warning 已接入；后续更完整伪静态规则配置仍待补强。
- [~] `content.category` 支持更好用的栏目树、排序、模板、权限：后台树接口、树形标题、breadcrumb 路径展示、关键词过滤、表格展开/收起、表格内排序保存、拖拽排序弹窗、拖拽弹窗内跨父级迁移、父栏目下拉、父级 path/level 计算、子树移动后的 path/level 重算、栏目描述/封面/模板键保存与返回、栏目 slug 保存期安全校验、存在子栏目/内容时删除保护、内容访问规则向栏目父级继承、公开页栏目筛选、栏目 SEO head 消费和栏目模板键布局选择已接入；实例配置 `maxDepth` 已接入保存和拖拽迁移检查；更完整树控件仍待补强。
- [~] `content.media` 支持封面图、正文资源、资源反查、删除保护：封面/正文资源引用同步、详情返回、引用列表、引用计数、删除保护、批量启用/停用/删除、URL 扩展名与常见 MIME 推导、宽高字段管理、URL/大小/尺寸元数据保存校验、浏览器侧图片尺寸识别、编辑页媒体资源选择器、媒体资源 `attachmentXid` 管理和宿主附件上传入口已接入；实例配置 `maxAssetSizeBytes`、`maxImageDimension`、`serverDetectImageSize` 与 `maxDetailMediaRows` 已接入保存校验、详情媒体附带输出上限和本地插件静态 PNG/JPEG/GIF 文件头自动回填尺寸；宿主附件上传后自动回填链路仍待补强。
- [~] `content.revision` 支持版本列表、字段级 diff、恢复：版本快照、版本列表、详情、字段级 diff、恢复 API、恢复前当前内容对比、能力包页恢复确认入口和编辑页版本弹窗/Diff/Restore 操作已接入；能力包页和编辑页已用字段级 Before/After 表格替代原始 JSON 展示，并已补长文本/多行字段逐行高亮、图片资源预览、URL 资源链接、对象/数组结构化 diff、数值 delta、布尔 typed value、日期时间文本、枚举 option label 和栏目 relation label；实例配置 `maxSnapshotsPerContent` 已接入按内容保留新版本、剪裁旧版本；自定义关联字段专用 diff 仍待补强。
- [~] `content.workflow` 支持提交、审核、驳回、发布、下线、定时发布：提交、通过/发布、驳回、下线、schedule、日志、审核待办、通知记录、工作流统计、审批配置诊断、定时到期诊断、能力包页动作入口、到期任务有界执行、编辑页动作按钮、动作指派人记录、基础状态流转校验和到期定时发布处理已接入，契约已声明 submit/review/publish 动作权限映射，并在发布/下线后同步搜索、sitemap、related、静态化等派生能力；定时发布默认批量数量、后台列表上限、审核原因长度上限、发布所需审核通过次数和不同审核人要求可通过生成期配置调整；次数型多级审批 V1、operatorId 日志记录和同操作者重复审核拦截已接入，多级节点模型和自动任务仍待补强。
- [~] `content.search` 支持后台和前台搜索：前台/后台搜索 API、索引重建、索引统计、管理页查询探针、排序解释、写入同步、独立索引查询、可配置权重排序、标题精确命中加权、标题前缀命中加权、查询词覆盖度加权、栏目收窄筛选、权重契约 `configKeys`、排序解释契约 `rankingExplain`、查询最短长度和返回数量上限配置、索引重建 `limit/offset/hasMore/nextOffset` 分片及能力页连续重建入口、命中附近摘要裁剪、公开页搜索接口切换、分数/摘要/关键词高亮展示、常见分隔符归一化、常见中文/全角标点分隔、连续 CJK 字符宽松 LIKE、空白分隔查询词有序匹配、LIKE 通配符转义和多词摘要来源选择已接入并按能力包启用状态注册；更完整中文分词和复杂相关性仍待补强。
- [~] `content.sitemap` 支持 sitemap、RSS、robots、增量生成：sitemap、sitemap-index、RSS、robots 输出、后台入口、只读刷新计划预检、条目刷新、统计、发布/删除同步、公开输出优先读取 `content_sitemap_entry`、`content.sitemap.siteUrl` 绝对 URL 输出、`sitemap.xml?page=N` 分片、写入侧落盘缓存、临时文件完整写入后替换、`sitemap/cache.json` write-through 元数据、`sitemap/dirty.json` 失效标记、TTL/过期时间、dirty 元数据读取、缓存文件存在性/大小/更新时间诊断、`siteUrl/cacheTtlSeconds` 契约配置键、表格化缓存统计面板和刷新上限 UI 已接入；真正后台任务队列仍待补强。
- [~] `content.related` 支持手工关联和规则关联：手工关联 CRUD、公开关联列表、详情附带输出、公开输出数量配置、有界同栏目规则重建、能力包页有界规则重建和只读规则分值预览、公式/原因解释表格、内容保存/导入后的当前内容规则刷新、有界反向推荐同步、工作流发布/定时发布后的同栏目邻近内容有界刷新、手工优先去重，以及按同栏目/共同标签/共同专题可配置加分的规则权重已接入；更完整复杂规则计算器仍待补强。
- [~] `content.form` 支持内容附属表单：表单定义、schema textarea、字段追加/删除/重排式设计辅助、字段预览/点选回填、空字段名/重复字段名/required 未命中/required 重复提示与保存校验、placeholder/校验边界/pattern/options 辅助写入、JSON 格式化/校验、保存期 `maxFields` 字段数量上限、公开提交、`required` 字段校验、`fields[].type/format/minLength/maxLength/min/max/minItems/maxItems/pattern/options|list` 校验、email/url/date/phone 格式校验、honeypot、`minSubmitSeconds` 提交耗时和 IP 窗口限频基础反垃圾校验、能力包级 `maxSubmissionBytes` 默认提交大小上限、单表单 schema 覆盖、后台提交记录、request/session 字段来源 IP 记录、JSON 导出、处理状态和处理备注/处理人/处理时间、本地通知日志、通知已读/未读状态、后台列表上限和提交导出上限配置已接入；完整拖拽式设计器和外部通知仍待补强。
- [~] `content.sensitive` 支持敏感词词库、扫描策略和命中日志：内容保存前扫描、命中日志、替换/拦截/仅记录/转审核策略、实例字段配置、按词库 scope 区分 content/comment/check 扫描、评论提交扫描与 mark 策略强制待审核、命中日志按 `logRetentionDays` 清理、批量导入、后台词库分组和分组筛选、`sensitive/stats` 词库/命中统计、`matchMode=substring|asciiWord` 词边界模式、`maxWordLength` 词条长度上限和 `maxHitsPerScan` 单次扫描命中上限已接入；中文分词仍待补强。
- [~] `content.static` 支持静态化生成、预览、清理和受限内容保护：规则保存、任务记录、产物记录、公开生成/预览、后台清理、自动生成、自动清理、保存期风险提示和受限内容 noindex 壳页已接入；自动生成规则数已通过 `autoGenerateRuleLimit` 限制并封顶 200，后台列表和产物清理已通过 `maxListRows/maxCleanRows` 做有界输出/删除，任务列表支持状态/规则/内容筛选，产物列表支持内容 ID 和路径筛选，失败任务可单条或按 `maxTaskRetryRows` 批量重试，后台统计可查看规则、任务、产物和状态分布；后台异步任务队列仍待补强。
- [~] `content.tag` / `content.topic` 支持标签、专题和内容绑定：标签列表/详情/内容列表、标签绑定、即时创建、数量上限、标签合并、标签/专题有界批量启用停用、后台标签/专题统计、专题列表/详情/内容列表、单专题/多专题绑定模式、批量绑定上限、公开列表/内容列表/详情附带字段/后台关系列表输出上限、专题内容有界排序和 Up/Down 编排辅助已接入；更细的可视化拖拽体验仍待补强。
- [~] `content.access` 支持阅读等级、会员组、付费、密码访问：阅读等级、登录、会员组、密码、付费和私有模式已接入基础读取检查，已支持 `content.category` 启用时内容规则向所属栏目及父级栏目继承，权限检查响应返回 `ruleMatched/ruleSource/ruleTargetId/inheritedCategoryId` 便于调试命中来源，paid 模式返回价格和支付要求，并支持外部订单系统通过 `paidSessionKey` 注入可信 session claim 后放行，password 模式新写入已升级为带 salt 与 `passwordHashIterations` 的 `xsha256i:` 迭代哈希，编辑空密码保留原哈希，并兼容旧 `xsha256:`、`xrt64:` 与明文；受限内容静态化已改为生成无正文的 `noindex,nofollow` 壳页面，避免公开裸静态内容泄露；正式订单插件仍待后续独立接入。
- [~] `content.audit-log` 覆盖所有敏感内容操作：核心内容写操作、栏目新增/更新/删除/排序、权限规则变更、媒体、相关推荐、表单、评论、标签、专题、敏感词、导入导出、静态化规则/任务、搜索重建、sitemap 刷新和定时发布手动运行已接入；请求处理路径中的审计 IP 已统一优先使用真实 request remote IP，并保留 session 字段来源兜底，后台已提供按保留天数清理旧日志，默认保留天数、后台列表上限和单次清理行数可通过生成期配置调整，后台列表支持 `targetType/action/targetId` 过滤，内容更新已写入字段级差异 detail，后台只读详情视图已能查看原始与解析后的 detail，后台统计已能查看动作和对象类型分布，后续继续补外部任务覆盖。
- [~] `content.import-export` 支持导入预检、失败行报告、字段选择：导入预检、字段白名单、确认导入、任务报告、失败行回放、`insert/update/skip` 冲突策略、已完成导出任务 JSON 附件下载、JSON 导出 `limit/offset` 分片、未传 `limit` 时默认按配置上限切片、管理页按 `nextOffset` 连续导出、管理页分片导入提交、服务端可配置单批导入上限和单次导出上限、导入成功计数和 `imported/partial_failed/failed` 任务状态、导入导出任务状态统计已接入；真正流式上传解析和后台任务队列仍待补强。
- [~] `content.form` 支持表单定义和提交管理：表单定义、字段辅助设计、字段新增/更新/删除/排序、字段预览/点选回填、空字段名/重复字段名/required 未命中/required 重复提示与保存校验、结构格式化、结构校验、必填/类型/格式/范围/options 校验、公开提交、提交列表、提交统计、导出、处理状态、通知日志、反垃圾边界和内容访问权限集成已接入；完整拖拽设计器和外部通知仍待补强。
- [~] `content.revision` 支持版本快照、对比和恢复：后台列表、按内容 ID 过滤、字段 diff、恢复预览、恢复确认、长文本/媒体 URL/图片/结构化字段对比、枚举 label、栏目 relation label、变更类型/类型变化/长度摘要和恢复后的派生数据同步已接入；字段类型专用 diff 继续细化。
- [~] `content.workflow` 支持审核流转和定时发布：提交审核、审核通过、驳回、下线、定时发布、到期执行、流转日志、审核待办、通知日志、日志按内容筛选、通知按审核人筛选、动作权限契约、次数型多级审批 V1、审批配置诊断、定时到期诊断、operatorId 日志记录、同操作者重复审核拦截和派生数据同步已接入；多级节点模型和外部自动任务仍待补强。
- [x] 所有能力包有生成前 advisor 风险提示：`Content_BuildAdvisor` 已按启用能力包输出生成前 warning，覆盖当前 21 个内置能力包；内容模型生成按钮已接入预检确认流，错误阻断生成，warning 需要确认后继续；advisor 已从逐包硬编码表迁移为读取能力包 manifest 的 `advisorMessage/acceptanceApiPath/acceptanceViewPath`，只保留通用兜底，检查脚本会强制校验三项元数据存在且验收路径包含 `{pluginXid}`。
- [~] 所有能力包有最小浏览器/API 验收路径：`Content_BuildAdvisor` 已为当前 21 个内置能力包输出 `kind=acceptance` 的 `apiPath` 与 `viewPath`，使用 `{pluginXid}` 占位；`tools/smoke_capability_acceptance.ps1` 已改为直接读取能力包 `pack.json.acceptanceApiPath/acceptanceViewPath` 做 API + 页面 HTTP smoke，避免脚本硬编码路径与 manifest 漂移，状态码仅接受 `2xx/3xx/401/403`，可以兼容鉴权并拒绝 404/5xx；2xx API 响应会继续校验 JSON 形态，2xx 页面响应会继续校验 xAdmin HTML/layui/managed 页面形态，避免错误页面或空响应误判为通过；真实生成物验收时可传入 `-ManifestPath runtime/capability.manifest.json` 或 `-RuntimeDir runtime`，脚本会读取 `loadPolicy=enabled-packs-only` 的 `abilityPacks` 并只烟测已启用能力包；新增 `tools/smoke_generated_runtime.ps1` 作为生成物验收入口，会从 `runtime/managed.json` 推断 `PluginXid`，固定校验 runtime 三清单、基础内容后台页、后台列表 API、公开列表 API，并串联能力包 enabled-only smoke；内容模型生成页预检后会展示已启用能力包的 API/页面验收路径；`tools/check_content_system.ps1` 已汇总能力包检查、前端 JS、模板 JS、C 模板编译、声明能力包 source 编译、动态路由风险 warning 展示链路和 smoke 脚本语法，`.github/workflows/cms-capability.yml` 已接入 push、pull_request 和手动触发门禁；后续接入自动生成与服务启动。

### Phase 3：能力包代码拆分与 Hook 化

- [~] 从稳定的大模板中识别真实扩展点：当前已从 `managed_main.c.tpl`、`managed_ability.html.tpl`、`managed_editor.html.tpl`、`managed_public.html.tpl` 和 `managed_category.html.tpl` 中整理出第一批稳定扩展点，先作为拆分依据，暂不立即 patch 模板。
- [~] 定义生成期 slot：第一批 slot 先覆盖 schema、权限/菜单/路由注册、后台能力页、编辑页字段注入、公开页 head/列表/详情扩展、写入侧同步、删除侧清理、能力包 advisor/acceptance 元数据。
- [~] 定义运行期生命周期 hook：第一批 hook 先覆盖 `beforeSave`、`afterSave`、`afterDelete`、`afterPublish`、`afterOffline`、`beforePublicList`、`beforePublicDetail`、`decorateListItem`、`decorateDetailItem`、`adminAction`、`scheduledTask`。
- [~] 支持能力包复制 source/include/template/assets：检查脚本已支持声明路径校验，生成器已能把启用能力包声明的 `sourceFiles/includeFiles/templateFiles/assetFiles` 复制到生成结果，并把启用能力包的 `sourceFiles` 写入 `plugin.json` build.sources、把 `includeFiles` 所在目录写入 build.includeDirs；`content.slug` 已加入最小 source/include 声明样例并通过汇总检查，后续拆分真实业务源码时继续扩大覆盖。
- [~] 支持能力包声明 include/source：`pack.json` 可声明 `sourceFiles/includeFiles/templateFiles/assetFiles` 相对路径，`tools/check_capability_packs.ps1` 会校验路径存在且不能越出能力包目录，生成器只消费启用能力包的声明文件。
- [~] 支持 `#ifdef XADMIN_CAP_*` 过渡开关：生成的 `plugin.json` build.defines 已按启用能力包派生 `XADMIN_CAP_CONTENT_*` 宏；大模板代码逐块包裹仍待后续拆分时推进。
- [~] 未启用能力包不复制、不 include、不编译：声明文件复制、build.sources、build.includeDirs 已只遍历启用能力包；后续拆分真实能力包源码时继续用验收用例压实。
- [x] `patches.json` 保留为高级能力，但不作为默认路径：当前系统只安装/展示 `patches_json`，生成器不消费 patch；检查脚本允许空 patch，若存在 operations 则必须显式声明 `advanced=true`，避免误进入默认装配链路。
- [~] 基础内容插件与能力包代码彻底分离：生成物已新增 `runtime/capability.manifest.json`，只写入启用能力包的 manifest、实例配置和挂载信息，作为后续能力包源码拆分后的运行清单；`tools/check_content_system.ps1` 已加入该生成链路的静态验收；大模板内代码仍待继续迁出。

#### Phase 3.1 首批扩展点清单

| 扩展点 | 当前模板位置 | 目标用途 | 当前依据 |
| --- | --- | --- | --- |
| `schema.sql` | `G_SchemaSql` / `Managed_EnsureSchema` | 能力包表、索引、轻量迁移 | 21 个能力包已有 schema 与 `Managed_TableColumnExists` 迁移路径 |
| `admin.auth` | `Managed_OnStart` 权限注册 | 能力包权限项按启用状态注册 | workflow/search/import-export 等已有权限契约 |
| `admin.route` | `Managed_OnStart` route 注册 | 后台 API、公开 API、后台页面路由按能力包挂载 | 当前所有新增 API 均按 `Managed_AbilityPackMounted` 分支注册 |
| `admin.menu` | `Managed_OnStart` 菜单注册 | 能力包后台入口按启用状态显示 | 能力包页、栏目页、静态化页已经拆入口 |
| `admin.abilityPage` | `managed_ability.html.tpl` | 表格、弹窗、工具栏、批量操作、验收入口 | 21 个能力包已有统一表格管理页 |
| `editor.fields` | `managed_editor.html.tpl` | SEO、slug、media、access 等编辑字段注入 | 当前编辑页已按能力包显示扩展字段 |
| `public.head` | `managed_public.html.tpl` | SEO、栏目 SEO、canonical、meta 输出 | content.seo/category 已接入 |
| `public.listItem` | `Managed_CreateItemFromStmt` / list 输出 | media、seo、related、access、search snippet 装饰列表项 | 当前列表返回已挂载多类扩展字段 |
| `public.detailItem` | `Managed_RequestDetailCommon` | detail 输出 related/media/seo/access 等字段 | 当前详情页已有扩展数据附加 |
| `write.beforeSave` | `Managed_RequestSave` 校验前后 | 敏感词、slug 唯一性、access 预校验 | sensitive/slug/form 校验逻辑已稳定 |
| `write.afterSave` | `Managed_RequestSave` 写库后 | revision、search、sitemap、related、media、seo、audit 同步 | 多能力包已在同一点同步 |
| `write.afterDelete` | `Managed_RequestDelete` | search/sitemap/static/related 清理 | 删除侧已有派生数据清理 |
| `workflow.afterPublish` | workflow action / scheduled publish | 发布后同步 search/sitemap/related/static | workflow 已调用派生能力同步 |
| `scheduledTask` | 后台任务/手动 API | sitemap refresh、search rebuild、scheduled publish | 当前以后台 API 形式存在，后续可升级任务调度 |
| `advisor.meta` | `content_advisor.h` + capability manifest | 生成前 warning 与最小验收路径 | 已覆盖 21 个能力包，advisor 已读取 manifest 元数据，保留通用兜底 |

Phase 3 拆分时保持一条硬边界：默认路径不做源码 patch；slot/hook 只允许在明确位置插入完整代码块或注册函数。`patches.json` 只保留给少数高级能力，必须单独开关、单独风险提示、单独验收。

## 8. 文档口径修订清单

已知旧口径需要被本文覆盖：

- `内容类型DSL规范.md` 中“栏目进入核心能力”的修订已过期。
- `栏目系统设计方案.md` 中“栏目不是可选能力包”的结论已过期。
- `内容系统能力包体系增强SPEC.md` 中 V1 七个能力包作为当前完成态保留，但下一阶段目标是更完整的 CMS 能力包体系。
- `内容系统能力包规范.md` 中能力包生成期装配方向保留，但当前实现路线先做强大模板和能力覆盖，后做代码拆分。

## 10. 开发与测试约束

### 10.1 页面开发约束

- 管理页面应遵循 xAdmin 现有开发范式，优先参考前后台权限管理相关页面。
- 数据管理页面以表格为主，新增、编辑以弹窗为主。
- 看板类能力必须拆成单独看板页面，不把统计、配置、列表、编辑能力全部挤在同一个管理页。
- 弹窗保存和取消按钮应固定在弹窗内容底部区域，并用横线与表单内容分开，参考前后台权限管理弹窗风格。
- 页面交互和布局优先保持后台管理系统的工作台风格，不做营销页或过度装饰化页面。

### 10.2 后端代码约束

- 后端代码尽可能保留必要注释，说明业务边界、权限边界、缓存边界和动态路由编译边界。
- 后端代码风格参考前后台权限管理模块，优先使用现有 DB、缓存、权限、响应封装方式。
- 请求路径上的代码必须保持轻量，避免引入不可预测耗时、数据库扫描或运行时规则编译。

### 10.3 测试约束

- 新接口调试期可以先将 URI 设置为不鉴权，通过 HTTP 协议直接访问并检查返回内容。
- 接口内容、状态码、错误分支调试完成后，再将 URI 调整为合理鉴权配置。
- 页面功能不要求每个页面都先进行人工交互测试；应优先通过 HTTP 接口验证数据读写和核心内容正确性。
- 涉及管理页的最终验收仍需要抽样检查表格、弹窗新增、弹窗编辑、保存、取消和错误提示是否符合 xAdmin 范式。

## 11. 验收标准

路由系统验收：

- 静态路由性能路径不增加动态规则检查成本。
- 动态路由新增、删除、禁用后能重新编译正则组。
- 动态路由编译失败不影响旧路由表。
- 动态路由权限可以在后台授权。
- 动态路由命中后可提取参数。
- 静态路由、动态路由、插件静态文件、网站静态文件按既定顺序工作。

能力包验收：

- 无能力包生成的内容插件可正常 CRUD 和前台访问。
- 每个启用能力包都有真实可运行功能。
- 能力包显示名称支持中文。
- 未启用能力包不注册对应路由、菜单、权限。
- 过渡期未启用能力包不执行对应逻辑。
- 最终阶段未启用能力包不生成对应代码。
## 最新实现记录

- [x] `content.media` 服务端本地图片元数据兜底：媒体保存时会在 `serverDetectImageSize=true` 下，仅对当前插件 `MANAGED_STATIC_URL_PREFIX` 下的本地静态资源读取文件头，识别 PNG/JPEG/GIF 的宽高，并在请求未传 `size/width/height` 时自动回填；外部 URL 不抓取，避免把媒体保存接口变成不可预测的远程 I/O。
- [x] `content.tag` 标签合并：后台新增 `tag/merge` 接口和能力页操作区，将源标签的内容关联以 `INSERT OR IGNORE` 合并到目标标签，再删除源关联，并可选择软删除源标签；合并动作写入审计日志，避免重复标签长期污染内容系统。
- [x] `content.topic` 专题内容排序：后台新增 `topic/content/sort` 有界批量排序接口，单次最多 500 条绑定记录，能力页提供 JSON 编排入口；排序写入审计日志，专题公开内容列表继续按 `topic_content.sort` 输出。
- [x] `content.comment` 站内通知日志：评论提交、审核通过/驳回、隐藏、删除会写入 `comment_notification`，后台新增 `comment/notification/list` 和能力页通知视图；本阶段只做站内事件记录，不接外部邮件/短信，避免引入不可控依赖。
- [x] `content.sensitive` 批量导入：后台新增 `sensitive/word/import`，支持 preview/confirm 两步和最多 1000 行 JSON 数组导入；能力页提供批量导入入口，confirm 写入审计日志。
- [x] `content.sensitive` import transaction guard: confirm-mode sensitive-word import now verifies transaction begin/prepare/commit, counts inserted rows only after successful `sqlite3_step`, and marks failed rows explicitly.
- [x] `content.seo` 内容级运行期模板：实例配置新增 title/keywords/description/canonical 模板入口，公开 SEO meta 构建时以显式字段和后台 SEO 表记录为优先，模板仅对空值做兜底；支持 `{id}`、`{title}`、`{summary}`、`{slug}`、`{categoryId}`、`{siteName}`、`{pluginXid}` 等基础变量。
- [x] `content.workflow` 站内通知日志：工作流动作写入 `content_workflow_log` 后同步写入 `content_workflow_notification`，后台新增 `workflow/notification/list`，能力页新增通知视图；本阶段只记录本地通知事件，不接外部消息通道。
- [x] `content.sensitive` 词边界模式：实例配置新增 `matchMode`，默认保持 `substring` 子串命中；可选 `asciiWord` 时要求命中词前后不是 ASCII 字母、数字或下划线，降低英文短词误杀风险。
- [x] `content.sensitive` 敏感词长度上限：实例配置新增 `maxWordLength`，默认 0 不改变现有行为；开启后敏感词保存和批量导入都会拒绝异常长词条，避免后续扫描被单个词条放大成本。
- [x] `content.sensitive` 单次扫描命中上限：实例配置新增 `maxHitsPerScan`，默认 200、最大 10000；内容保存、评论提交和公开检测扫描命中过多时停止继续写命中日志和返回命中数组，避免一次请求放大副作用。
- [x] `content.comment` 最小正文长度：实例配置新增 `minBodyLength`，默认 0 不改变现有行为；启用后公开评论提交会在写库前拒绝过短正文，与既有 `maxBodyLength`、重复窗口和 IP 窗口限频一起组成基础反垃圾策略。
- [x] `content.form` 本地通知日志：新增 `content_form_notification`，公开表单提交和后台处理状态更新会写入通知事件；后台新增 `form/notification/list`，能力页新增 Notifications 视图。
- [x] `content.access` 规则命中来源输出：`access/check` 和详情挂载的 access 信息新增 `ruleMatched/ruleSource`，明确区分无规则、内容规则与栏目继承规则，同时保留 `ruleTargetId/inheritedCategoryId` 供调试和后台判断使用。
- [x] `content.like` / `content.view-stat` 配置契约整理：点赞实例表单中文化，并在契约中声明 `allowGuest/dedup`；访问统计契约补齐 `rankEnabled` 和 `configKeys`，避免生成期配置与运行时能力说明脱节。
- [x] `content.like` 状态接口输入校验：公开 `like/status` 已补齐 `contentId` 必填和 actor key 安全校验，与 create/cancel 的公开输入边界保持一致。
- [x] `content.comment` 最短提交耗时配置化：实例配置新增 `minSubmitSeconds`，默认 2 秒保持原行为，可设为 0 关闭；公开评论提交继续只在写入前做有界检查。
- [x] `content.comment` 评论链接数量上限：实例配置新增 `maxLinks`，默认 0 不改变现有行为；开启后公开评论提交只扫描当前正文中的 `http://` 与 `https://` 次数，超限则拒绝，继续保持写入期、固定成本的基础反垃圾策略。
- [x] `content.comment` 公开评论列表上限配置化：实例配置新增 `maxPublicRows`，公开评论列表接口支持请求 `limit`，并按能力包配置封顶，树形回复构建只处理已取回的有界行集。
- [x] `content.comment` 回复层级上限配置化：实例配置新增 `maxReplyDepth`，默认 0 不改变现有行为；开启后回复提交会沿父评论链做有界检查，避免无限嵌套评论树放大后续读取和展示成本。
- [x] `content.form` 最短提交耗时配置化：实例配置新增 `minSubmitSeconds`，默认 2 秒保持原行为，可设为 0 关闭；公开表单提交继续只在写入前做有界检查。
- [x] `content.search` 权重配置契约补齐：`titleWeight/keywordWeight/summaryWeight/bodyWeight` 已补充 `runtime.configKeys`，让实例表单与运行时权重读取保持可校验一致。
- [x] `content.search` 查询边界配置化：实例配置新增 `minQueryLength` 与 `maxResultLimit`，默认保持当前行为；公开/后台搜索会在查询前做最短长度拦截和 limit 封顶，避免短词或超大返回数直接进入索引查询。
- [x] `content.search` rebuild transaction guard: search index rebuild now verifies transaction begin/commit and rolls back on commit failure before writing audit logs or returning success.
- [x] `content.search` CJK punctuation query normalization: common Chinese/fullwidth punctuation now acts as a bounded query delimiter in normalization, LIKE pattern generation, multi-term snippet selection and first-hit positioning; `contracts.runtime.queryNormalization` declares `cjkPunctuation`.
- [x] `content.search` exact title relevance boost: search ranking now adds configurable `exactTitleWeight` when normalized query equals indexed title, improving common CMS title lookup without adding a new table or request-time tokenizer.
- [x] `content.related` 公开推荐数量配置化：实例配置新增 `maxPublicRelated`，默认 20 条并封顶 50 条；详情附带 `relatedList` 和公开 `related/list` 均改为绑定 LIMIT 参数，避免推荐输出数量硬编码漂移。
- [x] `content.related` 后台列表上限配置化：实例配置新增 `maxAdminListRows`，默认 500 条并封顶 1000 条；后台关联列表支持请求 `limit`，但始终受能力包配置约束。
- [x] `content.audit-log` 后台列表上限配置化：实例配置新增 `maxListRows`，默认 500 条并封顶 1000 条；后台列表接口支持请求 `limit`，但始终受能力包配置约束。
- [x] `content.workflow` 后台列表上限配置化：实例配置新增 `maxListRows`，默认 200 条并封顶 1000 条；日志、待办和通知列表均支持请求 `limit`，但始终受能力包配置约束。
- [x] `content.sitemap` 缓存配置契约补齐：`cacheTtlSeconds` 已补充到 `runtime.configKeys`，让实例表单与运行时缓存 TTL 读取保持可校验一致。
- [x] `content.access` 配置契约标准化：保留既有 `rules.configKeys` 描述，同时补充 `runtime.configKeys`，让 `passwordHashIterations/paidSessionKey` 与其他能力包使用同一配置校验口径。
- [x] `content.access` read-level trust boundary: level-mode access now trusts session `authLevel/__authLevel__` by default; request query `readLevel` only works when `allowQueryReadLevel` is explicitly enabled for debugging, avoiding public users self-raising read level through URL parameters.
- [x] `content.access` read-level range guard: instance config now exposes `maxReadLevel`; level rules reject negative or over-limit `requiredReadLevel`, and runtime session/debug read levels are clamped before comparison.
- [x] `content.redirect` 提交期链路风险检查增强：保存和批量导入预检会在提交期最多追踪 8 跳已有跳转规则，提示两步环、链式环和链深过长风险；公开访问热路径不增加链路追踪成本。
- [x] `content.form` 默认提交大小上限配置化：实例配置新增 `maxSubmissionBytes`，作为表单提交 JSON 大小的能力包级默认值；单个表单 schema 的 `maxSubmissionBytes/maxBytes` 仍可覆盖。
- [x] `content.form` 表单字段数量上限配置化：实例配置新增 `maxFields`，后台保存表单 schema 时会解析 `fields[]` 并按配置拒绝超大表单定义，避免公开提交路径长期承担不可控字段遍历成本。
- [x] `content.form` 后台列表和导出上限配置化：实例配置新增 `maxListRows/maxExportRows`，表单列表、提交列表、通知列表和提交 JSON 导出均改为绑定 LIMIT 参数并受配置封顶。
- [x] `content.import-export` 导出分片上限配置化：实例配置新增 `maxExportRows`，默认 1000 行，服务端封顶 5000 行；未显式传入 `limit` 的导出也会按该上限切片返回，并通过 `hasMore/nextOffset` 连续导出；导入批次继续使用 `maxBatchRows`。
- [x] `content.access` 后台规则列表上限配置化：实例配置新增 `maxListRows`，规则列表接口支持请求 `limit`，SQL 使用绑定参数并始终受能力包配置封顶。
- [x] `content.seo` 后台元信息列表上限配置化：实例配置新增 `maxListRows`，SEO 元信息列表接口支持请求 `limit`，SQL 使用绑定参数并始终受能力包配置封顶。
- [x] `content.seo` meta target validation: SEO meta save now verifies `contentId` still points to a live content row before insert/update, avoiding orphan SEO records.
- [x] `content.revision` 后台版本列表上限配置化：实例配置新增 `maxListRows`，全局版本列表和按内容版本列表都使用绑定 LIMIT，并始终受能力包配置封顶。
- [x] `content.slug` 后台历史列表上限配置化：实例配置新增 `maxHistoryRows`，slug 历史列表接口支持请求 `limit`，按内容和全局历史查询都使用绑定 LIMIT。
- [x] `content.import-export` 后台任务列表上限配置化：实例配置新增 `maxJobRows`，导入任务列表和导出任务列表都支持请求 `limit`，并始终受能力包配置封顶。
- [x] `content.category` 公开栏目内容列表上限配置化：实例配置新增 `maxPublicContentRows`，`category.contents` 支持请求 `limit`，并使用绑定 LIMIT 控制公开输出规模。
- [x] `content.category` save/delete write guard: category save/delete now verify the target row was inserted, updated or soft-deleted before rewriting descendant paths, writing audit logs or returning success.
- [x] `content.sitemap` 列表和刷新上限配置化：实例配置新增 `maxEntryListRows/maxRefreshRows`，后台条目列表和手动刷新均支持请求 `limit`，并按能力包配置封顶。
- [x] `content.sitemap` refresh transaction guard: manual sitemap refresh now verifies `BEGIN IMMEDIATE` and `COMMIT` success, rolls back on commit failure, and only writes cache/audit/success after the DB refresh transaction commits.
- [x] `content.like` 后台排行列表上限配置化：实例配置新增 `maxAdminListRows`，点赞计数列表支持请求 `limit`，并按能力包配置封顶。
- [x] `content.view-stat` 排行和后台统计列表上限配置化：实例配置新增 `maxRankRows/maxAdminListRows`，公开排行、后台计数、访问日志和日统计列表均受配置上限约束。
- [x] `content.media` 后台引用列表上限配置化：实例配置新增 `maxRefListRows`，媒体引用列表支持请求 `limit`，按单资源和全局引用查询都使用绑定 LIMIT。
- [x] `content.media` MIME 白名单配置化：实例配置新增 `allowedMimeTypes`，默认空值不改变现有行为；配置后媒体保存会在扩展名推导 MIME 后做逗号分隔精确匹配，拒绝不在业务白名单中的资源登记。
- [x] `content.media` 媒体列表上限配置化：实例配置新增 `maxListRows`，公开和后台媒体列表接口支持请求 `limit`，并按能力包配置封顶，SQL 使用绑定 `LIMIT ?`。
- [x] `content.audit-log` 后台列表过滤：后台 `audit-log/list` 支持 `targetType/action/targetId` 查询参数，SQL 保持固定形态并全部使用绑定参数，便于生产排障时按对象或动作定位日志。
- [x] `content.audit-log` 单次清理上限配置化：实例配置新增 `maxCleanupRows`，清理接口只删除符合保留期条件的有界批次，避免一次请求删除过多审计日志。
- [x] `content.sitemap` RSS 回退输出上限配置化：实例配置新增 `maxRssRows`，缓存不可用时的 RSS 即时查询和缓存读取条数都受配置上限约束。
- [x] `content.related` 全量规则重建源内容上限配置化：实例配置新增 `maxRebuildSources`，全量重建时源内容扫描使用绑定 LIMIT，避免一次请求处理过多内容。
- [x] `content.related` rebuild transaction guard: manual related-rule rebuild now verifies transaction begin/commit and rolls back on commit failure before audit/success response.
- [x] `content.related` rebuild write guard: manual related-rule rebuild now also verifies rule cleanup updates and source scans before commit, rolling back with `related rebuild write failed` if the transactional rebuild cannot complete cleanly.
- [x] `content.related` publish peer cleanup guard: publish-time peer refresh now skips rebuilding a peer when its previous rule cleanup update fails, keeping bounded fan-out refreshes from layering new rule rows over stale ones.
- [x] 模板多行查询上限门禁：`tools/check_content_system.ps1` 新增 `managed multi-row limit guard`，禁止 `managed_main.c.tpl` 再出现硬编码多行 `LIMIT N`，要求后续列表/批处理统一使用能力包配置和绑定 LIMIT。
- [x] 能力包静态门禁接入 CI：新增 `.github/workflows/cms-capability.yml`，在 docs、能力包、内容模板、内容脚本、前端 JS 和 tools 变更时运行 `check_capability_packs.ps1`、`check_content_system.ps1` 与 `git diff --check`；`check_content_system.ps1` 已反向锁定工作流必须保留这三项门禁和关键路径触发条件。
- [x] `content.topic` 专题编排 UI 补强：能力页内容关联视图新增按专题加载、Up/Down 调整顺序、自动生成排序 JSON 和保存排序入口，继续复用已有 `topic/content/sort` 有界排序接口；契约新增 `arrangeUi`，检查脚本锁定编排 UI 标记。
- [x] `content.seo` 有效元信息预览：能力页新增按内容 ID 或 slug 调用公开 `seo/meta` 的预览入口，可直接查看模板、显式字段和后台覆盖后的最终 SEO 输出；契约新增 `previewUi`，检查脚本锁定预览 UI 标记。
- [x] `content.seo` 模板变量清单：能力页新增内容模板变量和栏目模板变量展示，契约新增 `contentTemplateVariables/categoryTemplateVariables`，检查脚本锁定 UI 与契约标记，减少模板配置误用。
- [x] `content.seo` 内容模板变量取值预览：公开 `seo/meta` 响应新增 `templateVariables`，能力页现有有效元信息预览会直接展示内容模板变量的实际取值；契约新增 `variableValuePreview`，检查脚本锁定响应字段。
- [x] `content.seo` 栏目模板变量对齐：公开页栏目 SEO 模板变量新增 `siteName`，与实例配置默认 `{categoryTitle} - {siteName}` 保持一致，同时保留 `pluginTitle` 别名。
- [x] `content.seo` 栏目模板变量取值预览：能力页新增栏目 ID/slug 预览入口，复用公开 `category/detail` 和运行期 `content.seo` 实例配置计算栏目 SEO meta，并返回 `templateVariables` 方便核对模板变量实际取值。
- [x] `content.seo` 模板 warning 预览：能力页内容/栏目 SEO 预览会扫描最终 title/keywords/description/canonical 中仍未替换的 `{token}`，在结果中返回 `warnings`；契约新增 `templateWarningPreview`。
- [x] `content.media` 附件元信息回填：能力页媒体资源弹窗新增“读取附件”，通过宿主 `/admin/attachment/get?xid=` 拉取附件记录并回填 URL、MIME、扩展名、大小和标题；契约新增 `attachmentFill`，检查脚本锁定 UI 与接口路径。
- [x] `content.media` 附件上传跨窗口回填：宿主附件上传页成功后向 opener 发送 `xadminAttachmentUploaded`，`content.media` 能力页收到同源消息后自动打开预填媒体资源弹窗；契约新增 `attachmentUploadMessage`，检查脚本锁定上传页和能力页消息标记。
- [x] `content.slug` public lookup limit and `content.category` tree list limits: slug detail/resolve scans now use `maxPublicLookupRows` with bound `LIMIT ?` and explicit overflow errors; category admin/public tree lists use `maxAdminTreeRows/maxPublicTreeRows` with bound `LIMIT ?` and return applied limits.
- [x] Base content list scan limit: `ui.list.maxScanRows` is now supported by generated plugins; core list APIs bind `LIMIT ?`, scan at most `maxScanRows + 1`, and return `scanLimit/scanLimitReached` so large payload-filtered lists have a predictable upper bound.
- [x] Content model editor exposes `ui.list.maxScanRows`: the page/display tab now saves and restores max scan rows, and the static check verifies the editor page wiring.
- [x] `content.sensitive` check and cleanup management UI: the ability page now exposes a public check probe using `/sensitive/check` and a bounded hit-log cleanup action using `/sensitive/log/cleanup` with `keepDays` and `limit`, so operators can validate dictionary behavior and clean logs without adding custom debug routes.
- [x] `content.comment` author name length bound: instance config now exposes `maxAuthorNameLength`, public comment submit rejects oversized author names before DB write, and the capability contract/check gate records this as part of bounded anti-spam input validation.
- [x] `content.comment` 黑白名单基础反垃圾：实例配置新增 `allowedAuthorNames/blockedAuthorNames/blockedBodyPhrases/blockedUserAgentPhrases`，评论提交前按固定分隔符列表做昵称精确匹配和正文/UA 子串匹配，不引入请求期外部服务或全局限流。
- [x] `content.comment` pre-parse request size bound: public comment submit now reads `maxRequestBytes` before JSON parsing and rejects oversized request bodies up front, keeping anonymous comment submission on a predictable input-size budget.
- [x] `content.form` pre-parse request size bound: public form submit now reads `maxRequestBytes` before JSON parsing, then still applies the existing schema-aware `maxSubmissionBytes` check after validation and serialization.
- [x] `content.like` pre-parse request size bound: like create/cancel now use `maxRequestBytes` before JSON parsing, with a small default budget because the public payload only needs content id and actor identity fields.
- [x] `content.view-stat` pre-parse request size bound: public view record writes now use `maxRequestBytes` before JSON parsing, keeping high-frequency analytics writes bounded while leaving public rank/status reads unchanged.
- [x] `content.comment` public hide request size bound: `comment/hide` now shares the same `maxRequestBytes` pre-parse guard as public comment creation, so both public comment write endpoints have the same input-size boundary.
- [x] `content.sensitive` pre-parse check request bound: public sensitive-word check now applies `maxCheckBytes` before JSON parsing and keeps the existing post-parse payload-size guard for the actual scanned data.
- [x] `content.static` generate request size bound: static generation now supports `maxGenerateRequestBytes` and rejects oversized JSON before parsing, keeping the manual generate entry on a small fixed payload budget.
- [x] `content.static` output path length guard: static relative output paths are capped at 240 bytes and configured output directories at 120 bytes during normalization, in addition to the existing traversal, drive-letter, backslash and percent-sign rejection.
- [x] `content.access` password input length bound: instance config now exposes `maxPasswordInputBytes`; admin rule save rejects oversized password inputs before hashing, and public password checks treat oversized input as denied without entering the iterated hash path.
- [x] `content.access` rule delete side-effect guard: access-rule delete now writes audit records and returns success only after the soft-delete matches a live rule row.
- [x] `content.redirect` admin boundary config: redirect list now clamps request `limit` by `maxListRows`; bulk import rejects item arrays beyond `maxImportRows`; save/import/delete reject oversized bodies by `maxRequestBytes` before JSON parsing, without adding cost to public redirect resolution.
- [x] `content.slug` repair boundary config: slug repair now clamps batch size by `maxRepairRows` and rejects oversized repair request bodies by `maxRequestBytes` before JSON parsing, while public slug lookup keeps the existing `maxPublicLookupRows` scan boundary.
- [x] `content.slug` repair transaction guard: confirm-mode slug repair now verifies `BEGIN IMMEDIATE` and `COMMIT` success, rolls back on commit failure, and reports explicit transaction/commit errors instead of partial false success.
- [x] `content.slug` 历史记录筛选：后台 `slug/history` 新增 contentId、oldSlug、newSlug、status 筛选，SQL 使用固定形态与绑定参数，能力页提供筛选和清空按钮，契约新增 `historyFilters`。
- [x] `content.comment` admin boundary config: comment status/delete now reject oversized request bodies by `maxRequestBytes` before JSON parsing; audit-log and notification list endpoints clamp request `limit` by `maxAdminListRows`.
- [x] `content.comment` write side-effect guard: public comment create/hide and admin status/delete now only refresh thread counts, emit notifications and write audit records after the insert/update actually succeeds.
- [x] `content.comment` counter refresh observability: comment count/create/hide/status/delete now return `counterRefreshed`, so stale thread counter refresh failures are visible without turning successful comment writes into false failures.
- [x] `content.comment` thread row guard: public comment create now refuses to insert a comment if the backing `comment_thread` row cannot be created or loaded, avoiding orphan thread ids after a failed thread bootstrap write.
- [x] `content.comment` User-Agent boundary: public comment create now reads request `User-Agent`, rejects oversized or control-character header values via configurable `maxUserAgentLength`, and persists the accepted value to `comment_item.user_agent` for later audit/diagnosis without treating it as a trusted unique identity.
- [x] `content.comment` 批量审核：后台新增 `comment/status-batch`，按 `maxAdminListRows` 限制单次 ID 数组，只允许批量通过或驳回，并逐条刷新线程计数、写入评论审核日志、通知和总审计日志；能力页审核工作台增加批量 ID 输入和结果面板。
- [x] `content.comment` 审核统计：后台新增 `comment/moderation-stats` 只读接口，返回待审、通过、驳回/隐藏、删除、近期待审数量和统计窗口天数；能力页审核工作台提供统计按钮，`moderationStatsRecentDays` 可配置近期窗口。
- [x] `content.comment` 专用审核队列：后台新增 `comment/moderation-queue` 只读接口，固定查询 pending 评论并联出内容标题、IP、UA、正文长度、链接数、积压秒数、审核次数和最近审核时间，支持 contentId/authorName 绑定筛选，继续受 `maxAdminListRows` 约束。
- [x] `content.search` CJK 宽松 LIKE：搜索 LIKE pattern 构造会识别连续 CJK 统一表意文字，并在相邻中文字符之间插入有界通配符，提高中文连续词查询对轻微分隔/组合差异的召回；响应返回 `cjkLooseLike` 便于排序解释和验收观察。
- [x] `content.comment` author name control-char boundary: public comment create now rejects author names containing ASCII control characters, keeping display names usable as untrusted presentation text while preserving the existing configurable length bound.
- [x] `content.comment` local log observability: comment audit and notification helpers now verify insert SQL; create/hide/status/delete responses expose `notificationSaved` and admin mutation responses also expose `commentAuditSaved`.
- [x] `content.sensitive` admin boundary config: sensitive word save/delete/import/cleanup now reject oversized admin request bodies by `maxRequestBytes` before JSON parsing; bulk import rows are capped by `maxImportRows` instead of a hardcoded limit and the runtime contract exposes both boundaries.
- [x] `content.sensitive` pending log promotion guard: content-create sensitive hit log promotion now verifies the target-id update SQL instead of treating pending-log ownership repair as an unchecked side effect.
- [x] `content.static` admin request boundary config: static rule save/delete and artifact cleanup now reject oversized admin request bodies by `maxRequestBytes` before JSON parsing, separate from the smaller `maxGenerateRequestBytes` budget used by manual generation.
- [x] `content.static` cleanup result observability: artifact cleanup now returns `cleaned` and `maxCleanRows` so the table/modal admin page can show the actual bounded deletion count instead of a generic success message.
- [x] `content.static` cleanup admin entry: the capability management page now exposes a bounded artifact cleanup form beside manual generation and renders the JSON result, so cleanup no longer requires manually calling the API.
- [x] `content.static` artifact registry guard: static generation now verifies old artifact-row cleanup before inserting the replacement registry row, avoiding successful file generation with stale artifact metadata.
- [x] `content.access` admin request boundary config: access rule save/delete now reject oversized admin request bodies by `maxRequestBytes` before JSON parsing, while the existing `maxPasswordInputBytes` guard remains focused on hashing cost.
- [x] `content.access` save write guard: access rule save now verifies insert/update success before returning success or writing audit logs, with explicit stale-row and save-failed errors.
- [x] `content.access` rule target validation: access rule save now verifies content targets and category targets still exist before writing permission rules, while category rules remain gated by `content.category`.
- [x] `content.related` admin request boundary config: related save/delete/rebuild now reject oversized admin request bodies by `maxRequestBytes` before JSON parsing, while rebuild source fan-out remains bounded by `maxRebuildSources`.
- [x] `content.related` save write guard: manual related-item save now verifies insert/update success and uses the saved relation id for audit logs instead of relying on unchecked `last_insert_rowid()`.
- [x] `content.related` relation target validation: manual related-item save now verifies both source and related content rows still exist before writing the relation, avoiding stale recommendation targets.
- [x] `content.related` delete side-effect guard: manual related-item delete now writes audit records and returns success only after the soft-delete matches a live relation row.
- [x] `content.audit-log` cleanup request boundary config: audit cleanup now rejects oversized admin request bodies by `maxRequestBytes` before JSON parsing, while list output and cleanup deletion stay bounded by `maxListRows/maxCleanupRows`.
- [x] `content.tag` / `content.topic` save write guard: tag/topic save now verifies insert/update success before returning ids or writing audit logs, matching the existing delete not-found handling.
- [x] `content.tag` / `content.topic` relation unbind guard: relation unbind endpoints now verify the relation row was actually deleted before refreshing counters, writing audit logs or returning success.
- [x] `content.tag` / `content.topic` counter refresh guard: tag/topic counter refresh helpers now verify SQL success; bind/merge transactions roll back on counter refresh failure, and unbind responses expose `counterRefreshed` for stale aggregate diagnosis.
- [x] `content.tag` merge transaction guard: tag merge now verifies source/target tags, requires transaction begin/commit success, rolls back if source soft-delete fails, and returns `sourceDeleted` for acceptance visibility.
- [x] `content.tag` bind transaction guard: tag bind now validates content/tag rows, rebuilds relations in an immediate transaction, rolls back non-empty requests that insert no relation rows, and returns deleted/inserted counts.
- [x] `content.tag` / `content.topic` commit-failure rollback: tag merge/tag bind/topic bind/topic bindContent now explicitly roll back when `COMMIT` fails, matching the other transactional write paths.
- [x] Managed transaction rollback gate: `tools/check_content_system.ps1` now scans `managed_main.c.tpl` and rejects any `COMMIT` failure branch that does not explicitly call `ROLLBACK`, keeping capability-pack transactional writes from reporting false success after failed commits.
- [x] `content.topic` content sort write guard: topic content sort now requires at least one relation row update before writing audit logs or returning success, so stale sort payloads fail explicitly.
- [x] `content.topic` content sort transaction guard: topic content sort now verifies transaction begin/commit and only counts updates after successful `sqlite3_step`.
- [x] `content.topic` bind transaction guard: topic bind and bindContent now run relation rebuilds in an immediate transaction, roll back non-empty bind requests that insert no relation rows, and return deleted/inserted counts.
- [x] `content.topic` bind target validation: topic bind and bindContent now validate topic/content rows before rebuilding relation sets, avoiding dangling `topic_content` rows when IDs are stale.
- [x] `content.form` admin request boundary config: form definition save/delete and submission status update now reuse `maxRequestBytes` before JSON parsing, matching the existing public submit request-size budget.
- [x] `content.form` write side-effect guard: form save/delete, submission status update and public submission insert now only return success, write audit logs or enqueue local notifications after the DB write actually updates or inserts a row; public submit also returns `submissionId` for traceable acceptance.
- [x] `content.form` notification observability: form submission and submission-status responses now include `notificationSaved`, so local notification-log write failures are visible without turning the primary form write into a false failure.
- [x] `content.form` bound content validation: form definition save now verifies optional `contentId` still points to a live content row before writing an attached form definition.
- [x] `content.sensitive` / `content.redirect` / `content.seo` / `content.static` write side-effect guard: sensitive word save/delete, redirect rule save/delete, SEO meta save/delete and static rule save/delete now verify insert/update/delete results before returning success or writing follow-up audit/response state, and return explicit not-found/save-failed errors for stale admin requests.
- [x] `content.redirect` write observability: redirect upsert now derives update success from `sqlite3_step` before checking changed rows, and resolve responses expose `hitRecorded` when the non-blocking hit counter update succeeds.
- [x] `content.like` write side-effect guard: public like creation now refreshes counters only after the like row is actually inserted; public cancel remains idempotent but returns `changed`, and admin status changes now fail on stale like IDs before counter refresh.
- [x] `content.like` counter write guard: like create/cancel/admin status now verify counter refresh SQL before returning success when the like state changed, and report `like counter save failed` instead of leaving visible counters stale.
- [x] `content.view-stat` write side-effect guard: public view record now verifies optional log insertion plus counter and daily-stat writes before returning `recorded`, and reports explicit save failures instead of silently accepting broken analytics writes.
- [x] `content.view-stat` public status aliases: `view/status`、`view/count` and `view/detail` are locked as lightweight counter aliases returning `contentId/viewCount/uniqueViewCount`, keeping compatibility routes explicit in the static gate.
- [x] `content.sitemap` refresh cache observability: manual sitemap refresh now returns `cacheWritten` and emits a warning when write-through cache files fail, while the audit detail records the cache write result for retry diagnosis.
- [x] `content.sitemap` refresh plan preflight: 后台新增 `sitemap/refresh-plan` 只读接口和能力页按钮，返回 `eligibleCount/currentEntryCount/willRefreshRows/willTruncate/writesData=false`，用于在不写库、不写缓存的情况下评估手动刷新成本。
- [x] `content.related` rule preview explanation: `related/rule/preview` 返回 `ruleFormula/reasonText/writeMode=preview-only/writesData=false`，能力页以表格展示候选的栏目分、共享标签/专题分和总分，便于不写库调参。
- [x] `content.sitemap` dirty metadata observability: `sitemap/stats` 不再固定返回 `cacheDirty=false`，而是读取 `sitemap/dirty.json` 的 dirty 字段，后台统计能反映缓存是否仍处于失效状态。
- [x] `content.form` notification status: 后台新增 `form/notification/status`，通知记录可标记已读/未读并记录 `readTime/readTimeText`，能力页通知视图提供行内操作。
- [x] `content.form` notification replay: 后台新增 `form/notification/replay`，通知记录可复制为新的 `form.notification.replay` 本地事件，返回 `sourceNotificationId/newNotificationId`，能力页通知视图提供行内重发。
- [x] `content.revision` diff summary metadata: diff 行新增 `changeKind/typeChanged/beforeLength/afterLength`，能力页字段列展示新增/删除/修改、类型变化和长度变化摘要。
- [x] `content.audit-log` / `content.sensitive` cleanup write guard: cleanup endpoints now verify the bounded DELETE statement actually completes before returning success, and audit detail records `deleted/beforeTime/limit` for traceable cleanup acceptance.
- [x] `content.audit-log` core write result guard: core audit writes now return success/failure and verify insert SQL instead of silently ignoring `sqlite3_step` errors; disabled audit-log packs remain a no-op.
- [x] `content.audit-log` 只读详情视图：后台新增 `audit-log/detail` 接口和能力页“详情”操作，单条日志可查看基础字段、原始 `detail_json` 与解析后的 detail 对象，不引入写入副作用。
- [x] `content.slug` 物理列兼容迁移：启用 slug 能力包时创建 `content_item.slug_value` 与索引，新保存、导入和 repair confirm 同步物理列，冲突检测与公开 `slug/resolve` 优先使用索引列，旧 payload 数据保留有界回退扫描。
- [x] Base content write guard: core content save/delete now verify the `content_item` insert/update/delete matched before running slug history, revision snapshot, audit, media, SEO, search, sitemap, related or static side effects.
- [x] Insert id guard gate: `tools/check_content_system.ps1` now scans `managed_main.c.tpl` and fails any `sqlite3_last_insert_rowid()` use that is not guarded by a successful `sqlite3_step(stmt) == SQLITE_DONE` path or equivalent task insert flag.
- [x] `content.import-export` admin request boundary config: import preview/commit/replay and JSON export now reject oversized admin request bodies by `maxRequestBytes` before JSON parsing, in addition to the existing per-batch row and export row limits.
- [x] `content.import-export` job acceptance observability: preview/import/export responses now return `jobId/jobSaved` and warning text when the job row cannot be recorded, while audit logs use the real job id instead of reading `last_insert_rowid()` after an unchecked insert.
- [x] `content.import-export` import transaction guard: confirm import now verifies transaction begin/commit and rolls back on commit failure before saving the import job, audit log, or success response.
- [x] `content.seo` admin request boundary config: SEO meta save/delete now reject oversized admin request bodies by `maxRequestBytes` before JSON parsing, while list output remains bounded by `maxListRows`.
- [x] `content.revision` restore request boundary config: revision restore now rejects oversized admin request bodies by `maxRequestBytes` before JSON parsing, while revision list retention remains bounded by `maxListRows/maxSnapshotsPerContent`.
- [x] `content.revision` restore derived-data sync: restoring a revision now refreshes media refs, SEO metadata, search index, sitemap rows, related-content rows and static output through the same bounded derived-data path used by normal content writes; the content-system check locks these calls inside the restore handler.
- [x] `content.revision` restore update-row guard: revision restore now verifies that the target content row was actually updated under `delete_time=0` before writing snapshots, audit logs, derived data or static output, avoiding side effects when a revision points to deleted content.
- [x] `content.revision` prune write guard: revision snapshot pruning now verifies the retention DELETE statement; snapshot creation reports failure if the post-insert retention cleanup cannot complete.
- [x] Base content derived sync guard: content save, import and revision restore now use `Managed_ContentSyncDerivedData`; media-ref/SEO/search/sitemap/related SQL sync helpers return explicit success, and write endpoints report `content derived sync failed` instead of silently accepting broken derived tables.
- [x] Base content side-effect guard: slug history, slug-generated redirect and revision snapshot helpers now return explicit success; content save/import/restore and slug repair stop or roll back on these optional side-effect failures instead of reporting clean success.
- [x] Base template SQLite write-result gate: `tools/check_content_system.ps1` now fails if `managed_main.c.tpl` contains unchecked `sqlite3_step(...)` statement calls, forcing write paths to inspect the step result or explicitly mark optional best-effort side effects.
- [x] `content.tag` / `content.topic` admin request boundary config: tag and topic write endpoints now expose `maxRequestBytes` and reject oversized save/delete/bind/merge/sort request bodies before JSON parsing, keeping batch relation writes on a predictable request-size budget.
- [x] `content.tag` / `content.topic` delete side-effect guard: tag/topic delete now clears relation rows and writes audit logs only after the primary soft-delete actually matches a live row.
- [x] `content.tag` / `content.topic` delete transaction guard: tag/topic delete now wraps primary soft-delete and relation cleanup in one immediate transaction, rolling back if relation cleanup or commit fails.
- [x] `content.tag` / `content.topic` admin aggregate stats: 后台新增只读 `tag/stats` 与 `topic/stats`，返回状态分布、关系总量和有界 Top 关联项；能力页提供统计按钮，统计查询沿用 `maxAdminLinkRows` 上限，不进入公开访问热路径。
- [x] `content.category` 后台树表展开/收起：栏目管理页新增本地展开全部、收起全部和行内展开/收起按钮，只作用于已按 `maxAdminTreeRows` 拉取的有界树结果，不新增后台查询和公开访问成本；契约新增 `treeUi`。
- [x] URL 规则快照统计和风险筛选：后台新增只读 `route-rule/stats`，在管理期同步 `content_route_rule` 后返回总量、启用/停用、warning 数、按能力包和规则类型分布；`route-rule/list` 新增 `warningOnly` 固定条件筛选，能力页提供“URL 规则统计”和风险筛选输入，不改变静态优先、动态兜底的请求访问链路。
- [x] `content.media` admin request and batch boundary config: media save/delete/batch now reject oversized request bodies by `maxRequestBytes` before JSON parsing, and batch enable/disable/delete rejects id arrays beyond `maxBatchRows`.
- [x] `content.media` save write guard: media save now verifies insert/update success before writing audit logs or returning success, and returns explicit `media save failed/media not found` errors for stale admin requests.
- [x] `content.media` delete side-effect guard: single media delete now writes audit records and returns success only after the soft-delete matches a live media row, matching the existing batch per-row result semantics.
- [x] `content.media` batch action guard and audit detail: media batch now rejects unsupported actions before opening the database or iterating ids, and audit detail records action/total/success/failed for bounded batch acceptance.
- [x] `content.workflow` admin request boundary config: workflow action and scheduled publish now reject oversized admin request bodies by `maxRequestBytes` before JSON parsing, while reason text and due-run batch size keep their existing bounded configs.
- [x] `content.workflow` derived-data and update-row guard: workflow status changes now refresh media refs with the rest of the derived data and verify the target row was actually updated before logs, audit records and derived sync are written.
- [x] `content.workflow` scheduled publish update-row guard: due-run publishing now only appends workflow logs, audit records and derived sync after the draft row is actually updated, keeping stale task scans side-effect free.
- [x] `content.workflow` scheduled transaction guard: scheduled publish runner now verifies `BEGIN IMMEDIATE` and `COMMIT` success, rolls back on commit failure, and returns explicit transaction/commit errors instead of reporting false success.
- [x] `content.workflow` log and notification observability: workflow action responses now return `logSaved` and `notificationSaved`, while workflow log/notification helpers verify insert SQL instead of silently ignoring local tracking write failures.
- [x] Base content derived delete guard: content delete now requires SEO meta delete, search index delete, sitemap entry disable and static artifact cleanup helpers to complete before returning success, avoiding false-success responses when optional derived cleanup SQL fails.
- [x] `content.like` admin status request boundary config: admin like status changes now reuse `maxRequestBytes` and reject oversized bodies before JSON parsing, matching the public create/cancel request budget.
- [x] `content.category` admin request and sort boundary config: category save/delete/sort now reject oversized request bodies by `maxRequestBytes` before JSON parsing, and drag-sort item arrays are capped by `maxSortRows`.
- [x] `content.category` descendant path observability: category save/sort now verify descendant path rewrite SQL, returning `descendantPathsUpdated` or `descendantPathFailures` so tree path drift is visible after parent/path changes.
- [x] `content.category` public contents access integration: category public content lists now filter restricted content through `content.access` when that pack is enabled, avoiding title/list leakage while preserving the no-access-pack fast path.
- [x] 能力包挂载配置请求边界：contracts admin POST 保存自定义挂载配置前增加固定请求体上限，避免能力包系统级 JSON 写入口无界解析。
- [x] 基础内容写入请求边界：内容保存和删除接口在 JSON 解析前增加固定请求体上限，避免基础写入入口把超大 payload 带入后续能力包同步链路。
- [x] `content.tag` / `content.topic` public contents access integration: tag/topic public content lists now filter restricted content through `content.access` when that pack is enabled, matching category list behavior.
- [x] `content.related` / `content.sitemap` access integration: related public output now filters restricted targets, and sitemap sync/refresh removes restricted content entries when `content.access` is enabled.
- [x] `content.search` public result count access integration: public search already filtered restricted rows; the returned `count` now also switches to the filtered result count when `content.access` is enabled.
- [x] `content.media` public access integration: public media list/detail now filter media whose content references are all restricted by `content.access`, while unreferenced public media remains available; reference checks are bounded by `maxPublicRefCheckRows`.
- [x] `content.slug` / `content.seo` / `content.sitemap` public access integration: slug.resolve and seo.meta now hide restricted content through `content.access`; public sitemap/RSS bypass stale file cache when access is mounted and filter fallback DB rows before output.
- [x] `content.category` public count access integration: public category list/detail now count only public-status content and mask aggregate `contentCount` when `content.access` is mounted, avoiding restricted-content quantity leakage without adding per-request count fan-out to the route hot path.
- [x] `content.tag` / `content.topic` public count access integration: public tag/topic list/detail now mask aggregate `contentCount` when `content.access` is mounted, matching the filtered contents endpoints and avoiding cached count leakage.
- [x] `content.comment` / `content.like` / `content.view-stat` / `content.form` public access integration: public comment list/count/create/hide, like status/create/cancel, view record/status/rank, and content-bound form submit now require readable content when `content.access` is enabled.
- [x] 后台日志/列表默认上限配置化：评论审计/通知列表默认使用 `maxAdminListRows`，访问日志列表默认使用 `content.view-stat.maxAdminListRows`，敏感词命中日志新增 `maxLogListRows`，slug repair 和 redirect list 默认使用各自能力包上限，并在响应中返回实际 limit。
- [x] 内容模板安全回归门禁：`tools/check_content_system.ps1` 新增请求函数级静态扫描，锁定 JSON body 解析必须有 `xsReqBodyLen` 边界、公开内容相关接口必须保留 access/mask 标记、绑定 `LIMIT ?` 的列表/批处理请求必须有查询或能力包配置上限来源。
- [x] 能力包 HTTP smoke 验收收紧：`tools/smoke_capability_acceptance.ps1` 会同时探测 `acceptanceApiPath` 和 `acceptanceViewPath`，只接受 `2xx/3xx/401/403`，避免 404 路由缺失被误判为通过；对 2xx API 响应继续校验 JSON 形态，对 2xx 页面响应继续校验 xAdmin HTML/layui/managed 页面形态，避免空响应或错误内容误判为通过。
- [x] 基础后台入口与访问统计解耦：生成插件的核心后台页面 `/admin/view/plugin/{pluginXid}`、文章、草稿和编辑器入口不再依赖 `content.view-stat`，无能力包或未启用访问统计时仍能进入基础内容管理；检查脚本新增静态/动态路由能力边界扫描。
- [x] 栏目迁移边界修复：`content_category.description/cover_url/template_key` 兼容迁移已收进 `content.category` 启用条件，避免未启用栏目能力包时基础插件启动阶段触碰栏目表；检查脚本锁定该边界。
- [x] 栏目 schema 能力包化：`content_category` 主表和索引迁入 `content.category/schema.sql`，基础 `G_SchemaSql` 不再创建栏目表；`content.access` 后台规则列表按栏目能力包启用状态选择是否联表栏目，栏目规则保存时未启用栏目能力包会直接拒绝。
- [x] 基础 schema 通用边界门禁：检查脚本会读取所有能力包 `contracts.tables`，禁止基础 `G_SchemaSql` 创建任意能力包声明表，后续新增能力包 schema 必须落在所属 `schema.sql`。
- [x] 可选生成页面边界门禁：检查脚本锁定 `generated/categories.html` 只随 `content.category` 生成，`generated/dashboard.html` 只随 `content.like/content.view-stat` 生成，并确认输出文件清单保留对应条件分支。
- [x] enabled 状态生成边界门禁：检查脚本锁定能力包 schema、权限、菜单、路由、build.defines、声明文件复制、runtime manifest 和 contracts 都必须读取并尊重 `enabled=false`。
- [x] 能力包 schema 静态结构门禁：`tools/check_capability_packs.ps1` 新增 `Test-SchemaFile`，逐包检查 `schema.sql` 非空、分号和括号完整、只使用预期的 `CREATE TABLE/INDEX IF NOT EXISTS` 语句、`contracts.tables` 与实际建表一致，并禁止跨能力包重复索引名。
- [x] 能力包 warning 归零门禁：`tools/check_capability_packs.ps1` 默认 `FailOnWarning=true`，能力包 manifest/contract/schema/advisor 检查只要出现 warning 就返回失败，避免 CI 对 `WarningCount>0` 的漂移放行。
- [x] 能力包命名显示边界门禁：`tools/check_capability_packs.ps1` 已锁定 `packId` 必须是英文小写点分段 ID，显示标题 `title` 必须包含中文文本，避免能力包管理页回退到英文显示名。
- [x] 能力包管理页弹窗与操作按钮边界：能力包管理页的新增/编辑弹窗统一使用底部 `x-dialog-actions` 操作区，并通过顶部分隔线与表单内容隔开；SEO 元信息弹窗已修正为中文“取消/保存”和中文弹窗标题，表格行操作里的对比、恢复、已处理、下载也已中文化，检查脚本锁定关键弹窗操作区并拒绝这些高频按钮回退为英文。
- [x] 能力包契约命名唯一性门禁：`tools/check_capability_packs.ps1` 已检查跨包 `contracts.permissions`、`contracts.adminPages`、`contracts.tables` 不得重复，避免后续新增能力包时权限项、后台页或表名撞车。
- [x] 真实生成物 smoke 子集选择：`tools/smoke_capability_acceptance.ps1` 新增 `-ManifestPath` 和 `-RuntimeDir`，可读取生成插件 `runtime/capability.manifest.json` 的 `abilityPacks`，只验收 `enabled-packs-only` 运行清单中的能力包，且会拒绝空 `packId` 和重复 `packId`，避免未启用能力包的 404 被错误纳入真实烟测；传入 `-ManifestPath` 或 `-RuntimeDir` 时还会自动校验同目录 `runtime/managed.json.capabilitySlots` 与 `runtime/contracts.json.abilityPacks/capabilities` 的 enabled-only 能力包列表一致，也可用 `-ManagedPath`、`-ContractsPath` 显式指定路径；`-ValidateManifestOnly` 可在未启动 HTTP 服务时只做运行时三清单一致性校验，且已接入 `tools/check_content_system.ps1` 的正向/负向临时清单测试。
- [x] smoke 手动能力包参数边界：`tools/smoke_capability_acceptance.ps1` 对手动传入的 `-PackId` 也会执行归一化校验，拒绝空值、重复值和非英文点分段 ID，避免验收输入本身造成漏测或重复测。
- [x] smoke 验收路径边界：能力包 `acceptanceApiPath/acceptanceViewPath` 已被检查脚本和 smoke 脚本共同锁定为站内相对路径，必须包含 `{pluginXid}`，且不得指向外部 URL，避免验收链路被错误拼接或漂移到系统外。
- [x] 表单能力包低成本验收路径：`content.form` 的 smoke API 已从导出接口 `/form/submission/export` 调整为有界列表接口 `/form/submission/list`，避免真实 HTTP smoke 把导出语义当成最小验收路径；检查脚本锁定该边界。
- [x] 跳转能力包无副作用验收路径：`content.redirect` 的 smoke API 已从公开解析 `/redirect/resolve` 调整为后台有界列表 `/redirect/list`，避免验收请求触发 `hit_count/last_hit_time` 写入；检查脚本锁定 acceptance 不得使用 resolve 路径。
- [x] 版本能力包低成本验收路径：`content.revision` 的 smoke API 已从字段级对比 `/revision/diff?id=1` 调整为有界列表 `/revision/list`，避免最小验收触发不必要的 diff 计算；检查脚本锁定 acceptance 不得使用 diff 路径。
- [x] 能力包 smoke API 低成本通用门禁：`tools/check_capability_packs.ps1` 已禁止 `acceptanceApiPath` 指向明显写入、删除、清理、刷新、重建、恢复、批量、导出等操作路径，并额外禁止 revision diff 与 redirect resolve 作为 smoke API。
- [x] 能力包 smoke API 契约归属门禁：`tools/check_capability_packs.ps1` 会把 `acceptanceApiPath` 解析为 `adminApis/publicApis` key，并要求它出现在对应 `contracts.json` 契约声明中；仅 `pack/list` 这类宿主通用能力包列表接口允许例外。
- [x] 能力包 API 契约命名门禁：`contracts.publicApis/adminApis` 已统一要求英文小写点分段 API key，禁止再混入斜杠路径；检查脚本已锁定该格式，`content.import-export` 契约已同步修正。
- [x] 能力包 smoke 路由注册门禁：`tools/check_capability_packs.ps1` 会把 `acceptanceApiPath` 转成生成模板中的 `route.path = "...{{PLUGIN_XID}}..."` 形态，并确认 `managed_main.c.tpl` 已注册对应路由，避免 manifest/contract 有路径但生成物无路由；已同步修正 `content.like` 为后台计数列表、`content.tag/content.topic` 为公开列表，消除验收路径与实际注册路由漂移。
- [x] 能力包 smoke 页面路由门禁：`tools/check_capability_packs.ps1` 会拒绝旧 `/ability/...` 页面验收路径，要求能力包管理页使用生成器真实注册的 `/admin/view/plugin/{pluginXid}/pack/<safeKey>` 路由，并校验 `managed_ability.html.tpl` 的 `safeToPack` 映射；`content.seo` 已纳入独立能力页验收，栏目能力包继续使用独立栏目页。
- [x] 能力包 HTTP smoke 结果汇总：`tools/smoke_capability_acceptance.ps1` 输出 `Summary`，分别统计 API/页面的 `2xx/3xx/401/403` 数量和页面探测数量，方便生成后批量验收时区分内容通过、跳转、鉴权拦截和未探测页面。
- [x] 生成物 runtime smoke 包装入口：新增 `tools/smoke_generated_runtime.ps1`，先校验 runtime 三清单，再探测基础内容插件核心后台页、后台列表 API 和公开列表 API，最后复用能力包 smoke 按 enabled-only 清单验收能力包；支持 `ValidateManifestOnly` 和 `SkipContentCheck`，为后续接入自动生成/启动闭环预留固定入口。
- [x] 动态路由候选集热路径收紧：`MatchDynamicRouteHTTP` 已从“正则组判定有命中后再遍历全部动态路由”改为按 `xrtRegexSetMatches` 返回的候选下标执行方法过滤和 capture 校验；候选下标会先按 pattern index 排序以保留动态路由 priority 顺序，避免动态路由数量增长后重新退化为全量扫描。
- [x] 动态路由权限编辑同步修复：后台权限管理保存 URI 配置时，内存路由表更新已从只查静态路由改为静态路由优先、动态路由兜底，并修正静态路由查找长度，避免动态路由权限编辑后必须重载才生效。
- [x] 静态化任务状态契约补齐：`content.static` 已把已注册的 `/static/task/status` 后台路由补入 `contracts.adminApis` 的 `static.task.status`，并由内容系统检查锁定，避免任务队列接口存在但能力包契约遗漏。
- [x] 能力包路由契约覆盖门禁：`tools/check_content_system.ps1` 新增生成模板路由到 `contracts.adminApis/publicApis` 的覆盖扫描，基础核心路由外的能力包 API 必须在能力包契约中声明；本轮补齐 `content.access`、`content.comment`、`content.sensitive`、`content.category`、`content.static`、`content.tag`、`content.topic`、`content.view-stat` 的真实路由键；`content.search` 的真实 `/search` 路由按既有点分段契约键 `search.query` 归一化。
- [x] 后台入口动态路由冲突检查：自定义后台入口保存时，精确 URI 冲突检查已从只看静态路由扩展为静态路由优先、动态路由兜底，避免动态路由 URI 资源被后台入口覆盖。
- [x] 路由热路径顺序门禁：`tools/check_content_system.ps1` 已锁定请求分发顺序为静态路由优先、动态路由次之、插件静态文件兜底，并检查动态路由继续使用 xrt 正则对象和 xrt 正则集合预编译匹配。
- [x] 前台会员 URI 动态路由门禁：`tools/check_content_system.ps1` 已锁定 `MemberAuth_LoadURIS` 对前台 URI 权限配置同样采用静态路由优先、动态路由兜底同步。
- [x] 动态路由重编译失败保护：`DynamicRoute_RebuildHTTP` 已改为先构建新的 xrt 正则集合，成功后再替换 `G_DynamicRouteTableHTTP.pRegexSet`，失败时恢复旧集合和旧编译计数；删除动态路由时也改为重编译成功后再释放被删除路由，失败则恢复列表；检查脚本已锁定该 copy-on-success 语义。
- [x] xrt 正则 API 边界门禁：设计文档已从直接 bbre API 口径修正为 xrt 正则 API 口径；`tools/check_content_system.ps1` 会拒绝动态路由实现中直接出现 bbre API 调用。
- [x] xrt 正则 API 全脚本边界门禁：`tools/check_content_system.ps1` 进一步扫描 `hosts/xadmin/script` 下 `.h/.c`，拒绝任何直接 bbre API 调用，避免非路由模块绕过 xrt 正则封装。
- [x] 动态路由候选匹配容量门禁：`MatchDynamicRouteHTTP` 的候选数组已从硬编码 `32` 改为 `XADMIN_DYNAMIC_ROUTE_MAX_MATCHES=256`，保持请求热路径有界，同时避免一组动态规则较多时过早截断候选；动态路由重编译后如果编译规则数超过候选上限，会在提交/编译期追加 `dynamic route candidate limit warning`，`/admin/trace` 同步输出 `dynamicCandidateLimit/dynamicCandidateOverLimit` 便于后台提示；检查脚本已锁定候选上限宏、xrt 正则集合匹配调用、容量 warning 和动态路由参数上下文的 begin/end 包裹。
- [x] 能力包默认权限契约门禁：`tools/check_capability_packs.ps1` 已从只检查 `Content_DefaultAbilityPermission` 映射存在，升级为校验映射返回的默认权限必须出现在对应能力包 `contracts.permissions` 中，避免生成器注册权限、菜单鉴权和能力包契约长期漂移。
- [x] 能力包权限与页面契约命名门禁：`tools/check_capability_packs.ps1` 已锁定 `contracts.permissions` 必须使用英文小写点分段权限键，`contracts.adminPages` 必须使用英文小写页面键，避免后续把 URI、中文标题或大小写混入能力包契约。
- [x] 能力包实例配置双向契约门禁：`tools/check_capability_packs.ps1` 已要求 `runtime.configKeys` 与 `instance.xform.json` 字段双向一致，避免运行时读取的配置没有表单入口，或表单暴露了运行时契约未声明的配置项。
- [x] 能力包实例配置中文化门禁：当前 21 个能力包 `instance.xform.json` 的字段和选项 label 已统一中文化；`tools/check_capability_packs.ps1` 会拒绝不含中文或以英文开头的配置 label，确保后台能力包表单显示名称不再回退为英文键名。
- [x] 能力包标题中文化门禁：`content.seo` 标题已从英文术语开头调整为 `搜索引擎优化 SEO`；`tools/check_capability_packs.ps1` 会拒绝以英文开头的能力包显示标题。
- [x] 能力页重点中文化：`content.seo`、`content.access`、`content.category`、`content.slug`、`content.redirect`、`content.audit-log`、`content.media`、`content.revision`、`content.workflow`、`content.search`、`content.sitemap`、`content.related`、`content.form` 与 `content.import-export` 的能力管理页表格列、工具栏、弹窗字段和提示文本已改为中文显示，保留英文 ID/模式值作为配置值而非展示主文案。
- [x] 能力页可见英文回归门禁：`tools/check_content_system.ps1` 已加入 `managed ability visible text localization`，锁定能力页和编辑页不得重新出现本轮清理掉的旧英文列名、按钮、提示和结果面板文案。
- [x] CMS 文本 mojibake 门禁：`tools/check_content_system.ps1` 已加入 `cms text mojibake scan`，扫描 SPEC 与主要内容系统模板，避免中文化过程中混入常见 Windows/UTF-8 乱码片段。
- [x] 能力包配置文本 mojibake 门禁：`tools/check_capability_packs.ps1` 已扫描 `pack.json`、`contracts.json`、`effects.json`、`advisor.json`、`instance.xform.json` 与 `schema.sql` 中的常见乱码字符，并修复 `content.search.maxRebuildRows` 配置项的乱码标签。
- [x] 能力包 API 契约命名唯一性门禁：`tools/check_capability_packs.ps1` 已检查 `contracts.publicApis` 与 `contracts.adminApis` 在各自作用域内不得跨能力包重复，避免后续新增能力包时出现同名 API key 归属不清。
- [x] 能力包实例配置键唯一性门禁：`tools/check_capability_packs.ps1` 已检查 `instance.xform.json` 字段名不得重复，`contracts.runtime.configKeys` 不得重复，且二者必须一一对应，避免生成期配置覆盖或运行期读取歧义。
- [x] 能力包实例配置分组标题本地化门禁：`tools/check_capability_packs.ps1` 已检查 `instance.xform.json` 分组标题必须包含中文且不得以英文开头，并修复 `content.seo` 的 `seo-admin` 分组标题。
- [x] 能力包 manifest 显示文本本地化门禁：`tools/check_capability_packs.ps1` 已将 `pack.json.title/description/advisorMessage` 升级为硬检查，要求显示文本包含中文且不得以英文开头，保持英文只作为 `packId`、权限键和 API key。
- [x] 动态路由启动与权限同步门禁：`tools/check_content_system.ps1` 已锁定 `RouteHTTP_Init` 会初始化动态路由参数上下文字典并激活共享 owner，同时锁定后台 URI 启动同步会遍历动态路由，避免动态路由命中后无法读取 capture 参数或重启后权限配置不同步。
- [x] xrt 正则命中语义门禁：动态路由匹配和表单 pattern 校验已按 `xrtRegexCaptures` 命中返回 `1` 处理；`XAdmin_RouteParam` 只暴露用户捕获组，capture 0 的整段匹配不再作为参数返回；`tools/check_content_system.ps1` 已锁定该语义并拒绝旧的 `== 0`/`!= 0` 判断回退。
- [x] 动态路由 capture 数量边界门禁：动态路由请求上下文已用 `XADMIN_DYNAMIC_ROUTE_MAX_CAPTURES` 固定捕获数组上限，注册期发现正则捕获组超过可暴露参数上限时只记录 warning，不在请求热路径追加动态检查；检查脚本已锁定该边界。
- [x] `content.sensitive` 命中替换语义修复：敏感词扫描会把 `matchMode` 写入命中记录，替换阶段通过 `Managed_TextReplaceWordMode` 复用同一模式；`asciiWord` 下只替换真正命中的 ASCII 词边界片段，避免检测阶段未命中的单词内部被普通全局替换误改。
- [x] 动态路由后台/API 前缀风险提示：`DynamicRoute_RecordPatternRisk` 已在注册期识别可能覆盖 `/admin` 或 `/api` 前缀的正则模式并记录 warning，继续保持请求热路径只做预编译集合匹配，不增加运行时排除判断。
- [x] `content.form` pattern 后端整值匹配：表单 schema 字段 `pattern` 校验已从“任意位置命中即可通过”收紧为 `span[0]` 覆盖整个输入值，保持后端校验与前端 pattern 语义一致；检查脚本已锁定该行为。
- [x] `content.search` 空结果语义修复：非空查询没有命中时不再回退普通内容列表，而是返回 `count=0` 的搜索结果；只有空查询继续走基础列表回退，避免前台/后台搜索误把无结果展示为全量内容。
- [x] `content.search` 多词摘要定位补强：搜索摘要构建在整串查询无法定位时，会回退到首个实际查询词的位置生成 240 字节摘要，避免多词查询命中正文却展示尾部无关片段；检查脚本已锁定该辅助函数。
- [x] `content.search` 标题前缀相关性补强：搜索排序在标题精确命中和普通标题包含命中之间增加 `prefixTitleWeight`，短查询更倾向返回标题以查询词开头的内容；该权重已纳入实例配置、契约和检查脚本。
- [x] `content.search` 查询词覆盖度加权：搜索结果在 SQL 基础分之外按归一化查询词在标题、关键词、摘要和正文中的覆盖数量追加 `termCoverageWeight` 有界加分，并在能力页排序解释中输出基础分与覆盖度分，避免引入请求期分词器或额外索引表。
- [x] `content.sensitive` 命中日志筛选：后台 `sensitive/log/list` 改为固定形态绑定 SQL，支持 `targetType/targetId/word/fieldName/action` 可选筛选并继续受 `maxLogListRows` 约束；能力页新增日志筛选栏，筛选只影响后台表格查询，不改变请求期按 scope 扫描词库的热路径。
- [x] `content.sensitive` 词库/命中统计：后台新增 `sensitive/stats` 只读聚合接口，能力页新增词库统计按钮，契约声明 `sensitive.stats`、`statsSummary` 和 `statsWindow`；统计窗口由 `statsRecentDays` 控制，接口只读取固定聚合 SQL。
- [x] slug/redirect/static 统一规则计划接口：新增后台只读 `route-rule/plan`，在任一相关能力包启用时返回 slug/redirect 动态前缀、动态正则、static 输出前缀和已持久化 `static_rule` 规则列表及提交期 warning；能力页优先展示接口结果，继续保持请求热路径只做静态优先和动态正则匹配。
- [x] `content.search` CJK 二元词覆盖加权：连续 CJK 查询会在排序阶段按相邻两个汉字组成的 bigram 计算标题、关键词、摘要和正文覆盖度，并通过 `cjkBigramWeight` 有界加分；该补强不引入外部分词器、不改变 SQL 查询形态，能力页排序解释输出 `cjkBigramScore`。
- [x] `content.import-export` 冲突策略提交期校验：导入提交会拒绝非 `insert/update/skip` 的 `conflictMode`，避免配置拼写错误静默退化为插入模式造成重复内容；检查脚本已锁定该边界。
- [x] `content.slug` / `content.redirect` 公开前缀配置：slug pretty URL 与 redirect pretty URL 已从固定 `/{pluginXid}` / `/{pluginXid}/r` 升级为实例配置项，后端注册动态路由时对前缀做正则字面量转义，前台公开页按 `slugRoutePrefix` 解析路径，继续保持请求热路径只走静态路由 miss 后的预编译动态路由集合。
- [x] `content.slug` / `content.redirect` 联合前缀检查：能力页新增 slug/redirect 联合检查，展示静态优先、动态次之的热路径策略、两个动态正则模式、示例 URL、匹配优先级和前缀嵌套/重叠 warning；该检查只在管理页执行，不增加请求期排除判断。
- [x] `content.slug` / `content.redirect` / `content.static` 统一规则计划：能力页联合检查已扩展为统一规则计划，展示 slug 动态规则、redirect 动态规则、static 产物规则来源、能力包挂载状态、样例路径、匹配顺序和提交期检查说明；该计划只读 contracts/实例配置，不增加请求热路径判断。
- [x] `content.category` 基础边界收紧：基础列表筛选只有在栏目能力包启用时才接受 `categoryId`，内容保存和导入时如果未启用 `content.category` 会强制将栏目归零，JSON 导出也不再写出 `categoryId`，避免无栏目能力包的基础内容插件暴露栏目业务语义；检查脚本已锁定该边界。
- [x] `content.import-export` 失败行回放边界：`import/replay` 返回失败行时会再次按 `maxBatchRows` 封顶，并返回 `truncated/maxBatchRows`，避免历史异常 report 或旧任务一次性放大响应；契约和检查脚本已补充 `replayLimit`。
- [x] `content.import-export` 导出字段计划：新增后台 `import-export/field-plan` 轻量接口，根据内容规格和字段白名单返回 `availableFields/requestedFields/exportedFields/ignoredFields`，不读取内容表、不创建导出任务；JSON 导出响应同步返回 `fieldPlan`，能力页提供“字段计划”按钮，便于提交前确认字段边界和未知字段。
- [x] `content.static` 任务状态一致性：静态页渲染成功但 `static_artifact` 删除/写入失败时，会把已创建的 `static_task` 回写为 `status=-1` 和 `artifact save failed`，避免任务列表显示 generated 但请求实际失败。
- [x] `content.static` 任务重试：新增后台 `static/task/retry`，按历史任务的 `rule_id/target_type/target_id` 重新创建静态生成任务，并在能力页任务表提供“重试”操作；接口继续使用 `maxRequestBytes` 边界，不引入请求热路径或常驻后台队列。
- [x] `content.revision` 类型化 diff 元数据：版本差异行新增 `fieldType/beforeKind/afterKind/beforeValue/afterValue`，前端优先使用原始值渲染结构化对象、数组和媒体 URL，同时保留原 `before/after` 字符串字段兼容既有消费方。
- [x] `content.revision` enum diff label：版本差异行会为 select/radio/checkbox/checklist/combobox 字段解析模型字段 `list/options`，输出 `beforeLabel/afterLabel`，后台能力页和编辑器版本弹窗同步展示。
- [x] `content.revision` category relation diff label：版本差异行会在 `content.category` 启用时为内置 `categoryId` 查询栏目标题和 path，输出 `diffMode=relation` 与 `beforeLabel/afterLabel`，未启用栏目包时不触碰栏目表。
- [x] `content.revision` custom content relation diff label：自定义字段使用 relation/reference/content 组件、`relationTarget=content` 或 `*ContentId` 命名时，版本差异行会在后台 diff 中解析 `content_item.title`，输出 `relationKind=content` 与 `beforeLabel/afterLabel`，只发生在管理期版本对比读取路径。
- [x] `content.category` 树行可观测字段：栏目列表、树接口和公开详情行新增 `childCount`，继续保留 `contentCount`，后台栏目表格直接显示子栏目数，便于删除保护和树密度判断；契约新增 `treeObservability`，该能力仍只在栏目包启用时注册/执行。
- [x] `content.category` 删除保护计数返回：栏目删除被子栏目或内容引用阻止时，响应会返回 `childCount/contentCount`，后台和 HTTP smoke 可以直接判断阻止原因；契约新增 `deleteProtectionCounts`。
- [x] `content.static` 任务状态筛选：后台 `static/task/list` 支持可选 `status=-1/0/1` 查询并继续受 `maxListRows` 约束，能力页任务视图增加状态筛选和清空按钮，便于定位失败任务；契约新增 `taskStatusFilter`。
- [x] `content.sitemap` dirty 诊断细化：后台 `sitemap/stats` 除 `cacheDirty` 外会读取 `sitemap/dirty.json` 中的 `reason/contentId/updateTime`，返回 `dirtyReason/dirtyContentId/dirtyUpdateTimeText`，能力页统计表同步展示；契约新增 `dirtyMetadataDetail`。
- [x] `content.form` 提交记录筛选：后台 `form/submission/list` 支持可选 `formId/contentId/status` 筛选，SQL 使用绑定参数并继续受 `maxListRows` 约束；能力页提交记录视图增加筛选和清空按钮，契约新增 `submissionFilters`。
- [x] `content.import-export` 任务状态筛选：后台导入任务列表和导出任务列表支持可选 `status` 查询，SQL 使用固定形态和绑定参数并继续受 `maxJobRows` 约束；能力页导入/导出任务视图增加状态筛选和清空按钮，契约新增 `jobStatusFilter`。
- [x] `content.search` 新鲜度加权：搜索结果在原字段命中和查询词覆盖度基础上，新增 `freshnessWeight/freshnessWindowDays` 配置，按内容 `update_time` 在固定窗口内给有界加分；响应和能力页排序解释同步返回 `freshnessScore`，便于调试排序原因。
- [x] `content.comment` 后台排障筛选：审核日志列表支持可选 `commentId/action` 筛选，通知记录列表支持可选 `commentId/contentId/status/event` 筛选，SQL 均使用固定形态和绑定参数，并继续受 `maxAdminListRows` 约束；能力页同步增加筛选和清空按钮，契约新增 `adminAuditFilters`。
- [x] `content.comment` 待审队列定位筛选：后台 `comment/moderation-queue` 在 contentId/authorName 外新增 body 子串和 ip 子串筛选，SQL 使用固定形态和绑定参数，能力页同步增加待审正文/IP 筛选输入，契约新增 `adminModerationQueueFilters`。
- [x] `content.slug` / `content.redirect` / `content.static` 跨能力包规则批量验证：新增后台只读 `route-rule/validate`，在任一相关能力包启用时按有界 `limit/maxListRows` 汇总 slug/redirect 公开前缀风险、二者前缀冲突和 `static_rule.path_pattern` warning；能力页新增批量验证入口，验证仅在管理期执行，不改变请求热路径的静态优先、动态正则次之策略。
- [x] `content.slug` / `content.redirect` / `content.static` 统一规则快照持久化：当任一相关能力包启用时，生成插件会创建 `content_route_rule` 表；`route-rule/plan` 与 `route-rule/validate` 会在管理期把 slug/redirect 动态前缀和 static 规则同步为独立规则快照，响应返回 `independentRouteRuleStore/syncedRouteRules/persistedRouteRules`，便于后续统一 URL 规则管理页直接读取。
- [x] 统一 URL 规则只读管理页：新增后台 `route-rule/list`，按管理期同步后的 `content_route_rule` 快照返回 slug/redirect/static 规则表格数据；`content.redirect` 能力页新增 `URL 规则` 表格视图展示能力包、规则类型、规则键、匹配规则、来源、状态、warning 和更新时间，继续保持请求热路径不做额外冲突扫描。
- [x] `content.category` 栏目绑定表过渡迁移：`content.category/schema.sql` 新增 `content_category_bind`，内容保存和删除在栏目能力包启用时同步绑定表；栏目列表/详情计数、公开栏目内容列表和栏目删除保护已优先使用绑定表，`content_item.category_id` 暂作为兼容镜像保留，避免一次性硬删破坏旧数据和现有导入导出链路。
- [x] `content.related` 同栏目规则绑定表迁移：同栏目候选、保存期反向同步、发布期刷新和全量重建已改为读取 `content_category_bind`；单内容预览/重建仍会在绑定表缺失时读取 legacy `content_item.category_id`，用于兼容旧数据，避免栏目能力包未启用时访问不存在的绑定表。
- [x] 生成物 live HTTP smoke 包装：新增 `tools/smoke_generated_runtime_live.ps1`，可在服务未启动时隐藏启动 `xs.exe`，等待 `BaseUrl` 可访问后调用既有 `smoke_generated_runtime.ps1`，并可按参数停止本次自启动进程；检查脚本同步做 PowerShell 语法和关键流程标记校验。
- [x] live smoke 接入真实生成入口：`smoke_generated_runtime_live.ps1` 新增 `GenerateXid`，可先请求 `/admin/content/generate?xid=...`，从响应 `data.pluginXid` 定位 `hosts/xadmin/plugin/{pluginXid}/runtime`，再执行 runtime smoke；如果生成接口仍受登录拦截会直接失败，避免把未生成的旧 runtime 误判为通过。
- [x] `content.category` 旧栏目字段幂等回填：启用栏目能力包时，`Managed_EnsureSchema` 会执行 `Managed_CategoryBindBackfillLegacy`，把仍存放在 `content_item.category_id` 的旧内容关系补入 `content_category_bind`，并用 `NOT EXISTS` 避免重复绑定，为后续彻底移除 legacy 字段提供迁移路径。
- [x] 固定生成物 live smoke 验收模型：新增 `tools/fixtures/content_smoke_model.json`，覆盖基础字段和 21 个能力包；新增 `tools/smoke_content_generation_live.ps1`，可保存该模型、调用真实生成接口、启用并重载生成插件、再执行 runtime HTTP smoke。检查脚本锁定 fixture、保存接口、生成接口、启用/重载和 smoke 串联标记。
- [x] live smoke 服务就绪检查收紧：生成物 smoke 不再只用站点根路径判断服务可用，而是分别探测 `/admin/plugin/list` 与 `/admin/content/types`；404 会被视为未连接到当前 xadmin 后台，避免误连旧服务或其他占用 80 端口的进程。
- [x] live smoke 后台入口参数化：`smoke_generated_runtime.ps1`、`smoke_generated_runtime_live.ps1` 与 `smoke_content_generation_live.ps1` 新增 `AdminBase`，默认 `/admin`，可传当前站点的自定义后台入口，避免后台入口改名后所有生成/启用/后台 API smoke 都误判 404。
- [x] live smoke 鉴权上下文传递：`smoke_capability_acceptance.ps1`、`smoke_generated_runtime.ps1`、`smoke_generated_runtime_live.ps1` 与 `smoke_content_generation_live.ps1` 已统一支持 `CookieHeader`，用于传入已有后台登录态；开启后台入口保护时，未登录访问真实后台 API 仍会按系统安全策略返回 404，不在 smoke 脚本中绕过鉴权。
- [x] `content.search` 完整短语相关性：搜索候选集在已有 SQL 命中后增加 `exactPhraseWeight/exactPhraseScore`，按归一化后的完整查询短语在标题、关键词、摘要、正文中的命中加分，标题短语命中双倍加权；该能力只在已取回候选上做固定成本内存评分，不改变索引表和 SQL 热路径形态。
- [x] CMS 能力包本地总入口：新增 `tools/check_cms_capability_workflow.ps1`，串联能力包契约检查、内容系统静态门禁、`git diff --check`，并在显式 `-RunLiveSmoke -CookieHeader` 时调用固定验收模型的真实生成/启用/HTTP smoke；无登录态时不会绕过后台保护，只报告 live smoke 跳过。
- [x] `content.category` 工作流派生同步迁移：`Managed_WorkflowLoadContent` 在栏目能力包启用时会优先读取 `content_category_bind`，旧 `content_item.category_id` 仅作为兼容回退，避免定时发布/审核发布后的 related、sitemap、search 等派生同步继续依赖遗留字段。
- [x] `content.search` 栏目绑定表集成：搜索结果行和搜索重建候选对象在栏目能力包启用时会调用 `Managed_CategoryBindApplyToItem`，优先用 `content_category_bind` 回填 `categoryId`；旧 `content_item.category_id` 继续作为兼容镜像，不新增搜索 SQL 形态。
- [x] `content.tag` / `content.topic` 栏目绑定表集成：标签内容列表和专题内容列表返回内容行时会优先用 `content_category_bind` 回填 `categoryId`，保证公开聚合接口与栏目能力包的绑定表口径一致；查询条件仍保持原有有界列表，不改变请求热路径 SQL 结构。
- [x] `content.sitemap` / `content.related` / `content.import-export` 栏目绑定表集成：sitemap 条目预览和手动刷新、公开相关推荐列表、JSON 导出内容行均会优先用 `content_category_bind` 回填 `categoryId`；这些路径均为有界列表或后台任务，旧 `content_item.category_id` 只作为兼容镜像。
- [x] `content.related` 规则配置诊断：后台 `related/stats` 新增只读 `ruleConfig`，返回 `ruleLimit/publishRefreshLimit/maxRebuildSources/categoryWeight/tagWeight/topicWeight/ruleFormula`，能力页统计入口以表格展示当前规则配置，不写入推荐关系。
- [x] `content.category` 详情与公开输出绑定表集成：内容详情、slug.resolve、sitemap.xml 和 RSS fallback 行在栏目能力包启用时会调用 `Managed_CategoryBindApplyToItem`，让公开详情和公开索引输出的 `categoryId` 与 `content_category_bind` 一致；这些检查只在既有单条或有界结果集上执行，不加入路由热路径。
- [x] CMS 能力包静态门禁 CI：新增 `.github/workflows/cms-capability.yml`，在 docs、能力包、内容模板、内容脚本、内容 JS 和 tools 变更时运行 `check_capability_packs.ps1`、`check_content_system.ps1` 和 `git diff --check`；真实受保护后台 live smoke 仍保留为显式本地门禁，需要 `CookieHeader`。
- [x] 受保护后台 live smoke 登录态获取：新增 `tools/get_admin_cookie.ps1`，按前端登录页相同规则计算 `username + "_xywhsoft_" + password` 的 SHA-256 客户端哈希，POST 到 `{AdminBase}/login` 后提取 `XSID`；`check_cms_capability_workflow.ps1 -RunLiveSmoke` 在未显式传 `CookieHeader` 时可用 `AdminUsername/AdminPassword/AdminPasswordHash` 自动获取登录态，仍不绕过登录重试、防护和权限体系。
- [x] `content.comment` 单条审核状态边界：后台 `comment/status` 现在只接受 pending(0)、approve(1)、reject(2)，与批量审核接口的状态边界保持一致，避免写入未知评论状态；契约新增 `adminStatusValidation`。
- [x] `content.static` 规则状态边界：后台 `static.rule.save` 保存时会把 `status` 归一化为 enabled(1) 或 disabled(0)，避免静态规则和统一 URL 规则快照出现未知状态；契约新增 `ruleStatusValidation`。
- [x] `content.access` 规则保存边界：后台 `access.rule.save` 现在只接受 public/login/level/group/password/paid/private 七种权限模式，并把 `status` 归一化为 enabled(1) 或 disabled(0)，避免未知模式落入运行期拒绝分支或未知状态污染规则表；契约新增 `modeValidation/statusValidation`。
- [x] `content.sensitive` 中文宽松匹配补强：`cjkLoose` 归一化现在会忽略常见零宽字符、BOM 类格式字符、CJK/全角标点和 ASCII 分隔符，用于识别中文敏感词被插入不可见字符或分隔符绕过的低成本场景；配置表单中文乱码已修复，契约新增 `cjkLooseZeroWidth`。
- [x] `content.sitemap` 缓存文件诊断：后台 `sitemap/stats` 新增 `cacheFiles` 只读数组，展示 sitemap.xml、sitemap-index.xml、rss.xml、robots.txt、cache.json、dirty.json 的存在性、字节数和更新时间；能力页统计表同步显示，契约新增 `cacheFileDiagnostics`，不改变公开 sitemap/RSS/robots 请求路径。
- [x] `content.import-export` 导入结果状态一致性：导入提交只有在单行校验、写库、slug 同步、版本快照和派生数据同步全部完成后才计入 `successCount`；保存的导入任务状态按最终行结果写为 `imported/partial_failed/failed`，响应返回 `jobStatus`，便于后台任务筛选和统计定位异常。
- [x] `content.workflow` 统计诊断补强：后台 `workflow/stats` 新增 `approvalConfig` 和 `scheduledDue` 只读诊断，展示 requiredApprovals、requireDistinctApprovers、多审启用状态，以及按 `maxListRows` 有界扫描得到的到期定时发布数量、扫描数量和截断标记；契约新增 `statsDiagnostics`。
- [x] `content.form` 通知统计表格化：能力页 `form/notification/stats` 结果从原始 JSON 面板升级为总量表和事件聚合表，仍保留 JSON 诊断；该入口只读取本地通知收件箱，不触发外部投递，契约新增 `notificationStatsUi`。
- [x] `content.form` 通知已读状态边界：后台 `form/notification/status` 只接受 0/1，拒绝把未知状态静默归一为已读，避免通知记录筛选和统计语义被污染；契约 `statusValidation` 已同步声明。
- [x] `content.comment` 审核统计表格化：能力页 `comment/moderation-stats` 结果从 JSON 面板升级为审核状态总量表，展示总数、待审、通过、驳回、隐藏、删除和近期待审窗口，仍保留 JSON 诊断；契约新增 `adminModerationStatsUi`。
- [x] `content.import-export` 任务统计表格化：能力页 `import-export/stats` 结果从 JSON 面板升级为导入/导出总量表和状态分布表，展示任务数、总行数、成功/失败行和最近完成时间，仍保留 JSON 诊断；契约新增 `jobStatsUi`。
- [x] `content.access` 权限统计表格化：能力页 `access/rule/stats` 结果从 JSON 面板升级为规则总量表和权限模式分布表，展示启用/停用、内容/栏目规则和最近更新时间，仍保留 JSON 诊断；契约新增 `adminStatsUi`。
- [x] `content.audit-log` 审计统计表格化：能力页 `audit-log/stats` 结果从 JSON 面板升级为审计总览、动作分布和对象类型分布表，展示日志数、操作者数、对象数、统计上限和最近时间，仍保留 JSON 诊断；契约新增 `adminStatsUi`。
- [x] `content.revision` 版本统计表格化：能力页 `revision/stats` 结果从 JSON 面板升级为版本总览、动作分布和状态分布表，展示版本总数、内容数、最大版本号和最近快照时间，仍保留 JSON 诊断；契约新增 `adminStatsUi`。
- [x] `content.tag` / `content.topic` 统计表格化：能力页 `tag/stats` 与 `topic/stats` 结果从 JSON 面板升级为总览、状态分布和 Top 关联项表，展示关系总量、Top 上限和最近更新时间，仍保留 JSON 诊断；契约新增 `adminStatsUi`。
- [x] `content.search` 索引统计表格化：能力页 `search/stats` 结果从 JSON 面板升级为索引健康表，展示内容总数、索引条数、缺失条数、索引状态和最近更新时间，仍保留 JSON 诊断；契约新增 `adminStatsUi`。
- [x] `content.workflow` 工作流统计表格化：能力页 `workflow/stats` 结果从 JSON 面板升级为待办/日志/通知总览、审批配置诊断、到期定时发布诊断和动作分布表，仍保留 JSON 诊断；契约新增 `adminStatsUi`。
- [x] `content.sensitive` 词库统计表格化：能力页 `sensitive/stats` 结果从 JSON 面板升级为词库总览和命中日志窗口表，展示启用/停用词条、分组、作用域、近期命中和最近命中时间，仍保留 JSON 诊断；契约新增 `statsUi`。
- [x] `content.static` 静态化统计表格化：能力页 `static/stats` 结果从 JSON 面板升级为规则、任务、产物和状态分布表，展示任务成功/失败/待处理、产物覆盖对象和最近更新时间，仍保留 JSON 诊断；契约新增 `adminStatsUi`。
- [x] `content.related` 关联统计 UI 契约补齐：能力页 `related/stats` 已表格展示关联总览、规则诊断和关联类型分布，本轮补齐契约 `adminStatsUi` 并锁定检查口径。
- [x] `content.sitemap` 刷新计划表格化：能力页 `sitemap/refresh-plan` 结果从 JSON 面板升级为候选内容、当前条目、本次刷新、截断状态和缓存策略表，仍保留 JSON 诊断；契约新增 `refreshPlanUi`。
- [x] `content.like` / `content.view-stat` 统计 UI 契约补齐：能力页 `like/stats` 与 `view/stats` 已表格展示指标，本轮补齐 `adminStats/adminStatsUi` 并锁定检查口径。
- [x] `content.form` 表单状态边界：后台 `form.save` 保存时会把表单定义状态归一化为 enabled(1) 或 disabled(0)，`form.submission.status` 只接受 0/1/2，避免提交处理工作台写入未知状态；契约新增 `statusValidation`。
- [x] `content.tag` / `content.topic` 状态边界：后台 `tag.save` 和 `topic.save` 保存时会把状态归一化为 enabled(1) 或 disabled(0)，禁用态可被正确持久化，公开列表继续只读取 `status=1`；契约新增 `statusValidation`。
- [x] `content.category` 栏目绑定迁移诊断表格化：能力页 `category.bind.status` 从裸 JSON 升级为固定迁移指标表，展示 legacy 内容行、绑定表有效行、缺失绑定、不一致数量和建议动作；`category.bind.backfill` 回填结果同步表格化，仍保留原始 JSON 诊断，契约新增 `categoryBindDiagnosticsUi/categoryBindBackfillUi`。
- [x] `content.search` 搜索测试和排序解释表格化：能力页 `search` 测试结果从裸 JSON 升级为查询概要与结果分项得分表，排序解释升级为 guard、权重和 Top 结果分数表，仍保留 JSON 诊断；契约新增 `rankingExplainUi/probeResultUi`。
- [x] `content.slug` / `content.redirect` / `content.static` 统一 URL 规则诊断表格化：能力页 `route-rule/plan`、`route-rule/validate`、`route-rule/stats` 从裸 JSON 升级为计划表、风险表和按能力包/规则类型聚合表，仍保留 JSON 诊断；所有检查保持管理期固定上限，不进入请求路由热路径，契约新增 `routeRulePlanUi/routeRuleValidateUi/routeRuleStatsUi`。
- [x] `content.category` 写路径绑定表补齐：导入提交和版本恢复在写回 legacy `content_item.category_id` 的同时会调用 `Managed_CategoryBindSet` 同步 `content_category_bind`，避免导入或恢复旧版本后重新制造绑定表漂移；契约新增 `categoryBindWritePaths/categoryBindImportSync/categoryBindRestoreSync`。
- [x] `content.workflow` 到期定时发布结果表格化：能力页 `workflow.scheduled-publish` 执行结果从裸 JSON 升级为本次发布数、执行上限、处理时间和任务类型表，仍保留 JSON 诊断；契约新增 `scheduledPublishUi`。
- [x] `content.related` 规则重建结果表格化：能力页 `related.rebuild` 执行结果从裸 JSON 升级为来源内容数、创建规则数、全量来源上限和单源候选上限表，仍保留 JSON 诊断；契约新增 `rebuildResultUi`。
- [x] `content.search` 索引重建结果表格化：能力页 `search.rebuild` 执行结果从裸 JSON 升级为扫描内容、写入索引、offset/limit、最大上限、是否还有下一批和下一偏移表，仍保留 JSON 诊断；契约新增 `rebuildResultUi`。
- [x] `content.workflow` 动作结果表格化：能力页 `workflow.action` 执行结果从裸 JSON 升级为动作、内容 ID、状态迁移、更新状态、日志/通知保存状态和审批进度表，仍保留 JSON 诊断；契约新增 `actionResultUi`。
- [x] `content.static` 管理操作结果表格化：能力页 `static.rule.preview`、`static.clean` 和 `static.task.retry-failed` 结果从裸 JSON 升级为规则预览、清理结果、批量重试概要与逐任务结果表，仍保留 JSON 诊断；契约新增 `rulePreviewUi/cleanResultUi/taskRetryFailedUi`，清理响应补充 `limit/targetId/ruleId`。
- [x] `content.static` 静态生成结果表格化：能力页 `static.generate` 结果从消息刷新升级为任务 ID、产物路径、结果和消息表，仍保留 JSON 诊断；契约新增 `generateResultUi`，该操作仍只在后台触发静态任务和产物写入，不改变请求期静态文件查找顺序。
- [x] `content.audit-log` 清理结果表格化：能力页 `audit-log.cleanup` 结果从消息刷新升级为删除数量、保留天数、清理前时间、本次上限和最大上限表，仍保留 JSON 诊断；契约新增 `cleanupResultUi`，清理仍按固定上限只影响后台审计日志表。
- [x] `content.revision` 恢复结果表格化：后台 `revision.restore` 响应补充 `revisionId/contentId/title/status/categoryId/isDraft/updateTime`，能力页确认恢复后展示固定结果表并保留 JSON 诊断；契约新增 `restoreResultUi`，恢复仍同步内容、栏目绑定、派生数据、静态生成和审计记录。
- [x] 能力页单行操作结果表格化：通用主表单保存、评论审核/隐藏、静态任务单条重试、表单通知已读/未读/重放、表单提交处理、删除和解绑等操作统一渲染为操作、目标 ID、结果和消息表；仍保持原表格刷新行为，不新增后端查询或请求热路径成本。
- [x] `content.slug` / `content.redirect` / `content.static` URL 规则快照显式刷新：新增后台 `route-rule/refresh`，按 `maxListRows` 有界重建 `content_route_rule` 管理期快照，返回同步前数量、本次同步数量、同步后数量和同步上限；能力页“刷新快照”按钮改为展示结果表，契约新增 `routeRuleRefreshApi/routeRuleRefreshUi`，请求期仍保持静态优先、动态兜底，不做额外冲突扫描。
- [x] `content.slug` 检测和修复结果表格化：能力页 `slug.check/preview` 从裸 JSON 升级为可用性、冲突内容、规范 URL、解析 API 和风险提示表，`slug.repair` 升级为扫描/变更/保存汇总与逐内容修复表，仍保留 JSON 诊断；契约新增 `checkPreviewUi/repairResultUi`。
- [x] `content.category` 栏目绑定漂移样本诊断：`category.bind.status` 在管理期返回有界 `missingSamples/mismatchSamples`，展示内容 ID、标题、旧栏目 ID 和绑定栏目 ID，能力页同步表格化；该检查只在后台诊断入口执行，不加入请求热路径，契约新增 `categoryBindDriftSamples`。
- [x] `content.category` 栏目树状态摘要：栏目管理页基于已加载的有界树结果显示总行数、当前可见行数和已收起节点数，配合关键词过滤和展开/收起定位大树；不新增后端查询，契约新增 `treeSummaryUi`。
- [x] `content.category` 栏目树搜索上下文：栏目管理页关键词过滤保留命中栏目、祖先链和子孙节点，搜索状态下不受已收起节点遮挡，避免大树中命中行失去层级上下文；只在前端处理已加载的有界树结果，契约新增 `treeFilterContextUi`。
- [x] `content.comment` 待审风险分布统计：`comment.moderation-stats` 复用现有风险评分函数，只扫描待审队列前 `maxAdminListRows` 条，返回 `riskStats/riskScanned/riskScanLimit`，能力页表格化展示低/中/高风险分布；不改变公开提交和审核策略，契约新增 `adminModerationRiskStats`。
- [x] `content.sensitive` 近期高频命中统计：`sensitive.stats` 在统计窗口内返回有界 `topWords/topGroups/topLimit`，能力页表格化展示高频敏感词和分组，便于词库运营；只读取命中日志聚合，不改变内容保存、评论提交或检测接口扫描路径，契约新增 `statsTopHits`。
- [x] `content.import-export` 最近失败导入样本：`import-export.stats` 返回有界 `importFailureSamples/failureSampleLimit`，能力页展示任务 ID、来源、状态、总行、成功、失败和完成时间，便于定位失败任务；只读任务表，不执行导入、导出或失败回放，契约新增 `failureSamples`。
- [x] `content.audit-log` 操作者分布诊断：`audit-log.stats` 新增有界 `operatorStats`，按 `operator_type/operator_id` 聚合数量和最近时间，能力页表格化展示，便于排查异常写操作来源；只读审计日志聚合，不改变审计写入和清理策略，契约新增 `operatorStats`。
- [x] `content.redirect` 导入结果表格化：能力页 `redirect.import` 预检/确认结果从裸 JSON 升级为总行数、有效行、保存数、失败数和逐行风险表，仍保留 JSON 诊断；契约新增 `importResultUi`。
- [x] `content.sensitive` 操作结果表格化：能力页 `sensitive.word.import`、`sensitive.check` 和 `sensitive.log.cleanup` 从裸 JSON 升级为导入汇总/逐词结果、检测命中明细和有界清理结果表，仍保留 JSON 诊断；契约新增 `importResultUi/checkResultUi/cleanupResultUi`。
- [x] `content.tag` / `content.topic` 管理操作结果表格化：能力页 `tag.merge` 显示来源/目标/移动/删除汇总，`topic.content.sort` 显示提交行数、更新行数和 500 行上限，仍保留 JSON 诊断；契约新增 `mergeResultUi/sortResultUi`。
- [x] `content.comment` / `content.seo` / `content.import-export` 管理结果表格化：评论批量审核、内容/栏目 SEO 预览和导入导出字段计划从裸 JSON 升级为汇总表/明细表/变量表，仍保留 JSON 诊断；契约新增 `adminBatchModerationUi/previewResultUi/fieldPlanUi`。
- [x] `content.import-export` 导入结果表格化：手工预检、确认导入、分片预检和分片导入结果显示任务汇总、逐行结果和逐分片汇总，仍保留 JSON 诊断；契约新增 `importResultUi/chunkResultUi`。
- [x] `content.import-export` JSON 导出结果表格化：导出结果显示任务保存、分页、字段计划和前 10 行预览，完整导出数据仍保留 JSON 诊断；契约新增 `exportResultUi`。
- [x] `content.form` 提交统计与导出结果表格化：提交统计显示总览、状态分布和表单分布，提交导出显示上限、数量、JSON 大小和前 10 行预览，仍保留 JSON 诊断；契约新增 `submissionStatsUi/submissionExportUi`。
- [x] `content.sitemap` 手动刷新结果表格化：刷新结果显示扫描/保存数量、请求上限、最大上限、缓存写入状态和 warning，仍保留 JSON 诊断；契约新增 `refreshResultUi`。
- [x] `content.media` / `content.tag` / `content.topic` 批量操作结果表格化：媒体批量启停/删除、标签批量状态和专题批量状态显示汇总与逐 ID 结果，仍保留 JSON 诊断；契约新增 `batchResultUi/batchStatusUi`。
