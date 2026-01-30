# xAdmin 插件系统扩展技术规格文档

## 版本历史

| 版本 | 日期 | 作者 | 说明 |
|------|------|------|------|
| 1.0 | 2026-01-31 | xAdmin | 初始版本 |

---

## 目录

1. [概述](#1-概述)
2. [数据库设计](#2-数据库设计)
3. [PluginContext 接口扩展](#3-plugincontext-接口扩展)
4. [自动资源关联机制](#4-自动资源关联机制)
5. [文件操作接口](#5-文件操作接口)
6. [xPack 集成](#6-xpack-集成)
7. [代码生成接口](#7-代码生成接口)
8. [数据库操作接口](#8-数据库操作接口)
9. [插件管理接口](#9-插件管理接口)
10. [模板渲染接口](#10-模板渲染接口)
11. [模型生成器插件规格](#11-模型生成器插件规格)
12. [留言板插件规格](#12-留言板插件规格)
13. [网络插件预留](#13-网络插件预留)

---

## 1. 概述

### 1.1 设计目标

插件系统扩展旨在为 xAdmin 提供以下能力：

1. **插件资源自动关联** - 插件的菜单、路由等资源通过 plugin_id 自动关联，卸载时自动清理
2. **文件操作能力** - 插件可以读写文件，用于生成新插件、存储数据等
3. **xPack 集成** - 支持插件打包和解压
4. **代码生成能力** - 插件可以动态创建新插件（如模型生成器生成文章系统）
5. **数据库操作** - 插件可以创建/删除数据表，管理自有数据
6. **模板渲染** - 插件可以使用模板引擎渲染动态内容

### 1.2 核心原则

- **平等性**：所有插件地位平等，无"特殊插件类型"概念
- **安全性**：插件商店进行严格代码审核，运行时无沙箱限制
- **简洁性**：插件开发者无需关心资源管理细节，系统自动处理
- **可扩展性**：预留网络插件商店相关接口

### 1.3 架构图

```
┌─────────────────────────────────────────────────────────────────┐
│                        xAdmin 核心                               │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐         │
│  │   数据库     │  │  PluginCtx   │  │   xPack      │         │
│  │  (plugin_id) │  │   (扩展)     │  │   (预留)     │         │
│  └──────────────┘  └──────────────┘  └──────────────┘         │
└─────────────────────────┬───────────────────────────────────────┘
                          │
        ┌─────────────────┼─────────────────┐
        │                 │                 │
        ▼                 ▼                 ▼
   ┌─────────┐      ┌─────────┐      ┌─────────┐
   │ 模型    │      │ 留言板  │      │ 其他    │
   │ 生成器  │      │ 插件    │      │ 插件    │
   │ (插件)  │      │ (插件)  │      │         │
   └─────────┘      └─────────┘      └─────────┘
        │                 │
        └───── 生成新插件 ──┘
```

---

## 2. 数据库设计

### 2.1 现有表扩展

#### menu 表扩展

```sql
ALTER TABLE menu ADD COLUMN plugin_id TEXT;

CREATE INDEX IF NOT EXISTS idx_menu_plugin_id ON menu(plugin_id);
```

**字段说明：**

| 字段 | 类型 | 说明 |
|------|------|------|
| plugin_id | TEXT | 关联的插件 ID，NULL 表示系统核心资源 |

#### uris 表扩展

```sql
ALTER TABLE uris ADD COLUMN plugin_id TEXT;

CREATE INDEX IF NOT EXISTS idx_uris_plugin_id ON uris(plugin_id);
```

### 2.2 新增表结构

#### plugin 表

```sql
CREATE TABLE IF NOT EXISTS plugin (
    -- 基础信息
    id TEXT PRIMARY KEY,
    name TEXT UNIQUE NOT NULL,
    title TEXT NOT NULL,
    description TEXT,
    version TEXT NOT NULL,
    author TEXT,
    homepage TEXT,
    repository TEXT,
    license TEXT,
    category TEXT,
    tags TEXT,
    
    -- 网络插件相关
    from_network INTEGER DEFAULT 0,
    download_url TEXT,
    checksum TEXT,
    verified INTEGER DEFAULT 0,
    latest_version TEXT,
    update_available INTEGER DEFAULT 0,
    update_check_time INTEGER,
    
    -- 系统兼容性
    min_system_version TEXT,
    
    -- 配置
    settings TEXT,
    exports TEXT,
    
    -- 状态
    enabled INTEGER DEFAULT 0,
    loaded INTEGER DEFAULT 0,
    sort INTEGER DEFAULT 100,
    
    -- 时间戳
    install_time INTEGER,
    enable_time INTEGER,
    update_time INTEGER,
    create_time INTEGER,
    is_delete INTEGER DEFAULT 0
);

CREATE INDEX IF NOT EXISTS idx_plugin_name ON plugin(name);
CREATE INDEX IF NOT EXISTS idx_plugin_from_network ON plugin(from_network);
CREATE INDEX IF NOT EXISTS idx_plugin_enabled ON plugin(enabled);
```

**字段说明：**

| 字段 | 类型 | 说明 |
|------|------|------|
| id | TEXT | 插件 UUID（网络插件），本地插件使用 name 作为 id |
| name | TEXT | 插件标识（目录名），唯一 |
| title | TEXT | 显示名称 |
| description | TEXT | 插件描述 |
| version | TEXT | 版本号（语义化版本：1.0.0） |
| author | TEXT | 作者 |
| homepage | TEXT | 主页 URL |
| repository | TEXT | 代码仓库 URL |
| license | TEXT | 开源协议 |
| category | TEXT | 分类 |
| tags | TEXT | 标签（JSON 数组字符串） |
| from_network | INTEGER | 是否来自网络插件商店（0=否，1=是） |
| download_url | TEXT | 下载地址 |
| checksum | TEXT | SHA256 校验和 |
| verified | INTEGER | 是否已验证（0=否，1=是） |
| latest_version | TEXT | 最新版本号 |
| update_available | INTEGER | 是否有可用更新（0=否，1=是） |
| update_check_time | INTEGER | 更新检查时间戳 |
| min_system_version | TEXT | 最低系统版本要求 |
| settings | TEXT | 配置（JSON） |
| exports | TEXT | 导出接口（JSON 数组字符串） |
| enabled | INTEGER | 是否启用（0=否，1=是） |
| loaded | INTEGER | 是否已加载（0=否，1=是） |
| sort | INTEGER | 加载排序 |
| install_time | INTEGER | 安装时间 |
| enable_time | INTEGER | 启用时间 |
| update_time | INTEGER | 更新时间 |
| create_time | INTEGER | 创建时间 |
| is_delete | INTEGER | 是否已删除（0=否，1=是） |

#### plugin_dependency 表

```sql
CREATE TABLE IF NOT EXISTS plugin_dependency (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    plugin_name TEXT NOT NULL,
    dependency_name TEXT NOT NULL,
    min_version TEXT,
    max_version TEXT,
    
    FOREIGN KEY (plugin_name) REFERENCES plugin(name) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_plugin_dependency_plugin ON plugin_dependency(plugin_name);
CREATE INDEX IF NOT EXISTS idx_plugin_dependency_dep ON plugin_dependency(dependency_name);
```

**字段说明：**

| 字段 | 类型 | 说明 |
|------|------|------|
| plugin_name | TEXT | 插件名称 |
| dependency_name | TEXT | 依赖的插件名称 |
| min_version | TEXT | 最低依赖版本 |
| max_version | TEXT | 最高兼容版本 |

#### plugin_table 表

```sql
CREATE TABLE IF NOT EXISTS plugin_table (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    plugin_id TEXT NOT NULL,
    table_name TEXT NOT NULL,
    table_type TEXT DEFAULT 'data',
    description TEXT,
    create_time INTEGER NOT NULL,
    
    FOREIGN KEY (plugin_id) REFERENCES plugin(id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_plugin_table_plugin_id ON plugin_table(plugin_id);
CREATE INDEX IF NOT EXISTS idx_plugin_table_name ON plugin_table(table_name);
```

**字段说明：**

| 字段 | 类型 | 说明 |
|------|------|------|
| plugin_id | TEXT | 插件 ID |
| table_name | TEXT | 表名 |
| table_type | TEXT | 表类型：data=数据表, cache=缓存表, log=日志表 |
| description | TEXT | 表描述 |
| create_time | INTEGER | 创建时间 |

#### plugin_install_log 表

```sql
CREATE TABLE IF NOT EXISTS plugin_install_log (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    plugin_id TEXT,
    plugin_name TEXT,
    version TEXT,
    action TEXT,
    status TEXT,
    error_message TEXT,
    install_time INTEGER NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_plugin_install_log_plugin ON plugin_install_log(plugin_id);
CREATE INDEX IF NOT EXISTS idx_plugin_install_log_time ON plugin_install_log(install_time);
```

**字段说明：**

| 字段 | 类型 | 说明 |
|------|------|------|
| plugin_id | TEXT | 插件 ID |
| plugin_name | TEXT | 插件名称 |
| version | TEXT | 版本号 |
| action | TEXT | 操作类型：install=安装, uninstall=卸载, update=更新 |
| status | TEXT | 状态：success=成功, failed=失败 |
| error_message | TEXT | 错误信息（如有） |
| install_time | INTEGER | 操作时间 |

---

## 3. PluginContext 接口扩展

### 3.1 插件自身信息

```c
// 获取当前插件的 UUID
str (*GetPluginId)();

// 获取当前插件的名称
str (*GetPluginName)();

// 获取当前插件的目录路径
str (*GetPluginPath)();

// 获取当前插件的数据目录路径
str (*GetPluginDataPath)();
```

### 3.2 文件操作接口

```c
// 写入文件
bool (*WriteFile)(str filePath, str content, size_t len);

// 读取文件
bool (*ReadFile)(str filePath, str* outContent, size_t* outLen);

// 删除文件
bool (*DeleteFile)(str filePath);

// 检查文件是否存在
bool (*FileExists)(str filePath);

// 创建目录
bool (*CreateDir)(str dirPath);

// 删除目录
bool (*DeleteDir)(str dirPath, bool bRecursive);

// 检查目录是否存在
bool (*DirExists)(str dirPath);

// 遍历目录
typedef bool (*DirScanCallback)(str path, size_t size, int type, ptr data, size_t pathSize);
bool (*ScanDir)(str dirPath, bool bRecursive, DirScanCallback callback, ptr userData);

// 复制文件
bool (*CopyFile)(str srcPath, str destPath);

// 移动/重命名文件
bool (*MoveFile)(str srcPath, str destPath);
```

### 3.3 xPack 集成接口

```c
// 创建 xpkg 包
// 参数：输出路径, 文件列表, 文件数量, 压缩级别 (0-15)
int (*CreateXpkg)(str outputPath, str* fileList, int fileCount, int compressLevel);

// 解压 xpkg 包
// 参数：xpkg 路径, 输出目录
int (*ExtractXpkg)(str xpkgPath, str outputDir);

// 获取 xpkg 包信息
// 返回：包含 manifest.json 的 xvalue 对象
xvalue (*GetXpkgInfo)(str xpkgPath);
```

### 3.4 代码生成接口

```c
// 生成模型插件
// 参数：模型名称, 模型配置
bool (*GenerateModel)(str modelName, xvalue modelConfig);

// 编译插件
// 参数：插件名称
bool (*CompilePlugin)(str pluginName);

// 重载插件
// 参数：插件名称
bool (*ReloadPlugin)(str pluginName);

// 获取插件配置
xvalue (*GetPluginConfig)(str pluginName);

// 设置插件配置
bool (*SetPluginConfig)(str pluginName, xvalue config);
```

### 3.5 数据库操作接口

```c
// 创建数据表
bool (*CreateTable)(str tableName, str sql);

// 删除数据表
bool (*DropTable)(str tableName);

// 执行 SQL
bool (*ExecuteSQL)(str sql);

// 查询 SQL
xvalue (*QuerySQL)(str sql);

// 预编译 SQL
sqlite3_stmt* (*PrepareSQL)(str sql);

// 执行预编译语句
bool (*ExecuteStmt)(sqlite3_stmt* stmt);

// 释放预编译语句
void (*FinalizeStmt)(sqlite3_stmt* stmt);
```

### 3.6 插件管理接口

```c
// 安装插件（从 xpkg）
bool (*InstallPlugin)(str xpkgPath);

// 卸载插件
bool (*UninstallPlugin)(str pluginName);

// 更新插件
bool (*UpgradePlugin)(str pluginName, str newXpkgPath);
```

### 3.7 模板渲染接口

```c
// 渲染模板文件
// 参数：模板路径, 数据对象
str (*RenderTemplate)(str templatePath, xvalue data);

// 渲染模板字符串
// 参数：模板字符串, 数据对象
str (*RenderString)(str templateString, xvalue data);
```

---

## 4. 自动资源关联机制

### 4.1 设计原理

插件调用 `AddRoute` 或 `AddMenu` 时，系统自动将当前插件的 `plugin_id` 写入对应表的记录中。插件卸载时，系统自动删除所有关联资源。

### 4.2 全局变量

```c
// 当前正在加载/卸载的插件 ID
str G_CurrentPluginId = NULL;

// 当前正在加载/卸载的插件实例
PluginInstance* G_CurrentPlugin = NULL;
```

### 4.3 实现流程

#### 4.3.1 AddRoute 扩展

```c
RouteInfo* PluginCtx_AddRoute(str uri, void* proc, bool bAuth, bool bAdmin, int authId, int authLevel)
{
    RouteInfo* pInfo = xrtDictSet(G_StaticRouteTableHTTP, uri, strlen(uri), NULL);
    if ( pInfo ) {
        pInfo->Proc = proc;
        pInfo->bAuth = bAuth;
        pInfo->bAdmin = bAdmin;
        pInfo->bPutLog = FALSE;
        pInfo->bActive = FALSE;
        pInfo->AuthID = authId;
        pInfo->AuthLevel = authLevel;
        
        // 自动写入 plugin_id 到 uris 表
        if ( G_CurrentPluginId ) {
            int64 iNow = xrtNow();
            
            // 检查 URI 是否存在
            str sCheckSQL = xrtFormat("SELECT id FROM uris WHERE uri = '%s'", uri);
            sqlite3_stmt* stmtCheck;
            sqlite3_prepare_v3(G_DB->objDB, sCheckSQL, -1, 0, &stmtCheck, NULL);
            xrtFree(sCheckSQL);
            
            if ( sqlite3_step(stmtCheck) == SQLITE_ROW ) {
                // 更新
                int iId = sqlite3_column_int(stmtCheck, 0);
                sqlite3_finalize(stmtCheck);
                
                str sUpdateSQL = xrtFormat(
                    "UPDATE uris SET plugin_id = '%s', updateTime = %lld WHERE id = %d",
                    G_CurrentPluginId, iNow, iId
                );
                sqlite3_exec(G_DB->objDB, sUpdateSQL, NULL, NULL, NULL);
                xrtFree(sUpdateSQL);
            } else {
                // 插入
                sqlite3_finalize(stmtCheck);
                
                str sInsertSQL = xrtFormat(
                    "INSERT INTO uris (authID, uri, desc, isBackend, needAuth, needLog, putLog, putData, createTime, updateTime, plugin_id) "
                    "VALUES (%d, '%s', '', %d, %d, %d, 0, 0, %lld, %lld, '%s')",
                    authId, uri, bAdmin ? 1 : 0, bAuth ? 1 : 0, 0, iNow, iNow, G_CurrentPluginId
                );
                sqlite3_exec(G_DB->objDB, sInsertSQL, NULL, NULL, NULL);
                xrtFree(sInsertSQL);
            }
        }
    }
    return pInfo;
}
```

#### 4.3.2 AddMenu 扩展

```c
int PluginCtx_AddMenu(int parent, str title, str icon, int type, str openType, str href, int sort, bool visible)
{
    int64 iNow = xrtNow();
    
    sqlite3_bind_int(stmt_menu_add, 1, parent);
    sqlite3_bind_text(stmt_menu_add, 2, title, -1, NULL);
    sqlite3_bind_text(stmt_menu_add, 3, icon ? icon : (str)"", -1, NULL);
    sqlite3_bind_int(stmt_menu_add, 4, type);
    sqlite3_bind_text(stmt_menu_add, 5, openType ? openType : (str)"_component", -1, NULL);
    sqlite3_bind_text(stmt_menu_add, 6, href ? href : (str)"", -1, NULL);
    sqlite3_bind_int(stmt_menu_add, 7, sort);
    sqlite3_bind_int(stmt_menu_add, 8, visible ? 1 : 0);
    sqlite3_bind_text(stmt_menu_add, 9, "", -1, NULL);
    sqlite3_bind_int64(stmt_menu_add, 10, iNow);
    sqlite3_bind_int64(stmt_menu_add, 11, iNow);
    
    // 自动写入 plugin_id
    if ( G_CurrentPluginId ) {
        sqlite3_bind_text(stmt_menu_add, 12, G_CurrentPluginId, -1, NULL);
    } else {
        sqlite3_bind_null(stmt_menu_add, 12);
    }
    
    sqlite3_step(stmt_menu_add);
    int iMenuId = sqlite3_last_insert_rowid(G_DB->objDB);
    sqlite3_reset(stmt_menu_add);
    
    printf("        [Plugin] Menu added: %s (id=%d, plugin_id=%s)\n", 
           title, iMenuId, G_CurrentPluginId ? G_CurrentPluginId : "NULL");
    
    return iMenuId;
}
```

#### 4.3.3 插件卸载时的自动清理

```c
// 清理插件关联的资源
void Plugin_CleanupResources(str pluginId)
{
    if ( !pluginId ) return;
    
    printf("        [Plugin] Cleaning up resources for plugin_id: %s\n", pluginId);
    
    // 1. 软删除关联的菜单
    str sSQL = xrtFormat(
        "UPDATE menu SET isDelete = 1, updateTime = %lld WHERE plugin_id = '%s'",
        xrtNow(), pluginId
    );
    sqlite3_exec(G_DB->objDB, sSQL, NULL, NULL, NULL);
    xrtFree(sSQL);
    
    // 2. 删除关联的 URI
    sSQL = xrtFormat("DELETE FROM uris WHERE plugin_id = '%s'", pluginId);
    sqlite3_exec(G_DB->objDB, sSQL, NULL, NULL, NULL);
    xrtFree(sSQL);
    
    // 3. 删除关联的权限（软删除）
    sSQL = xrtFormat("UPDATE authGroup SET isDelete = 1 WHERE plugin_id = '%s'", pluginId);
    sqlite3_exec(G_DB->objDB, sSQL, NULL, NULL, NULL);
    xrtFree(sSQL);
    
    sSQL = xrtFormat("UPDATE auth SET isDelete = 1 WHERE plugin_id = '%s'", pluginId);
    sqlite3_exec(G_DB->objDB, sSQL, NULL, NULL, NULL);
    xrtFree(sSQL);
    
    // 4. 删除关联的数据表
    sSQL = xrtFormat("SELECT table_name FROM plugin_table WHERE plugin_id = '%s'", pluginId);
    sqlite3_stmt* stmt;
    sqlite3_prepare_v3(G_DB->objDB, sSQL, -1, 0, &stmt, NULL);
    xrtFree(sSQL);
    
    while ( sqlite3_step(stmt) == SQLITE_ROW ) {
        str sTableName = (str)sqlite3_column_text(stmt, 0);
        
        // 删除表
        str sDropSQL = xrtFormat("DROP TABLE IF EXISTS %s", sTableName);
        sqlite3_exec(G_DB->objDB, sDropSQL, NULL, NULL, NULL);
        xrtFree(sDropSQL);
        
        printf("        [Plugin] Dropped table: %s\n", sTableName);
    }
    sqlite3_finalize(stmt);
    
    // 5. 删除 plugin_table 记录
    sSQL = xrtFormat("DELETE FROM plugin_table WHERE plugin_id = '%s'", pluginId);
    sqlite3_exec(G_DB->objDB, sSQL, NULL, NULL, NULL);
    xrtFree(sSQL);
    
    // 6. 删除依赖记录
    sSQL = xrtFormat("DELETE FROM plugin_dependency WHERE plugin_name = '%s'", pluginId);
    sqlite3_exec(G_DB->objDB, sSQL, NULL, NULL, NULL);
    xrtFree(sSQL);
    
    printf("        [Plugin] Cleanup completed for plugin_id: %s\n", pluginId);
}
```

#### 4.3.4 插件加载/卸载时设置全局变量

```c
// 修改 Plugin_Enable 函数
bool Plugin_Enable(PluginInstance* pPlugin)
{
    printf("        [Plugin] Enabling %s...\n", pPlugin->sName);
    
    if ( pPlugin->bLoaded ) {
        printf("        [Plugin] Plugin is already loaded\n");
        return TRUE;
    }
    
    // 设置全局变量
    G_CurrentPlugin = pPlugin;
    G_CurrentPluginId = pPlugin->sName;  // 本地插件使用 name 作为 id
    
    // 使用 TCC 加载插件代码
    if ( !Plugin_TccLoad(pPlugin) ) {
        printf("        [Plugin] Failed to load plugin with TCC\n");
        G_CurrentPlugin = NULL;
        G_CurrentPluginId = NULL;
        return FALSE;
    }
    
    // 添加到已加载列表
    int iIdx = xrtListCount(G_PluginMgr->lstLoadedPlugins);
    xrtListSetPtr(G_PluginMgr->lstLoadedPlugins, iIdx, pPlugin, NULL);
    pPlugin->iLoadOrder = iIdx;
    
    // 更新状态
    pPlugin->bLoaded = TRUE;
    pPlugin->bEnabled = TRUE;
    pPlugin->iEnableTime = xrtNow();
    
    // 保存配置
    Plugin_SaveConfig(pPlugin);
    
    // 清除全局变量
    G_CurrentPlugin = NULL;
    G_CurrentPluginId = NULL;
    
    printf("        [Plugin] Plugin enabled: %s\n", pPlugin->sName);
    return TRUE;
}

// 修改 Plugin_Disable 函数
bool Plugin_Disable(PluginInstance* pPlugin)
{
    printf("        [Plugin] Disabling %s...\n", pPlugin->sName);
    
    if ( !pPlugin->bLoaded ) {
        printf("        [Plugin] Plugin is not loaded\n");
        return TRUE;
    }
    
    // 设置全局变量
    G_CurrentPlugin = pPlugin;
    G_CurrentPluginId = pPlugin->sName;
    
    // 使用 TCC 卸载插件
    Plugin_TccUnload(pPlugin);
    
    // 自动清理资源
    Plugin_CleanupResources(pPlugin->sName);
    
    // 从已加载列表移除
    int iCount = xrtListCount(G_PluginMgr->lstLoadedPlugins);
    for ( int i = 0; i < iCount; i++ ) {
        if ( xrtListGetPtr(G_PluginMgr->lstLoadedPlugins, i) == pPlugin ) {
            xrtListRemove(G_PluginMgr->lstLoadedPlugins, i);
            break;
        }
    }
    
    // 更新状态
    pPlugin->bLoaded = FALSE;
    pPlugin->bEnabled = FALSE;
    
    // 保存配置
    Plugin_SaveConfig(pPlugin);
    
    // 清除全局变量
    G_CurrentPlugin = NULL;
    G_CurrentPluginId = NULL;
    
    printf("        [Plugin] Plugin disabled: %s\n", pPlugin->sName);
    return TRUE;
}
```

---

## 5. 文件操作接口

### 5.1 WriteFile

```c
bool PluginCtx_WriteFile(str filePath, str content, size_t len)
{
    if ( !filePath || !content ) {
        printf("        [Plugin] WriteFile: invalid parameters\n");
        return FALSE;
    }
    
    // 确保目录存在
    str sDir = xrtPathGetDir(filePath, 0);
    if ( sDir ) {
        xrtDirCreate(sDir);
        xrtFree(sDir);
    }
    
    // 写入文件
    size_t iWritten = 0;
    bool bResult = xrtFileWrite(filePath, content, len > 0 ? len : strlen(content), &iWritten, XRT_CP_UTF8);
    
    printf("        [Plugin] WriteFile: %s (%d bytes)\n", filePath, (int)iWritten);
    
    return bResult;
}
```

### 5.2 ReadFile

```c
bool PluginCtx_ReadFile(str filePath, str* outContent, size_t* outLen)
{
    if ( !filePath || !outContent ) {
        return FALSE;
    }
    
    str sContent = xrtFileReadAll(filePath, XRT_CP_UTF8, outLen);
    if ( !sContent ) {
        printf("        [Plugin] ReadFile failed: %s\n", filePath);
        return FALSE;
    }
    
    *outContent = sContent;
    return TRUE;
}
```

### 5.3 DeleteFile

```c
bool PluginCtx_DeleteFile(str filePath)
{
    if ( !xrtFileExists(filePath) ) {
        return FALSE;
    }
    
    bool bResult = xrtFileDelete(filePath);
    printf("        [Plugin] DeleteFile: %s (%s)\n", filePath, bResult ? "OK" : "FAILED");
    
    return bResult;
}
```

### 5.4 FileExists

```c
bool PluginCtx_FileExists(str filePath)
{
    return xrtFileExists(filePath);
}
```

### 5.5 CreateDir

```c
bool PluginCtx_CreateDir(str dirPath)
{
    if ( xrtDirExists(dirPath) ) {
        return TRUE;
    }
    
    bool bResult = xrtDirCreate(dirPath);
    printf("        [Plugin] CreateDir: %s (%s)\n", dirPath, bResult ? "OK" : "FAILED");
    
    return bResult;
}
```

### 5.6 DeleteDir

```c
bool PluginCtx_DeleteDir(str dirPath, bool bRecursive)
{
    if ( !xrtDirExists(dirPath) ) {
        return FALSE;
    }
    
    bool bResult = xrtDirDelete(dirPath, bRecursive);
    printf("        [Plugin] DeleteDir: %s (recursive=%d, %s)\n", 
           dirPath, bRecursive, bResult ? "OK" : "FAILED");
    
    return bResult;
}
```

### 5.7 DirExists

```c
bool PluginCtx_DirExists(str dirPath)
{
    return xrtDirExists(dirPath);
}
```

### 5.8 ScanDir

```c
bool PluginCtx_ScanDir(str dirPath, bool bRecursive, DirScanCallback callback, ptr userData)
{
    return xrtDirScan(dirPath, bRecursive, callback, userData);
}
```

### 5.9 CopyFile

```c
bool PluginCtx_CopyFile(str srcPath, str destPath)
{
    if ( !xrtFileExists(srcPath) ) {
        return FALSE;
    }
    
    // 确保目标目录存在
    str sDestDir = xrtPathGetDir(destPath, 0);
    if ( sDestDir ) {
        xrtDirCreate(sDestDir);
        xrtFree(sDestDir);
    }
    
    // 读取源文件
    size_t iLen = 0;
    str sContent = xrtFileReadAll(srcPath, XRT_CP_UTF8, &iLen);
    if ( !sContent ) {
        return FALSE;
    }
    
    // 写入目标文件
    size_t iWritten = 0;
    bool bResult = xrtFileWrite(destPath, sContent, iLen, &iWritten, XRT_CP_UTF8);
    
    xrtFree(sContent);
    
    printf("        [Plugin] CopyFile: %s -> %s (%s)\n", srcPath, destPath, bResult ? "OK" : "FAILED");
    
    return bResult;
}
```

### 5.10 MoveFile

```c
bool PluginCtx_MoveFile(str srcPath, str destPath)
{
    if ( !xrtFileExists(srcPath) ) {
        return FALSE;
    }
    
    // 确保目标目录存在
    str sDestDir = xrtPathGetDir(destPath, 0);
    if ( sDestDir ) {
        xrtDirCreate(sDestDir);
        xrtFree(sDestDir);
    }
    
    // 尝试直接重命名（同文件系统）
    bool bResult = xrtFileMove(srcPath, destPath);
    
    if ( !bResult ) {
        // 跨文件系统，复制后删除
        bResult = PluginCtx_CopyFile(srcPath, destPath);
        if ( bResult ) {
            xrtFileDelete(srcPath);
        }
    }
    
    printf("        [Plugin] MoveFile: %s -> %s (%s)\n", srcPath, destPath, bResult ? "OK" : "FAILED");
    
    return bResult;
}
```

---

## 6. xPack 集成

### 6.1 概述

xPack 是 xAdmin 的文件压缩库，位于 `D:\Git\xPack`。未来将集成到 xserver 框架内，目前先预留接口。

### 6.2 预留接口实现

```c
int PluginCtx_CreateXpkg(str outputPath, str* fileList, int fileCount, int compressLevel)
{
    // 预留实现
    printf("        [Plugin] CreateXpkg: %s (files=%d, level=%d) - NOT IMPLEMENTED\n", 
           outputPath, fileCount, compressLevel);
    return -1;
}

int PluginCtx_ExtractXpkg(str xpkgPath, str outputDir)
{
    // 预留实现
    printf("        [Plugin] ExtractXpkg: %s -> %s - NOT IMPLEMENTED\n", xpkgPath, outputDir);
    return -1;
}

xvalue PluginCtx_GetXpkgInfo(str xpkgPath)
{
    // 预留实现
    printf("        [Plugin] GetXpkgInfo: %s - NOT IMPLEMENTED\n", xpkgPath);
    return NULL;
}
```

### 6.3 xpkg 包格式

```
article_system.xpkg
├── manifest.json          # 包清单
├── config.json           # 插件配置
├── main.c                # 主代码文件
├── views/                # 前端视图
│   └── list.html
└── assets/               # 资源文件
    └── icon.png
```

### 6.4 manifest.json 格式

```json
{
    "format": "xpkg",
    "version": "1.0",
    "name": "article_system",
    "title": "文章系统",
    "description": "文章内容管理系统",
    "version": "1.0.0",
    "author": "xAdmin",
    "created": 1704067200,
    "compression": {
        "algorithm": "lz4",
        "level": 6
    },
    "files": [
        "config.json",
        "main.c",
        "views/list.html",
        "assets/icon.png"
    ],
    "checksum": "sha256:..."
}
```

---

## 7. 代码生成接口

### 7.1 GenerateModel

模型生成器插件调用此接口生成新的内容管理插件。

```c
bool PluginCtx_GenerateModel(str modelName, xvalue modelConfig)
{
    if ( !modelName || !modelConfig ) {
        printf("        [Plugin] GenerateModel: invalid parameters\n");
        return FALSE;
    }
    
    printf("        [Plugin] Generating model plugin: %s\n", modelName);
    
    // 1. 创建插件目录
    str sPluginPath = xrtPathJoin(3, PluginPath, modelName, "");
    if ( !xrtDirCreate(sPluginPath) ) {
        printf("        [Plugin] Failed to create plugin directory\n");
        xrtFree(sPluginPath);
        return FALSE;
    }
    
    // 2. 生成 config.json
    xvalue tblConfig = xvoCreateTable();
    xvoTableSetText(tblConfig, "name", 4, modelName, 0, FALSE);
    xvoTableSetText(tblConfig, "title", 5, modelName, 0, FALSE);
    xvoTableSetText(tblConfig, "desc", 4, "Auto-generated model plugin", 0, FALSE);
    xvoTableSetText(tblConfig, "version", 7, "1.0.0", 0, FALSE);
    xvoTableSetText(tblConfig, "author", 6, "xAdmin Model Generator", 0, FALSE);
    xvoTableSetInt(tblConfig, "sort", 4, 200);
    xvoTableSetBool(tblConfig, "enabled", 7, FALSE);
    
    str sConfigPath = xrtPathJoin(2, sPluginPath, "config.json");
    size_t iLen = 0;
    str sConfigJson = xrtStringifyJSON(tblConfig, TRUE, &iLen);
    ctx->WriteFile(sConfigPath, sConfigJson, iLen);
    xrtFree(sConfigPath);
    xrtFree(sConfigJson);
    xvoUnref(tblConfig);
    
    // 3. 生成 main.c
    str sMainC = GenerateModelCode(modelName, modelConfig);
    if ( !sMainC ) {
        printf("        [Plugin] Failed to generate main.c\n");
        xrtFree(sPluginPath);
        return FALSE;
    }
    
    str sMainPath = xrtPathJoin(2, sPluginPath, "main.c");
    ctx->WriteFile(sMainPath, sMainC, 0);
    xrtFree(sMainPath);
    xrtFree(sMainC);
    
    // 4. 生成视图文件
    str sViewPath = xrtPathJoin(3, sPluginPath, "views", "");
    xrtDirCreate(sViewPath);
    
    str sListHtml = GenerateListView(modelName, modelConfig);
    str sListPath = xrtPathJoin(2, sViewPath, "list.html");
    ctx->WriteFile(sListPath, sListHtml, 0);
    xrtFree(sListPath);
    xrtFree(sListHtml);
    xrtFree(sViewPath);
    
    xrtFree(sPluginPath);
    
    printf("        [Plugin] Model plugin generated: %s\n", modelName);
    return TRUE;
}

// 生成插件代码
str GenerateModelCode(str modelName, xvalue modelConfig)
{
    xvalue tblFields = xvoTableGetValue(modelConfig, "fields", 6);
    if ( !tblFields ) return NULL;
    
    str sCode = xrtFormat(
        "#include \"../plugin.h\"\n\n"
        "PluginContext* ctx;\n"
        "xvalue g_tblSettings = NULL;\n\n"
        "void Plugin_SetGlobalData(int idx, void* ptr)\n"
        "{\n"
        "    if ( idx == 1 ) ctx = (PluginContext*)ptr;\n"
        "    else if ( idx == 2 ) g_tblSettings = (xvalue)ptr;\n"
        "}\n\n"
        "// 数据库初始化\n"
        "void %s_InitDB()\n"
        "{\n"
        "    str sSQL = \"CREATE TABLE IF NOT EXISTS %s (\"\n"
        "               \"id INTEGER PRIMARY KEY AUTOINCREMENT,\"",
        modelName, modelName
    );
    
    // 添加字段定义
    int iFieldCount = xvoArrayCount(tblFields);
    for ( int i = 0; i < iFieldCount; i++ ) {
        xvalue tblField = xvoArrayGetValue(tblFields, i);
        str sFieldName = xvoTableGetText(tblField, "name", 4);
        str sFieldType = xvoTableGetText(tblField, "type", 4);
        
        str sFieldSQL = xrtFormat(
            "\n               \"%s %s,\",",
            sFieldName, sFieldType
        );
        
        str sNewCode = xrtFormat("%s%s", sCode, sFieldSQL);
        xrtFree(sCode);
        xrtFree(sFieldSQL);
        sCode = sNewCode;
    }
    
    str sNewCode = xrtFormat(
        "%s\n               \"createTime INTEGER NOT NULL\n"
        "               \")\";\n"
        "    sqlite3_exec(ctx->pDB->objDB, sSQL, NULL, NULL, NULL);\n"
        "}\n\n"
        "void Plugin_%s_Init()\n"
        "{\n"
        "    ctx->Log(LOG_INFO, \"[%s] Initializing...\");\n"
        "    %s_InitDB();\n"
        "    ctx->AddRoute(\"/admin/api/%s/list\", API_%s_List, TRUE, TRUE, 0, 0);\n"
        "    ctx->AddRoute(\"/admin/api/%s/add\", API_%s_Add, TRUE, TRUE, 0, 0);\n"
        "    ctx->Log(LOG_INFO, \"[%s] Initialized!\");\n"
        "}\n\n"
        "void Plugin_%s_Unit()\n"
        "{\n"
        "    ctx->RemoveRoute(\"/admin/api/%s/list\");\n"
        "    ctx->RemoveRoute(\"/admin/api/%s/add\");\n"
        "    ctx->Log(LOG_INFO, \"[%s] Unloaded!\");\n"
        "}\n",
        sCode, modelName, modelName, modelName, modelName, modelName, modelName, modelName, modelName, modelName, modelName, modelName
    );
    
    xrtFree(sCode);
    return sNewCode;
}
```

### 7.2 CompilePlugin

```c
bool PluginCtx_CompilePlugin(str pluginName)
{
    PluginInstance* pPlugin = PluginMgr_GetPlugin(pluginName);
    if ( !pPlugin ) {
        printf("        [Plugin] Plugin not found: %s\n", pluginName);
        return FALSE;
    }
    
    // 如果已加载，先卸载
    if ( pPlugin->bLoaded ) {
        Plugin_Disable(pPlugin);
    }
    
    // 重新加载
    return Plugin_Enable(pPlugin);
}
```

### 7.3 ReloadPlugin

```c
bool PluginCtx_ReloadPlugin(str pluginName)
{
    return PluginCtx_CompilePlugin(pluginName);
}
```

### 7.4 GetPluginConfig / SetPluginConfig

```c
xvalue PluginCtx_GetPluginConfig(str pluginName)
{
    PluginInstance* pPlugin = PluginMgr_GetPlugin(pluginName);
    if ( !pPlugin ) {
        return NULL;
    }
    
    if ( pPlugin->tblSettings ) {
        xvoAddRef(pPlugin->tblSettings);
        return pPlugin->tblSettings;
    }
    
    return NULL;
}

bool PluginCtx_SetPluginConfig(str pluginName, xvalue config)
{
    PluginInstance* pPlugin = PluginMgr_GetPlugin(pluginName);
    if ( !pPlugin || !config ) {
        return FALSE;
    }
    
    if ( pPlugin->tblSettings ) {
        xvoUnref(pPlugin->tblSettings);
    }
    
    xvoAddRef(config);
    pPlugin->tblSettings = config;
    
    Plugin_SaveConfig(pPlugin);
    
    return TRUE;
}
```

---

## 8. 数据库操作接口

### 8.1 CreateTable

```c
bool PluginCtx_CreateTable(str tableName, str sql)
{
    if ( !tableName || !sql ) {
        return FALSE;
    }
    
    char* sErr = NULL;
    int iRet = sqlite3_exec(G_DB->objDB, sql, NULL, NULL, &sErr);
    
    if ( iRet != SQLITE_OK ) {
        printf("        [Plugin] CreateTable failed: %s, error: %s\n", tableName, sErr);
        if ( sErr ) sqlite3_free(sErr);
        return FALSE;
    }
    
    // 记录到 plugin_table 表
    if ( G_CurrentPluginId ) {
        str sInsertSQL = xrtFormat(
            "INSERT INTO plugin_table (plugin_id, table_name, table_type, description, create_time) "
            "VALUES ('%s', '%s', 'data', '', %lld)",
            G_CurrentPluginId, tableName, xrtNow()
        );
        sqlite3_exec(G_DB->objDB, sInsertSQL, NULL, NULL, NULL);
        xrtFree(sInsertSQL);
    }
    
    printf("        [Plugin] Table created: %s\n", tableName);
    return TRUE;
}
```

### 8.2 DropTable

```c
bool PluginCtx_DropTable(str tableName)
{
    if ( !tableName ) {
        return FALSE;
    }
    
    str sSQL = xrtFormat("DROP TABLE IF EXISTS %s", tableName);
    char* sErr = NULL;
    int iRet = sqlite3_exec(G_DB->objDB, sSQL, NULL, NULL, &sErr);
    xrtFree(sSQL);
    
    if ( iRet != SQLITE_OK ) {
        printf("        [Plugin] DropTable failed: %s, error: %s\n", tableName, sErr);
        if ( sErr ) sqlite3_free(sErr);
        return FALSE;
    }
    
    // 从 plugin_table 删除记录
    str sDeleteSQL = xrtFormat("DELETE FROM plugin_table WHERE table_name = '%s'", tableName);
    sqlite3_exec(G_DB->objDB, sDeleteSQL, NULL, NULL, NULL);
    xrtFree(sDeleteSQL);
    
    printf("        [Plugin] Table dropped: %s\n", tableName);
    return TRUE;
}
```

### 8.3 ExecuteSQL

```c
bool PluginCtx_ExecuteSQL(str sql)
{
    if ( !sql ) {
        return FALSE;
    }
    
    char* sErr = NULL;
    int iRet = sqlite3_exec(G_DB->objDB, sql, NULL, NULL, &sErr);
    
    if ( iRet != SQLITE_OK ) {
        printf("        [Plugin] ExecuteSQL failed: %s\n", sErr);
        if ( sErr ) sqlite3_free(sErr);
        return FALSE;
    }
    
    return TRUE;
}
```

### 8.4 QuerySQL

```c
xvalue PluginCtx_QuerySQL(str sql)
{
    if ( !sql ) {
        return NULL;
    }
    
    sqlite3_stmt* stmt;
    int iRet = sqlite3_prepare_v3(G_DB->objDB, sql, -1, 0, &stmt, NULL);
    if ( iRet != SQLITE_OK ) {
        printf("        [Plugin] QuerySQL prepare failed\n");
        return NULL;
    }
    
    xvalue arrResult = xvoCreateArray();
    
    while ( sqlite3_step(stmt) == SQLITE_ROW ) {
        int iColCount = sqlite3_column_count(stmt);
        xvalue tblRow = xvoCreateTable();
        
        for ( int i = 0; i < iColCount; i++ ) {
            str sColName = (str)sqlite3_column_name(stmt, i);
            int iType = sqlite3_column_type(stmt, i);
            
            switch ( iType ) {
                case SQLITE_INTEGER:
                    xvoTableSetInt64(tblRow, sColName, strlen(sColName), sqlite3_column_int64(stmt, i));
                    break;
                case SQLITE_FLOAT:
                    xvoTableSetDouble(tblRow, sColName, strlen(sColName), sqlite3_column_double(stmt, i));
                    break;
                case SQLITE_TEXT:
                    xvoTableSetText(tblRow, sColName, strlen(sColName), (str)sqlite3_column_text(stmt, i), 0, FALSE);
                    break;
                case SQLITE_NULL:
                    xvoTableSetNull(tblRow, sColName, strlen(sColName));
                    break;
            }
        }
        
        xvoArrayAppendValue(arrResult, tblRow, TRUE);
    }
    
    sqlite3_finalize(stmt);
    return arrResult;
}
```

### 8.5 PrepareSQL / ExecuteStmt / FinalizeStmt

```c
sqlite3_stmt* PluginCtx_PrepareSQL(str sql)
{
    if ( !sql ) return NULL;
    
    sqlite3_stmt* stmt;
    int iRet = sqlite3_prepare_v3(G_DB->objDB, sql, -1, 0, &stmt, NULL);
    
    if ( iRet != SQLITE_OK ) {
        printf("        [Plugin] PrepareSQL failed\n");
        return NULL;
    }
    
    return stmt;
}

bool PluginCtx_ExecuteStmt(sqlite3_stmt* stmt)
{
    if ( !stmt ) return FALSE;
    
    int iRet = sqlite3_step(stmt);
    sqlite3_reset(stmt);
    
    return iRet == SQLITE_DONE || iRet == SQLITE_ROW;
}

void PluginCtx_FinalizeStmt(sqlite3_stmt* stmt)
{
    if ( stmt ) {
        sqlite3_finalize(stmt);
    }
}
```

---

## 9. 插件管理接口

### 9.1 InstallPlugin

```c
bool PluginCtx_InstallPlugin(str xpkgPath)
{
    // 预留实现：从 xpkg 安装插件
    printf("        [Plugin] InstallPlugin: %s - NOT IMPLEMENTED\n", xpkgPath);
    return FALSE;
}
```

### 9.2 UninstallPlugin

```c
bool PluginCtx_UninstallPlugin(str pluginName)
{
    PluginInstance* pPlugin = PluginMgr_GetPlugin(pluginName);
    if ( !pPlugin ) {
        return FALSE;
    }
    
    // 先禁用
    if ( pPlugin->bLoaded ) {
        Plugin_Disable(pPlugin);
    }
    
    // 删除插件目录
    str sPluginPath = pPlugin->sPath;
    if ( xrtDirExists(sPluginPath) ) {
        xrtDirDelete(sPluginPath, TRUE);
    }
    
    // 从管理器移除
    xrtDictRemove(G_PluginMgr->tblPlugins, pluginName, strlen(pluginName));
    Plugin_Destroy(pPlugin);
    
    printf("        [Plugin] Plugin uninstalled: %s\n", pluginName);
    return TRUE;
}
```

### 9.3 UpgradePlugin

```c
bool PluginCtx_UpgradePlugin(str pluginName, str newXpkgPath)
{
    // 预留实现：升级插件
    printf("        [Plugin] UpgradePlugin: %s from %s - NOT IMPLEMENTED\n", 
           pluginName, newXpkgPath);
    return FALSE;
}
```

---

## 10. 模板渲染接口

### 10.1 RenderTemplate

```c
str PluginCtx_RenderTemplate(str templatePath, xvalue data)
{
    if ( !templatePath ) {
        return NULL;
    }
    
    size_t iRetSize = 0;
    str sHtml = MakePageWithTemplate(templatePath, data, &iRetSize);
    
    if ( !sHtml ) {
        printf("        [Plugin] RenderTemplate failed: %s\n", templatePath);
        return NULL;
    }
    
    return sHtml;
}
```

### 10.2 RenderString

```c
str PluginCtx_RenderString(str templateString, xvalue data)
{
    if ( !templateString || !data ) {
        return NULL;
    }
    
    // 使用 xrt 的模板引擎
    str sResult = xrtRenderString(templateString, data);
    
    if ( !sResult ) {
        printf("        [Plugin] RenderString failed\n");
        return NULL;
    }
    
    return sResult;
}
```

---

## 11. 模型生成器插件规格

### 11.1 插件概述

模型生成器插件通过预定义模板快速创建内容管理插件，如文章系统、新闻系统、下载系统等。

### 11.2 目录结构

```
script/plugin/model_generator/
├── config.json
├── main.c
└── templates/
    ├── article.json
    ├── news.json
    ├── download.json
    └── product.json
```

### 11.3 config.json

```json
{
    "name": "model_generator",
    "title": "模型生成器",
    "desc": "通过配置模板快速创建内容管理模型",
    "version": "1.0.0",
    "author": "xAdmin",
    "sort": 50,
    "enabled": true,
    "dependencies": [],
    "exports": [],
    "settings": {
        "defaultNamespace": "cms"
    }
}
```

### 11.4 模板文件格式 (templates/article.json)

```json
{
    "templateId": "article",
    "templateTitle": "文章系统",
    "templateDesc": "标准的文章内容管理",
    "templateIcon": "layui-icon layui-icon-read",
    "config": {
        "namespace": "cms",
        "modelName": "article",
        "modelTitle": "文章",
        "features": {
            "enableApi": true,
            "enableAdmin": true,
            "enableSubmit": true,
            "enableReply": true,
            "enableAccessControl": false
        },
        "fields": [
            {
                "name": "title",
                "type": "TEXT",
                "label": "标题",
                "required": true,
                "maxLength": 200,
                "searchable": true,
                "showInList": true,
                "showInForm": true
            },
            {
                "name": "content",
                "type": "TEXT",
                "label": "内容",
                "required": true,
                "widget": "editor",
                "showInList": false,
                "showInForm": true
            },
            {
                "name": "categoryId",
                "type": "INTEGER",
                "label": "分类",
                "widget": "select",
                "showInList": true,
                "showInForm": true
            },
            {
                "name": "coverImage",
                "type": "TEXT",
                "label": "封面图",
                "widget": "image",
                "showInList": true,
                "showInForm": true
            },
            {
                "name": "status",
                "type": "INTEGER",
                "label": "状态",
                "widget": "radio",
                "defaultValue": 0,
                "options": [
                    {"value": 0, "label": "草稿"},
                    {"value": 1, "label": "发布"},
                    {"value": 2, "label": "下架"}
                ],
                "showInList": true,
                "showInForm": true
            },
            {
                "name": "views",
                "type": "INTEGER",
                "label": "浏览量",
                "defaultValue": 0,
                "showInList": true,
                "showInForm": false
            }
        ]
    }
}
```

### 11.5 main.c 核心功能

```c
#include "../plugin.h"

PluginContext* ctx;
xvalue g_tblSettings = NULL;
xvalue g_tblTemplates = NULL;
int g_iMenuId = 0;

void Plugin_SetGlobalData(int idx, void* ptr)
{
    if ( idx == 1 ) ctx = (PluginContext*)ptr;
    else if ( idx == 2 ) g_tblSettings = (xvalue)ptr;
}

// 加载模板文件
xvalue ModelGen_LoadTemplates()
{
    str sTemplateDir = xrtPathJoin(3, ctx->GetPluginPath(), "templates", "");
    
    xvalue arrTemplates = xvoCreateArray();
    
    typedef struct {
        str sFile;
        str sId;
    } TemplateFile;
    
    TemplateFile files[] = {
        {"article.json", "article"},
        {"news.json", "news"},
        {"download.json", "download"},
        {"product.json", "product"}
    };
    
    for ( int i = 0; i < 4; i++ ) {
        str sFilePath = xrtPathJoin(2, sTemplateDir, files[i].sFile);
        
        size_t iLen = 0;
        str sContent = NULL;
        if ( ctx->ReadFile(sFilePath, &sContent, &iLen) ) {
            xvalue tblTemplate = ctx->JsonParse(sContent, iLen);
            xrtFree(sContent);
            
            if ( tblTemplate ) {
                xvoArrayAppendValue(arrTemplates, tblTemplate, TRUE);
            }
        }
        
        xrtFree(sFilePath);
    }
    
    xrtFree(sTemplateDir);
    return arrTemplates;
}

// API: 获取模板列表
void API_ModelGen_Templates(void* s, void* h, struct mg_connection* c, struct mg_http_message* hm)
{
    xvalue ret = xvoCreateTable();
    xvoTableSetBool(ret, "result", 6, TRUE);
    xvoTableSetValue(ret, "data", 4, g_tblTemplates, FALSE);
    
    size_t iLen = 0;
    str sJson = ctx->JsonStringify(ret, &iLen);
    ctx->SendJson(c, 200, sJson, iLen);
    ctx->Free(sJson);
    xvoUnref(ret);
}

// API: 获取模板详情
void API_ModelGen_TemplateDetail(void* s, void* h, struct mg_connection* c, struct mg_http_message* hm)
{
    xvalue form = ctx->JsonParse(hm->body.buf, hm->body.len);
    if ( !form ) {
        ctx->SendJson(c, 200, "{\"result\":false,\"message\":\"参数错误\"}", 0);
        return;
    }
    
    str sTemplateId = xvoTableGetText(form, "templateId", 11);
    xvoUnref(form);
    
    if ( !sTemplateId ) {
        ctx->SendJson(c, 200, "{\"result\":false,\"message\":\"缺少 templateId\"}", 0);
        return;
    }
    
    int iCount = xvoArrayCount(g_tblTemplates);
    xvalue tblTemplate = NULL;
    
    for ( int i = 0; i < iCount; i++ ) {
        xvalue tbl = xvoArrayGetValue(g_tblTemplates, i);
        str sId = xvoTableGetText(tbl, "templateId", 11);
        
        if ( sId && strcmp(sId, sTemplateId) == 0 ) {
            tblTemplate = tbl;
            break;
        }
    }
    
    if ( !tblTemplate ) {
        ctx->SendJson(c, 200, "{\"result\":false,\"message\":\"模板不存在\"}", 0);
        return;
    }
    
    xvalue ret = xvoCreateTable();
    xvoTableSetBool(ret, "result", 6, TRUE);
    xvoTableSetValue(ret, "data", 4, tblTemplate, FALSE);
    
    size_t iLen = 0;
    str sJson = ctx->JsonStringify(ret, &iLen);
    ctx->SendJson(c, 200, sJson, iLen);
    ctx->Free(sJson);
    xvoUnref(ret);
}

// API: 创建模型
void API_ModelGen_Create(void* s, void* h, struct mg_connection* c, struct mg_http_message* hm)
{
    xvalue form = ctx->JsonParse(hm->body.buf, hm->body.len);
    if ( !form ) {
        ctx->SendJson(c, 200, "{\"result\":false,\"message\":\"参数错误\"}", 0);
        return;
    }
    
    str sTemplateId = xvoTableGetText(form, "templateId", 11);
    str sModelName = xvoTableGetText(form, "modelName", 10);
    str sModelTitle = xvoTableGetText(form, "modelTitle", 11);
    
    if ( !sTemplateId || !sModelName ) {
        xvoUnref(form);
        ctx->SendJson(c, 200, "{\"result\":false,\"message\":\"缺少必需参数\"}", 0);
        return;
    }
    
    // 查找模板
    xvalue tblTemplate = NULL;
    int iCount = xvoArrayCount(g_tblTemplates);
    for ( int i = 0; i < iCount; i++ ) {
        xvalue tbl = xvoArrayGetValue(g_tblTemplates, i);
        str sId = xvoTableGetText(tbl, "templateId", 11);
        if ( sId && strcmp(sId, sTemplateId) == 0 ) {
            tblTemplate = tbl;
            break;
        }
    }
    
    if ( !tblTemplate ) {
        xvoUnref(form);
        ctx->SendJson(c, 200, "{\"result\":false,\"message\":\"模板不存在\"}", 0);
        return;
    }
    
    // 获取模板配置
    xvalue tblConfig = xvoTableGetValue(tblTemplate, "config", 6);
    
    // 应用自定义配置
    if ( sModelName ) {
        xvoTableSetText(tblConfig, "modelName", 10, sModelName, 0, FALSE);
    }
    if ( sModelTitle ) {
        xvoTableSetText(tblConfig, "modelTitle", 11, sModelTitle, 0, FALSE);
    }
    
    // 生成模型插件
    bool bResult = ctx->GenerateModel(sModelName, tblConfig);
    
    xvoUnref(form);
    
    xvalue ret = xvoCreateTable();
    xvoTableSetBool(ret, "result", 6, bResult);
    xvoTableSetText(ret, "message", 7, bResult ? "创建成功" : "创建失败", 0, FALSE);
    
    size_t iLen = 0;
    str sJson = ctx->JsonStringify(ret, &iLen);
    ctx->SendJson(c, 200, sJson, iLen);
    ctx->Free(sJson);
    xvoUnref(ret);
}

void Plugin_model_generator_Init()
{
    ctx->Log(LOG_INFO, "[ModelGenerator] Initializing...");
    
    g_tblTemplates = ModelGen_LoadTemplates();
    
    ctx->AddRoute("/admin/api/model_generator/templates", API_ModelGen_Templates, TRUE, TRUE, 0, 0);
    ctx->AddRoute("/admin/api/model_generator/template_detail", API_ModelGen_TemplateDetail, TRUE, TRUE, 0, 0);
    ctx->AddRoute("/admin/api/model_generator/create", API_ModelGen_Create, TRUE, TRUE, 0, 0);
    
    g_iMenuId = ctx->AddMenu(0, "模型生成器", "layui-icon layui-icon-template-1", 
                             1, "_component", "/admin/view/model_generator", 100, TRUE);
    
    ctx->Log(LOG_INFO, "[ModelGenerator] Initialized!");
}

void Plugin_model_generator_Unit()
{
    if ( g_iMenuId > 0 ) {
        ctx->RemoveMenu(g_iMenuId);
    }
    
    ctx->RemoveRoute("/admin/api/model_generator/templates");
    ctx->RemoveRoute("/admin/api/model_generator/template_detail");
    ctx->RemoveRoute("/admin/api/model_generator/create");
    
    if ( g_tblTemplates ) {
        xvoUnref(g_tblTemplates);
    }
    
    ctx->Log(LOG_INFO, "[ModelGenerator] Unloaded!");
}
```

---

## 12. 留言板插件规格

### 12.1 插件概述

留言板插件支持多实例，可以在网站不同位置嵌入多个独立的留言板。

### 12.2 目录结构

```
script/plugin/guestbook/
├── config.json
└── main.c
```

### 12.3 config.json

```json
{
    "name": "guestbook",
    "title": "留言板",
    "desc": "支持多实例的留言板系统",
    "version": "1.0.0",
    "author": "xAdmin",
    "sort": 100,
    "enabled": true,
    "dependencies": [],
    "exports": [],
    "settings": {}
}
```

### 12.4 main.c 核心功能

```c
#include "../plugin.h"

PluginContext* ctx;
xvalue g_tblSettings = NULL;
xdict g_tblInstances = NULL;
int g_iMenuId = 0;

typedef struct {
    str sInstanceId;
    str sName;
    str sDescription;
    int iMaxMessages;
    bool bRequireLogin;
    bool bRequireApproval;
    str sTheme;
    sqlite3_stmt* stmtAdd;
    sqlite3_stmt* stmtList;
} GuestbookInstance;

void Plugin_SetGlobalData(int idx, void* ptr)
{
    if ( idx == 1 ) ctx = (PluginContext*)ptr;
    else if ( idx == 2 ) g_tblSettings = (xvalue)ptr;
}

// 创建留言板实例
GuestbookInstance* Guestbook_CreateInstance(str sInstanceId, str sName, str sDescription)
{
    GuestbookInstance* pInst = xrtMalloc(sizeof(GuestbookInstance));
    memset(pInst, 0, sizeof(GuestbookInstance));
    
    pInst->sInstanceId = xrtCopyStr(sInstanceId, 0);
    pInst->sName = xrtCopyStr(sName ? sName : sInstanceId, 0);
    pInst->sDescription = xrtCopyStr(sDescription ? sDescription : "", 0);
    pInst->iMaxMessages = 100;
    pInst->bRequireLogin = FALSE;
    pInst->bRequireApproval = FALSE;
    pInst->sTheme = xrtCopyStr("default", 0);
    
    return pInst;
}

// 销毁留言板实例
void Guestbook_DestroyInstance(GuestbookInstance* pInst)
{
    if ( !pInst ) return;
    
    if ( pInst->stmtAdd ) sqlite3_finalize(pInst->stmtAdd);
    if ( pInst->stmtList ) sqlite3_finalize(pInst->stmtList);
    
    xrtFree(pInst->sInstanceId);
    xrtFree(pInst->sName);
    xrtFree(pInst->sDescription);
    xrtFree(pInst->sTheme);
    xrtFree(pInst);
}

// 初始化留言板数据表
bool Guestbook_InitTable(GuestbookInstance* pInst)
{
    str sTableName = xrtFormat("guestbook_%s", pInst->sInstanceId);
    
    str sSQL = xrtFormat(
        "CREATE TABLE IF NOT EXISTS %s ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "memberId INTEGER DEFAULT 0,"
        "username TEXT NOT NULL,"
        "content TEXT NOT NULL,"
        "status INTEGER DEFAULT 0,"
        "reply TEXT,"
        "replyTime INTEGER,"
        "createTime INTEGER NOT NULL"
        ")",
        sTableName
    );
    
    bool bResult = ctx->CreateTable(sTableName, sSQL);
    
    xrtFree(sTableName);
    xrtFree(sSQL);
    
    if ( !bResult ) return FALSE;
    
    // 预编译语句
    str sAddSQL = xrtFormat(
        "INSERT INTO guestbook_%s (memberId, username, content, status, createTime) VALUES (?, ?, ?, 0, ?)",
        pInst->sInstanceId
    );
    sqlite3_prepare_v3(ctx->pDB->objDB, sAddSQL, -1, 0, &pInst->stmtAdd, NULL);
    xrtFree(sAddSQL);
    
    str sListSQL = xrtFormat(
        "SELECT id, memberId, username, content, status, reply, replyTime, createTime FROM guestbook_%s ORDER BY id DESC LIMIT ?",
        pInst->sInstanceId
    );
    sqlite3_prepare_v3(ctx->pDB->objDB, sListSQL, -1, 0, &pInst->stmtList, NULL);
    xrtFree(sListSQL);
    
    return TRUE;
}

// API: 获取留言列表
void API_Guestbook_List(void* s, void* h, struct mg_connection* c, struct mg_http_message* hm)
{
    xvalue form = ctx->JsonParse(hm->body.buf, hm->body.len);
    if ( !form ) {
        ctx->SendJson(c, 200, "{\"result\":false,\"message\":\"参数错误\"}", 0);
        return;
    }
    
    str sInstanceId = xvoTableGetText(form, "instanceId", 11);
    xvoUnref(form);
    
    if ( !sInstanceId ) {
        ctx->SendJson(c, 200, "{\"result\":false,\"message\":\"缺少 instanceId\"}", 0);
        return;
    }
    
    GuestbookInstance** ppInst = xrtDictGet(g_tblInstances, sInstanceId, strlen(sInstanceId));
    if ( !ppInst || !(*ppInst) ) {
        ctx->SendJson(c, 200, "{\"result\":false,\"message\":\"留言板不存在\"}", 0);
        return;
    }
    
    GuestbookInstance* pInst = *ppInst;
    
    // 删除数据表
    str sTableName = xrtFormat("guestbook_%s", pInst->sInstanceId);
    ctx->DropTable(sTableName);
    xrtFree(sTableName);
    
    // 销毁实例
    Guestbook_DestroyInstance(pInst);
    
    // 从字典移除
    xrtDictRemove(g_tblInstances, sInstanceId, strlen(sInstanceId));
    
    ctx->SendJson(c, 200, "{\"result\":true,\"message\":\"删除成功\"}", 0);
}

void Plugin_guestbook_Init()
{
    ctx->Log(LOG_INFO, "[Guestbook] Initializing...");
    
    g_tblInstances = xrtDictCreate(sizeof(ptr));
    
    ctx->AddRoute("/admin/api/guestbook/list", API_Guestbook_List, TRUE, TRUE, 0, 0);
    ctx->AddRoute("/admin/api/guestbook/add", API_Guestbook_Add, TRUE, TRUE, 0, 0);
    ctx->AddRoute("/admin/api/guestbook/create_instance", API_Guestbook_CreateInstance, TRUE, TRUE, 0, 0);
    ctx->AddRoute("/admin/api/guestbook/delete_instance", API_Guestbook_DeleteInstance, TRUE, TRUE, 0, 0);
    
    g_iMenuId = ctx->AddMenu(0, "留言板", "layui-icon layui-icon-dialogue", 
                             1, "_component", "/admin/view/guestbook", 100, TRUE);
    
    ctx->Log(LOG_INFO, "[Guestbook] Initialized!");
}

void Plugin_guestbook_Unit()
{
    if ( g_iMenuId > 0 ) {
        ctx->RemoveMenu(g_iMenuId);
    }
    
    ctx->RemoveRoute("/admin/api/guestbook/list");
    ctx->RemoveRoute("/admin/api/guestbook/add");
    ctx->RemoveRoute("/admin/api/guestbook/create_instance");
    ctx->RemoveRoute("/admin/api/guestbook/delete_instance");
    
    // 清理所有实例
    if ( g_tblInstances ) {
        xdict tbl = g_tblInstances;
        // ... 遍历清理
        xrtDictDestroy(g_tblInstances);
        g_tblInstances = NULL;
    }
    
    ctx->Log(LOG_INFO, "[Guestbook] Unloaded!");
}
```

---

## 13. 网络插件预留

### 13.1 概述

网络插件商店是一个独立的大型系统，当前仅预留接口和数据库字段，暂不实现。

### 13.2 预留接口

```c
// 预留：检查插件更新
bool PluginCtx_CheckUpdate(str pluginName);

// 预留：下载插件
bool PluginCtx_DownloadPlugin(str pluginId, str downloadUrl);

// 预留：验证插件校验和
bool PluginCtx_VerifyChecksum(str xpkgPath, str expectedChecksum);
```

### 13.3 预留数据库字段

plugin 表中已预留以下字段：

- `from_network` - 是否来自网络
- `download_url` - 下载地址
- `checksum` - SHA256 校验和
- `verified` - 是否已验证
- `latest_version` - 最新版本
- `update_available` - 是否有可用更新
- `update_check_time` - 更新检查时间

### 13.4 未来扩展

网络插件商店系统需要独立设计，包括：

1. 插件仓库服务器
2. 插件上传/审核流程
3. 版本管理
4. 下载/安装/更新流程
5. 评分/评论系统
6. 依赖解析

---

## 附录

### A. 实施步骤

1. **数据库迁移**
   - 执行 ALTER TABLE 添加 plugin_id 字段
   - 创建新表：plugin, plugin_dependency, plugin_table, plugin_install_log

2. **PluginContext 扩展**
   - 在 plugin_ctx.h 中添加新接口定义
   - 在 plugin_mgr.h 中实现新接口

3. **自动资源关联**
   - 修改 AddRoute 添加 plugin_id 写入逻辑
   - 修改 AddMenu 添加 plugin_id 写入逻辑
   - 实现 Plugin_CleanupResources
   - 修改 Plugin_Enable/Disable 设置全局变量

4. **文件操作实现**
   - 实现所有文件操作接口

5. **数据库操作实现**
   - 实现数据库操作接口

6. **插件开发**
   - 实现模型生成器插件
   - 实现留言板插件

### B. 安全注意事项

- 插件代码必须经过插件商店严格审核
- 插件运行时无权限限制，可访问所有系统功能
- 插件开发者应遵循最佳实践，避免恶意行为
- 系统管理员应谨慎安装未知来源的插件

### C. 兼容性说明

- 本扩展向后兼容现有插件系统
- 现有插件无需修改即可继续使用
- 新接口仅对使用它们的插件生效
- 插件卸载时会自动清理关联资源