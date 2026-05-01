# 内容系统能力包体系增强 SPEC

## 1. 目标

本 SPEC 用于跟踪内容模型系统的能力包体系升级。

核心目标：

- 能力包成为独立于插件的统一安装实体。
- 内容模型菜单改为目录，包含“模型管理 / 能力包商店 / 能力包管理”。
- 能力包可见、可管理、可配置、可校验、可预览。
- 能力包真正参与内容插件生成，而不是只写入一个 `contracts.json` 标记。
- 不再区分内置能力包和安装能力包，只通过 manifest/registry 字段表达来源、保护策略和在线更新关系。

## 2. 范围

### 2.1 包含

- [ ] 新增能力包统一目录规范。
- [ ] 新增能力包 registry 数据表。
- [ ] 新增能力包扫描、校验、同步逻辑。
- [ ] 新增“能力包管理”页面。
- [ ] 新增“能力包商店”占位页面。
- [ ] 调整“内容模型”后台菜单为目录。
- [ ] 将当前 `data/content/capabilities/*.json` 迁移为能力包目录结构。
- [ ] 模型编辑页能力 Tab 改为读取能力包 registry。
- [ ] 能力包全局配置与模型实例配置分离。
- [ ] 生成器读取 `effects/hooks/symbols/patches/contracts/templates` 并参与代码生成。

### 2.2 不包含

- [ ] 第一阶段不实现在线能力包商店安装。
- [ ] 第一阶段不实现能力包热加载。
- [ ] 第一阶段不实现插件运行时动态贡献能力包。
- [ ] 第一阶段不实现复杂多包冲突自动修复。

## 3. 术语

`能力包`：内容模型生成系统的生成期扩展单元，用于声明一组可配置、可校验、可预览、可组合的生成能力。

`全局配置`：能力包自身的默认行为配置，在“能力包管理”页面维护。

`实例配置`：某个内容模型启用某个能力包时的配置，在“模型编辑 > 能力”中维护。

`运行时服务`：能力包依赖的插件或外部服务，例如评论服务插件。运行时服务不是能力包本身。

## 4. 目标目录结构

统一目录：

```text
hosts/xadmin/capability-pack/
  content.comment/
    pack.json
    global.xform.json
    instance.xform.json
    effects.json
    hooks.json
    symbols.json
    patches.json
    contracts.json
    templates/
    migrations/
    assets/
    docs/
  content.tag/
  content.seo/
```

进度：

- [ ] 创建 `hosts/xadmin/capability-pack/`。
- [ ] 创建 `content.comment` 能力包。
- [ ] 创建 `content.tag` 能力包。
- [ ] 创建 `content.seo` 能力包。
- [ ] 停止把新能力包放入 `hosts/xadmin/data/content/capabilities/`。
- [ ] 保留旧 capability 目录作为过渡读取来源。

## 5. pack.json 规范

目标字段：

```json
{
  "formatVersion": 1,
  "packId": "content.comment",
  "name": "comment",
  "title": "评论",
  "description": "为内容模型生成评论接入能力。",
  "version": "1.0.0",
  "author": "xAdmin",
  "source": "xadmin",
  "installType": "bundled",
  "status": "active",
  "update": {
    "channel": "official",
    "packageId": "content.comment",
    "canUpdate": true,
    "canUninstall": false
  },
  "protection": {
    "system": true,
    "readonly": true,
    "disableAllowed": true,
    "deleteAllowed": false
  },
  "compat": {
    "minXadminVersion": "4.0.0",
    "contentModelVersion": "1"
  },
  "requires": {
    "plugins": [],
    "packs": [],
    "features": []
  },
  "config": {
    "globalForm": "global.xform.json",
    "instanceForm": "instance.xform.json"
  },
  "effects": "effects.json",
  "hooks": "hooks.json",
  "symbols": "symbols.json",
  "patches": "patches.json",
  "contracts": "contracts.json"
}
```

进度：

- [ ] 定义 `pack.json` 解析结构。
- [ ] 校验 `packId` 唯一性。
- [ ] 校验 `formatVersion`。
- [ ] 校验 `compat`。
- [ ] 校验 `update` 字段。
- [ ] 校验 `protection` 字段。
- [ ] 校验配置文件引用是否存在。
- [ ] 校验 effects/hooks/symbols/patches/contracts 引用是否存在。

## 6. 数据库设计

### 6.1 content_pack

记录已安装能力包。

```sql
CREATE TABLE IF NOT EXISTS content_pack (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  pack_id TEXT NOT NULL UNIQUE,
  name TEXT NOT NULL DEFAULT '',
  title TEXT NOT NULL DEFAULT '',
  description TEXT NOT NULL DEFAULT '',
  version TEXT NOT NULL DEFAULT '',
  author TEXT NOT NULL DEFAULT '',
  source TEXT NOT NULL DEFAULT '',
  install_type TEXT NOT NULL DEFAULT '',
  status TEXT NOT NULL DEFAULT 'active',
  path TEXT NOT NULL DEFAULT '',
  manifest_json TEXT NOT NULL DEFAULT '{}',
  global_form_json TEXT NOT NULL DEFAULT '{}',
  instance_form_json TEXT NOT NULL DEFAULT '{}',
  effects_json TEXT NOT NULL DEFAULT '{}',
  hooks_json TEXT NOT NULL DEFAULT '{}',
  symbols_json TEXT NOT NULL DEFAULT '{}',
  patches_json TEXT NOT NULL DEFAULT '{}',
  contracts_json TEXT NOT NULL DEFAULT '{}',
  update_channel TEXT NOT NULL DEFAULT '',
  update_package_id TEXT NOT NULL DEFAULT '',
  can_update INTEGER NOT NULL DEFAULT 0,
  can_uninstall INTEGER NOT NULL DEFAULT 0,
  readonly INTEGER NOT NULL DEFAULT 0,
  system INTEGER NOT NULL DEFAULT 0,
  sort INTEGER NOT NULL DEFAULT 0,
  create_time INTEGER NOT NULL DEFAULT 0,
  update_time INTEGER NOT NULL DEFAULT 0
);
```

### 6.2 content_pack_option

记录能力包全局配置。

```sql
CREATE TABLE IF NOT EXISTS content_pack_option (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  pack_id TEXT NOT NULL UNIQUE,
  options_json TEXT NOT NULL DEFAULT '{}',
  create_time INTEGER NOT NULL DEFAULT 0,
  update_time INTEGER NOT NULL DEFAULT 0
);
```

### 6.3 content_model_pack

记录内容模型启用的能力包实例配置。

```sql
CREATE TABLE IF NOT EXISTS content_model_pack (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  model_id INTEGER NOT NULL,
  pack_id TEXT NOT NULL,
  enabled INTEGER NOT NULL DEFAULT 1,
  instance_options_json TEXT NOT NULL DEFAULT '{}',
  mount_json TEXT NOT NULL DEFAULT '{}',
  sort INTEGER NOT NULL DEFAULT 0,
  create_time INTEGER NOT NULL DEFAULT 0,
  update_time INTEGER NOT NULL DEFAULT 0,
  UNIQUE(model_id, pack_id)
);
```

进度：

- [ ] 创建 `content_pack`。
- [ ] 创建 `content_pack_option`。
- [ ] 创建 `content_model_pack`。
- [ ] 建立 `content_pack(status, sort)` 索引。
- [ ] 建立 `content_model_pack(model_id)` 索引。
- [ ] 建立 `content_model_pack(pack_id)` 索引。
- [ ] 评估旧 `content_capability` / `content_model_capability` 的过渡策略。

## 7. 后台菜单

目标菜单：

```text
内容模型
  - 模型管理
  - 能力包商店
  - 能力包管理
```

进度：

- [ ] 调整后台菜单注册。
- [ ] 原 `/admin/view/content` 作为“模型管理”。
- [ ] 新增 `/admin/view/content/pack-store`。
- [ ] 新增 `/admin/view/content/packs`。
- [ ] 保持权限体系和 page 路由机制一致。

## 8. 能力包商店页面

第一阶段只做占位。

页面要求：

- [ ] 使用 layui/xadmin 风格。
- [ ] 展示“能力包商店暂未开放”。
- [ ] 保留搜索、分类、安装状态、更新状态的页面骨架。
- [ ] 不接入真实在线接口。

## 9. 能力包管理页面

能力包管理页用于管理本机已安装能力包。

列表要求：

- [ ] 显示能力包标题、packId、版本、来源、状态。
- [ ] 显示安装类型：bundled/local/store。
- [ ] 显示是否可更新、可卸载、只读、系统保护。
- [ ] 显示安装路径。
- [ ] 支持启用/禁用。
- [ ] 支持查看详情。

详情要求：

- [ ] 展示 manifest。
- [ ] 展示全局配置表单。
- [ ] 展示 effects。
- [ ] 展示 hooks。
- [ ] 展示 symbols。
- [ ] 展示 patches。
- [ ] 展示 contracts。
- [ ] 展示依赖检查结果。
- [ ] 展示正在使用该能力包的内容模型。

配置要求：

- [ ] 根据 `global.xform.json` 渲染全局配置。
- [ ] 保存到 `content_pack_option`。
- [ ] 配置保存前执行 xform/schema 校验。

## 10. 模型编辑页能力 Tab

模型编辑页的能力 Tab 从能力包 registry 读取可用能力包。

要求：

- [ ] 左侧展示可用能力包。
- [ ] 右侧展示能力包说明、影响、依赖和实例配置。
- [ ] 根据 `instance.xform.json` 渲染实例配置。
- [ ] 保存到 `content_model_pack`。
- [ ] 同步写入现有 spec，保持生成器过渡可用。
- [ ] 生成预检中展示能力包影响。

## 11. 生成器接入

生成器必须从“能力标记”升级为“能力包装配”。

装配顺序：

1. 读取模型 DSL。
2. 读取模型启用的能力包。
3. 合并能力包全局配置。
4. 合并模型实例配置。
5. 校验 requires/conflicts/protection。
6. 合并 effects。
7. 合并 symbols。
8. 执行 hooks。
9. 渲染模板片段。
10. 执行受控 patches。
11. 输出 contracts。
12. 输出生成影响报告。

进度：

- [ ] 实现能力包装配上下文。
- [ ] 读取 `effects.json`。
- [ ] 读取 `hooks.json`。
- [ ] 读取 `symbols.json`。
- [ ] 读取 `patches.json`。
- [ ] 读取 `contracts.json`。
- [ ] 支持模板片段渲染。
- [ ] 支持生成器 symbol 白名单。
- [ ] 支持 symbol 冲突检测。
- [ ] 支持主模板 patch anchor 标记。
- [ ] 支持锚点范围内 `replace` patch。
- [ ] 支持锚点范围内 `insertBefore` patch。
- [ ] 支持锚点范围内 `insertAfter` patch。
- [ ] 支持 `replaceAnchor` patch。
- [ ] patch 失败时按 `required` 决定生成失败或警告。
- [ ] patch 应用结果写入生成报告。
- [ ] 生成预检展示 effects。
- [ ] 生成结果写入 contracts。
- [ ] 生成结果记录使用的 pack/version/options 摘要。

## 11.1 生成干预安全边界

能力包不得直接对生成文件执行任意字符串替换。

允许：

- [ ] 覆盖生成器声明的 symbol。
- [ ] 向生成器声明的 hook slot 注入模板片段。
- [ ] 对带 `@xadmin-patch-begin` / `@xadmin-patch-end` 的锚点区域做受控 patch。
- [ ] 在生成预检中展示所有 symbol 覆盖和 patch 计划。

不允许：

- [ ] 按行号 patch。
- [ ] 无 anchor patch。
- [ ] 全文件正则替换。
- [ ] patch 未开放的模板区域。
- [ ] patch 后不校验。

主模板锚点格式：

```c
/* @xadmin-patch-begin content.table.symbol */
static const char* g_content_table_name = "{{TABLE_NAME}}";
/* @xadmin-patch-end content.table.symbol */
```

patch 示例：

```json
{
  "patches": [
    {
      "id": "rename-content-table-symbol",
      "target": "generated/main.c",
      "type": "replace",
      "anchor": "content.table.symbol",
      "find": "static const char* g_content_table_name",
      "replace": "static const char* g_article_table_name",
      "required": true
    }
  ]
}
```

## 12. 当前能力迁移

当前原型：

```text
hosts/xadmin/data/content/capabilities/comment.json
hosts/xadmin/data/content/capabilities/tag.json
hosts/xadmin/data/content/capabilities/seo.json
```

迁移目标：

- [ ] `comment.json` 迁移为 `capability-pack/content.comment/`。
- [ ] `tag.json` 迁移为 `capability-pack/content.tag/`。
- [ ] `seo.json` 迁移为 `capability-pack/content.seo/`。
- [ ] `category.json` 不作为能力包迁移，栏目属于内容插件核心能力。
- [ ] 删除或降级旧硬编码 comment fallback。
- [ ] API 返回能力包 registry，而不是旧 capability registry。

## 13. 七个目标能力包开发任务

七个目标能力包必须按“可用子系统”标准实现。挂载后生成的内容插件必须具备后台管理页面、前端接口、完整配置项、数据结构、权限点、菜单入口和生成影响说明。

### 13.1 评论系统 content.comment

目录与定义：

- [ ] 创建 `capability-pack/content.comment/pack.json`。
- [ ] 创建 `global.xform.json`。
- [ ] 创建 `instance.xform.json`。
- [ ] 创建 `effects.json`。
- [ ] 创建 `hooks.json`。
- [ ] 创建 `contracts.json`。
- [ ] 创建后台页面模板片段。
- [ ] 创建前台评论区模板片段。

数据与迁移：

- [ ] 生成 `comment_thread` 表。
- [ ] 生成 `comment_item` 表。
- [ ] 生成 `comment_audit_log` 表。
- [ ] 生成评论数统计字段或统计表。
- [ ] 生成安装迁移 SQL。
- [ ] 生成卸载/清理策略说明。

后台能力：

- [ ] 生成评论列表页。
- [ ] 生成评论审核页。
- [ ] 内容详情页生成评论面板或评论入口。
- [ ] 生成评论管理菜单。
- [ ] 生成 `comment.view` 权限点。
- [ ] 生成 `comment.audit` 权限点。
- [ ] 生成 `comment.delete` 权限点。

前端接口：

- [ ] 生成评论列表 API。
- [ ] 生成发表评论 API。
- [ ] 生成评论删除/隐藏 API。
- [ ] 生成评论数量 API。

配置：

- [ ] 支持审核策略配置。
- [ ] 支持游客评论配置。
- [ ] 支持分页数量配置。
- [ ] 支持排序配置。
- [ ] 支持嵌套层级配置。

事件：

- [ ] 接入 `content.afterDelete`。
- [ ] 接入 `content.afterPublish`。
- [ ] 接入 `content.afterView`。

### 13.2 标签系统 content.tag

- [ ] 创建能力包目录和 manifest。
- [ ] 生成 `tag` 表。
- [ ] 生成 `content_tag_rel` 表。
- [ ] 生成标签管理页。
- [ ] 内容编辑页注入标签选择器。
- [ ] 内容列表页注入标签筛选。
- [ ] 生成标签列表 API。
- [ ] 生成标签内容聚合 API。
- [ ] 支持自动创建标签配置。
- [ ] 支持最大标签数量配置。
- [ ] 支持标签别名、排序、状态。
- [ ] 生成 `tag.manage` 权限点。
- [ ] 接入 `content.beforeSave`。
- [ ] 接入 `content.afterSave`。
- [ ] 接入 `content.afterDelete`。

### 13.3 专题系统 content.topic

- [ ] 创建能力包目录和 manifest。
- [ ] 生成 `topic` 表。
- [ ] 生成 `topic_content_rel` 表。
- [ ] 生成专题管理页。
- [ ] 生成专题内容管理页。
- [ ] 内容编辑页注入专题选择器。
- [ ] 内容列表页注入专题筛选。
- [ ] 生成专题列表 API。
- [ ] 生成专题详情 API。
- [ ] 生成专题内容列表 API。
- [ ] 支持专题封面、摘要、排序、状态。
- [ ] 支持单专题/多专题绑定配置。
- [ ] 生成 `topic.manage` 权限点。
- [ ] 生成 `topic.content.manage` 权限点。
- [ ] 接入 `content.afterPublish`。
- [ ] 接入 `content.afterDelete`。

### 13.4 敏感词过滤 content.sensitive

- [ ] 创建能力包目录和 manifest。
- [ ] 生成 `sensitive_word` 表。
- [ ] 生成 `sensitive_hit_log` 表。
- [ ] 生成敏感词库管理页。
- [ ] 生成过滤日志页。
- [ ] 内容保存前执行检测。
- [ ] 内容发布前执行检测。
- [ ] 评论提交前执行检测。
- [ ] 生成前端检测结果 API。
- [ ] 支持阻断、替换、标记待审、仅记录策略。
- [ ] 支持作用字段配置。
- [ ] 支持替换符配置。
- [ ] 支持命中日志配置。
- [ ] 生成 `sensitive.manage` 权限点。
- [ ] 生成 `sensitive.log.view` 权限点。
- [ ] 接入 `content.beforeSave`。
- [ ] 接入 `content.beforePublish`。
- [ ] 接入 `comment.beforeCreate`。

### 13.5 静态化生成 content.static

- [ ] 创建能力包目录和 manifest。
- [ ] 生成 `static_rule` 表。
- [ ] 生成 `static_job` 表。
- [ ] 生成 `static_file` 表。
- [ ] 生成静态化任务页。
- [ ] 生成静态化规则配置页。
- [ ] 内容详情页注入重新生成按钮。
- [ ] 生成手动生成 API。
- [ ] 生成增量生成 API。
- [ ] 生成清理静态文件 API。
- [ ] 生成任务状态查询 API。
- [ ] 支持发布后自动生成配置。
- [ ] 支持删除后自动清理配置。
- [ ] 支持输出目录、URL 策略、模板策略配置。
- [ ] 生成 `static.generate` 权限点。
- [ ] 生成 `static.clean` 权限点。
- [ ] 接入 `content.afterPublish`。
- [ ] 接入 `content.afterDelete`。
- [ ] 接入 `topic.afterPublish`。
- [ ] 接入 `tag.afterUpdate`。
- [ ] 接入任务创建、重试、状态、日志能力。

### 13.6 点赞系统 content.like

- [ ] 创建能力包目录和 manifest。
- [ ] 生成 `like_record` 表。
- [ ] 生成 `like_counter` 表。
- [ ] 生成点赞记录页。
- [ ] 内容列表页注入点赞数列。
- [ ] 内容详情页注入点赞统计。
- [ ] 生成点赞 API。
- [ ] 生成取消点赞 API。
- [ ] 生成点赞状态查询 API。
- [ ] 支持游客点赞配置。
- [ ] 支持会员、IP、Cookie、设备标识防重复策略。
- [ ] 生成 `like.view` 权限点。
- [ ] 生成 `like.manage` 权限点。
- [ ] 接入 `content.afterDelete`。
- [ ] 接入 `like.afterCreate`。
- [ ] 接入 `like.afterCancel`。

### 13.7 访问量统计 content.view-stat

- [ ] 创建能力包目录和 manifest。
- [ ] 生成 `view_counter` 表。
- [ ] 生成 `view_log` 表。
- [ ] 生成 `view_daily_stat` 表。
- [ ] 生成访问统计概览页。
- [ ] 生成内容访问排行页。
- [ ] 内容列表页注入访问量列。
- [ ] 生成访问计数 API。
- [ ] 生成前台排行 API。
- [ ] 生成内容详情访问量展示 API。
- [ ] 支持每次访问、IP 去重、会话去重、按天去重策略。
- [ ] 支持是否记录明细日志配置。
- [ ] 支持统计周期配置。
- [ ] 生成 `view_stat.view` 权限点。
- [ ] 生成 `view_stat.export` 权限点。
- [ ] 接入 `content.afterView`。
- [ ] 接入 `content.afterDelete`。
- [ ] 接入统计汇总任务。
- [ ] 接入日志清理任务。

## 14. 能力包底座补充任务

为支撑七个目标能力包，底座必须补齐以下通用能力。

事件系统：

- [ ] 定义内容生命周期事件。
- [ ] 定义能力包扩展事件。
- [ ] 生成器支持事件 hook 注入。
- [ ] 生成插件运行时能触发事件。
- [ ] 事件处理失败有明确策略：阻断、警告、忽略。

任务系统：

- [ ] 能力包可声明异步任务。
- [ ] 生成任务表结构。
- [ ] 生成任务创建接口。
- [ ] 生成任务状态接口。
- [ ] 生成任务日志接口。
- [ ] 支持任务失败重试。
- [ ] 支持静态化和访问统计汇总场景。

后台扩展：

- [ ] 支持能力包声明后台菜单。
- [ ] 支持能力包声明后台页面。
- [ ] 支持能力包注入列表列。
- [ ] 支持能力包注入筛选项。
- [ ] 支持能力包注入详情面板。
- [ ] 支持能力包注入表单字段。

前台扩展：

- [ ] 支持能力包声明前台 API。
- [ ] 支持能力包声明前台详情页挂载区域。
- [ ] 支持能力包声明前台列表页挂载区域。
- [ ] 支持能力包声明静态资源。

权限与菜单：

- [ ] 能力包可声明权限点。
- [ ] 生成插件注册能力包权限点。
- [ ] 生成插件菜单绑定能力包权限点。

配置与预检：

- [ ] 能力包全局配置进入能力包管理页。
- [ ] 能力包实例配置进入模型编辑页。
- [ ] 生成预检展示七类能力包的表、路由、页面、权限、任务影响。
- [ ] 能力包冲突和依赖在预检阶段阻断。

## 15. 验收标准

- [ ] 内容模型菜单显示为目录，并包含三个页面。
- [ ] 能力包商店页面可访问但明确为占位。
- [ ] 能力包管理页面能列出本地能力包。
- [ ] 能力包详情能展示 manifest/effects/hooks/symbols/patches/contracts。
- [ ] 能力包全局配置可保存。
- [ ] 模型编辑页能选择能力包并渲染实例配置。
- [ ] 生成预检能说明能力包带来的影响。
- [ ] 生成预检能说明 symbol 覆盖和 patch 计划。
- [ ] patch 失败时能给出明确错误。
- [ ] 生成插件中能看到能力包真实参与生成，而不是只有空 contracts。
- [ ] comment 能力包来源清晰可见。
- [ ] 禁用能力包后，模型编辑页不可继续选择该能力包。
- [ ] 七个目标能力包均生成可访问的后台管理页面。
- [ ] 七个目标能力包均生成对应前端 API。
- [ ] 七个目标能力包均具备全局配置和模型实例配置。
- [ ] 七个目标能力包均在生成预检中展示明确影响。
- [ ] 七个目标能力包不是占位入口，至少能完成核心数据读写闭环。

## 16. 实施阶段

### Phase 1：文档与规格

- [x] 明确能力包不是插件附属物。
- [x] 明确能力包是统一安装实体。
- [x] 明确菜单结构。
- [x] 明确商店占位与管理页职责。
- [x] 明确全局配置和实例配置分层。
- [x] 生成可跟踪 SPEC。

### Phase 2：registry 与目录

- [ ] 新建能力包目录。
- [ ] 新建 registry 表。
- [ ] 实现目录扫描。
- [ ] 实现 manifest 校验。
- [ ] 同步 registry。

### Phase 3：后台页面

- [ ] 调整菜单。
- [ ] 建立商店占位页。
- [ ] 建立能力包管理列表页。
- [ ] 建立能力包详情页。
- [ ] 接入全局配置表单。

### Phase 4：模型编辑页接入

- [ ] 能力 Tab 改读能力包 registry。
- [ ] 接入实例配置表单。
- [ ] 保存 `content_model_pack`。
- [ ] 同步旧 spec 过渡字段。

### Phase 5：生成器装配

- [ ] effects 接入预检。
- [ ] hooks 接入模板渲染。
- [ ] symbols 接入模板渲染。
- [ ] patches 接入受控补丁。
- [ ] contracts 接入生成输出。
- [ ] comment 能力包形成端到端闭环。

### Phase 6：清理旧 capability

- [ ] 移除旧 capability JSON 依赖。
- [ ] 移除旧硬编码 comment fallback。
- [ ] 更新旧文档引用。
- [ ] 完成浏览器验收。

### Phase 7：底座补齐

- [ ] 实现内容生命周期事件。
- [ ] 实现能力包扩展事件。
- [ ] 实现异步任务声明和生成。
- [ ] 实现后台菜单/页面/列表/表单/详情 surface。
- [ ] 实现前台 API 和前台页面 surface。
- [ ] 实现能力包权限点生成。
- [ ] 实现能力包依赖和冲突预检。

### Phase 8：七个目标能力包

- [ ] 完成 `content.comment` 评论系统能力包。
- [ ] 完成 `content.tag` 标签系统能力包。
- [ ] 完成 `content.topic` 专题系统能力包。
- [ ] 完成 `content.sensitive` 敏感词过滤能力包。
- [ ] 完成 `content.static` 静态化生成能力包。
- [ ] 完成 `content.like` 点赞系统能力包。
- [ ] 完成 `content.view-stat` 访问量统计能力包。
- [ ] 每个能力包完成后台页、前端 API、配置、数据结构、权限、菜单和生成影响验收。
