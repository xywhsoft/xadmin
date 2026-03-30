

// ============================================
// 插件上下文定�?
// ============================================

#ifndef PLUGIN_CTX_H
#define PLUGIN_CTX_H




// ==================== 预定义事�?====================

// 系统事件
#define EVENT_SYSTEM_READY          "system.ready"           // 系统启动完成
#define EVENT_SYSTEM_SHUTDOWN       "system.shutdown"        // 系统关闭�?

// 用户事件（由 member 插件触发�?
#define EVENT_MEMBER_LOGIN          "member.login"           // 前台用户登录
#define EVENT_MEMBER_LOGOUT         "member.logout"          // 前台用户登出
#define EVENT_MEMBER_REGISTER       "member.register"        // 前台用户注册
#define EVENT_MEMBER_BALANCE_CHANGE "member.balance.change"  // 余额变动

// 附件事件（由 attachment 插件触发�?
#define EVENT_ATTACHMENT_UPLOAD     "attachment.upload"      // 附件上传
#define EVENT_ATTACHMENT_DELETE     "attachment.delete"      // 附件删除
#define EVENT_ATTACHMENT_PURCHASE   "attachment.purchase"    // 附件购买

// 模型事件（由 model 插件触发�?
#define EVENT_MODEL_ENABLE          "model.enable"           // 模型启用
#define EVENT_MODEL_DISABLE         "model.disable"          // 模型禁用
#define EVENT_MODEL_DATA_ADD        "model.data.add"         // 模型数据添加
#define EVENT_MODEL_DATA_UPDATE     "model.data.update"      // 模型数据更新
#define EVENT_MODEL_DATA_DELETE     "model.data.delete"      // 模型数据删除



// ==================== 日志级别 ====================

#define LOG_DEBUG   0
#define LOG_INFO    1
#define LOG_WARN    2
#define LOG_ERROR   3



// ==================== 类型定义 ====================

typedef bool (*DirScanCallback)(str path, size_t size, int type, ptr data, size_t pathSize);



// ==================== 插件上下文结�?====================

typedef struct {
	
	// ===== 核心数据 =====
	sqlite3* pDB;                    // 数据库连�?
	xvalue* pAdminSession;              // 后台 Session 表指�?
	xvalue* pMemberSession;             // 前台 Session 表指�?
	xvalue* pOption;                    // 全局配置表指�?
	
	// ===== 路径信息 =====
	str sAppPath;                       // 应用根目�?
	str sWebPath;                       // Web 根目�?
	str sDataPath;                      // 数据目录
	str sPluginPath;                    // 插件目录
	str sPagePath;                      // 页面模板目录
	
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
	str (*CreateAdminSession)(int64 userId, str userName, int roleId, int timeout);
	str (*CreateMemberSession)(int64 userId, str userName, int groupId, int timeout);
	void (*DestroyAdminSession)(str token);
	void (*DestroyMemberSession)(str token);
	void (*ExtendSession)(bool isAdmin, str token, int timeout);
	
	// ===== HTTP 响应 =====
	void (*SendJson)(struct mg_connection* c, int code, str json, size_t len);
	void (*SendHtml)(struct mg_connection* c, int code, str html);
	void (*SendPage)(struct mg_connection* c, str pagePath, xvalue data);
	void (*LoadPage)(struct mg_connection* c, int code, str head, str pagePath);
	void (*SendFile)(struct mg_connection* c, str filePath, str mimeType);
	void (*SendError)(struct mg_connection* c, int code, str message);
	
	// ===== 配置操作 =====
	xvalue (*GetOption)(str group, str key);
	bool (*SetOption)(str group, str key, xvalue value);
	void (*ReloadOption)(str group);
	
	// ===== JSON 操作 =====
	xvalue (*JsonParse)(str json, size_t len);
	str (*JsonStringify)(xvalue val, size_t* outLen);
	void (*JsonFree)(xvalue val);
	
	// ===== 工具函数 =====
	int64 (*TimeNow)();
	str (*Format)(str fmt, ...);
	void (*Free)(void* ptr);
	str (*HashPassword)(str user, str salt, str clientHash);
	str (*GenerateSalt)();
	str (*GenerateToken)(int length);
	
	// ===== 日志 =====
	void (*Log)(int level, str format, ...);
	void (*LogAccess)(str user, str uri, str method, str param, str body);

	// ===== 插件间通信 =====
	void* (*GetPluginExport)(str pluginName, str exportName);
	bool (*SetPluginExport)(str pluginName, str exportName, void* ptr);

	// ===== 事件系统 =====
	bool (*EmitEvent)(str eventName, xvalue eventData);
	bool (*OnEvent)(str eventName, void* callback);
	void (*OffEvent)(str eventName, void* callback);

	// ===== 插件自身信息 =====
	str (*GetPluginId)();
	str (*GetPluginName)();
	str (*GetPluginPath)();

	// ===== 文件操作 =====
	bool (*WriteFile)(str filePath, str content, size_t len);
	bool (*ReadFile)(str filePath, str* outContent, size_t* outLen);
	bool (*DeleteFile)(str filePath);
	bool (*FileExists)(str filePath);
	bool (*CreateDir)(str dirPath);
	bool (*DeleteDir)(str dirPath, bool bRecursive);
	bool (*DirExists)(str dirPath);
	bool (*ScanDir)(str dirPath, bool bRecursive, DirScanCallback callback, ptr userData);
	bool (*CopyFile)(str srcPath, str destPath);
	bool (*MoveFile)(str srcPath, str destPath);

	// ===== xPack 集成 =====
	int (*CreateXpkg)(str outputPath, str* fileList, int fileCount, int compressLevel);
	int (*ExtractXpkg)(str xpkgPath, str outputDir);
	xvalue (*GetXpkgInfo)(str xpkgPath);

	// ===== 代码生成 =====
	bool (*GenerateModel)(str modelName, xvalue modelConfig);
	bool (*CompilePlugin)(str pluginName);
	bool (*ReloadPlugin)(str pluginName);
	xvalue (*GetPluginConfig)(str pluginName);
	bool (*SetPluginConfig)(str pluginName, xvalue config);

	// ===== 数据库操�?=====
	bool (*CreateTable)(str tableName, str sql);
	bool (*DropTable)(str tableName);
	bool (*ExecuteSQL)(str sql);
	xvalue (*QuerySQL)(str sql);
	sqlite3_stmt* (*PrepareSQL)(str sql);
	bool (*ExecuteStmt)(sqlite3_stmt* stmt);
	void (*FinalizeStmt)(sqlite3_stmt* stmt);

	// ===== 插件管理 =====
	bool (*InstallPlugin)(str xpkgPath);
	bool (*UninstallPlugin)(str pluginName);
	bool (*UpgradePlugin)(str pluginName, str newXpkgPath);

	// ===== 模板渲染 =====
	str (*RenderTemplate)(str templatePath, xvalue data);
	str (*RenderString)(str templateString, xvalue data);

} PluginContext;



// ==================== 事件回调函数类型 ====================

typedef void (*PluginEventCallback)(str eventName, xvalue eventData);



// ==================== 事件监听器结�?====================

typedef struct {
	str sEventName;                     // 事件名称
	xlist lstCallbacks;                 // 回调函数列表
} EventListener;



// ==================== 插件导出项结�?====================

typedef struct {
	str sPluginName;                    // 插件名称
	str sExportName;                    // 导出名称
	void* pPtr;                         // 导出指针
} PluginExport;



// ==================== 插件依赖项结构 =============