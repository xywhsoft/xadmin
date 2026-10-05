/* Cash purchase and seller payout share a transaction and a durable receipt.
 * The host can reconstruct its order after a crash without charging again. */
static int Billing_CashLookup(const char* key,XBillingCashPayment* out)
{
    if(!XP_Id(key,96) || !out || out->size!=sizeof(*out))return XBILL_INVALID;
    sqlite3_stmt* s=XP_SQL(G_DB,"SELECT buyer_id,seller_id,amount,seller_income FROM cash_payment WHERE operation_id=?");
    XP_Bind(s,1,key);int rc=s?sqlite3_step(s):SQLITE_ERROR;
    if(rc==SQLITE_ROW){out->buyer_id=sqlite3_column_int64(s,0);out->seller_id=sqlite3_column_int64(s,1);
        out->amount=sqlite3_column_int64(s,2);out->seller_income=sqlite3_column_int64(s,3);}
    sqlite3_finalize(s);return rc==SQLITE_ROW?0:rc==SQLITE_DONE?404:XBILL_UNAVAILABLE;
}
static int Billing_CashPay(const char* key,const XBillingCashPayment* payment)
{
    if(!XP_Id(key,96) || !payment || payment->size!=sizeof(*payment) || payment->buyer_id<=0 ||
        payment->seller_id<0 || payment->seller_id==payment->buyer_id || payment->amount<0 ||
        payment->amount>XBILL_MAX_AMOUNT || payment->seller_income<0 || payment->seller_income>payment->amount ||
        (!payment->seller_id && payment->seller_income))return XBILL_INVALID;
    if(!Billing_Begin())return XBILL_UNAVAILABLE;
    XBillingCashPayment prior={sizeof(prior)};int found=Billing_CashLookup(key,&prior);
    if(found==0){bool same=prior.buyer_id==payment->buyer_id && prior.seller_id==payment->seller_id &&
        prior.amount==payment->amount && prior.seller_income==payment->seller_income;
        return Billing_End(true)?(same?0:XBILL_CONFLICT):XBILL_UNAVAILABLE;}
    if(found!=404 || !Billing_Ensure(payment->buyer_id) ||
        (payment->seller_id && !Billing_Ensure(payment->seller_id))){Billing_End(false);return XBILL_UNAVAILABLE;}
    sqlite3_stmt* s=XP_SQL(G_DB,"UPDATE account SET cash=cash-? WHERE member_id=? AND cash-reserved>=?");
    if(s){sqlite3_bind_int64(s,1,payment->amount);sqlite3_bind_int64(s,2,payment->buyer_id);sqlite3_bind_int64(s,3,payment->amount);}
    bool ok=XP_Done(s);
    if(!ok){Billing_End(false);return XBILL_UNAVAILABLE;}
    if(sqlite3_changes(G_DB)!=1){Billing_End(false);return XBILL_INSUFFICIENT;}
    if(payment->seller_income){
        s=XP_SQL(G_DB,"UPDATE account SET cash=cash+? WHERE member_id=? AND cash<=1000000000000000-?");
        if(s){sqlite3_bind_int64(s,1,payment->seller_income);sqlite3_bind_int64(s,2,payment->seller_id);sqlite3_bind_int64(s,3,payment->seller_income);}
        ok=XP_Done(s) && sqlite3_changes(G_DB)==1;
    }
    char entry[120];snprintf(entry,sizeof(entry),"cash.buy.%s",key);
    if(ok)ok=Billing_Ledger(payment->buyer_id,entry,"purchase",-payment->amount,"cash",key,"Cash purchase","service");
    if(ok && payment->seller_income){snprintf(entry,sizeof(entry),"cash.sell.%s",key);
        ok=Billing_Ledger(payment->seller_id,entry,"sale",payment->seller_income,"cash",key,"Cash sale","service");}
    if(ok){
        s=XP_SQL(G_DB,"INSERT INTO cash_payment(operation_id,buyer_id,seller_id,amount,seller_income,created_at) VALUES(?,?,?,?,?,?)");
        XP_Bind(s,1,key);if(s){sqlite3_bind_int64(s,2,payment->buyer_id);sqlite3_bind_int64(s,3,payment->seller_id);
            sqlite3_bind_int64(s,4,payment->amount);sqlite3_bind_int64(s,5,payment->seller_income);sqlite3_bind_int64(s,6,(int64_t)time(NULL));}
        ok=XP_Done(s) && sqlite3_changes(G_DB)==1;
    }
    return Billing_End(ok)?0:XBILL_UNAVAILABLE;
}
