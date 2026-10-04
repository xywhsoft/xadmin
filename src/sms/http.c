/* Compatibility surface for registered SMS adapters; all networking is shared. */
typedef XAHttpUrl XASmsUrl;
static bool XA_SmsUrlParse(const char* u, XASmsUrl* out) { return XA_HttpsUrlParse(u, out); }
static bool XA_SmsHttpHeader(XASmsHttpRequest* r,const char* n,const char* v) { return XA_HttpsHttpHeader(r,n,v); }
static bool XA_SmsHttpBody(XASmsHttpRequest* r,const char* u,const char* t,const char* b) { return XA_HttpsHttpBody(r,u,t,b); }
static void XA_SmsHttpUnit(XASmsHttpRequest* r) { XA_HttpsHttpUnit(r); }
static bool XA_SmsHttp(void* e,const char* ca,const XASmsHttpRequest* r,int* status,char** response)
{ return XA_HttpsHttp(e,ca,r,15000,65536,status,response); }
