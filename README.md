# xAdmin · 新版 xs 迁移开发线

在新版 xs/xrt 上平移 v1 的后台应用。**当前是第一批可运行迁移，不是完整替代版本，暂不用于替换线上 v1。**

迁移原则：复用业务、数据库和界面；只适配底层接口与应用基础设施。`dev/v1` 保留旧版基线；根目录是新版开发线。

## 启动

在本目录运行：

```powershell
.\xs.exe .\xs.json
```

默认仅监听 `127.0.0.1:9081`。`xs.exe` 来自同级 `xserver/release`，使用其内置 C SDK/TCC 环境，不加载 v1 的 `tcc` 或旧版 xs。

主库是 `db/main.db`，已有账号和密码格式不变。若原来的 `options/global.json` 中配置了 `cp_url`，登录仍走该受保护入口，直接访问 `/admin/login` 返回 404 是原有行为；未设置时才使用 `/admin/login`。本次没有重置真实账号、密码或入口配置。

首次从已归档的 v1 导入资产可运行 `python tools/migrate_v1.py`。它使用 SQLite 备份接口，跳过根目录已有文件，不覆盖后续开发成果；不是双向同步工具，也不是线上切换工具。

## 开发结构

| 位置 | 职责 |
| --- | --- |
| `main.c` | 服务初始化、模块依赖顺序和卸载 |
| `route.h` | 现有 URI 注册清单 |
| `route_http/` | 迁自 v1 的业务处理函数 |
| `modules/` | 值/HTTP 适配、路由、会话、权限、数据库和资源加载 |
| `db/` | 主库、插件库、插件历史日志库和安装种子库 |
| `options/` | 原有 JSON 配置；插件私有配置在 `options/plugin/` |
| `page/`、`template/` | 受控页面、服务端模板，不对外直接开放 |
| `wwwroot/` | 网站公开资源：图片、前端 JS/CSS 等 |
| `includes/`、`librarys/` | 额外的第三方 C 头文件、链接库 |
| `forms/`、`uploads/`、`install/` | 已导入的表单、受控附件和安装资产，处理模块待接入 |
| `plugin/`、`plugin_data/` | 原插件包与其他私有数据，插件运行时待适配 |

## 已跑通的第一条链路

HTTP 请求 → 静态/动态路由 → 方法槽 → 会话与权限 → 原业务 SQL → 原页面/模板或 JSON 响应。

已接入 61 个旧 URI，涉及登录、后台权限、会员及会员权限、菜单、日志、品牌信息和会员基础 API。管理员增删改查、会员注册/登录/资料/余额查询/注销已有隔离回归；其他已接入功能的测试覆盖见迁移记录。

旧路由使用 `XHTTP_METHOD_ANY`，保留回调内原来的方法分支和响应契约。新路由可单独注册 GET、POST 或 CRUD；同一 URI/方法重复注册会警告并覆盖该槽，空槽返回 405 和 Allow。动态参数由 `xsReqRouteValue()` 返回借用视图。

## 验证

需要 Python 3，只有标准库依赖：

```powershell
python tests/smoke.py
python tests/smoke.py --protected-entry
```

测试默认监听 `127.0.0.1:19081`，使用 `tests/.runtime/` 下的独立数据库副本和临时账号，不写入根目录主库。也会校验 832 个 v1 页面、模板、静态资源及插件资产的 SHA-256。测试专用 URI 只存在于 `tests/host.c`，正式入口不包含它们。

`tests/write_regression.py` 随上述测试自动执行，覆盖管理端写入失败、零影响行、失败后重试、删除账号撤销所有会话，以及权限关联迁移和余额流水的事务回滚；故障注入仅作用于测试库。

`--port` 可换端口；`--keep-running` 可保留隔离预览实例，并输出 PID 和路径。预览账号仅属于测试库：`migration_smoke` / `Temporary-test-only-9081`；不要把测试入口或测试目录用于部署。默认测试结束会停止自己启动的 xs，保留日志便于排错。

详细的范围、兼容约定和后续接入顺序见 [迁移记录](docs/migration.md)。
