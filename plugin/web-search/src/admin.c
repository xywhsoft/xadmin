static void Search_Admin(XS_ServerObject server, XS_HostObject host, XS_RequestObject req, XS_ResponseObject resp, xvalue* session)
{
    (void)server; (void)host; (void)session;
    if (xsReqMethodID(req) != XHTTP_METHOD_GET) { Search_Reply(resp,405,"Method not allowed",NULL); return; }
    XAdmin_LoadPluginPage(G_Handle,resp,200,"Content-Type: text/html; charset=utf-8\r\nCache-Control: no-store\r\n","settings.html");
}
static void Search_Credentials(XS_ServerObject server, XS_HostObject host, XS_RequestObject req, XS_ResponseObject resp, xvalue* session)
{
    (void)server; (void)host;
    if (xsReqMethodID(req) == XHTTP_METHOD_GET) {
        const char* csrf = XAdmin_AdminCSRFToken(session);
        if (!csrf) { Search_Reply(resp,503,"CSRF token unavailable",NULL); return; }
        bool invalid; xvalue* credentials = Search_ReadCredentials(&invalid);
        xvalue* data = ValueObject(); ValueSetText(data,"csrf_token",csrf); ValueSetBool(data,"file_invalid",invalid);
        const char* providers[] = {"bocha","zai"}; size_t i;
        for (i = 0; i < 2; i++) {
            xvalue* state = ValueObject(); const char* stored = Search_Text(credentials,providers[i],1000);
            const char* env = getenv(i==0?"BOCHA_API_KEY":"ZAI_API_KEY"); char key[1001];
            ValueSetBool(state,"configured",Search_Key(providers[i],key)); xrtSecureZero(key,sizeof(key));
            ValueSetBool(state,"stored",stored && *stored); ValueSetBool(state,"environment",env && *env);
            ValueSetOwn(data,providers[i],state);
        }
        Search_WipeCredentials(credentials); Search_Reply(resp,200,"",data); return;
    }
    if (xsReqMethodID(req) != XHTTP_METHOD_POST) { Search_Reply(resp,405,"Method not allowed",NULL); return; }
    if (!XAdmin_CheckAdminCSRF(req,session)) { Search_Reply(resp,403,"CSRF verification failed",NULL); return; }
    xvalue* patch = Search_Body(req,4096); const char* fields[] = {"bocha","zai"};
    bool ok = Search_Fields(patch,fields,2); size_t i;
    for (i = 0; ok && i < 2; i++) if (ValueHas(patch,fields[i])) ok = Search_ValidKey(Search_Text(patch,fields[i],1000));
    if (!ok) { Search_WipeCredentials(patch); Search_Reply(resp,400,"Invalid credentials",NULL); return; }
    bool invalid; xvalue* current = Search_ReadCredentials(&invalid);
    /* A corrupt file can only be replaced by supplying both fields explicitly. */
    if (invalid && !(ValueHas(patch,"bocha") && ValueHas(patch,"zai"))) {
        Search_WipeCredentials(patch); Search_Reply(resp,409,"Replace both provider keys to repair invalid credential file",NULL); return;
    }
    if (!current) current = ValueObject();
    for (i = 0; ok && i < 2; i++) if (ValueHas(patch,fields[i])) {
        const char* old = ValueText(current,fields[i]); if (old) xrtSecureZero((void*)old,strlen(old));
        ok = ValueSetText(current,fields[i],Search_Text(patch,fields[i],1000));
    }
    size_t n = 0; char* bytes = ok ? xrtJsonStringify(current,false,&n) : NULL;
    ok = bytes && xrtFileWriteAtomic(G_CredentialPath,(xbytesview){(cbytes)bytes,n});
#if !defined(_WIN32) && !defined(_WIN64)
    if (ok) ok = xrtPathSetMode(G_CredentialPath,false,0600);
#endif
    if (bytes) xrtSecureZero(bytes,n); xrtFree(bytes);
    Search_WipeCredentials(current); Search_WipeCredentials(patch);
    Search_Reply(resp,ok?200:503,ok?"Credentials saved":"Unable to save credentials",NULL);
}
