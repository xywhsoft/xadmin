# 内容系统内置化重构 SPEC

## 1. 目标

本 SPEC 用于跟踪内容系统从旧 `content-system` 插件重写为 xadmin 内置内容生成器的进度。

本轮不是迁移旧插件，也不做旧接口兼容。旧 `hosts/xadmin/plugin/content-system` 只作为需求和实现参考；新实现直接建立在 xadmin 内部模块、主数据库、标准 page/template 目录之上。

核心结论：

- 生成出来的业务内容模块仍然是插件。
- 内容插件生成器本身不再是插件。
- 内容系统管理数据进入 xadmin 主数据库。
- 生成出来的业务插件继续使用自己的插件数据库。
- 能力包互通机制改为 xadmin 内置能力注册中心，避免插件之间互扫和互相耦合。

前置依赖：

- [x] 先完成 [插件系统资源目录增强 SPEC](./插件系统资源目录增强SPEC.md) 中的插件 `page/template/option/static/inc/lib/src` 标准目录、静态资源映射和动态表单接入。
- [x] 内容系统生成的业务插件必须使用增强后的插件系统，不再把静态资源写入 xadmin `wwwroot`。

## 2. 范围

### 2.1 包含

- [x] 新建 xadmin 内置内容模块。
- [x] 新建内容系统主数据库表。
- [x] 新建标准后台页面目录。
- [x] 新建标准模板目录。
- [~] 重新实现内容模型 CRUD、修订、预检、生成。
- [x] 重新实现 xadmin 内置能力 registry。
- [x] 将 comment 能力作为第一批内置能力注册。
- [x] 生成业务插件仍输出到 `hosts/xadmin/plugin/<plugin_xid>/`。
- [x] 生成业务插件继续使用插件私有数据库。

### 2.2 不包含

- [x] 不兼容旧 `/admin/api/plugin/content-system/*` 接口。
- [x] 不兼容旧 `/admin/view/plugin/content-system/*` 页面。
- [x] 不保留旧 `content-system` 插件运行形态。
- [x] 不做插件之间能力 provider 扫描协议。
- [x] 不把业务内容数据写入 xadmin 主库。

## 3. 目标目录结构

### 3.1 后端模块

目标位置：

```text
hosts/xadmin/script/content/
  content_init.h
  content_route.h
  content_db.h
  content_model.h
  content_spec.h
  content_revision.h
  content_advisor.h
  content_generator.h
  content_capability.h
```

进度：

- [x] 建立 `script/content/` 目录。
- [x] 建立 `content_init.h`，负责模块初始化。
- [x] 建立内容系统路由入口，当前落点为 `route_http/content.h` 并由 `route.h` 注册。
- [x] 建立 `content_db.h`，负责建表、迁移、事务、SQL 工具。
- [x] 建立 `content_model.h`，负责模型 CRUD。
- [x] 建立 `content_spec.h`，负责 DSL normalize/validate。
- [x] 建立 `content_revision.h`，负责修订和快照。
- [x] 建立 `content_advisor.h`，负责生成预检和升级风险。
- [x] 建立 `content_generator.h`，负责模板渲染和业务插件输出。
- [x] 建立 `content_capability.h`，负责内置能力注册和查询。
- [x] 在 `hosts/xadmin/script/main.c` 中接入 `Content_Init()`。
- [x] 在 `hosts/xadmin/script/route.h` 中接入内容系统路由。
- [x] 在后台菜单中注册内置内容模型入口。

### 3.2 页面目录

目标位置：

```text
hosts/xadmin/data/page/content/
  index.html
  editor.html

hosts/xadmin/data/page/content/js/
  index.js
  content-api.js
  editor-state.js
  editor-fields.js
  editor-display.js
  editor-capability.js
  editor-generate.js
  editor-main.js

hosts/xadmin/data/page/content/css/
  index.css
  editor.css
```

进度：

- [x] 建立 `data/page/content/index.html`。
- [x] 建立 `data/page/content/editor.html`。
- [x] 将内容模型列表页脚本拆到 `index.js`。
- [x] 将内容模型列表页样式拆到 `index.css`。
- [x] 将 API 调用封装到 `content-api.js`。
- [x] 将编辑器状态管理拆到 `editor-state.js`。
- [x] 将字段编辑逻辑拆到 `editor-fields.js`。
- [x] 将页面配置逻辑拆到 `editor-display.js`。
- [x] 将能力装配逻辑拆到 `editor-capability.js`。
- [x] 将生成预检逻辑拆到 `editor-generate.js`。
- [x] 将编辑器事件绑定拆到 `editor-main.js`。
- [x] 将页面样式拆到 `editor.css`。
- [x] 页面使用 layui/xadmin 风格，不再保留插件页自定义杂糅风格。

### 3.3 模板目录

目标位置：

```text
hosts/xadmin/data/template/content/
  plugin.main.c.tpl
  plugin.admin.html.tpl
  plugin.public.html.tpl
  plugin.managed.json.tpl
  plugin.contracts.json.tpl
```

进度：

- [x] 建立 `data/template/content/`。
- [x] 建立业务插件 C 入口模板。
- [x] 建立业务插件后台页面模板。
- [x] 建立业务插件前台页面模板。
- [x] 建立 `managed.json` 模板。
- [x] 建立 `contracts.json` 模板。
- [x] 生成器只从标准模板目录读取模板。
- [x] 生成业务插件时输出 `template/static/detail.html`，供 `content.static` 服务端静态化渲染使用。
- [x] 静态化详情模板默认消费 `<field>_html` 字段，例如 Markdown 正文字段输出为 `content_html`。

### 3.4 内置能力目录

目标位置：

```text
hosts/xadmin/data/content/capabilities/
  comment.json
  category.json
  tag.json
  seo.json
```

进度：

- [x] 建立 `data/content/capabilities/`。
- [x] 建立 `comment.json`。
- [x] 建立 `category.json` 占位定义。
- [x] 建立 `tag.json` 占位定义。
- [x] 建立 `seo.json` 占位定义。
- [x] 后端从该目录加载能力定义。
- [x] 前端从 `/admin/content/capabilities` 读取能力列表。
- [x] 移除前端硬编码 provider 列表。

## 4. 路由设计

新路由：

```text
/admin/view/content
/admin/view/content/editor

/admin/content/types
/admin/content/type
/admin/content/save
/admin/content/revisions
/admin/content/advisor
/admin/content/generate
/admin/content/capabilities
/admin/content/templates
```

进度：

- [x] 注册 `/admin/view/content`。
- [x] 注册 `/admin/view/content/editor`。
- [x] 注册 `/admin/content/types`。
- [x] 注册 `/admin/content/type`。
- [x] 注册 `/admin/content/save`。
- [x] 注册 `/admin/content/revisions`。
- [x] 注册 `/admin/content/advisor`。
- [x] 注册 `/admin/content/generate`。
- [x] 注册 `/admin/content/capabilities`。
- [x] 注册 `/admin/content/templates`。
- [x] 确认新页面不再调用 `/admin/api/plugin/content-system/*`。

## 5. 数据库设计

内容系统使用 xadmin 主数据库：

```text
hosts/xadmin/data/db/main.db
```

生成出来的业务插件继续使用插件私有数据库，不将业务数据写入主库。

### 5.1 content_model

保存当前内容模型草稿和生成状态摘要。

```sql
CREATE TABLE IF NOT EXISTS content_model (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  xid TEXT NOT NULL UNIQUE,
  name TEXT NOT NULL,
  namespace TEXT NOT NULL DEFAULT '',
  title TEXT NOT NULL,
  description TEXT NOT NULL DEFAULT '',
  icon TEXT NOT NULL DEFAULT '',
  table_name TEXT NOT NULL DEFAULT '',
  status TEXT NOT NULL DEFAULT 'active',
  field_count INTEGER NOT NULL DEFAULT 0,
  current_revision INTEGER NOT NULL DEFAULT 0,
  applied_revision INTEGER NOT NULL DEFAULT 0,
  generated_plugin_xid TEXT NOT NULL DEFAULT '',
  spec_json TEXT NOT NULL,
  spec_hash TEXT NOT NULL DEFAULT '',
  create_time INTEGER NOT NULL,
  update_time INTEGER NOT NULL
);
```

索引：

- [x] `idx_content_model_update_time(update_time DESC)`
- [x] `idx_content_model_status(status)`
- [x] `idx_content_model_generated_plugin(generated_plugin_xid)`

### 5.2 content_model_revision

保存不可变修订。

```sql
CREATE TABLE IF NOT EXISTS content_model_revision (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  model_id INTEGER NOT NULL,
  revision INTEGER NOT NULL,
  spec_json TEXT NOT NULL,
  spec_hash TEXT NOT NULL DEFAULT '',
  note TEXT NOT NULL DEFAULT '',
  generator_version TEXT NOT NULL DEFAULT '',
  create_time INTEGER NOT NULL,
  UNIQUE(model_id, revision)
);
```

索引：

- [x] `idx_content_model_revision_model_rev(model_id, revision DESC)`

### 5.3 content_generation

保存每次生成任务和结果，不把生成日志塞进模型表。

```sql
CREATE TABLE IF NOT EXISTS content_generation (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  model_id INTEGER NOT NULL,
  target_revision INTEGER NOT NULL,
  plugin_xid TEXT NOT NULL,
  status TEXT NOT NULL DEFAULT 'pending',
  output_json TEXT NOT NULL DEFAULT '{}',
  advisor_json TEXT NOT NULL DEFAULT '{}',
  error_message TEXT NOT NULL DEFAULT '',
  create_time INTEGER NOT NULL,
  finish_time INTEGER NOT NULL DEFAULT 0
);
```

索引：

- [x] `idx_content_generation_model_time(model_id, create_time DESC)`
- [x] `idx_content_generation_status(status)`

### 5.4 content_capability

xadmin 内置能力注册表，不依赖插件互扫。

```sql
CREATE TABLE IF NOT EXISTS content_capability (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  capability_key TEXT NOT NULL UNIQUE,
  title TEXT NOT NULL,
  provider_kind TEXT NOT NULL DEFAULT 'builtin',
  provider_xid TEXT NOT NULL DEFAULT '',
  surfaces_json TEXT NOT NULL DEFAULT '[]',
  defaults_json TEXT NOT NULL DEFAULT '{}',
  schema_json TEXT NOT NULL DEFAULT '{}',
  status TEXT NOT NULL DEFAULT 'active',
  sort INTEGER NOT NULL DEFAULT 0,
  create_time INTEGER NOT NULL,
  update_time INTEGER NOT NULL
);
```

索引：

- [x] `idx_content_capability_status_sort(status, sort)`

### 5.5 content_model_capability

记录某个模型启用了哪些能力，避免所有能力配置只能藏在 `spec_json` 中。

```sql
CREATE TABLE IF NOT EXISTS content_model_capability (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  model_id INTEGER NOT NULL,
  capability_key TEXT NOT NULL,
  config_json TEXT NOT NULL DEFAULT '{}',
  mount_json TEXT NOT NULL DEFAULT '{}',
  status TEXT NOT NULL DEFAULT 'enabled',
  create_time INTEGER NOT NULL,
  update_time INTEGER NOT NULL,
  UNIQUE(model_id, capability_key)
);
```

索引：

- [x] `idx_content_model_capability_model(model_id)`
- [x] `idx_content_model_capability_key(capability_key)`

### 5.6 数据库实施进度

- [x] 编写建表 SQL。
- [x] 编写主库 schema 初始化。
- [x] 编写轻量迁移版本记录。
- [x] 编写模型保存事务。
- [x] 编写修订保存事务。
- [x] 编写生成任务写入事务。
- [x] 编写能力注册刷新逻辑。
- [x] 编写基础数据校验。

## 6. 业务插件数据库边界

生成出来的业务插件仍然是插件，并继续使用自己的插件数据库。

业务插件可拥有：

```text
content records
draft records
category relations
tag relations
comment records
plugin runtime config
```

不得写入：

```text
content_model
content_model_revision
content_generation
content_capability
content_model_capability
```

进度：

- [x] 生成器输出业务插件私有 schema。
- [x] 生成插件初始化自己的数据库。
- [x] 生成插件不反向依赖内容系统管理表。
- [x] 内容系统只记录生成结果和插件 xid。

## 7. 能力设计

### 7.1 能力定义格式

建议能力定义：

```json
{
  "key": "comment",
  "title": "评论",
  "providerKind": "builtin",
  "providerXid": "comment",
  "surfaces": ["public.record-detail", "admin.record-detail"],
  "defaults": {
    "threadTitle": "评论",
    "pageSize": 50,
    "allowPublicPost": true,
    "moderationMode": "inherit"
  },
  "schema": {
    "groups": []
  }
}
```

进度：

- [x] 定义 capability JSON schema。
- [x] 实现 capability 文件加载。
- [x] 实现 capability 数据库同步。
- [x] 实现模型启用能力。
- [x] 实现模型禁用能力。
- [x] 实现能力默认配置合并。
- [x] 实现能力配置校验。
- [x] 编辑器显示能力来源、挂载面、默认配置和生成影响。

### 7.2 comment 能力

第一阶段 comment 作为内置能力注册，不再从 `comment-system` 插件扫描。

进度：

- [x] 建立 `comment.json`。
- [x] 注册 `public.record-detail` 挂载面。
- [x] 注册 `admin.record-detail` 挂载面。
- [x] 定义评论分页、公开提交、审核策略配置。
- [x] 生成器能把 comment 能力写入业务插件 contracts。
- [x] 编辑器能以表单配置 comment 能力。

## 8. 前端编辑器设计

顶部 tabs：

```text
概览 / 字段 / 页面 / 能力 / 策略 / 生成 / 高级
```

进度：

- [x] 概览页维护模型身份和实体基础信息。
- [x] 字段页左侧常驻字段列表。
- [x] 字段页支持新增、编辑、复制、删除、排序。
- [x] 字段选项使用表格可视化编辑。
- [x] 页面页可视化维护列表列。
- [x] 页面页可视化维护展示分组。
- [x] 页面页可视化维护字段分组。
- [x] 页面页可视化维护默认排序。
- [x] 页面页可视化维护前台详情关键字段。
- [x] 能力页从 `/admin/content/capabilities` 加载能力。
- [x] 能力页支持启用 comment。
- [x] 能力页支持配置 comment。
- [x] 策略页维护草稿、发布、删除、作者策略。
- [x] 生成页展示 advisor、文件影响、数据库影响、能力影响。
- [x] 高级 DSL 默认只读，解锁后才允许编辑。

## 9. 代码质量要求

- [x] 路由层只做参数解析、鉴权、调用服务、返回响应。
- [x] 路由层不直接写 SQL。
- [x] 路由层不直接生成文件。
- [x] DB 层统一封装事务。
- [x] 所有 SQL 使用 bind 参数。
- [x] spec 校验错误返回明确字段路径。
- [x] spec 校验会前置拒绝非法 `generatedPluginXid` 和重复 capability key。
- [x] generator 不读取 HTTP 请求对象。
- [x] 页面 HTML 不内联大段 JS。
- [x] 页面 CSS 独立文件维护。
- [x] 列表页和编辑页均不保留内联 CSS/JS。
- [x] API 返回统一结构：`result`、`message`、`data`、`errorCode`。
- [x] 前端 API 层对非 JSON、JSON 解析失败和网络失败做统一兜底，避免控制台异常。
- [x] 生成前必须先运行 advisor。
- [x] 生成前必须重新校验数据库中的 stored spec；失败要写入 `content_generation`。
- [x] 生成结果必须写入 `content_generation`。
- [x] 模型保存中能力同步失败必须回滚事务，不能静默吞掉 SQLite 写入错误。
- [x] 生成文件列表使用统一安全封装设置 data/size，避免构建结果为空时触发空指针长度计算。
- [x] 生成后台/前台 HTML 时对模型标题、模型 xid、字段名、字段标题和字段类型做 HTML 转义。

## 10. 实施阶段

### Phase 1：内置模块骨架

- [x] 建立目录结构。
- [x] 接入 `Content_Init()`。
- [x] 注册新路由。
- [x] 页面能通过 `/admin/view/content` 打开。
- [x] 编辑器能通过 `/admin/view/content/editor` 打开。

完成标准：

- [x] 不依赖 `plugin/content-system`。
- [x] 新页面返回 200。（通过安全入口登录后，`/admin/view/content` 和 `/admin/view/content/editor?new=1` 均可打开。）
- [x] 旧内容系统插件禁用不影响新页面打开。

### Phase 2：主库 schema

- [x] 创建 `content_model`。
- [x] 创建 `content_model_revision`。
- [x] 创建 `content_generation`。
- [x] 创建 `content_capability`。
- [x] 创建 `content_model_capability`。
- [x] 初始化索引。
- [x] 提供 schema 版本记录。

完成标准：

- [x] xadmin 启动后主库自动具备内容系统表。
- [x] 重复初始化不会破坏已有数据。

### Phase 3：模型与修订

- [x] 实现模型列表。
- [x] 实现模型读取。
- [x] 实现模型保存。
- [x] 实现修订创建。
- [x] 实现修订列表。
- [x] 实现 spec hash。
- [x] 实现 spec normalize/validate。

完成标准：

- [x] 不打开高级 DSL 也能保存一个基础内容模型。
- [x] 保存相同 spec 不重复创建修订。

### Phase 4：能力 registry

- [x] 加载内置 capability 文件。
- [x] 同步能力到 `content_capability`。
- [x] 实现能力列表 API。
- [x] 实现模型能力保存。
- [x] 实现 comment 能力表单。

完成标准：

- [x] 前端不再硬编码 comment provider。
- [x] comment 能力来自 xadmin 内置 registry。

### Phase 5：生成器

- [x] 建立标准模板目录。
- [x] 实现模板读取。
- [x] 实现模板渲染。
- [x] 实现业务插件目录输出。
- [x] 实现 `plugin.json` 输出。
- [x] 实现业务插件 schema 输出。
- [x] 实现 managed/contracts 输出。
- [x] 写入 `content_generation`。

完成标准：

- [x] 能从新内容系统生成一个业务插件。（浏览器生成 `cms.codex_verify_0501` 成功，生成历史显示 `success r1`。）
- [x] 生成插件可被插件系统发现。（重启后日志显示 `Discovered package: cms.codex_verify_0501`，且未自动启用。）
- [x] 生成插件使用自己的数据库。

### Phase 6：编辑器重构落地

- [x] 拆分页面 JS。
- [x] 拆分页面 CSS。
- [x] 接入新 API。
- [x] 接入新能力 API。
- [x] 移除旧插件 API 调用。
- [x] 浏览器验证全部 tabs。（概览、字段、页面、能力、策略、生成、高级均可切换；生成 tab 使用精确 `data-tab=generate` 验收。）

完成标准：

- [x] 浏览器控制台无 error。（列表页、编辑器页、保存、预检、生成流程均无 error/warn。）
- [x] 字段、页面、能力、策略、生成、高级均可操作。（tabs、保存修订、生成预检、生成插件已通过浏览器验收。）
- [x] 页面风格符合 layui/xadmin。（顶部工具条、tabs、左右工作区、状态面板按 layui/xadmin 风格显示。）

### Phase 7：清理旧插件依赖

- [x] 搜索并移除新代码中的 `content-system` 插件路径依赖。
- [x] 搜索并移除新代码中的 `/admin/api/plugin/content-system` 调用。
- [x] 搜索并移除新代码中的 `/admin/view/plugin/content-system` 调用。
- [x] 标记旧插件目录为废弃或删除。

完成标准：

- [x] 禁用旧插件后，新内容系统完整可用。（旧 `content-system` 插件保持禁用，内置内容系统已完成浏览器保存、预检、生成验收。）

## 11. 验收标准

- [x] 内容系统入口是 xadmin 内置路由。
- [x] 内容系统管理数据写入主数据库。
- [x] 生成出来的业务插件仍是插件。
- [x] 生成出来的业务插件使用插件私有数据库。
- [x] 页面、JS、CSS 放在标准 page 目录。
- [x] 生成模板放在标准 template 目录。
- [x] 能力来自 xadmin 内置 registry。
- [x] comment 能力不再依赖插件互扫。
- [x] 后端代码不再是单文件堆叠。
- [x] 前端代码不再是单 HTML 大脚本。
- [x] 生成前有 advisor。
- [x] 生成后有 generation 记录。
- [x] 旧内容系统插件禁用不影响新实现。（启动、路由、编辑器、保存、生成均已验收。）

## 12. 当前验收状态

- [x] 当前工作区只保留一个 `xs.exe` 服务进程监听 80 端口。
- [x] `xsdbg.exe` 启动校验通过：`Template cache ready`、`Content_Init done`、`runtime entered serving loop`，stderr 为空。
- [x] 内容编辑器拆分后的 JS 文件通过 `node --check`。
- [x] 内置 capability JSON 文件通过 JSON 解析。
- [x] 2026-05-01 重新校验：事务硬化、生成文件封装、生成 HTML 转义、spec 校验增强、生成入口 stored spec 校验后，JS、capability JSON、`xsdbg.exe` 启动加载均通过。
- [x] 浏览器端内容编辑器验收完成：使用安全入口 `http://127.0.0.1/jvb5umu0000a0az05tjn9q0ttrnms888` 和测试账号登录后，完成列表页、编辑器、tabs、保存修订、生成预检、生成插件、列表回显验证。
- [x] 生成验收模型 `cms.codex_verify_0501`：保存修订 1，生成后已应用修订 1，列表显示 `1 / 1 active`。
- [x] 运行时发现验证：重启后 `xs_runtime.out.log` 显示 `Discovered package: cms.codex_verify_0501`，`AutoStart check` 为 `enabled=0`，未运行新生成插件代码。
