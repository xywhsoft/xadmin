


// ============================================
// xAdmin 插件公共头文件
// 所有插件都应包含此头文件
// ============================================

#ifndef XADMIN_PLUGIN_H
#define XADMIN_PLUGIN_H



// 引入 xserver 基础库
#include <xsbase.h>



// ==================== 预定义事件 ====================

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



// ==================== 日志级别 ====================

#define LOG_DEBUG   0
#define LOG_INFO    1
#define LOG_WARN    2
#define LOG_ERROR   3



// ==================== HTTP 响应常量 ====================

#define HTTP_CT_HTML "Content-Type: text/html\r\n"
#define HTTP_CT_TEXT "Content-Type: text/plain\r\n"
#define HTTP_CT_JSON "Content-Type: application/json\r\n"



// ==================== 路由信息结构 ====================

typedef struct {
	void (*Proc)(void*, void*, struct mg_connection*, struct mg_http_message*);
	bool bAuth;
	bool bAdmin;
	bool bPutLog;
	bool bActive;
	uint32 AuthID;
	uint32 AuthLevel;
} RouteInfo;



// ==================== 插件上下文结构 ====================

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
	
} PluginContext;



// ==================== 插件必须实现的函数 ====================

// 全局上下文变量（由 Plugin_SetGlobalData 传入）
// 插件代码中需要声明: PluginContext* ctx;

// 接收主系统传递的全局数据
// 插件代码中需要实现:
// void Plugin_SetGlobalData(int idx, void* ptr)
// {
//     if ( idx == 1 ) {
//         ctx = (PluginContext*)ptr;
//     }
// }

// 插件初始化函数（启用时调用）
// 命名格式: Plugin_{插件名}_Init
// 例如: void Plugin_member_Init()
// 注意: 使用命名空间宏后，无需手动添加前缀

// 插件卸载函数（禁用时调用）
// 命名格式: Plugin_{插件名}_Unit
// 例如: void Plugin_member_Unit()
// 注意: 使用命名空间宏后，无需手动添加前缀



// ==================== 命名空间宏（V2新增）====================

// 插件命名空间自动添加前缀，避免符号冲突
// 系统会自动在代码头部注入宏定义：
// #define PLUGIN_NS(name) _plugin_{plugin_name}_##name
// #define PLUGIN_API(name) PLUGIN_NS(API_##name)
// #define PLUGIN_FUNC(name) PLUGIN_NS(name)

// 使用示例:
// void PLUGIN_FUNC(Init)() { ... }                    // 展开为 void _plugin_hello_Init()
// void PLUGIN_FUNC(Unit)() { ... }                    // 展开为 void _plugin_hello_Unit()
// void PLUGIN_API(Greeting)(...) { ... }              // 展开为 void _plugin_hello_API_Greeting()
// void Plugin_SetGlobalData(int idx, void* ptr) { ... }

// 注意:
// 1. Plugin_SetGlobalData 函数名保持不变，系统会自动添加前缀
// 2. 路由处理函数建议使用 PLUGIN_API 宏，例如 PLUGIN_API(MyHandler)
// 3. 内部函数建议使用 PLUGIN_FUNC 宏，例如 PLUGIN_FUNC(DoSomething)
// 4. 前缀由插件名自动生成，格式: _plugin_{name}_



// ==================== 便捷宏定义 ====================

// SQL 预编译默认选项
#define SQL_PREPARE_DEFAULT  SQLITE_PREPARE_PERSISTENT | SQLITE_PREPARE_DONT_LOG

// 快速发送 JSON 响应
#define SEND_JSON_OK(c, msg) \
	ctx->SendJson(c, 200, "{\"result\":true,\"message\":\"" msg "\"}", 0)

#define SEND_JSON_ERR(c, msg) \
	ctx->SendJson(c, 200, "{\"result\":false,\"message\":\"" msg "\"}", 0)

// HTTP 方法检查
#define CHECK_METHOD_GET(c, hm) \
	if ( hm->methodCode != HTTP_GET ) { \
		SEND_JSON_ERR(c, "Method not allowed"); \
		return; \
	}

#define CHECK_METHOD_POST(c, hm) \
	if ( hm->methodCode != HTTP_POST ) { \
		SEND_JSON_ERR(c, "Method not allowed"); \
		return; \
	}



#endif // XADMIN_PLUGIN_H

