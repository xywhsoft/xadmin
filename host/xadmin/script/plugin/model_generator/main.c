#include "plugin.h"

PluginContext* ctx;
xvalue g_tblSettings = NULL;
xvalue g_tblTemplates = NULL;
int g_iMenuId = 0;

void Plugin_SetGlobalData(int idx, void* ptr)
{
	if ( idx == 1 ) ctx = (PluginContext*)ptr;
	else if ( idx == 2 ) g_tblSettings = (xvalue)ptr;
}

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

	int iCount = xvoArrayItemCount(g_tblTemplates);
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

	xvalue tblTemplate = NULL;

	int iCount = xvoArrayItemCount(g_tblTemplates);

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

	xvalue tblConfig = xvoTableGetValue(tblTemplate, "config", 6);

	if ( sModelName ) {
		xvoTableSetText(tblConfig, "modelName", 10, sModelName, 0, FALSE);
	}
	if ( sModelTitle ) {
		xvoTableSetText(tblConfig, "modelTitle", 11, sModelTitle, 0, FALSE);
	}

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
