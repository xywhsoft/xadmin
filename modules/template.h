/* 保留 v1 模板文件字节与 {{ ... }} 语法，使用新版原生模板引擎。
 * 按首次使用编译并缓存；请求锁同时保护缓存读写，卸载时统一释放。
 */
static xroot G_TemplateRoot;
static xmap* G_Templates;
static char* MakePageWithTemplate(const char* name, xvalue* data, size_t* size)
{
	xbytesview key = {(cbytes)name, strlen(name)};
	xtemplate* compiled = xrtMapGetPtr(G_Templates, key);
	if (size) *size = 0;
	if (!compiled) {
		size_t length = 0; bytes source = RootFileReadAll(G_TemplateRoot, name, &length);
		xtemplateconfig config;
		if (!source) return NULL;
		xrtTemplateConfigInit(&config);
		config.Open = XRT_STR_LITERAL("{{"); config.Close = XRT_STR_LITERAL("}}");
		compiled = xrtTemplateCompileConfig((xstrview){(const char*)source, length}, &config);
		xrtFree(source);
		if (!compiled) { printf("[template][error] %s\n", name); return NULL; }
		if (!xrtMapSetPtr(G_Templates, key, compiled)) { xrtTemplateRelease(compiled); return NULL; }
	}
	return xrtTemplateRender(compiled, data, size);
}
static void Template_Unit(void)
{
	xmapiter it = {0}; void* slot; xbytesview key;
	if (G_Templates && xrtMapIterBegin(G_Templates, &it)) {
		while ((slot = xrtMapIterNext(&it, &key)) != NULL) xrtTemplateRelease(*(xtemplate**)slot);
		xrtMapIterEnd(&it);
	}
	xrtMapDestroy(G_Templates); G_Templates = NULL;
	if (G_TemplateRoot) xrtRootClose(G_TemplateRoot);
	G_TemplateRoot = NULL;
}
