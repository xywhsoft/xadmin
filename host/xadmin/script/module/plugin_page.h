// plugin page and template runtime

extern str PluginPath;
extern xvalue tblENV;
extern xdict G_Template;

static xdict G_PluginTemplateCache = NULL;



void Plugin_LoadPage(XS_ResponseObject objResp, int iCode, str sHead, str sPluginId, str sPage)
{
	str sPluginDir = xrtPathJoin(3, PluginPath, sPluginId, "page");
	str sFile = xrtPathJoin(2, sPluginDir, sPage);
	size_t iRetSize = 0;
	str sHTML = NULL;

	if ( !xrtFileExists(sFile) ) {
		xrtFree(sFile);
		xrtFree(sPluginDir);
		http_reply(objResp, 404, HTTP_CT_HTML, "<!DOCTYPE html><html><head><meta charset='utf-8'><title>404</title></head><body><h1>page not found</h1></body></html>", 0);
		return;
	}

	sHTML = xrtFileGetAll(sFile, &iRetSize);
	http_reply(objResp, iCode, sHead, sHTML, iRetSize);
	xrtFree(sHTML);
	xrtFree(sFile);
	xrtFree(sPluginDir);
}



void Plugin_LoadTemplates(str sPluginId)
{
	printf("[Plugin] Templates ready for: %s\n", sPluginId);
}



str Plugin_MakePageWithTemplate(str sPluginId, str sTemplate, xvalue tblData, size_t* pRetSize)
{
	xtetemplate hTemplate;
	XTE_Error tError = { 0 };
	str sTemplateDir;
	str sFilePath;
	str sText;
	str sPage;
	size_t iSize = 0;

	if ( (sPluginId == NULL) || (sTemplate == NULL) ) {
		return xrtCopyStr("<!DOCTYPE html><html><body><p>plugin template required</p></body></html>", 0);
	}

	sTemplateDir = xrtPathJoin(3, PluginPath, sPluginId, "template");
	sFilePath = xrtPathJoin(2, sTemplateDir, sTemplate);
	xrtFree(sTemplateDir);

	sText = xrtFileReadAll(sFilePath, XRT_CP_BINARY, NULL);
	xrtFree(sFilePath);
	if ( sText == NULL ) {
		return xrtFormat("<!DOCTYPE html><html><body><p>plugin template not found: %s/%s</p></body></html>", sPluginId, sTemplate);
	}

	iSize = strlen(sText);
	hTemplate = xteParseEx(NULL, sText, iSize, NULL, &tError);
	xrtFree(sText);
	if ( hTemplate == NULL ) {
		return xrtFormat("<!DOCTYPE html><html><body><p>plugin template parse failed: %s/%s</p></body></html>", sPluginId, sTemplate);
	}

	sPage = xteMake(hTemplate, tblData, tblENV, G_Template, pRetSize);
	xteDestroyTemplate(hTemplate);
	if ( sPage == NULL ) {
		return xrtFormat("<!DOCTYPE html><html><body><p>plugin template render failed: %s/%s</p></body></html>", sPluginId, sTemplate);
	}

	return sPage;
}



void Plugin_UnloadTemplates(str sPluginId)
{
	printf("[Plugin] Templates released for: %s\n", sPluginId);
}



void PluginTemplate_Init()
{
	G_PluginTemplateCache = xrtDictCreate(sizeof(ptr), XRT_OBJMODE_SHARED);
	xrtOwnerActivateShared(&G_PluginTemplateCache->Owner);
	xrtOwnerActivateShared(&G_PluginTemplateCache->AVLT.Owner);
}



void PluginTemplate_Unit()
{
	if ( G_PluginTemplateCache ) {
		xrtDictDestroy(G_PluginTemplateCache);
		G_PluginTemplateCache = NULL;
	}
}
