#include "plugin.h"

PluginContext* ctx;
xvalue g_tblSettings = NULL;
xvalue g_tblTemplates = NULL;
int g_iMenuId = 0;

void Plugin_SetGlobalData(int idx, void* ptr)
{
	if ( idx == 1 ) {
		ctx = (PluginContext*)ptr;
	} else if ( idx == 2 ) {
		g_tblSettings = (xvalue)ptr;
	}
}

void Plugin_model_generator_View(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession);

xvalue ModelGen_LoadTemplates()
{
	str sTemplateDir = xrtPathJoin(3, ctx->GetPluginPath(), "templates", "");
	xvalue objArrTemplates = xvoCreateArray();

	typedef struct {
		str sFile;
		str sId;
	} TemplateFile;

	TemplateFile arrFiles[] = {
		{"article.json", "article"},
		{"news.json", "news"},
		{"download.json", "download"},
		{"product.json", "product"}
	};

	for ( int i = 0; i < 4; i++ ) {
		str sFilePath = xrtPathJoin(2, sTemplateDir, arrFiles[i].sFile);
		size_t iLen = 0;
		str sContent = NULL;

		if ( ctx->ReadFile(sFilePath, &sContent, &iLen) ) {
			xvalue objTblTemplate = ctx->JsonParse(sContent, iLen);
			xrtFree(sContent);
			if ( objTblTemplate ) {
				xvoArrayAppendValue(objArrTemplates, objTblTemplate, TRUE);
			}
		}

		xrtFree(sFilePath);
	}

	xrtFree(sTemplateDir);
	return objArrTemplates;
}

void API_ModelGen_Templates(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue objRet = xvoCreateTable();
	xvoTableSetBool(objRet, "result", 6, TRUE);
	xvoTableSetValue(objRet, "data", 4, g_tblTemplates, FALSE);

	size_t iLen = 0;
	str sJson = ctx->JsonStringify(objRet, &iLen);
	ctx->SendJson(objResp, 200, sJson, iLen);
	ctx->Free(sJson);
	xvoUnref(objRet);
}

void API_ModelGen_TemplateDetail(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue objForm = ctx->JsonParse((const char*)xsReqBody(objReq), xsReqBodyLen(objReq));
	if ( !objForm ) {
		ctx->SendJson(objResp, 200, "{\"result\":false,\"message\":\"invalid request\"}", 0);
		return;
	}

	str sTemplateId = xvoTableGetText(objForm, "templateId", 11);
	if ( !sTemplateId ) {
		xvoUnref(objForm);
		ctx->SendJson(objResp, 200, "{\"result\":false,\"message\":\"missing templateId\"}", 0);
		return;
	}

	int iCount = xvoArrayItemCount(g_tblTemplates);
	xvalue objTblTemplate = NULL;
	for ( int i = 0; i < iCount; i++ ) {
		xvalue objTbl = xvoArrayGetValue(g_tblTemplates, i);
		str sId = xvoTableGetText(objTbl, "templateId", 11);
		if ( sId && strcmp(sId, sTemplateId) == 0 ) {
			objTblTemplate = objTbl;
			break;
		}
	}

	if ( !objTblTemplate ) {
		xvoUnref(objForm);
		ctx->SendJson(objResp, 200, "{\"result\":false,\"message\":\"template not found\"}", 0);
		return;
	}

	xvalue objRet = xvoCreateTable();
	xvoTableSetBool(objRet, "result", 6, TRUE);
	xvoTableSetValue(objRet, "data", 4, objTblTemplate, FALSE);

	size_t iLen = 0;
	str sJson = ctx->JsonStringify(objRet, &iLen);
	ctx->SendJson(objResp, 200, sJson, iLen);
	ctx->Free(sJson);
	xvoUnref(objRet);
	xvoUnref(objForm);
}

void API_ModelGen_Create(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue objForm = ctx->JsonParse((const char*)xsReqBody(objReq), xsReqBodyLen(objReq));
	if ( !objForm ) {
		ctx->SendJson(objResp, 200, "{\"result\":false,\"message\":\"invalid request\"}", 0);
		return;
	}

	str sTemplateId = xvoTableGetText(objForm, "templateId", 11);
	str sModelName = xvoTableGetText(objForm, "modelName", 10);
	str sModelTitle = xvoTableGetText(objForm, "modelTitle", 11);
	if ( !sTemplateId || !sModelName ) {
		xvoUnref(objForm);
		ctx->SendJson(objResp, 200, "{\"result\":false,\"message\":\"missing required fields\"}", 0);
		return;
	}

	xvalue objTblTemplate = NULL;
	int iCount = xvoArrayItemCount(g_tblTemplates);
	for ( int i = 0; i < iCount; i++ ) {
		xvalue objTbl = xvoArrayGetValue(g_tblTemplates, i);
		str sId = xvoTableGetText(objTbl, "templateId", 11);
		if ( sId && strcmp(sId, sTemplateId) == 0 ) {
			objTblTemplate = objTbl;
			break;
		}
	}

	if ( !objTblTemplate ) {
		xvoUnref(objForm);
		ctx->SendJson(objResp, 200, "{\"result\":false,\"message\":\"template not found\"}", 0);
		return;
	}

	xvalue objTblConfig = xvoTableGetValue(objTblTemplate, "config", 6);
	if ( sModelName ) {
		xvoTableSetText(objTblConfig, "modelName", 10, sModelName, 0, FALSE);
	}
	if ( sModelTitle ) {
		xvoTableSetText(objTblConfig, "modelTitle", 11, sModelTitle, 0, FALSE);
	}

	bool bResult = ctx->GenerateModel(sModelName, objTblConfig);
	xvoUnref(objForm);

	xvalue objRet = xvoCreateTable();
	xvoTableSetBool(objRet, "result", 6, bResult);
	xvoTableSetText(objRet, "message", 7, bResult ? "created" : "create failed", 0, FALSE);

	size_t iLen = 0;
	str sJson = ctx->JsonStringify(objRet, &iLen);
	ctx->SendJson(objResp, 200, sJson, iLen);
	ctx->Free(sJson);
	xvoUnref(objRet);
}

void Plugin_model_generator_Init()
{
	ctx->Log(LOG_INFO, "[ModelGenerator] Initializing...");

	g_tblTemplates = ModelGen_LoadTemplates();

	ctx->AddRoute("/admin/api/model_generator/templates", API_ModelGen_Templates, TRUE, TRUE, 0, 0);
	ctx->AddRoute("/admin/api/model_generator/template_detail", API_ModelGen_TemplateDetail, TRUE, TRUE, 0, 0);
	ctx->AddRoute("/admin/api/model_generator/create", API_ModelGen_Create, TRUE, TRUE, 0, 0);
	ctx->AddRoute("/admin/view/model_generator", Plugin_model_generator_View, TRUE, TRUE, 0, 0);

	g_iMenuId = ctx->AddMenu(0, "Model Generator", "layui-icon layui-icon-template-1", 1, "_component", "/admin/view/model_generator", 100, TRUE);

	ctx->Log(LOG_INFO, "[ModelGenerator] Initialized!");
}

void Plugin_model_generator_View(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	LOAD_SEND_PAGE(objResp, "index.html");
}

void Plugin_model_generator_Unit()
{
	ctx->RemoveRoute("/admin/api/model_generator/templates");
	ctx->RemoveRoute("/admin/api/model_generator/template_detail");
	ctx->RemoveRoute("/admin/api/model_generator/create");
	ctx->RemoveRoute("/admin/view/model_generator");

	if ( g_tblTemplates ) {
		xvoUnref(g_tblTemplates);
		g_tblTemplates = NULL;
	}

	ctx->Log(LOG_INFO, "[ModelGenerator] Unloaded!");
}
