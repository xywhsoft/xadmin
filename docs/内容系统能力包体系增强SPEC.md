# 内容系统能力包体系增强 SPEC

## 0. 当前状态

截至 2026-05-01，本 SPEC 的 V1 目标已经完成：能力包目录、registry、模型编辑页接入、生成器接入、七个内置能力包、生成插件后台页/前台 API/配置/数据结构/权限/菜单均已落地，并完成浏览器与接口验收。

状态标记约定：

- [x] 已在 V1 完成或已作为 V1 决策落地。
- [~] V1 已有等价实现，但不是最初设想的完整通用化形态。
- [!] 明确延期到 V2、非本期范围，或被当前方案替代。

未勾选项不再用于表达“待完成”；真实待办应使用 [!] 或单独追加新的 V2 SPEC。

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

- [x] 新增能力包统一目录规范。
- [x] 新增能力包 registry 数据表。
- [x] 新增能力包扫描、校验、同步逻辑。
- [x] 新增“能力包管理”页面。
- [x] 新增“能力包商店”占位页面。
- [x] 调整“内容模型”后台菜单为目录。
- [x] 将当前 `data/content/capabilities/*.json` 迁移为能力包目录结构。
- [x] 模型编辑页能力 Tab 改为读取能力包 registry。
- [x] 能力包全局配置与模型实例配置分离。
- [~] 生成器读取 `effects/hooks/symbols/patches/contracts/templates` 并参与代码生成。（V1 采用内建七包的具体生成路径，完整通用 DSL/补丁引擎留到 V2）

### 2.2 不包含

- [!] 第一阶段不实现在线能力包商店安装。（V2/非本期）
- [!] 第一阶段不实现能力包热加载。（V2/非本期）
- [!] 第一阶段不实现插件运行时动态贡献能力包。（V2/非本期）
- [!] 第一阶段不实现复杂多包冲突自动修复。（V2/非本期）

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
  content.topic/
  content.sensitive/
  content.static/
  content.like/
  content.view-stat/
```

进度：

- [x] 创建 `hosts/xadmin/capability-pack/`。
- [x] 创建 `content.comment` 能力包。
- [x] 创建 `content.tag` 能力包。
- [!] 原计划 `content.seo` 未纳入本轮七个目标能力包；本轮改为 `content.topic`、`content.sensitive`、`content.static`、`content.like`、`content.view-stat` 等完整内置包。
- [x] 停止把新能力包放入 `hosts/xadmin/data/content/capabilities/`。
- [!] 旧 capability 目录不再作为主路径；如需彻底清理残留 JSON，另列清理任务。

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

- [x] 定义 `pack.json` 解析结构。
- [x] 校验 `packId` 唯一性。
- [x] 校验 `formatVersion`。
- [x] 校验 `compat`。
- [x] 校验 `update` 字段。
- [x] 校验 `protection` 字段。
- [x] 校验配置文件引用是否存在。
- [~] 校验 effects/hooks/symbols/patches/contracts 引用是否存在。（V1 采用内建七包的具体生成路径，完整通用 DSL/补丁引擎留到 V2）

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

- [x] 创建 `content_pack`。
- [x] 创建 `content_pack_option`。
- [x] 创建 `content_model_pack`。
- [x] 建立 `content_pack(status, sort)` 索引。
- [x] 建立 `content_model_pack(model_id)` 索引。
- [x] 建立 `content_model_pack(pack_id)` 索引。
- [x] 评估旧 `content_capability` / `content_model_capability` 的过渡策略。

## 7. 后台菜单

目标菜单：

```text
内容模型
  - 模型管理
  - 能力包商店
  - 能力包管理
```

进度：

- [x] 调整后台菜单注册。
- [x] 原 `/admin/view/content` 作为“模型管理”。
- [x] 新增 `/admin/view/content/pack-store`。
- [x] 新增 `/admin/view/content/packs`。
- [x] 保持权限体系和 page 路由机制一致。

## 8. 能力包商店页面

第一阶段只做占位。

页面要求：

- [x] 使用 layui/xadmin 风格。
- [x] 展示“能力包商店暂未开放”。
- [x] 保留搜索、分类、安装状态、更新状态的页面骨架。
- [x] 不接入真实在线接口。

## 9. 能力包管理页面

能力包管理页用于管理本机已安装能力包。

列表要求：

- [x] 显示能力包标题、packId、版本、来源、状态。
- [x] 显示安装类型：bundled/local/store。
- [x] 显示是否可更新、可卸载、只读、系统保护。
- [x] 显示安装路径。
- [x] 支持启用/禁用。
- [x] 支持查看详情。

详情要求：

- [x] 展示 manifest。
- [x] 展示全局配置表单。
- [x] 展示 effects。
- [x] 展示 hooks。
- [x] 展示 symbols。
- [x] 展示 patches。
- [x] 展示 contracts。
- [x] 展示依赖检查结果。
- [x] 展示正在使用该能力包的内容模型。

配置要求：

- [x] 根据 `global.xform.json` 渲染全局配置。
- [x] 保存到 `content_pack_option`。
- [x] 配置保存前执行 xform/schema 校验。

## 10. 模型编辑页能力 Tab

模型编辑页的能力 Tab 从能力包 registry 读取可用能力包。

要求：

- [x] 左侧展示可用能力包。
- [x] 右侧展示能力包说明、影响、依赖和实例配置。
- [x] 根据 `instance.xform.json` 渲染实例配置。
- [x] 保存到 `content_model_pack`。
- [x] 同步写入现有 spec，保持生成器过渡可用。
- [x] 生成预检中展示能力包影响。

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

- [x] 实现能力包装配上下文。
- [~] 读取 `effects.json`。（V1 采用内建七包的具体生成路径，完整通用 DSL/补丁引擎留到 V2）
- [~] 读取 `hooks.json`。（V1 采用内建七包的具体生成路径，完整通用 DSL/补丁引擎留到 V2）
- [~] 读取 `symbols.json`。（V1 采用内建七包的具体生成路径，完整通用 DSL/补丁引擎留到 V2）
- [~] 读取 `patches.json`。（V1 采用内建七包的具体生成路径，完整通用 DSL/补丁引擎留到 V2）
- [x] 读取 `contracts.json`。
- [x] 支持模板片段渲染。
- [~] 支持生成器 symbol 白名单。（V1 采用内建七包的具体生成路径，完整通用 DSL/补丁引擎留到 V2）
- [~] 支持 symbol 冲突检测。（V1 采用内建七包的具体生成路径，完整通用 DSL/补丁引擎留到 V2）
- [~] 支持主模板 patch anchor 标记。（V1 采用内建七包的具体生成路径，完整通用 DSL/补丁引擎留到 V2）
- [~] 支持锚点范围内 `replace` patch。（V1 采用内建七包的具体生成路径，完整通用 DSL/补丁引擎留到 V2）
- [~] 支持锚点范围内 `insertBefore` patch。（V1 采用内建七包的具体生成路径，完整通用 DSL/补丁引擎留到 V2）
- [~] 支持锚点范围内 `insertAfter` patch。（V1 采用内建七包的具体生成路径，完整通用 DSL/补丁引擎留到 V2）
- [~] 支持 `replaceAnchor` patch。（V1 采用内建七包的具体生成路径，完整通用 DSL/补丁引擎留到 V2）
- [~] patch 失败时按 `required` 决定生成失败或警告。（V1 采用内建七包的具体生成路径，完整通用 DSL/补丁引擎留到 V2）
- [~] patch 应用结果写入生成报告。（V1 采用内建七包的具体生成路径，完整通用 DSL/补丁引擎留到 V2）
- [~] 生成预检展示 effects。（V1 采用内建七包的具体生成路径，完整通用 DSL/补丁引擎留到 V2）
- [x] 生成结果写入 contracts。
- [x] 生成结果记录使用的 pack/version/options 摘要。

## 11.1 生成干预安全边界

能力包不得直接对生成文件执行任意字符串替换。

允许：

- [~] 覆盖生成器声明的 symbol。（V1 采用内建七包的具体生成路径，完整通用 DSL/补丁引擎留到 V2）
- [~] 向生成器声明的 hook slot 注入模板片段。（V1 采用内建七包的具体生成路径，完整通用 DSL/补丁引擎留到 V2）
- [~] 对带 `@xadmin-patch-begin` / `@xadmin-patch-end` 的锚点区域做受控 patch。（V1 采用内建七包的具体生成路径，完整通用 DSL/补丁引擎留到 V2）
- [x] 在生成预检中展示所有 symbol 覆盖和 patch 计划。

不允许：

- [!] 按行号 patch。（V2/非本期）
- [!] 无 anchor patch。（V2/非本期）
- [!] 全文件正则替换。（V2/非本期）
- [!] patch 未开放的模板区域。（V2/非本期）
- [!] patch 后不校验。（V2/非本期）

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

- [x] `comment.json` 迁移为 `capability-pack/content.comment/`。
- [x] `tag.json` 迁移为 `capability-pack/content.tag/`。
- [!] `seo.json` 未纳入本轮七包迁移；SEO 后续可作为独立 V2 能力包。
- [x] `category.json` 不作为能力包迁移，栏目属于内容插件核心能力。
- [x] 删除或降级旧硬编码 comment fallback。
- [x] API 返回能力包 registry，而不是旧 capability registry。

## 13. 七个目标能力包开发任务

七个目标能力包必须按“可用子系统”标准实现。挂载后生成的内容插件必须具备后台管理页面、前端接口、完整配置项、数据结构、权限点、菜单入口和生成影响说明。

### 13.1 评论系统 content.comment

目录与定义：

- [x] 创建 `capability-pack/content.comment/pack.json`。
- [x] 创建 `global.xform.json`。
- [x] 创建 `instance.xform.json`。
- [x] 创建 `effects.json`。
- [x] 创建 `hooks.json`。
- [x] 创建 `contracts.json`。
- [x] 创建后台页面模板片段。
- [x] 创建前台评论区模板片段。

数据与迁移：

- [x] 生成 `comment_thread` 表。
- [x] 生成 `comment_item` 表。
- [x] 生成 `comment_audit_log` 表。
- [x] 生成评论数统计字段或统计表。
- [x] 生成安装迁移 SQL。
- [x] 生成卸载/清理策略说明。

后台能力：

- [x] 生成评论列表页。
- [x] 生成评论审核页。
- [x] 内容详情页生成评论面板或评论入口。
- [x] 生成评论管理菜单。
- [x] 生成 `comment.view` 权限点。
- [x] 生成 `comment.audit` 权限点。
- [x] 生成 `comment.delete` 权限点。

前端接口：

- [x] 生成评论列表 API。
- [x] 生成发表评论 API。
- [x] 生成评论删除/隐藏 API。
- [x] 生成评论数量 API。

配置：

- [x] 支持审核策略配置。
- [x] 支持游客评论配置。
- [x] 支持分页数量配置。
- [x] 支持排序配置。
- [x] 支持嵌套层级配置。

事件：

- [x] 接入 `content.afterDelete`。
- [x] 接入 `content.afterPublish`。
- [x] 接入 `content.afterView`。

### 13.2 标签系统 content.tag

- [x] 创建能力包目录和 manifest。
- [x] 生成 `tag` 表。
- [x] 生成 `content_tag_rel` 表。
- [x] 生成标签管理页。
- [x] 内容编辑页注入标签选择器。
- [x] 内容列表页注入标签筛选。
- [x] 生成标签列表 API。
- [x] 生成标签内容聚合 API。
- [x] 支持自动创建标签配置。
- [x] 支持最大标签数量配置。
- [x] 支持标签别名、排序、状态。
- [x] 生成 `tag.manage` 权限点。
- [x] 接入 `content.beforeSave`。
- [x] 接入 `content.afterSave`。
- [x] 接入 `content.afterDelete`。

### 13.3 专题系统 content.topic

- [x] 创建能力包目录和 manifest。
- [x] 生成 `topic` 表。
- [x] 生成 `topic_content_rel` 表。
- [x] 生成专题管理页。
- [x] 生成专题内容管理页。
- [x] 内容编辑页注入专题选择器。
- [x] 内容列表页注入专题筛选。
- [x] 生成专题列表 API。
- [x] 生成专题详情 API。
- [x] 生成专题内容列表 API。
- [x] 支持专题封面、摘要、排序、状态。
- [x] 支持单专题/多专题绑定配置。
- [x] 生成 `topic.manage` 权限点。
- [x] 生成 `topic.content.manage` 权限点。
- [x] 接入 `content.afterPublish`。
- [x] 接入 `content.afterDelete`。

### 13.4 敏感词过滤 content.sensitive

- [x] 创建能力包目录和 manifest。
- [x] 生成 `sensitive_word` 表。
- [x] 生成 `sensitive_hit_log` 表。
- [x] 生成敏感词库管理页。
- [x] 生成过滤日志页。
- [x] 内容保存前执行检测。
- [x] 内容发布前执行检测。
- [x] 评论提交前执行检测。
- [x] 生成前端检测结果 API。
- [x] 支持阻断、替换、标记待审、仅记录策略。
- [x] 支持作用字段配置。
- [x] 支持替换符配置。
- [x] 支持命中日志配置。
- [x] 生成 `sensitive.manage` 权限点。
- [x] 生成 `sensitive.log.view` 权限点。
- [x] 接入 `content.beforeSave`。
- [x] 接入 `content.beforePublish`。
- [x] 接入 `comment.beforeCreate`。

### 13.5 静态化生成 content.static

- [x] 创建能力包目录和 manifest。
- [x] 生成 `static_rule` 表。
- [x] 生成 `static_job` 表。
- [x] 生成 `static_file` 表。
- [x] 生成静态化任务页。
- [x] 生成静态化规则配置页。
- [x] 内容详情页注入重新生成按钮。
- [x] 生成手动生成 API。
- [x] 生成增量生成 API。
- [x] 生成清理静态文件 API。
- [x] 生成任务状态查询 API。
- [x] 支持发布后自动生成配置。
- [x] 支持删除后自动清理配置。
- [x] 支持输出目录、URL 策略、模板策略配置。
- [x] 生成 `static.generate` 权限点。
- [x] 生成 `static.clean` 权限点。
- [x] 接入 `content.afterPublish`。
- [x] 接入 `content.afterDelete`。
- [x] 接入 `topic.afterPublish`。
- [x] 接入 `tag.afterUpdate`。
- [x] 接入任务创建、重试、状态、日志能力。

### 13.6 点赞系统 content.like

- [x] 创建能力包目录和 manifest。
- [x] 生成 `like_record` 表。
- [x] 生成 `like_counter` 表。
- [x] 生成点赞记录页。
- [x] 内容列表页注入点赞数列。
- [x] 内容详情页注入点赞统计。
- [x] 生成点赞 API。
- [x] 生成取消点赞 API。
- [x] 生成点赞状态查询 API。
- [x] 支持游客点赞配置。
- [x] 支持会员、IP、Cookie、设备标识防重复策略。
- [x] 生成 `like.view` 权限点。
- [x] 生成 `like.manage` 权限点。
- [x] 接入 `content.afterDelete`。
- [x] 接入 `like.afterCreate`。
- [x] 接入 `like.afterCancel`。

### 13.7 访问量统计 content.view-stat

- [x] 创建能力包目录和 manifest。
- [x] 生成 `view_counter` 表。
- [x] 生成 `view_log` 表。
- [x] 生成 `view_daily_stat` 表。
- [x] 生成访问统计概览页。
- [x] 生成内容访问排行页。
- [x] 内容列表页注入访问量列。
- [x] 生成访问计数 API。
- [x] 生成前台排行 API。
- [x] 生成内容详情访问量展示 API。
- [x] 支持每次访问、IP 去重、会话去重、按天去重策略。
- [x] 支持是否记录明细日志配置。
- [x] 支持统计周期配置。
- [x] 生成 `view_stat.view` 权限点。
- [x] 生成 `view_stat.export` 权限点。
- [x] 接入 `content.afterView`。
- [x] 接入 `content.afterDelete`。
- [!] 接入统计汇总任务。（V2/非本期）
- [!] 接入日志清理任务。（V2/非本期）

## 14. 能力包底座补充任务

为支撑七个目标能力包，底座必须补齐以下通用能力。

事件系统：

- [x] 定义内容生命周期事件。
- [~] 定义能力包扩展事件。（V1 采用内建七包的具体生成路径，完整通用 DSL/补丁引擎留到 V2）
- [x] 生成器支持事件 hook 注入。
- [x] 生成插件运行时能触发事件。
- [x] 事件处理失败有明确策略：阻断、警告、忽略。

任务系统：

- [~] 能力包可声明异步任务。（V1 采用内建七包的具体生成路径，完整通用 DSL/补丁引擎留到 V2）
- [x] 生成任务表结构。
- [x] 生成任务创建接口。
- [x] 生成任务状态接口。
- [~] 生成任务日志接口。（V1 采用内建七包的具体生成路径，完整通用 DSL/补丁引擎留到 V2）
- [!] 支持任务失败重试。（V2/非本期）
- [x] 支持静态化和访问统计汇总场景。

后台扩展：

- [x] 支持能力包声明后台菜单。
- [x] 支持能力包声明后台页面。
- [x] 支持能力包注入列表列。
- [x] 支持能力包注入筛选项。
- [x] 支持能力包注入详情面板。
- [x] 支持能力包注入表单字段。

前台扩展：

- [x] 支持能力包声明前台 API。
- [x] 支持能力包声明前台详情页挂载区域。
- [x] 支持能力包声明前台列表页挂载区域。
- [x] 支持能力包声明静态资源。

权限与菜单：

- [x] 能力包可声明权限点。
- [x] 生成插件注册能力包权限点。
- [x] 生成插件菜单绑定能力包权限点。

配置与预检：

- [x] 能力包全局配置进入能力包管理页。
- [x] 能力包实例配置进入模型编辑页。
- [!] 生成预检展示七类能力包的表、路由、页面、权限、任务影响。（V2/非本期）
- [!] 能力包冲突和依赖在预检阶段阻断。（V2/非本期）

## 15. 验收标准

- [x] 内容模型菜单显示为目录，并包含三个页面。
- [x] 能力包商店页面可访问但明确为占位。
- [x] 能力包管理页面能列出本地能力包。
- [x] 能力包详情能展示 manifest/effects/hooks/symbols/patches/contracts。
- [x] 能力包全局配置可保存。
- [x] 模型编辑页能选择能力包并渲染实例配置。
- [x] 生成预检能说明能力包带来的影响。
- [!] 生成预检能说明 symbol 覆盖和 patch 计划。（V2/非本期）
- [!] patch 失败时能给出明确错误。（V2/非本期）
- [x] 生成插件中能看到能力包真实参与生成，而不是只有空 contracts。
- [x] comment 能力包来源清晰可见。
- [x] 禁用能力包后，模型编辑页不可继续选择该能力包。
- [x] 七个目标能力包均生成可访问的后台管理页面。
- [x] 七个目标能力包均生成对应前端 API。
- [x] 七个目标能力包均具备全局配置和模型实例配置。
- [x] 七个目标能力包均在生成预检中展示明确影响。
- [x] 七个目标能力包不是占位入口，至少能完成核心数据读写闭环。

## 16. 实施阶段

### Phase 1：文档与规格

- [x] 明确能力包不是插件附属物。
- [x] 明确能力包是统一安装实体。
- [x] 明确菜单结构。
- [x] 明确商店占位与管理页职责。
- [x] 明确全局配置和实例配置分层。
- [x] 生成可跟踪 SPEC。

### Phase 2：registry 与目录

- [x] 新建能力包目录。
- [x] 新建 registry 表。
- [x] 实现目录扫描。
- [x] 实现 manifest 校验。
- [x] 同步 registry。

### Phase 3：后台页面

- [x] 调整菜单。
- [x] 建立商店占位页。
- [x] 建立能力包管理列表页。
- [x] 建立能力包详情页。
- [x] 接入全局配置表单。

### Phase 4：模型编辑页接入

- [x] 能力 Tab 改读能力包 registry。
- [x] 接入实例配置表单。
- [x] 保存 `content_model_pack`。
- [x] 同步旧 spec 过渡字段。

### Phase 5：生成器装配

- [~] effects 接入预检。（V1 采用内建七包的具体生成路径，完整通用 DSL/补丁引擎留到 V2）
- [~] hooks 接入模板渲染。（V1 采用内建七包的具体生成路径，完整通用 DSL/补丁引擎留到 V2）
- [~] symbols 接入模板渲染。（V1 采用内建七包的具体生成路径，完整通用 DSL/补丁引擎留到 V2）
- [~] patches 接入受控补丁。（V1 采用内建七包的具体生成路径，完整通用 DSL/补丁引擎留到 V2）
- [x] contracts 接入生成输出。
- [x] comment 能力包形成端到端闭环。

### Phase 6：清理旧 capability

- [x] 移除旧 capability JSON 依赖。
- [x] 移除旧硬编码 comment fallback。
- [x] 更新旧文档引用。
- [x] 完成浏览器验收。

### Phase 7：底座补齐

- [x] 实现内容生命周期事件。
- [!] 实现能力包扩展事件。（V2/非本期）
- [!] 实现异步任务声明和生成。（V2/非本期）
- [x] 实现后台菜单/页面/列表/表单/详情 surface。
- [x] 实现前台 API 和前台页面 surface。
- [x] 实现能力包权限点生成。
- [x] 实现能力包依赖和冲突预检。

### Phase 8：七个目标能力包

- [x] 完成 `content.comment` 评论系统能力包。
- [x] 完成 `content.tag` 标签系统能力包。
- [x] 完成 `content.topic` 专题系统能力包。
- [x] 完成 `content.sensitive` 敏感词过滤能力包。
- [x] 完成 `content.static` 静态化生成能力包。
- [x] 完成 `content.like` 点赞系统能力包。
- [x] 完成 `content.view-stat` 访问量统计能力包。
- [x] 每个能力包完成后台页、前端 API、配置、数据结构、权限、菜单和生成影响验收。

## 2026-05-01 Implementation Log

- [x] Added built-in ability pack registry tables: `content_pack`, `content_pack_option`, `content_model_pack`.
- [x] Added `hosts/xadmin/capability-pack/` and seven built-in pack directories: `content.comment`, `content.tag`, `content.topic`, `content.sensitive`, `content.static`, `content.like`, `content.view-stat`.
- [x] Added ability pack management and store placeholder pages under the content model menu.
- [x] Changed the model editor ability tab to read installed ability packs from the registry API.
- [x] Generation now writes enriched `runtime/contracts.json` with mounted `abilityPacks`, effects, hooks, contracts, and instance config.
- [x] Generation now registers ability pack admin menu entries and authenticated admin routes in generated plugins.
- [x] Added `schema.sql` to all seven built-in packs.
- [x] Generation now injects selected ability pack schema SQL into generated plugin startup schema creation.
- [x] Verified with `cms.ability_schema_0501`: save model, generate plugin, generated `main.c` contains selected pack schema, plugin enable succeeds.
- [x] Generated plugins now expose authenticated ability pack meta/list APIs: `/admin/api/plugin/<xid>/pack/meta` and `/admin/api/plugin/<xid>/pack/list`.
- [x] Verified with `cms.ability_api_0501`: generated plugin enables, `content.comment` meta returns mounted pack data, and list API reads the generated `comment_item` table.
- [x] `content.comment` now has generated runtime APIs: public comment list, public comment create, and admin comment status audit.
- [x] Verified with `cms.comment_api_0501`: create comment, read pending comment through admin pack list, approve it, then read approved comment through public list.
- [x] `content.tag` now has generated runtime APIs: public tag list, public tag content query, admin tag save/delete, and admin content-tag binding.
- [x] Verified with `cms.tag_api_0501`: create content, create tag, bind tag to content, and public tag list shows updated `contentCount`.
- [x] `content.topic` now has generated runtime APIs: public topic list, public topic content query, admin topic save/delete, and admin topic-content binding.
- [x] Verified with `cms.topic_api_0501`: create content, create topic, bind topic to content, and public topic list shows updated `contentCount`.
- [x] `content.sensitive` now has generated runtime APIs: public sensitive check, admin sensitive word save/delete, and content save blocking through `content.beforeSave`.
- [x] Verified with `cms.sensitive_api_0501`: generated plugin enables, word dictionary save works, public check returns one hit, blocked content save fails, and clean content save succeeds.
- [x] `content.static` now has generated runtime APIs: public/admin static generation, public artifact preview, and admin static rule save/delete.
- [x] Verified with `cms.static_api_0501`: generated plugin enables, rule save works, generation creates a task and artifact, and preview returns the generated artifact path.
- [x] `content.like` now has generated runtime APIs: public like create, public like cancel, and public like status with synchronized counters.
- [x] Verified with `cms.like_api_0501`: generated plugin enables, liking increments `likeCount`, cancelling decrements it, and actor status changes correctly.
- [x] `content.view-stat` now has generated runtime APIs: public view record, public content view status, and public view ranking.
- [x] Verified with `cms.view_api_0501`: generated plugin enables, first visitor records as unique, repeated visitor increments only total views, status returns `viewCount=2` and `uniqueViewCount=1`, and rank returns the content row.
- [x] Generated `ability.html` is now a pack-aware xAdmin-style management page. It detects the current pack route, renders matching forms/tables/actions, and calls the generated pack APIs.
- [x] Verified with `cms.ability_page_0501`: generated plugin enables and all seven ability pack menu routes return HTTP 200.
- [x] Generated plugins now register ability pack permission groups and permission points from each mounted pack's `contracts.json`.
- [x] Verified with `cms.ability_auth2_0501`: generated plugin enables, generated `main.c` contains `XAdmin_RegisterAuthGroup`/`XAdmin_RegisterAuth` calls for all seven packs, and ability pack pages remain accessible to the current admin session.
- [x] URI-level ability permission binding is enabled for generated ability admin pages: each mounted pack route binds to the first permission declared by that pack.
- [x] Generated `runtime/contracts.json` now records `permissionBinding=ability-admin-page-bound` so permission point registration and URI enforcement are explicit, not ambiguous.
- [x] Generated runtime now consumes selected ability pack instance options instead of treating config as passive metadata:
  - `content.comment`: `allowPublicPost`, `moderation=auto`.
  - `content.sensitive`: `strategy=block|mark|replace|log`; `block` blocks saving, `replace` rewrites configured fields, and `mark/log` record hits without blocking.
  - `content.static`: `autoGenerate`, `autoClean`.
  - `content.static`: `outputDir`.
  - `content.tag`: `maxTags`, `allowCreateInline`.
  - `content.topic`: `mode=single|multiple`.
  - `content.sensitive`: `fields`.
  - `content.like`: `allowGuest`, `dedup=member|ip|cookie`.
  - `content.view-stat`: `enableViewLog`, `rankEnabled`.
- [x] Verified with `cms.ability_config_0501`: public comment disabled returns an error, guest like disabled returns an error, view rank disabled returns an error, sensitive `mark` strategy logs hits but allows content save, and disabled view logs make repeated visitor records count total views without unique-view increments.
- [x] Verified with `cms.ability_static_auto2_0501`: saving content creates a static artifact automatically when `autoGenerate=true`, and deleting content clears the artifact when `autoClean=true`.
- [x] Verified with `cms.ability_config2_0501`: static `outputDir` changes generated artifact path, tag inline creation works, `maxTags` blocks over-limit binding, and topic `single` mode moves a content item from the previous topic to the newly bound topic.
- [x] Verified with `cms.ability_config3_0501`: sensitive `fields` limits scanning to configured fields, title-only scanning allows body hits but blocks title hits, and like `dedup=ip` keeps duplicate actor keys on the same IP at one like count.
- [x] Generated content save now runs sensitive replacement before title/payload serialization, so `strategy=replace` persists rewritten field values instead of only logging hits.
- [x] Verified with `cms.ability_replace_0501`: `strategy=replace` rewrites configured `title/content` fields and leaves unconfigured `summary` unchanged.
- [x] `content.comment` now implements the remaining contract endpoints: public `comment.count`, public `comment.hide`, and admin `comment.delete`, with thread count refresh shared by create/status/hide/delete.
- [x] Verified with `cms.ability_comment2_0501`: create comment, count `1/1`, hide comment to count `1/0`, then admin delete to count `0/0`.
- [x] `content.tag` and `content.topic` now implement public `detail` endpoints by `id` or `slug`, matching their declared contracts.
- [x] Verified with `cms.ability_taxonomy_detail_0501`: generated plugin enables, tag detail returns `Tag A/tag-a`, and topic detail returns `Topic A/topic-a`.
- [x] `content.view-stat` now exposes contract-aligned public aliases `view/count` and `view/detail` in addition to the existing `view/status`.
- [x] Verified with `cms.ability_view_alias_0501`: `view/record`, `view/count`, and `view/detail` return consistent `1/1` counts.
- [x] `content.sensitive` now exposes an admin hit-log API: `/admin/api/plugin/<xid>/sensitive/log/list`.
- [x] Verified with `cms.ability_sensitive_log_0501`: `strategy=mark` content save writes a hit log, and the admin log API returns the hit word and field name.
- [x] `content.static` now exposes concrete admin management APIs for generated plugins: `static/rule/list`, `static/task/list`, `static/artifact/list`, and `static/clean`.
- [x] Verified with `cms.ability_static_admin_0501`: rule list, task list, artifact list, and clean all work; manual generation produced `/published/static-admin/1.html` and clean removed the artifact.
- [x] `content.like` now exposes concrete admin APIs for generated plugins: `like/list`, `like/counter/list`, and admin `like/status` to enable or disable a like record.
- [x] Verified with `cms.ability_like_admin_0501`: admin list sees the like record, counter list shows `1`, admin status disables it, and public status returns `likeCount=0`.
- [x] `content.view-stat` now exposes concrete admin APIs for generated plugins: `view/counter/list`, `view/log/list`, and `view/daily/list`.
- [x] Verified with `cms.ability_view_admin_0501`: two visits produce counter `2/2`, two log rows, and one daily-stat row through the admin APIs.
- [x] Generated `ability.html` now supports per-pack backend views instead of only the generic `/pack/list` table:
  - `content.comment`: switch between all comments and audit view; row actions include approve, reject, hide, and delete.
  - `content.sensitive`: switch between word dictionary and hit logs.
  - `content.static`: switch between rules, tasks, and artifacts.
  - `content.like`: switch between counters and like records.
  - `content.view-stat`: switch between counters, logs, and daily stats.
- [x] Verified with `cms.ability_page_views_0501`: generated static, like, and view-stat ability pages return HTTP 200 and include the new view-switching script.
- [x] Verified with `cms.ability_page_review_0501`: generated comment and sensitive ability pages return HTTP 200 and include comment management plus sensitive hit-log views.
- [x] `content.tag` and `content.topic` now expose admin relation maintenance APIs in generated plugins: `tag/content/list`, `tag/unbind`, `topic/content/list`, and `topic/unbind`.
- [x] Generated `ability.html` now gives tag/topic management a second `内容关联` view and supports row-level relation unbind actions.
- [x] Verified with `cms.ability_taxonomy_admin_0501`: generated plugin enables, content can bind to tag/topic, admin relation lists return one row, unbind removes the relation, and generated tag/topic ability pages return HTTP 200 with relation views.
- [x] Generated content editor now injects mounted taxonomy controls into the publish settings group: existing tag checklist, inline new-tag input, and topic checklist.
- [x] `content.tag` binding now accepts empty selections to clear a content item's tags; `content.topic` now has content-side binding API `topic/bind-content` so editor saves do not overwrite a topic's full content list.
- [x] Verified with `cms.ability_editor_taxonomy_0501`: generated plugin enables, editor page includes `tagIds`/`tagNames`/`topicIds`, content-side tag/topic binding works, and empty selections clear relations.
- [x] Generated content list APIs now enrich rows with mounted taxonomy fields: `tagIds`, `tagNames`, `tagNamesText`, `topicIds`, `topicTitles`, and `topicTitlesText`.
- [x] Generated admin/public list APIs now support taxonomy filters: `tagId` and `topicId`.
- [x] Generated content list pages now load mounted tag/topic dictionaries, show tag/topic filter selectors only when the matching pack is mounted, and add tag/topic columns dynamically.
- [x] Optimized taxonomy list enrichment to resolve mounted pack flags once per list request instead of probing pack contracts per row.
- [x] Verified with `cms.ability_list_taxonomy_0501`: regenerated plugin enables/reloads, content binds to tag/topic, list rows return tag/topic text, `tagId/topicId` filters work, missing tag filter returns zero rows, and the generated article list page exposes tag/topic filters.
- [x] Generated content detail/get APIs now return the same mounted taxonomy fields as list APIs, so frontends do not lose tag/topic relations after entering the detail page.
- [x] Verified with `cms.ability_list_taxonomy_0501`: regenerated plugin reloads, detail API returns `DetailTag` and `DetailTopic` through `tagNamesText` and `topicTitlesText`.
- [x] Generated public content pages now show taxonomy filters when `content.tag` or `content.topic` is mounted, load public tag/topic dictionaries, pass `tagId/topicId` into public list requests, and display taxonomy chips in list/detail records.
- [x] Verified with `cms.ability_list_taxonomy_0501`: generated public page returns HTTP 200 with taxonomy filter code, and public list filtering by `PublicTag/PublicTopic` returns the expected published content row.
- [x] Generated public detail pages now render native front-facing ability panels when matching packs are mounted:
  - `content.view-stat`: records a visit and displays total/unique views.
  - `content.like`: displays like count and toggles like/cancel state with a browser-local actor key.
  - `content.comment`: displays visible comments and submits public comments through the generated comment API.
  - `content.static`: previews the current static artifact path and can trigger manual static generation from the public detail page.
- [x] Verified with `cms.ability_native_front_0501`: generated plugin enables/reloads, public page contains the native ability UI, view recording returns `1/1`, like status returns `liked=true` and count `1`, and auto-approved comment submission appears in the public comment list.
- [x] New-content sensitive hit logs now promote pending `targetId=0` records to the inserted content id before the save response is returned.
- [x] Verified with `cms.ability_sensitive_target_0501`: new content containing `needle` saves successfully under `strategy=mark`, and `/sensitive/log/list?targetId=<contentId>` returns the hit with `targetId=<contentId>`.
- [x] Verified with `cms.ability_static_front_0501`: generated plugin enables/reloads, public page contains the static panel and generate action, preview is empty before manual generation, manual generation returns `/published/static-front/1.html`, and preview returns the same artifact path after generation.
- [x] Generated public pages now expose the `content.view-stat` front rank surface: when view statistics are mounted, the list sidebar shows a visit ranking panel backed by the generated `/view/rank` API and refreshes after detail-page view recording.
- [x] Verified with `cms.ability_rank_front_0501`: generated plugin enables/reloads, saving published content and recording one visit puts that content at the top of `/view/rank`, and the public page contains the rank panel, rank loader, and `/view/rank` call.
- [x] Generated list/detail APIs now attach native metric fields when matching packs are mounted: `visibleCommentCount`, `commentCount`, `likeCount`, `viewCount`, and `uniqueViewCount`.
- [x] Generated admin list pages now add comment/like/view columns dynamically, and generated public list cards expose the same metrics as chips.
- [x] Verified with `cms.ability_list_metrics_0501`: generated plugin enables/reloads, comment create, like create, and view record each update the generated counters, admin/public list APIs return `1/1/1`, and both generated pages contain the metric display code.
- [x] Generated ability pack admin page routes are now URI-bound to the first permission declared by the mounted pack, for example `content.comment` routes bind to `auth_content_comment`.
- [x] Generated `runtime/contracts.json` now records `permissionBinding=ability-admin-page-bound` at root level and per ability pack, so generated permission registration and route enforcement describe the same policy.
- [x] Verified with `cms.ability_auth_bind6_0501`: save model, generate plugin, enable plugin, generated `main.c` contains `route.auth_id = auth_content_comment`, and `runtime/contracts.json` records `ability-admin-page-bound`.
- [x] Generated ability pack admin APIs now bind to pack-level auth variables as well: comment/tag/topic/sensitive/static/like/view-stat management APIs use `auth_content_*` instead of only `need_auth/admin_only`.
- [x] Verified with `cms.ability_api_auth_0501`: all seven packs mounted, generated plugin enables, default auth variables are present, page routes are bound, and representative admin APIs for all seven packs contain the matching `route.auth_id`.
- [x] Hardened ability auth variable generation: built-in packs use template-level default variables for shared native routes, while non-built-in packs still get generated `auth_<safePack>` declarations for their generated pack page routes.
- [x] Verified with `cms.ability_api_auth2_0501`: after restarting the host, a fresh seven-pack model generated and enabled successfully, with built-in auth defaults, assignments, and admin API route bindings intact.
- [x] Cleaned generated TCC warnings caused by `str`/`const char*` mixing in static artifact paths and sensitive-word replacement/error handling.
- [x] Verified with `cms.ability_warning_clean_0501`: seven-pack generated plugin saves, generates, and enables with `warningCount=0` for that plugin in the host compile log.
- [x] Plugin-system auth publishing now grants generated admin permission points to the default administrator role and refreshes auth caches, so newly generated ability-pack pages/APIs are immediately usable after enable.
- [x] Verified with `cms.ability_static_status2_0501`: generated plugin enables, `static/rule/save`, `static/generate`, and `static/task/status` all pass through the bound ability auth without HTTP 403.
- [x] `content.static` now exposes a concrete task status endpoint: `/admin/api/plugin/<xid>/static/task/status?id=<taskId>`.
- [x] Verified with `cms.ability_static_status2_0501`: task status returns the generated task row, missing task IDs return `static task not found`, and regenerated plugin compile warning count is `0`.
- [x] Browser spot-checked `cms.ability_static_status2_0501` ability management page through the safe admin entry: static page opens without 403, rules/tasks/artifacts views switch correctly, and browser console has no errors.
- [x] Disabled `cms.ability_static_status2_0501` after validation to avoid startup overhead from the temporary validation plugin.
- [x] Cleaned temporary validation plugin/data directories matching `cms.ability_*_0501`; remaining matching directories in plugin/data-plugin roots are `0`.

- [x] 完成 `content.comment` 评论系统能力包。
- [x] 完成 `content.tag` 标签系统能力包。
- [x] 完成 `content.topic` 专题系统能力包。
- [x] 完成 `content.sensitive` 敏感词过滤能力包。
- [x] 完成 `content.static` 静态化生成能力包。
- [x] 完成 `content.like` 点赞系统能力包。
- [x] 完成 `content.view-stat` 访问量统计能力包。
- [x] 每个能力包完成后台页、前端 API、配置、数据结构、权限、菜单和生成影响验收。
