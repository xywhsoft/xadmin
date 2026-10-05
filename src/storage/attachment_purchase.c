/* The wallet receipt survives host order failures. Both purchase retries and
 * downloads repair the order from that receipt, without a second payment. */
static const XBillingCashService* Attachment_CashService(XAdminServiceLease* lease)
{
    char authority[65],name[70];const void* table=NULL;*lease=NULL;
    if(Member_BalanceAuthority(authority)!=1)return NULL;
    snprintf(name,sizeof(name),"%s.cash",authority);
    if(XAdmin_AcquireService(NULL,name,1,lease,&table))return NULL;
    const XBillingCashService* service=table;
    if(!service || service->size<sizeof(*service) || service->version!=1 || !service->pay || !service->lookup){
        XAdmin_ReleaseService(*lease);*lease=NULL;return NULL;}
    return service;
}
static bool Attachment_PaymentKey(const char* xid,int64 memberId,char key[80])
{
    unsigned char digest[32];int i;
    char* source=xrtFormat("%lld:%s",(long long)memberId,xid);
    bool ok=source && xrtSha256(source,strlen(source),digest);xrtFree(source);
    if(!ok)return false;
    strcpy(key,"attachment.");
    for(i=0;i<32;i++)snprintf(key+11+i*2,3,"%02x",digest[i]);
    return true;
}
static bool Attachment_RecordPayment(const char* xid,int64 memberId,const XBillingCashPayment* payment)
{
    if(payment->buyer_id!=memberId || payment->amount<0 || payment->amount>XBILL_MAX_AMOUNT ||
        payment->seller_income<0 || payment->seller_income>payment->amount ||
        payment->amount%10000 || payment->seller_income%10000)return false;
    if(!DB_BeginWrite())return false;
    bool ok=Attachment_HasOrder(xid,memberId) ||
        (Attachment_AddOrder(xid,memberId,payment->amount/10000,0,payment->seller_id,payment->seller_income/10000) &&
         Attachment_UpdateSales(xid));
    return DB_EndWrite(ok);
}
static bool Attachment_RecoverPurchase(const char* xid,int64 memberId)
{
    char key[80];XAdminServiceLease lease;const XBillingCashService* service=Attachment_CashService(&lease);
    XBillingCashPayment payment={sizeof(payment)};
    bool ok=service && Attachment_PaymentKey(xid,memberId,key) && !service->lookup(key,&payment);
    if(lease)XAdmin_ReleaseService(lease);
    return ok && Attachment_RecordPayment(xid,memberId,&payment);
}
static int Attachment_Pay(const char* xid,int64 buyer,int64 seller,int64 price,int64 income)
{
    char authority[65];int managed=Member_BalanceAuthority(authority);
    if(managed<0)return XBILL_UNAVAILABLE;
    if(managed){
        char key[80];XAdminServiceLease lease;const XBillingCashService* service=Attachment_CashService(&lease);
        XBillingCashPayment payment={sizeof(payment),buyer,seller,price*10000,income*10000};
        XBillingCashPayment receipt={sizeof(receipt)};int status=XBILL_UNAVAILABLE;
        if(service && Attachment_PaymentKey(xid,buyer,key)){
            status=service->lookup(key,&receipt);
            if(status==404){status=service->pay(key,&payment);receipt=payment;}
            /* Existing receipts retain the original price and seller even if
             * attachment settings changed between payment and recovery. */
            if(!status && !Attachment_RecordPayment(xid,buyer,&receipt))status=XBILL_UNAVAILABLE;
        }
        if(lease)XAdmin_ReleaseService(lease);
        return status;
    }
    /* Legacy deployments keep cash, order and sales in the same database.
     * Check every write and the commit; no partial debit/payout is accepted. */
    if(!DB_BeginWrite())return XBILL_UNAVAILABLE;
    sqlite3_stmt* stmt=NULL;
    bool ok=sqlite3_prepare_v2(G_DB,"UPDATE member SET balance=balance-? WHERE id=? AND isDelete=0 AND balance>=?",-1,&stmt,NULL)==SQLITE_OK;
    if(ok){sqlite3_bind_int64(stmt,1,price);sqlite3_bind_int64(stmt,2,buyer);sqlite3_bind_int64(stmt,3,price);}
    int rc=ok?sqlite3_step(stmt):SQLITE_ERROR;
    int changed=sqlite3_changes(G_DB);sqlite3_finalize(stmt);stmt=NULL;
    if(rc==SQLITE_DONE && !changed){DB_EndWrite(false);return XBILL_INSUFFICIENT;}
    ok=rc==SQLITE_DONE && changed==1;
    if(ok && seller && income){
        ok=sqlite3_prepare_v2(G_DB,"UPDATE member SET balance=balance+? WHERE id=? AND isDelete=0 AND balance<=?",-1,&stmt,NULL)==SQLITE_OK;
        if(ok){sqlite3_bind_int64(stmt,1,income);sqlite3_bind_int64(stmt,2,seller);sqlite3_bind_int64(stmt,3,XBILL_MAX_AMOUNT/10000-income);}
        ok=ok && DB_Write(stmt,true);sqlite3_finalize(stmt);
    }
    ok=ok && Attachment_AddOrder(xid,buyer,price,0,seller,income) && Attachment_UpdateSales(xid);
    return DB_EndWrite(ok)?0:XBILL_UNAVAILABLE;
}
