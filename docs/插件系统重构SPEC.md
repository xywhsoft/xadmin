# xAdmin 插件系统重构 SPEC

## 1. 目的

本文档定义 xAdmin 新插件系统的正式重构边界。

本次重构采用以下明确决策：

1. 不兼容现有插件系统。
2. 不保留旧插件系统的兼容层。
3. 只实现 `docs/插件系统V3设计方案.md` 中已经定义的能力，不额外扩展新功能。
4. 旧实现仅作为备份保存在 `dev/legacy-plugin-system-20260402/`。

---

## 2. 重构目标

新插件系统必须满足以下目标：

1. 插件本质上仍然是通过 `TCC` 在运行时编译和执行的 C 代码。
2. 宿主与插件之间通过稳定、版本化的 Host ABI 交互。
3. 插件系统采用 `package / instance / generation` 三层运行模型。
4. 所有注册资源都必须可追踪、可回收。
5. 支持热更新、回滚和 generation drain。
6. 支持多实例插件。
7. 支持路由、菜单、权限、服务、Hook、事件、任务、模板、模型等扩展点。
8. 保持实现简单干净，不加入文档之外的新能力。

---

## 3. 明确不做

以下内容不属于本次重构范围：

1. 旧插件兼容层。
2. `PluginContext` 延续演进。
3. `config.json + Plugin_SetGlobalData + Plugin_{name}_Init / Unit` 旧式插件接口。
4. `GetPluginExport / SetPluginExport` 裸函数导出机制。
5. 远程插件市场闭环。
6. 操作系统级沙箱。
7. 文档未定义的新注册中心或新运行时能力。

---

## 4. 旧系统处理原则

旧插件系统不再继续演进，处理原则如下：

1. 旧实现代码已备份到 `dev/legacy-plugin-system-20260402/`。
2. 后续新系统实现时，不以旧 `plugin_ctx.h / plugin_mgr.h / plugin_page.h` 为演进基础。
3. 新系统落地后，主工程应切换到全新的插件运行时模块。
4. 若需要回看旧逻辑，只能参考备份目录，不再向主线回灌旧接口。

---

## 5. 插件包结构

插件目录结构统一如下：

```text
script/plugin/<plugin_id>/
├── plugin.json
├── config.defaults.json
├── config.schema.json
├── main.c
├── src/
├── inc/
├── lib/
├── page/
├── template/
├── assets/
├── migrations/
│   ├── 001_init.sql
│   └── 002_upgrade.sql
├── models/
├── services/
└── README.md
```

约束：

1. `main.c` 是默认入口源码。
2. `src/` 用于附加源码文件。
3. `inc/` 自动注册为该插件的 include 路径。
4. `lib/` 自动注册为该插件的 library 路径。
5. 插件只能引用包内相对路径，不允许越出插件根目录。
6. 动态库如需参与运行，应放在 `lib/runtime/<platform>/`，由宿主复制到 generation 私有目录后再加载。

---

## 6. Manifest 规范

插件主清单文件为 `plugin.json`。

最小字段集合如下：

```json
{
  "formatVersion": 3,
  "id": "cms.article",
  "name": "article_system",
  "title": "文章系统",
  "description": "内容管理插件",
  "version": "3.0.0",
  "author": "xAdmin",
  "kind": "singleton",
  "runtime": {
    "compiler": "tcc",
    "language": "c"
  },
  "build": {
    "entry": "main.c",
    "sources": ["main.c"],
    "includeDirs": ["inc"],
    "libraryDirs": ["lib"],
    "libraries": [],
    "defines": ["XADMIN_PLUGIN=1"]
  },
  "compat": {
    "minHostVersion": "3.0.0",
    "maxHostVersion": "4.0.0",
    "abiVersion": 3
  },
  "capabilities": [],
  "dependencies": {
    "plugins": [],
    "services": [],
    "features": []
  },
  "contributes": {
    "menus": [],
    "routes": [],
    "jobs": [],
    "hooks": [],
    "models": []
  },
  "multiInstance": false,
  "defaultConfig": "config.defaults.json",
  "configSchema": "config.schema.json"
}
```

字段要求：

1. `plugin.json` 只描述静态元数据和构建意图。
2. `config.defaults.json` 存放默认配置。
3. `config.schema.json` 存放配置结构和校验规则。
4. 运行中的实例配置只能存数据库，不反写插件目录。

---

## 7. 编译模型

编译模型固定为 `TCC + C`。

编译规则：

1. 每个 generation 对应一个独立的 `TCCState`。
2. 创建 `TCCState` 时，宿主先注册全局运行时路径，再注册当前插件的 `inc/` 和 `lib/`。
3. `build.sources` 中的源码文件按声明顺序参与编译。
4. `build.includeDirs`、`build.libraryDirs`、`build.libraries`、`build.defines` 作为 manifest 显式输入。
5. 编译结果只服务于当前 generation，不跨 generation 复用内存对象。

---

## 8. 运行时模型

### 8.1 Package

`Plugin Package` 表示一个已安装插件包，负责承载：

1. 包元数据。
2. 源码与资源文件。
3. 默认配置与 schema。
4. 安装包签名、校验信息。

### 8.2 Instance

`Plugin Instance` 表示包的一个实际运行实例，负责承载：

1. 实例配置。
2. 实例启停状态。
3. 实例挂载路径和命名空间。
4. 实例自己的 generation 链。

### 8.3 Generation

`Plugin Generation` 表示实例的一次运行代，负责承载：

1. 当前 generation 的编译结果。
2. 当前 generation 的注册资源。
3. 当前 generation 的健康状态。
4. 当前 generation 的活动引用计数。

任一实例同一时刻允许：

1. 一个 `active generation`
2. 零个或一个 `candidate generation`
3. 零个或多个 `draining generation`

---

## 9. Host ABI

宿主与插件之间只允许通过版本化 Host ABI 交互。

基本要求：

1. ABI 结构体必须携带 `abi_version` 和 `size`。
2. 新字段只能追加，不能重排。
3. 公共 ABI 优先返回 opaque handle。
4. 跨边界内存必须明确所有权。

宿主 API 由以下子 API 组成：

1. `XAdminCoreAPI`
2. `XAdminHttpAPI`
3. `XAdminDbAPI`
4. `XAdminUiAPI`
5. `XAdminAuthAPI`
6. `XAdminEventAPI`
7. `XAdminHookAPI`
8. `XAdminJobAPI`
9. `XAdminFsAPI`
10. `XAdminTemplateAPI`
11. `XAdminServiceAPI`
12. `XAdminModelAPI`

统一插件入口：

```c
XADMIN_EXPORT const XAdminPluginDescriptor* XAdmin_GetPluginDescriptor(void);
```

统一描述符回调：

1. `OnLoad`
2. `OnInstall`
3. `OnStart`
4. `OnConfigChanged`
5. `OnHealthCheck`
6. `OnStop`
7. `OnUnload`

---

## 10. 生命周期

状态机固定如下：

```text
Discovered
  -> Resolved
  -> Compiled
  -> Loaded
  -> Installed
  -> Started
  -> Active
  -> Draining
  -> Stopped
  -> Unloaded

任意阶段可进入 Failed
```

生命周期作用域固定分为三层：

1. `package scope`
2. `instance scope`
3. `generation scope`

约束：

1. `OnInstall` 是实例级安装回调，不参与普通 reload。
2. `OnStart / OnStop / OnUnload` 是 generation 级回调。
3. 普通 reload 只执行升级迁移、`OnLoad`、`OnStart`、`OnStop`、`OnUnload`。
4. 新 generation 健康检查通过后，才能切换为 active。

---

## 11. 热更新与回滚

Reload 流程固定如下：

```text
1. 解析并编译新 generation
2. 必要时执行升级迁移，再执行 OnLoad / OnStart
3. 健康检查通过
4. 原子切换 active generation
5. 旧 generation 进入 Draining
6. 等待旧活动引用归零
7. 调用 OnStop / OnUnload
```

回滚规则：

1. 编译失败时回滚。
2. 依赖解析失败时回滚。
3. 迁移失败时回滚。
4. 启动失败时回滚。
5. 健康检查失败时回滚。
6. 回滚时保持原 active generation 不变。

活动引用来源固定包括：

1. HTTP 请求。
2. WebSocket 或长连接。
3. 正在执行的任务。
4. 事件或异步回调。
5. 正在持有的 service lease。

---

## 12. 扩展点体系

V1 实现范围只包含以下注册中心：

1. `Route Registry`
2. `UI Registry`
3. `Auth Registry`
4. `Service Registry`
5. `Hook Registry`
6. `Event Bus`
7. `Job Registry`
8. `Model Registry`
9. `Template Registry`

说明：

1. Hook 用于可排序、可阻断、可改写的稳定切点。
2. Event 用于广播通知，不参与主流程决策。
3. Service 用于版本化能力消费，不允许退回裸函数导出。

---

## 13. 服务模型

服务模型要求如下：

1. 服务必须按 `service_name + major_version` 解析。
2. 服务声明必须记录 `provider_instance_id` 和 `provider_generation`。
3. 消费端通过 `acquire_service()` 获取 `service_lease + vtable`。
4. 消费完成后必须 `release_service()`。
5. 旧 generation 进入 draining 后，已发放 lease 可以继续使用，直到引用归零。

---

## 14. 能力模型

能力声明采用 manifest 显式声明。

首批能力项限定为：

1. `route.public`
2. `route.admin`
3. `ui.menu`
4. `ui.page`
5. `auth.define`
6. `auth.member_define`
7. `event.emit`
8. `event.listen`
9. `hook.register`
10. `service.provide`
11. `service.consume`
12. `job.register`
13. `template.render`
14. `template.extend`
15. `fs.plugin_data`
16. `fs.shared_data`
17. `fs.app_write`
18. `db.query`
19. `db.exec`
20. `db.prepare`
21. `db.plugin_table`
22. `db.raw`
23. `model.define`
24. `model.generate`
25. `plugin.install`
26. `plugin.reload`

信任等级限定为：

1. `system`
2. `trusted`
3. `reviewed`
4. `untrusted`

高危能力限定为：

1. `db.raw`
2. `fs.app_write`
3. `plugin.install`
4. `plugin.reload`
5. `model.generate`

---

## 15. 配置系统

配置分层固定如下：

1. Manifest 配置。
2. 默认配置。
3. 实例配置。
4. 运行态配置。

配置变更流程固定如下：

```text
1. 提交新配置
2. schema 校验
3. 插件自定义 validate 回调
4. 写入数据库
5. 触发 OnConfigChanged
6. 若失败则回滚配置版本
```

---

## 16. 数据库存储

核心表固定为：

1. `plugin_package`
2. `plugin_instance`
3. `plugin_generation`
4. `plugin_dependency`
5. `plugin_resource`
6. `plugin_migration_log`
7. `plugin_service`

现有业务表最少扩展：

1. `menu` 增加 `plugin_instance_id` 和 `plugin_generation`
2. `uris` 增加 `plugin_instance_id` 和 `plugin_generation`
3. `auth / authGroup / memberAuth / memberAuthGroup` 增加 `plugin_instance_id` 和 `plugin_generation`

`plugin_resource` 必须至少记录：

1. `instance_id`
2. `generation`
3. `owner_scope`
4. `resource_type`
5. `resource_key`
6. `resource_ref`
7. `destroy_policy`
8. `create_time`
9. `status`

---

## 17. 资源归属与回收

每个注册资源都必须记录：

1. `package_id`
2. `instance_id`
3. `generation`

每个资源还必须声明作用域：

1. `package`
2. `instance`
3. `generation`

回收规则：

1. 停止 generation 时，只回收 generation 级运行时资源。
2. 卸载实例时，回收该实例全部资源。
3. 卸载包时，要求无活动实例。
4. `create_table()` 默认视为 `instance scope`，不能在 generation stop 时自动 drop 业务表。

---

## 18. 模型系统定位

模型系统不再作为旁路动态编译系统存在，而是插件系统内的标准扩展类型。

要求：

1. 模型运行时由官方系统插件提供。
2. 模型字段、校验器、渲染器、权限策略和内容 Hook 都走统一插件扩展点。
3. 模型生成器生成标准插件包或实例模板，而不是特权代码。

---

## 19. 实现拆分建议

新系统应采用新的模块拆分，不复用旧 `plugin_mgr.h` 巨型实现。

建议最少拆分为：

1. `plugin_types.*`
2. `plugin_manifest.*`
3. `plugin_abi.*`
4. `plugin_runtime.*`
5. `plugin_registry.*`
6. `plugin_storage.*`
7. `plugin_service.*`
8. `plugin_manager.*`

要求：

1. 类型定义、清单解析、运行时加载、注册中心、存储层分离。
2. 每个模块职责单一。
3. 不再使用单一“大上下文结构体”承载全部宿主能力。

---

## 20. 实施顺序

建议按以下顺序落地：

1. 完成旧实现备份并冻结旧接口。
2. 定义新的核心类型、manifest 和 Host ABI。
3. 实现 `package / instance / generation` 基础数据结构。
4. 实现资源追踪和核心表。
5. 实现 TCC 编译加载、生命周期和 reload。
6. 实现 Service Registry 与 Hook/Event 边界。
7. 接入 Route / UI / Auth / Job / Template / Model 注册中心。
8. 最后替换主工程入口，移除旧插件系统接线。

---

## 21. 验收标准

重构完成后，至少满足以下条件：

1. 主工程中不再依赖旧插件系统头文件。
2. 新插件只能通过 `plugin.json + Host ABI + XAdmin_GetPluginDescriptor()` 接入。
3. 多实例与 generation 切换可正常工作。
4. 所有资源都可按 `instance + generation` 追踪和回收。
5. 裸函数导出、旧生命周期、旧 `PluginContext` 不再出现在主线实现中。
