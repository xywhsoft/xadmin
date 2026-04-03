# xAdmin 插件系统重构 SPEC

## 1. 目标

重构后的插件系统必须满足以下目标：

1. 插件是原生 C 代码，不是二等公民运行时
2. 插件与 xadmin 环境完全打通
3. 公共 ABI 以 `xs_plugin.h + 稳定导出函数` 形式存在
4. 只把宿主管理型能力做成宿主导出函数
5. 生命周期、reload、drain、资源回收由插件系统统一治理
6. xadmin 内核只保留基础设施，业务能力全部插件化

## 2. 系统分层

### 2.1 xs

负责：

- 协议层
- TCC
- `xrt`
- `sqlite3`
- `xvalue / xs`

### 2.2 xadmin

负责：

- 前后台用户系统
- session
- 鉴权
- 操作日志
- 附件管理
- 全局配置系统
- 插件系统运行时

### 2.3 插件

负责：

- 内容管理
- 留言板
- 下载系统
- 计划任务
- 服务器管理
- 模型系统
- 其他业务能力

## 3. 信任模型

插件默认视为完全信任的本地代码。

约束：

1. 不设计运行时沙盒
2. 不设计权限隔离
3. 不设计高危能力授权开关
4. 风险控制依赖发布前代码审查

## 4. 核心原则

1. Native First
2. 能直接调用的能力直接调用
3. 不把宿主已有普通函数重新包装成大 function table
4. 只把宿主管理型资源接口做成稳定导出函数
5. 插件系统治理资源与生命周期，不治理插件普通计算逻辑

## 5. 插件身份

插件唯一身份是 `xid`。

`xid` 同时用于：

1. 插件目录名
2. 数据库关联键
3. 生成型插件身份
4. 后续插件市场身份

## 6. 运行模型

系统运行模型固定为两层：

1. `package`
2. `generation`

### 6.1 package

表示一个插件包，对应一个 `xid`。

### 6.2 generation

表示插件当前的一次运行代。

generation 负责：

1. 编译结果
2. 宿主管理型资源注册
3. 活动引用计数
4. reload/drain 生命周期

### 6.3 非目标

平台级多实例不是本次重构目标。

多实例需求由两种方式替代：

1. 生成多个独立插件
2. 插件内部自己支持多个上下文 ID

## 7. 插件目录结构

```text
plugin/<xid>/
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
└── migrations/
```

规则：

1. 插件目录名必须等于 `xid`
2. `inc/` 自动加入 include path
3. `lib/` 自动加入 library path

## 8. Manifest

`plugin.json` 最少应包含：

1. `formatVersion`
2. `xid`
3. `name`
4. `title`
5. `version`
6. `kind`
7. `build`
8. `defaultConfig`
9. `configSchema`

`build` 最少应包含：

1. `entry`
2. `sources`
3. `includeDirs`
4. `libraryDirs`
5. `libraries`
6. `defines`

## 9. 编译模型

插件编译模型固定为：

1. `TCC + C`

规则：

1. 每个 generation 对应一个独立 `TCCState`
2. 插件代码运行在当前宿主环境里
3. 宿主注册全局运行时路径与插件自己的 `inc/`、`lib/`
4. 编译输入来自 `plugin.json.build`
5. 不依赖变量导出共享宿主单例
6. 宿主级单例统一通过 `XAdmin_PluginSetGlobalData()` 注入

## 10. Native 直用边界

以下能力默认允许插件直接使用，不单独设计 API：

1. C 标准库
2. `xrt`
3. `sqlite3`
4. `xvalue / xs`
5. `http_reply`
6. `HttpReplyFormat`
7. `LoadPage`
8. 模板渲染
9. 宿主中已经存在的普通函数

以下内容明确不设计为额外 API：

1. `DB API`
2. `FS API`
3. `Template API`
4. `Config API`
5. 通用 `malloc/free/log/time` wrapper

## 11. 插件 SDK

插件 SDK 文件固定为：

`tcc/inc_xs/xs_plugin.h`

它负责：

1. 生命周期描述符
2. 宿主管理型资源的数据结构
3. 稳定导出函数声明
4. Global Data 注入槽位定义

它不负责：

1. 数据访问封装
2. 文件访问封装
3. 普通页面输出封装
4. 通用运行时包装
5. 普通配置读写封装

## 12. 宿主导出函数

必须稳定导出的函数分组如下：

1. Route
2. UI
3. Auth
4. Event
5. Hook
6. Service
7. Plugin Control

这些函数之所以必须由宿主导出，是因为它们：

1. 会修改宿主全局注册表
2. 需要 owner tracking
3. 需要 reload / unload 回收
4. 需要 generation drain

## 13. 插件入口

统一插件入口：

```c
XADMIN_EXPORT const XAdminPluginDescriptor* XAdmin_GetPluginDescriptor(void);
```

统一全局注入入口：

```c
XADMIN_EXPORT void XAdmin_PluginSetGlobalData(int idx, void* ptr);
```

## 14. Global Data 注入

宿主必须在 `tcc_relocate` 之后、`OnLoad` 之前完成注入。

首批固定注入槽位：

1. `XADMIN_GLOBAL_MAIN_DB`
2. `XADMIN_GLOBAL_OPTION_TABLE`
3. `XADMIN_GLOBAL_PLUGIN_XID`
4. `XADMIN_GLOBAL_PLUGIN_ROOT_PATH`
5. `XADMIN_GLOBAL_PLUGIN_DATA_PATH`
6. `XADMIN_GLOBAL_PLUGIN_PRIVATE_DB_PATH`

规则：

1. 不通过变量导出共享宿主单例
2. `G_DB` 通过注入提供
3. `G_Option` 通过注入提供
4. `session / req / resp` 不进入全局注入
5. 插件私有数据库不是系统能力，插件自己调用 native `sqlite3_open*()`

## 15. 生命周期

统一描述符回调：

1. `OnLoad`
2. `OnInstall`
3. `OnStart`
4. `OnConfigChanged`
5. `OnHealthCheck`
6. `OnStop`
7. `OnUnload`

规则：

1. `OnInstall` 不参与普通 reload
2. `OnStart / OnStop / OnUnload` 是 generation 级回调
3. reload 只切换 generation

## 16. Reload 与 Drain

reload 固定流程：

1. 编译新 generation
2. 注入全局数据
3. `OnLoad`
4. `OnStart`
5. 健康检查
6. 切 active
7. 旧 generation 进入 `draining`
8. 等活动引用归零
9. `OnStop`
10. `OnUnload`

活动引用来源至少包括：

1. HTTP 请求
2. service lease
3. hook 调用
4. event 回调

## 17. 配置系统

必须区分“xadmin 全局配置”和“插件自身配置”。

### 17.1 xadmin 全局配置

xadmin 继续沿用文件制配置：

- 文件位于 `data/options`
- 启动时扫描 JSON
- 载入到 `G_Option`
- 配置页面按 JSON 动态生成

这套能力直接暴露给插件：

- 通过 `XADMIN_GLOBAL_OPTION_TABLE` 注入 `G_Option`
- 不设计额外 Config API

### 17.2 插件自身配置

插件自身配置分四层：

1. `plugin.json`  
   插件包元信息

2. `config.defaults.json`  
   默认配置

3. `data/plugin/<xid>/config.json`  
   当前持久化配置

4. runtime state  
   插件派生出的内存态

规则：

1. 保存配置前按 `config.schema.json` 校验
2. 保存成功后更新 `config.json`
3. 运行中的 generation 通过 `OnConfigChanged` 接收新配置

## 18. 生成型插件

插件系统必须支持“插件生成插件”。

典型场景：

- 内容系统插件生成新闻插件、文章插件、下载插件
- 留言板生成器生成多个独立留言板插件

生成出来的插件拥有：

1. 独立 `xid`
2. 独立目录
3. 独立数据目录
4. 独立私有数据库
5. 独立路由、菜单、权限

## 19. 私有数据库

私有数据库不是一项插件系统专用能力。

插件本身已经可以直接调用 `sqlite3`，因此插件自己决定：

1. 使用 `G_DB`
2. 使用自己的数据库文件

插件系统只负责注入：

1. `data_path`
2. `private_db_path`

## 20. 最小落地结果

本轮重构完成后，系统至少应满足：

1. 新插件通过 `xs_plugin.h` 编译
2. 不再依赖 `XAdminHostAPI` 大函数表
3. 插件通过稳定导出函数注册 route/menu/auth/service/hook/event
4. `G_DB` 和 `G_Option` 通过 `XAdmin_PluginSetGlobalData()` 注入
5. 插件配置使用 `config.defaults.json + config.schema.json + data/plugin/<xid>/config.json`
6. reload 与 drain 正常工作
7. 生成型插件能够生成并启用新的插件包

## 21. 结论

这次重构的关键不是把旧 `PluginContext` 替换成更大的 function table。

真正的方向是：

1. 插件与 xadmin 完全打通
2. 默认走 native 调用
3. 用 `xs_plugin.h` 固定插件 SDK
4. 用稳定导出函数承载宿主管理型能力
5. 用 `package + generation` 解决生命周期
6. 用生成型插件和插件内部多上下文替代平台级多实例
