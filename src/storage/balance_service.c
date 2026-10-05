/* Generic optional wallet bridge. Financial policy remains in its plugin.
 * Activation is persistent: unloading a provider cannot revive stale cash. */
#include "../../plugin_sdk/billing_service.h"
static int Member_BalanceAuthority(char name[65])
{
    if(!G_DB)return 0;sqlite3_stmt* s=NULL;
    if(sqlite3_prepare_v2(G_DB,"SELECT 1 FROM sqlite_master WHERE type='table' AND name='xadmin_balance_provider'",-1,&s,NULL)!=SQLITE_OK)return -1;
    int rc=sqlite3_step(s);sqlite3_finalize(s);if(rc==SQLITE_DONE)return 0;if(rc!=SQLITE_ROW)return -1;
    if(sqlite3_prepare_v2(G_DB,"SELECT service_name FROM xadmin_balance_provider WHERE id=1",-1,&s,NULL)!=SQLITE_OK)return -1;
    rc=sqlite3_step(s);bool valid=rc==SQLITE_ROW && sqlite3_column_bytes(s,0)>0 && sqlite3_column_bytes(s,0)<65;
    if(valid)snprintf(name,65,"%s",sqlite3_column_text(s,0));sqlite3_finalize(s);return valid?1:rc==SQLITE_DONE?0:-1;
}
static const XBillingService* Member_BalanceService(XAdminServiceLease* lease)
{
    char name[65];const void* table=NULL;*lease=NULL;
    if(Member_BalanceAuthority(name)!=1 || XAdmin_AcquireService(NULL,name,1,lease,&table))return NULL;
    const XBillingService* service=table;
    if(!service || service->size<sizeof(*service) || service->version!=1){XAdmin_ReleaseService(*lease);*lease=NULL;return NULL;}
    return service;
}
static bool Member_SetBalance(xvalue* data,int64 owner,int64 legacy_cents)
{
    char name[65];int managed=Member_BalanceAuthority(name);int64 micros=0,cents=legacy_cents;bool ok=managed>=0;
    if(managed==1){XAdminServiceLease lease;const XBillingService* service=Member_BalanceService(&lease);
        XBillingAccount account={sizeof(account)};ok=service && !service->account(owner,&account);
        if(ok){micros=account.cash;cents=micros/10000;}if(lease)XAdmin_ReleaseService(lease);}
    else if(managed==0 && cents>=0 && cents<=XBILL_MAX_AMOUNT/10000)micros=cents*10000;
    else ok=false;
    ValueSetBool(data,"balance_available",ok);ValueSetBool(data,"balance_managed",managed!=0);
    if(ok){ValueSetInt(data,"balance",cents);ValueSetInt(data,"balance_micros",micros);}
    else{ValueSetOwn(data,"balance",xrtValueNull());ValueSetOwn(data,"balance_micros",xrtValueNull());}
    return ok;
}
static int Member_ServiceAdjust(int64 owner,int64 cents,const char* reason,const char* actor)
{
    char name[65];int managed=Member_BalanceAuthority(name);if(managed!=1)return managed;
    if(!cents || cents>XBILL_MAX_AMOUNT/10000 || cents<-XBILL_MAX_AMOUNT/10000)return -1;
    XAdminServiceLease lease;const XBillingService* service=Member_BalanceService(&lease);
    char operation[65];bool ok=service && XA_Random(operation) &&
        service->adjust_cash(owner,cents*10000,operation,reason?reason:"Balance adjustment",actor?actor:"administrator")==0;
    if(lease)XAdmin_ReleaseService(lease);return ok?1:-1;
}
static xvalue* Member_ServiceTransactions(int64 owner,int64 offset,int limit)
{
    XAdminServiceLease lease;const XBillingService* service=Member_BalanceService(&lease);
    xvalue* rows=service?service->transactions(owner,offset,limit):NULL;if(lease)XAdmin_ReleaseService(lease);return rows;
}
