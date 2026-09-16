# 能力包配置面审计（2026-09-16）

> 数据来源：capability-pack/*/instance.xform.json（21 包 166 字段）× 生成插件 gui.cmsdemo（启用全部能力面）的配置读取点全量提取（Managed_AbilityPackConfig{Int,Bool,TextDup,ArrayDup} + SeoConfigText + SearchWeight + RoutePrefixDup）。

## 总览

- 能力包表单字段：**166**；生成代码消费点：**162**——除 SEO 栏目模板 4 字段外**双向完全对齐**；xform 属性面板（33539b6）上线后全部可在编辑器 UI 配置。
- 模型级配置（页面/策略页签）：pageSize/maxScanRows/defaultSort/statusFlow 已消费；**8 个死字段**见缺口清单。
- 硬编码层：核心内容请求体上限 1MB 不可配（唯一下放候选）。

## 逐包可配置项清单（默认值即生成代码兜底值）

### 阅读权限（`content.access`，7 项）

| 字段 | 标签 | 类型 | 默认值 |
|---|---|---|---|
| `passwordHashIterations` | 密码哈希迭代次数 | number | 200 |
| `maxPasswordInputBytes` | 密码输入字节上限 | number | 128 |
| `maxRequestBytes` | 后台请求体字节上限 | number | 65536 |
| `paidSessionKey` | 付费内容 Session 字段 | input | paidContentIds |
| `allowQueryReadLevel` | 允许调试参数覆盖阅读等级 | switch | False |
| `maxReadLevel` | 阅读等级上限 | number | 100 |
| `maxListRows` | 后台规则列表上限 | number | 500 |

### 操作审计（`content.audit-log`，4 项）

| 字段 | 标签 | 类型 | 默认值 |
|---|---|---|---|
| `defaultKeepDays` | 默认保留天数 | number | 90 |
| `maxListRows` | 列表最大行数 | number | 500 |
| `maxCleanupRows` | 单次清理最大行数 | number | 1000 |
| `maxRequestBytes` | 后台请求体字节上限 | number | 65536 |

### 栏目（`content.category`，7 项）

| 字段 | 标签 | 类型 | 默认值 |
|---|---|---|---|
| `maxDepth` | 最大栏目层级 | number | 8 |
| `maxPublicContentRows` | 公开栏目内容列表上限 | number | 200 |
| `maxAdminTreeRows` | 后台栏目树行数上限 | number | 1000 |
| `maxPublicTreeRows` | 公开栏目树行数上限 | number | 1000 |
| `maxSortRows` | 排序行数上限 | number | 500 |
| `maxRequestBytes` | 请求体字节上限 | number | 65536 |
| `maxDriftSampleRows` | 绑定漂移样本行数 | number | 10 |

### 评论管理（`content.comment`，20 项）

| 字段 | 标签 | 类型 | 默认值 |
|---|---|---|---|
| `moderation` | 审核方式 | select | manual |
| `allowPublicPost` | 允许前台提交 | switch | True |
| `maxAuthorNameLength` | 评论昵称长度上限 | number | 80 |
| `maxUserAgentLength` | 请求 User-Agent 长度上限 | number | 256 |
| `maxRequestBytes` | 评论请求体字节上限 | number | 65536 |
| `maxBodyLength` | 评论最大长度 | number | 5000 |
| `minBodyLength` | 评论最小长度 | number | 0 |
| `minSubmitSeconds` | 最短提交耗时秒数 | number | 2 |
| `duplicateWindowSeconds` | 重复评论窗口秒数 | number | 60 |
| `ipWindowSeconds` | 同 IP 限流窗口秒数 | number | 60 |
| `ipWindowLimit` | 同 IP 限流次数 | number | 10 |
| `maxLinks` | 评论正文链接数量上限 | number | 0 |
| `maxPublicRows` | 公开评论列表行数上限 | number | 500 |
| `maxAdminListRows` | 后台列表行数上限 | number | 500 |
| `moderationStatsRecentDays` | 审核统计近期天数 | number | 7 |
| `maxReplyDepth` | 评论回复层级上限 | number | 0 |
| `allowedAuthorNames` | 允许昵称名单 | textarea |  |
| `blockedAuthorNames` | 禁用昵称名单 | textarea |  |
| `blockedBodyPhrases` | 正文禁用片段 | textarea |  |
| `blockedUserAgentPhrases` | 请求代理禁用片段 | textarea |  |

### 内容表单（`content.form`，9 项）

| 字段 | 标签 | 类型 | 默认值 |
|---|---|---|---|
| `ipWindowSeconds` | 同 IP 限流窗口秒数 | number | 60 |
| `ipWindowLimit` | 同 IP 限流次数 | number | 20 |
| `minSubmitSeconds` | 最短提交耗时秒数 | number | 2 |
| `maxSubmissionBytes` | 默认提交大小上限字节 | number | 262144 |
| `maxRequestBytes` | 请求体字节上限 | number | 262144 |
| `maxFields` | 表单字段数量上限 | number | 100 |
| `maxListRows` | 后台列表最大行数 | number | 500 |
| `maxExportRows` | 提交导出最大行数 | number | 5000 |
| `maxNotificationStatEvents` | 通知统计事件上限 | number | 50 |

### 导入导出（`content.import-export`，4 项）

| 字段 | 标签 | 类型 | 默认值 |
|---|---|---|---|
| `maxBatchRows` | 单批导入最大行数 | number | 200 |
| `maxExportRows` | 单次导出最大行数 | number | 1000 |
| `maxJobRows` | 后台任务列表上限 | number | 200 |
| `maxRequestBytes` | 后台请求体字节上限 | number | 1048576 |

### 点赞管理（`content.like`，4 项）

| 字段 | 标签 | 类型 | 默认值 |
|---|---|---|---|
| `allowGuest` | 允许游客点赞 | switch | True |
| `dedup` | 去重策略 | select | ip |
| `maxAdminListRows` | 后台排行列表上限 | number | 100 |
| `maxRequestBytes` | 点赞请求体字节上限 | number | 8192 |

### 媒体资源（`content.media`，10 项）

| 字段 | 标签 | 类型 | 默认值 |
|---|---|---|---|
| `maxAssetSizeBytes` | 最大文件大小字节 | number | 0 |
| `maxImageDimension` | 最大图片宽高 | number | 100000 |
| `serverDetectImageSize` | 服务端识别本地图片尺寸 | switch | True |
| `allowedMimeTypes` | 允许的 MIME 类型 | text |  |
| `maxListRows` | 媒体列表行数上限 | number | 500 |
| `maxRefListRows` | 后台引用列表上限 | number | 300 |
| `maxDetailMediaRows` | 详情媒体行数上限 | number | 50 |
| `maxPublicRefCheckRows` | 公开引用检查行数上限 | number | 100 |
| `maxBatchRows` | 批量操作行数上限 | number | 200 |
| `maxRequestBytes` | 请求体字节上限 | number | 262144 |

### 跳转规则（`content.redirect`，5 项）

| 字段 | 标签 | 类型 | 默认值 |
|---|---|---|---|
| `enablePrettyRedirectRoute` | 启用 /{pluginXid}/r/... 动态跳转入口 | switch | True |
| `redirectRoutePrefix` | 跳转公开前缀 | text | /{pluginXid}/r |
| `maxListRows` | 后台列表行数上限 | number | 200 |
| `maxImportRows` | 导入行数上限 | number | 200 |
| `maxRequestBytes` | 请求体字节上限 | number | 262144 |

### 相关推荐（`content.related`，9 项）

| 字段 | 标签 | 类型 | 默认值 |
|---|---|---|---|
| `ruleLimit` | 单篇推荐数量 | number | 5 |
| `publishRefreshLimit` | 发布时回刷同类数量 | number | 20 |
| `maxPublicRelated` | 公开最大推荐数量 | number | 20 |
| `maxAdminListRows` | 后台列表最大行数 | number | 500 |
| `maxRebuildSources` | 全量重建内容上限 | number | 1000 |
| `maxRequestBytes` | 后台请求体字节上限 | number | 65536 |
| `categoryWeight` | 栏目权重 | number | 10 |
| `tagWeight` | 标签权重 | number | 30 |
| `topicWeight` | 专题权重 | number | 20 |

### 内容版本（`content.revision`，3 项）

| 字段 | 标签 | 类型 | 默认值 |
|---|---|---|---|
| `maxSnapshotsPerContent` | 单内容最大修订数 | number | 100 |
| `maxListRows` | 后台版本列表上限 | number | 200 |
| `maxRequestBytes` | 后台请求体字节上限 | number | 65536 |

### 内容搜索（`content.search`，14 项）

| 字段 | 标签 | 类型 | 默认值 |
|---|---|---|---|
| `titleWeight` | 标题权重 | number | 100 |
| `exactTitleWeight` | 标题精确命中权重 | number | 200 |
| `prefixTitleWeight` | 标题前缀命中权重 | number | 120 |
| `keywordWeight` | 关键词权重 | number | 60 |
| `summaryWeight` | 摘要权重 | number | 35 |
| `bodyWeight` | 正文权重 | number | 10 |
| `exactPhraseWeight` | 完整短语命中权重 | number | 50 |
| `termCoverageWeight` | 查询词覆盖权重 | number | 20 |
| `cjkBigramWeight` | 中文二元词覆盖权重 | number | 12 |
| `freshnessWeight` | 新鲜度权重 | number | 15 |
| `freshnessWindowDays` | 新鲜度时间窗口天数 | number | 30 |
| `minQueryLength` | 最短查询长度 | number | 1 |
| `maxResultLimit` | 单次最大返回数 | number | 100 |
| `maxRebuildRows` | 单次重建索引行数 | number | 1000 |

### 敏感词管理（`content.sensitive`，12 项）

| 字段 | 标签 | 类型 | 默认值 |
|---|---|---|---|
| `strategy` | 命中策略 | select | mark |
| `fields` | 扫描字段 | checkbox | ["title", "summary", "content"] |
| `logRetentionDays` | 命中日志保留天数 | number | 90 |
| `statsRecentDays` | 统计近期天数 | number | 7 |
| `matchMode` | 命中匹配模式 | select | substring |
| `maxWordLength` | 敏感词长度上限 | number | 0 |
| `maxHitsPerScan` | 单次扫描命中上限 | number | 200 |
| `maxCheckBytes` | 检测载荷字节上限 | number | 65536 |
| `maxCleanupRows` | 清理行数上限 | number | 1000 |
| `maxLogListRows` | 命中日志列表行数上限 | number | 200 |
| `maxRequestBytes` | 后台请求体字节上限 | number | 262144 |
| `maxImportRows` | 导入行数上限 | number | 1000 |

### 搜索引擎优化 SEO（`content.seo`，10 项）

| 字段 | 标签 | 类型 | 默认值 |
|---|---|---|---|
| `titleTemplate` | 内容 Title 模板 | input |  |
| `keywordsTemplate` | 内容 Keywords 模板 | input |  |
| `descriptionTemplate` | 内容 Description 模板 | input |  |
| `canonicalTemplate` | 内容 Canonical 模板 | input |  |
| `categoryTitleTemplate` ⚠️未消费 | 栏目 Title 模板 | input | {categoryTitle} - {siteName} |
| `categoryKeywordsTemplate` ⚠️未消费 | 栏目 Keywords 模板 | input | {categoryTitle},{siteName} |
| `categoryDescriptionTemplate` ⚠️未消费 | 栏目 Description 模板 | input | {categoryDescription} |
| `categoryCanonicalTemplate` ⚠️未消费 | 栏目 Canonical 模板 | input | /plugin/{pluginXid}?categoryId={categoryId} |
| `maxListRows` | 后台元信息列表上限 | number | 500 |
| `maxRequestBytes` | 后台请求体字节上限 | number | 65536 |

### 站点地图（`content.sitemap`，5 项）

| 字段 | 标签 | 类型 | 默认值 |
|---|---|---|---|
| `siteUrl` | 站点根地址 | input |  |
| `cacheTtlSeconds` | 缓存 TTL 秒数 | number | 3600 |
| `maxEntryListRows` | 后台条目列表上限 | number | 500 |
| `maxRefreshRows` | 单次刷新内容上限 | number | 5000 |
| `maxRssRows` | 订阅 RSS 输出上限 | number | 100 |

### 固定链接（`content.slug`，6 项）

| 字段 | 标签 | 类型 | 默认值 |
|---|---|---|---|
| `enablePrettySlugRoute` | 启用 /{pluginXid}/{slug} 动态漂亮 URL | switch | True |
| `slugRoutePrefix` | 固定链接公开前缀 | text | /{pluginXid} |
| `maxHistoryRows` | 后台历史列表上限 | number | 200 |
| `maxPublicLookupRows` | 公开 slug 查询行数上限 | number | 5000 |
| `maxRepairRows` | 修复行数上限 | number | 500 |
| `maxRequestBytes` | 请求体字节上限 | number | 65536 |

### 静态化管理（`content.static`，9 项）

| 字段 | 标签 | 类型 | 默认值 |
|---|---|---|---|
| `outputDir` | 输出目录 | input | content |
| `autoGenerate` | 发布后自动生成 | switch | True |
| `autoClean` | 删除后自动清理 | switch | True |
| `autoGenerateRuleLimit` | 自动生成规则上限 | number | 20 |
| `maxListRows` | 列表行数上限 | number | 500 |
| `maxCleanRows` | 单次清理产物数 | number | 500 |
| `maxTaskRetryRows` | 失败任务重试上限 | number | 20 |
| `maxGenerateRequestBytes` | 生成请求体字节上限 | number | 8192 |
| `maxRequestBytes` | 后台请求体字节上限 | number | 65536 |

### 标签管理（`content.tag`，8 项）

| 字段 | 标签 | 类型 | 默认值 |
|---|---|---|---|
| `maxTags` | 单内容最大标签数 | number | 8 |
| `allowCreateInline` | 允许编辑页即时创建标签 | switch | True |
| `maxPublicListRows` | 公开标签行数上限 | number | 500 |
| `maxDetailTags` | 详情标签行数上限 | number | 20 |
| `maxPublicContentRows` | 公开内容行数上限 | number | 200 |
| `maxAdminLinkRows` | 后台关联行数上限 | number | 500 |
| `maxRequestBytes` | 请求体字节上限 | number | 65536 |
| `maxBatchRows` | 批量操作行数上限 | number | 100 |

### 专题管理（`content.topic`，8 项）

| 字段 | 标签 | 类型 | 默认值 |
|---|---|---|---|
| `mode` | 绑定模式 | select | single |
| `maxBindContents` | 单次最大绑定内容数 | number | 200 |
| `maxPublicListRows` | 公开专题行数上限 | number | 500 |
| `maxDetailTopics` | 详情专题行数上限 | number | 20 |
| `maxPublicContentRows` | 公开内容行数上限 | number | 200 |
| `maxAdminLinkRows` | 后台关联行数上限 | number | 500 |
| `maxRequestBytes` | 请求体字节上限 | number | 65536 |
| `maxBatchRows` | 批量操作行数上限 | number | 100 |

### 访问统计（`content.view-stat`，6 项）

| 字段 | 标签 | 类型 | 默认值 |
|---|---|---|---|
| `enableViewLog` | 记录访问日志 | switch | True |
| `rankEnabled` | 启用排行接口 | switch | True |
| `uniqueWindowSeconds` | 唯一访客窗口秒数 | number | 0 |
| `maxRankRows` | 公开排行列表上限 | number | 20 |
| `maxAdminListRows` | 后台统计列表上限 | number | 100 |
| `maxRequestBytes` | 访问统计请求体字节上限 | number | 8192 |

### 审核流程（`content.workflow`，6 项）

| 字段 | 标签 | 类型 | 默认值 |
|---|---|---|---|
| `defaultScheduledLimit` | 默认批量处理数量 | number | 100 |
| `maxListRows` | 列表最大行数 | number | 200 |
| `maxReasonLength` | 审核原因最大长度 | number | 1000 |
| `maxRequestBytes` | 请求体最大字节数 | number | 65536 |
| `requiredApprovals` | 发布所需审核通过次数 | number | 1 |
| `requireDistinctApprovers` | 要求不同审核人 | switch | False |

## 缺口清单

### 1. 死表单字段（表单有、生成代码零消费）——共 4+8 项

- `content.seo`：categoryTitleTemplate / categoryKeywordsTemplate / categoryDescriptionTemplate / categoryCanonicalTemplate——页面级 4 模板有消费，**栏目级 4 模板无消费**（v1 遗留）。处理：生成器补栏目页 SEO 渲染消费，或从表单删除。
- 模型级 页面 tab：listColumns / detailFields / displayGroups / fieldGroups——后台列表/详情页当前按固定列渲染，这 4 项未参与生成。处理：生成器按其过滤列/分组，或页面 tab 移除。
- 模型级 策略 tab：softDelete / auditTime / createRole / manageRole——**软删除与审计时间硬编码恒开**（DELETE 恒为 UPDATE delete_time；create/update_time 恒写），角色校验未实现；仅 statusFlow 有消费。处理：策略开关接入生成器条件分支，或页签标注"恒开"。

### 2. 代码兜底默认值不一致（未配置时不同入口取值不同）

- content.comment：maxAdminListRows 200/500 两处、maxBodyLength 4096/5000 两处
- content.view-stat：maxAdminListRows 100/200 两处
- 影响：仅"从未配置"时可见（表单默认值会在启用时写入配置覆盖全部读取点）；建议生成器统一同键兜底值。

### 3. 硬编码可下放候选（核心层）

- `Managed_ContentMaxRequestBytes()` 恒 1048576：核心内容 CRUD（save/get/detail）请求体上限不可配，而各能力包自己的 maxRequestBytes 均可配。候选：下放为模型级配置（页面 tab 增"请求体上限"）。
- `MANAGED_STATIC_URL_PREFIX`（静态资源前缀）：按插件 XID 生成，属标识而非运营参数，**不建议**下放。

## 生成代码可配置项全景（三层）

| 层 | 配置位置 | 项数 | 说明 |
|---|---|---|---|
| 能力包级 | 编辑器-能力页签 xform 属性面板 → spec.capabilities[].config | 162 | 运行时经 Managed_AbilityPackConfig* 族读取，含请求上限/行数上限/防滥用限流/审核策略/SEO 模板/搜索权重/路由前缀/敏感词策略/静态化输出等 |
| 模型级 | 编辑器-页面/策略页签 → spec.pages / spec.policies | 5 生效 | pageSize/maxScanRows/defaultSort/statusFlow（另 8 项未消费，见缺口） |
| 硬编码 | 生成器模板 | 1 候选 | 核心内容请求体 1MB（下放候选）；静态前缀（合理固定） |
