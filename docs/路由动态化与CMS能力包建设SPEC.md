# 路由动态化与 CMS 能力包建设 SPEC

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

### 1.4 bbre 使用边界

`bbre_set_matches` 适合做批量候选规则命中定位：

```text
path -> matched pattern index
```

它不直接负责路由参数提取。命中后应进入第二步：

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

若同时启用 `content.static` 和 `content.access`，受限内容不得生成公开裸静态详情页，除非生成的是需要二次校验的壳页面。当前生成入口和自动静态化已对非 public 内容做生成前拒绝，避免受限内容落成公开静态产物。

## 7. 三阶段路线图

### Phase 1：能力覆盖与动态路由基础

- [x] 新建动态路由设计与数据结构。
- [x] 支持动态路由注册 API。
- [x] 支持动态路由正则组预编译。
- [x] 支持 static route miss 后进入 dynamic route。
- [x] 支持动态路由命中后的参数提取。
- [x] 支持动态路由权限资源同步。
- [~] 支持动态路由提交期风险检查和提示：动态路由注册时已使用 xrt 正则库完成编译校验，静态 route key 冲突会拒绝注册，宽泛规则和可能覆盖静态资源路径的规则只在注册期记录 warning，不进入请求热路径；`/admin/trace` 已输出 `dynamicLastWarning` 供后台提示；`content.redirect` 保存/批量导入预检、`content.slug` 检查/预览/批量修复和 `content.static` 规则保存已对站点根路径、后台/API 前缀、静态资源前缀做提交期 warning，能力包管理页已显示这些 warning，`tools/check_content_system.ps1` 已加入静态门禁；真实伪静态动态路由融合仍待后续补强。
- [~] 补强 `content.comment` 能力包：评论列表、提交、审核、隐藏、删除、计数和线程计数刷新已接入；能力包管理页已增加基于 `status=0` 的待审核视图，通用能力包列表 API 支持存在 `status` 列时按状态过滤；公开评论列表保留平铺 `data` 并额外返回 `tree/children` 回复树，评论回复提交会校验父评论属于同一内容且已公开；通知和更细反垃圾策略仍待补强。
- [~] 补强 `content.like` / `content.view-stat` 能力包：点赞和访问统计已按能力包启用状态注册前台记录/状态/排行接口、后台计数/明细/日统计接口；本轮补充后台 `like/stats` 与 `view/stats` 汇总 API，并在能力包管理页提供 Stats 入口；启用点赞或访问统计时会额外生成并注册独立 `generated/dashboard.html` 统计看板页，看板已接入 `/view/daily/list` 日趋势表；图形化趋势和更细去重策略仍待补强。
- [~] 新增 `content.seo` 能力包：已补充清单/契约，并完成编辑页 SEO 字段按能力包注入、公开 `seo/meta` API 按能力包注册、后台 `seo/list|save|delete` 管理 API、独立 `content_seo_meta` 表、内容保存/导入/删除同步、公开展示页 head 注入、列表接口批量输出 `seoMeta`、栏目 SEO 字段按能力包启用显示、公开页栏目筛选时消费栏目 SEO 更新 head，以及能力包页独立 SEO 管理表格/弹窗；更细的栏目独立模板仍待补强。
- [~] 新增 `content.slug` 能力包：已补充清单/契约，并完成编辑页 slug 字段按能力包注入、详情接口 slug 查询按能力包启用、公开 `slug/resolve` API、后台 `slug/check|preview|history|repair` API、保存时唯一性校验、批量填充/去重修复和 `content_slug_history` 历史表按能力包注册；当前过渡期 slug 主值仍使用 payload 存储，真实伪静态规则后续补强。
- [~] 新增 `content.redirect` 能力包：已补充清单/契约，并完成启用时生成 `content_redirect` 表、公开 `redirect/resolve` API 按能力包注册、后台列表/保存/删除/批量导入 API、能力包管理页表格与新增/编辑弹窗、命中统计回写；批量导入已支持 preview 与 `confirm=true` 写入，slug 历史自动写入和 `redirect=1` 真实 301/302 响应已接入，后续与动态路由规则融合仍待补强。
- [~] 新增 `content.category` 能力包：已补充清单/契约，并完成栏目后台路由、菜单、列表页栏目筛选、编辑页栏目字段和 `generated/categories.html` 文件按能力包挂载/生成；已补公开 `category/list|detail|contents` API、后台 `category/tree` API、树形标题输出、父级 path/level 计算、子树 path/level 重算、栏目描述/封面/模板键字段、父栏目下拉、表格内排序保存、拖拽排序弹窗、拖拽弹窗内跨父级迁移、删除保护、公开页栏目筛选、栏目 SEO head 消费，以及公开列表按栏目 `templateKey` 切换 default/compact/grid/feature 布局；更完整树控件仍待后续补强。
- [~] 新增 `content.media` 能力包：已补充清单/契约/schema，并完成启用时生成 `content_media`、`content_media_ref` 表，公开 `media/list|detail` API、后台 `media/list|save|delete|batch|ref/list` API、内容保存时同步封面/正文资源引用、详情/列表返回 `mediaList` 与 `coverMediaUrl`、媒体列表返回 `refCount`、能力包管理页资源/引用双视图、批量启用/停用/删除、URL 扩展名与常见 MIME 推导、资源宽高字段展示/保存、浏览器侧图片尺寸识别、编辑页按能力包注入封面资源 ID/正文资源 ID 字段并提供媒体资源选择器；真实上传、附件联动和服务端图片尺寸识别仍待补强。
- [~] 新增 `content.revision` 能力包：已补充 schema，并完成启用时生成 `content_revision` 表、内容保存自动写入版本快照、后台 `revision/list|detail|diff|restore|restore-preview` API 按能力包注册、能力管理页版本表格、Diff 入口、恢复前当前内容对比确认，以及编辑页版本弹窗、Diff 和 Restore 入口；能力管理页和编辑页的 Diff/Restore Preview 已从 JSON 面板升级为字段级 Before/After 表格，并对长文本/多行字段提供逐行高亮显示，更细的字段类型专用 diff 仍待补强。
- [~] 新增 `content.workflow` 能力包：已修复能力包 effects 描述，补充 `content_workflow_log` schema，并完成启用时注册后台 `workflow/action`、`workflow/log/list`、`workflow/todo/list` 与 `workflow/scheduled-publish` API，支持 submit、approve/publish、reject、offline、schedule 基础动作、工作流日志表格、审核待办表格、能力包页动作面板、到期定时发布有界执行入口、编辑页 submit/approve/reject/offline 动作入口、动作指派人记录、到期定时发布处理和发布/下线后的派生能力同步；多级审批流、通知和自动任务仍待补强。
- [~] 新增 `content.search` 能力包：已补充 `content_search_index` schema，并完成启用时注册公开 `/search`、后台 `/search|search/rebuild|search/stats` API；当前搜索覆盖标题、slug、摘要和 DSL 中标记 `searchable` 的字段，已补独立索引重建、保存/导入/删除同步、索引统计、管理页有界搜索探针、公开页无额外筛选时走真实搜索接口、搜索分数、命中附近摘要和关键词高亮展示，以及空白分隔查询词的有序多词 LIKE 匹配；更完整分词和复杂权重仍待补强。
- [~] 新增 `content.sitemap` 能力包：已补充 `content_sitemap_entry` schema，并完成启用时注册公开 `sitemap.xml`、`sitemap-index.xml`、`rss.xml`、`robots.txt` API，以及后台 `sitemap/entry/list|refresh|stats` API；当前已补条目刷新、保存/导入/删除同步、统计入口、公开 XML/RSS 优先读取条目缓存、`content.sitemap.siteUrl` 绝对 URL 输出配置、`sitemap.xml?page=N` 分片和 `sitemap-index.xml` 索引，以及写入侧刷新 `sitemap/sitemap.xml|sitemap-index.xml|rss.xml|robots.txt` 落盘缓存，并写入 `sitemap/cache.json` 记录 write-through 策略、生成时间、条目数、TTL 和过期时间；后台统计页已表格化展示缓存策略、缓存文件、缓存条目数、TTL、过期状态和更新时间；后台队列失效策略仍待补强。
- [~] 新增 `content.related` 能力包：已补充 `content_related` schema，并完成启用时注册公开 `related/list` API、后台 `related/list/save/delete/rebuild` API，以及内容详情 `relatedList` 附带输出；当前支持手工关联，并已补同栏目规则重建生成 `relationType=rule` 关联、保存/导入后按 `content.related.ruleLimit` 同步刷新当前内容规则关联和最近同栏目内容的反向推荐，能力包页规则重建已增加 1-20 有界输入和独立结果面板，规则重建会跳过已存在的手工关联以避免重复推荐；复杂规则计算和推荐权重模型仍待补强。
- [~] 新增 `content.form` 能力包：已补充 `content_form` 与 `content_form_submission` schema，并完成启用时注册公开 `form/submit` API、后台 `form/list/save/delete` 与 `form/submission/list|export|status` API；当前支持基础 JSON schema 记录、schema textarea 管理、字段追加/删除/上下移动式可视化辅助，设计器可写入 placeholder、minLength/maxLength、min/max、pattern 和 options，JSON 格式化/校验、`required` 字段校验、`fields[].type/format/minLength/maxLength/min/max/pattern/options|list` 校验、honeypot/提交耗时基础反垃圾校验、提交数据入库、session 字段来源 IP 记录、JSON 导出和提交处理状态更新，完整拖拽式设计器、复杂字段校验和通知仍待补强。
- [~] 新增 `content.access` 能力包：已补充 `content_access_rule` schema，并完成启用时注册公开 `access/check` API、后台 `access/rule/list/save/delete` API；内容公开列表会过滤未授权内容，详情读取会返回访问拒绝，当前支持 public、login、level、group、password、paid、private 基础模式，paid 模式会明确返回 `payRequired/price` 且在订单系统接入前默认拒绝，password 模式新写入已使用带 salt 的 `xsha256:` 过渡哈希，并兼容旧 `xrt64:` 与明文；付费订单校验、真正 KDF 密码哈希和静态化联动仍待补强。
- [~] 新增 `content.audit-log` 能力包：已补充 `content_audit_log` schema，并完成启用时注册后台 `audit-log/list` 与 `audit-log/cleanup` API；内容保存、删除、版本恢复、工作流动作、阅读权限规则变更及评论、标签、专题、静态化、敏感词、推荐、表单、导入导出、媒体等能力包写操作会自动写入基础审计日志，已支持从真实 `xsReqRemote(objReq)` 和 session 的 `ip/clientIp/remoteAddr` 字段采集 IP，内容更新会记录字段级差异摘要，并支持按保留天数清理旧日志；更完整的外部任务/系统级操作覆盖仍待补强。
- [~] 新增 `content.import-export` 能力包：已补充 `content_import_job` 与 `content_export_job` schema，并完成启用时注册后台导入预检、确认导入、导入任务列表、失败行回放、JSON 导出、导出任务列表与导出下载 API；当前支持字段白名单、失败行报告、显式 `confirm=true` 的受控导入、`insert/update/skip` 导入冲突策略、按导入任务取回失败行并填回 JSON 编辑区、已完成导出任务 JSON 附件下载，JSON 导出 `limit/offset` 分片、`hasMore/nextOffset` 响应和管理页“下一片”连续导出，以及管理页按 1-200 条切片连续调用 preview/commit 的分片导入入口；真正流式上传解析和后台任务队列仍待补强。
- [x] 所有能力包在后台可见中文显示名。
- [~] 未启用能力包时，生成插件仍可运行：栏目入口、栏目页面文件、栏目筛选/编辑字段、旧 7 个能力包公开/后台 API 路由已按能力包开关不注册/不生成/不渲染；schema 字段和更细模板代码仍待后续拆分。
- [~] 启用能力包时，对应菜单、权限、路由、schema 可运行：已新增 `tools/check_capability_packs.ps1`，覆盖 21 个能力包的 manifest/contract/effects/schema/advisor 验收路径一致性检查，当前 `ErrorCount=0`、`WarningCount=0`；已新增 `tools/smoke_capability_acceptance.ps1` 可按能力包 acceptance path 对生成插件做 HTTP smoke，后续还需接入生成结果页/CI。

### Phase 2：能力生产级补强

- [~] `content.seo` 支持内容级 SEO 元信息：编辑页字段、公开 meta API、后台 SEO 管理 API/表格/弹窗、独立 `content_seo_meta` 表、内容保存/导入/删除同步、公开详情页 head 注入、列表接口批量 `seoMeta` 输出、栏目 SEO 字段保存/返回和公开页栏目筛选 head 消费已接入；更细的栏目独立模板仍待补强。
- [~] `content.slug` 支持冲突检测、规则预览、历史 URL：后台冲突检测、规则预览、历史 URL 表格、保存时唯一性校验、redirect 自动联动和批量修复 preview/confirm 已接入；真实伪静态规则仍待补强。
- [~] `content.redirect` 支持 301/302、命中统计、批量导入：基础解析 API、301/302 状态码数据、命中统计、后台管理表格、新增/编辑弹窗、批量导入 preview/confirm、slug 历史自动写入和 `redirect=1` 真实 HTTP 跳转已接入；后续与动态路由规则融合仍待补强。
- [~] `content.category` 支持更好用的栏目树、排序、模板、权限：后台树接口、树形标题、表格内排序保存、拖拽排序弹窗、拖拽弹窗内跨父级迁移、父栏目下拉、父级 path/level 计算、子树移动后的 path/level 重算、栏目描述/封面/模板键保存与返回、存在子栏目/内容时删除保护、内容访问规则向栏目父级继承、公开页栏目筛选、栏目 SEO head 消费和栏目模板键布局选择已接入；更完整树控件仍待补强。
- [~] `content.media` 支持封面图、正文资源、资源反查、删除保护：封面/正文资源引用同步、详情返回、引用列表、引用计数、删除保护、批量启用/停用/删除、URL 扩展名与常见 MIME 推导、宽高字段管理、浏览器侧图片尺寸识别和编辑页媒体资源选择器已接入；真实上传、上传/附件联动和服务端图片尺寸识别仍待补强。
- [~] `content.revision` 支持版本列表、字段级 diff、恢复：版本快照、版本列表、详情、字段级 diff、恢复 API、恢复前当前内容对比、能力包页恢复确认入口和编辑页版本弹窗/Diff/Restore 操作已接入；能力包页和编辑页已用字段级 Before/After 表格替代原始 JSON 展示，并已补长文本/多行字段逐行高亮，字段类型专用 diff 和图片资源差异仍待补强。
- [~] `content.workflow` 支持提交、审核、驳回、发布、下线、定时发布：提交、通过/发布、驳回、下线、schedule、日志、审核待办、能力包页动作入口、到期任务有界执行、编辑页动作按钮、动作指派人记录和到期定时发布处理已接入，契约已声明 submit/review/publish 动作权限映射，并在发布/下线后同步搜索、sitemap、related、静态化等派生能力；多级审批流、通知和自动任务仍待补强。
- [~] `content.search` 支持后台和前台搜索：前台/后台搜索 API、索引重建、索引统计、管理页查询探针、写入同步、独立索引查询、基础权重排序、命中附近摘要裁剪、公开页搜索接口切换、分数/摘要/关键词高亮展示和空白分隔查询词有序匹配已接入并按能力包启用状态注册；更完整分词和复杂相关性仍待补强。
- [~] `content.sitemap` 支持 sitemap、RSS、robots、增量生成：sitemap、sitemap-index、RSS、robots 输出、后台入口、条目刷新、统计、发布/删除同步、公开输出优先读取 `content_sitemap_entry`、`content.sitemap.siteUrl` 绝对 URL 输出、`sitemap.xml?page=N` 分片、写入侧落盘缓存、`sitemap/cache.json` write-through 元数据、TTL/过期时间和表格化缓存统计面板已接入；后台队列失效策略仍待补强。
- [~] `content.related` 支持手工关联和规则关联：手工关联 CRUD、公开关联列表、详情附带输出、同栏目规则重建、能力包页有界规则重建、内容保存/导入后的当前内容规则刷新、有界反向推荐同步和手工优先去重已接入；复杂规则计算器、推荐权重模型和全量发布事件刷新仍待补强。
- [~] `content.form` 支持内容附属表单：表单定义、schema textarea、字段追加/删除/重排式设计辅助、placeholder/校验边界/pattern/options 辅助写入、JSON 格式化/校验、公开提交、`required` 字段校验、`fields[].type/format/minLength/maxLength/min/max/pattern/options|list` 校验、honeypot/提交耗时基础反垃圾校验、后台提交记录、session 字段来源 IP 记录、JSON 导出和处理状态更新已接入；完整拖拽式设计器、更复杂类型/格式校验和通知仍待补强。
- [~] `content.access` 支持阅读等级、会员组、付费、密码访问：阅读等级、登录、会员组、密码、付费和私有模式已接入基础读取检查，已支持 `content.category` 启用时内容规则向所属栏目及父级栏目继承，paid 模式返回价格和支付要求但订单未接入前默认拒绝，password 模式新写入已升级为带 salt 的 `xsha256:` 过渡哈希，并兼容旧 `xrt64:` 与明文；受限内容已禁止生成公开裸静态页，订单校验、真正 KDF 密码哈希和壳页面式受限静态策略仍待补强。
- [~] `content.audit-log` 覆盖所有敏感内容操作：核心内容写操作、权限规则变更、媒体、相关推荐、表单、评论、标签、专题、敏感词、导入导出和静态化规则/任务操作已接入；请求处理路径中的审计 IP 已统一优先使用真实 request remote IP，并保留 session 字段来源兜底，后台已提供按保留天数清理旧日志，内容更新已写入字段级差异 detail，后续继续补外部任务和系统级操作覆盖。
- [~] `content.import-export` 支持导入预检、失败行报告、字段选择：导入预检、字段白名单、确认导入、任务报告、失败行回放、`insert/update/skip` 冲突策略、已完成导出任务 JSON 附件下载、JSON 导出 `limit/offset` 分片、管理页按 `nextOffset` 连续导出和管理页分片导入提交已接入；真正流式上传解析和后台任务队列仍待补强。
- [x] 所有能力包有生成前 advisor 风险提示：`Content_BuildAdvisor` 已按启用能力包输出生成前 warning，覆盖当前 21 个内置能力包；内容模型生成按钮已接入预检确认流，错误阻断生成，warning 需要确认后继续；advisor 已从逐包硬编码表迁移为读取能力包 manifest 的 `advisorMessage/acceptanceApiPath/acceptanceViewPath`，只保留通用兜底，检查脚本会强制校验三项元数据存在且验收路径包含 `{pluginXid}`。
- [~] 所有能力包有最小浏览器/API 验收路径：`Content_BuildAdvisor` 已为当前 21 个内置能力包输出 `kind=acceptance` 的 `apiPath` 与 `viewPath`，使用 `{pluginXid}` 占位；`tools/smoke_capability_acceptance.ps1` 已能消费同一批 API path 做 HTTP smoke，内容模型生成页预检后会展示已启用能力包的 API/页面验收路径；`tools/check_content_system.ps1` 已汇总能力包检查、前端 JS、模板 JS、C 模板编译、声明能力包 source 编译、动态路由风险 warning 展示链路和 smoke 脚本语法，可作为 CI 入口，后续接真实 CI 配置。

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
| `advisor.meta` | `content_advisor.h` | 生成前 warning 与最小验收路径 | 已覆盖 21 个能力包，后续迁到 manifest/advisor 元数据 |

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
