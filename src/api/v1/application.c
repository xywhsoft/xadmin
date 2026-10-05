static void XA_ApplicationBegin(XAdminRequest* req, xvalue* query)
{
    const char* const fields[] = {"client_id", "redirect_uri", "response_type", "state", "code_challenge", "code_challenge_method"};
    const char* client = XA_Text(query, "client_id", 64);
    const char* redirect = XA_Text(query, "redirect_uri", 512);
    const char* state = XA_Text(query, "state", 128);
    const char* challenge = XA_Text(query, "code_challenge", 43);
    const char* method = XA_Text(query, "code_challenge_method", 8);
    const char* response = XA_Text(query, "response_type", 8);
    const XAApplicationConfig* app = XA_ApplicationFind(client);
    char id[65], cookie[65], hash[65];
    sqlite3_stmt* s;
    /* Invalid redirect requests never redirect, even for a known client. */
    if (!query || !XA_ConfigFields(query, fields, 6) || !XA_ApplicationRedirect(app, redirect) ||
        !XA_ApplicationAscii(state, 32, 128) || !XA_ApplicationAscii(challenge, 43, 43) ||
        strchr(challenge ? challenge : "", '.') || strchr(challenge ? challenge : "", '~') ||
        !method || strcmp(method, "S256") || !response || strcmp(response, "code")) {
        XA_Reply(req, 400, "invalid application authorization request", NULL, NULL); return;
    }
    s = XA_SQL("DELETE FROM identity_application WHERE expires_at<?");
    if (s) sqlite3_bind_int64(s, 1, XA_Now() - 86400);
    if (!XA_Done(s, false)) { XA_Reply(req, 503, "application authorization unavailable", NULL, NULL); return; }
    s = XA_SQL("SELECT count(*),sum(CASE WHEN remote=? THEN 1 ELSE 0 END) FROM identity_application WHERE expires_at>? AND consumed_at=0");
    XA_BindText(s, 1, req->remote); if (s) sqlite3_bind_int64(s, 2, XA_Now());
    bool capacity = s && sqlite3_step(s) == SQLITE_ROW && sqlite3_column_int(s, 0) < 128 && sqlite3_column_int(s, 1) < 8;
    sqlite3_finalize(s);
    if (!capacity) { XA_Reply(req, 429, "too many application login attempts", NULL, NULL); return; }
    if (!XA_Random(id) || !XA_Random(cookie) || !XA_Hash(cookie, hash)) {
        XA_Reply(req, 503, "application authorization unavailable", NULL, NULL); return;
    }
    s = XA_SQL("INSERT INTO identity_application(request_id,client_id,redirect_uri,state,challenge,browser_hash,remote,created_at,expires_at)VALUES(?,?,?,?,?,?,?,?,?)");
    XA_BindText(s, 1, id); XA_BindText(s, 2, client); XA_BindText(s, 3, redirect); XA_BindText(s, 4, state);
    XA_BindText(s, 5, challenge); XA_BindText(s, 6, hash); XA_BindText(s, 7, req->remote);
    if (s) { sqlite3_bind_int64(s, 8, XA_Now()); sqlite3_bind_int64(s, 9, XA_Now() + 600); }
    if (!XA_Done(s, true)) { XA_Reply(req, 503, "application authorization unavailable", NULL, NULL); return; }
    char* headers = xrtFormat("Location: /account/index.html?application=%s\r\nCache-Control: no-store\r\nReferrer-Policy: no-referrer\r\nSet-Cookie: XAPP=%s; Path=/api/v1/auth/; HttpOnly; SameSite=Lax; Max-Age=600%s\r\n",
        id, cookie, (G_Identity.secure_cookie || req->raw->tls) ? "; Secure" : "");
    if (headers) xsHttpReplyAuto(req, 303, headers, "", 0);
    else XA_Reply(req, 503, "application authorization unavailable", NULL, NULL);
    xrtFree(headers); xrtSecureZero(cookie, sizeof(cookie));
}
static void XA_ApplicationContext(XAdminRequest* req, const char* id, xvalue* session)
{
    XAApplicationRequest pending;
    if (!XA_IsHex(id, 64) || !XA_ApplicationRead(id, NULL, &pending) ||
        pending.expires <= XA_Now() || pending.decision || pending.consumed || !XA_ApplicationBrowser(req, &pending)) {
        XA_Reply(req, 410, "application login expired or cancelled", NULL, NULL); return;
    }
    const XAApplicationConfig* app = XA_ApplicationFind(pending.client);
    if (!XA_ApplicationRedirect(app, pending.redirect)) { XA_Reply(req, 410, "application is unavailable", NULL, NULL); return; }
    xvalue* data = ValueObject();
    bool ok = data && ValueSetText(data, "client_id", app->id) && ValueSetText(data, "name", app->name) &&
        ValueSetInt(data, "expires_in", pending.expires - XA_Now()) &&
        ValueSetBool(data, "signed_in", xrtValueType(session) == XVALUE_OBJECT);
    XA_Reply(req, ok ? 200 : 500, ok ? "success" : "application authorization unavailable", ok ? data : NULL, NULL);
    xrtValueRelease(data);
}
static void XA_ApplicationDecision(XAdminRequest* req, xvalue* body, xvalue* session)
{
    const char* const fields[] = {"request_id", "approve"};
    const char* id = XA_Text(body, "request_id", 64);
    bool approve = false;
    XAApplicationRequest pending;
    char code[65] = {0}, hash[65] = {0};
    if (!body || !XA_ConfigFields(body, fields, 2) || !xrtValueGetBool(ValueGet(body, "approve"), &approve) || !XA_IsHex(id, 64)) {
        XA_Reply(req, 400, "invalid application decision", NULL, NULL); return;
    }
    if (!XA_ApplicationRead(id, NULL, &pending) || pending.expires <= XA_Now() || pending.decision ||
        pending.consumed || !XA_ApplicationBrowser(req, &pending) ||
        !XA_ApplicationRedirect(XA_ApplicationFind(pending.client), pending.redirect)) {
        XA_Reply(req, 410, "application login expired or cancelled", NULL, NULL); return;
    }
    if (approve && (xrtValueType(session) != XVALUE_OBJECT || !XA_RequestCSRF(req, session))) {
        XA_Reply(req, xrtValueType(session) == XVALUE_OBJECT ? 403 : 401, "member confirmation required", NULL, NULL); return;
    }
    if (approve && (!XA_Random(code) || !XA_Hash(code, hash))) { XA_Reply(req, 503, "application authorization unavailable", NULL, NULL); return; }
    char* redirect = XA_ApplicationReturn(&pending, approve ? code : NULL, "access_denied");
    xvalue* data = ValueObject();
    bool ok = redirect && data && ValueSetText(data, "redirect_uri", redirect);
    if (ok) {
        sqlite3_stmt* s = XA_SQL("UPDATE identity_application SET decision=?,source_sid=?,code_hash=?,expires_at=? WHERE request_id=? AND decision=0 AND consumed_at=0 AND expires_at>?");
        if (s) { sqlite3_bind_int(s, 1, approve ? 1 : 2); sqlite3_bind_int64(s, 4, XA_Now() + 60); sqlite3_bind_int64(s, 6, XA_Now()); }
        XA_BindText(s, 2, approve ? ValueText(session, "sid") : ""); XA_BindText(s, 3, hash); XA_BindText(s, 5, id);
        ok = XA_Done(s, true);
    }
    XA_Reply(req, ok ? 200 : 503, ok ? "application decision saved" : "application authorization unavailable", ok ? data : NULL, NULL);
    xrtFree(redirect); xrtValueRelease(data); xrtSecureZero(code, sizeof(code));
}
static void XA_ApplicationToken(XAdminRequest* req, xvalue* body)
{
    const char* const fields[] = {"grant_type", "client_id", "redirect_uri", "code", "code_verifier"};
    const char* type = XA_Text(body, "grant_type", 32);
    const char* client = XA_Text(body, "client_id", 64);
    const char* redirect = XA_Text(body, "redirect_uri", 512);
    const char* code = XA_Text(body, "code", 64);
    const char* verifier = XA_Text(body, "code_verifier", 128);
    char hash[65], challenge[44]; XAApplicationRequest pending; XATokenSet tokens = {0};
    if (!body || !XA_ConfigFields(body, fields, 5) || !type || strcmp(type, "authorization_code") ||
        !XA_IsHex(code, 64) || !XA_Hash(code, hash) || !XA_ApplicationChallenge(verifier, challenge) ||
        !XA_ApplicationRedirect(XA_ApplicationFind(client), redirect)) {
        XA_Reply(req, 400, "invalid application token request", NULL, NULL); return;
    }
    if (!XA_ApplicationRead(NULL, hash, &pending) || strcmp(pending.client, client) || strcmp(pending.redirect, redirect) ||
        pending.expires <= XA_Now() || pending.decision != 1 || pending.consumed ||
        !xrtConstTimeEqual(challenge, pending.challenge, 43)) {
        XA_Reply(req, 401, "application code invalid or expired", NULL, NULL); return;
    }
    xvalue* source = XA_SessionRead(pending.sid, NULL);
    if (!source) { XA_Reply(req, 401, "authorization session revoked", NULL, NULL); return; }
    int64 owner = ValueInt(source, "id");
    int64 proof_until = ValueInt(source, "reauth_until"),mfa_version=ValueInt(source,"mfa_version"),mfa_verified=ValueInt(source,"mfa_verified_at"); xrtValueRelease(source);
    /* SessionIssue has its own savepoint. The outer one makes code consumption
     * and issuance atomic, including rollback on response allocation failure. */
    if (sqlite3_exec(G_DB, "SAVEPOINT xa_application", NULL, NULL, NULL) != SQLITE_OK) {
        XA_Reply(req, 503, "application authorization unavailable", NULL, NULL); return;
    }
    sqlite3_stmt* s = XA_SQL("UPDATE identity_application SET consumed_at=? WHERE request_id=? AND consumed_at=0 AND decision=1 AND expires_at>?");
    if (s) { sqlite3_bind_int64(s, 1, XA_Now()); sqlite3_bind_int64(s, 3, XA_Now()); } XA_BindText(s, 2, pending.id);
    bool ok = XA_Done(s, true) && XA_SessionIssueVerified(req, owner, &tokens,mfa_version,mfa_verified);
    if (ok) {
        /* An old browser cookie does not establish fresh identity proof. */
        s = XA_SQL("UPDATE member_session SET reauth_until=? WHERE sid=?");
        if (s) sqlite3_bind_int64(s, 1, proof_until > XA_Now() ? proof_until : 0);
        XA_BindText(s, 2, tokens.sid); ok = XA_Done(s, true);
    }
    xvalue* data = ok ? XA_TokenData(&tokens) : NULL;
    ok = ok && data && sqlite3_exec(G_DB, "RELEASE xa_application", NULL, NULL, NULL) == SQLITE_OK;
    if (!ok) { sqlite3_exec(G_DB, "ROLLBACK TO xa_application", NULL, NULL, NULL); sqlite3_exec(G_DB, "RELEASE xa_application", NULL, NULL, NULL); }
    /* Native sessions are independent of the browser session; no cookie is
     * returned and cancelling/logging out of mdo never logs out the browser. */
    XA_Reply(req, ok ? 200 : 503, ok ? "signed in" : "application authorization unavailable", ok ? data : NULL, NULL);
    xrtValueRelease(data); XA_TokensUnit(&tokens);
}
static void XA_ApplicationHandler(XS_ServerObject server, XS_HostObject host,
    XAdminRequest* req, XAdminRequest* resp, xvalue* session)
{
    (void)server; (void)host; (void)resp;
    int method = xsReqMethodID(req);
    bool token = !strcmp(req->path, "/api/v1/auth/token");
    if (token ? method != XHTTP_METHOD_POST : (method != XHTTP_METHOD_GET && method != XHTTP_METHOD_POST)) {
        XA_Reply(req, 405, "method not allowed", NULL, NULL); return;
    }
    if (method == XHTTP_METHOD_POST && !XA_SameOrigin(req)) { XA_Reply(req, 403, "CSRF verification failed", NULL, NULL); return; }
    xvalue* value = method == XHTTP_METHOD_GET ? XA_ApplicationQuery(req->query) : XA_Body(req);
    if (!value) { XA_Reply(req, 400, "invalid application request", NULL, NULL); return; }
    if (token) XA_ApplicationToken(req, value);
    else if (method == XHTTP_METHOD_POST) XA_ApplicationDecision(req, value, session);
    else if (ValueHas(value, "request_id")) {
        const char* const fields[] = {"request_id"};
        if (!XA_ConfigFields(value, fields, 1)) XA_Reply(req, 400, "invalid application request", NULL, NULL);
        else XA_ApplicationContext(req, XA_Text(value, "request_id", 64), session);
    } else XA_ApplicationBegin(req, value);
    xrtValueRelease(value);
}
static void XA_ApplicationRegisterRoutes(void)
{
    const char* paths[] = {"/api/v1/auth/authorize", "/api/v1/auth/token"}; size_t i;
    for (i = 0; i < 2; i++) {
        RouteInfo* route = AddStaticRouteHTTP(paths[i], XHTTP_METHOD_ANY, XA_ApplicationHandler, true);
        if (route) { route->bAdmin = false; route->bAuth = false; }
    }
}
