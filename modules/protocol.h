/* 请求入口只负责连接新版请求视图与既有业务链。
 * 当前阶段为复用的预编译 SQL/可变缓存串行化整个业务区，正文读取在锁外。
 * 不能只给 sqlite3_step 加锁：bind → step → reset 必须作为一个整体保护。
 */
static void XAdmin_Dispatch(XAdminRequest* req, const RouteInfo* route, bool alias)
{
	char id[128] = {0}; xvalue* session; XAdminRouteProc proc = NULL; int i;
	xsReqCookieValue(req, route->bAdmin ? "XSID" : "MSID", id, sizeof(id));
	if (route->bAdmin) session=Session_Acquire(true,id);
	else {
		bool invalid=false;session=XA_RequestSession(req,&invalid);
		/* Invalid explicit Bearer never falls back to cookies or anonymous login. */
		bool duplicate=false;const xhttpfield* authorization=XA_Header(req,"Authorization",&duplicate);
		if(invalid&&(authorization||duplicate)){XA_Reply(req,401,"invalid bearer token",NULL,NULL);goto done;}
	}
	if(!route->bAdmin&&xsReqMethodID(req)!=XHTTP_METHOD_GET&&xsReqMethodID(req)!=XHTTP_METHOD_HEAD&&
	   xrtValueType(session)==XVALUE_OBJECT&&strcmp(req->path,"/api/v1/token/refresh")&&
	   strcmp(req->path,"/api/v1/login")&&strcmp(req->path,"/api/v1/register")&&!XA_RequestCSRF(req,session)){
		XA_Reply(req,403,"CSRF verification failed",NULL,NULL);goto done;
	}
	if (Option_AdminEntryEnabled() && route->bAdmin && xrtValueType(session) != XVALUE_OBJECT && !alias) {
		LoadPage(req, 404, HTTP_CT_HTML, "status/404.html");
		goto done;
	}
	if (route->bAuth) {
		bool ok = false;
		if (route->bAdmin && route->bPutLog) Logs_Add(req, session, route->bMaskBody);
		if (xrtValueType(session) == XVALUE_OBJECT) {
			int64 role = ValueInt(session, route->bAdmin ? "roleID" : "groupId");
			xvalue* rights = XAdminIDCacheGetValue(route->bAdmin ? G_CACHE_RoleAuth : G_CACHE_MemberGroupAuth, role);
			ok = ValueBool(rights, route->Path);
			if (!ok && route->AuthID > 0) ok = route->bAdmin ? Auth_DBRoleGetAccess(role, route->AuthID, NULL)
			                                : MemberAuth_DBGroupGetAccess(role, route->AuthID, NULL);
			if (!ok) {
				if (route->bAdmin) LoadPage(req, 403, HTTP_CT_HTML, "status/403.html");
				else xsHttpReplyAuto(req, 403, HTTP_CT_JSON, "{\"code\":403,\"msg\":\"forbidden\"}", 0);
				goto done;
			}
		} else {
			if (route->bAdmin) {
				char* headers = xrtFormat("Content-Type: text/plain\r\nLocation: %s\r\n", Option_GetAdminLoginPath());
				xsHttpReplyAuto(req, 302, headers, "", 0);
				xrtFree(headers);
			}
			else xsHttpReplyAuto(req, 401, HTTP_CT_JSON, "{\"code\":401,\"msg\":\"unauthorized\"}", 0);
			goto done;
		}
		if (route->bActive) {
			if (route->bAdmin) Session_ExtendAdmin(session); else Session_ExtendMember(session);
		}
	}
	for (i = 0; i < 10; i++) if (xsReqMethodID(req) == G_Methods[i]) proc = route->Proc[i];
	if (proc) proc(req->raw->server, req->raw->host, req, req, session);
	else RouteHTTP_Reply405(req, route);
done:
	xrtValueRelease(session);
}

XS_RequestResult RequestProc(XS_HttpReq* raw)
{
	XAdminRequest req = {0}; char* target; char* query; RouteInfo route; RouteInfo* found;
	bool alias; const char* lookup; xnetaddr addr; xnetstream* stream;
	if (!G_Ready && !G_InstallMode) { ReplyText(raw, 503, "xadmin initialization failed"); return XS_OK; }
	req.raw = raw;
	target = xrtStrDupView(raw->head->Target);
	if (!target) { ReplyText(raw, 500, "out of memory"); return XS_OK; }
	query = strchr(target, '?');
	if (query) *query++ = 0;
	req.path = target; req.query = query ? query : "";
	snprintf(req.method, sizeof(req.method), "%.*s", (int)raw->head->Method.Size, raw->head->Method.Data);
	stream = raw->tcp ? raw->tcp : xrtTlsStreamTransport(raw->tls);
	if (xrtNetStreamRemote(stream, &addr)) xrtNetAddrText(&addr, req.remote, sizeof(req.remote));
	req.body = ReqBodyText(raw, &req.body_size);
	xrtMutexLock(G_RequestLock);
	if (G_InstallMode) {
		/* 安装向导接管（v1 http.h 语义）：建库+业务段启动在锁内完成 */
		Install_RequestWizard(raw->host, &req);
		xrtMutexUnlock(G_RequestLock);
		xrtFree(req.body); xrtFree(target);
		return XS_OK;
	}
	alias = Option_AdminEntryIsMatch(req.path);
	lookup = alias ? "/admin/login" : req.path;
	found = RouteHTTP_Match(lookup, &req);
	if (found) {
		route = *found; XAdmin_Dispatch(&req, &route, alias);
		if (!req.replied) xsHttpReplyAuto(&req, 500, HTTP_CT_TEXT, "handler did not produce a response", 0);
	}
	xrtMutexUnlock(G_RequestLock);
	if (!found) {
		/* 路由未命中：独立页面按 URI 兜底，其次插件静态资源（均免鉴权）。
		 * req.path 指向 target，须在两处兜底都放弃后才能释放。 */
		bool handled = StandalonePage_Dispatch(req.path, &req)
			|| Plugin_TryServeStatic(req.path, &req);
		xrtFree(req.body); xrtFree(target);
		return handled ? XS_OK : XS_FALLBACK;
	}
	xrtFree(req.body); xrtFree(target);
	return XS_OK;
}
