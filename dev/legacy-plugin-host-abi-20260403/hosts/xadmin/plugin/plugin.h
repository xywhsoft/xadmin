#ifndef XADMIN_PLUGIN_H
#define XADMIN_PLUGIN_H

#include <xsbase.h>

#define EVENT_SYSTEM_READY			"system.ready"
#define EVENT_SYSTEM_SHUTDOWN			"system.shutdown"
#define EVENT_MEMBER_LOGIN			"member.login"
#define EVENT_MEMBER_LOGOUT			"member.logout"
#define EVENT_MEMBER_REGISTER			"member.register"
#define EVENT_MEMBER_BALANCE_CHANGE		"member.balance.change"
#define EVENT_ATTACHMENT_UPLOAD			"attachment.upload"
#define EVENT_ATTACHMENT_DELETE			"attachment.delete"
#define EVENT_ATTACHMENT_PURCHASE		"attachment.purchase"
#define EVENT_MODEL_ENABLE			"model.enable"
#define EVENT_MODEL_DISABLE			"model.disable"
#define EVENT_MODEL_DATA_ADD			"model.data.add"
#define EVENT_MODEL_DATA_UPDATE			"model.data.update"
#define EVENT_MODEL_DATA_DELETE			"model.data.delete"

#define LOG_DEBUG	0
#define LOG_INFO	1
#define LOG_WARN	2
#define LOG_ERROR	3

#define HTTP_CT_HTML	"Content-Type: text/html\r\n"
#define HTTP_CT_TEXT	"Content-Type: text/plain\r\n"
#define HTTP_CT_JSON	"Content-Type: application/json\r\n"

typedef struct HttpMultipartPart {
	const char* sName;
	size_t iNameLen;
	const char* sFileName;
	size_t iFileNameLen;
	const char* pBody;
	size_t iBodyLen;
} HttpMultipartPart;

typedef struct {
	void (*Proc)(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession);
	bool bAuth;
	bool bAdmin;
	bool bPutLog;
	bool bActive;
	uint32 AuthID;
	uint32 AuthLevel;
} RouteInfo;

typedef bool (*DirScanCallback)(str path, size_t size, int type, ptr data, size_t pathSize);

typedef struct {
	sqlite3* pDB;
	xvalue* pAdminSession;
	xvalue* pMemberSession;
	xvalue* pOption;

	str sAppPath;
	str sWebPath;
	str sDataPath;
	str sPluginPath;
	str sPagePath;

	RouteInfo* (*AddRoute)(str uri, void* proc, bool bAuth, bool bAdmin, int authId, int authLevel);
	void (*RemoveRoute)(str uri);
	RouteInfo* (*GetRoute)(str uri);

	int (*AddMenu)(int parent, str title, str icon, int type, str openType, str href, int sort, bool visible);
	bool (*RemoveMenu)(int menuId);
	bool (*ShowMenu)(int menuId);
	bool (*HideMenu)(int menuId);

	int (*AddAuthGroup)(str name, str desc, int sort);
	int (*AddAuth)(int groupId, str name, str desc, int sort);
	bool (*RemoveAuthGroup)(int groupId);
	bool (*RemoveAuth)(int authId);
	void (*SyncUriAuth)(str uri, int authId, str desc, bool isBackend, bool needAuth, bool needLog);
	void (*ReloadAuthCache)();

	xvalue (*GetAdminSession)(str token);
	xvalue (*GetMemberSession)(str token);
	str (*CreateAdminSession)(int64 userId, str userName, int roleId, int timeout);
	str (*CreateMemberSession)(int64 userId, str userName, int groupId, int timeout);
	void (*DestroyAdminSession)(str token);
	void (*DestroyMemberSession)(str token);
	void (*ExtendSession)(bool isAdmin, str token, int timeout);

	void (*SendJson)(XS_ResponseObject objResp, int code, str json, size_t len);
	void (*SendHtml)(XS_ResponseObject objResp, int code, str html);
	void (*SendPage)(XS_ResponseObject objResp, str pagePath, xvalue data);
	void (*LoadPage)(XS_ResponseObject objResp, int code, str head, str pagePath);
	void (*SendFile)(XS_ResponseObject objResp, str filePath, str mimeType);
	void (*SendError)(XS_ResponseObject objResp, int code, str message);

	xvalue (*GetOption)(str group, str key);
	bool (*SetOption)(str group, str key, xvalue value);
	void (*ReloadOption)(str group);

	xvalue (*JsonParse)(str json, size_t len);
	str (*JsonStringify)(xvalue val, size_t* outLen);
	void (*JsonFree)(xvalue val);

	int64 (*TimeNow)();
	str (*Format)(str fmt, ...);
	void (*Free)(void* ptr);
	str (*HashPassword)(str user, str salt, str clientHash);
	str (*GenerateSalt)();
	str (*GenerateToken)(int length);

	void (*Log)(int level, str format, ...);
	void (*LogAccess)(str user, str uri, str method, str param, str body);

	void* (*GetPluginExport)(str pluginName, str exportName);
	bool (*SetPluginExport)(str pluginName, str exportName, void* ptr);

	bool (*EmitEvent)(str eventName, xvalue eventData);
	bool (*OnEvent)(str eventName, void* callback);
	void (*OffEvent)(str eventName, void* callback);

	str (*GetPluginId)();
	str (*GetPluginName)();
	str (*GetPluginPath)();

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

	int (*CreateXpkg)(str outputPath, str* fileList, int fileCount, int compressLevel);
	int (*ExtractXpkg)(str xpkgPath, str outputDir);
	xvalue (*GetXpkgInfo)(str xpkgPath);

	bool (*GenerateModel)(str modelName, xvalue modelConfig);
	bool (*CompilePlugin)(str pluginName);
	bool (*ReloadPlugin)(str pluginName);
	xvalue (*GetPluginConfig)(str pluginName);
	bool (*SetPluginConfig)(str pluginName, xvalue config);

	bool (*CreateTable)(str tableName, str sql);
	bool (*DropTable)(str tableName);
	bool (*ExecuteSQL)(str sql);
	xvalue (*QuerySQL)(str sql);
	sqlite3_stmt* (*PrepareSQL)(str sql);
	bool (*ExecuteStmt)(sqlite3_stmt* stmt);
	void (*FinalizeStmt)(sqlite3_stmt* stmt);

	bool (*InstallPlugin)(str xpkgPath);
	bool (*UninstallPlugin)(str pluginName);
	bool (*UpgradePlugin)(str pluginName, str newXpkgPath);

	str (*RenderTemplate)(str templatePath, xvalue data);
	str (*RenderString)(str templateString, xvalue data);
} PluginContext;

typedef void (*PluginEventCallback)(str eventName, xvalue eventData);

typedef struct {
	str sEventName;
	xlist lstCallbacks;
} EventListener;

typedef struct {
	str sPluginName;
	str sExportName;
	void* pPtr;
} PluginExport;

bool HttpMethodIs(XS_RequestObject objReq, const char* sMethod);
int HttpGetQueryVar(XS_RequestObject objReq, const char* sName, char* sOut, size_t iOutCap);
bool HttpMultipartNameIs(const HttpMultipartPart* pPart, const char* sName);
bool HttpMultipartNext(XS_RequestObject objReq, size_t* pOffset, HttpMultipartPart* pPart);
int http_reply(XS_ResponseObject objResp, int iCode, str sHead, const void* pBody, size_t iLen);
int HttpReplyFormat(XS_ResponseObject objResp, int iCode, str sHead, str sFormat, ...);

#define SQL_PREPARE_DEFAULT	(SQLITE_PREPARE_PERSISTENT | SQLITE_PREPARE_DONT_LOG)

// 插件初始化函数（启用时调用）
// 命名格式: Plugin_{插件名}_Init
// 例如: void Plugin_member_Init()
// 注意: 使用命名空间宏后，无需手动添加前缀

// 插件卸载函数（禁用时调用）
// 命名格式: Plugin_{插件名}_Unit
// 例如: void Plugin_member_Unit()
// 注意: 使用命名空间宏后，无需手动添加前缀



// ==================== 命名空间宏（V2新增）=============

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
#define SEND_JSON_OK(objResp, msg) \
	ctx->SendJson(objResp, 200, "{\"result\":true,\"message\":\"" msg "\"}", 0)

#define SEND_JSON_ERR(objResp, msg) \
	ctx->SendJson(objResp, 200, "{\"result\":false,\"message\":\"" msg "\"}", 0)

#define CHECK_METHOD_GET(objResp, hm) \
	if ( !HttpMethodIs(objReq, "GET") ) { \
		SEND_JSON_ERR(objResp, "Method not allowed"); \
		return; \
	}

#define CHECK_METHOD_POST(objResp, hm) \
	if ( !HttpMethodIs(objReq, "POST") ) { \
		SEND_JSON_ERR(objResp, "Method not allowed"); \
		return; \
	}

#define RENDER_SEND_TEMPLATE(objResp, templatePath, data) \
	do { \
		str _html = ctx->RenderTemplate(templatePath, data); \
		if ( _html ) { \
			ctx->SendHtml(objResp, 200, _html); \
			ctx->Free(_html); \
		} else { \
			ctx->SendHtml(objResp, 500, "<h1>Template render failed</h1>"); \
		} \
	} while (0)

#define LOAD_SEND_PAGE(objResp, pagePath) \
	ctx->LoadPage(objResp, 200, HTTP_CT_HTML, pagePath)

#define LOAD_SEND_PAGE_CODE(objResp, code, pagePath) \
	ctx->LoadPage(objResp, code, HTTP_CT_HTML, pagePath)

#endif