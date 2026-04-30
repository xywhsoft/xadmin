# 内容类型 DSL 规范

## 1. 目标

本规范定义内容系统用于生成受管内容插件的 DSL。

设计目标：

- 作为内容系统的唯一真源
- 同时驱动数据库、后台表单、列表、前台 API 和能力包装配
- 支持 revision、diff、迁移和再生成
- 避免旧模型系统里 `dataType/formType` 与 `type/widget` 双 DSL 并存的问题

## 2. 总体结构

建议一个内容类型定义至少包含以下顶层节点：

- `dslVersion`
- `identity`
- `entity`
- `coreFeatures`
- `capabilityPacks`
- `ui`
- `policies`
- `metadata`

示例：

```json
{
  "dslVersion": 1,
  "identity": {},
  "entity": {},
  "coreFeatures": {},
  "capabilityPacks": {},
  "ui": {},
  "policies": {},
  "metadata": {}
}
```

## 3. `identity`

`identity` 描述生成插件的身份。

最少字段：

- `xid`
- `name`
- `title`
- `namespace`
- `kind`
- `icon`
- `description`

示例：

```json
{
  "xid": "cms-article",
  "name": "article",
  "title": "文章",
  "namespace": "cms",
  "kind": "content-plugin",
  "icon": "x-icon-article",
  "description": "CMS 文章插件"
}
```

规则：

- `xid` 是生成插件身份，不等于能力包身份
- `namespace + name` 是业务身份
- `xid` 应与最终插件目录保持一致

## 4. `entity`

`entity` 描述内容主实体。

最少字段：

- `entityName`
- `table`
- `titleField`
- `statusField`
- `fields`

可选字段：

- `slugField`
- `summaryField`
- `coverField`
- `publishedAtField`
- `indexes`

示例：

```json
{
  "entityName": "article",
  "table": "cms_article_content",
  "titleField": "title",
  "slugField": "slug",
  "statusField": "status",
  "summaryField": "summary",
  "coverField": "cover",
  "publishedAtField": "publishedAt",
  "fields": [],
  "indexes": []
}
```

## 5. 字段结构

字段是 DSL 的核心。

每个字段至少包含：

- `name`
- `storage`
- `semantic`
- `component`

常用扩展字段：

- `title`
- `description`
- `required`
- `nullable`
- `defaultValue`
- `showInForm`
- `showInList`
- `showInDetail`
- `sortable`
- `filterable`
- `searchable`
- `options`

示例：

```json
{
  "name": "title",
  "title": "标题",
  "description": "",
  "storage": {
    "type": "text",
    "length": 200
  },
  "semantic": {
    "role": "title"
  },
  "component": {
    "type": "input",
    "props": {
      "placeholder": "请输入标题"
    }
  },
  "required": true,
  "nullable": false,
  "defaultValue": "",
  "showInForm": true,
  "showInList": true,
  "showInDetail": true,
  "sortable": true,
  "filterable": false,
  "searchable": true
}
```

## 6. `storage`

`storage` 决定数据库如何存储。

第一版建议支持：

- `text`
- `integer`
- `real`
- `json`
- `datetime`
- `bool`

示例：

```json
{
  "type": "text",
  "length": 200
}
```

注意：

- `storage` 只描述数据存储
- 不直接描述 UI 渲染

## 7. `semantic`

`semantic` 描述字段在内容系统中的业务角色。

第一版建议支持：

- `title`
- `slug`
- `summary`
- `content`
- `cover`
- `status`
- `author`
- `publishedAt`
- `sort`
- `category`
- `tag`

示例：

```json
{
  "role": "content"
}
```

规则：

- 一个内容实体可以有多个普通字段
- 但同一业务角色应有清晰约束
- 例如 `titleField` 应映射到语义角色为 `title` 的字段

## 8. `component`

`component` 描述后台编辑器如何渲染字段。

这一层必须复用 xadmin form 系统。

第一版建议支持：

- `input`
- `textarea`
- `number`
- `switch`
- `select`
- `radio`
- `checkbox`
- `image`
- `images`
- `file`
- `files`
- `richtext`
- `markdown`
- `datetime`

示例：

```json
{
  "type": "select",
  "props": {
    "clearable": true
  }
}
```

## 9. `coreFeatures`

`coreFeatures` 表示内容插件的基础骨架能力。

第一版建议：

- `adminCrud`
- `publicApi`
- `draft`

示例：

```json
{
  "adminCrud": true,
  "publicApi": true,
  "draft": {
    "enabled": true,
    "mode": "same-table"
  }
}
```

规则：

- `draft` 可以不是单纯布尔值
- 基础能力也允许带配置

## 10. `capabilityPacks`

`capabilityPacks` 表示所选择的能力包及其实例配置。

示例：

```json
{
  "comment": {
    "enabled": true,
    "version": "1.2.0",
    "config": {
      "requireApproval": true,
      "allowGuest": false
    }
  },
  "category": {
    "enabled": true,
    "version": "1.0.0",
    "config": {
      "mode": "single"
    }
  }
}
```

规则：

- `enabled` 表示是否启用该能力包
- `version` 表示期望锁定的能力包版本
- `config` 必须符合能力包自己的 `config.schema.json`

## 11. `ui`

`ui` 描述后台和前台的展示偏好，不直接承担业务语义。

第一版建议支持：

- `list`
- `detail`
- `form`

示例：

```json
{
  "list": {
    "defaultSort": ["id", "desc"],
    "pageSize": 20
  },
  "detail": {
    "showAuthor": true,
    "showPublishedAt": true
  },
  "form": {
    "layout": "two-column"
  }
}
```

## 12. `policies`

`policies` 描述内容实体的默认策略。

第一版建议支持：

- `statusFlow`
- `deleteMode`
- `authorMode`
- `publishMode`

示例：

```json
{
  "statusFlow": "draft-review-published",
  "deleteMode": "soft-delete",
  "authorMode": "admin-or-member",
  "publishMode": "manual"
}
```

## 13. `metadata`

`metadata` 用于记录非业务字段。

建议支持：

- `labels`
- `tags`
- `notes`

示例：

```json
{
  "labels": ["cms", "article"],
  "tags": ["content"],
  "notes": "第一版文章模型"
}
```

## 14. 索引结构

`indexes` 建议采用显式结构。

示例：

```json
[
  {
    "name": "idx_article_slug",
    "fields": ["slug"],
    "unique": true
  },
  {
    "name": "idx_article_status_publish_time",
    "fields": ["status", "publishedAt"],
    "unique": false
  }
]
```

## 15. Diff 友好性要求

为了支持升级顾问和迁移，DSL 必须具备良好的差异可计算性。

要求：

- 字段必须有稳定 `name`
- 能力包必须有稳定 `packId`
- 不使用顺序隐式表达业务语义
- 重命名必须走显式映射

## 16. 字段变更约束

字段变更建议遵守：

- 新增字段：自动可迁移
- 删除字段：默认先 `deprecated`
- 重命名字段：必须显式映射
- 类型变化：必须走兼容矩阵

这部分由升级规范消费，但 DSL 结构必须为它提供足够信息。

## 17. 第一版示例

示例：

```json
{
  "dslVersion": 1,
  "identity": {
    "xid": "cms-article",
    "name": "article",
    "title": "文章",
    "namespace": "cms",
    "kind": "content-plugin",
    "icon": "x-icon-article",
    "description": "文章系统"
  },
  "entity": {
    "entityName": "article",
    "table": "cms_article_content",
    "titleField": "title",
    "slugField": "slug",
    "statusField": "status",
    "fields": [
      {
        "name": "title",
        "title": "标题",
        "storage": { "type": "text", "length": 200 },
        "semantic": { "role": "title" },
        "component": { "type": "input", "props": {} },
        "required": true,
        "showInForm": true,
        "showInList": true,
        "showInDetail": true,
        "searchable": true
      },
      {
        "name": "content",
        "title": "正文",
        "storage": { "type": "text" },
        "semantic": { "role": "content" },
        "component": { "type": "richtext", "props": {} },
        "required": true,
        "showInForm": true,
        "showInList": false,
        "showInDetail": true
      }
    ],
    "indexes": []
  },
  "coreFeatures": {
    "adminCrud": true,
    "publicApi": true,
    "draft": {
      "enabled": true,
      "mode": "same-table"
    }
  },
  "capabilityPacks": {
    "comment": {
      "enabled": true,
      "version": "1.0.0",
      "config": {
        "requireApproval": true
      }
    }
  },
  "ui": {
    "list": {
      "defaultSort": ["id", "desc"],
      "pageSize": 20
    }
  },
  "policies": {
    "statusFlow": "draft-published",
    "deleteMode": "soft-delete",
    "authorMode": "admin-or-member",
    "publishMode": "manual"
  },
  "metadata": {
    "labels": ["cms"]
  }
}
```

## 18. 最低实现要求

第一版内容类型 DSL，至少必须支持：

- `identity`
- `entity.fields`
- `coreFeatures`
- `capabilityPacks`
- `ui.list`
- `policies`

如果这几个部分还不稳定，就不应进入生成器实现阶段。

## 19. 2026-05-01 修订：栏目进入核心能力

早期规范中曾把 `category` 放在 `capabilityPacks` 示例里。该设计已调整。

新的 DSL 规则：

- `category` 属于 `coreFeatures`，不是可选能力包。
- `category.enabled = true` 时，生成插件必须生成自己的栏目表、栏目 API、栏目管理页面和内容表 `category_id` 字段。
- `semantic: "category"` 只能映射到当前生成插件内部的 `category_id`，不能指向宿主级全局栏目。
- 栏目不能跨插件共享，也不能在同一栏目下混合不同内容模型。

推荐结构：

```json
{
  "coreFeatures": {
    "adminCrud": true,
    "publicApi": true,
    "draft": true,
    "category": {
      "enabled": true,
      "mode": "tree",
      "required": false,
      "maxDepth": 5,
      "slugUniqueScope": "siblings"
    }
  }
}
```

`capabilityPacks` 后续仍用于评论、标签、SEO、全文搜索、附件接入、审核流等可独立演进的能力。
