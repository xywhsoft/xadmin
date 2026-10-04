/* These handlers use the same member/admin realms, CSRF and RBAC as the
 * rest of the identity service. Never return answers, hashes or answer hints. */
static void XA_SecurityProfile(XAdminRequest* req,xvalue* session,xvalue* body)
{
    int64 owner=ValueInt(session,"id");XASecuritySnapshot before;int found=XA_SecurityRead(owner,&before);
    if(found<0){XA_Reply(req,500,"security questions unavailable",NULL,NULL);return;}
    if(xsReqMethodID(req)==XHTTP_METHOD_GET){
        xvalue* data=ValueObject(),*questions=ValueArray();int i;
        ValueSetBool(data,"configured",found==1);ValueSetInt(data,"updated_at",before.updated);
        for(i=0;found==1&&i<3;i++)ValueArrayOwn(questions,xrtValueInt(before.question[i]));
        ValueSetOwn(data,"questions",questions);ValueSetText(data,"recovery_mode","administrator_review");
        XA_Reply(req,200,"success",data,NULL);xrtValueRelease(data);xrtSecureZero(&before,sizeof(before));return;
    }
    if(!G_Identity.security_questions&&xsReqMethodID(req)!=XHTTP_METHOD_DELETE){XA_Reply(req,403,"security questions are disabled",NULL,NULL);xrtSecureZero(&before,sizeof(before));return;}
    if(!XA_Recent(session)){XA_Reply(req,403,"confirm your identity again",NULL,NULL);xrtSecureZero(&before,sizeof(before));return;}
    bool removing=xsReqMethodID(req)==XHTTP_METHOD_DELETE;XASecurityAnswers answers={0};
    const char* const fields[]={"question1","answer1","question2","answer2","question3","answer3"};
    if(!XA_ConfigFields(body,fields,removing?0:6)||(!removing&&!XA_SecurityParse(body,&answers))){
        XA_Reply(req,400,"choose three different questions and distinct 4-128 byte answers",NULL,NULL);xrtSecureZero(&answers,sizeof(answers));xrtSecureZero(&before,sizeof(before));return;
    }
    char records[3][257]={{0}},revision[65]={0};bool ok=true;int i;
    if(!removing){
        char key[32];snprintf(key,sizeof(key),"%lld",(long long)owner);
        int rate=XA_Rate("security-setup",key,6,3600);
        if(rate){XA_Reply(req,rate,"security question changes are too frequent",NULL,NULL);goto cleanup;}
        xrtMutexUnlock(G_RequestLock);
        for(i=0;i<3;i++)if(!XA_PasswordHash(answers.answer[i],records[i]))ok=false;
        xrtMutexLock(G_RequestLock);ok=ok&&XA_Random(revision);
    }
    if(!ok){XA_Reply(req,500,"security questions unavailable",NULL,NULL);goto cleanup;}
    if(!XA_SessionStillValid(session,true)||!XA_SecurityCurrent(owner,&before,found)){
        XA_Reply(req,409,"account or session changed; retry",NULL,NULL);goto cleanup;
    }
    if(!XA_Begin()){XA_Reply(req,500,"security questions unavailable",NULL,NULL);goto cleanup;}
    sqlite3_stmt* s;
    if(removing){
        s=XA_SQL("DELETE FROM member_security_question WHERE member_id=?");if(s)sqlite3_bind_int64(s,1,owner);ok=XA_Done(s,false);
    }else{
        s=XA_SQL("INSERT INTO member_security_question(member_id,revision,q1,a1,q2,a2,q3,a3,updated_at)VALUES(?,?,?,?,?,?,?,?,?) ON CONFLICT(member_id)DO UPDATE SET revision=excluded.revision,q1=excluded.q1,a1=excluded.a1,q2=excluded.q2,a2=excluded.a2,q3=excluded.q3,a3=excluded.a3,updated_at=excluded.updated_at");
        if(s){sqlite3_bind_int64(s,1,owner);sqlite3_bind_int64(s,9,XA_Now());}
        XA_BindText(s,2,revision);
        for(i=0;i<3;i++){if(s)sqlite3_bind_int(s,3+i*2,answers.question[i]);XA_BindText(s,4+i*2,records[i]);}
        ok=XA_Done(s,true);
    }
    ok=ok&&XA_SecurityClose(owner,"questions_changed",NULL,NULL)&&XA_SessionRevokeAccount(owner,ValueText(session,"sid"));
    ok=XA_End(ok);XA_Reply(req,ok?200:500,ok?"security questions updated":"security questions unavailable",NULL,NULL);
cleanup:
    xrtSecureZero(&answers,sizeof(answers));xrtSecureZero(records,sizeof(records));xrtSecureZero(&before,sizeof(before));
}
static void XA_SecurityRecovery(XAdminRequest* req,xvalue* body)
{
    if(!G_Identity.security_questions){XA_Reply(req,403,"security questions are disabled",NULL,NULL);return;}
    const char* const fields[]={"identifier","question1","answer1","question2","answer2","question3","answer3"};
    const char* identifier=XA_Text(body,"identifier",254);XAIdentifier parsed;XASecurityAnswers answers={0};
    if(!XA_ConfigFields(body,fields,7)||!identifier||!XA_IdentifierParse(identifier,strlen(identifier),G_Identity.country,&parsed)||!XA_SecurityParse(body,&answers)){
        XA_Reply(req,400,"invalid identifier or security question fields",NULL,NULL);xrtSecureZero(&answers,sizeof(answers));return;
    }
    int rate=XA_Rate("security-recovery-ip",req->remote,20,3600);
    if(!rate)rate=XA_Rate("security-recovery-target",parsed.key,5,3600);
    if(rate){XA_Reply(req,rate,"recovery requests are too frequent",NULL,NULL);xrtSecureZero(&answers,sizeof(answers));return;}
    XAAccount account={0};XASecuritySnapshot snapshot={0};
    bool owner=XA_AccountByIdentifier(&parsed,&account)&&account.status==1;
    int found=owner?XA_SecurityRead(account.id,&snapshot):0;
    if(found<0){XA_Reply(req,500,"recovery unavailable",NULL,NULL);goto cleanup;}
    bool verified=owner&&found==1;int i;
    /* Always perform all three KDFs, including absent/disabled accounts, and
     * return an identical response. Public callers cannot inspect tickets. */
    xrtMutexUnlock(G_RequestLock);
    for(i=0;i<3;i++){
        bool match;int index=i,j;XAAccount answerAccount={0};char dummy[257]={0};
        /* Match by question ID, so users need not remember their setup order.
         * Unknown IDs still execute a full check but cannot count as correct. */
        for(j=0;j<3;j++)if(snapshot.question[j]==answers.question[i])index=j;
        if(found==1){strcpy(answerAccount.password,snapshot.record[index]);match=XA_PasswordCheck(&answerAccount,answers.answer[i]);}
        else {XA_PasswordHash(answers.answer[i],dummy);match=false;}
        verified=match&&snapshot.question[index]==answers.question[i]&&verified;
        xrtSecureZero(&answerAccount,sizeof(answerAccount));xrtSecureZero(dummy,sizeof(dummy));
    }
    xrtMutexLock(G_RequestLock);
    if(owner&&(!XA_PasswordCurrent(&account)||!XA_SecurityCurrent(account.id,&snapshot,found)))verified=false;
    bool ok=true;
    if(verified){
        char id[65];ok=XA_Random(id)&&XA_Begin();
        if(ok){
            sqlite3_stmt* s=XA_SQL("DELETE FROM member_security_recovery WHERE expires_at<?");
            if(s)sqlite3_bind_int64(s,1,XA_Now()-7*86400);ok=XA_Done(s,false);
            s=XA_SQL("SELECT count(*) FROM member_security_recovery");
            int count=s&&sqlite3_step(s)==SQLITE_ROW?sqlite3_column_int(s,0):-1;sqlite3_finalize(s);ok=ok&&count>=0&&count<10000;
            if(ok)ok=XA_SecurityClose(account.id,"superseded",NULL,NULL);
            if(ok){
                s=XA_SQL("INSERT INTO member_security_recovery(id,member_id,revision,created_at,expires_at)VALUES(?,?,?,?,?)");
                XA_BindText(s,1,id);XA_BindText(s,3,snapshot.revision);
                if(s){sqlite3_bind_int64(s,2,account.id);sqlite3_bind_int64(s,4,XA_Now());sqlite3_bind_int64(s,5,XA_Now()+86400);}
                ok=XA_Done(s,true);
            }
            ok=XA_End(ok);
        }
    }
    /* No successful-answer oracle even when the ticket store is temporarily
     * full. This is submission acknowledgement, never proof of account access. */
    if(!ok)printf("[xadmin][error] security recovery request could not be stored\n");
    XA_Reply(req,202,"if the details match, an administrator will review your recovery request",NULL,NULL);
cleanup:
    xrtSecureZero(&answers,sizeof(answers));xrtSecureZero(&snapshot,sizeof(snapshot));xrtSecureZero(&account,sizeof(account));
}
static void XA_SecurityHandler(XS_ServerObject server,XS_HostObject host,XAdminRequest* req,XAdminRequest* resp,xvalue* session)
{
    (void)server;(void)host;(void)resp;
    bool catalog=!strcmp(req->path,"/api/v1/auth/security-questions"),recovery=!strcmp(req->path,"/api/v1/auth/security-questions/recover");
    int method=xsReqMethodID(req);
    if((catalog&&method!=XHTTP_METHOD_GET)||(recovery&&method!=XHTTP_METHOD_POST)||
        (!catalog&&!recovery&&method!=XHTTP_METHOD_GET&&method!=XHTTP_METHOD_PUT&&method!=XHTTP_METHOD_DELETE)){
        XA_Reply(req,405,"method not allowed",NULL,NULL);return;
    }
    if(catalog){xvalue* data=XA_SecurityCatalog();XA_Reply(req,200,"success",data,NULL);xrtValueRelease(data);return;}
    if(!recovery&&xrtValueType(session)!=XVALUE_OBJECT){XA_Reply(req,401,"unauthorized",NULL,NULL);return;}
    if(method!=XHTTP_METHOD_GET&&(!XA_OriginAllowed(req)||(!recovery&&!XA_RequestCSRF(req,session)))){
        XA_Reply(req,403,"CSRF verification failed",NULL,NULL);return;
    }
    xvalue* body=method==XHTTP_METHOD_GET?NULL:XA_Body(req);
    if(method!=XHTTP_METHOD_GET&&!body){XA_Reply(req,400,"a bounded JSON object is required",NULL,NULL);return;}
    if(recovery)XA_SecurityRecovery(req,body);else XA_SecurityProfile(req,session,body);
    xrtValueRelease(body);
}

static void XA_AdminSecurity(XS_ServerObject server,XS_HostObject host,XAdminRequest* req,XAdminRequest* resp,xvalue* session)
{
    (void)server;(void)host;(void)resp;
    if(xrtValueType(session)!=XVALUE_OBJECT){XA_Reply(req,401,"administrator login required",NULL,NULL);return;}
    int method=xsReqMethodID(req);
    if(!strcmp(req->path,"/admin/view/member/user/security")){
        if(method==XHTTP_METHOD_GET)LoadPage(req,200,HTTP_CT_HTML,"member/security.html");
        else XA_Reply(req,405,"method not allowed",NULL,NULL);return;
    }
    if(method==XHTTP_METHOD_GET){
        char query[32];xsReqQueryValue(req,"id",query,sizeof(query));int64 owner=Util_ParseI64(query);
        XAAccount account;XASecuritySnapshot snapshot;int found;
        if(owner<=0||!XA_AccountByID(owner,&account)){XA_Reply(req,404,"member not found",NULL,NULL);return;}
        found=XA_SecurityRead(owner,&snapshot);
        if(found<0){XA_Reply(req,500,"security questions unavailable",NULL,NULL);return;}
        char csrf[65];
        if(!XA_IsHex(ValueText(session,"_identityCSRF"),64)&&(!XA_Random(csrf)||!ValueSetText(session,"_identityCSRF",csrf))){XA_Reply(req,500,"security questions unavailable",NULL,NULL);return;}
        xvalue* data=ValueObject(),*questions=ValueArray(),*requests=ValueArray();int i;
        ValueSetInt(data,"id",owner);ValueSetText(data,"username",account.username);ValueSetBool(data,"security_questions_configured",found==1);
        ValueSetInt(data,"security_questions_updated_at",snapshot.updated);ValueSetText(data,"csrf_token",ValueText(session,"_identityCSRF"));
        for(i=0;found==1&&i<3;i++)if(snapshot.question[i]>=1&&snapshot.question[i]<=XA_SECURITY_QUESTION_COUNT)
            ValueArrayOwn(questions,xrtValueString(xrtStrView(XA_SecurityQuestions[snapshot.question[i]-1])));
        ValueSetOwn(data,"questions",questions);
        sqlite3_stmt* s=XA_SQL("SELECT phone,email,phone_verified_at,email_verified_at FROM member WHERE id=?");if(s)sqlite3_bind_int64(s,1,owner);
        bool ok=s&&sqlite3_step(s)==SQLITE_ROW;
        if(ok){ValueSetText(data,"phone",(const char*)sqlite3_column_text(s,0));ValueSetText(data,"email",(const char*)sqlite3_column_text(s,1));
            ValueSetInt(data,"phone_verified_at",sqlite3_column_int64(s,2));ValueSetInt(data,"email_verified_at",sqlite3_column_int64(s,3));
            ValueSetBool(data,"phone_verified",sqlite3_column_int64(s,2)>0);ValueSetBool(data,"email_verified",sqlite3_column_int64(s,3)>0);}
        sqlite3_finalize(s);
        s=XA_SQL("SELECT id,created_at,expires_at,closed_at,admin_user,resolution,reason FROM member_security_recovery WHERE member_id=? ORDER BY created_at DESC,rowid DESC LIMIT 20");
        if(s)sqlite3_bind_int64(s,1,owner);int rc=SQLITE_ERROR;
        while(s&&(rc=sqlite3_step(s))==SQLITE_ROW){
            xvalue* item=ValueObject();ValueSetText(item,"id",(const char*)sqlite3_column_text(s,0));
            ValueSetInt(item,"created_at",sqlite3_column_int64(s,1));ValueSetInt(item,"expires_at",sqlite3_column_int64(s,2));ValueSetInt(item,"closed_at",sqlite3_column_int64(s,3));
            ValueSetText(item,"admin_user",(const char*)sqlite3_column_text(s,4));ValueSetText(item,"resolution",(const char*)sqlite3_column_text(s,5));ValueSetText(item,"reason",(const char*)sqlite3_column_text(s,6));ValueArrayOwn(requests,item);
        }
        sqlite3_finalize(s);ok=ok&&rc==SQLITE_DONE;ValueSetOwn(data,"requests",requests);
        XA_Reply(req,ok?200:500,ok?"success":"security questions unavailable",ok?data:NULL,NULL);xrtValueRelease(data);
        xrtSecureZero(&snapshot,sizeof(snapshot));xrtSecureZero(&account,sizeof(account));return;
    }
    if(method!=XHTTP_METHOD_POST){XA_Reply(req,405,"method not allowed",NULL,NULL);return;}
    if(!XA_AdminIdentityCSRF(req,session)){XA_Reply(req,403,"CSRF verification failed",NULL,NULL);return;}
    xvalue* body=XA_Body(req);
    if(!body){XA_Reply(req,400,"a bounded JSON object is required",NULL,NULL);return;}
    const char* const fields[]={"member_id","action","request_id","newPassword","reason","independently_verified"};
    int64 owner=0;const char* action=XA_Text(body,"action",32),*reason=XA_Text(body,"reason",256),*id=XA_Text(body,"request_id",64);
    bool clear=action&&!strcmp(action,"clear_questions"),reject=action&&!strcmp(action,"reject_recovery"),reset=action&&!strcmp(action,"reset_password"),confirmed=false;
    XAAccount account={0};char record[257]={0};XASecuritySnapshot snapshot={0};int found=0;
    if(!XA_ConfigFields(body,fields,6)||!xrtValueGetInt(ValueGet(body,"member_id"),&owner)||owner<=0||(!clear&&!reject&&!reset)||!reason||strlen(reason)<4){
        XA_Reply(req,400,"member, action and a review reason are required",NULL,NULL);goto cleanup;
    }
    if(!XA_AccountByID(owner,&account)){XA_Reply(req,404,"member not found",NULL,NULL);goto cleanup;}
    if((!clear&&!XA_IsHex(id,64))||(reset&&(!XA_ConfigBool(body,"independently_verified",&confirmed)||!confirmed||!XA_PasswordPolicy(XA_Text(body,"newPassword",128))))){
        XA_Reply(req,400,"independent identity verification and a new password are required",NULL,NULL);goto cleanup;
    }
    found=XA_SecurityRead(owner,&snapshot);if(found<0){XA_Reply(req,500,"security questions unavailable",NULL,NULL);goto cleanup;}
    if(reset){
        if(account.status!=1){XA_Reply(req,409,"disabled member cannot be recovered",NULL,NULL);goto cleanup;}
        xrtMutexUnlock(G_RequestLock);bool hashed=XA_PasswordHash(XA_Text(body,"newPassword",128),record);xrtMutexLock(G_RequestLock);
        if(!hashed||!XA_PasswordCurrent(&account)||!XA_SecurityCurrent(owner,&snapshot,found)){
            XA_Reply(req,409,"account changed; retry",NULL,NULL);goto cleanup;
        }
        /* Recheck the administrator after unlocked CPU work as well. */
        char cookie[129];xsReqCookieValue(req,"XSID",cookie,sizeof(cookie));xvalue* current=Session_Acquire(true,cookie);
        bool valid=xrtValueType(current)==XVALUE_OBJECT&&ValueInt(current,"id")==ValueInt(session,"id")&&XA_AdminIdentityCSRF(req,current);
        RouteInfo* route=RouteHTTP_Registered(req->path);
        valid=valid&&route&&Auth_DBRoleGetAccess(ValueInt(current,"roleID"),route->AuthID,NULL);
        xrtValueRelease(current);if(!valid){XA_Reply(req,401,"administrator session expired",NULL,NULL);goto cleanup;}
    }
    if(!XA_Begin()){XA_Reply(req,500,"security questions unavailable",NULL,NULL);goto cleanup;}
    sqlite3_stmt* s;bool ok;
    if(clear){
        s=XA_SQL("DELETE FROM member_security_question WHERE member_id=?");if(s)sqlite3_bind_int64(s,1,owner);ok=XA_Done(s,false);
    }else{
        s=XA_SQL("UPDATE member_security_recovery SET closed_at=?,admin_user=?,resolution=?,reason=? WHERE id=? AND member_id=? AND closed_at=0 AND expires_at>? AND revision=?");
        if(s){sqlite3_bind_int64(s,1,XA_Now());sqlite3_bind_int64(s,6,owner);sqlite3_bind_int64(s,7,XA_Now());}
        XA_BindText(s,2,ValueText(session,"user"));XA_BindText(s,3,reset?"password_reset":"rejected");XA_BindText(s,4,reason);XA_BindText(s,5,id);XA_BindText(s,8,snapshot.revision);ok=found==1&&XA_Done(s,true);
        if(found!=1)sqlite3_finalize(s);
        if(ok&&reset)ok=XA_SetPassword(owner,record);
    }
    if(ok)ok=XA_SecurityClose(owner,clear?"admin_cleared":reset?"password_reset":"rejected",ValueText(session,"user"),reason);
    if(ok&&(clear||reset))ok=XA_SessionRevokeAccount(owner,NULL);
    if(ok){
        s=XA_SQL("INSERT INTO member_security_review(member_id,request_id,action,admin_user,reason,created_at)VALUES(?,?,?,?,?,?)");
        if(s){sqlite3_bind_int64(s,1,owner);sqlite3_bind_int64(s,6,XA_Now());}
        XA_BindText(s,2,id?id:"");XA_BindText(s,3,action);XA_BindText(s,4,ValueText(session,"user"));XA_BindText(s,5,reason);ok=XA_Done(s,true);
    }
    ok=XA_End(ok);XA_Reply(req,ok?200:409,ok?"security review completed":"request expired, changed or update failed",NULL,NULL);
cleanup:
    xrtSecureZero(record,sizeof(record));xrtSecureZero(&snapshot,sizeof(snapshot));xrtSecureZero(&account,sizeof(account));xrtValueRelease(body);
}
static void XA_SecurityRegisterRoutes(void)
{
    const char* paths[]={"/api/v1/auth/security-questions","/api/v1/auth/security-questions/recover","/api/v1/profile/security-questions","/admin/view/member/user/security","/admin/member/user/security"};size_t i;
    for(i=0;i<5;i++){
        RouteInfo* r=AddStaticRouteHTTP(paths[i],XHTTP_METHOD_ANY,i<3?XA_SecurityHandler:XA_AdminSecurity,true);
        if(r){if(i<3){r->bAdmin=false;r->bAuth=false;}else r->bPutLog=true;}
    }
}
