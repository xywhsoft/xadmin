/* 内容生成器（v1 script/content/content_generator.h + content_generation.h 对齐，方案 A 原生方言）。
 * 阶段 2：模板装载/占位符替换、managed 产物构建（main.c/plugin.json/spec.json/
 * runtime 三件套/页面 HTML）、能力包声明文件拷贝与生成编排。
 * 由 main.c 在 modules/content.h 之后 include（依赖 Content_* / ContentPack_* /
 * Plugin_BindText / XAdmin_GeneratePlugin / plugin_host.h 静态装配函数，同 TU）。
 * 时间列 Unix 微秒（xrtNow），与 v1 秒不互换——内容表为 v3 新表。 */

/* ==================== 文本与模板基础 ==================== */

static const char* Content_TextOr(const char* text, const char* fallback)
{
	if (text && text[0]) return text;
	return fallback ? fallback : "";
}

/* C 字符串字面量转义（\ " 换行 回车；v1 Content_EscapeCString 同集） */
static char* Content_EscapeCString(const char* text)
{
	size_t len; size_t extra = 0; const char* p; char* out; char* w;
	if (!text) return xrtStrDup("");
	len = strlen(text);
	for (p = text; *p; p++)
		if (*p == '\\' || *p == '"' || *p == '\n' || *p == '\r') extra++;
	out = (char*)xrtMalloc(len + extra + 1);
	if (!out) return NULL;
	w = out;
	for (p = text; *p; p++) {
		char c = *p;
		if (c == '\\') { *w++ = '\\'; *w++ = '\\'; }
		else if (c == '"') { *w++ = '\\'; *w++ = '"'; }
		else if (c == '\n') { *w++ = '\\'; *w++ = 'n'; }
		else if (c == '\r') { *w++ = '\\'; *w++ = 'r'; }
		else *w++ = c;
	}
	*w = '\0';
	return out;
}

/* 标识符清洗：字母数字下划线保留，. - 换 _，空结果回退 fallback（v1 同） */
static char* Content_SanitizeSqlIdent(const char* text, const char* fallback)
{
	const char* fb = Content_TextOr(fallback, "content_record");
	const char* p; char* out; size_t w = 0; bool hasChar = false;
	if (!text) text = "";
	out = (char*)xrtMalloc(strlen(text) + strlen(fb) + 2);
	if (!out) return NULL;
	for (p = text; *p; p++) {
		char c = *p;
		if (((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')) || ((c >= '0') && (c <= '9')) || (c == '_')) {
			out[w++] = c;
			hasChar = true;
		} else if (c == '.' || c == '-') {
			out[w++] = '_';
			hasChar = true;
		}
	}
	if (!hasChar) {
		memcpy(out + w, fb, strlen(fb));
		w += strlen(fb);
	}
	out[w] = '\0';
	return out;
}

static char* Content_TemplatePath(const char* name)
{
	if (!AppPath || !name || !name[0]) return NULL;
	return xrtPathJoin(xrtPathJoin(xrtPathJoin(AppPath, "content"), "templates"), name);
}

/* 读模板 + 去 UTF-8 BOM；失败返回 NULL（v1 Content_LoadGeneratorTemplate 同语义） */
static char* Content_LoadGeneratorTemplate(const char* name)
{
	char* path = Content_TemplatePath(name);
	char* text = NULL;
	if (path) {
		size_t size = 0;
		bytes data = xrtFileReadAll(path, &size);
		if (data) {
			text = (char*)xrtMalloc(size + 1);
			if (text) {
				memcpy(text, data, size);
				text[size] = '\0';
				if (size >= 3 && (unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF)
					memmove(text, text + 3, size - 3 + 1);
			}
			xrtFree(data);
		}
		xrtFree(path);
	}
	return text;
}

/* 单趟扫描全量替换（v1 Content_StringReplaceAll；needle 为空回退整串拷贝） */
static char* Content_StringReplaceAll(const char* text, const char* needle, const char* value)
{
	size_t needleLen; size_t valueLen; size_t count = 0; size_t outCap;
	const char* cursor; const char* hit; char* out; char* w;
	if (!text) return NULL;
	if (!needle || !needle[0]) return xrtStrDup(text);
	if (!value) value = "";
	needleLen = strlen(needle);
	valueLen = strlen(value);
	for (cursor = strstr(text, needle); cursor; cursor = strstr(cursor + needleLen, needle))
		count++;
	outCap = strlen(text) + count * (valueLen > needleLen ? valueLen - needleLen : 0) + 1;
	out = (char*)xrtMalloc(outCap);
	if (!out) return NULL;
	w = out;
	cursor = text;
	while ((hit = strstr(cursor, needle)) != NULL) {
		if (hit > cursor) {
			memcpy(w, cursor, (size_t)(hit - cursor));
			w += hit - cursor;
		}
		if (valueLen) {
			memcpy(w, value, valueLen);
			w += valueLen;
		}
		cursor = hit + needleLen;
	}
	{
		size_t rest = strlen(cursor);
		if (rest) {
			memcpy(w, cursor, rest);
			w += rest;
		}
	}
	*w = '\0';
	return out;
}

/* 消费旧串的所有权并返回替换结果（v1 Content_TemplateSet） */
static char* Content_TemplateSet(char* template, const char* needle, const char* value)
{
	char* next = Content_StringReplaceAll(template, needle, value);
	xrtFree(template);
	return next;
}

static char* Content_StringifyJson(xvalue* value, bool pretty)
{
	size_t size = 0;
	return value ? xrtJsonStringify(value, pretty, &size) : NULL;
}

/* ==================== 能力包静态映射表（v1 照抄） ==================== */

/* define 名：XADMIN_CAP_ + key 各段大写（. - → _），如 content.category → XADMIN_CAP_CONTENT_CATEGORY */
static char* Content_CapabilityDefineName(const char* packId)
{
	const char* p; char* out; size_t w = 0;
	if (!packId || !packId[0]) return xrtStrDup("XADMIN_CAP_UNKNOWN");
	out = (char*)xrtMalloc(strlen(packId) + 16);
	if (!out) return NULL;
	memcpy(out, "XADMIN_CAP_", 11);
	w = 11;
	for (p = packId; *p; p++) {
		char c = *p;
		if ((c >= 'a') && (c <= 'z')) out[w++] = (char)(c - 'a' + 'A');
		else if (((c >= 'A') && (c <= 'Z')) || ((c >= '0') && (c <= '9'))) out[w++] = c;
		else out[w++] = '_';
	}
	out[w] = '\0';
	return out;
}

static bool Content_IsBuiltinAbilityPack(const char* packId)
{
	if (!packId) return false;
	if (!strcmp(packId, "content.comment")) return true;
	if (!strcmp(packId, "content.tag")) return true;
	if (!strcmp(packId, "content.topic")) return true;
	if (!strcmp(packId, "content.sensitive")) return true;
	if (!strcmp(packId, "content.static")) return true;
	if (!strcmp(packId, "content.like")) return true;
	if (!strcmp(packId, "content.view-stat")) return true;
	return false;
}

/* v1 缺省权限映射（21 条 + 兜底） */
static const char* Content_DefaultAbilityPermission(const char* packId)
{
	if (!packId) return "ability.manage";
	if (!strcmp(packId, "content.comment")) return "comment.view";
	if (!strcmp(packId, "content.tag")) return "tag.manage";
	if (!strcmp(packId, "content.topic")) return "topic.manage";
	if (!strcmp(packId, "content.sensitive")) return "sensitive.manage";
	if (!strcmp(packId, "content.static")) return "static.manage";
	if (!strcmp(packId, "content.like")) return "like.view";
	if (!strcmp(packId, "content.view-stat")) return "view_stat.view";
	if (!strcmp(packId, "content.seo")) return "seo.manage";
	if (!strcmp(packId, "content.slug")) return "slug.manage";
	if (!strcmp(packId, "content.redirect")) return "redirect.manage";
	if (!strcmp(packId, "content.category")) return "category.manage";
	if (!strcmp(packId, "content.media")) return "media.manage";
	if (!strcmp(packId, "content.revision")) return "revision.manage";
	if (!strcmp(packId, "content.workflow")) return "workflow.manage";
	if (!strcmp(packId, "content.search")) return "search.manage";
	if (!strcmp(packId, "content.sitemap")) return "sitemap.manage";
	if (!strcmp(packId, "content.related")) return "related.manage";
	if (!strcmp(packId, "content.form")) return "form.manage";
	if (!strcmp(packId, "content.access")) return "access.manage";
	if (!strcmp(packId, "content.audit-log")) return "audit_log.view";
	if (!strcmp(packId, "content.import-export")) return "import_export.manage";
	return "ability.manage";
}

static const char* Content_DefaultAbilityMenuTitle(const char* packId, const char* fallback)
{
	if (!packId) return fallback;
	if (!strcmp(packId, "content.comment")) return "评论管理";
	if (!strcmp(packId, "content.tag")) return "标签管理";
	if (!strcmp(packId, "content.topic")) return "专题管理";
	if (!strcmp(packId, "content.sensitive")) return "敏感词管理";
	if (!strcmp(packId, "content.static")) return "静态化管理";
	if (!strcmp(packId, "content.like")) return "点赞管理";
	if (!strcmp(packId, "content.view-stat")) return "访问统计";
	if (!strcmp(packId, "content.seo")) return "SEO 优化";
	if (!strcmp(packId, "content.slug")) return "固定链接";
	if (!strcmp(packId, "content.redirect")) return "跳转规则";
	if (!strcmp(packId, "content.category")) return "栏目";
	if (!strcmp(packId, "content.media")) return "媒体资源";
	if (!strcmp(packId, "content.revision")) return "内容版本";
	if (!strcmp(packId, "content.workflow")) return "审核流程";
	if (!strcmp(packId, "content.search")) return "内容搜索";
	if (!strcmp(packId, "content.sitemap")) return "站点地图";
	if (!strcmp(packId, "content.related")) return "相关推荐";
	if (!strcmp(packId, "content.form")) return "内容表单";
	if (!strcmp(packId, "content.access")) return "阅读权限";
	if (!strcmp(packId, "content.audit-log")) return "操作审计";
	if (!strcmp(packId, "content.import-export")) return "导入导出";
	return fallback;
}

/* capabilities[] 条目启用判定（缺省视为启用） */
static bool Content_CapabilityEnabled(xvalue* item)
{
	if (!item || xrtValueType(item) != XVALUE_OBJECT) return false;
	return ValueHas(item, "enabled") ? ValueBool(item, "enabled") : true;
}

static bool Content_SpecHasCapability(xvalue* spec, const char* key)
{
	xvalue* capabilities = ValueGet(spec, "capabilities");
	uint32 i;
	if (!key || !capabilities || xrtValueType(capabilities) != XVALUE_ARRAY) return false;
	for (i = 0; i < ValueCount(capabilities); i++) {
		xvalue* cap = xrtValueArrayGet(capabilities, i);
		str capKey = (cap && xrtValueType(cap) == XVALUE_OBJECT) ? ValueText(cap, "key") : NULL;
		if (Content_CapabilityEnabled(cap) && capKey && !strcmp(capKey, key)) return true;
	}
	return false;
}

/* ==================== managed main.c 生成段（v3 方言 C 代码） ==================== */

/* 各启用包 schema.sql 拼接为 G_SchemaSql 的 C 字符串续段（每包一段换行分隔） */
static char* Content_BuildAbilityPackSchemaSql(xvalue* spec)
{
	xvalue* packs = ValueGet(spec, "capabilities");
	char* code = xrtStrDup("");
	uint32 i;
	if (!packs || xrtValueType(packs) != XVALUE_ARRAY) return code;
	for (i = 0; i < ValueCount(packs); i++) {
		xvalue* item = xrtValueArrayGet(packs, i);
		str key = (item && xrtValueType(item) == XVALUE_OBJECT) ? ValueText(item, "key") : NULL;
		xvalue* packDetail = NULL;
		str dir = NULL;
		char* path = NULL;
		char* sql = NULL;
		char* escaped = NULL;
		char* next = NULL;
		if (!key || !key[0]) continue;
		if (!Content_CapabilityEnabled(item)) continue;
		packDetail = ContentPack_GetDetail(key);
		dir = packDetail ? ValueText(packDetail, "path") : NULL;
		if (!dir || !dir[0]) {
			xrtValueRelease(packDetail);
			continue;
		}
		path = xrtPathJoin(dir, "schema.sql");
		if (path && xrtPathExists(path)) {
			size_t size = 0;
			bytes data = xrtFileReadAll(path, &size);
			if (data) {
				sql = (char*)xrtMalloc(size + 1);
				if (sql) {
					memcpy(sql, data, size);
					sql[size] = '\0';
				}
				xrtFree(data);
			}
		}
		if (sql && sql[0]) {
			escaped = Content_EscapeCString(sql);
			next = xrtFormat("%s\n\t\"%s\"", Content_TextOr(code, ""), Content_TextOr(escaped, ""));
			xrtFree(code);
			code = next;
		}
		xrtFree(path);
		xrtFree(sql);
		xrtFree(escaped);
		xrtValueRelease(packDetail);
	}
	return code;
}

/* 能力包后台路由注册段：path=/admin/view/plugin/{xid}/pack/{key}，auth 绑定本包权限 */
static char* Content_BuildAbilityPackRouteCode(const char* pluginXid, xvalue* spec)
{
	xvalue* packs = ValueGet(spec, "capabilities");
	char* code = xrtStrDup("");
	uint32 i;
	if (!packs || xrtValueType(packs) != XVALUE_ARRAY) return code;
	for (i = 0; i < ValueCount(packs); i++) {
		xvalue* item = xrtValueArrayGet(packs, i);
		str key = (item && xrtValueType(item) == XVALUE_OBJECT) ? ValueText(item, "key") : NULL;
		char* safeKey = NULL;
		char* next = NULL;
		if (!key || !key[0]) continue;
		if (!Content_CapabilityEnabled(item)) continue;
		safeKey = Content_SanitizeSqlIdent(key, "pack");
		next = xrtFormat(
			"%s\t\tmemset(&route, 0, sizeof(route));\n"
			"\t\troute.path = \"/admin/view/plugin/%s/pack/%s\";\n"
			"\t\troute.proc = Managed_RequestAbilityPackView;\n"
			"\t\troute.need_auth = true;\n"
			"\t\troute.admin_only = true;\n"
			"\t\troute.auth_id = auth_%s;\n"
			"\t\tif ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {\n"
			"\t\t\tprintf(\"        [ManagedPlugin] ability route register failed: xid=%s path=%%s\\n\", route.path);\n"
			"\t\t\tgoto failed;\n"
			"\t\t}\n\n",
			Content_TextOr(code, ""),
			Content_TextOr(pluginXid, ""),
			Content_TextOr(safeKey, "pack"),
			Content_TextOr(safeKey, "pack"),
			Content_TextOr(pluginXid, ""));
		xrtFree(code);
		xrtFree(safeKey);
		code = next;
	}
	return code;
}

/* 能力包菜单注册段：title=包标题（v1 中文缺省表），href 同路由 */
static char* Content_BuildAbilityPackMenuCode(const char* pluginXid, xvalue* spec)
{
	xvalue* packs = ValueGet(spec, "capabilities");
	char* code = xrtStrDup("");
	uint32 i;
	if (!packs || xrtValueType(packs) != XVALUE_ARRAY) return code;
	for (i = 0; i < ValueCount(packs); i++) {
		xvalue* item = xrtValueArrayGet(packs, i);
		str key = (item && xrtValueType(item) == XVALUE_OBJECT) ? ValueText(item, "key") : NULL;
		xvalue* packDetail = NULL;
		str packTitle = NULL;
		const char* title = NULL;
		char* safeKey = NULL;
		char* safeTitle = NULL;
		char* next = NULL;
		if (!key || !key[0]) continue;
		if (!Content_CapabilityEnabled(item)) continue;
		packDetail = ContentPack_GetDetail(key);
		packTitle = packDetail ? ValueText(packDetail, "title") : key;
		title = Content_DefaultAbilityMenuTitle(key, packTitle);
		safeKey = Content_SanitizeSqlIdent(key, "pack");
		safeTitle = Content_EscapeCString(Content_TextOr(title, key));
		next = xrtFormat(
			"%s\t\tmemset(&menu, 0, sizeof(menu));\n"
			"\t\tmenu.key = \"%s.pack.%s\";\n"
			"\t\tmenu.parent_id = iRootMenuId;\n"
			"\t\tmenu.title = \"%s\";\n"
			"\t\tmenu.icon = \"layui-icon layui-icon-component\";\n"
			"\t\tmenu.type = 1;\n"
			"\t\tmenu.open_type = \"_component\";\n"
			"\t\tmenu.href = \"/admin/view/plugin/%s/pack/%s\";\n"
			"\t\tmenu.sort = %d;\n"
			"\t\tmenu.visible = true;\n"
			"\t\tmenu.remark = \"Generated ability pack admin page\";\n"
			"\t\tif ( XAdmin_RegisterMenu(handle, &menu, NULL, NULL) != 0 ) {\n"
			"\t\t\tprintf(\"        [ManagedPlugin] ability menu register failed: xid=%s href=%%s\\n\", menu.href);\n"
			"\t\t\tgoto failed;\n"
			"\t\t}\n\n",
			Content_TextOr(code, ""),
			Content_TextOr(pluginXid, ""),
			Content_TextOr(safeKey, "pack"),
			Content_TextOr(safeTitle, ""),
			Content_TextOr(pluginXid, ""),
			Content_TextOr(safeKey, "pack"),
			100 + (int)i * 10,
			Content_TextOr(pluginXid, ""));
		xrtFree(code);
		xrtFree(safeKey);
		xrtFree(safeTitle);
		xrtValueRelease(packDetail);
		code = next;
	}
	return code;
}

/* 能力包权限注册段：一个 authGroup（sort=700000）+ 每包每权限一条 XAdmin_RegisterAuth。
 * 内置包的 auth_<key> 变量在模板 OnStart 中预声明（赋值形式），非内置包就地声明。 */
static char* Content_BuildAbilityPackAuthCode(const char* pluginXid, const char* pluginTitle, xvalue* spec)
{
	xvalue* packs = ValueGet(spec, "capabilities");
	char* code = xrtStrDup("");
	char* safeGroupName = Content_EscapeCString(Content_TextOr(pluginTitle, pluginXid ? pluginXid : "Generated Content Plugin"));
	bool hasPack = false;
	uint32 i;
	if (!packs || xrtValueType(packs) != XVALUE_ARRAY) {
		xrtFree(safeGroupName);
		return code;
	}
	for (i = 0; i < ValueCount(packs); i++) {
		xvalue* item = xrtValueArrayGet(packs, i);
		if (!Content_CapabilityEnabled(item)) continue;
		hasPack = true;
		break;
	}
	if (!hasPack) {
		xrtFree(safeGroupName);
		return code;
	}
	xrtFree(code);
	code = xrtFormat(
		"\tmemset(&authGroup, 0, sizeof(authGroup));\n"
		"\tauthGroup.scope = XADMIN_AUTH_SCOPE_ADMIN;\n"
		"\tauthGroup.name = \"%s Ability Packs\";\n"
		"\tauthGroup.description = \"Generated content ability pack permissions\";\n"
		"\tauthGroup.sort = 700000;\n"
		"\tif ( XAdmin_RegisterAuthGroup(handle, &authGroup, &iAbilityAuthGroupId, NULL) != 0 ) goto failed;\n\n",
		Content_TextOr(safeGroupName, ""));
	for (i = 0; i < ValueCount(packs); i++) {
		xvalue* item = xrtValueArrayGet(packs, i);
		str key = (item && xrtValueType(item) == XVALUE_OBJECT) ? ValueText(item, "key") : NULL;
		char* safeKey = NULL;
		xvalue* packDetail = NULL;
		xvalue* contracts = NULL;
		xvalue* permissions = NULL;
		str contractsJson = NULL;
		bool builtinPack = false;
		bool declaredFirst = false;
		const char* fallbackPerm = NULL;
		uint32 permCount;
		uint32 j;
		char* next = NULL;
		if (!key || !key[0]) continue;
		if (!Content_CapabilityEnabled(item)) continue;
		safeKey = Content_SanitizeSqlIdent(key, "pack");
		builtinPack = Content_IsBuiltinAbilityPack(key);
		fallbackPerm = Content_DefaultAbilityPermission(key);
		packDetail = ContentPack_GetDetail(key);
		contractsJson = packDetail ? ValueText(packDetail, "contractsJson") : NULL;
		if (contractsJson && contractsJson[0]) {
			contracts = JsonParseN(contractsJson, 0);
			permissions = contracts ? ValueGet(contracts, "permissions") : NULL;
		}
		if (!permissions || xrtValueType(permissions) != XVALUE_ARRAY || ValueCount(permissions) == 0)
			permissions = NULL;
		permCount = permissions ? (uint32)ValueCount(permissions) : 1;
		for (j = 0; j < permCount; j++) {
			xvalue* permValue = permissions ? xrtValueArrayGet(permissions, j) : NULL;
			const char* perm = permissions ? ValueTextOf(permValue) : fallbackPerm;
			char* authName = NULL;
			char* authDesc = NULL;
			char* safeName = NULL;
			char* safeDesc = NULL;
			if (!perm || !perm[0]) continue;
			authName = xrtFormat("%s.%s", Content_TextOr(pluginXid, ""), perm);
			authDesc = xrtFormat("Ability pack permission: %s", perm);
			safeName = Content_EscapeCString(authName);
			safeDesc = Content_EscapeCString(authDesc);
			next = xrtFormat(
				"%s\tint auth_%s_%u = 0;\n"
				"\tmemset(&auth, 0, sizeof(auth));\n"
				"\tauth.scope = XADMIN_AUTH_SCOPE_ADMIN;\n"
				"\tauth.group_id = iAbilityAuthGroupId;\n"
				"\tauth.name = \"%s\";\n"
				"\tauth.description = \"%s\";\n"
				"\tauth.sort = %d;\n"
				"\tif ( XAdmin_RegisterAuth(handle, &auth, &auth_%s_%u, NULL) != 0 ) goto failed;\n",
				Content_TextOr(code, ""),
				Content_TextOr(safeKey, "pack"), j,
				Content_TextOr(safeName, ""),
				Content_TextOr(safeDesc, ""),
				700100 + (int)i * 100 + (int)j,
				Content_TextOr(safeKey, "pack"), j);
			xrtFree(code);
			code = next;
			if (!declaredFirst) {
				next = xrtFormat(
					builtinPack ? "%s\tauth_%s = auth_%s_%u;\n" : "%s\tint auth_%s = auth_%s_%u;\n",
					Content_TextOr(code, ""),
					Content_TextOr(safeKey, "pack"),
					Content_TextOr(safeKey, "pack"), j);
				xrtFree(code);
				code = next;
				declaredFirst = true;
			}
			xrtFree(authName);
			xrtFree(authDesc);
			xrtFree(safeName);
			xrtFree(safeDesc);
		}
		if (!declaredFirst) {
			next = xrtFormat(
				builtinPack ? "%s\tauth_%s = 0;\n" : "%s\tint auth_%s = 0;\n",
				Content_TextOr(code, ""),
				Content_TextOr(safeKey, "pack"));
			xrtFree(code);
			code = next;
		}
		next = xrtFormat("%s\n", Content_TextOr(code, ""));
		xrtFree(code);
		code = next;
		xrtValueRelease(contracts);
		xrtValueRelease(packDetail);
		xrtFree(safeKey);
	}
	xrtFree(safeGroupName);
	return code;
}

/* 解析 "key|标题:f1,f2" 多行分组映射 → [{key,title,fields:[name]}]（行分隔为换行符） */
static xvalue* Content_ParseGroupAssignments(const char* text)
{
	xvalue* arr = ValueArray();
	const char* p = text ? text : "";
	while (*p) {
		const char* lineEnd = strchr(p, 10);
		size_t lineLen = lineEnd ? (size_t)(lineEnd - p) : strlen(p);
		const char* colon = NULL;
		size_t i;
		char sKey[96];
		char sTitle[96];
		xvalue* grp;
		xvalue* names;
		const char* q;
		if (lineLen < 2) { p = lineEnd ? lineEnd + 1 : p + lineLen; continue; }
		for (i = 0; i < lineLen; i++) {
			if (p[i] == ':') { colon = p + i; break; }
		}
		if (!colon || colon == p) { p = lineEnd ? lineEnd + 1 : p + lineLen; continue; }
		{
			size_t keyLen = (size_t)(colon - p);
			const char* bar = NULL;
			for (i = 0; i < keyLen; i++) {
				if (p[i] == '|') { bar = p + i; break; }
			}
			if (bar) {
				size_t t1 = (size_t)(bar - p);
				size_t t2 = keyLen - t1 - 1;
				if (t1 >= sizeof(sKey)) t1 = sizeof(sKey) - 1;
				if (t2 >= sizeof(sTitle)) t2 = sizeof(sTitle) - 1;
				memcpy(sKey, p, t1); sKey[t1] = 0;
				memcpy(sTitle, bar + 1, t2); sTitle[t2] = 0;
			} else {
				if (keyLen >= sizeof(sKey)) keyLen = sizeof(sKey) - 1;
				memcpy(sKey, p, keyLen); sKey[keyLen] = 0;
				snprintf(sTitle, sizeof(sTitle), "%s", sKey);
			}
		}
		grp = ValueObject();
		ValueSetText(grp, "key", sKey);
		ValueSetText(grp, "title", sTitle);
		names = ValueArray();
		q = colon + 1;
		for (;;) {
			const char* comma = q;
			char sName[96];
			size_t n = 0;
			while (comma < p + lineLen && *comma != ',') comma++;
			n = (size_t)(comma - q);
			if (n > 0 && n < sizeof(sName)) {
				memcpy(sName, q, n); sName[n] = 0;
				ValueArrayOwn(names, xrtValueString(xrtStrView(sName)));
			}
			if (comma >= p + lineLen) break;
			q = comma + 1;
		}
		ValueSetOwn(grp, "fields", names);
		ValueArrayOwn(arr, grp);
		p = lineEnd ? lineEnd + 1 : p + lineLen;
	}
	return arr;
}

/* 逗号分隔字段名 → 字符串数组 */
static xvalue* Content_ParseNameList(const char* text)
{
	xvalue* arr = ValueArray();
	const char* p = text ? text : "";
	while (*p) {
		char sName[96];
		size_t n = 0;
		while (*p && *p != ',' && *p != 32 && *p != 9 && n + 1 < sizeof(sName)) sName[n++] = *p++;
		sName[n] = 0;
		if (n) ValueArrayOwn(arr, xrtValueString(xrtStrView(sName)));
		while (*p == ',' || *p == 32 || *p == 9) p++;
	}
	return arr;
}

/* 策略角色名 → authLevel（生成期解析烘焙；未配置/找不到=0 不设门） */
static int64 Content_ResolveRoleAuthLevel(const char* roleName)
{
	sqlite3_stmt* stmt = NULL;
	int64 level = 0;
	if (!roleName || !roleName[0] || !G_DB) return 0;
	if (sqlite3_prepare_v2(G_DB, "SELECT authLevel FROM role WHERE name=? AND isDelete=0 ORDER BY id ASC LIMIT 1", -1, &stmt, NULL) != SQLITE_OK)
		return 0;
	sqlite3_bind_text(stmt, 1, roleName, -1, NULL);
	if (sqlite3_step(stmt) == SQLITE_ROW) level = sqlite3_column_int64(stmt, 0);
	sqlite3_finalize(stmt);
	return level;
}

/* ==================== R3/R4 烘焙：运行时配置读取 → 生成期常量 ====================
 * 原则：配置好的模型编译为静态操作代码。spec.capabilities[].config 在生成期
 * 解析为常量/专属调用，运行时不再装载 contracts.json 解释配置值。 */

static xvalue* Content_FindCapabilityConfig(xvalue* spec, const char* sPackId)
{
	xvalue* caps = spec ? ValueGet(spec, "capabilities") : NULL;
	uint32 i;
	if (!caps || xrtValueType(caps) != XVALUE_ARRAY) return NULL;
	for (i = 0; i < ValueCount(caps); i++) {
		xvalue* cap = xrtValueArrayGet(caps, i);
		str key = (cap && xrtValueType(cap) == XVALUE_OBJECT) ? ValueText(cap, "key") : NULL;
		if (key && strcmp(key, sPackId) == 0 && Content_CapabilityEnabled(cap)) {
			xvalue* cfg = ValueGet(cap, "config");
			return (cfg && xrtValueType(cfg) == XVALUE_OBJECT) ? cfg : NULL;
		}
	}
	return NULL;
}

static long long Content_BakeIntValue(xvalue* cfg, const char* sKey, long long iDef)
{
	xvalue* v = cfg ? ValueGet(cfg, sKey) : NULL;
	if (!v) return iDef;
	if (xrtValueType(v) == XVALUE_INT) return ValueIntOf(v);
	if (xrtValueType(v) == XVALUE_STRING) {
		str t = ValueTextOf(v);
		return t ? atoll(t) : iDef;
	}
	if (xrtValueType(v) == XVALUE_BOOL) return ValueBoolOf(v) ? 1 : 0;
	return iDef;
}

static bool Content_BakeBoolValue(xvalue* cfg, const char* sKey, bool bDef)
{
	xvalue* v = cfg ? ValueGet(cfg, sKey) : NULL;
	if (!v) return bDef;
	if (xrtValueType(v) == XVALUE_BOOL) return ValueBoolOf(v) ? true : false;
	if (xrtValueType(v) == XVALUE_INT) return ValueIntOf(v) != 0;
	if (xrtValueType(v) == XVALUE_STRING) {
		str t = ValueTextOf(v);
		return t && (strcmp(t, "true") == 0 || strcmp(t, "1") == 0 || strcmp(t, "on") == 0);
	}
	return bDef;
}

static str Content_BakeTextValueDup(xvalue* cfg, const char* sKey, const char* sDef)
{
	xvalue* v = cfg ? ValueGet(cfg, sKey) : NULL;
	str t = NULL;
	if (v && xrtValueType(v) == XVALUE_STRING) t = ValueTextOf(v);
	return xrtStrDup(t ? t : (sDef ? sDef : ""));
}

/* 追加片段并保证 NUL 终止（xbuffer 无 NUL 约定） */
static char* Content_BakeBufferToString(xbuffer* buf)
{
	str out;
	if (!buf) return xrtStrDup("");
	xrtBufferAppendByte(buf, 0);
	out = buf->Data ? xrtStrDup((const char*)buf->Data) : xrtStrDup("");
	xrtBufferDestroy(buf);
	return out;
}

static void Content_BakeAppend(xbuffer* buf, const char* sText, size_t iLen)
{
	xrtBufferAppend(buf, (xbytesview){(cbytes)sText, iLen});
}

/* 读双引号字面量内容（起点为开引号后），返回结束引号位置 */
static const char* Content_BakeReadQuoted(const char* p, char* out, size_t cap)
{
	size_t n = 0;
	while (*p && *p != '"') {
		if (n + 1 < cap) out[n++] = *p;
		p++;
	}
	out[n] = 0;
	return (*p == '"') ? p : NULL;
}

/* 形态 A：FN("PACK", "KEY", DEF)（DEF 为数字或 true/false）→ 常量表达式 */
static char* Content_BakePassBare(char* text, xvalue* spec, const char* sFn, bool bIsBool, const char* sForcePack, int* pCount)
{
	xbuffer* out = xrtBufferCreate();
	const char* p = text;
	size_t nFn = strlen(sFn);
	char sPack[96], sKey[96];
	if (!out) return text;
	while (*p) {
		const char* hit = strstr(p, sFn);
		char sRep[256];
		const char* q;
		if (!hit) { Content_BakeAppend(out, p, strlen(p)); break; }
		Content_BakeAppend(out, p, (size_t)(hit - p));
		q = hit + nFn;
		if (q[0] != '"') {
			Content_BakeAppend(out, sFn, nFn);
			p = hit + nFn;
			continue;
		}
		if (sForcePack) {
			if (!(q = Content_BakeReadQuoted(q + 1, sKey, sizeof(sKey)))) {
				Content_BakeAppend(out, sFn, nFn);
				p = hit + nFn;
				continue;
			}
		} else {
			if (!(q = Content_BakeReadQuoted(q + 1, sPack, sizeof(sPack)))) {
				Content_BakeAppend(out, sFn, nFn);
				p = hit + nFn;
				continue;
			}
			q++;
			if (q[0] != ',' || q[1] != ' ' || q[2] != '"' || !(q = Content_BakeReadQuoted(q + 3, sKey, sizeof(sKey)))) {
				Content_BakeAppend(out, sFn, nFn);
				p = hit + nFn;
				continue;
			}
		}
		q++;
		if (q[0] == ',' && q[1] == ' ') {
			xvalue* cfg = Content_FindCapabilityConfig(spec, sForcePack ? sForcePack : sPack);
			const char* d = q + 2;
			const char* e = strchr(d, ')');
			char sDef[64];
			size_t n = e ? (size_t)(e - d) : 0;
			if (e && n < sizeof(sDef)) {
				memcpy(sDef, d, n); sDef[n] = 0;
				if (bIsBool) {
					bool bVal = Content_BakeBoolValue(cfg, sKey, strcmp(sDef, "true") == 0);
					snprintf(sRep, sizeof(sRep), "(%s)", bVal ? "1" : "0");
				} else {
					long long iVal = Content_BakeIntValue(cfg, sKey, atoll(sDef));
					/* SearchWeight 家族 0..1000 钳位（对齐被删助手的语义） */
					if (sForcePack) {
						if (iVal < 0) iVal = 0;
						if (iVal > 1000) iVal = 1000;
					}
					snprintf(sRep, sizeof(sRep), "((int)%lld)", iVal);
				}
				Content_BakeAppend(out, sRep, strlen(sRep));
				(*pCount)++;
				p = e + 1;
				continue;
			}
		}
		Content_BakeAppend(out, sFn, nFn);
		p = hit + nFn;
	}
	return Content_BakeBufferToString(out);
}

/* 形态 B：FN("PACK", "KEY", "DEF")（DEF 为字符串字面量）→ xrtStrDup("VALUE") */
static char* Content_BakePassQuoted(char* text, xvalue* spec, const char* sFn, int* pCount)
{
	xbuffer* out = xrtBufferCreate();
	const char* p = text;
	size_t nFn = strlen(sFn);
	char sPack[96], sKey[96], sDef[256];
	if (!out) return text;
	while (*p) {
		const char* hit = strstr(p, sFn);
		const char* q;
		if (!hit) { Content_BakeAppend(out, p, strlen(p)); break; }
		Content_BakeAppend(out, p, (size_t)(hit - p));
		q = hit + nFn;
		if (q[0] != '"' || !(q = Content_BakeReadQuoted(q + 1, sPack, sizeof(sPack)))) {
			Content_BakeAppend(out, sFn, nFn); p = hit + nFn; continue;
		}
		q++;
		if (q[0] != ',' || q[1] != ' ' || q[2] != '"' || !(q = Content_BakeReadQuoted(q + 3, sKey, sizeof(sKey)))) {
			Content_BakeAppend(out, sFn, nFn); p = hit + nFn; continue;
		}
		q++;
		if (q[0] == ',' && q[1] == ' ' && q[2] == '"' && (q = Content_BakeReadQuoted(q + 3, sDef, sizeof(sDef))) && q[1] == ')') {
			xvalue* cfg = Content_FindCapabilityConfig(spec, sPack);
			str sVal = Content_BakeTextValueDup(cfg, sKey, sDef);
			str sEsc = Content_EscapeCString(sVal ? sVal : "");
			xbuffer* rep = xrtBufferCreate();
			if (rep) {
				Content_BakeAppend(rep, "xrtStrDup(\"", 11);
				Content_BakeAppend(rep, sEsc ? sEsc : "", sEsc ? strlen(sEsc) : 0);
				Content_BakeAppend(rep, "\")", 2);
			}
			{
				str sRepStr = Content_BakeBufferToString(rep);
				Content_BakeAppend(out, sRepStr ? sRepStr : "xrtStrDup(\"\")", sRepStr ? strlen(sRepStr) : 13);
				xrtFree(sRepStr);
			}
			xrtFree(sVal);
			xrtFree(sEsc);
			(*pCount)++;
			p = q + 2;
			continue;
		}
		Content_BakeAppend(out, sFn, nFn);
		p = hit + nFn;
	}
	return Content_BakeBufferToString(out);
}

static void Content_BakeSanitizeIdent(const char* sIn, char* out, size_t cap)
{
	size_t n = 0;
	for (; sIn && *sIn && n + 1 < cap; sIn++) {
		char ch = *sIn;
		bool bOk = ((ch >= 'a') && (ch <= 'z')) || ((ch >= 'A') && (ch <= 'Z')) || ((ch >= '0') && (ch <= '9')) || (ch == '_');
		out[n++] = bOk ? ch : '_';
	}
	out[n] = 0;
}

/* SeoConfigText(tblConfig, "KEY") → Managed_BakedSeoTemplateText("KEY") */
static char* Content_BakePassSeoLiteral(char* text, int* pCount)
{
	xbuffer* out = xrtBufferCreate();
	const char* p = text;
	const char* ND = "Managed_SeoConfigText(tblConfig, \"";
	size_t nFn = strlen(ND);
	char sKey[96];
	if (!out) return text;
	while (*p) {
		const char* hit = strstr(p, ND);
		const char* q;
		char sRep[160];
		if (!hit) { Content_BakeAppend(out, p, strlen(p)); break; }
		Content_BakeAppend(out, p, (size_t)(hit - p));
		q = hit + nFn;
		if ((q = Content_BakeReadQuoted(q, sKey, sizeof(sKey))) && q[1] == ')') {
			snprintf(sRep, sizeof(sRep), "Managed_BakedSeoTemplateText(\"%s\")", sKey);
			Content_BakeAppend(out, sRep, strlen(sRep));
			(*pCount)++;
			p = q + 2;
			continue;
		}
		Content_BakeAppend(out, "Managed_SeoConfigText(", 22);
		p = hit + 22;
	}
	return Content_BakeBufferToString(out);
}

/* ArrayDup("PACK","KEY") → 烘焙数组辅助函数调用（辅助定义累积到 *psHelpers） */
static char* Content_BakePassArray(char* text, xvalue* spec, str* psHelpers, int* pCount)
{
	xbuffer* out = xrtBufferCreate();
	const char* p = text;
	const char* ND = "Managed_AbilityPackConfigArrayDup(\"";
	size_t nFn = strlen(ND);
	if (!out) return text;
	while (*p) {
		const char* hit = strstr(p, ND);
		const char* q;
		char sPack[96], sKey[96];
		if (!hit) { Content_BakeAppend(out, p, strlen(p)); break; }
		Content_BakeAppend(out, p, (size_t)(hit - p));
		q = hit + nFn;
		if ((q = Content_BakeReadQuoted(q, sPack, sizeof(sPack))) && q[1] == ',' && q[2] == ' ' && q[3] == '"' && (q = Content_BakeReadQuoted(q + 4, sKey, sizeof(sKey))) && q[1] == ')') {
			char sCleanPack[96], sCleanKey[96], sFnName[200], sNeedle[220];
			Content_BakeSanitizeIdent(sPack, sCleanPack, sizeof(sCleanPack));
			Content_BakeSanitizeIdent(sKey, sCleanKey, sizeof(sCleanKey));
			snprintf(sFnName, sizeof(sFnName), "Managed_BakedConfigArray_%s_%s()", sCleanPack, sCleanKey);
			snprintf(sNeedle, sizeof(sNeedle), "Managed_BakedConfigArray_%s_%s(void)", sCleanPack, sCleanKey);
			if (!(*psHelpers) || !strstr(*psHelpers, sNeedle)) {
				xvalue* cfg = Content_FindCapabilityConfig(spec, sPack);
				xvalue* v = cfg ? ValueGet(cfg, sKey) : NULL;
				str sDef = NULL;
				if (v && xrtValueType(v) == XVALUE_ARRAY) {
					char* sJson = Content_StringifyJson(v, false);
					str sEsc = Content_EscapeCString(sJson ? sJson : "[]");
					if (sEsc) sDef = xrtFormat("static xvalue* %s\n{\n\treturn JsonParseN((str)\"%s\", %d);\n}\n\n", sNeedle, sEsc, (int)strlen(sEsc));
					xrtFree(sJson);
					xrtFree(sEsc);
				}
				if (!sDef) sDef = xrtFormat("static xvalue* %s\n{\n\treturn NULL;\n}\n\n", sNeedle);
				{
					str sOld = *psHelpers;
					*psHelpers = xrtFormat("%s%s", sOld ? sOld : "", sDef);
					xrtFree(sOld);
				}
				xrtFree(sDef);
			}
			Content_BakeAppend(out, sFnName, strlen(sFnName));
			(*pCount)++;
			p = q + 2;
			continue;
		}
		Content_BakeAppend(out, "Managed_AbilityPackConfigArrayDup(", 35);
		p = hit + 35;
	}
	return Content_BakeBufferToString(out);
}

/* R3 总编排：全部配置读取烘焙为常量/烘焙调用，返回新文本与辅助函数代码 */
static char* Content_BakeConfigReads(char* text, xvalue* spec, str* psHelpers, int* pCount)
{
	str sHelpers = NULL;
	xvalue* seoCfg = Content_FindCapabilityConfig(spec, "content.seo");
	char* t = text;
	int i = 0;
	int k;
	static const char* SEO_KEYS[8] = {
		"titleTemplate", "keywordsTemplate", "descriptionTemplate", "canonicalTemplate",
		"categoryTitleTemplate", "categoryKeywordsTemplate", "categoryDescriptionTemplate", "categoryCanonicalTemplate"
	};

	/* SeoConfigText 动态形态（栏目模板函数内 sCfgKey） */
	{
		const char* ND = "Managed_SeoConfigText(tblConfig, sCfgKey)";
		const char* hit = strstr(t, ND);
		if (hit) {
			xbuffer* out = xrtBufferCreate();
			if (out) {
				Content_BakeAppend(out, t, (size_t)(hit - t));
				Content_BakeAppend(out, "Managed_BakedSeoTemplateText(sCfgKey)", 37);
				Content_BakeAppend(out, hit + strlen(ND), strlen(hit + strlen(ND)));
				xrtFree(t);
				t = Content_BakeBufferToString(out);
				i++;
			}
		}
	}

	t = Content_BakePassBare(t, spec, "Managed_AbilityPackConfigInt(", false, NULL, &i);
	t = Content_BakePassBare(t, spec, "Managed_AbilityPackConfigBool(", true, NULL, &i);
	t = Content_BakePassQuoted(t, spec, "Managed_AbilityPackConfigTextDup(", &i);
	t = Content_BakePassBare(t, spec, "Managed_SearchWeight(", false, "content.search", &i);
	t = Content_BakePassQuoted(t, spec, "Managed_AbilityRoutePrefixDup(", &i);
	t = Content_BakePassSeoLiteral(t, &i);
	t = Content_BakePassArray(t, spec, &sHelpers, &i);

	/* SEO 模板值表（8 键全量烘焙；未配置=空串） */
	{
		str sHead = xrtFormat("%sstatic const char* Managed_BakedSeoTemplateText(const char* sKey)\n{\n\tif ( sKey == NULL ) return \"\";\n", sHelpers ? sHelpers : "");
		xrtFree(sHelpers);
		sHelpers = sHead;
	}
	for (k = 0; k < 8; k++) {
		str sVal = Content_BakeTextValueDup(seoCfg, SEO_KEYS[k], "");
		str sEsc = Content_EscapeCString(sVal ? sVal : "");
		str line = xrtFormat("\tif ( strcmp(sKey, \"%s\") == 0 ) return \"%s\";\n", SEO_KEYS[k], sEsc ? sEsc : "");
		str next = xrtFormat("%s%s", sHelpers ? sHelpers : "", line ? line : "");
		xrtFree(sVal);
		xrtFree(sEsc);
		xrtFree(line);
		xrtFree(sHelpers);
		sHelpers = next;
	}
	{
		str next = xrtFormat("%s\treturn \"\";\n}\n\n", sHelpers ? sHelpers : "");
		xrtFree(sHelpers);
		sHelpers = next;
	}

	*pCount = i;
	if (psHelpers) *psHelpers = sHelpers;
	else xrtFree(sHelpers);
	return t;
}

/* R4：页面 tab 数值烘焙（defaultSort 此前从未进 managed spec=断链，本处一并接通） */
static void Content_ApplyR3R4Baking(char** pTemplate, xvalue* spec)
{
	char* t = *pTemplate;
	str sHelpers = NULL;
	int iBaked = 0;
	xvalue* pagesObj = spec ? ValueGet(spec, "pages") : NULL;
	bool bHasPages = pagesObj && xrtValueType(pagesObj) == XVALUE_OBJECT;
	long long pageSize = bHasPages ? ValueInt(pagesObj, "pageSize") : 0;
	long long maxScan = bHasPages ? ValueInt(pagesObj, "maxScanRows") : 0;
	str defaultSort = bHasPages ? ValueText(pagesObj, "defaultSort") : NULL;
	char body[160];

	t = Content_BakeConfigReads(t, spec, &sHelpers, &iBaked);
	t = Content_TemplateSet(t, "{{CONTENT_BAKED_CONFIG_HELPERS}}", sHelpers ? sHelpers : "");
	xrtFree(sHelpers);

	if (pageSize >= 1) {
		if (pageSize > 200) pageSize = 200;
		snprintf(body, sizeof(body), "\t(void)tblSpec;\n\t(void)iFallback;\n\treturn %lld;", pageSize);
	} else {
		snprintf(body, sizeof(body), "\t(void)tblSpec;\n\treturn (iFallback > 0) ? iFallback : 20;");
	}
	t = Content_TemplateSet(t, "{{CONTENT_UI_PAGE_SIZE_BODY}}", body);

	if (maxScan >= 1) {
		if (maxScan < 200) maxScan = 200;
		if (maxScan > 50000) maxScan = 50000;
		snprintf(body, sizeof(body), "\t(void)tblSpec;\n\t(void)iFallback;\n\treturn %lld;", maxScan);
	} else {
		snprintf(body, sizeof(body), "\t(void)tblSpec;\n\treturn (iFallback > 0) ? iFallback : 5000;");
	}
	t = Content_TemplateSet(t, "{{CONTENT_UI_MAXSCAN_BODY}}", body);

	{
		str field = NULL;
		bool bAsc = false;
		bool bHas = false;
		if (defaultSort && defaultSort[0]) {
			str s = defaultSort;
			size_t n = strlen(s);
			if (n > 4 && strcmp(s + n - 4, "_asc") == 0) { bAsc = true; bHas = true; }
			else if (n > 5 && strcmp(s + n - 5, "_desc") == 0) { bHas = true; }
			else bHas = true;
			if (strcmp(s, "id") == 0 || strcmp(s, "id_desc") == 0 || strcmp(s, "id_asc") == 0) field = "id";
			else if (strcmp(s, "title") == 0 || strcmp(s, "title_desc") == 0 || strcmp(s, "title_asc") == 0) field = "title";
			else if (strcmp(s, "status") == 0 || strcmp(s, "status_desc") == 0 || strcmp(s, "status_asc") == 0) field = "status";
			else if (strcmp(s, "createTime") == 0 || strcmp(s, "createTime_desc") == 0 || strcmp(s, "createTime_asc") == 0) field = "create_time";
			else if (strcmp(s, "updateTime") == 0 || strcmp(s, "updateTime_desc") == 0 || strcmp(s, "updateTime_asc") == 0
				|| strcmp(s, "update_time_desc") == 0 || strcmp(s, "update_time_asc") == 0) field = "update_time";
		}
		if (bHas && field) snprintf(body, sizeof(body), "\t(void)tblSpec;\n\treturn \"%s\";", field);
		else snprintf(body, sizeof(body), "\t(void)tblSpec;\n\treturn NULL;");
		t = Content_TemplateSet(t, "{{CONTENT_UI_SORTFIELD_BODY}}", body);
		snprintf(body, sizeof(body), "\t(void)tblSpec;\n\treturn %s;", bAsc ? "true" : "false");
		t = Content_TemplateSet(t, "{{CONTENT_UI_SORTASC_BODY}}", body);
	}

	/* R6：displayGroups（含字段标题）烘焙为 C 字符串常量 */
	{
		xvalue* dgArr = bHasPages ? ValueGet(pagesObj, "displayGroups") : NULL;
		bool bDg = dgArr && xrtValueType(dgArr) == XVALUE_ARRAY && ValueCount(dgArr) > 0;
		char* sConst = NULL;
		if (bDg) {
			xvalue* out = ValueArray();
			xvalue* fieldsAll = spec ? ValueGet(spec, "fields") : NULL;
			uint32 g, f, m;
			for (g = 0; g < ValueCount(dgArr); g++) {
				xvalue* grp = xrtValueArrayGet(dgArr, g);
				xvalue* names = grp ? ValueGet(grp, "fields") : NULL;
				xvalue* one = ValueObject();
				xvalue* flds = ValueArray();
				str title = grp ? ValueText(grp, "title") : NULL;
				ValueSetText(one, "title", title ? title : (str)"");
				if (names && xrtValueType(names) == XVALUE_ARRAY) {
					for (f = 0; f < ValueCount(names); f++) {
						str name = ValueArrayText(names, f);
						str ftitle = NULL;
						xvalue* fo = ValueObject();
						for (m = 0; fieldsAll && m < ValueCount(fieldsAll); m++) {
							xvalue* fl = xrtValueArrayGet(fieldsAll, m);
							str fn = fl ? ValueText(fl, "name") : NULL;
							if (fn && name && strcmp(fn, name) == 0) { ftitle = ValueText(fl, "title"); break; }
						}
						ValueSetText(fo, "name", name ? name : (str)"");
						ValueSetText(fo, "title", ftitle ? ftitle : (name ? name : (str)""));
						ValueArrayOwn(flds, fo);
					}
				}
				ValueSetOwn(one, "fields", flds);
				ValueArrayOwn(out, one);
			}
			{
				char* sJson = Content_StringifyJson(out, false);
				str sEsc = Content_EscapeCString(sJson ? sJson : "[]");
				sConst = xrtFormat("\"%s\"", sEsc ? sEsc : "[]");
				xrtFree(sJson);
				xrtFree(sEsc);
			}
			xrtValueRelease(out);
		}
		t = Content_TemplateSet(t, "{{CONTENT_DISPLAY_GROUPS_JSON}}", sConst ? sConst : "\"[]\"");
		xrtFree(sConst);
	}

	printf("        [content-generator] R3/R4 baked %d runtime config reads\n", iBaked);
	*pTemplate = t;
}

/* ==================== R2：字段解释烘焙（静态字段表） ====================
 * 五解释器（defaults/nullable/coerce/validate/extract）不再每请求装载
 * spec.json 解释字段——生成期落为编译期常量表 + 专属循环。 */

static str Content_R2NormalizeStorage(const char* sType)
{
	if (!sType || !sType[0]) return xrtStrDup("text");
	if (strcmp(sType, "int") == 0) return xrtStrDup("integer");
	if (strcmp(sType, "real") == 0 || strcmp(sType, "number") == 0) return xrtStrDup("float");
	if (strcmp(sType, "bool") == 0) return xrtStrDup("boolean");
	return xrtStrDup(sType);
}

/* 语义角色解析（与 Content_FieldSemanticRole/字段 semantic.role 对齐） */
static str Content_R2ResolveRoleField(xvalue* spec, const char* sRole, const char* sFallbackName)
{
	xvalue* fields = spec ? ValueGet(spec, "fields") : NULL;
	uint32 i;
	if (fields && xrtValueType(fields) == XVALUE_ARRAY) {
		for (i = 0; i < ValueCount(fields); i++) {
			xvalue* f = xrtValueArrayGet(fields, i);
			xvalue* sem = f ? ValueGet(f, "semantic") : NULL;
			str role = sem ? ValueText(sem, "role") : NULL;
			if (role && strcmp(role, sRole) == 0) {
				str name = ValueText(f, "name");
				return xrtStrDup(name ? name : "");
			}
		}
		for (i = 0; i < ValueCount(fields); i++) {
			xvalue* f = xrtValueArrayGet(fields, i);
			str name = f ? ValueText(f, "name") : NULL;
			if (name && strcmp(name, sFallbackName) == 0) return xrtStrDup(name);
		}
	}
	return NULL;
}

static void Content_ApplyR2Baking(char** pTemplate, xvalue* spec)
{
	char* t = *pTemplate;
	xvalue* fields = spec ? ValueGet(spec, "fields") : NULL;
	xbuffer* code = xrtBufferCreate();
	xbuffer* tbl = xrtBufferCreate();
	str sRoleTitle = Content_R2ResolveRoleField(spec, "title", "title");
	str sRoleStatus = Content_R2ResolveRoleField(spec, "status", "status");
	str sRoleSlug = Content_R2ResolveRoleField(spec, "slug", "slug");
	str sRoleSummary = NULL;
	char body[256];
	uint32 i;
	(void)sRoleSummary;

	if (!code || !tbl) {
		if (code) xrtBufferDestroy(code);
		if (tbl) xrtBufferDestroy(tbl);
		xrtFree(sRoleTitle);
		xrtFree(sRoleStatus);
		xrtFree(sRoleSlug);
		return;
	}

	/* ---- 字段表 + 烘焙变体助手（经 BAKED_CONFIG_HELPERS 通道发射） ---- */
	{
		str sHead = xrtStrDup(
			"bool Managed_ValueIsEmpty(xvalue* objValue);\n"
			"str Managed_ValueToTextDup(xvalue* objValue);\n"
			"bool Managed_TextEqualsIgnoreCase(const char* sLeft, const char* sRight);\n"
			"typedef struct {\n"
			"\tconst char* name;\n"
			"\tconst char* title;\n"
			"\tconst char* storageType;\n"
			"\tbool columnar;\n"
			"\tbool required;\n"
			"\tbool nullable;\n"
			"\tchar defaultKind;\n"
			"\tint64 defaultInt;\n"
			"\tdouble defaultFloat;\n"
			"\tbool defaultBool;\n"
			"\tconst char* defaultString;\n"
			"} Managed_BakedField;\n\n"
			"static void Managed_ApplyMissingBakedFieldDefault(xvalue* tblData, const Managed_BakedField* fld)\n"
			"{\n"
			"\txvalue* objCurrent;\n"
			"\tif ((tblData == NULL) || (xrtValueType(tblData) != XVALUE_OBJECT) || (fld == NULL)) return;\n"
			"\tobjCurrent = ValueGet(tblData, fld->name);\n"
			"\tif (objCurrent != NULL) return;\n"
			"\tif (fld->defaultKind == 'i') ValueSetOwn(tblData, fld->name, xrtValueInt(fld->defaultInt));\n"
			"\telse if (fld->defaultKind == 'f') ValueSetOwn(tblData, fld->name, xrtValueFloat(fld->defaultFloat));\n"
			"\telse if (fld->defaultKind == 'b') ValueSetOwn(tblData, fld->name, xrtValueBool(fld->defaultBool));\n"
			"\telse if (fld->defaultKind == 's') ValueSetOwn(tblData, fld->name, xrtValueString(xrtStrView(fld->defaultString)));\n"
			"}\n\n"
			"static void Managed_NormalizeNullableBakedField(xvalue* tblData, const Managed_BakedField* fld)\n"
			"{\n"
			"\txvalue* objValue;\n"
			"\tif ((tblData == NULL) || (xrtValueType(tblData) != XVALUE_OBJECT) || (fld == NULL)) return;\n"
			"\tif (!fld->nullable) return;\n"
			"\tobjValue = ValueGet(tblData, fld->name);\n"
			"\tif ((objValue == NULL) || !Managed_ValueIsEmpty(objValue)) return;\n"
			"\tValueSetOwn(tblData, fld->name, xrtValueNull());\n"
			"}\n\n"
			"static void Managed_CoerceBakedFieldValue(xvalue* tblData, const Managed_BakedField* fld)\n"
			"{\n"
			"\txvalue* objValue;\n"
			"\tconst char* sType;\n"
			"\tif ((tblData == NULL) || (xrtValueType(tblData) != XVALUE_OBJECT) || (fld == NULL)) return;\n"
			"\tobjValue = ValueGet(tblData, fld->name);\n"
			"\tif (Managed_ValueIsEmpty(objValue)) return;\n"
			"\tsType = fld->storageType;\n"
			"\tif (strcmp(sType, \"text\") == 0) {\n"
			"\t\tif (xrtValueType(objValue) != XVALUE_STRING) {\n"
			"\t\t\tstr sText = Managed_ValueToTextDup(objValue);\n"
			"\t\t\tValueSetOwnedText(tblData, fld->name, sText ? sText : (str)\"\");\n"
			"\t\t\tif (sText) xrtFree(sText);\n"
			"\t\t}\n"
			"\t\treturn;\n"
			"\t}\n"
			"\tif (strcmp(sType, \"integer\") == 0) {\n"
			"\t\tif (xrtValueType(objValue) == XVALUE_FLOAT) ValueSetInt(tblData, fld->name, (int64)ValueFloatOf(objValue));\n"
			"\t\telse if (xrtValueType(objValue) == XVALUE_STRING) ValueSetInt(tblData, fld->name, atoll(ValueTextOf(objValue)));\n"
			"\t\treturn;\n"
			"\t}\n"
			"\tif (strcmp(sType, \"float\") == 0) {\n"
			"\t\tif (xrtValueType(objValue) == XVALUE_INT) ValueSetFloat(tblData, fld->name, (double)ValueIntOf(objValue));\n"
			"\t\telse if (xrtValueType(objValue) == XVALUE_STRING) ValueSetFloat(tblData, fld->name, strtod(ValueTextOf(objValue), NULL));\n"
			"\t\treturn;\n"
			"\t}\n"
			"\tif (strcmp(sType, \"boolean\") == 0) {\n"
			"\t\tif (xrtValueType(objValue) == XVALUE_INT) ValueSetBool(tblData, fld->name, ValueIntOf(objValue) != 0);\n"
			"\t\telse if (xrtValueType(objValue) == XVALUE_FLOAT) ValueSetBool(tblData, fld->name, ValueFloatOf(objValue) != 0.0);\n"
			"\t\telse if (xrtValueType(objValue) == XVALUE_STRING) {\n"
			"\t\t\tconst char* sText = ValueTextOf(objValue);\n"
			"\t\t\tValueSetBool(tblData, fld->name, Managed_TextEqualsIgnoreCase(sText, \"true\") || (strcmp(sText, \"1\") == 0));\n"
			"\t\t}\n"
			"\t}\n"
			"}\n\n"
			"static const Managed_BakedField MANAGED_BAKED_FIELDS[] = {\n");
		{
			xrtBufferAppend(tbl, (xbytesview){(cbytes)sHead, strlen(sHead)});
			xrtFree(sHead);
		}
		if (fields && xrtValueType(fields) == XVALUE_ARRAY) {
			for (i = 0; i < ValueCount(fields); i++) {
				xvalue* f = xrtValueArrayGet(fields, i);
				str name = f ? ValueText(f, "name") : NULL;
				str title = f ? ValueText(f, "title") : NULL;
				str rawStorage = f ? ValueText(f, "type") : NULL;
				xvalue* storage = f ? ValueGet(f, "storage") : NULL;
				str storageType = storage ? ValueText(storage, "type") : NULL;
				xvalue* dv = f ? ValueGet(f, "defaultValue") : NULL;
				str sNorm;
				str eName = Content_EscapeCString(name ? name : "");
				str eTitle = Content_EscapeCString(title ? title : "");
				char line[1024];
				char kind = 0;
				int64 di = 0;
				double df = 0;
				bool db = false;
				str ds = NULL;
				if (!name || !name[0]) { xrtFree(eName); xrtFree(eTitle); continue; }
				sNorm = Content_R2NormalizeStorage(storageType ? storageType : rawStorage);
				if (dv) {
					if (xrtValueType(dv) == XVALUE_INT) { kind = 'i'; di = ValueIntOf(dv); }
					else if (xrtValueType(dv) == XVALUE_FLOAT) { kind = 'f'; df = ValueFloatOf(dv); }
					else if (xrtValueType(dv) == XVALUE_BOOL) { kind = 'b'; db = ValueBoolOf(dv); }
					else if (xrtValueType(dv) == XVALUE_STRING) { kind = 's'; ds = Content_EscapeCString(ValueTextOf(dv)); }
				}
				{
					bool bColumnar = true;
					if (sRoleTitle && name && strcmp(sRoleTitle, name) == 0) bColumnar = false;
					if (sRoleStatus && name && strcmp(sRoleStatus, name) == 0) bColumnar = false;
					snprintf(line, sizeof(line),
						"\t{ \"%s\", \"%s\", \"%s\", %s, %s, %s, '%c', %lld, %.17g, %s, \"%s\" },\n",
						eName ? eName : "", eTitle ? eTitle : "", sNorm ? sNorm : "text",
						bColumnar ? "true" : "false",
						(f && ValueBool(f, "required")) ? "true" : "false",
						(f && ValueBool(f, "nullable")) ? "true" : "false",
						kind ? kind : '0', (long long)di, df, db ? "true" : "false", ds ? ds : "");
				}
				xrtBufferAppend(tbl, (xbytesview){(cbytes)line, strlen(line)});
				xrtFree(sNorm);
				xrtFree(eName);
				xrtFree(eTitle);
				xrtFree(ds);
			}
		}
		{
			const char* tail = "};\n#define MANAGED_BAKED_FIELD_COUNT (sizeof(MANAGED_BAKED_FIELDS)/sizeof(MANAGED_BAKED_FIELDS[0]))\n\n"
			"static int64 Managed_BakedFieldIntOf(xvalue* v)\n"
			"{\n"
			"\tif (!v) return 0;\n"
			"\tif (xrtValueType(v) == XVALUE_INT) return ValueIntOf(v);\n"
			"\tif (xrtValueType(v) == XVALUE_FLOAT) return (int64)ValueFloatOf(v);\n"
			"\tif (xrtValueType(v) == XVALUE_BOOL) return ValueBoolOf(v) ? 1 : 0;\n"
			"\tif (xrtValueType(v) == XVALUE_STRING) return atoll(ValueTextOf(v));\n"
			"\treturn 0;\n"
			"}\n\n"
			"static double Managed_BakedFieldFloatOf(xvalue* v)\n"
			"{\n"
			"\tif (!v) return 0.0;\n"
			"\tif (xrtValueType(v) == XVALUE_FLOAT) return ValueFloatOf(v);\n"
			"\tif (xrtValueType(v) == XVALUE_INT) return (double)ValueIntOf(v);\n"
			"\tif (xrtValueType(v) == XVALUE_STRING) return strtod(ValueTextOf(v), NULL);\n"
			"\treturn 0.0;\n"
			"}\n\n"
			"static int Managed_BakedFieldBoolOf(xvalue* v)\n"
			"{\n"
			"\tif (!v) return 0;\n"
			"\tif (xrtValueType(v) == XVALUE_BOOL) return ValueBoolOf(v) ? 1 : 0;\n"
			"\tif (xrtValueType(v) == XVALUE_INT) return ValueIntOf(v) != 0;\n"
			"\tif (xrtValueType(v) == XVALUE_FLOAT) return ValueFloatOf(v) != 0.0;\n"
			"\tif (xrtValueType(v) == XVALUE_STRING) { const char* s = ValueTextOf(v); return (strcmp(s, \"true\") == 0 || strcmp(s, \"1\") == 0) ? 1 : 0; }\n"
			"\treturn 0;\n"
			"}\n\n"
			"/* R1：模型字段类型化列绑定（columnar 字段按 storage 类型写入 f_<name> 列） */\n"
			"static int Managed_BindFieldColumns(sqlite3_stmt* stmt, int iStart, xvalue* tblData)\n"
			"{\n"
			"\tint idx = iStart;\n"
			"\tsize_t i;\n"
			"\tfor (i = 0; i < MANAGED_BAKED_FIELD_COUNT; i++) {\n"
			"\t\tconst Managed_BakedField* fld = &MANAGED_BAKED_FIELDS[i];\n"
			"\t\txvalue* v;\n"
			"\t\tif (!fld->columnar) continue;\n"
			"\t\tv = tblData ? ValueGet(tblData, fld->name) : NULL;\n"
			"\t\tif (strcmp(fld->storageType, \"integer\") == 0) {\n"
			"\t\t\tsqlite3_bind_int64(stmt, idx, (sqlite3_int64)Managed_BakedFieldIntOf(v));\n"
			"\t\t} else if (strcmp(fld->storageType, \"float\") == 0) {\n"
			"\t\t\tsqlite3_bind_double(stmt, idx, Managed_BakedFieldFloatOf(v));\n"
			"\t\t} else if (strcmp(fld->storageType, \"boolean\") == 0) {\n"
			"\t\t\tsqlite3_bind_int(stmt, idx, Managed_BakedFieldBoolOf(v));\n"
			"\t\t} else if (v && xrtValueType(v) == XVALUE_STRING) {\n"
			"\t\t\tsqlite3_bind_text(stmt, idx, ValueTextOf(v), -1, SQLITE_TRANSIENT);\n"
			"\t\t} else if (v && !Managed_ValueIsEmpty(v)) {\n"
			"\t\t\tstr s = Managed_ValueToTextDup(v);\n"
			"\t\t\tsqlite3_bind_text(stmt, idx, s ? s : (str)\"\", -1, SQLITE_TRANSIENT);\n"
			"\t\t\tif (s) xrtFree(s);\n"
			"\t\t} else {\n"
			"\t\t\tsqlite3_bind_null(stmt, idx);\n"
			"\t\t}\n"
			"\t\tidx++;\n"
			"\t}\n"
			"\treturn idx - iStart;\n"
			"}\n\n";
			xrtBufferAppend(tbl, (xbytesview){(cbytes)tail, strlen(tail)});
		}
	}

	/* ---- 四解释器体 + 三 getter 体 ---- */
	snprintf(body, sizeof(body),
		"\t(void)tblSpec;\n\t{\n\t\tsize_t i;\n\t\tfor (i = 0; i < MANAGED_BAKED_FIELD_COUNT; i++) Managed_ApplyMissingBakedFieldDefault(tblData, &MANAGED_BAKED_FIELDS[i]);\n\t}\n");
	t = Content_TemplateSet(t, "{{CONTENT_FIELD_DEFAULTS_BODY}}", body);
	snprintf(body, sizeof(body),
		"\t(void)tblSpec;\n\t{\n\t\tsize_t i;\n\t\tfor (i = 0; i < MANAGED_BAKED_FIELD_COUNT; i++) Managed_NormalizeNullableBakedField(tblData, &MANAGED_BAKED_FIELDS[i]);\n\t}\n");
	t = Content_TemplateSet(t, "{{CONTENT_FIELD_NULLABLE_BODY}}", body);
	snprintf(body, sizeof(body),
		"\t(void)tblSpec;\n\t{\n\t\tsize_t i;\n\t\tfor (i = 0; i < MANAGED_BAKED_FIELD_COUNT; i++) Managed_CoerceBakedFieldValue(tblData, &MANAGED_BAKED_FIELDS[i]);\n\t}\n");
	t = Content_TemplateSet(t, "{{CONTENT_FIELD_COERCE_BODY}}", body);
	{
		xbuffer* vb = xrtBufferCreate();
		const char* head =
			"\t(void)tblSpec;\n"
			"\tif ( psError ) *psError = NULL;\n"
			"\tif ( (tblData == NULL) || (xrtValueType(tblData) != XVALUE_OBJECT) ) {\n"
			"\t\tif ( psError ) *psError = xrtStrDup(\"data must be an object\");\n"
			"\t\treturn false;\n"
			"\t}\n"
			"\t{\n"
			"\t\tsize_t i;\n"
			"\t\tfor (i = 0; i < MANAGED_BAKED_FIELD_COUNT; i++) {\n"
			"\t\t\tconst Managed_BakedField* fld = &MANAGED_BAKED_FIELDS[i];\n"
			"\t\t\txvalue* objValue = ValueGet(tblData, fld->name);\n"
			"\t\t\tif ( !Managed_ValueIsEmpty(objValue) ) {\n"
			"\t\t\t\tif ( !Managed_ValueMatchesStorageType(objValue, fld->storageType) ) {\n"
			"\t\t\t\t\tif ( psError ) *psError = xrtFormat(\"%s must match storage.type = %s\", (fld->title[0] ? fld->title : fld->name), fld->storageType);\n"
			"\t\t\t\t\treturn false;\n"
			"\t\t\t\t}\n"
			"\t\t\t\tcontinue;\n"
			"\t\t\t}\n"
			"\t\t\tif ( bSkipRequired ) continue;\n"
			"\t\t\tif ( !fld->required ) continue;\n"
			"\t\t\tif ( psError ) *psError = xrtFormat(\"%s is required\", (fld->title[0] ? fld->title : fld->name));\n"
			"\t\t\t\treturn false;\n"
			"\t\t}\n"
			"\t}\n"
			"\treturn true;\n";
		if (vb) {
			xrtBufferAppend(vb, (xbytesview){(cbytes)head, strlen(head)});
			{
				str sBody = Content_BakeBufferToString(vb);
				t = Content_TemplateSet(t, "{{CONTENT_FIELD_VALIDATE_BODY}}", sBody ? sBody : "");
				xrtFree(sBody);
			}
		}
	}
	if (sRoleTitle) snprintf(body, sizeof(body), "\t(void)tblSpec;\n\treturn \"%s\";", sRoleTitle);
	else snprintf(body, sizeof(body), "\t(void)tblSpec;\n\treturn NULL;");
	t = Content_TemplateSet(t, "{{CONTENT_TITLE_FIELD_BODY}}", body);
	if (sRoleStatus) snprintf(body, sizeof(body), "\t(void)tblSpec;\n\treturn \"%s\";", sRoleStatus);
	else snprintf(body, sizeof(body), "\t(void)tblSpec;\n\treturn NULL;");
	t = Content_TemplateSet(t, "{{CONTENT_STATUS_FIELD_BODY}}", body);
	if (sRoleSlug) snprintf(body, sizeof(body), "\t(void)tblSpec;\n\treturn \"%s\";", sRoleSlug);
	else snprintf(body, sizeof(body), "\t(void)tblSpec;\n\treturn NULL;");
	t = Content_TemplateSet(t, "{{CONTENT_SLUG_FIELD_BODY}}", body);

	/* 字段表并入 BAKED_CONFIG_HELPERS：重取旧值拼接 */
	{
		const char* ph = "{{CONTENT_BAKED_CONFIG_HELPERS}}";
		const char* pHit = strstr(t, ph);
		if (pHit) {
			xbuffer* merged = xrtBufferCreate();
			if (merged) {
				xrtBufferAppend(merged, (xbytesview){(cbytes)t, (size_t)(pHit - t)});
				xrtBufferAppend(merged, (xbytesview){(cbytes)tbl->Data, tbl->Size});
				xrtBufferAppend(merged, (xbytesview){(cbytes)pHit, strlen(ph)});
				xrtBufferAppend(merged, (xbytesview){(cbytes)pHit + strlen(ph), strlen(pHit + strlen(ph))});
				xrtFree(t);
				t = Content_BakeBufferToString(merged);
			}
		}
	}

	xrtFree(sRoleTitle);
	xrtFree(sRoleStatus);
	xrtFree(sRoleSlug);
	xrtBufferDestroy(code);
	xrtBufferDestroy(tbl);
	*pTemplate = t;
}

/* R1：模型字段 → 类型化列（f_<name>）。DDL 探测式幂等 ALTER + json_extract
 * 回填；写入路径经 Managed_BindFieldColumns；读取仍走 payload_json（双写）。 */
static const char* Content_R1ColumnType(const char* sStorage)
{
	if (!sStorage) return "TEXT";
	if (strcmp(sStorage, "integer") == 0) return "INTEGER";
	if (strcmp(sStorage, "boolean") == 0) return "INTEGER";
	if (strcmp(sStorage, "float") == 0) return "REAL";
	return "TEXT";
}

static void Content_ApplyR1Baking(char** pTemplate, xvalue* spec)
{
	char* t = *pTemplate;
	xvalue* fields = spec ? ValueGet(spec, "fields") : NULL;
	xbuffer* ddl = xrtBufferCreate();
	xbuffer* sets = xrtBufferCreate();
	xbuffer* cols = xrtBufferCreate();
	xbuffer* vals = xrtBufferCreate();
	str sRoleTitle = Content_R2ResolveRoleField(spec, "title", "title");
	str sRoleStatus = Content_R2ResolveRoleField(spec, "status", "status");
	uint32 i;
	if (!ddl || !sets || !cols || !vals) {
		if (ddl) xrtBufferDestroy(ddl);
		if (sets) xrtBufferDestroy(sets);
		if (cols) xrtBufferDestroy(cols);
		if (vals) xrtBufferDestroy(vals);
		xrtFree(sRoleTitle);
		xrtFree(sRoleStatus);
		return;
	}
	if (fields && xrtValueType(fields) == XVALUE_ARRAY) {
		for (i = 0; i < ValueCount(fields); i++) {
			xvalue* f = xrtValueArrayGet(fields, i);
			str name = f ? ValueText(f, "name") : NULL;
			xvalue* storage = f ? ValueGet(f, "storage") : NULL;
			str st = storage ? ValueText(storage, "type") : (f ? ValueText(f, "type") : NULL);
			str sNorm;
			char col[160];
			char block[640];
			if (!name || !name[0]) continue;
			if (sRoleTitle && strcmp(sRoleTitle, name) == 0) continue;
			if (sRoleStatus && strcmp(sRoleStatus, name) == 0) continue;
			sNorm = Content_R2NormalizeStorage(st);
			snprintf(col, sizeof(col), "f_%s", name);
			snprintf(block, sizeof(block),
				"\tif ( bOK && !Managed_TableColumnExists(pDb, \"content_item\", \"%s\") ) {\n"
				"\t\tbOK = Managed_ExecSql(pDb, \"ALTER TABLE content_item ADD COLUMN %s %s\");\n"
				"\t\tif ( bOK ) bOK = Managed_ExecSql(pDb, \"UPDATE content_item SET %s = json_extract(payload_json, '$.%s') WHERE %s IS NULL AND json_extract(payload_json, '$.%s') IS NOT NULL\");\n"
				"\t}\n",
				col, col, Content_R1ColumnType(sNorm), col, name, col, name);
			xrtBufferAppend(ddl, (xbytesview){(cbytes)block, strlen(block)});
			{
				char seg[160];
				snprintf(seg, sizeof(seg), ",%s=?", col);
				xrtBufferAppend(sets, (xbytesview){(cbytes)seg, strlen(seg)});
				snprintf(seg, sizeof(seg), ",%s", col);
				xrtBufferAppend(cols, (xbytesview){(cbytes)seg, strlen(seg)});
				snprintf(seg, sizeof(seg), ",?");
				xrtBufferAppend(vals, (xbytesview){(cbytes)seg, 2});
			}
			xrtFree(sNorm);
		}
	}
	{
		str sDdl = Content_BakeBufferToString(ddl);
		str sSets = Content_BakeBufferToString(sets);
		str sCols = Content_BakeBufferToString(cols);
		str sVals = Content_BakeBufferToString(vals);
		t = Content_TemplateSet(t, "{{CONTENT_FIELD_COLUMN_DDL}}", sDdl ? sDdl : "");
		t = Content_TemplateSet(t, "{{CONTENT_FIELD_UPDATE_SETS}}", sSets ? sSets : "");
		t = Content_TemplateSet(t, "{{CONTENT_FIELD_INSERT_COLS}}", sCols ? sCols : "");
		t = Content_TemplateSet(t, "{{CONTENT_FIELD_INSERT_VALS}}", sVals ? sVals : "");
		xrtFree(sDdl);
		xrtFree(sSets);
		xrtFree(sCols);
		xrtFree(sVals);
	}
	xrtFree(sRoleTitle);
	xrtFree(sRoleStatus);
	xrtBufferDestroy(ddl);
	xrtBufferDestroy(sets);
	xrtBufferDestroy(cols);
	xrtBufferDestroy(vals);
	*pTemplate = t;
}

static char* Content_BuildManagedMainC(const char* pluginXid, const char* pluginTitle, const char* menuTitle, xvalue* spec)
{
	char* template = Content_LoadGeneratorTemplate("managed_main.c.tpl");
	char* safePluginTitle = Content_EscapeCString(Content_TextOr(pluginTitle, pluginXid ? pluginXid : "Generated Content Plugin"));
	char* safeMenuTitle = Content_EscapeCString(Content_TextOr(menuTitle, pluginTitle ? pluginTitle : (pluginXid ? pluginXid : "Generated Content Plugin")));
	char* packRoutes = NULL;
	char* packMenus = NULL;
	char* packSchemaSql = NULL;
	char* packAuth = NULL;
	if (!template) {
		xrtFree(safePluginTitle);
		xrtFree(safeMenuTitle);
		return xrtStrDup("");
	}
	template = Content_TemplateSet(template, "{{PLUGIN_XID}}", pluginXid ? pluginXid : "");
	template = Content_TemplateSet(template, "{{PLUGIN_TITLE_C}}", Content_TextOr(safePluginTitle, ""));
	template = Content_TemplateSet(template, "{{PLUGIN_VERSION}}", "1.0.0");
	packRoutes = Content_BuildAbilityPackRouteCode(pluginXid, spec);
	packMenus = Content_BuildAbilityPackMenuCode(pluginXid, spec);
	packSchemaSql = Content_BuildAbilityPackSchemaSql(spec);
	packAuth = Content_BuildAbilityPackAuthCode(pluginXid, pluginTitle, spec);
	template = Content_TemplateSet(template, "{{ABILITY_PACK_ROUTE_REGISTRATIONS}}", Content_TextOr(packRoutes, ""));
	template = Content_TemplateSet(template, "{{ABILITY_PACK_MENU_REGISTRATIONS}}", Content_TextOr(packMenus, ""));
	template = Content_TemplateSet(template, "{{ABILITY_PACK_SCHEMA_SQL}}", Content_TextOr(packSchemaSql, ""));
	template = Content_TemplateSet(template, "{{ABILITY_PACK_AUTH_REGISTRATIONS}}", Content_TextOr(packAuth, ""));
	/* 策略/页面 tab 烘焙：softDelete/auditTime/createRole/manageRole、detailFields 投影——
	 * 生成期决策为常量与专属代码，运行时零动态判断 */
	{
		xvalue* policies = ValueGet(spec, "policies");
		xvalue* pagesObj = ValueGet(spec, "pages");
		bool bHasPolicies = policies && xrtValueType(policies) == XVALUE_OBJECT;
		bool bSoftDelete = !bHasPolicies || ValueBool(policies, "softDelete");
		bool bAuditTime = !bHasPolicies || ValueBool(policies, "auditTime");
		str createRole = bHasPolicies ? ValueText(policies, "createRole") : NULL;
		str manageRole = bHasPolicies ? ValueText(policies, "manageRole") : NULL;
		int64 createLevel = Content_ResolveRoleAuthLevel(createRole);
		int64 manageLevel = Content_ResolveRoleAuthLevel(manageRole);
		str detailFields = (pagesObj && xrtValueType(pagesObj) == XVALUE_OBJECT) ? ValueText(pagesObj, "detailFields") : NULL;
		char sDefines[512];
		char* sDeleteExec = NULL;
		char* sSaveGuards = NULL;
		char* sFilterFn = xrtStrDup("static void Managed_ApplyDetailFieldFilter(xvalue* tblItem)\n{\n\t(void)tblItem;\n}\n");
		snprintf(sDefines, sizeof(sDefines),
			"#define MANAGED_CONTENT_TIME(now) (%s)\n#define MANAGED_CREATE_AUTH_LEVEL %lld\n#define MANAGED_MANAGE_AUTH_LEVEL %lld",
			bAuditTime ? "now" : "0", (long long)createLevel, (long long)manageLevel);
		if ( bSoftDelete ) {
			sDeleteExec = xrtFormat(
				"{\n"
				"	if ( MANAGED_MANAGE_AUTH_LEVEL > 0 ) {\n"
				"		int64 iSessionLevel = 0;\n"
				"		if ( objSession && (xrtValueType(objSession) == XVALUE_OBJECT) ) {\n"
				"			iSessionLevel = ValueInt(objSession, \"authLevel\");\n"
				"			if ( iSessionLevel <= 0 ) iSessionLevel = ValueInt(objSession, \"__authLevel__\");\n"
				"		}\n"
				"		if ( iSessionLevel < MANAGED_MANAGE_AUTH_LEVEL ) {\n"
				"			Managed_CloseDb(pDb);\n"
				"			xrtValueRelease(tblSpec);\n"
				"			xrtValueRelease(tblForm);\n"
				"			Managed_SendError(objResp, \"delete denied by model policy\");\n"
				"			return;\n"
				"		}\n"
				"	}\n"
				"	if ( sqlite3_prepare_v2(pDb, \"UPDATE content_item SET delete_time = ?, update_time = ? WHERE id = ? AND delete_time = 0\", -1, &stmt, NULL) == SQLITE_OK ) {\n"
				"		xtime iNow = xrtNow();\n"
				"		sqlite3_bind_int64(stmt, 1, iNow);\n"
				"		sqlite3_bind_int64(stmt, 2, MANAGED_CONTENT_TIME(iNow));\n"
				"		sqlite3_bind_int64(stmt, 3, (sqlite3_int64)iId);\n"
				"		if ( sqlite3_step(stmt) == SQLITE_DONE ) {\n"
				"			bDeleted = sqlite3_changes(pDb) > 0 ? true : false;\n"
				"		}\n"
				"	}\n"
				"}");
		} else {
			sDeleteExec = xrtFormat(
				"{\n"
				"	if ( MANAGED_MANAGE_AUTH_LEVEL > 0 ) {\n"
				"		int64 iSessionLevel = 0;\n"
				"		if ( objSession && (xrtValueType(objSession) == XVALUE_OBJECT) ) {\n"
				"			iSessionLevel = ValueInt(objSession, \"authLevel\");\n"
				"			if ( iSessionLevel <= 0 ) iSessionLevel = ValueInt(objSession, \"__authLevel__\");\n"
				"		}\n"
				"		if ( iSessionLevel < MANAGED_MANAGE_AUTH_LEVEL ) {\n"
				"			Managed_CloseDb(pDb);\n"
				"			xrtValueRelease(tblSpec);\n"
				"			xrtValueRelease(tblForm);\n"
				"			Managed_SendError(objResp, \"delete denied by model policy\");\n"
				"			return;\n"
				"		}\n"
				"	}\n"
				"	if ( sqlite3_prepare_v2(pDb, \"DELETE FROM content_item WHERE id = ?\", -1, &stmt, NULL) == SQLITE_OK ) {\n"
				"		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iId);\n"
				"		if ( sqlite3_step(stmt) == SQLITE_DONE ) {\n"
				"			bDeleted = sqlite3_changes(pDb) > 0 ? true : false;\n"
				"		}\n"
				"	}\n"
				"}");
		}
		sSaveGuards = xrtFormat(
			"{\n"
			"	int64 iSessionLevel = 0;\n"
			"	if ( objSession && (xrtValueType(objSession) == XVALUE_OBJECT) ) {\n"
			"		iSessionLevel = ValueInt(objSession, \"authLevel\");\n"
			"		if ( iSessionLevel <= 0 ) iSessionLevel = ValueInt(objSession, \"__authLevel__\");\n"
			"	}\n"
			"	if ( bInsert && (MANAGED_CREATE_AUTH_LEVEL > 0) && (iSessionLevel < MANAGED_CREATE_AUTH_LEVEL) ) {\n"
			"		xrtValueRelease(tblSpec);\n"
			"		xrtValueRelease(tblForm);\n"
			"		Managed_SendError(objResp, \"create denied by model policy\");\n"
			"		return;\n"
			"	}\n"
			"	if ( !bInsert && (MANAGED_MANAGE_AUTH_LEVEL > 0) && (iSessionLevel < MANAGED_MANAGE_AUTH_LEVEL) ) {\n"
			"		xrtValueRelease(tblSpec);\n"
			"		xrtValueRelease(tblForm);\n"
			"		Managed_SendError(objResp, \"update denied by model policy\");\n"
			"		return;\n"
			"	}\n"
			"}");
		if ( detailFields && detailFields[0] ) {
			/* detailFields 投影：允许清单烘焙为静态数组（覆盖默认空实现） */
			xvalue* flds = Content_ParseNameList(detailFields);
			char* fn = NULL;
			uint32 fi;
			xrtFree(sFilterFn);
			fn = xrtStrDup("static void Managed_ApplyDetailFieldFilter(xvalue* tblItem)\n{\n\tstatic const char* sAllowed[] = {");
			for ( fi = 0; fi < ValueCount(flds); fi++ ) {
				char* next = xrtFormat("%s \"%s\",", Content_TextOr(fn, ""), Content_TextOr(ValueArrayText(flds, fi), ""));
				xrtFree(fn);
				fn = next;
			}
			if ( fn ) {
				char* next = xrtFormat("%s NULL };\n\txvalue* tblData = tblItem ? ValueGet(tblItem, \"data\") : NULL;\n\txvalue* tblNew;\n\tuint32 ai;\n\tif ( !tblData || xrtValueType(tblData) != XVALUE_OBJECT ) return;\n\ttblNew = ValueObject();\n\tfor ( ai = 0; sAllowed[ai]; ai++ ) {\n\t\txvalue* v = ValueGet(tblData, sAllowed[ai]);\n\t\tif ( v ) ValueSetRef(tblNew, sAllowed[ai], v);\n\t}\n\tValueSetOwn(tblItem, \"data\", tblNew);\n}\n", Content_TextOr(fn, ""));
				xrtFree(fn);
				fn = next;
			}
			sFilterFn = fn;
			xrtValueRelease(flds);
		}
		template = Content_TemplateSet(template, "{{CONTENT_POLICY_DEFINES}}", sDefines);
		template = Content_TemplateSet(template, "{{CONTENT_DELETE_EXEC}}", sDeleteExec ? sDeleteExec : "");
		template = Content_TemplateSet(template, "{{CONTENT_SAVE_GUARDS}}", sSaveGuards ? sSaveGuards : "");
		template = Content_TemplateSet(template, "{{CONTENT_DETAIL_FILTER_FN}}", sFilterFn ? sFilterFn : "");
		xrtFree(sDeleteExec);
		xrtFree(sSaveGuards);
		xrtFree(sFilterFn);
	}
	Content_ApplyR2Baking(&template, spec);
	Content_ApplyR1Baking(&template, spec);
	Content_ApplyR3R4Baking(&template, spec);
	/* 菜单标题覆盖：pluginTitle 与 menuTitle 不同时改写根菜单标题行（v1 同） */
	if (safeMenuTitle && pluginTitle && menuTitle && strcmp(pluginTitle, menuTitle) != 0) {
		char* needle = xrtFormat("menu.title = \"%s\";", Content_TextOr(safePluginTitle, ""));
		char* value = xrtFormat("menu.title = \"%s\";", Content_TextOr(safeMenuTitle, ""));
		if (needle && value) template = Content_TemplateSet(template, needle, value);
		xrtFree(needle);
		xrtFree(value);
	}
	xrtFree(safePluginTitle);
	xrtFree(safeMenuTitle);
	xrtFree(packRoutes);
	xrtFree(packMenus);
	xrtFree(packSchemaSql);
	xrtFree(packAuth);
	return template;
}

/* ==================== 页面 HTML ==================== */

/* DOM 前缀 + xid 清洗（非字母数字 → _），保证 JS 标识符合法（v1 同） */
static char* Content_BuildPluginDomIdBase(const char* pluginXid)
{
	const char* base = pluginXid ? pluginXid : "";
	static const char* prefix = "Content_MakePlugin_";
	size_t baseLen = strlen(base);
	char* out = (char*)xrtMalloc(strlen(prefix) + baseLen + 1);
	size_t pos = 0;
	size_t i;
	if (!out) return NULL;
	memcpy(out, prefix, strlen(prefix));
	pos = strlen(prefix);
	for (i = 0; i < baseLen; i++) {
		char ch = base[i];
		if (((ch >= 'a') && (ch <= 'z')) || ((ch >= 'A') && (ch <= 'Z')) || ((ch >= '0') && (ch <= '9')))
			out[pos++] = ch;
		else
			out[pos++] = '_';
	}
	out[pos] = '\0';
	return out;
}

static char* Content_BuildManagedAdminPageHtml(const char* pluginXid, const char* pageKind, xvalue* spec)
{
	char* template = Content_LoadGeneratorTemplate("managed_admin.html.tpl");
	char* domIdBase = Content_BuildPluginDomIdBase(pluginXid);
	char* pageDomIdBase = NULL;
	if (!template) {
		xrtFree(domIdBase);
		return NULL;
	}
	pageDomIdBase = xrtFormat("%s_%s", domIdBase ? domIdBase : "Content_MakePlugin", (pageKind && !strcmp(pageKind, "drafts")) ? "Drafts" : "Articles");
	template = Content_TemplateSet(template, "{{PLUGIN_XID}}", pluginXid ? pluginXid : "");
	template = Content_TemplateSet(template, "{{PLUGIN_DOM_ID_BASE}}", pageDomIdBase ? pageDomIdBase : (domIdBase ? domIdBase : "Content_MakePlugin"));
	template = Content_TemplateSet(template, "{{PLUGIN_PAGE_KIND}}", pageKind ? pageKind : "articles");
	/* 页面 tab 列表列：烘焙列配置（含字段标题），空数组=默认三列 */
	{
		xvalue* pagesObj = spec ? ValueGet(spec, "pages") : NULL;
		str listColumns = (pagesObj && xrtValueType(pagesObj) == XVALUE_OBJECT) ? ValueText(pagesObj, "listColumns") : NULL;
		char* sColsJson = NULL;
		if ( listColumns && listColumns[0] ) {
			xvalue* cols = Content_ParseNameList(listColumns);
			xvalue* arr = ValueArray();
			xvalue* fields = spec ? ValueGet(spec, "fields") : NULL;
			uint32 ci;
			for ( ci = 0; ci < ValueCount(cols); ci++ ) {
				str name = ValueArrayText(cols, ci);
				xvalue* one = ValueObject();
				str title = NULL;
				uint32 fi;
				for ( fi = 0; fields && fi < ValueCount(fields); fi++ ) {
					xvalue* f = xrtValueArrayGet(fields, fi);
					str fname = f ? ValueText(f, "name") : NULL;
					if ( fname && name && strcmp(fname, name) == 0 ) { title = ValueText(f, "title"); break; }
				}
				ValueSetText(one, "field", name ? name : (str)"");
				ValueSetText(one, "title", Content_TextOr(title, name ? name : ""));
				ValueArrayOwn(arr, one);
			}
			sColsJson = Content_StringifyJson(arr, false);
			xrtValueRelease(arr);
			xrtValueRelease(cols);
		}
		template = Content_TemplateSet(template, "{{CONTENT_LIST_COLUMNS_JS}}", sColsJson ? sColsJson : "[]");
		xrtFree(sColsJson);
	}
	xrtFree(pageDomIdBase);
	xrtFree(domIdBase);
	return template;
}

static char* Content_BuildManagedEditorHtml(const char* pluginXid)
{
	char* template = Content_LoadGeneratorTemplate("managed_editor.html.tpl");
	char* domIdBase = Content_BuildPluginDomIdBase(pluginXid);
	char* pageDomIdBase = NULL;
	if (!template) {
		xrtFree(domIdBase);
		return NULL;
	}
	pageDomIdBase = xrtFormat("%s_Editor", domIdBase ? domIdBase : "Content_MakePlugin");
	template = Content_TemplateSet(template, "{{PLUGIN_XID}}", pluginXid ? pluginXid : "");
	template = Content_TemplateSet(template, "{{PLUGIN_DOM_ID_BASE}}", pageDomIdBase ? pageDomIdBase : (domIdBase ? domIdBase : "Content_MakePlugin_Editor"));
	template = Content_TemplateSet(template, "{{PLUGIN_LIST_DOM_ID_BASE}}", domIdBase ? domIdBase : "Content_MakePlugin");
	xrtFree(pageDomIdBase);
	xrtFree(domIdBase);
	return template;
}

static char* Content_BuildManagedCategoryHtml(const char* pluginXid)
{
	char* template = Content_LoadGeneratorTemplate("managed_category.html.tpl");
	char* domIdBase = Content_BuildPluginDomIdBase(pluginXid);
	char* pageDomIdBase = NULL;
	if (!template) {
		xrtFree(domIdBase);
		return NULL;
	}
	pageDomIdBase = xrtFormat("%s_Categories", domIdBase ? domIdBase : "Content_MakePlugin");
	template = Content_TemplateSet(template, "{{PLUGIN_XID}}", pluginXid ? pluginXid : "");
	template = Content_TemplateSet(template, "{{PLUGIN_DOM_ID_BASE}}", pageDomIdBase ? pageDomIdBase : (domIdBase ? domIdBase : "Content_MakePlugin_Category"));
	xrtFree(pageDomIdBase);
	xrtFree(domIdBase);
	return template;
}

static char* Content_BuildManagedPublicHtml(const char* pluginXid)
{
	char* template = Content_LoadGeneratorTemplate("managed_public.html.tpl");
	char* domIdBase = Content_BuildPluginDomIdBase(pluginXid);
	if (!template) {
		xrtFree(domIdBase);
		return NULL;
	}
	template = Content_TemplateSet(template, "{{PLUGIN_XID}}", pluginXid ? pluginXid : "");
	template = Content_TemplateSet(template, "{{PLUGIN_DOM_ID_BASE}}", domIdBase ? domIdBase : "Content_MakePlugin");
	template = Content_TemplateSet(template, "{{PLUGIN_PAGE_KIND}}", "public");
	xrtFree(domIdBase);
	return template;
}

static char* Content_BuildManagedAbilityHtml(const char* pluginXid)
{
	char* template = Content_LoadGeneratorTemplate("managed_ability.html.tpl");
	if (!template) return xrtStrDup("<div style=\"padding:16px;\">Ability pack page missing.</div>");
	/* @@ 系运行时占位（@@ABILITY_PAGE_KEY@@ 由插件启动后替换）；生成期替换 xid 双写法 */
	template = Content_TemplateSet(template, "@@PLUGIN_XID@@", pluginXid ? pluginXid : "");
	template = Content_TemplateSet(template, "{{PLUGIN_XID}}", pluginXid ? pluginXid : "");
	return template;
}

static char* Content_BuildManagedDashboardHtml(const char* pluginXid)
{
	char* template = Content_LoadGeneratorTemplate("managed_dashboard.html.tpl");
	char* domIdBase = Content_BuildPluginDomIdBase(pluginXid);
	char* pageDomIdBase = NULL;
	if (!template) {
		xrtFree(domIdBase);
		return xrtStrDup("<div style=\"padding:16px;\">Dashboard page missing.</div>");
	}
	pageDomIdBase = xrtFormat("%s_Dashboard", domIdBase ? domIdBase : "Content_MakePlugin");
	template = Content_TemplateSet(template, "{{PLUGIN_XID}}", pluginXid ? pluginXid : "");
	template = Content_TemplateSet(template, "{{PLUGIN_DOM_ID_BASE}}", pageDomIdBase ? pageDomIdBase : (domIdBase ? domIdBase : "Content_MakePlugin_Dashboard"));
	xrtFree(pageDomIdBase);
	xrtFree(domIdBase);
	return template;
}

static char* Content_BuildManagedTasksHtml(const char* pluginXid)
{
	char* template = Content_LoadGeneratorTemplate("managed_tasks.html.tpl");
	char* domIdBase = Content_BuildPluginDomIdBase(pluginXid);
	char* pageDomIdBase = NULL;
	if (!template) {
		xrtFree(domIdBase);
		return xrtStrDup("<div style=\"padding:16px;\">Task dashboard page missing.</div>");
	}
	pageDomIdBase = xrtFormat("%s_Tasks", domIdBase ? domIdBase : "Content_MakePlugin");
	template = Content_TemplateSet(template, "{{PLUGIN_XID}}", pluginXid ? pluginXid : "");
	template = Content_TemplateSet(template, "{{PLUGIN_DOM_ID_BASE}}", pageDomIdBase ? pageDomIdBase : (domIdBase ? domIdBase : "Content_MakePlugin_Tasks"));
	xrtFree(pageDomIdBase);
	xrtFree(domIdBase);
	return template;
}

static char* Content_BuildManagedStaticDetailHtml(void)
{
	char* template = Content_LoadGeneratorTemplate("managed_static_detail.html.tpl");
	if (!template)
		return xrtStrDup("<!doctype html><html><body><h1>{{title}}</h1><article>{{content_html}}</article></body></html>");
	return template;
}

/* ==================== spec 三视图规范化（generated/spec.json） ==================== */

static const char* Content_FieldStorageType(const char* type)
{
	if (!type) return "text";
	if (!strcmp(type, "integer") || !strcmp(type, "int")) return "integer";
	if (!strcmp(type, "number") || !strcmp(type, "float") || !strcmp(type, "double")) return "real";
	if (!strcmp(type, "bool") || !strcmp(type, "boolean") || !strcmp(type, "switch")) return "integer";
	return "text";
}

static const char* Content_FieldComponentType(const char* type, const char* options)
{
	if (type && !strcmp(type, "textarea")) return "textarea";
	if (type && !strcmp(type, "text")) return "text";
	if (type && !strcmp(type, "number")) return "number";
	if (type && !strcmp(type, "int")) return "int";
	if (type && !strcmp(type, "integer")) return "int";
	if (type && !strcmp(type, "decimal")) return "number";
	if (type && !strcmp(type, "password")) return "password";
	if (type && !strcmp(type, "select")) return "select";
	if (type && !strcmp(type, "combobox")) return "combobox";
	if (type && !strcmp(type, "radio")) return "radio";
	if (type && !strcmp(type, "checkbox")) return "checkbox";
	if (type && !strcmp(type, "checklist")) return "checklist";
	if (type && !strcmp(type, "date")) return "date";
	if (type && !strcmp(type, "datetime")) return "datetime";
	if (type && !strcmp(type, "time")) return "time";
	if (type && !strcmp(type, "intrange")) return "intrange";
	if (type && !strcmp(type, "numrange")) return "numrange";
	if (type && !strcmp(type, "daterange")) return "daterange";
	if (type && !strcmp(type, "timerange")) return "timerange";
	if (type && !strcmp(type, "datetimerange")) return "datetimerange";
	if (type && !strcmp(type, "editor_md")) return "editor_md";
	if (type && !strcmp(type, "editor_html")) return "editor_html";
	if (type && !strcmp(type, "editor_code")) return "editor_code";
	if (type && !strcmp(type, "icon_picker")) return "icon_picker";
	if (type && !strcmp(type, "image")) return "image";
	if (type && !strcmp(type, "images")) return "images";
	if (type && !strcmp(type, "file")) return "file";
	if (type && !strcmp(type, "files")) return "files";
	if (type && !strcmp(type, "badge_picker")) return "badge_picker";
	if (options && options[0]) return "select";
	if (type && (!strcmp(type, "bool") || !strcmp(type, "boolean") || !strcmp(type, "switch"))) return "switch";
	return "input";
}

static const char* Content_FieldSemanticRole(const char* name)
{
	if (!name) return "";
	if (!strcmp(name, "title")) return "title";
	if (!strcmp(name, "status")) return "status";
	if (!strcmp(name, "slug")) return "slug";
	if (!strcmp(name, "summary")) return "summary";
	if (!strcmp(name, "cover")) return "cover";
	if (!strcmp(name, "publishedAt") || !strcmp(name, "published_at")) return "publishedAt";
	return "";
}

static bool Content_TableBoolDefault(xvalue* tbl, const char* key, bool defaultValue)
{
	xvalue* value = ValueGet(tbl, key);
	return value ? ValueBoolOf(value) : defaultValue;
}

/* "a,b:c,d" → [{value,label}]（v1 同：逗号分段，冒号前 value 后 label，首部空白裁剪） */
static xvalue* Content_ParseOptionList(const char* options)
{
	xvalue* list = ValueArray();
	char* copy = NULL;
	char* p = NULL;
	if (!list) return NULL;
	if (!options || !options[0]) return list;
	copy = xrtStrDup(options);
	if (!copy) return list;
	p = copy;
	while (p && *p) {
		char* next = strchr(p, ',');
		char* sep = NULL;
		char* value = NULL;
		char* label = NULL;
		if (next) {
			*next = '\0';
			next++;
		}
		while (*p == ' ' || *p == '\t') p++;
		sep = strchr(p, ':');
		if (sep) {
			*sep = '\0';
			label = sep + 1;
		} else {
			label = p;
		}
		value = p;
		while (value[0] == ' ' || value[0] == '\t') value++;
		while (label[0] == ' ' || label[0] == '\t') label++;
		if (value[0]) {
			xvalue* item = ValueObject();
			ValueSetText(item, "value", value);
			ValueSetText(item, "label", label[0] ? label : value);
			ValueArrayOwn(list, item);
		}
		p = next;
	}
	xrtFree(copy);
	return list;
}

static xvalue* Content_BuildManagedSpecField(xvalue* field)
{
	str name = ValueText(field, "name");
	str type = ValueText(field, "type");
	str options = ValueText(field, "options");
	const char* role = Content_FieldSemanticRole(name);
	xvalue* existingStorage = ValueGet(field, "storage");
	xvalue* existingComponent = ValueGet(field, "component");
	xvalue* existingSemantic = ValueGet(field, "semantic");
	xvalue* list = Content_ParseOptionList(options);
	xvalue* out = xrtValueDeepClone(field);
	if (!out) out = ValueObject();
	if (existingStorage && xrtValueType(existingStorage) == XVALUE_OBJECT) {
		ValueSetOwn(out, "storage", xrtValueDeepClone(existingStorage));
	} else {
		xvalue* storage = ValueObject();
		ValueSetText(storage, "type", Content_FieldStorageType(type));
		ValueSetOwn(out, "storage", storage);
	}
	if (existingComponent && xrtValueType(existingComponent) == XVALUE_OBJECT) {
		ValueSetOwn(out, "component", xrtValueDeepClone(existingComponent));
	} else {
		xvalue* component = ValueObject();
		ValueSetText(component, "type", Content_FieldComponentType(type, options));
		if (list && ValueCount(list) > 0)
			ValueSetOwn(component, "list", xrtValueDeepClone(list));
		ValueSetOwn(out, "component", component);
	}
	if (existingSemantic && xrtValueType(existingSemantic) == XVALUE_OBJECT) {
		ValueSetOwn(out, "semantic", xrtValueDeepClone(existingSemantic));
	} else if (role[0]) {
		xvalue* semantic = ValueObject();
		ValueSetText(semantic, "role", role);
		ValueSetOwn(out, "semantic", semantic);
	}
	if (list && ValueCount(list) > 0 && !ValueGet(out, "options")) {
		ValueSetOwn(out, "list", list);
		list = NULL;
	}
	xrtValueRelease(list);
	if (!ValueGet(out, "showInForm")) ValueSetBool(out, "showInForm", true);
	if (!ValueGet(out, "showInList")) ValueSetBool(out, "showInList", Content_TableBoolDefault(field, "list", true));
	if (!ValueGet(out, "showInDetail")) ValueSetBool(out, "showInDetail", Content_TableBoolDefault(field, "detail", true));
	return out;
}

static xvalue* Content_CollectEnabledCapabilities(xvalue* spec)
{
	xvalue* capabilities = ValueGet(spec, "capabilities");
	xvalue* enabled = ValueArray();
	if (capabilities && xrtValueType(capabilities) == XVALUE_ARRAY) {
		uint32 i;
		for (i = 0; i < ValueCount(capabilities); i++) {
			xvalue* cap = xrtValueArrayGet(capabilities, i);
			if (!cap || xrtValueType(cap) != XVALUE_OBJECT) continue;
			if (!Content_CapabilityEnabled(cap)) continue;
			ValueArrayOwn(enabled, xrtValueDeepClone(cap));
		}
	}
	return enabled;
}

static char* Content_BuildManagedSpecJson(xvalue* spec, const char* modelXid, const char* title, const char* description)
{
	xvalue* root = ValueObject();
	xvalue* identity = ValueObject();
	xvalue* entity = ValueObject();
	xvalue* fields = ValueGet(spec, "fields");
	xvalue* managedFields = ValueArray();
	xvalue* core = ValueObject();
	xvalue* draft = ValueObject();
	xvalue* ui = ValueObject();
	xvalue* uiList = ValueObject();
	xvalue* uiForm = ValueObject();
	xvalue* policies = ValueGet(spec, "policies");
	xvalue* pages = ValueGet(spec, "pages");
	str name = ValueText(spec, "name");
	str ns = ValueText(spec, "namespace");
	str tableName = ValueText(spec, "tableName");
	char* json;
	if (fields && xrtValueType(fields) == XVALUE_ARRAY) {
		uint32 i;
		for (i = 0; i < ValueCount(fields); i++) {
			xvalue* field = xrtValueArrayGet(fields, i);
			if (field && xrtValueType(field) == XVALUE_OBJECT)
				ValueArrayOwn(managedFields, Content_BuildManagedSpecField(field));
		}
	}
	/* fieldGroups（数组 {title,fields}）落到字段 group 键——编辑器表单分组生效；
	 * key 由标题清洗派生，presentation.groups 同源构建 */
	{
		xvalue* fgArr = pages ? ValueGet(pages, "fieldGroups") : NULL;
		if (fgArr && xrtValueType(fgArr) == XVALUE_ARRAY) {
			uint32 g, f, m;
			for (g = 0; g < ValueCount(fgArr); g++) {
				xvalue* grp = xrtValueArrayGet(fgArr, g);
				xvalue* names = grp ? ValueGet(grp, "fields") : NULL;
				str title = grp ? ValueText(grp, "title") : NULL;
				char sKey[80];
				size_t w = 0;
				const char* q = (title && title[0]) ? title : "group";
				while (*q && w + 1 < sizeof(sKey)) {
					char ch = *q++;
					bool bOk = ((ch >= 'a') && (ch <= 'z')) || ((ch >= 'A') && (ch <= 'Z')) || ((ch >= '0') && (ch <= '9')) || (ch == '_');
					sKey[w++] = bOk ? ch : '_';
				}
				sKey[w] = 0;
				if (!names || xrtValueType(names) != XVALUE_ARRAY) continue;
				for (f = 0; f < ValueCount(names); f++) {
					str want = ValueArrayText(names, f);
					for (m = 0; m < ValueCount(managedFields); m++) {
						xvalue* fld = xrtValueArrayGet(managedFields, m);
						str have = fld ? ValueText(fld, "name") : NULL;
						if (have && want && strcmp(have, want) == 0) {
							ValueSetText(fld, "group", sKey);
							ValueSetText(fld, "groupTitle", title ? title : (str)sKey);
							break;
						}
					}
				}
			}
		}
	}
	ValueSetText(identity, "xid", Content_TextOr(modelXid, ""));
	ValueSetText(identity, "name", Content_TextOr(name, modelXid));
	ValueSetText(identity, "namespace", Content_TextOr(ns, ""));
	ValueSetText(identity, "title", Content_TextOr(title, modelXid));
	ValueSetText(identity, "description", Content_TextOr(description, ""));
	ValueSetOwn(root, "identity", identity);
	ValueSetText(entity, "table", Content_TextOr(tableName, "content_item"));
	ValueSetText(entity, "entityName", Content_TextOr(name, "content"));
	ValueSetText(entity, "titleField", "title");
	ValueSetText(entity, "statusField", "status");
	ValueSetOwn(entity, "fields", managedFields);
	ValueSetOwn(root, "entity", entity);
	ValueSetBool(draft, "enabled", true);
	ValueSetText(draft, "mode", "same-table");
	ValueSetOwn(core, "draft", draft);
	ValueSetBool(core, "adminCrud", true);
	ValueSetBool(core, "publicApi", true);
	ValueSetOwn(root, "coreFeatures", core);
	ValueSetInt(uiList, "pageSize", pages ? ValueInt(pages, "pageSize") : 20);
	{
		str listColumns = pages ? ValueText(pages, "listColumns") : NULL;
		str detailFields = pages ? ValueText(pages, "detailFields") : NULL;
		if (listColumns && listColumns[0]) {
			xvalue* cols = Content_ParseNameList(listColumns);
			if (ValueCount(cols) > 0) ValueSetOwn(uiList, "columns", cols);
			else xrtValueRelease(cols);
		}
		if (detailFields && detailFields[0]) {
			xvalue* flds = Content_ParseNameList(detailFields);
			if (ValueCount(flds) > 0) {
				xvalue* uiDetail = ValueObject();
				ValueSetOwn(uiDetail, "fields", flds);
				ValueSetOwn(ui, "detail", uiDetail);
			} else {
				xrtValueRelease(flds);
			}
		}
	}
	ValueSetOwn(ui, "list", uiList);
	ValueSetText(uiForm, "layout", "single-column");
	ValueSetOwn(ui, "form", uiForm);
	ValueSetOwn(root, "ui", ui);
	if (policies && xrtValueType(policies) == XVALUE_OBJECT)
		ValueSetOwn(root, "policies", xrtValueDeepClone(policies));
	ValueSetOwn(root, "capabilitySlots", Content_CollectEnabledCapabilities(spec));
	if (pages && xrtValueType(pages) == XVALUE_OBJECT) {
		/* fieldGroups/displayGroups（数组 {title,fields}）直通 presentation：
		 * groups 驱动编辑器表单分组，displayGroups 驱动详情展示分组 */
		xvalue* fgArr = ValueGet(pages, "fieldGroups");
		xvalue* dgArr = ValueGet(pages, "displayGroups");
		bool bFg = fgArr && xrtValueType(fgArr) == XVALUE_ARRAY && ValueCount(fgArr) > 0;
		bool bDg = dgArr && xrtValueType(dgArr) == XVALUE_ARRAY && ValueCount(dgArr) > 0;
		if (bFg || bDg) {
			xvalue* presentation = ValueObject();
			if (bFg) {
				xvalue* groups = ValueArray();
				uint32 g;
				for (g = 0; g < ValueCount(fgArr); g++) {
					xvalue* grp = xrtValueArrayGet(fgArr, g);
					xvalue* one = ValueObject();
					str title = grp ? ValueText(grp, "title") : NULL;
					char sKey[80];
					size_t w = 0;
					const char* q = (title && title[0]) ? title : "group";
					while (*q && w + 1 < sizeof(sKey)) {
						char ch = *q++;
						bool bOk = ((ch >= 'a') && (ch <= 'z')) || ((ch >= 'A') && (ch <= 'Z')) || ((ch >= '0') && (ch <= '9')) || (ch == '_');
						sKey[w++] = bOk ? ch : '_';
					}
					sKey[w] = 0;
					ValueSetText(one, "key", sKey);
					ValueSetText(one, "title", title ? title : (str)sKey);
					if (grp) {
						xvalue* names = ValueGet(grp, "fields");
						if (names) ValueSetOwn(one, "fields", xrtValueDeepClone(names));
					}
					ValueArrayOwn(groups, one);
				}
				ValueSetOwn(presentation, "groups", groups);
			}
			if (bDg) ValueSetOwn(presentation, "displayGroups", xrtValueDeepClone(dgArr));
			ValueSetOwn(root, "presentation", presentation);
		}
	}
	json = Content_StringifyJson(root, false);
	xrtValueRelease(root);
	return json;
}

/* ==================== runtime/managed.json ==================== */

static char* Content_BuildRuntimeManagedJson(const char* pluginXid, int revision, int64 now, xvalue* spec)
{
	xvalue* root = ValueObject();
	char* json;
	ValueSetBool(root, "managed", true);
	ValueSetText(root, "managedBy", "content");
	ValueSetText(root, "managedType", "generated-plugin");
	ValueSetText(root, "pluginXid", Content_TextOr(pluginXid, ""));
	ValueSetInt(root, "contentTypeRevision", revision);
	ValueSetText(root, "generatedRoot", "generated");
	ValueSetText(root, "runtimeRoot", "runtime");
	ValueSetText(root, "customRoot", "custom");
	ValueSetInt(root, "generatedAt", now);
	ValueSetOwn(root, "capabilitySlots", Content_CollectEnabledCapabilities(spec));
	json = Content_StringifyJson(root, false);
	xrtValueRelease(root);
	if (!json)
		json = xrtFormat(
			"{\"managed\":true,\"managedBy\":\"content\",\"managedType\":\"generated-plugin\",\"pluginXid\":\"%s\",\"contentTypeRevision\":%d,\"generatedRoot\":\"generated\",\"runtimeRoot\":\"runtime\",\"customRoot\":\"custom\",\"generatedAt\":%lld,\"capabilitySlots\":[]}\n",
			Content_TextOr(pluginXid, ""), revision, (long long)now);
	return json;
}

/* ==================== runtime/contracts.json 与 capability.manifest.json ==================== */

static void Content_AppendCapabilityHookSlot(xvalue* slots, const char* key, const char* surface, const char* phase, const char* description)
{
	xvalue* slot = ValueObject();
	if (!slots || !slot) {
		xrtValueRelease(slot);
		return;
	}
	ValueSetText(slot, "key", Content_TextOr(key, ""));
	ValueSetText(slot, "surface", Content_TextOr(surface, ""));
	ValueSetText(slot, "phase", Content_TextOr(phase, ""));
	ValueSetText(slot, "description", Content_TextOr(description, ""));
	ValueArrayOwn(slots, slot);
}

static void Content_AppendCapabilityHookSlots(xvalue* slots)
{
	Content_AppendCapabilityHookSlot(slots, "schema", "database", "generation", "Ability packs may declare tables, indexes and bounded migrations.");
	Content_AppendCapabilityHookSlot(slots, "route", "plugin.route", "startup", "Ability packs may register admin or public routes only when mounted.");
	Content_AppendCapabilityHookSlot(slots, "menu", "admin.menu", "startup", "Ability packs may register admin menu entries only when mounted.");
	Content_AppendCapabilityHookSlot(slots, "page", "admin.page", "generation", "Ability packs may copy generated admin pages and templates.");
	Content_AppendCapabilityHookSlot(slots, "task", "background.task", "runtime", "Ability packs may register bounded background task producers and dashboards.");
	Content_AppendCapabilityHookSlot(slots, "public-head", "public.html.head", "render", "Ability packs may inject public head assets only when mounted.");
	Content_AppendCapabilityHookSlot(slots, "public-render", "public.html.body", "render", "Ability packs may inject public list/detail render fragments only when mounted.");
}

static char* Content_BuildGeneratedContracts(const char* modelXid, int revision, xvalue* spec)
{
	xvalue* capabilities = ValueGet(spec, "capabilities");
	xvalue* root = ValueObject();
	xvalue* enabledCapabilities = ValueArray();
	xvalue* packs = ValueArray();
	xvalue* hookSlots = ValueArray();
	char* json;
	ValueSetText(root, "model", Content_TextOr(modelXid, ""));
	ValueSetInt(root, "revision", revision);
	ValueSetText(root, "permissionBinding", "ability-admin-page-bound");
	ValueSetText(root, "permissionBindingNote", "Ability permissions are registered and each mounted ability admin page route is bound to the first permission declared by that ability pack.");
	Content_AppendCapabilityHookSlots(hookSlots);
	if (capabilities && xrtValueType(capabilities) == XVALUE_ARRAY) {
		uint32 i;
		for (i = 0; i < ValueCount(capabilities); i++) {
			xvalue* item = xrtValueArrayGet(capabilities, i);
			str key = (item && xrtValueType(item) == XVALUE_OBJECT) ? ValueText(item, "key") : NULL;
			xvalue* packDetail = NULL;
			xvalue* out = NULL;
			str jsonText = NULL;
			if (!key || !key[0]) continue;
			if (!Content_CapabilityEnabled(item)) continue;
			if (item && xrtValueType(item) == XVALUE_OBJECT)
				ValueArrayOwn(enabledCapabilities, xrtValueDeepClone(item));
			packDetail = ContentPack_GetDetail(key);
			out = ValueObject();
			ValueSetText(out, "packId", key);
			ValueSetText(out, "permissionBinding", "ability-admin-page-bound");
			if (packDetail && xrtValueType(packDetail) == XVALUE_OBJECT) {
				xvalue* parsed = NULL;
				ValueSetText(out, "title", ValueText(packDetail, "title"));
				ValueSetText(out, "version", ValueText(packDetail, "version"));
				ValueSetText(out, "description", ValueText(packDetail, "description"));
				jsonText = ValueText(packDetail, "effectsJson");
				parsed = (jsonText && jsonText[0]) ? JsonParseN(jsonText, 0) : NULL;
				if (parsed) ValueSetOwn(out, "effects", parsed);
				jsonText = ValueText(packDetail, "contractsJson");
				parsed = (jsonText && jsonText[0]) ? JsonParseN(jsonText, 0) : NULL;
				if (parsed) ValueSetOwn(out, "contracts", parsed);
				jsonText = ValueText(packDetail, "hooksJson");
				parsed = (jsonText && jsonText[0]) ? JsonParseN(jsonText, 0) : NULL;
				if (parsed) ValueSetOwn(out, "hooks", parsed);
			}
			if (item && xrtValueType(item) == XVALUE_OBJECT) {
				xvalue* config = ValueGet(item, "config");
				xvalue* mount = ValueGet(item, "mount");
				if (config) ValueSetOwn(out, "instanceConfig", xrtValueDeepClone(config));
				if (mount) ValueSetOwn(out, "mount", xrtValueDeepClone(mount));
			}
			ValueArrayOwn(packs, out);
			xrtValueRelease(packDetail);
		}
	}
	ValueSetOwn(root, "capabilities", enabledCapabilities);
	ValueSetOwn(root, "abilityPacks", packs);
	ValueSetOwn(root, "capabilityHookSlots", hookSlots);
	json = Content_StringifyJson(root, true);
	xrtValueRelease(root);
	return json ? json : xrtStrDup("{\"capabilities\":[],\"abilityPacks\":[],\"capabilityHookSlots\":[]}\n");
}

static char* Content_BuildGeneratedCapabilityManifest(const char* modelXid, int revision, xvalue* spec)
{
	xvalue* capabilities = ValueGet(spec, "capabilities");
	xvalue* root = ValueObject();
	xvalue* packs = ValueArray();
	char* json;
	ValueSetInt(root, "formatVersion", 1);
	ValueSetText(root, "model", Content_TextOr(modelXid, ""));
	ValueSetInt(root, "revision", revision);
	ValueSetText(root, "loadPolicy", "enabled-packs-only");
	if (capabilities && xrtValueType(capabilities) == XVALUE_ARRAY) {
		uint32 i;
		for (i = 0; i < ValueCount(capabilities); i++) {
			xvalue* item = xrtValueArrayGet(capabilities, i);
			str key = (item && xrtValueType(item) == XVALUE_OBJECT) ? ValueText(item, "key") : NULL;
			xvalue* packDetail = NULL;
			xvalue* out = NULL;
			str manifestJson = NULL;
			if (!key || !key[0]) continue;
			if (!Content_CapabilityEnabled(item)) continue;
			packDetail = ContentPack_GetDetail(key);
			out = ValueObject();
			ValueSetText(out, "packId", key);
			if (packDetail && xrtValueType(packDetail) == XVALUE_OBJECT) {
				xvalue* manifest = NULL;
				ValueSetText(out, "title", ValueText(packDetail, "title"));
				ValueSetText(out, "version", ValueText(packDetail, "version"));
				ValueSetText(out, "description", ValueText(packDetail, "description"));
				manifestJson = ValueText(packDetail, "manifestJson");
				if (manifestJson && manifestJson[0]) {
					manifest = JsonParseN(manifestJson, 0);
					if (manifest && xrtValueType(manifest) == XVALUE_OBJECT) {
						ValueSetOwn(out, "manifest", manifest);
						manifest = NULL;
					}
					xrtValueRelease(manifest);
				}
			}
			if (item && xrtValueType(item) == XVALUE_OBJECT) {
				xvalue* config = ValueGet(item, "config");
				xvalue* mount = ValueGet(item, "mount");
				if (config) ValueSetOwn(out, "instanceConfig", xrtValueDeepClone(config));
				if (mount) ValueSetOwn(out, "mount", xrtValueDeepClone(mount));
			}
			ValueArrayOwn(packs, out);
			xrtValueRelease(packDetail);
		}
	}
	ValueSetOwn(root, "abilityPacks", packs);
	json = Content_StringifyJson(root, true);
	xrtValueRelease(root);
	return json ? json : xrtStrDup("{\"formatVersion\":1,\"abilityPacks\":[]}\n");
}

/* ==================== plugin.json（v1 字段全集 + runtime.xadminManaged） ==================== */

static bool Content_ValueArrayTextContains(xvalue* arr, const char* text)
{
	uint32 i;
	if (!arr || !text) return false;
	for (i = 0; i < ValueCount(arr); i++)
		if (ValueArrayText(arr, i) && !strcmp(ValueArrayText(arr, i), text)) return true;
	return false;
}

static void Content_AppendArrayText(xvalue* arr, const char* text)
{
	xvalue* item = xrtValueString(xrtStrView(text));
	if (item) ValueArrayOwn(arr, item);
}

static void Content_BuildCapabilityDefines(xvalue* defines, xvalue* capabilities)
{
	Content_AppendArrayText(defines, "XADMIN_PLUGIN=1");
	if (!capabilities || xrtValueType(capabilities) != XVALUE_ARRAY) return;
	{
		uint32 i;
		for (i = 0; i < ValueCount(capabilities); i++) {
			xvalue* item = xrtValueArrayGet(capabilities, i);
			str key = (item && xrtValueType(item) == XVALUE_OBJECT) ? ValueText(item, "key") : NULL;
			char* define = NULL;
			if (!key || !key[0]) continue;
			if (!Content_CapabilityEnabled(item)) continue;
			define = Content_CapabilityDefineName(key);
			if (define) {
				char* entry = xrtFormat("%s=1", define);
				Content_AppendArrayText(defines, entry);
				xrtFree(entry);
			}
			xrtFree(define);
		}
	}
}

static char* Content_BuildGeneratedPluginJson(const char* pluginXid, const char* title, const char* description, xvalue* capabilities, xvalue* sources, xvalue* includeDirs)
{
	xvalue* m = ValueObject();
	xvalue* runtime = ValueObject();
	xvalue* resources = ValueObject();
	xvalue* build = ValueObject();
	xvalue* compat = ValueObject();
	xvalue* deps = ValueObject();
	xvalue* contributes = ValueObject();
	xvalue* caps = ValueArray();
	xvalue* defines = ValueArray();
	xvalue* managed = ValueObject();
	char* json;
	ValueSetInt(m, "formatVersion", 4);
	ValueSetText(m, "xid", pluginXid);
	ValueSetText(m, "name", pluginXid);
	ValueSetText(m, "title", Content_TextOr(title, "Generated Content Plugin"));
	ValueSetText(m, "description", Content_TextOr(description, "Generated by xAdmin built-in content system"));
	ValueSetText(m, "version", "1.0.0");
	ValueSetText(m, "author", "xAdmin");
	ValueSetText(m, "kind", "singleton");
	ValueSetText(runtime, "compiler", "tcc");
	ValueSetText(runtime, "language", "c");
	ValueSetText(runtime, "xadminManaged", "content");
	ValueSetOwn(m, "runtime", runtime);
	ValueSetText(resources, "page", "page");
	ValueSetText(resources, "template", "template");
	ValueSetText(resources, "option", "option");
	ValueSetText(resources, "static", "static");
	ValueSetOwn(m, "resources", resources);
	ValueSetText(build, "entry", "generated/main.c");
	{
		xvalue* sourceList = ValueArray();
		Content_AppendArrayText(sourceList, "generated/main.c");
		if (sources && xrtValueType(sources) == XVALUE_ARRAY) {
			uint32 i;
			for (i = 0; i < ValueCount(sources); i++) {
				str rel = ValueArrayText(sources, i);
				if (rel && rel[0] && !Content_ValueArrayTextContains(sourceList, rel))
					Content_AppendArrayText(sourceList, rel);
			}
		}
		ValueSetOwn(build, "sources", sourceList);
	}
	ValueSetOwn(build, "includeDirs", includeDirs ? xrtValueDeepClone(includeDirs) : ValueArray());
	ValueSetOwn(build, "includeDirs", includeDirs ? xrtValueDeepClone(includeDirs) : ValueArray());
	Content_BuildCapabilityDefines(defines, capabilities);
	ValueSetOwn(build, "defines", defines);
	ValueSetOwn(m, "build", build);
	ValueSetText(compat, "minHostVersion", "4.0.0");
	ValueSetText(compat, "maxHostVersion", "5.0.0");
	ValueSetInt(compat, "abiVersion", XADMIN_ABI_VERSION);
	ValueSetOwn(m, "compat", compat);
	Content_AppendArrayText(caps, "route.public");
	Content_AppendArrayText(caps, "route.admin");
	ValueSetOwn(m, "capabilities", caps);
	ValueSetOwn(deps, "plugins", ValueArray());
	ValueSetOwn(deps, "services", ValueArray());
	ValueSetOwn(deps, "features", ValueArray());
	ValueSetOwn(m, "dependencies", deps);
	ValueSetOwn(contributes, "menus", ValueArray());
	ValueSetOwn(contributes, "routes", ValueArray());
	ValueSetOwn(contributes, "hooks", ValueArray());
	ValueSetOwn(contributes, "events", ValueArray());
	ValueSetOwn(m, "contributes", contributes);
	ValueSetText(m, "defaultConfig", "config.defaults.json");
	ValueSetText(m, "configSchema", "config.schema.json");
	ValueSetBool(managed, "managed", true);
	ValueSetText(managed, "generator", "content");
	ValueSetText(managed, "managedBy", "content");
	ValueSetText(managed, "managedType", "generated-plugin");
	ValueSetText(managed, "generatedRoot", "generated");
	ValueSetText(managed, "runtimeRoot", "runtime");
	ValueSetText(managed, "customRoot", "custom");
	ValueSetOwn(m, "xadminManaged", managed);
	json = Content_StringifyJson(m, true);
	xrtValueRelease(m);
	return json;
}

/* ==================== 相对路径安全与声明文件拷贝 ==================== */

/* v1 Content_GeneratedRelativePathSafe 照抄：拒绝绝对路径、..、盘符冒号 */
static bool Content_GeneratedRelativePathSafe(const char* path)
{
	if (!path || !path[0]) return false;
	if (path[0] == '/' || path[0] == '\\') return false;
	if (strstr(path, "..")) return false;
	if (strchr(path, ':')) return false;
	return true;
}

static char* Content_CopyGeneratedPathDir(const char* path)
{
	const char* slash1 = NULL;
	const char* slash2 = NULL;
	const char* slash = NULL;
	size_t len = 0;
	char* out = NULL;
	if (!Content_GeneratedRelativePathSafe(path)) return NULL;
	slash1 = strrchr(path, '/');
	slash2 = strrchr(path, '\\');
	if (slash1 && slash2) slash = (slash1 > slash2) ? slash1 : slash2;
	else slash = slash1 ? slash1 : slash2;
	if (!slash) return NULL;
	len = (size_t)(slash - path);
	if (len == 0) return NULL;
	out = (char*)xrtMalloc(len + 1);
	if (!out) return NULL;
	memcpy(out, path, len);
	out[len] = '\0';
	return out;
}

static void Content_SetGeneratedFile(XAdminGeneratedFile* file, const char* relativePath, const char* data)
{
	const char* safeData = data ? data : "";
	if (!file) return;
	file->relative_path = relativePath;
	file->data = safeData;
	file->size = strlen(safeData);
}

static void Content_SetGeneratedFileBinary(XAdminGeneratedFile* file, const char* relativePath, const void* data, size_t size)
{
	if (!file) return;
	file->relative_path = relativePath ? relativePath : "";
	file->data = data;
	file->size = size;
}

/* 收集各启用包 build 声明：sourceFiles → sources；includeFiles 目录 → includeDirs（去重） */
static void Content_AppendDeclaredPackBuildPaths(xvalue* packDetail, xvalue* sources, xvalue* includeDirs)
{
	str manifestJson = packDetail ? ValueText(packDetail, "manifestJson") : NULL;
	xvalue* manifest = NULL;
	if (!manifestJson || !manifestJson[0]) return;
	manifest = JsonParseN(manifestJson, 0);
	if (!manifest || xrtValueType(manifest) != XVALUE_OBJECT) {
		xrtValueRelease(manifest);
		return;
	}
	{
		xvalue* sourceFiles = ValueGet(manifest, "sourceFiles");
		xvalue* includeFiles = ValueGet(manifest, "includeFiles");
		if (sourceFiles && xrtValueType(sourceFiles) == XVALUE_ARRAY) {
			uint32 i;
			for (i = 0; i < ValueCount(sourceFiles); i++) {
				str rel = ValueTextOf(xrtValueArrayGet(sourceFiles, i));
				if (!Content_GeneratedRelativePathSafe(rel)) continue;
				if (sources && !Content_ValueArrayTextContains(sources, rel))
					Content_AppendArrayText(sources, rel);
			}
		}
		if (includeFiles && xrtValueType(includeFiles) == XVALUE_ARRAY) {
			uint32 i;
			for (i = 0; i < ValueCount(includeFiles); i++) {
				str rel = ValueTextOf(xrtValueArrayGet(includeFiles, i));
				char* dir = Content_CopyGeneratedPathDir(rel);
				if (dir && includeDirs && !Content_ValueArrayTextContains(includeDirs, dir))
					Content_AppendArrayText(includeDirs, dir);
				xrtFree(dir);
			}
		}
	}
	xrtValueRelease(manifest);
}

/* 各启用包声明的 sourceFiles/includeFiles/templateFiles/assetFiles 逐个读入文件清单 */
static void Content_AppendDeclaredPackFiles(XAdminGeneratedFile* files, int* fileCount, int fileCap, char** ownedPaths, bytes* ownedData, int* ownedCount, int ownedCap, xvalue* packDetail)
{
	static const char* keys[4] = {"sourceFiles", "includeFiles", "templateFiles", "assetFiles"};
	str packPath = packDetail ? ValueText(packDetail, "path") : NULL;
	str manifestJson = packDetail ? ValueText(packDetail, "manifestJson") : NULL;
	xvalue* manifest = NULL;
	int k;
	if (!files || !fileCount || !ownedPaths || !ownedData || !ownedCount) return;
	if (!packPath || !packPath[0] || !manifestJson || !manifestJson[0]) return;
	manifest = JsonParseN(manifestJson, 0);
	if (!manifest || xrtValueType(manifest) != XVALUE_OBJECT) {
		xrtValueRelease(manifest);
		return;
	}
	for (k = 0; k < 4; k++) {
		xvalue* arr = ValueGet(manifest, keys[k]);
		uint32 i;
		if (!arr || xrtValueType(arr) != XVALUE_ARRAY) continue;
		for (i = 0; i < ValueCount(arr); i++) {
			str rel = ValueTextOf(xrtValueArrayGet(arr, i));
			char* fullPath = NULL;
			bytes data = NULL;
			size_t size = 0;
			if (!Content_GeneratedRelativePathSafe(rel)) continue;
			if ((*fileCount >= fileCap) || (*ownedCount >= ownedCap)) continue;
			fullPath = xrtPathJoin(packPath, rel);
			if (!fullPath) continue;
			data = xrtFileReadAll(fullPath, &size);
			xrtFree(fullPath);
			if (!data) continue;
			ownedPaths[*ownedCount] = xrtStrDup(rel);
			ownedData[*ownedCount] = data;
			Content_SetGeneratedFileBinary(&files[*fileCount], ownedPaths[*ownedCount], data, size);
			(*fileCount)++;
			(*ownedCount)++;
		}
	}
	xrtValueRelease(manifest);
}

/* ==================== 生成台账（事务性：pending → success/failed） ==================== */

static sqlite3_int64 Content_InsertGenerationPending(int modelId, int revision, const char* pluginXid, int64 now)
{
	sqlite3_stmt* stmt = NULL;
	if (sqlite3_prepare_v2(G_DB,
		"INSERT INTO content_generation (model_id,target_revision,plugin_xid,status,output_json,advisor_json,error_message,create_time,finish_time) "
		"VALUES (?,?,?,'pending','{}','{}','',?,0)", -1, &stmt, NULL) != SQLITE_OK)
		return 0;
	sqlite3_bind_int(stmt, 1, modelId);
	sqlite3_bind_int(stmt, 2, revision);
	Content_BindText(stmt, 3, pluginXid);
	sqlite3_bind_int64(stmt, 4, now);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	return sqlite3_last_insert_rowid(G_DB);
}

static void Content_FinishGenerationSuccess(sqlite3_int64 rowId, const char* outputJson, const char* advisorJson, int64 now)
{
	sqlite3_stmt* stmt = NULL;
	if (rowId <= 0) return;
	if (sqlite3_prepare_v2(G_DB,
		"UPDATE content_generation SET status='success', output_json=?, advisor_json=?, finish_time=? WHERE id=?", -1, &stmt, NULL) != SQLITE_OK)
		return;
	Content_BindText(stmt, 1, outputJson ? outputJson : "{}");
	Content_BindText(stmt, 2, advisorJson ? advisorJson : "{}");
	sqlite3_bind_int64(stmt, 3, now);
	sqlite3_bind_int64(stmt, 4, rowId);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
}

static void Content_FinishGenerationFailure(sqlite3_int64 rowId, const char* advisorJson, const char* errorMessage, int64 now)
{
	sqlite3_stmt* stmt = NULL;
	if (rowId <= 0) return;
	if (sqlite3_prepare_v2(G_DB,
		"UPDATE content_generation SET status='failed', advisor_json=?, error_message=?, finish_time=? WHERE id=?", -1, &stmt, NULL) != SQLITE_OK)
		return;
	Content_BindText(stmt, 1, advisorJson ? advisorJson : "{}");
	Content_BindText(stmt, 2, errorMessage ? errorMessage : "generate plugin failed");
	sqlite3_bind_int64(stmt, 3, now);
	sqlite3_bind_int64(stmt, 4, rowId);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
}

static void Content_UpdateAppliedRevision(int modelId, int revision, const char* pluginXid)
{
	sqlite3_stmt* stmt = NULL;
	if (sqlite3_prepare_v2(G_DB,
		"UPDATE content_model SET generated_plugin_xid=?, applied_revision=?, update_time=? WHERE id=?", -1, &stmt, NULL) != SQLITE_OK)
		return;
	Content_BindText(stmt, 1, pluginXid);
	sqlite3_bind_int(stmt, 2, revision);
	sqlite3_bind_int64(stmt, 3, xrtNow());
	sqlite3_bind_int(stmt, 4, modelId);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
}

/* ==================== 落盘驱动 ==================== */

/* 已存在实例（换代覆盖）时刷新 plugin_package 台账行（v1 Discover 语义） */
static void Content_RefreshGeneratedPackageRow(const char* root, const char* pluginXid)
{
	char* path = xrtPathJoin(root, "plugin.json");
	char* text = NULL;
	sqlite3_stmt* stmt = NULL;
	size_t size = 0;
	bytes data;
	if (!path) return;
	data = xrtFileReadAll(path, &size);
	xrtFree(path);
	if (data) {
		text = (char*)xrtMalloc(size + 1);
		if (text) {
			memcpy(text, data, size);
			text[size] = '\0';
		}
		xrtFree(data);
	}
	if (sqlite3_prepare_v2(G_DB,
		"UPDATE plugin_package SET install_path=?, manifest_json=? WHERE xid=?", -1, &stmt, NULL) == SQLITE_OK) {
		Plugin_BindText(stmt, 1, root);
		Plugin_BindText(stmt, 2, text ? text : "");
		Plugin_BindText(stmt, 3, pluginXid);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}
	xrtFree(text);
}

/* 宿主侧代写（v1 PluginSystem_Generate 语义）：目录骨架 + 文件 + manifest + 登记/覆盖。
 * 与 XAdmin_GeneratePlugin 的差别仅在于不要求插件调用者、允许目标已存在（内容系统换代覆盖）。 */
static bool Content_WriteGeneratedPluginLocal(const XAdminGeneratedPluginSpec* spec)
{
	static const char* dirs[] = {"page", "template", "option", "static", "inc", "lib", "src", "data"};
	char* root = NULL;
	bool ok = true;
	size_t i;
	if (!spec || !Plugin_XidValid(spec->xid)) return false;
	root = xrtPathJoin(xrtPathJoin(AppPath, "plugin"), (char*)spec->xid);
	if (!root) return false;
	xrtDirCreateAll(root);
	for (i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++) {
		char* sub = xrtPathJoin(root, (char*)dirs[i]);
		if (sub) {
			xrtDirCreateAll(sub);
			xrtFree(sub);
		}
	}
	for (i = 0; i < spec->file_count; i++) {
		if (!Plugin_WriteGeneratedFile(root, &spec->files[i])) {
			printf("[content][error] generate %s: write file failed: %s\n", spec->xid,
				spec->files[i].relative_path ? spec->files[i].relative_path : "");
			ok = false;
			break;
		}
	}
	if (ok && !Plugin_GeneratedSpecHasFile(spec, "plugin.json") && !Plugin_WriteGeneratedManifest(root, spec)) {
		printf("[content][error] generate %s: write manifest failed\n", spec->xid);
		ok = false;
	}
	if (ok && !Plugin_Find(spec->xid) && !Plugin_RegisterGenerated(root, spec->xid))
		ok = false;
	if (ok) Content_RefreshGeneratedPackageRow(root, spec->xid);
	if (ok && spec->auto_enable)
		ok = PluginHost_SetEnabled(spec->xid, true);
	xrtFree(root);
	return ok;
}

/* ABI 入口优先；宿主路由上下文无插件调用者或目标已存在（换代覆盖）时走本地代写 */
static bool Content_RunPluginGeneration(const XAdminGeneratedPluginSpec* spec)
{
	if (XAdmin_GeneratePlugin(NULL, spec) == 0) return true;
	return Content_WriteGeneratedPluginLocal(spec);
}

/* ==================== 生成编排入口 ==================== */

#define CONTENT_GEN_FILES_MAX  128
#define CONTENT_GEN_OWNED_MAX  96

/* 全流程：按 xid 读库内 spec → 复校验 + advisor 门禁 → 组装产物 → 落盘 → 台账。
 * 返回 data 对象（v1 契约）：{pluginXid, revision, generated, outputJson}；失败置 *error。 */
static xvalue* Content_GeneratePluginForModel(const char* xid, str* error)
{
	xvalue* model = NULL;
	xvalue* spec = NULL;
	xvalue* advisor = NULL;
	xvalue* capabilities = NULL;
	xvalue* sources = ValueArray();
	xvalue* includeDirs = ValueArray();
	xvalue* outputFiles = ValueArray();
	xvalue* ret = NULL;
	XAdminGeneratedFile files[CONTENT_GEN_FILES_MAX];
	XAdminGeneratedPluginSpec genSpec;
	char* ownedPaths[CONTENT_GEN_OWNED_MAX];
	bytes ownedData[CONTENT_GEN_OWNED_MAX];
	int modelId = 0;
	int revision = 0;
	str pluginXid = NULL;
	str modelTitle = NULL;
	str specJson = NULL;
	str pluginTitle = NULL;
	str menuTitle = NULL;
	str pluginDescription = NULL;
	char* validateError = NULL;
	char* advisorJson = NULL;
	char* managedSpecJson = NULL;
	char* pluginJson = NULL;
	char* mainC = NULL;
	char* adminHtml = NULL;
	char* draftHtml = NULL;
	char* editorHtml = NULL;
	char* categoryHtml = NULL;
	char* dashboardHtml = NULL;
	char* tasksHtml = NULL;
	char* publicHtml = NULL;
	char* abilityHtml = NULL;
	char* staticDetailHtml = NULL;
	char* managedJson = NULL;
	char* contractsJson = NULL;
	char* capabilityManifestJson = NULL;
	char* migrationPlanJson = NULL;
	char* outputJson = NULL;
	int64 now = xrtNow();
	sqlite3_int64 genRowId = 0;
	bool categoryPack = false;
	bool metricPack = false;
	bool taskPack = false;
	bool ok = false;
	int fileCount = 0;
	int ownedCount = 0;
	uint32 i;

	if (error) *error = NULL;
	if (!Content_IsValidXid(xid)) {
		if (error) *error = xrtStrDup("invalid xid");
		return NULL;
	}
	model = Content_GetModelByXid(xid);
	if (!model) {
		if (error) *error = xrtStrDup("model not found");
		return NULL;
	}
	modelId = (int)ValueInt(model, "id");
	revision = (int)ValueInt(model, "currentRevision");
	pluginXid = ValueText(model, "generatedPluginXid");
	if (!pluginXid || !pluginXid[0]) pluginXid = (str)xid;
	modelTitle = ValueText(model, "title");
	specJson = ValueText(model, "specJson");

	/* 事务性：先落 pending 行，后续统一更新为 success/failed */
	genRowId = Content_InsertGenerationPending(modelId, revision, pluginXid, now);

	spec = (specJson && specJson[0]) ? JsonParseN(specJson, 0) : NULL;
	if (!spec || xrtValueType(spec) != XVALUE_OBJECT || !Content_SpecValidate(spec, &validateError)) {
		char* message = xrtFormat("stored spec invalid: %s", validateError ? validateError : "invalid json");
		if (error) *error = xrtStrDup(message ? message : "stored spec invalid");
		Content_FinishGenerationFailure(genRowId, "{}", message ? message : "stored spec invalid", now);
		xrtFree(message);
		goto cleanup;
	}
	advisor = Content_BuildAdvisor(spec);
	if (advisor) {
		str advisorStatus = ValueText(advisor, "status");
		advisorJson = Content_StringifyJson(advisor, false);
		if (!advisorStatus || strcmp(advisorStatus, "ok") != 0) {
			if (error) *error = xrtStrDup("advisor check failed");
			Content_FinishGenerationFailure(genRowId, advisorJson ? advisorJson : "{}", "advisor check failed", now);
			goto cleanup;
		}
	} else {
		advisorJson = xrtStrDup("{}");
	}
	pluginTitle = ValueText(spec, "pluginTitle");
	menuTitle = ValueText(spec, "menuTitle");
	pluginDescription = ValueText(spec, "description");
	capabilities = ValueGet(spec, "capabilities");
	categoryPack = Content_SpecHasCapability(spec, "content.category");
	metricPack = Content_SpecHasCapability(spec, "content.like") || Content_SpecHasCapability(spec, "content.view-stat");
	taskPack = Content_SpecHasCapability(spec, "content.static") || Content_SpecHasCapability(spec, "content.sitemap")
		|| Content_SpecHasCapability(spec, "content.import-export") || Content_SpecHasCapability(spec, "content.search")
		|| Content_SpecHasCapability(spec, "content.form") || Content_SpecHasCapability(spec, "content.audit-log");

	/* build 声明收集（sources/includeDirs 去重） */
	if (capabilities && xrtValueType(capabilities) == XVALUE_ARRAY) {
		for (i = 0; i < ValueCount(capabilities); i++) {
			xvalue* cap = xrtValueArrayGet(capabilities, i);
			str capKey = (cap && xrtValueType(cap) == XVALUE_OBJECT) ? ValueText(cap, "key") : NULL;
			xvalue* packDetail = NULL;
			if (!capKey || !capKey[0]) continue;
			if (!Content_CapabilityEnabled(cap)) continue;
			packDetail = ContentPack_GetDetail(capKey);
			Content_AppendDeclaredPackBuildPaths(packDetail, sources, includeDirs);
			xrtValueRelease(packDetail);
		}
	}

	managedSpecJson = Content_BuildManagedSpecJson(spec, xid, modelTitle, pluginDescription);
	pluginJson = Content_BuildGeneratedPluginJson(pluginXid,
		Content_TextOr(pluginTitle, modelTitle), pluginDescription, capabilities, sources, includeDirs);
	mainC = Content_BuildManagedMainC(pluginXid,
		Content_TextOr(pluginTitle, modelTitle),
		Content_TextOr(menuTitle, Content_TextOr(pluginTitle, modelTitle)),
		spec);
	adminHtml = Content_BuildManagedAdminPageHtml(pluginXid, "articles", spec);
	draftHtml = Content_BuildManagedAdminPageHtml(pluginXid, "drafts", spec);
	editorHtml = Content_BuildManagedEditorHtml(pluginXid);
	if (categoryPack) categoryHtml = Content_BuildManagedCategoryHtml(pluginXid);
	if (metricPack) dashboardHtml = Content_BuildManagedDashboardHtml(pluginXid);
	if (taskPack) tasksHtml = Content_BuildManagedTasksHtml(pluginXid);
	publicHtml = Content_BuildManagedPublicHtml(pluginXid);
	abilityHtml = Content_BuildManagedAbilityHtml(pluginXid);
	staticDetailHtml = Content_BuildManagedStaticDetailHtml();
	managedJson = Content_BuildRuntimeManagedJson(pluginXid, revision, now, spec);
	contractsJson = Content_BuildGeneratedContracts(xid, revision, spec);
	capabilityManifestJson = Content_BuildGeneratedCapabilityManifest(xid, revision, spec);

	memset(files, 0, sizeof(files));
	memset(ownedPaths, 0, sizeof(ownedPaths));
	memset(ownedData, 0, sizeof(ownedData));
	Content_SetGeneratedFile(&files[fileCount++], "plugin.json", pluginJson);
	Content_AppendArrayText(outputFiles, "plugin.json");
	Content_SetGeneratedFile(&files[fileCount++], "generated/main.c", mainC);
	Content_AppendArrayText(outputFiles, "generated/main.c");
	Content_SetGeneratedFile(&files[fileCount++], "generated/admin.html", adminHtml);
	Content_AppendArrayText(outputFiles, "generated/admin.html");
	Content_SetGeneratedFile(&files[fileCount++], "generated/drafts.html", draftHtml);
	Content_AppendArrayText(outputFiles, "generated/drafts.html");
	Content_SetGeneratedFile(&files[fileCount++], "generated/editor.html", editorHtml);
	Content_AppendArrayText(outputFiles, "generated/editor.html");
	if (categoryPack) {
		Content_SetGeneratedFile(&files[fileCount++], "generated/categories.html", categoryHtml);
		Content_AppendArrayText(outputFiles, "generated/categories.html");
	}
	if (metricPack) {
		Content_SetGeneratedFile(&files[fileCount++], "generated/dashboard.html", dashboardHtml);
		Content_AppendArrayText(outputFiles, "generated/dashboard.html");
	}
	if (taskPack) {
		Content_SetGeneratedFile(&files[fileCount++], "generated/tasks.html", tasksHtml);
		Content_AppendArrayText(outputFiles, "generated/tasks.html");
	}
	Content_SetGeneratedFile(&files[fileCount++], "generated/public.html", publicHtml);
	Content_AppendArrayText(outputFiles, "generated/public.html");
	Content_SetGeneratedFile(&files[fileCount++], "generated/ability.html", abilityHtml);
	Content_AppendArrayText(outputFiles, "generated/ability.html");
	Content_SetGeneratedFile(&files[fileCount++], "generated/spec.json", managedSpecJson);
	Content_AppendArrayText(outputFiles, "generated/spec.json");
	Content_SetGeneratedFile(&files[fileCount++], "template/static/detail.html", staticDetailHtml);
	Content_AppendArrayText(outputFiles, "template/static/detail.html");
	Content_SetGeneratedFile(&files[fileCount++], "config.defaults.json", "{\"pageSize\":20}\n");
	Content_AppendArrayText(outputFiles, "config.defaults.json");
	Content_SetGeneratedFile(&files[fileCount++], "config.schema.json",
		"{\"type\":\"object\",\"properties\":{\"pageSize\":{\"type\":\"integer\",\"title\":\"Page Size\"}},\"additionalProperties\":false}\n");
	Content_AppendArrayText(outputFiles, "config.schema.json");
	Content_SetGeneratedFile(&files[fileCount++], "runtime/managed.json", managedJson);
	Content_AppendArrayText(outputFiles, "runtime/managed.json");
	Content_SetGeneratedFile(&files[fileCount++], "runtime/contracts.json", contractsJson);
	Content_AppendArrayText(outputFiles, "runtime/contracts.json");
	Content_SetGeneratedFile(&files[fileCount++], "runtime/capability.manifest.json", capabilityManifestJson);
	Content_AppendArrayText(outputFiles, "runtime/capability.manifest.json");
	Content_SetGeneratedFile(&files[fileCount++], "runtime/capability.mounts.example.json", "{\"mounts\":[]}\n");
	Content_AppendArrayText(outputFiles, "runtime/capability.mounts.example.json");
	Content_SetGeneratedFile(&files[fileCount++], "runtime/capability.mounts.schema.json",
		"{\"type\":\"object\",\"properties\":{\"mounts\":{\"type\":\"array\"}},\"required\":[\"mounts\"]}\n");
	Content_AppendArrayText(outputFiles, "runtime/capability.mounts.schema.json");
	migrationPlanJson = xrtFormat("{\"pluginXid\":\"%s\",\"revision\":%d,\"items\":[],\"sql\":[]}\n", pluginXid ? pluginXid : "", revision);
	Content_SetGeneratedFile(&files[fileCount++], "runtime/migration.plan.json", migrationPlanJson);
	Content_AppendArrayText(outputFiles, "runtime/migration.plan.json");
	Content_SetGeneratedFile(&files[fileCount++], "generated/migration.sql",
		"-- Managed content migration is handled by generated plugin startup.\n");
	Content_AppendArrayText(outputFiles, "generated/migration.sql");
	Content_SetGeneratedFile(&files[fileCount++], "custom/README.txt",
		"This directory is reserved for user-owned extensions.\n");
	Content_AppendArrayText(outputFiles, "custom/README.txt");
	if (capabilities && xrtValueType(capabilities) == XVALUE_ARRAY) {
		for (i = 0; i < ValueCount(capabilities); i++) {
			xvalue* cap = xrtValueArrayGet(capabilities, i);
			str capKey = (cap && xrtValueType(cap) == XVALUE_OBJECT) ? ValueText(cap, "key") : NULL;
			xvalue* packDetail = NULL;
			if (!capKey || !capKey[0]) continue;
			if (!Content_CapabilityEnabled(cap)) continue;
			packDetail = ContentPack_GetDetail(capKey);
			Content_AppendDeclaredPackFiles(files, &fileCount, CONTENT_GEN_FILES_MAX,
				ownedPaths, ownedData, &ownedCount, CONTENT_GEN_OWNED_MAX, packDetail);
			xrtValueRelease(packDetail);
		}
	}
	{
		xvalue* output = ValueObject();
		ValueSetText(output, "pluginXid", pluginXid);
		ValueSetInt(output, "revision", revision);
		ValueSetOwn(output, "files", outputFiles);
		outputFiles = NULL;
		outputJson = Content_StringifyJson(output, false);
		xrtValueRelease(output);
	}

	memset(&genSpec, 0, sizeof(genSpec));
	genSpec.xid = pluginXid;
	genSpec.title = modelTitle ? modelTitle : "Generated Content Plugin";
	genSpec.version = "1.0.0";
	genSpec.entry = "generated/main.c";
	genSpec.auto_enable = 0;
	genSpec.file_count = (size_t)fileCount;
	genSpec.files = files;
	ok = Content_RunPluginGeneration(&genSpec);

	if (ok) {
		Content_FinishGenerationSuccess(genRowId, outputJson, advisorJson, now);
		Content_UpdateAppliedRevision(modelId, revision, pluginXid);
		ret = ValueObject();
		ValueSetText(ret, "pluginXid", pluginXid);
		ValueSetInt(ret, "revision", revision);
		ValueSetBool(ret, "generated", true);
		if (outputJson) ValueSetText(ret, "outputJson", outputJson);
	} else {
		if (error) *error = xrtStrDup("generate plugin failed");
		Content_FinishGenerationFailure(genRowId, advisorJson, "generate plugin failed", now);
	}

cleanup:
	xrtValueRelease(advisor);
	xrtValueRelease(spec);
	xrtValueRelease(model);
	xrtValueRelease(outputFiles);
	xrtValueRelease(sources);
	xrtValueRelease(includeDirs);
	xrtFree(validateError);
	xrtFree(advisorJson);
	xrtFree(managedSpecJson);
	xrtFree(pluginJson);
	xrtFree(mainC);
	xrtFree(adminHtml);
	xrtFree(draftHtml);
	xrtFree(editorHtml);
	xrtFree(categoryHtml);
	xrtFree(dashboardHtml);
	xrtFree(tasksHtml);
	xrtFree(publicHtml);
	xrtFree(abilityHtml);
	xrtFree(staticDetailHtml);
	xrtFree(managedJson);
	xrtFree(contractsJson);
	xrtFree(capabilityManifestJson);
	xrtFree(migrationPlanJson);
	xrtFree(outputJson);
	for (i = 0; i < (uint32)ownedCount; i++) {
		xrtFree(ownedPaths[i]);
		xrtFree(ownedData[i]);
	}
	return ret;
}
