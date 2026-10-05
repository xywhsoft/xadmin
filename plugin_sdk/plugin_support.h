#ifndef XADMIN_PLUGIN_SUPPORT_H
#define XADMIN_PLUGIN_SUPPORT_H
#include <xs_plugin.h>
#include <value_util.h>
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
/* Small boundary helpers shared by first-party plugins. They never retain
 * borrowed request/value views or hide database transactions. */
static const char* XP_Text(const xvalue* object,const char* key,size_t limit)
{
    xstrview v={0};
    if(!xrtValueGetString(ValueGet(object,key),&v) || v.Size>limit ||
        memchr(v.Data,0,v.Size) || !xrtUtf8Valid(v,NULL))return NULL;
    return v.Data;
}
static bool XP_Int(const xvalue* object,const char* key,int64_t low,int64_t high,int64_t* out)
{
    int64 value;
    if(!xrtValueGetInt(ValueGet(object,key),&value) || value<low || value>high)return false;
    *out=value;return true;
}
static bool XP_Id(const char* text,size_t limit)
{
    size_t i,n=text?strlen(text):0;if(!n || n>limit)return false;
    for(i=0;i<n;i++){unsigned char c=text[i];if(!((c>='a'&&c<='z') || (c>='A'&&c<='Z') ||
        (c>='0'&&c<='9') || c=='-' || c=='_' || c=='.'))return false;}
    return true;
}
static bool XP_Positive(const char* text,int64_t* out)
{
    int64_t value=0;const unsigned char* p=(const unsigned char*)text;
    if(!p || !*p)return false;
    for(;*p;p++){if(*p<'0' || *p>'9' || value>(INT64_MAX-(*p-'0'))/10)return false;value=value*10+*p-'0';}
    if(!value)return false;*out=value;return true;
}
static bool XP_Fields(const xvalue* object,const char* const* names,size_t count)
{
    if(!object || xrtValueType(object)!=XVALUE_OBJECT)return false;
    xvalueiter it={0};xvaluekey key;xvalue* v;bool valid=true;
    if(xrtValueIterBegin(object,&it)){
        while((v=xrtValueIterNext(&it,&key))){size_t i;
            for(i=0;i<count;i++)if(key.String.Size==strlen(names[i]) && !memcmp(key.String.Data,names[i],key.String.Size))break;
            if(i==count){valid=false;break;}}
        xrtValueIterEnd(&it);
    }return valid;
}
static xvalue* XP_Body(XS_RequestObject req,size_t limit)
{
    const char* body=XAdmin_ReqBody(req);size_t size=XAdmin_ReqBodyLen(req);
    if(!body || !size || size>limit || memchr(body,0,size))return NULL;
    return xrtJsonParse(xrtStrViewN(body,size));
}
static void XP_Reply(XS_ResponseObject resp,int status,const char* message,xvalue* data)
{
    xvalue* value=ValueObject();ValueSetInt(value,"code",status<400?0:status);
    ValueSetText(value,"message",message?message:"");if(data)ValueSetOwn(value,"data",data);
    size_t n=0;char* json=xrtJsonStringify(value,false,&n);
    xsHttpReplyAuto(resp,status,"Content-Type: application/json; charset=utf-8\r\nCache-Control: no-store\r\n",json?json:"{}",json?n:2);
    xrtFree(json);xrtValueRelease(value);
}
static bool XP_Member(XS_RequestObject req,XS_ResponseObject resp,xvalue* session,int method)
{
    if(xsReqMethodID(req)!=method){XP_Reply(resp,405,"Method not allowed",NULL);return false;}
    if(!session || ValueInt(session,"id")<=0 || XAdmin_MemberContactStatus(session)<0){
        XP_Reply(resp,401,"Member login required",NULL);return false;}
    return true;
}
static sqlite3_stmt* XP_SQL(sqlite3* db,const char* sql)
{sqlite3_stmt* s=NULL;if(!db || sqlite3_prepare_v2(db,sql,-1,&s,NULL)!=SQLITE_OK)return NULL;return s;}
/* Reject newer private schemas instead of silently resetting their version. */
static bool XP_SchemaSupported(sqlite3* db,int maximum)
{sqlite3_stmt* s=XP_SQL(db,"PRAGMA user_version");bool ok=s && sqlite3_step(s)==SQLITE_ROW && sqlite3_column_int(s,0)>=0 && sqlite3_column_int(s,0)<=maximum;sqlite3_finalize(s);return ok;}
static void XP_Bind(sqlite3_stmt* s,int i,const char* text)
{if(s)sqlite3_bind_text(s,i,text?text:"",-1,SQLITE_TRANSIENT);}
static bool XP_Done(sqlite3_stmt* s)
{bool ok=s && sqlite3_step(s)==SQLITE_DONE;sqlite3_finalize(s);return ok;}
static bool XP_Random(char out[33])
{
    unsigned char bytes[16];size_t i;if(!xrtSecureRandom(bytes,sizeof(bytes)))return false;
    for(i=0;i<16;i++)snprintf(out+i*2,3,"%02x",bytes[i]);xrtSecureZero(bytes,sizeof(bytes));return true;
}
static xvalue* XP_Rows(sqlite3_stmt* s)
{
    if(!s)return NULL;xvalue* rows=ValueArray();int rc,i;
    while((rc=sqlite3_step(s))==SQLITE_ROW){xvalue* row=ValueObject();
        for(i=0;i<sqlite3_column_count(s);i++){
            const char* name=sqlite3_column_name(s,i);
            if(sqlite3_column_type(s,i)==SQLITE_INTEGER)ValueSetInt(row,name,sqlite3_column_int64(s,i));
            else if(sqlite3_column_type(s,i)==SQLITE_NULL)ValueSetOwn(row,name,xrtValueNull());
            else ValueSetText(row,name,(const char*)sqlite3_column_text(s,i));
        }ValueArrayOwn(rows,row);
    }sqlite3_finalize(s);if(rc!=SQLITE_DONE){xrtValueRelease(rows);return NULL;}return rows;
}
#endif
