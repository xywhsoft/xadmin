static xvalue* Billing_AccountValue(int64_t owner)
{
    XBillingAccount account={sizeof(account)};XBillingEntitlement e={sizeof(e)};
    if(!Billing_Maintain() || !Billing_Periods() || Billing_ReadAccount(owner,&account) || Billing_Entitlement(owner,&e))return NULL;
    xvalue* data=ValueObject();ValueSetInt(data,"member_id",owner);ValueSetText(data,"currency","CNY");ValueSetInt(data,"amount_scale",1000000);
    ValueSetInt(data,"cash_micros",account.cash);ValueSetInt(data,"cash_reserved_micros",account.cash_reserved);
    ValueSetInt(data,"credit_micros",account.credit);ValueSetInt(data,"credit_reserved_micros",account.credit_reserved);
    ValueSetInt(data,"available_micros",account.cash-account.cash_reserved+account.credit-account.credit_reserved);
    ValueSetText(data,"plan_id",e.plan_id);ValueSetInt(data,"plan_expires_at",e.expires_at);ValueSetInt(data,"discount_bps",e.discount_bps);ValueSetInt(data,"concurrency_limit",e.concurrency_limit);return data;
}
static void Billing_Public(XS_ServerObject server,XS_HostObject host,XS_RequestObject req,XS_ResponseObject resp,xvalue* session)
{
    (void)server;(void)host;if(!XP_Member(req,resp,session,XHTTP_METHOD_GET))return;
    int64_t owner=ValueInt(session,"id");const char* path=XAdmin_ReqPath(req);xvalue* data=NULL;sqlite3_stmt* s;
    if(!strcmp(path,"/api/v1/billing/account"))data=Billing_AccountValue(owner);
    else{
        const char* sql=!strcmp(path,"/api/v1/billing/transactions")?
            "SELECT id,kind,amount AS amount_micros,source,request_id,reason,created_at FROM ledger WHERE member_id=? ORDER BY id DESC LIMIT 100":
            !strcmp(path,"/api/v1/billing/subscriptions")?
            "SELECT id,plan_id,title,starts_at,expires_at,cancelled_at FROM subscription WHERE member_id=? ORDER BY id DESC LIMIT 100":
            !strcmp(path,"/api/v1/billing/credits")?
            "SELECT id,balance AS balance_micros,reserved AS reserved_micros,expires_at,source FROM credit WHERE member_id=? ORDER BY id DESC LIMIT 100":
            "SELECT id,title,duration_seconds,period_seconds,credit_amount AS credit_micros,discount_bps,concurrency_limit,model_ids FROM plan WHERE enabled=1 ORDER BY id";
        s=XP_SQL(G_DB,sql);if(s && strcmp(path,"/api/v1/billing/plans"))sqlite3_bind_int64(s,1,owner);data=XP_Rows(s);
    }
    XP_Reply(resp,data?200:503,data?"":"Billing unavailable",data);
}
static bool Billing_AdminOwner(const xvalue* body,int64_t* owner)
{return XP_Int(body,"member_id",1,INT64_MAX,owner) && Billing_Ensure(*owner);}
static void Billing_AdminAPI(XS_ServerObject server,XS_HostObject host,XS_RequestObject req,XS_ResponseObject resp,xvalue* session)
{
    (void)server;(void)host;const char* path=XAdmin_ReqPath(req);
    if(xsReqMethodID(req)==XHTTP_METHOD_GET){
        xvalue* data=ValueObject();const char* csrf=XAdmin_AdminCSRFToken(session);if(!csrf){xrtValueRelease(data);XP_Reply(resp,503,"CSRF unavailable",NULL);return;}
        ValueSetText(data,"csrf_token",csrf);sqlite3_stmt* s=NULL;char owner_text[32];int64_t owner=0;
        if(xsReqQueryValue(req,"member_id",owner_text,sizeof(owner_text))>0 && !XP_Positive(owner_text,&owner)){xrtValueRelease(data);XP_Reply(resp,400,"Invalid member",NULL);return;}
        if(!Billing_Maintain() || !Billing_Periods()){xrtValueRelease(data);XP_Reply(resp,503,"Billing maintenance failed",NULL);return;}
        if(owner){xvalue* account=Billing_AccountValue(owner);if(!account){xrtValueRelease(data);XP_Reply(resp,404,"Member unavailable",NULL);return;}ValueSetOwn(data,"account",account);}
        ValueSetOwn(data,"plans",XP_Rows(XP_SQL(G_DB,"SELECT *,credit_amount AS credit_micros FROM plan ORDER BY id")));
        s=XP_SQL(G_DB,owner?"SELECT * FROM ledger WHERE member_id=? ORDER BY id DESC LIMIT 100":"SELECT * FROM ledger ORDER BY id DESC LIMIT 100");if(s&&owner)sqlite3_bind_int64(s,1,owner);ValueSetOwn(data,"transactions",XP_Rows(s));
        s=XP_SQL(G_DB,owner?"SELECT * FROM reservation WHERE member_id=? ORDER BY created_at DESC LIMIT 100":"SELECT * FROM reservation ORDER BY created_at DESC LIMIT 100");if(s&&owner)sqlite3_bind_int64(s,1,owner);ValueSetOwn(data,"requests",XP_Rows(s));
        s=XP_SQL(G_DB,owner?"SELECT * FROM subscription WHERE member_id=? ORDER BY id DESC LIMIT 100":"SELECT * FROM subscription ORDER BY id DESC LIMIT 100");if(s&&owner)sqlite3_bind_int64(s,1,owner);ValueSetOwn(data,"subscriptions",XP_Rows(s));
        XP_Reply(resp,200,"",data);return;
    }
    if(xsReqMethodID(req)!=XHTTP_METHOD_POST){XP_Reply(resp,405,"Method not allowed",NULL);return;}
    if(!XAdmin_CheckAdminCSRF(req,session)){XP_Reply(resp,403,"CSRF verification failed",NULL);return;}
    xvalue* body=XP_Body(req,8192);const char* key=XP_Text(body,"operation_id",96),*reason=XP_Text(body,"reason",256);
    const char* actor=ValueText(session,"user");if(!actor)actor="administrator";int status=400;int64_t owner,amount,expires;
    if(!strcmp(path,"/admin/billing/plan"))status=Billing_SavePlan(body);
    else if(!strcmp(path,"/admin/billing/adjust")){
        const char* names[]={"member_id","amount_micros","operation_id","reason"};
        if(XP_Fields(body,names,4) && Billing_AdminOwner(body,&owner) && XP_Int(body,"amount_micros",-XBILL_MAX_AMOUNT,XBILL_MAX_AMOUNT,&amount) && reason)
            status=Billing_AdjustCash(owner,amount,key,reason,actor);
    }else if(!strcmp(path,"/admin/billing/grant")){
        const char* names[]={"member_id","amount_micros","expires_at","operation_id","reason"};
        if(XP_Fields(body,names,5) && Billing_AdminOwner(body,&owner) && XP_Int(body,"amount_micros",1,XBILL_MAX_AMOUNT,&amount) && XP_Int(body,"expires_at",0,INT64_MAX,&expires) && reason)
            status=Billing_Grant(owner,amount,expires,reason,key,actor);
    }else if(!strcmp(path,"/admin/billing/subscribe")){
        const char* names[]={"member_id","plan_id","operation_id"};
        if(XP_Fields(body,names,3) && Billing_AdminOwner(body,&owner))status=Billing_Subscribe(owner,XP_Text(body,"plan_id",64),key,actor);
    }else if(!strcmp(path,"/admin/billing/cancel")){
        const char* names[]={"member_id","subscription_id"};int64_t id;
        if(XP_Fields(body,names,2) && Billing_AdminOwner(body,&owner) && XP_Int(body,"subscription_id",1,INT64_MAX,&id))status=Billing_Cancel(owner,id);
    }else if(!strcmp(path,"/admin/billing/refund")){
        const char* names[]={"request_id","operation_id","reason"};
        if(XP_Fields(body,names,3) && reason)status=Billing_Refund(XP_Text(body,"request_id",96),key,reason,actor);
    }else if(!strcmp(path,"/admin/billing/resolve")){
        const char* names[]={"request_id","amount_micros","reason"};const char* id=XP_Text(body,"request_id",96);
        XBillingResult r={sizeof(r)};
        if(XP_Fields(body,names,3) && reason && XP_Int(body,"amount_micros",0,XBILL_MAX_AMOUNT,&amount) && !Billing_Lookup(id,&r) && !strcmp(r.state,"pending")){
            xvalue* evidence=ValueObject();ValueSetText(evidence,"source","administrator_reconciliation");ValueSetText(evidence,"reason",reason);ValueSetText(evidence,"actor",actor);
            char* json=xrtJsonStringify(evidence,false,NULL);XBillingSettlement settlement={sizeof(settlement),id,amount,json,amount?"settled":"released"};
            status=Billing_Finalize(&settlement);xrtFree(json);xrtValueRelease(evidence);
        }
    }
    xrtValueRelease(body);XP_Reply(resp,status?status:200,status?"Billing operation rejected":"Saved",NULL);
}
static void Billing_Page(XS_ServerObject s,XS_HostObject h,XS_RequestObject req,XS_ResponseObject resp,xvalue* session)
{
    (void)s;(void)h;(void)session;if(xsReqMethodID(req)!=XHTTP_METHOD_GET){XP_Reply(resp,405,"Method not allowed",NULL);return;}
    XAdmin_LoadPluginPage(G_Handle,resp,200,"Content-Type: text/html; charset=utf-8\r\nCache-Control: no-store\r\n",
        !strcmp(XAdmin_ReqPath(req),"/admin/billing")?"admin.html":"account.html");
}
