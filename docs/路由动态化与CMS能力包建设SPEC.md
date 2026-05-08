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

若同时启用 `content.static` 和 `content.access`，受限内容不得生成公开裸静态详情页，除非生成的是需要二次校验的壳页面。

## 7. 三阶段路线图

### Phase 1：能力覆盖与动态路由基础

- [x] 新建动态路由设计与数据结构。
- [x] 支持动态路由注册 API。
- [x] 支持动态路由正则组预编译。
- [x] 支持 static route miss 后进入 dynamic route。
- [x] 支持动态路由命中后的参数提取。
- [x] 支持动态路由权限资源同步。
- [~] 支持动态路由提交期风险检查和提示。
- [~] 新增 `content.seo` 能力包：已补充清单/契约，并完成编辑页 SEO 字段按能力包注入、公开 `seo/meta` API 按能力包注册；当前过渡期使用 payload 存储，独立 SEO 表和独立管理页后续补强。
- [~] 新增 `content.slug` 能力包：已补充清单/契约，并完成编辑页 slug 字段按能力包注入、详情接口 slug 查询按能力包启用、公开 `slug/resolve` API 按能力包注册；当前过渡期使用 payload 存储，唯一性检查、URL 规则预览和历史 URL 后续补强。
- [~] 新增 `content.redirect` 能力包：已补充清单/契约，并完成启用时生成 `content_redirect` 表、公开 `redirect/resolve` API 按能力包注册、后台列表/保存/删除 API、能力包管理页表格与新增/编辑弹窗、命中统计回写；批量导入、slug 历史自动写入和真实 301/302 响应仍待补强。
- [~] 新增 `content.category` 能力包：已补充清单/契约，并完成栏目后台路由、菜单、列表页栏目筛选、编辑页栏目字段和 `generated/categories.html` 文件按能力包挂载/生成；schema 字段和更细页面模板仍待后续拆分。
- [~] 新增 `content.media` 能力包：已补充清单/契约/schema，并完成启用时生成 `content_media`、`content_media_ref` 表，公开 `media/list|detail` API、后台 `media/list|save|delete` API、内容保存时同步封面/正文资源引用、详情/列表返回 `mediaList` 与 `coverMediaUrl`、编辑页按能力包注入封面资源 ID/正文资源 ID 字段；真实上传资源选择器、资源反查管理页和图片处理仍待补强。
- [~] 新增 `content.revision` 能力包：已补充 schema，并完成启用时生成 `content_revision` 表、内容保存自动写入版本快照、后台 `revision/list|detail|restore` API 按能力包注册、能力管理页版本表格；字段级 diff、编辑页内版本抽屉和更完整恢复确认流程仍待补强。
- [~] 新增 `content.workflow` 能力包：已修复能力包 effects 描述，补充 `content_workflow_log` schema，并完成启用时注册后台 `workflow/action` 与 `workflow/log/list` API，支持 submit、approve/publish、reject、offline 四类基础动作和工作流日志表格；定时发布、编辑页动作按钮、权限细分和审核人流转仍待补强。
- [~] 新增 `content.search` 能力包：已补充 `content_search_index` schema，并完成启用时注册公开 `/search` 与后台 `/search` API，当前复用内容列表的 q 检索能力，覆盖标题、slug、摘要和 DSL 中标记 `searchable` 的字段；独立索引重建、分词、权重排序和搜索统计仍待补强。
- [~] 新增 `content.sitemap` 能力包：已补充 `content_sitemap_entry` schema，并完成启用时注册公开 `sitemap.xml`、`rss.xml`、`robots.txt` API，以及后台 `sitemap/entry/list` 表格；当前按已发布内容实时输出，绝对域名、增量生成、任务化刷新和落盘缓存仍待补强。
- [~] 新增 `content.related` 能力包：已补充 `content_related` schema，并完成启用时注册公开 `related/list` API、后台 `related/list/save/delete` API，以及内容详情 `relatedList` 附带输出；当前支持手工关联和 `relationType=rule` 的规则类型占位，自动规则计算、去重策略和推荐权重模型仍待补强。
- [~] 新增 `content.form` 能力包：已补充 `content_form` 与 `content_form_submission` schema，并完成启用时注册公开 `form/submit` API、后台 `form/list/save/delete` 与 `form/submission/list` API；当前支持基础 JSON schema 记录和提交数据入库，表单设计器、字段级校验、通知、导出和反垃圾策略仍待补强。
- [~] 新增 `content.access` 能力包：已补充 `content_access_rule` schema，并完成启用时注册公开 `access/check` API、后台 `access/rule/list/save/delete` API；内容公开列表会过滤未授权内容，详情读取会返回访问拒绝，当前支持 public、login、level、password、private 五类基础模式，会员组、付费订单、密码哈希和静态化联动仍待补强。
- [~] 新增 `content.audit-log` 能力包：已补充 `content_audit_log` schema，并完成启用时注册后台 `audit-log/list` API；内容保存、删除、版本恢复、工作流动作和阅读权限规则变更会自动写入基础审计日志，IP 采集、差异摘要、归档清理和更完整的敏感操作覆盖仍待补强。
- [~] 新增 `content.import-export` 能力包。
- [x] 所有能力包在后台可见中文显示名。
- [~] 未启用能力包时，生成插件仍可运行：栏目入口、栏目页面文件、栏目筛选/编辑字段、旧 7 个能力包公开/后台 API 路由已按能力包开关不注册/不生成/不渲染；schema 字段和更细模板代码仍待后续拆分。
- [ ] 启用能力包时，对应菜单、权限、路由、schema 可运行。

### Phase 2：能力生产级补强

- [~] `content.seo` 支持内容级 SEO 元信息：编辑页字段和公开 meta API 已接入；栏目、列表、详情页模板注入、独立 SEO 表仍待补强。
- [~] `content.slug` 支持冲突检测、规则预览、历史 URL：基础 slug 字段注入和公开解析 API 已接入；冲突检测、规则预览、历史 URL 仍待补强。
- [~] `content.redirect` 支持 301/302、命中统计、批量导入：基础解析 API、301/302 状态码数据、命中统计、后台管理表格和新增/编辑弹窗已接入；真实 HTTP 跳转和批量导入仍待补强。
- [ ] `content.category` 支持更好用的栏目树、排序、模板、权限。
- [~] `content.media` 支持封面图、正文资源、资源反查、删除保护：封面/正文资源引用同步、详情返回和删除保护已接入；资源反查管理页、上传/附件联动、图片尺寸识别和批量管理仍待补强。
- [~] `content.revision` 支持版本列表、字段级 diff、恢复：版本快照、版本列表、详情和恢复 API 已接入；字段级 diff、可视化恢复入口和恢复前对比仍待补强。
- [~] `content.workflow` 支持提交、审核、驳回、发布、下线、定时发布：提交、通过/发布、驳回、下线和日志已接入；定时发布、审核人流转、编辑页动作入口和权限细分仍待补强。
- [~] `content.search` 支持后台和前台搜索：前台/后台搜索 API 已接入并按能力包启用状态注册；生产级索引、分词、权重排序、高亮和重建任务仍待补强。
- [~] `content.sitemap` 支持 sitemap、RSS、robots、增量生成：sitemap、RSS、robots 实时输出和后台入口已接入；增量生成、落盘缓存、绝对 URL 配置和发布/删除事件刷新仍待补强。
- [~] `content.related` 支持手工关联和规则关联：手工关联 CRUD、公开关联列表和详情附带输出已接入；规则关联当前先保留 relationType 与权重字段，后续补规则计算器和发布事件刷新。
- [~] `content.form` 支持内容附属表单：表单定义、公开提交和后台提交记录已接入；字段级校验、可视化设计器、导出、通知和风控仍待补强。
- [~] `content.access` 支持阅读等级、会员组、付费、密码访问：阅读等级、登录、密码和私有模式已接入基础读取检查；会员组和付费字段已预留，订单校验、会员组判定、密码哈希和受限静态页策略仍待补强。
- [~] `content.audit-log` 覆盖所有敏感内容操作：核心内容写操作和权限规则变更已接入，媒体、相关推荐、表单、评论、标签、专题、敏感词和静态化任务等能力包写操作仍需继续补齐。
- [ ] `content.import-export` 支持导入预检、失败行报告、字段选择。
- [ ] 所有能力包有生成前 advisor 风险提示。
- [ ] 所有能力包有最小浏览器/API 验收路径。

### Phase 3：能力包代码拆分与 Hook 化

- [ ] 从稳定的大模板中识别真实扩展点。
- [ ] 定义生成期 slot。
- [ ] 定义运行期生命周期 hook。
- [ ] 支持能力包复制 source/include/template/assets。
- [ ] 支持能力包声明 include/source。
- [ ] 支持 `#ifdef XADMIN_CAP_*` 过渡开关。
- [ ] 未启用能力包不复制、不 include、不编译。
- [ ] `patches.json` 保留为高级能力，但不作为默认路径。
- [ ] 基础内容插件与能力包代码彻底分离。

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
