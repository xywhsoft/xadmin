/* 保留 v1 模板文件字节与 {{ ... }} 语法，使用新版原生模板引擎。
 * 按首次使用编译并缓存；请求锁同时保护缓存读写，卸载时统一释放。
 */
static xroot G_TemplateRoot;
static xmap* G_Templates;
static xtemplateregistry* G_TemplateRegistry; /* 扩展注册表（{{#form}} 等），Form 模块装配 */
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
		config.Registry = G_TemplateRegistry;
		compiled = xrtTemplateCompileConfig((xstrview){(const char*)source, length}, &config);
		xrtFree(source);
		if (!compiled) { printf("[template][error] %s\n", name); return NULL; }
		if (!xrtMapSetPtr(G_Templates, key, compiled)) { xrtTemplateRelease(compiled); return NULL; }
	}
	{
		/* H1：动态 {{$path}} 输出统一 HTML 转义（& < > " '）。转义只作用于
		 * 输出节点——模板原文与 {{#form}} 等扩展（自带转义）不受影响。 */
		xtemplaterenderconfig renderConfig;
		xstrbuf output;
		str result;
		xrtTemplateRenderHtmlConfigInit(&renderConfig);
		renderConfig.Root = data;
		renderConfig.Current = data;
		xrtStrBufInit(&output);
		if (!xrtTemplateRenderTo(compiled, &renderConfig, &output)) {
			xrtStrBufFree(&output);
			return NULL;
		}
		if (size) *size = output.Size;
		result = xrtStrBufTake(&output);
		xrtStrBufFree(&output);
		return result;
	}
}
static void Template_ReleaseCache(xmap* cache)
{
	xmapiter it = {0}; void* slot; xbytesview key;
	if (cache && xrtMapIterBegin(cache, &it)) {
		while ((slot = xrtMapIterNext(&it, &key)) != NULL) xrtTemplateRelease(*(xtemplate**)slot);
		xrtMapIterEnd(&it);
	}
	xrtMapDestroy(cache);
}
static void Template_Unit(void)
{
	Template_ReleaseCache(G_Templates); G_Templates = NULL;
	if (G_TemplateRoot) xrtRootClose(G_TemplateRoot);
	G_TemplateRoot = NULL;
}

/* v1 Template_RebuildCache 语义：全量重扫模板目录并预编译进新缓存。
 * 全部失败时保留旧缓存；调用方已持有请求锁，被替换的旧缓存无并发读者。
 * 模板目录与 ServiceInit 的 G_TemplateRoot 同源（AppPath/template）。 */
typedef struct TemplateRebuildContext {
	xmap* cache;
	uint32 loaded;
	uint32 failed;
	size_t root_len;
} TemplateRebuildContext;
static char* Template_RelativeKey(const char* path, size_t root_len)
{
	char* key; size_t i;
	if (!path || strlen(path) <= root_len + 1) return NULL;
	key = xrtStrDup(path + root_len + 1);
	if (!key) return NULL;
	for (i = 0; key[i]; i++) if (key[i] == '\\') key[i] = '/';
	return key;
}
static int Template_RebuildFileProc(const char* path, size_t size, bool dir, void* param)
{
	TemplateRebuildContext* ctx = (TemplateRebuildContext*)param;
	char* key; bytes source; size_t length = 0; xtemplate* compiled; xtemplateconfig config;
	(void)size;
	if (dir || !ctx || !ctx->cache) return 0;
	key = Template_RelativeKey(path, ctx->root_len);
	if (!key) return 0;
	source = RootFileReadAll(G_TemplateRoot, key, &length);
	if (!source) { ctx->failed++; xrtFree(key); return 0; }
	xrtTemplateConfigInit(&config);
	config.Open = XRT_STR_LITERAL("{{"); config.Close = XRT_STR_LITERAL("}}");
	config.Registry = G_TemplateRegistry;
	compiled = xrtTemplateCompileConfig((xstrview){(const char*)source, length}, &config);
	xrtFree(source);
	if (!compiled) {
		printf("[template][error] %s\n", key);
		ctx->failed++; xrtFree(key); return 0;
	}
	if (!xrtMapSetPtr(ctx->cache, (xbytesview){(cbytes)key, strlen(key)}, compiled)) {
		xrtTemplateRelease(compiled); ctx->failed++; xrtFree(key); return 0;
	}
	ctx->loaded++;
	xrtFree(key);
	return 0;
}
static bool Template_RebuildCache(uint32* loaded, uint32* failed)
{
	char* dir; TemplateRebuildContext ctx;
	if (loaded) *loaded = 0;
	if (failed) *failed = 0;
	if (!G_TemplateRoot) return false;
	ctx.cache = xrtMapCreate(sizeof(xtemplate*));
	ctx.loaded = ctx.failed = 0;
	dir = xrtPathJoin(AppPath, "template");
	if (!ctx.cache || !dir) {
		Template_ReleaseCache(ctx.cache); xrtFree(dir);
		return false;
	}
	ctx.root_len = strlen(dir);
	DirScan(dir, true, Template_RebuildFileProc, &ctx);
	xrtFree(dir);
	if (ctx.loaded == 0 && ctx.failed > 0) {
		/* 与 v1 相同：一个都没编译成功视为配置性故障，不动旧缓存。 */
		Template_ReleaseCache(ctx.cache);
		*failed = ctx.failed;
		return false;
	}
	Template_ReleaseCache(G_Templates);
	G_Templates = ctx.cache;
	*loaded = ctx.loaded;
	*failed = ctx.failed;
	return true;
}
