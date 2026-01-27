# xAdmin 插件系统开发指南

本文档详细介绍如何为 xAdmin 开发插件，帮助你理解插件系统的工作原理和开发流程。

---

## 目录

1. [插件系统概述](#1-插件系统概述)
2. [快速入门](#2-快速入门)
3. [目录结构](#3-目录结构)
4. [配置文件详解](#4-配置文件详解)
5. [插件代码结构](#5-插件代码结构)
6. [PluginContext 接口详解](#6-plugincontext-接口详解)
7. [事件系统](#7-事件系统)
8. [插件间通信](#8-插件间通信)
9. [完整示例](#9-完整示例)
10. [调试技巧](#10-调试技巧)
11. [最佳实践](#11-最佳实践)

---

## 1. 插件系统概述

### 1.1 什么是插件系统？

xAdmin 插件系统允许你在不修改核心代码的情况下，扩展系统功能。每个插件都是独立的功能模块，可以：

- **动态加载/卸载**：运行时启用或禁用，无需重启服务
- **注册路由**：添加新的 API 接口或页面
- **添加菜单**：在后台管理界面添加菜单项
- **监听事件**：响应系统或其他插件触发的事件
- **与其他插件通信**：通过导出接口实现插件间调用

### 1.2 工作原理

```
┌─────────────────────────────────────────────────────────────┐
│                        xAdmin 核心                           │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐         │
│  │   数据库    │  │   Session   │  │    路由     │         │
│  └─────────────┘  └─────────────┘  └─────────────┘         │
│                         │                                    │
│              ┌──────────┴──────────┐                        │
│              │    PluginContext    │ ◄─── 核心能力封装       │
│              └──────────┬──────────┘                        │
└─────────────────────────┼───────────────────────────────────┘
                          │
        ┌─────────────────┼─────────────────┐
        │                 │                 │
        ▼                 ▼                 ▼
   ┌─────────┐      ┌─────────┐      ┌─────────┐
   │ 插件 A  │      │ 插件 B  │      │ 插件 C  │
   │ (TCC)   │      │ (TCC)   │      │ (TCC)   │
   └─────────┘      └─────────┘      └─────────┘
```

**核心流程：**
1. 系统启动时，**插件管理器**扫描 `script/plugin/` 目录
2. 读取每个插件的 `config.json` 配置文件
3. 对于启用的插件，使用 **TCC（Tiny C Compiler）** 动态编译 `main.c`
4. 调用插件的初始化函数 `Plugin_{name}_Init()`
5. 插件通过 **PluginContext** 访问系统核心能力

---

## 2. 快速入门

### 2.1 创建你的第一个插件

**步骤 1：创建插件目录**

在 `script/plugin/` 下创建你的插件目录：

```
script/plugin/
└── myplugin/           # 你的插件目录
    ├── config.json     # 配置文件（必需）
    └── main.c          # 主代码文件（必需）
```

**步骤 2：编写配置文件**

创建 `config.json`：

```json
{
    "name": "myplugin",
    "title": "我的第一个插件",
    "desc": "这是一个测试插件",
    "version": "1.0.0",
    "author": "你的名字",
    "sort": 100,
    "enabled": true,
    "dependencies": [],
    "exports": [],
    "settings": {}
}
```

**步骤 3：编写插件代码**

创建 `main.c`：

```c
// 引入插件公共头文件
#include "../plugin.h"

// 全局上下文（由系统传入）
PluginContext* ctx;

// 接收系统传递的数据
void Plugin_SetGlobalData(int idx, void* ptr)
{
    if ( idx == 1 ) {
        ctx = (PluginContext*)ptr;
    }
}

// 插件初始化（启用时调用）
void Plugin_myplugin_Init()
{
    ctx->Log(LOG_INFO, "[MyPlugin] 插件已启用！");
}

// 插件卸载（禁用时调用）
void Plugin_myplugin_Unit()
{
    ctx->Log(LOG_INFO, "[MyPlugin] 插件已禁用！");
}
```

**步骤 4：启用插件**

1. 重启 xAdmin 服务，或
2. 在后台「设置 → 插件管理」页面点击「启用」

---

## 3. 目录结构

### 3.1 插件目录结构

```
host/xadmin/
├── script/
│   └── plugin/                    # 插件根目录
│       ├── plugin.h               # 插件公共头文件（系统提供）
│       ├── myplugin/              # 你的插件
│       │   ├── config.json        # 配置文件
│       │   └── main.c             # 主代码文件
│       └── another_plugin/        # 另一个插件
│           ├── config.json
│           └── main.c
│
└── data/
    └── plugin/                    # 插件数据目录
        └── myplugin/              # 插件专属数据目录（自动创建）
```

### 3.2 文件说明

| 文件 | 必需 | 说明 |
|------|------|------|
| `config.json` | ✅ | 插件配置，定义名称、版本、设置等 |
| `main.c` | ✅ | 插件主代码，包含初始化和卸载逻辑 |
| 其他 `.c/.h` | ❌ | 可选的辅助代码文件 |

---

## 4. 配置文件详解

### 4.1 完整配置示例

```json
{
    "name": "myplugin",
    "title": "我的插件",
    "desc": "插件功能描述",
    "version": "1.0.0",
    "author": "作者名称",
    "sort": 100,
    "enabled": true,
    "dependencies": ["other_plugin"],
    "exports": ["MyFunction", "AnotherFunction"],
    "settings": {
        "option1": "默认值",
        "option2": 123,
        "option3": true
    }
}
```

### 4.2 字段说明

| 字段 | 类型 | 必需 | 说明 |
|------|------|------|------|
| `name` | string | ✅ | 插件标识，必须与目录名一致，仅限字母数字下划线 |
| `title` | string | ✅ | 显示名称，在管理界面展示 |
| `desc` | string | ❌ | 插件描述 |
| `version` | string | ✅ | 版本号，格式：`主版本.次版本.修订号` |
| `author` | string | ❌ | 作者信息 |
| `sort` | int | ❌ | 加载顺序，数字越小越先加载（默认：100） |
| `enabled` | bool | ❌ | 是否启用（默认：false） |
| `dependencies` | array | ❌ | 依赖的其他插件名称列表 |
| `exports` | array | ❌ | 导出的函数名称列表（供其他插件调用） |
| `settings` | object | ❌ | 插件自定义设置项 |

---

## 5. 插件代码结构

### 5.1 基本模板

```c
// ============================================
// 插件名称：XXX
// 功能描述：XXX
// ============================================

#include "../plugin.h"

// ==================== 全局变量 ====================

PluginContext* ctx;           // 系统上下文
xvalue g_tblSettings = NULL;  // 插件设置

// ==================== 系统回调 ====================

// 接收系统传递的全局数据
void Plugin_SetGlobalData(int idx, void* ptr)
{
    if ( idx == 1 ) {
        ctx = (PluginContext*)ptr;     // 上下文
    } else if ( idx == 2 ) {
        g_tblSettings = (xvalue)ptr;   // 设置
    }
}

// ==================== 路由处理函数 ====================

void API_MyRoute(void* objServer, void* objHost, 
                 struct mg_connection* c, struct mg_http_message* hm)
{
    // 你的 API 逻辑
    ctx->SendJson(c, 200, "{\"result\":true}", 0);
}

// ==================== 初始化和卸载 ====================

// 插件初始化
void Plugin_myplugin_Init()
{
    ctx->Log(LOG_INFO, "[MyPlugin] Initializing...");
    
    // 注册路由
    ctx->AddRoute("/api/myplugin/test", API_MyRoute, FALSE, FALSE, 0, 0);
    
    ctx->Log(LOG_INFO, "[MyPlugin] Initialized!");
}

// 插件卸载
void Plugin_myplugin_Unit()
{
    ctx->Log(LOG_INFO, "[MyPlugin] Unloading...");
    
    // 注销路由
    ctx->RemoveRoute("/api/myplugin/test");
    
    ctx->Log(LOG_INFO, "[MyPlugin] Unloaded!");
}
```

### 5.2 必须实现的函数

| 函数 | 签名 | 说明 |
|------|------|------|
| `Plugin_SetGlobalData` | `void Plugin_SetGlobalData(int idx, void* ptr)` | 接收系统数据 |
| `Plugin_{name}_Init` | `void Plugin_{name}_Init()` | 初始化函数 |
| `Plugin_{name}_Unit` | `void Plugin_{name}_Unit()` | 卸载函数 |

> **注意**：`{name}` 必须与 `config.json` 中的 `name` 字段完全一致！

### 5.3 Plugin_SetGlobalData 参数说明

| idx | ptr 类型 | 说明 |
|-----|----------|------|
| 1 | `PluginContext*` | 系统上下文，包含所有核心接口 |
| 2 | `xvalue` | 插件设置（来自 config.json 的 settings） |

---

## 6. PluginContext 接口详解

`PluginContext` 是插件与系统交互的核心接口，提供了丰富的功能。

### 6.1 核心数据

```c
// 数据库连接
ctx->pDB                    // XDO_Connect 类型

// Session 表指针
ctx->pAdminSession          // 后台 Session
ctx->pMemberSession         // 前台 Session

// 配置表指针
ctx->pOption                // 全局配置

// 路径信息
ctx->sAppPath               // 应用根目录
ctx->sWebPath               // Web 根目录  
ctx->sDataPath              // 数据目录
ctx->sPluginPath            // 插件目录
ctx->sPagePath              // 页面模板目录
```

### 6.2 路由操作

```c
// 添加路由
// 参数：URI, 处理函数, 是否需要登录, 是否后台路由, 权限ID, 权限等级
RouteInfo* route = ctx->AddRoute("/api/test", MyHandler, FALSE, FALSE, 0, 0);

// 移除路由
ctx->RemoveRoute("/api/test");

// 获取路由信息
RouteInfo* info = ctx->GetRoute("/api/test");
```

**路由处理函数签名：**
```c
void MyHandler(void* objServer, void* objHost, 
               struct mg_connection* c, struct mg_http_message* hm)
{
    // hm->method    - 请求方法 (GET/POST/...)
    // hm->uri       - 请求 URI
    // hm->query     - 查询字符串
    // hm->body      - 请求体
    
    ctx->SendJson(c, 200, "{\"result\":true}", 0);
}
```

**AddRoute 参数详解：**

| 参数 | 类型 | 说明 |
|------|------|------|
| uri | str | 路由路径，如 `/api/test` |
| proc | void* | 处理函数指针 |
| bAuth | bool | TRUE=需要登录，FALSE=公开访问 |
| bAdmin | bool | TRUE=后台路由，FALSE=前台路由 |
| authId | int | 关联的权限ID（0=不关联） |
| authLevel | int | 所需权限等级（0=不检查） |

### 6.3 HTTP 响应

```c
// 发送 JSON 响应
// 参数：连接, 状态码, JSON字符串, 长度(0=自动计算)
ctx->SendJson(c, 200, "{\"result\":true}", 0);

// 发送 HTML 响应
ctx->SendHtml(c, 200, "<h1>Hello</h1>");

// 发送模板页面
ctx->SendPage(c, "mypage.html", data);

// 发送文件
ctx->SendFile(c, "/path/to/file.pdf", "application/pdf");

// 发送错误页面
ctx->SendError(c, 404, "页面不存在");
```

> ⚠️ **重要**：`SendJson` 的第4个参数 `len` 必须传入！传 `0` 表示自动计算长度。

### 6.4 菜单操作

```c
// 添加菜单
// 参数：父级ID, 标题, 图标, 类型, 打开方式, 链接, 排序, 是否可见
int menuId = ctx->AddMenu(0, "我的菜单", "layui-icon layui-icon-app", 
                          1, "_component", "/admin/view/mypage", 100, TRUE);

// 移除菜单
ctx->RemoveMenu(menuId);

// 显示/隐藏菜单
ctx->ShowMenu(menuId);
ctx->HideMenu(menuId);
```

**菜单参数说明：**

| 参数 | 说明 | 可选值 |
|------|------|--------|
| parent | 父级菜单ID | 0=顶级菜单 |
| type | 菜单类型 | 0=目录, 1=页面 |
| openType | 打开方式 | `_component`=组件, `_iframe`=iframe, `_blank`=新窗口 |
| sort | 排序 | 数字越小越靠前 |

### 6.5 权限操作

```c
// 添加权限分类
int groupId = ctx->AddAuthGroup("我的模块", "模块描述", 100);

// 添加权限分组
int authId = ctx->AddAuth(groupId, "数据管理", "增删改查权限", 100);

// 移除权限
ctx->RemoveAuth(authId);
ctx->RemoveAuthGroup(groupId);

// 同步 URI 权限
ctx->SyncUriAuth("/api/test", authId, "测试接口", TRUE, TRUE, TRUE);

// 刷新权限缓存（修改权限后调用）
ctx->ReloadAuthCache();
```

### 6.6 Session 操作

```c
// 获取后台 Session
xvalue session = ctx->GetAdminSession(token);

// 获取前台 Session  
xvalue session = ctx->GetMemberSession(token);

// 创建 Session
str token = ctx->CreateAdminSession(userId, userName, roleId, 86400);
str token = ctx->CreateMemberSession(userId, userName, groupId, 86400);

// 销毁 Session
ctx->DestroyAdminSession(token);
ctx->DestroyMemberSession(token);

// 延长 Session 有效期
ctx->ExtendSession(TRUE, token, 86400);  // TRUE=后台, FALSE=前台
```

### 6.7 配置操作

```c
// 获取配置
xvalue val = ctx->GetOption("group", "key");

// 设置配置
ctx->SetOption("group", "key", value);

// 重新加载配置
ctx->ReloadOption("group");
```

### 6.8 JSON 操作

```c
// 解析 JSON
xvalue obj = ctx->JsonParse(jsonStr, strlen(jsonStr));

// 序列化为 JSON
size_t len = 0;
str json = ctx->JsonStringify(obj, &len);

// 释放 JSON 对象
ctx->JsonFree(obj);
```

### 6.9 工具函数

```c
// 获取当前时间戳
int64 now = ctx->TimeNow();

// 格式化字符串
str s = ctx->Format("Hello %s, count=%d", name, count);

// 释放内存
ctx->Free(ptr);

// 密码哈希
str hash = ctx->HashPassword(user, salt, clientHash);

// 生成随机盐值
str salt = ctx->GenerateSalt();

// 生成随机令牌
str token = ctx->GenerateToken(32);
```

### 6.10 日志

```c
// 记录日志
// level: LOG_DEBUG(0), LOG_INFO(1), LOG_WARN(2), LOG_ERROR(3)
ctx->Log(LOG_INFO, "[MyPlugin] 这是一条日志: %s", message);

// 记录访问日志
ctx->LogAccess(user, uri, method, param, body);
```

---

## 7. 事件系统

### 7.1 事件机制

插件可以通过事件系统实现解耦通信：
- **触发事件**：通知其他插件某事发生
- **监听事件**：响应其他插件或系统的事件

### 7.2 预定义事件

```c
// 系统事件
EVENT_SYSTEM_READY          // 系统启动完成
EVENT_SYSTEM_SHUTDOWN       // 系统关闭前

// 用户事件
EVENT_MEMBER_LOGIN          // 前台用户登录
EVENT_MEMBER_LOGOUT         // 前台用户登出
EVENT_MEMBER_REGISTER       // 前台用户注册
EVENT_MEMBER_BALANCE_CHANGE // 余额变动

// 附件事件
EVENT_ATTACHMENT_UPLOAD     // 附件上传
EVENT_ATTACHMENT_DELETE     // 附件删除
EVENT_ATTACHMENT_PURCHASE   // 附件购买

// 模型事件
EVENT_MODEL_ENABLE          // 模型启用
EVENT_MODEL_DISABLE         // 模型禁用
EVENT_MODEL_DATA_ADD        // 模型数据添加
EVENT_MODEL_DATA_UPDATE     // 模型数据更新
EVENT_MODEL_DATA_DELETE     // 模型数据删除
```

### 7.3 使用事件

```c
// 定义事件回调函数
void OnUserLogin(xvalue eventData)
{
    str username = xvoTableGetText(eventData, "username", 8);
    ctx->Log(LOG_INFO, "[MyPlugin] 用户登录: %s", username);
}

// 监听事件
ctx->OnEvent(EVENT_MEMBER_LOGIN, OnUserLogin);

// 取消监听
ctx->OffEvent(EVENT_MEMBER_LOGIN, OnUserLogin);

// 触发事件
xvalue data = xvoCreateTable();
xvoTableSetText(data, "username", 8, "张三", 0, FALSE);
ctx->EmitEvent(EVENT_MEMBER_LOGIN, data);
xvoUnref(data);
```

### 7.4 自定义事件

```c
// 定义自定义事件名
#define EVENT_MY_CUSTOM "myplugin.custom"

// 触发自定义事件
ctx->EmitEvent(EVENT_MY_CUSTOM, myData);

// 其他插件监听
ctx->OnEvent("myplugin.custom", MyHandler);
```

---

## 8. 插件间通信

### 8.1 导出接口

插件可以导出函数供其他插件调用：

**插件 A（提供方）：**
```c
// 要导出的函数
int MyCalculate(int a, int b)
{
    return a + b;
}

void Plugin_pluginA_Init()
{
    // 注册导出接口
    ctx->SetPluginExport("pluginA", "MyCalculate", MyCalculate);
}

void Plugin_pluginA_Unit()
{
    // 可选：清理导出（系统会自动处理）
}
```

**config.json 中声明导出：**
```json
{
    "name": "pluginA",
    "exports": ["MyCalculate"]
}
```

**插件 B（使用方）：**
```c
// 定义函数指针类型
typedef int (*FnCalculate)(int, int);

void UsePluginA()
{
    // 获取导出接口
    FnCalculate calc = (FnCalculate)ctx->GetPluginExport("pluginA", "MyCalculate");
    
    if ( calc ) {
        int result = calc(10, 20);
        ctx->Log(LOG_INFO, "计算结果: %d", result);
    }
}
```

### 8.2 依赖声明

如果插件 B 依赖插件 A，在 config.json 中声明：

```json
{
    "name": "pluginB",
    "dependencies": ["pluginA"]
}
```

系统会确保 pluginA 在 pluginB 之前加载。

---

## 9. 完整示例

### 9.1 留言板插件

**目录结构：**
```
script/plugin/guestbook/
├── config.json
└── main.c
```

**config.json：**
```json
{
    "name": "guestbook",
    "title": "留言板",
    "desc": "简单的留言板功能",
    "version": "1.0.0",
    "author": "xAdmin",
    "sort": 200,
    "enabled": true,
    "dependencies": [],
    "exports": [],
    "settings": {
        "maxLength": 500,
        "requireLogin": false
    }
}
```

**main.c：**
```c
#include "../plugin.h"

// 全局变量
PluginContext* ctx;
xvalue g_tblSettings = NULL;
int g_iMenuId = 0;
sqlite3_stmt* stmt_add = NULL;
sqlite3_stmt* stmt_list = NULL;

void Plugin_SetGlobalData(int idx, void* ptr)
{
    if ( idx == 1 ) ctx = (PluginContext*)ptr;
    else if ( idx == 2 ) g_tblSettings = (xvalue)ptr;
}

// ==================== 数据库操作 ====================

void Guestbook_InitDB()
{
    // 创建表
    str sSQL = "CREATE TABLE IF NOT EXISTS guestbook ("
               "id INTEGER PRIMARY KEY AUTOINCREMENT,"
               "name TEXT NOT NULL,"
               "content TEXT NOT NULL,"
               "createTime INTEGER NOT NULL"
               ")";
    sqlite3_exec(ctx->pDB->objDB, sSQL, NULL, NULL, NULL);
    
    // 预编译语句
    sqlite3_prepare_v3(ctx->pDB->objDB,
        "INSERT INTO guestbook (name, content, createTime) VALUES (?, ?, ?)",
        -1, 0, &stmt_add, NULL);
    
    sqlite3_prepare_v3(ctx->pDB->objDB,
        "SELECT id, name, content, createTime FROM guestbook ORDER BY id DESC LIMIT 100",
        -1, 0, &stmt_list, NULL);
}

void Guestbook_FreeDB()
{
    if ( stmt_add ) sqlite3_finalize(stmt_add);
    if ( stmt_list ) sqlite3_finalize(stmt_list);
}

// ==================== API 处理 ====================

// 获取留言列表
void API_Guestbook_List(void* s, void* h, struct mg_connection* c, struct mg_http_message* hm)
{
    xvalue arrList = xvoCreateArray();
    
    while ( sqlite3_step(stmt_list) == SQLITE_ROW ) {
        xvalue item = xvoCreateTable();
        xvoTableSetInt(item, "id", 2, sqlite3_column_int(stmt_list, 0));
        xvoTableSetText(item, "name", 4, (str)sqlite3_column_text(stmt_list, 1), 0, FALSE);
        xvoTableSetText(item, "content", 7, (str)sqlite3_column_text(stmt_list, 2), 0, FALSE);
        xvoTableSetInt64(item, "createTime", 10, sqlite3_column_int64(stmt_list, 3));
        xvoArrayAppendValue(arrList, item, TRUE);
    }
    sqlite3_reset(stmt_list);
    
    xvalue ret = xvoCreateTable();
    xvoTableSetBool(ret, "result", 6, TRUE);
    xvoTableSetValue(ret, "data", 4, arrList, TRUE);
    
    size_t len = 0;
    str json = ctx->JsonStringify(ret, &len);
    ctx->SendJson(c, 200, json, len);
    ctx->Free(json);
    xvoUnref(ret);
}

// 添加留言
void API_Guestbook_Add(void* s, void* h, struct mg_connection* c, struct mg_http_message* hm)
{
    xvalue form = ctx->JsonParse(hm->body.buf, hm->body.len);
    if ( !form ) {
        ctx->SendJson(c, 200, "{\"result\":false,\"message\":\"参数错误\"}", 0);
        return;
    }
    
    str name = xvoTableGetText(form, "name", 4);
    str content = xvoTableGetText(form, "content", 7);
    
    if ( !name || !content || strlen(name) == 0 || strlen(content) == 0 ) {
        xvoUnref(form);
        ctx->SendJson(c, 200, "{\"result\":false,\"message\":\"请填写完整\"}", 0);
        return;
    }
    
    // 检查长度限制
    int maxLen = 500;
    if ( g_tblSettings ) {
        maxLen = xvoTableGetInt(g_tblSettings, "maxLength", 9);
    }
    if ( strlen(content) > maxLen ) {
        xvoUnref(form);
        ctx->SendJson(c, 200, "{\"result\":false,\"message\":\"内容过长\"}", 0);
        return;
    }
    
    // 写入数据库
    sqlite3_bind_text(stmt_add, 1, name, -1, NULL);
    sqlite3_bind_text(stmt_add, 2, content, -1, NULL);
    sqlite3_bind_int64(stmt_add, 3, ctx->TimeNow());
    sqlite3_step(stmt_add);
    sqlite3_reset(stmt_add);
    
    xvoUnref(form);
    ctx->SendJson(c, 200, "{\"result\":true,\"message\":\"留言成功\"}", 0);
}

// ==================== 初始化和卸载 ====================

void Plugin_guestbook_Init()
{
    ctx->Log(LOG_INFO, "[Guestbook] Initializing...");
    
    // 初始化数据库
    Guestbook_InitDB();
    
    // 注册路由
    ctx->AddRoute("/api/guestbook/list", API_Guestbook_List, FALSE, FALSE, 0, 0);
    ctx->AddRoute("/api/guestbook/add", API_Guestbook_Add, FALSE, FALSE, 0, 0);
    
    // 添加后台菜单
    g_iMenuId = ctx->AddMenu(0, "留言管理", "layui-icon layui-icon-dialogue", 
                             1, "_component", "/admin/view/guestbook", 150, TRUE);
    
    ctx->Log(LOG_INFO, "[Guestbook] Initialized!");
}

void Plugin_guestbook_Unit()
{
    ctx->Log(LOG_INFO, "[Guestbook] Unloading...");
    
    // 移除菜单
    if ( g_iMenuId > 0 ) {
        ctx->RemoveMenu(g_iMenuId);
    }
    
    // 注销路由
    ctx->RemoveRoute("/api/guestbook/list");
    ctx->RemoveRoute("/api/guestbook/add");
    
    // 释放数据库资源
    Guestbook_FreeDB();
    
    ctx->Log(LOG_INFO, "[Guestbook] Unloaded!");
}
```

---

## 10. 调试技巧

### 10.1 日志调试

```c
// 使用不同级别的日志
ctx->Log(LOG_DEBUG, "[Plugin] 调试信息: %s", data);
ctx->Log(LOG_INFO,  "[Plugin] 普通信息: %d", count);
ctx->Log(LOG_WARN,  "[Plugin] 警告: %s", warning);
ctx->Log(LOG_ERROR, "[Plugin] 错误: %s", error);
```

### 10.2 查看插件状态

访问后台「设置 → 插件管理」可以查看：
- 插件列表和状态
- 启用/禁用/重载插件
- 修改插件设置

### 10.3 常见问题

**Q: 插件无法加载**
- 检查 `config.json` 格式是否正确
- 检查 `name` 字段是否与目录名一致
- 检查 `main.c` 中的函数名是否正确

**Q: 路由不生效**
- 确认在 `Plugin_xxx_Init()` 中调用了 `AddRoute`
- 检查路由路径是否与已有路由冲突
- 查看控制台日志确认路由已注册

**Q: 菜单不显示**
- 刷新浏览器页面
- 检查 `visible` 参数是否为 `TRUE`
- 确认父级菜单存在

---

## 11. 最佳实践

### 11.1 命名规范

```c
// 函数命名：插件名_功能名
void MyPlugin_DoSomething();

// 全局变量命名：g_ 前缀
int g_iCount;
xvalue g_tblData;

// 预编译语句命名：stmt_ 前缀  
sqlite3_stmt* stmt_select;
```

### 11.2 资源管理

```c
void Plugin_xxx_Init()
{
    // 初始化时获取资源
    // - 注册路由
    // - 添加菜单
    // - 预编译 SQL
    // - 注册事件监听
}

void Plugin_xxx_Unit()
{
    // 卸载时释放所有资源（逆序）
    // - 取消事件监听
    // - 释放 SQL 语句
    // - 移除菜单
    // - 注销路由
}
```

### 11.3 错误处理

```c
void API_Handler(void* s, void* h, struct mg_connection* c, struct mg_http_message* hm)
{
    // 1. 验证请求方法
    if ( hm->methodCode != HTTP_POST ) {
        ctx->SendJson(c, 200, "{\"result\":false,\"message\":\"方法不允许\"}", 0);
        return;
    }
    
    // 2. 解析参数
    xvalue form = ctx->JsonParse(hm->body.buf, hm->body.len);
    if ( !form ) {
        ctx->SendJson(c, 200, "{\"result\":false,\"message\":\"参数错误\"}", 0);
        return;
    }
    
    // 3. 验证必填字段
    str name = xvoTableGetText(form, "name", 4);
    if ( !name || strlen(name) == 0 ) {
        xvoUnref(form);
        ctx->SendJson(c, 200, "{\"result\":false,\"message\":\"请填写名称\"}", 0);
        return;
    }
    
    // 4. 业务逻辑...
    
    // 5. 清理并返回
    xvoUnref(form);
    ctx->SendJson(c, 200, "{\"result\":true}", 0);
}
```

### 11.4 性能优化

- 使用**预编译 SQL** 而非动态拼接
- 大量数据操作时使用**事务**
- 避免在循环中频繁分配/释放内存
- 合理使用缓存减少数据库查询

---

## 附录：便捷宏定义

`plugin.h` 提供了一些便捷宏：

```c
// 快速发送成功响应
SEND_JSON_OK(c, "操作成功");

// 快速发送错误响应
SEND_JSON_ERR(c, "操作失败");

// 检查请求方法
CHECK_METHOD_GET(c, hm);   // 必须是 GET
CHECK_METHOD_POST(c, hm);  // 必须是 POST
```

---

如有问题或建议，欢迎反馈！
