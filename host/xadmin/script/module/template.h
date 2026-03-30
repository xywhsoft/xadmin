
// template runtime env
xvalue tblENV = NULL;

// shared include table (kept for compatibility)
xdict G_Template = NULL;



xvalue TemplateProc_Project_MakeXID(xvalue varENV, xvalue varParam)
{
	(void)varENV;
	(void)varParam;

	return xvoCreateText(xrtMakeXIDS(), 0, TRUE);
}



char* MakePageWithTemplate(char* sTemplate, xvalue tblData, size_t* pRetSize)
{
	XTE_LiteObject objTemplate;
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
	objTemplate = xteParse(sText, iSize, NULL);
	xrtFree(sText);
	if ( (objTemplate == NULL) || (!objTemplate->Success) ) {
		if ( objTemplate ) {
			xteParseFree(objTemplate);
		}
		return xrtFormat("<!DOCTYPE html><html><body><p>template parse failed: %s</p></body></html>", sTemplate);
	}

	sPage = xteMake(objTemplate, tblData, tblENV, G_Template, pRetSize);
	xteParseFree(objTemplate);
	if ( sPage == NULL ) {
		return xrtFormat("<!DOCTYPE html><html><body><p>template render failed: %s</p></body></html>", sTemplate);
	}

	return sPage;
}



void Template_Init()
{
	printf("        Template_Init \n");

	G_Template = xrtDictCreate(sizeof(ptr), 0);
	tblENV = xvoCreateTable();
	xvoTableSetFunc(tblENV, "MakeXID", 7, TemplateProc_Project_MakeXID);
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