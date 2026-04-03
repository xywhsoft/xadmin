// template runtime env
xvalue tblENV = NULL;

// shared include table (kept for compatibility)
xdict G_Template = NULL;

#define XADMIN_TEMPLATE_BRACKET "{{}}"
XTE_ParseOptions G_TemplateParseOptions = { XADMIN_TEMPLATE_BRACKET, 0 };



xvalue TemplateProc_Project_MakeXID(xvalue varENV, xvalue varParam)
{
	(void)varENV;
	(void)varParam;

	return xvoCreateText(xrtMakeXIDS(), 0, TRUE);
}



char* MakePageWithTemplate(char* sTemplate, xvalue tblData, size_t* pRetSize)
{
	xtetemplate hTemplate;
	XTE_Error tError = { 0 };
	str sFilePath;
	str sText;
	char* sPage;
	size_t iSize = 0;

	if ( (sTemplate == NULL) || (sTemplate[0] == '\0') ) {
		return xrtCopyStr("<!DOCTYPE html><html><body><p>template name required</p></body></html>", 0);
	}

	sFilePath = xrtPathJoin(2, TemplatePath, sTemplate);
	sText = xrtFileReadAll(sFilePath, XRT_CP_BINARY, NULL);
	xrtFree(sFilePath);
	if ( sText == NULL ) {
		return xrtFormat("<!DOCTYPE html><html><body><p>template not found: %s</p></body></html>", sTemplate);
	}

	iSize = strlen(sText);
	hTemplate = xteParseEx(NULL, sText, iSize, &G_TemplateParseOptions, &tError);
	xrtFree(sText);
	if ( hTemplate == NULL ) {
		return xrtFormat("<!DOCTYPE html><html><body><p>template parse failed: %s (%s at %u:%u)</p></body></html>", sTemplate, tError.sDesc ? tError.sDesc : "unknown error", tError.iLine, tError.iColumn);
	}

	sPage = xteMake(hTemplate, tblData, tblENV, G_Template, pRetSize);
	xteDestroyTemplate(hTemplate);
	if ( sPage == NULL ) {
		return xrtFormat("<!DOCTYPE html><html><body><p>template render failed: %s</p></body></html>", sTemplate);
	}

	return sPage;
}



void Template_Init()
{
	printf("        Template_Init \n");

	G_Template = xrtDictCreate(sizeof(ptr), XRT_OBJMODE_SHARED);
	xrtOwnerActivateShared(&G_Template->Owner);
	xrtOwnerActivateShared(&G_Template->AVLT.Owner);
	tblENV = xvoCreateTable();
	xvoTableSetFunc(tblENV, "MakeXID", 7, TemplateProc_Project_MakeXID);
	xrtOwnerActivateShared(&tblENV->vTable->AVLT.Owner);
	xrtOwnerActivateShared(&tblENV->vTable->Owner);
	xvoSetShared_Inline(tblENV);
}



void Template_Unit()
{
	printf("        Template_Unit \n");

	if ( G_Template ) {
		xrtDictDestroy(G_Template);
		G_Template = NULL;
	}

	if ( tblENV ) {
		xvoUnref(tblENV);
		tblENV = NULL;
	}
}
