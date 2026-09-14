#include <xs_plugin.h>
#include "model.h"
#include "db.h"

static XAdminPluginHandle G_PluginHandle = NULL;
static const char* G_PrivateDbPath = NULL;

XADMIN_EXPORT void XAdmin_PluginSetGlobalData(int idx, void* ptr)
{
	if ( idx == XADMIN_GLOBAL_PLUGIN_PRIVATE_DB_PATH ) {
		G_PrivateDbPath = (const char*)ptr;
	}
}

void ContentPlugin_RequestAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer;
	(void)objHost;
	(void)objReq;
	(void)objSession;
	XAdmin_LoadPluginPage(G_PluginHandle, objResp, 200, "Content-Type: text/html; charset=utf-8\r\n", "admin.html");
}

void ContentPlugin_RequestPublic(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer;
	(void)objHost;
	(void)objReq;
	(void)objSession;
	XAdmin_LoadPluginPage(G_PluginHandle, objResp, 200, "Content-Type: text/html; charset=utf-8\r\n", "public.html");
}

int ContentPlugin_OnLoad(XAdminPluginHandle* out_handle)
{
	if ( out_handle ) {
		G_PluginHandle = *out_handle;
	}
	return 0;
}

int ContentPlugin_OnStart(XAdminPluginHandle handle)
{
	XAdminRouteDecl route;
	XAdminMenuDecl menu;
	int iRet;
	G_PluginHandle = handle;
	if ( !ContentPlugin_EnsureSchema(G_PrivateDbPath) ) {
		return -1;
	}
	memset(&route, 0, sizeof(route));
	route.path = "@@ADMIN_ROUTE@@";
	route.proc = ContentPlugin_RequestAdmin;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	iRet = XAdmin_RegisterRoute(handle, &route, NULL);
	if ( iRet != 0 ) {
		return iRet;
	}
	memset(&route, 0, sizeof(route));
	route.path = "@@PUBLIC_ROUTE@@";
	route.proc = ContentPlugin_RequestPublic;
	route.need_auth = FALSE;
	route.admin_only = FALSE;
	iRet = XAdmin_RegisterRoute(handle, &route, NULL);
	if ( iRet != 0 ) {
		return iRet;
	}
	memset(&menu, 0, sizeof(menu));
	menu.key = "@@PLUGIN_XID@@.settings";
	menu.title = CONTENT_MENU_TITLE;
	menu.icon = "layui-icon layui-icon-template-1";
	menu.type = 1;
	menu.open_type = "_component";
	menu.href = "@@OPTION_ROUTE@@";
	menu.sort = 990000;
	menu.visible = TRUE;
	menu.remark = "Generated content plugin settings";
	XAdmin_RegisterMenu(handle, &menu, NULL, NULL);
	return 0;
}

void ContentPlugin_OnStop(XAdminPluginHandle handle)
{
	(void)handle;
}

void ContentPlugin_OnUnload(XAdminPluginHandle handle)
{
	(void)handle;
	G_PluginHandle = NULL;
	G_PrivateDbPath = NULL;
}

static XAdminPluginDescriptor G_Plugin = {
	XADMIN_ABI_VERSION,
	sizeof(XAdminPluginDescriptor),
	"@@PLUGIN_XID@@",
	"1.0.0",
	CONTENT_PLUGIN_TITLE,
	ContentPlugin_OnLoad,
	NULL,
	ContentPlugin_OnStart,
	NULL,
	NULL,
	ContentPlugin_OnStop,
	ContentPlugin_OnUnload
};

XADMIN_DECLARE_PLUGIN(G_Plugin)

