# xAdmin 插件系统设计方案

## 一、设计目标

1. **功能解耦**: 将前台用户、附件、模型、留言板等功能抽离为独立插件
2. **保持核心小巧**: 核心只保留基础框架（路由、Session、权限、配置）
3. **热插拔**: 支持运行时启用/禁用插件，无需重启服务
4. **统一接口**: 通过 PluginContext 暴露核心能力，插件无需了解内部实现

---

## 二、核心架构

### 2.1 目录结构

```
host/xadmin/
├── script/
│   ├── main.c                    # 主入口（精简）
│   ├── route.h                   # 核心路由（仅后台框架路由）
│   ├── module/
│   │   ├── define.h              # 全局定义
│   │   ├── db.h                  # 数据库
│   │   ├── session.h             # Session
│   │   ├── auth.h                # 后台权限（核心保留）
│   │   ├── http.h                # HTTP 处理
│   │   ├── template.h            # 模板渲染
│   │   ├── plugin_mgr.h          # [新增] 插件管理器
│   │   └── plugin_ctx.h          # [新增] 插件上下文定义
│   └── plugin/                   # [新增] 插件目录
│       ├── plugin.h              # 插件公共头文件
│       ├── member/               # 前台用户插件
│       │   ├── config.json
│       │   └── main.c
│       ├── attachment/           # 附件插件
│       │   ├── config.json
│       │   └── main.c
│       ├── model/                # 模型系统插件
│       │   ├── config.json
│       │   └── main.c
│       └── guestbook/            # 留言板插件
│           ├── config.json
│           └── main.c
└── data/
    └── plugin/                   # 插件数据目录
        ├── member/
        ├── attachment/
        └── ...
```

### 2.2 插件实例结构

```c
// host/xadmin/script/module/plugin_mgr.h

typedef struct {
    // ===== 基础信息 =====
    str sName;              // 插件标识（目录名）
    str sTitle;             // 显示名称
    str sDesc;              // 描述
    str sVersion;           // 版本号
    str sAuthor;            // 作者
    int iSort;              // 排序（加载顺序）
    
    // ===== 路径 =====
    str sPath;              // 插件目录路径
    str sConfigPath;        // 配置文件路径
    str sCodePath;          // 主代码文件路径
    str sDataPath;          // 数据目录路径
    
    // ===== 状态 =====
    bool bEnabled;          // 是否启用
    bool bLoaded;           // 是否已加载
    int iLoadOrder;         // 实际加载顺序
    
    // ===== TCC 状态机 =====
    TCCState* pTccState;
    
    // ===== 依赖 =====
    xlist lstDependencies;  // 依赖的其他插件
    xlist lstRoutes;        // 注册的路由
    xlist lstMenus;         // 注册的菜单ID
    
    // ===== 时间戳 =====
    int64 iCreateTime;
    int64 iUpdateTime;
    int64 iEnableTime;
    
} PluginInstance;
```

---

## 三、插件上下文 (PluginContext)

### 3.1 上下文结构定义

```c
// host/xadmin/script/module/plugin_ctx.h

typedef struct {
    
    // ===== 核心数据 =====
    XDO_Connect pDB;                    // 数据库连接
    xvalue* pAdminSession;              // 后台 Session 表指针
    xvalue* pMemberSession;             // 前台 Session 表指针
    xvalue* pOption;                    // 全局配置表指针
    
    // ===== 路径信息 =====
    str sAppPath;                       // 应用根目录
    str sWebPath;                       // Web 根目录
    str sDataPath;                      // 数据目录
    str sPluginPath;                    // 插件目录
    
    // ===== 路由操作 =====
    RouteInfo* (*AddRoute)(str uri, void* proc, bool bAuth, bool bAdmin, int authId, int authLevel);
    void (*RemoveRoute)(str uri);
    RouteInfo* (*GetRoute)(str uri);
    
    // ===== 菜单操作 =====
    int (*AddMenu)(int parent, str title, str icon, int type, str openType, str href, int sort, bool visible);
    bool (*RemoveMenu)(int menuId);
    bool (*ShowMenu)(int menuId);
    bool (*HideMenu)(int menuId);
    
    // ===== 权限操作 =====
    int (*AddAuthGroup)(str name, str desc, int sort);          // 添加权限分类
    int (*AddAuth)(int groupId, str name, str desc, int sort);  // 添加权限分组
    bool (*RemoveAuthGroup)(int groupId);
    bool (*RemoveAuth)(int authId);
    void (*SyncUriAuth)(str uri, int authId, str desc, bool isBackend, bool needAuth, bool needLog);
    void (*ReloadAuthCache)();                                  // 刷新权限缓存
    
    // ===== Session 操作 =====
    xvalue (*GetAdminSession)(str token);
    xvalue (*GetMemberSession)(str token);
    xvalue (*CreateSession)(bool isAdmin, str userId, int timeout);
    void (*DestroySession)(bool isAdmin, str token);
    void (*ExtendSession)(bool isAdmin, str token, int timeout);
    
    // ===== HTTP 响应 =====
    void (*SendJson)(struct mg_connection* c, int code, str json);
    void (*SendHtml)(struct mg_connection* c, int code, str html);
    void (*SendPage)(struct mg_connection* c, str pagePath, xvalue data);
    void (*SendFile)(struct mg_connection* c, str filePath, str mimeType);
    void (*SendError)(struct mg_connection* c, int code, str message);
    
    // ===== 配置操作 =====
    xvalue (*GetOption)(str group, str key);
    bool (*SetOption)(str group, str key, xvalue value);
    void (*ReloadOption)(str group);
    
    // ===== JSON 操作 =====
    xvalue (*JsonParse)(str json);
    str (*JsonStringify)(xvalue val);
    void (*JsonFree)(xvalue val);
    
    // ===== 工具函数 =====
    int64 (*TimeNow)();
    str (*Format)(str fmt, ...);
    void (*Free)(void* ptr);
    str (*HashPassword)(str user, str salt, str clientHash);
    str (*GenerateSalt)();
    str (*GenerateToken)(int length);
    
    // ===== 日志 =====
    void (*Log)(int level, str format, ...);                    // level: 0=DEBUG, 1=INFO, 2=WARN, 3=ERROR
    void (*LogAccess)(str user, str uri, str method, str param, str body);
    
    // ===== 插件间通信 =====
    void* (*GetPluginExport)(str pluginName, str exportName);   // 获取其他插件导出的接口
    bool (*SetPluginExport)(str pluginName, str exportName, void* ptr);  // 导出接口供其他插件使用
    
    // ===== 事件系统 =====
    bool (*EmitEvent)(str eventName, xvalue eventData);         // 触发事件
    bool (*OnEvent)(str eventName, void* callback);             // 监听事件
    void (*OffEvent)(str eventName, void* callback);            // 取消监听
    
} PluginContext;
```

### 3.2 预定义事件

```c
// 系统事件
#define EVENT_SYSTEM_READY          "system.ready"           // 系统启动完成
#define EVENT_SYSTEM_SHUTDOWN       "system.shutdown"        // 系统关闭前

// 用户事件（由 member 插件触发）
#define EVENT_MEMBER_LOGIN          "member.login"           // 前台用户登录
#define EVENT_MEMBER_LOGOUT         "member.logout"          // 前台用户登出
#define EVENT_MEMBER_REGISTER       "member.register"        // 前台用户注册
#define EVENT_MEMBER_BALANCE_CHANGE "member.balance.change"  // 余额变动

// 附件事件（由 attachment 插件触发）
#define EVENT_ATTACHMENT_UPLOAD     "attachment.upload"      // 附件上传
#define EVENT_ATTACHMENT_DELETE     "attachment.delete"      // 附件删除
#define EVENT_ATTACHMENT_PURCHASE   "attachment.purchase"    // 附件购买

// 模型事件（由 model 插件触发）
#define EVENT_MODEL_ENABLE          "model.enable"           // 模型启用
#define EVENT_MODEL_DISABLE         "model.disable"          // 模型禁用
#define EVENT_MODEL_DATA_ADD        "model.data.add"         // 模型数据添加
#define EVENT_MODEL_DATA_UPDATE     "model.data.update"      // 模型数据更新
#define EVENT_MODEL_DATA_DELETE     "model.data.delete"      // 模型数据删除
```

---

## 四、插件配置文件

```json
// host/xadmin/script/plugin/member/config.json
{
    "name": "member",
    "title": "前台用户系统",
    "desc": "提供前台用户注册、登录、权限管理等功能",
    "version": "1.0.0",
    "author": "xAdmin",
    "sort": 100,
    "enabled": true,
    "dependencies": [],
    "exports": ["MemberAuth_Check", "MemberAuth_GetLevel"],
    "settings": {
        "allowRegister": true,
        "defaultGroupId": 1,
        "sessionTimeout": 86400
    }
}
```

---

## 五、插件代码模板

```c
// host/xadmin/script/plugin/member/main.c

// 引入插件公共头文件
#include "../plugin.h"

// 全局上下文（由 Plugin_SetGlobalData 传入）
PluginContext* ctx;

// 接收主系统传递的全局数据
void Plugin_SetGlobalData(int idx, void* ptr)
{
    if ( idx == 1 ) {
        ctx = (PluginContext*)ptr;
    }
}

// ==================== 插件导出接口 ====================

// 检查用户权限（供其他插件调用）
bool MemberAuth_Check(int memberId, int authId)
{
    // 实现...
}

// 获取用户权限级别（供其他插件调用）
int MemberAuth_GetLevel(int memberId)
{
    // 实现...
}

// ==================== 内部实现 ====================

// 预编译 SQL 语句
sqlite3_stmt* stmt_member_login;
// ...

void Member_InitStmt()
{
    sqlite3_prepare_v3(ctx->pDB->objDB, 
        "SELECT * FROM member WHERE username = ? AND isDelete = 0",
        -1, 0, &stmt_member_login, NULL);
    // ...
}

void Member_FreeStmt()
{
    sqlite3_finalize(stmt_member_login);
    // ...
}

// ==================== 路由处理 ====================

void API_Member_Login(XS_ServerObject objServer, XS_HostObject objHost, 
                      struct mg_connection* c, struct mg_http_message* hm)
{
    // 使用 ctx 访问核心功能
    // ctx->SendJson(c, 200, "{...}");
    // ctx->EmitEvent(EVENT_MEMBER_LOGIN, eventData);
}

// ...

// ==================== 路由注册 ====================

void Member_RegisterRoutes()
{
    // 前台 API
    ctx->AddRoute("/api/v1/login", API_Member_Login, FALSE, FALSE, 0, 0);
    ctx->AddRoute("/api/v1/register", API_Member_Register, FALSE, FALSE, 0, 0);
    ctx->AddRoute("/api/v1/logout", API_Member_Logout, TRUE, FALSE, 0, 0);
    ctx->AddRoute("/api/v1/profile", API_Member_Profile, TRUE, FALSE, 0, 0);
    // ...
    
    // 后台管理
    ctx->AddRoute("/admin/member/user", Admin_Member_User, TRUE, TRUE, g_iAuthId, 0);
    // ...
}

void Member_UnregisterRoutes()
{
    ctx->RemoveRoute("/api/v1/login");
    ctx->RemoveRoute("/api/v1/register");
    // ...
}

// ==================== 菜单注册 ====================

int g_iMenuParentId = 0;
int g_arrMenuIds[10];
int g_iMenuCount = 0;
int g_iAuthGroupId = 0;
int g_iAuthId = 0;

void Member_RegisterMenus()
{
    // 创建权限分类
    g_iAuthGroupId = ctx->AddAuthGroup("前台用户管理", "前台用户相关功能", 300000);
    
    // 创建权限分组
    g_iAuthId = ctx->AddAuth(g_iAuthGroupId, "用户管理", "前台用户增删改查", 300000);
    
    // 创建菜单目录
    g_iMenuParentId = ctx->AddMenu(0, "前台用户管理", "layui-icon layui-icon-friends", 
                                    0, "", "", 300000, TRUE);
    g_arrMenuIds[g_iMenuCount++] = g_iMenuParentId;
    
    // 创建子菜单
    g_arrMenuIds[g_iMenuCount++] = ctx->AddMenu(g_iMenuParentId, "用户管理", 
        "layui-icon layui-icon-username", 1, "_component", 
        "/admin/view/member/user", 300100, TRUE);
    // ...
}

void Member_UnregisterMenus()
{
    for ( int i = g_iMenuCount - 1; i >= 0; i-- ) {
        ctx->RemoveMenu(g_arrMenuIds[i]);
    }
    ctx->RemoveAuth(g_iAuthId);
    ctx->RemoveAuthGroup(g_iAuthGroupId);
}

// ==================== 导出注册 ====================

void Member_RegisterExports()
{
    ctx->SetPluginExport("member", "MemberAuth_Check", MemberAuth_Check);
    ctx->SetPluginExport("member", "MemberAuth_GetLevel", MemberAuth_GetLevel);
}

// ==================== 插件入口 ====================

// 插件初始化（启用时调用）
void Plugin_member_Init()
{
    ctx->Log(1, "[Plugin] member initializing...");
    
    Member_InitStmt();
    Member_RegisterMenus();
    Member_RegisterRoutes();
    Member_RegisterExports();
    
    ctx->ReloadAuthCache();
    
    ctx->Log(1, "[Plugin] member initialized");
}

// 插件卸载（禁用时调用）
void Plugin_member_Unit()
{
    ctx->Log(1, "[Plugin] member unloading...");
    
    Member_UnregisterRoutes();
    Member_UnregisterMenus();
    Member_FreeStmt();
    
    ctx->ReloadAuthCache();
    
    ctx->Log(1, "[Plugin] member unloaded");
}
```

---

## 六、插件管理器核心流程

### 6.1 插件加载流程

```
1. 扫描 script/plugin/ 目录
2. 读取各插件 config.json
3. 解析依赖关系，计算加载顺序
4. 按顺序加载启用的插件:
   a. 创建 TCC 状态机
   b. 编译插件代码
   c. 注册核心符号到 TCC
   d. tcc_relocate()
   e. 调用 Plugin_SetGlobalData() 传递上下文
   f. 调用 Plugin_{name}_Init()
5. 触发 EVENT_SYSTEM_READY 事件
```

### 6.2 插件卸载流程

```
1. 按加载顺序的逆序卸载:
   a. 调用 Plugin_{name}_Unit()
   b. 释放 TCC 状态机
2. 清理插件注册的资源（路由、菜单、权限等）
3. 刷新缓存
```

### 6.3 热重载流程

```
1. 调用 Plugin_Disable(name)
2. 等待当前请求处理完成
3. 调用 Plugin_Enable(name)
```

---

## 七、插件管理后台

### 7.1 后台路由

```c
// 插件列表页面
AddStaticRouteHTTP("/admin/view/plugin", Request_View_Plugin_List);

// 插件 API
AddStaticRouteHTTP("/admin/plugin/list", Request_Plugin_List);
AddStaticRouteHTTP("/admin/plugin/get", Request_Plugin_Get);
AddStaticRouteHTTP("/admin/plugin/enable", Request_Plugin_Enable);
AddStaticRouteHTTP("/admin/plugin/disable", Request_Plugin_Disable);
AddStaticRouteHTTP("/admin/plugin/reload", Request_Plugin_Reload);
AddStaticRouteHTTP("/admin/plugin/settings", Request_Plugin_Settings);
```

### 7.2 后台菜单

在 "设置" 下添加 "插件管理" 菜单项。

---

## 八、实施步骤

### 阶段一：基础框架 (1-2天)

1. 创建 `plugin_ctx.h` - 定义 PluginContext 结构
2. 创建 `plugin_mgr.h` - 实现插件管理器核心逻辑
3. 创建 `script/plugin/plugin.h` - 插件公共头文件
4. 修改 `main.c` - 集成插件管理器

### 阶段二：核心功能迁移 (2-3天)

1. 将 `member.h` + `member_auth.h` 迁移为 `member` 插件
2. 将 `attachment.h` 迁移为 `attachment` 插件
3. 将 `model_mgr.h` 迁移为 `model` 插件

### 阶段三：管理界面 (1天)

1. 创建插件管理页面 `data/page/plugin/list.html`
2. 实现插件管理 API 路由

### 阶段四：测试与优化 (1-2天)

1. 功能测试
2. 性能测试
3. 文档编写

---

## 九、核心精简后的结构

迁移完成后，核心只保留：

```
module/
├── define.h          # 全局定义、路径
├── guard.h           # 安全防护
├── session.h         # Session 管理
├── db.h              # 数据库连接
├── auth.h            # 后台权限（仅基础RBAC）
├── logs.h            # 日志记录
├── admin.h           # 后台基础功能
├── menu.h            # 菜单管理
├── option.h          # 配置管理
├── template.h        # 模板渲染
├── page.h            # 页面API
├── http.h            # HTTP 处理
├── install.h         # 安装向导
├── plugin_ctx.h      # [新增] 插件上下文
└── plugin_mgr.h      # [新增] 插件管理器
```

核心代码量预计减少 60%+。
