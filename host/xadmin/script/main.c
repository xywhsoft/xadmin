


// XS 鍩虹鏈嶅姟搴?
#include <xs_vnext_full.h>
#if defined(_WIN32) || defined(_WIN64)
	// windows 鏂规
#else
	// 鍏朵粬骞冲彴鏂规
	#include <pthread.h>
	#include <sys/utsname.h>
#endif



// 鍏ㄥ眬瀹氫箟
#include "module/define.h"

// 瀹夊叏闃叉姢妯″潡
#include "module/guard.h"

// 鐙珛椤甸潰 API
#include "module/page.h"

// 妯℃澘鐩稿叧鍔熻兘
#include "module/template.h"

// 瀹夎鐩稿叧鍔熻兘
#include "module/install.h"

// Session 鐩稿叧鍔熻兘
#include "module/session.h"

// 鏁版嵁搴撶浉鍏冲姛鑳?
#include "module/db.h"

// 鏉冮檺绠＄悊妯″潡
#include "module/auth.h"

// 鏃ュ織璁板綍妯″潡
#include "module/logs.h"

// 鍚庡彴鍔熻兘妯″潡
#include "module/admin.h"

// 閰嶇疆绠＄悊妯″潡
#include "module/option.h"

// 鑿滃崟绠＄悊妯″潡
#include "module/menu.h"

// 鍓嶅彴鐢ㄦ埛妯″潡
#include "module/member.h"

// 鍓嶅彴鏉冮檺缂撳瓨妯″潡
#include "module/member_auth.h"

// 妯″瀷瀛楁绫诲瀷瀹氫箟
#include "module/model_field.h"

// 妯″瀷绠＄悊鍣?
#include "module/model_mgr.h"

// 闄勪欢绠＄悊妯″潡
#include "module/attachment.h"

// 鎻掍欢涓婁笅鏂囧畾涔?
#include "module/plugin_ctx.h"

// 鎻掍欢椤甸潰鍜屾ā鏉垮姞杞?
#include "module/plugin_page.h"

// 鎻掍欢绠＄悊鍣?
#include "module/plugin_mgr.h"



// 璺敱璋冪敤 - HTTP
#include "route_http/index.h"
#include "route_http/login.h"
#include "route_http/logs.h"
#include "route_http/auth.h"
#include "route_http/option.h"
#include "route_http/menu.h"
#include "route_http/trace.h"
#include "route_http/api.h"
#include "route_http/member.h"
#include "route_http/model.h"
#include "route_http/attachment.h"
#include "route_http/attachment_api.h"
#include "route_http/plugin.h"



// 鍏ㄥ眬闈欐€佽矾鐢辫〃
#include "route.h"

// HTTP 鍗忚澶勭悊
#include "module/http.h"





// 鏈嶅姟鍒濆鍖?
void ServiceInit(XS_ServerObject objServer, XS_HostObject objHost)
{
	printf("[xadmin:init] Define_Init begin\n");
	fflush(stdout);
	Define_Init(objServer, objHost);
	printf("[xadmin:init] Define_Init done\n");
	fflush(stdout);

	printf("[xadmin:init] Guard_Init begin\n");
	fflush(stdout);
	Guard_Init();
	printf("[xadmin:init] Guard_Init done\n");
	fflush(stdout);

	printf("[xadmin:init] Template_Init begin\n");
	fflush(stdout);
	Template_Init();
	printf("[xadmin:init] Template_Init done\n");
	fflush(stdout);

	printf("[xadmin:init] PluginTemplate_Init begin\n");
	fflush(stdout);
	PluginTemplate_Init();
	printf("[xadmin:init] PluginTemplate_Init done\n");
	fflush(stdout);

	printf("[xadmin:init] Install_Init begin\n");
	fflush(stdout);
	Install_Init();
	printf("[xadmin:init] Install_Init done\n");
	fflush(stdout);

	printf("[xadmin:init] Session_Init begin\n");
	fflush(stdout);
	Session_Init();
	printf("[xadmin:init] Session_Init done\n");
	fflush(stdout);

	printf("[xadmin:init] DB_Init begin\n");
	fflush(stdout);
	DB_Init();
	printf("[xadmin:init] DB_Init done\n");
	fflush(stdout);

	printf("[xadmin:init] RouteHTTP_Init begin\n");
	fflush(stdout);
	RouteHTTP_Init();
	printf("[xadmin:init] RouteHTTP_Init done\n");
	fflush(stdout);

	printf("[xadmin:init] Admin_Init begin\n");
	fflush(stdout);
	Admin_Init();
	printf("[xadmin:init] Admin_Init done\n");
	fflush(stdout);

	printf("[xadmin:init] Auth_Init begin\n");
	fflush(stdout);
	Auth_Init();
	printf("[xadmin:init] Auth_Init done\n");
	fflush(stdout);

	printf("[xadmin:init] Logs_Init begin\n");
	fflush(stdout);
	Logs_Init();
	printf("[xadmin:init] Logs_Init done\n");
	fflush(stdout);

	printf("[xadmin:init] Option_Init begin\n");
	fflush(stdout);
	Option_Init();
	printf("[xadmin:init] Option_Init done\n");
	fflush(stdout);

	printf("[xadmin:init] Menu_Init begin\n");
	fflush(stdout);
	Menu_Init();
	printf("[xadmin:init] Menu_Init done\n");
	fflush(stdout);

	printf("[xadmin:init] Member_Init begin\n");
	fflush(stdout);
	Member_Init();
	printf("[xadmin:init] Member_Init done\n");
	fflush(stdout);

	printf("[xadmin:init] MemberAuth_Init begin\n");
	fflush(stdout);
	MemberAuth_Init();
	printf("[xadmin:init] MemberAuth_Init done\n");
	fflush(stdout);

	printf("[xadmin:init] ModelMgr_Init begin\n");
	fflush(stdout);
	ModelMgr_Init();
	printf("[xadmin:init] ModelMgr_Init done\n");
	fflush(stdout);

	printf("[xadmin:init] Attachment_Init begin\n");
	fflush(stdout);
	Attachment_Init();
	printf("[xadmin:init] Attachment_Init done\n");
	fflush(stdout);

	printf("[xadmin:init] Auth_SyncURIS begin\n");
	fflush(stdout);
	Auth_SyncURIS();
	printf("[xadmin:init] Auth_SyncURIS done\n");
	fflush(stdout);

	printf("[xadmin:init] PluginMgr_Init begin\n");
	fflush(stdout);
	PluginMgr_Init();
	printf("[xadmin:init] PluginMgr_Init done\n");
	fflush(stdout);
}



// 鏈嶅姟鍗歌浇
void ServiceUnit(XS_ServerObject objServer, XS_HostObject objHost)
{

	// 鍗歌浇鎻掍欢妯℃澘绯荤粺
	PluginTemplate_Unit();

	// 鍗歌浇鎻掍欢绠＄悊鍣紙鏈€鍏堝嵏杞斤級
	PluginMgr_Unit();
	
	// 鍗歌浇闄勪欢绠＄悊妯″潡
	Attachment_Unit();
	
	// 鍗歌浇妯″瀷绠＄悊鍣?
	ModelMgr_Unit();
	
	// 鍗歌浇鍓嶅彴鏉冮檺缂撳瓨妯″潡
	MemberAuth_Unit();
	
	// 鍗歌浇鍓嶅彴鐢ㄦ埛妯″潡
	Member_Unit();
	
	// 鍗歌浇鑿滃崟绠＄悊妯″潡
	Menu_Unit();
	
	// 鍗歌浇閰嶇疆绠＄悊妯″潡
	Option_Unit();
	
	// 鍗歌浇鍚庡彴鍔熻兘妯″潡
	Admin_Unit();
	
	// 鍗歌浇鏃ュ織璁板綍妯″潡
	Logs_Unit();
	
	// 鍗歌浇鏉冮檺绠＄悊妯″潡
	Auth_Unit();
	
	// 鍗歌浇 HTTP 璺敱琛?
	RouteHTTP_Unit();
	
	// 閲婃斁鏁版嵁搴?
	DB_Unit();
	
	// 鍗歌浇 Session 妯″潡
	Session_Unit();
	
	// 鍗歌浇瀹夎妯″潡
	Install_Unit();
	
	// 鍗歌浇妯℃澘妯″潡
	Template_Unit();
	
	// 鍗歌浇闃叉姢妯″潡
	Guard_Unit();
	
	// 鍗歌浇鍏ㄥ眬鏁版嵁
	Define_Unit();
	
}