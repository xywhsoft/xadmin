# xAdmin 插件系统重构 SPEC

## 1. 目的

本文档定义 xAdmin 新插件系统的正式重构边界，并作为后续实现的唯一约束来源。

本次重构采用以下明确决策：

1. 不兼容现有插件系统。
2. 不保留旧插件系统的兼容层。
3. 只实现 `docs/插件系统V3设计方案.md` 中已经确认的能力，不额外扩展新功能。
4. 旧实现仅作为备份保存在 `dev/legacy-plugin-system-20260402/`。

---

## 2. 核心定位

xAdmin 的定位是基础设施内核，而不是业务大合集。

核心只保留：

1. 前后台用户体系。
2. 鉴权系统。
3. 后台操作日志。
4. 附件管理。
5. 自由配置系统。
6. 插件运行时、注册中心、资源治理和生命周期管理。

以下能力应作为插件实现：

1. 内容管理。
2. 留言板。
3. 计划任务系统。
4. 服务器管理。
5. 模型系统。
6. 其他业务模块。

---

## 3. 重构目标

新插件系统必须满足以下目标：

1. 插件本质上仍然是通过 `TCC` 在运行时编译和执行的 C 代码。
2. 宿主与插件之间通过稳定、版本化的 Host ABI 交互。
3. 插件系统采用 `package / instance / generation` 三层运行模型。
4. 所有宿主管理资源都必须可追踪、可回收。
5. 支持热更新、回滚和 generation drain。
6. 支持多实例插件。
7. 支持 `Route / UI / Auth / Service / Hook / Event / Plugin Control` 这些宿主管理扩展点。
8. 插件默认按 native 方式直接使用 `xs/xrt/sqlite3` 能力，不额外包装二等 API。
9. 支持插件生成插件，且生成出的插件可以拥有独立数据目录和独立数据库。
10. 保持实现简单干净，不加入文档之外的新能力。

---

## 4. 明确不做

以下内容不属于本次重构范围：

1. 旧插件兼容层。
2. `PluginContext` 延续演进。
3. `config.json + Plugin_SetGlobalData + Plugin_{name}_Init / Unit` 旧式插件接口。
4. `GetPluginExport / SetPluginExport` 裸函数导出机制。
5. 远程插件市场闭环。
6. 操作系统级沙箱。
7. 文档未定义的新注册中心或新运行时能力。
8. 为 `sqlite3`、`xrt`、模板渲染等现成 native 能力再包一层通用 wrapper API。
9. 在核心内置模型系统、任务系统、内容系统等业务子系统。

---

## 5. 关键原则

### 5.1 Infrastructure Kernel

核心只保留基础设施内核和插件运行时。  
业务能力一律优先走插件，而不是继续堆进核心。

### 5.2 Native First

插件与宿主运行在同一套 `xs/xrt/sqlite3` 原生环境中。

默认原则：

1. 已经能直接调用的 native 能力，插件直接调用。
2. 不为这些能力增加二等 API、额外中间层或冗长数据结构。
3. 只有会修改宿主全局状态、需要 owner tracking / reload 回收 / 审计的能力，才保留显式 Host ABI。

### 5.3 XID First

每个插件都必须有一个稳定唯一的 `xid`。

`xid` 同时用于：

1. 插件目录名。
2. 包和实例的关联标识。
3. 数据库中的插件关联 ID。
4. 未来插件市场的稳定身份。

### 5.4 Global Data Injection

由于 `TCC` 导出全局变量的重定位不可靠，宿主级共享对象不能依赖“导出变量符号”给插件直接链接。  
必须通过函数式注入，把少量宿主单例写入插件运行环境。

适合注入：

1. `G_DB`
2. Host API 指针
3. 少量宿主级共享句柄

不适合注入：

1. `session`
2. `req / resp`
3. 当前请求上下文

请求级对象继续通过原生路由参数、Hook 参数或 Event payload 直传。

---

## 6. 旧系统处理原则

旧插件系统不再继续演进，处理原则如下：

1. 旧实现代码已备份到 `dev/legacy-plugin-system-20260402/`。
2. 后续新系统实现时，不以旧 `plugin_ctx.h / plugin_mgr.h / plugin_page.h` 为演进基础。
3. 主工程只接入新的插件运行时模块。
4. 若需要回看旧逻辑，只能参考备份目录，不再向主线回灌旧接口。
5. 旧插件需要手工迁移到新入口。

---

## 7. 插件包结构

插件目录结构统一如下：

```text
script/plugin/<xid>/
├── plugin.json
├── config.defaults.json
├── config.schema.json
├── main.c
├── src/
├── inc/
├── lib/
├── page/
├── template/                # 插件私有模板或页面片段
├── assets/
├── migrations/
│   ├── 001_init.sql
│   └── 002_upgrade.sql
├── models/                  # 插件私有业务描述，不是核心保留目录
├── services/
└── README.md
```

约束：

1. 目录名必须等于插件 `xid`。
2. `main.c` 是默认入口源码。
3. `src/` 用于附加源码文件。
4. `inc/` 自动注册为该插件的 include 路径。
5. `lib/` 自动注册为该插件的 library 路径。
6. 插件只能引用包内相对路径，不允许越出插件根目录。
7. 动态库如需参与运行，应放在 `lib/runtime/<platform>/`，由宿主复制到 generation 私有目录后再加载。

---

## 8. Manifest 规范

插件主清单文件为 `plugin.json`。

最小字段集合如下：

```json
{
  "formatVersion": 3,
  "xid": "cms.article",
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
    "hooks": [],
    "events": []
  },
  "multiInstance": false,
  "defaultConfig": "config.defaults.json",
  "configSchema": "config.schema.json"
}
```

字段要求：

1. `plugin.json` 只描述静态元数据和构建意图。
2. `xid` 是插件唯一标识，同时用于目录名、数据库关联和未来插件市场身份。
3. `config.defaults.json` 存放默认配置。
4. `config.schema.json` 存放配置结构和校验规则。
5. 运行中的实例配置只能存数据库，不反写插件目录。

---

## 9. 编译模型

编译模型固定为 `TCC + C`。

编译规则：

1. 每个 generation 对应一个独立的 `TCCState`。
2. 创建 `TCCState` 时，宿主先注册全局运行时路径，再注册当前插件的 `inc/` 和 `lib/`。
3. `build.sources` 中的源码文件按声明顺序参与编译。
4. `build.includeDirs`、`build.libraryDirs`、`build.libraries`、`build.defines` 作为 manifest 显式输入。
5. 编译结果只服务于当前 generation，不跨 generation 复用内存对象。
6. 宿主禁止插件依赖导出全局变量符号；宿主级单例统一通过函数式 GlobalData 注入。

---

## 10. 运行时模型

### 10.1 Package

`Plugin Package` 表示一个已安装插件包，负责承载：

1. `xid` 和包身份。
2. 包元数据。
3. 源码与资源文件。
4. 默认配置与 schema。
5. 安装包签名、校验信息。

### 10.2 Instance

`Plugin Instance` 表示包的一个实际运行实例，负责承载：

1. 实例配置。
2. 实例启停状态。
3. 实例挂载路径和命名空间。
4. 实例自己的数据目录和私有数据库路径。
5. 实例自己的 generation 链。

### 10.3 Generation

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

## 11. Host ABI

宿主与插件之间只允许通过版本化 Host ABI 交互。

基本要求：

1. ABI 结构体必须携带 `abi_version` 和 `size`。
2. 新字段只能追加，不能重排。
3. 公共 ABI 优先返回 opaque handle。
4. 跨边界内存必须明确所有权。

宿主 API 由以下子 API 组成：

1. `XAdminCoreAPI`
2. `XAdminHttpAPI`
3. `XAdminUiAPI`
4. `XAdminAuthAPI`
5. `XAdminEventAPI`
6. `XAdminHookAPI`
7. `XAdminServiceAPI`
8. `XAdminPluginControlAPI`

统一插件入口：

```c
XADMIN_EXPORT const XAdminPluginDescriptor* XAdmin_GetPluginDescriptor(void);
```

可选的全局注入入口：

```c
XADMIN_EXPORT void XAdmin_PluginSetGlobalData(int idx, void* ptr);
```

约束：

1. 宿主在 `tcc_relocate` 之后、`OnLoad` 之前调用。
2. 只用于注入 `G_DB`、Host API 指针等少量宿主单例。
3. `session`、`req / resp` 等请求级对象不进入全局注入，而是继续通过路由参数直传。
4. 该接口是新的 V3 启动机制，不等价于旧插件接口兼容层。
5. 首批固定注入槽位包括：
   - `XADMIN_GLOBAL_HOST_API`
   - `XADMIN_GLOBAL_MAIN_DB`
   - `XADMIN_GLOBAL_PLUGIN_XID`
   - `XADMIN_GLOBAL_PLUGIN_ROOT_PATH`
   - `XADMIN_GLOBAL_PLUGIN_INSTANCE_ID`
   - `XADMIN_GLOBAL_PLUGIN_INSTANCE_NAME`
   - `XADMIN_GLOBAL_PLUGIN_MOUNT_PATH`
   - `XADMIN_GLOBAL_PLUGIN_DATA_PATH`
   - `XADMIN_GLOBAL_PLUGIN_PRIVATE_DB_PATH`
6. 插件私有数据库不是单独 API；插件拿到路径后自行用 native `sqlite3_open*()` 管理。

统一描述符回调：

1. `OnLoad`
2. `OnInstall`
3. `OnStart`
4. `OnConfigChanged`
5. `OnHealthCheck`
6. `OnStop`
7. `OnUnload`

Native First 约束：

1. `sqlite3`、`xrt`、模板渲染和大部分 xs 运行时函数默认直接使用。
2. 不再为这些底层能力额外定义通用 wrapper API。
3. 只有会修改宿主注册表、插件目录或插件运行状态的能力，才通过显式 Host ABI 暴露。

Plugin Control 边界：

1. 插件生成插件。
2. 插件启用、禁用、重载。
3. 插件目录写入与注册。
4. 这些操作都属于高风险宿主管理能力，必须经过显式授权和操作日志。

---

## 12. 生命周期

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

## 13. 热更新与回滚

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
3. 正在执行的后台任务。
4. 事件或异步回调。
5. 正在持有的 service lease。

---

## 14. 扩展点体系

V1 实现范围只包含以下宿主管理扩展点：

1. `Route Registry`
2. `UI Registry`
3. `Auth Registry`
4. `Service Registry`
5. `Hook Registry`
6. `Event Bus`
7. `Plugin Control API`

说明：

1. Hook 用于可排序、可阻断、可改写的稳定切点。
2. Event 用于广播通知，不参与主流程决策。
3. Service 用于版本化能力消费，不允许退回裸函数导出。
4. `Plugin Control API` 负责插件生成、启停、重载等高风险宿主管理操作。
5. `sqlite3`、`xrt`、模板渲染、模型运行时等非宿主管理能力，不作为首批注册中心单独实现。

---

## 15. 服务模型

服务模型要求如下：

1. 服务必须按 `service_name + major_version` 解析。
2. 服务声明必须记录 `provider_instance_id` 和 `provider_generation`。
3. 消费端通过 `acquire_service()` 获取 `service_lease + vtable`。
4. 消费完成后必须 `release_service()`。
5. 旧 generation 进入 draining 后，已发放 lease 可以继续使用，直到引用归零。

---

## 16. 能力模型

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
12. `plugin.generate`
13. `plugin.enable`
14. `plugin.disable`
15. `plugin.reload`

信任等级限定为：

1. `system`
2. `trusted`
3. `reviewed`
4. `untrusted`

高危能力限定为：

1. `plugin.generate`
2. `plugin.reload`

说明：

1. 由于插件本质上仍是 native 代码，能力模型更偏向治理和审计，不是强隔离沙箱。
2. `sqlite3` 和 `xrt` 的直接调用不作为可强制约束的细粒度能力项。

---

## 17. 配置系统

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

## 18. 数据库存储

主库只保存插件系统元数据，不承担所有业务插件的数据存储。

核心表固定为：

1. `plugin_package`
2. `plugin_instance`
3. `plugin_generation`
4. `plugin_dependency`
5. `plugin_resource`
6. `plugin_migration_log`
7. `plugin_service`

字段要求：

1. `plugin_package` 必须保存 `xid`。
2. `plugin_instance` 必须保存 `xid`、`data_path`、`private_db_path`。
3. `plugin_generation` 必须保存 generation 状态和错误信息。

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

业务插件默认应拥有自己的实例数据目录和私有数据库，例如：

1. `data/plugin/<xid>/<instance_name>/plugin.db`
2. `data/plugin/<xid>/<instance_id>/plugin.db`

插件通过 native `sqlite3_open*()` 自行打开和管理该数据库；插件系统负责路径约定、启停治理和迁移日志。

---

## 19. 资源归属与回收

每个宿主管理资源都必须记录：

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
4. 插件私有数据库和实例数据目录默认视为 `instance scope`，不能在 generation stop 时自动删除。

说明：

1. 路由、菜单、权限、服务、Hook、事件监听等由宿主自动追踪。
2. 插件私有数据库中的业务表不通过统一 DB wrapper 自动拦截，而通过实例数据目录约定和迁移日志治理。

---

## 20. 业务系统插件化定位

xAdmin 核心只保留基础设施内核。  
内容管理、留言板、计划任务、服务器管理、模型系统等，都应作为插件在统一运行时之上实现。

要求：

1. 模型系统不是核心保留能力，而是普通插件或官方系统插件。
2. 内容系统插件可以根据模型配置生成新的业务插件。
3. 生成出的插件必须是标准 V3 插件，并拥有自己的 `xid`、目录、实例数据目录和私有数据库。
4. “新闻系统”“文章系统”“下载系统”等生成型业务模块，本质上都是独立插件，而不是内容系统内部的子模型。

---

## 21. 实现拆分建议

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
9. `plugin_control.*`

要求：

1. 类型定义、清单解析、运行时加载、注册中心、存储层分离。
2. 每个模块职责单一。
3. 不再使用单一“大上下文结构体”承载全部宿主能力。

---

## 22. 实施顺序

建议按以下顺序落地：

1. 完成旧实现备份并冻结旧接口。
2. 定义新的核心类型、manifest 和 Host ABI。
3. 实现 `package / instance / generation` 基础数据结构。
4. 接入 `XAdmin_PluginSetGlobalData()` 与宿主单例注入。
5. 实现资源追踪和核心表。
6. 实现 TCC 编译加载、生命周期和 reload。
7. 实现 Service Registry、Hook/Event 与 Plugin Control API。
8. 接入 Route / UI / Auth 注册中心。
9. 补齐多实例、完整 activity drain，以及实例私有数据目录/数据库约定。
10. 最后替换主工程入口，移除旧插件系统接线。

---

## 23. 验收标准

重构完成后，至少满足以下条件：

1. 主工程中不再依赖旧插件系统头文件。
2. 新插件只能通过 `plugin.json + Host ABI + XAdmin_GetPluginDescriptor()` 接入。
3. 多实例与 generation 切换可正常工作。
4. 所有宿主管理资源都可按 `instance + generation` 追踪和回收。
5. 插件控制接口可安全生成新的 `xid` 插件并触发加载。
6. 裸函数导出、旧生命周期、旧 `PluginContext` 不再出现在主线实现中。
