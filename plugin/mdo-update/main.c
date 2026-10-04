/* Two independent current packages. The host serializes route callbacks and
 * pins the plugin during each request; no borrowed request data escapes.
 * Publish the immutable object first, then atomically publish its metadata.
 * A failed upload can leave an unreferenced object but never a broken pointer. */
#include <xs_plugin.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define UPDATE_MAX_BYTES (32u * 1024u * 1024u)
#define UPDATE_META_MAX 8192u
typedef struct UpdatePackage {
    char Hash[65], Previous[65], Name[256], Notes[1025];
    uint64 Size, UpdatedAt;
    bool Required;
} UpdatePackage;
static const char* const Platforms[] = {"windows-x86_64", "android-arm64-v8a"};
static const char* const Extensions[] = {".exe", ".apk"};
static XAdminPluginHandle Handle;
static XAdminHostContext* Host;
static char *Directory, *MetadataPath;
static UpdatePackage Packages[2];
static bool MetadataInvalid;

XADMIN_EXPORT void XAdmin_PluginSetGlobalData(int index, void* value)
{ if (index == XADMIN_GLOBAL_HOST_CONTEXT) Host = value; }

static bool PutText(xvalue* v, const char* k, const char* s)
{ return xrtValueObjectSetNew(v, xrtStrView(k), xrtValueString(xrtStrView(s))); }
static bool PutUInt(xvalue* v, const char* k, uint64 n)
{ return xrtValueObjectSetNew(v, xrtStrView(k), xrtValueUInt(n)); }
static bool HashValid(const char* s)
{
    size_t i;
    if (!s || strlen(s) != 64) return false;
    for (i = 0; i < 64; ++i)
        if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f'))) return false;
    return true;
}
static bool CopyText(char* out, size_t cap, const char* s, size_t n)
{
    if (!s || n >= cap || memchr(s, 0, n) || !xrtUtf8Valid(xrtStrViewN(s,n),NULL)) return false;
    memcpy(out,s,n); out[n] = 0; return true;
}
static bool ReadText(xvalue* object, const char* key, char* out, size_t cap)
{
    xstrview s = {0};
    return xrtValueGetString(xrtValueObjectGet(object,xrtStrView(key)),&s) &&
        CopyText(out,cap,s.Data,s.Size);
}
static bool ReadUInt(xvalue* object, const char* key, uint64* out)
{
    xvalue* v = xrtValueObjectGet(object,xrtStrView(key)); int64 n;
    if (xrtValueGetUInt(v,out)) return true;
    if (!xrtValueGetInt(v,&n) || n < 0) return false;
    *out = (uint64)n; return true;
}
/* Old publications are optional. A present field must be a JSON boolean. */
static bool ReadRequired(xvalue* object, bool* out)
{
    xvalue* v = xrtValueObjectGet(object,XRT_STR_LITERAL("required"));
    *out = false;
    return !v || xrtValueGetBool(v,out);
}
static int Platform(const char* s)
{
    int i; for (i = 0; s && i < 2; ++i) if (!strcmp(s,Platforms[i])) return i;
    return -1;
}
static void Reply(XS_ResponseObject resp, int status, xvalue* data)
{
    size_t n = 0; char* text = data ? xrtJsonStringify(data,false,&n) : NULL;
    xsHttpReplyAuto(resp,text ? status : 503,
        "Content-Type: application/json; charset=utf-8\r\nCache-Control: no-store\r\n",
        text ? text : "{\"message\":\"Unable to serialize response\"}", text ? n : 0);
    xrtFree(text); xrtValueRelease(data);
}
static void Error(XS_ResponseObject resp, int status, const char* message)
{
    xvalue* v = xrtValueObject(); PutUInt(v,"code",(uint64)status); PutText(v,"message",message);
    Reply(resp,status,v);
}
static char* ObjectPath(int platform, const char* hash)
{
    char name[70];
    if (platform < 0 || platform > 1 || !HashValid(hash)) return NULL;
    snprintf(name,sizeof(name),"%s%s",hash,Extensions[platform]);
    return xrtPathJoin(Directory,name);
}
static bool FileHash(const char* path, char hash[65], uint64* size)
{
    xfileinfo info; xfile f = NULL; xsha256 state; unsigned char buf[65536], digest[32];
    uint64 total = 0; size_t got, i; bool ok = false;
    if (!xrtPathStat(path,false,&info) || info.Type != XFILE_TYPE_FILE ||
        info.Size == 0 || info.Size > UPDATE_MAX_BYTES) return false;
    f = xrtOpen(path,XFILE_READ); if (!f) return false;
    xrtSha256Init(&state);
    while (xrtRead(f,buf,sizeof(buf),&got)) {
        if (!got) {
            ok = total == info.Size && xrtSha256Final(&state,digest); break;
        }
        total += got;
        if (total > UPDATE_MAX_BYTES || !xrtSha256Update(&state,buf,got)) break;
    }
    xrtClose(f);
    if (!ok) return false;
    for (i = 0; i < 32; ++i) snprintf(hash + i*2,3,"%02x",(unsigned)digest[i]);
    *size = total; return true;
}
static uint16 U16(const unsigned char* p)
{ return (uint16)p[0] | ((uint16)p[1] << 8); }
static uint32 U32(const unsigned char* p)
{ return (uint32)U16(p) | ((uint32)U16(p+2) << 16); }
static uint64 U64(const unsigned char* p)
{ return (uint64)U32(p) | ((uint64)U32(p+4) << 32); }

/* Structural checks only. Never load/execute uploaded code on the server. */
static bool ExeValid(const unsigned char* p, size_t n)
{
    size_t pe; uint64 archive;
    if (n < 144 || memcmp(p,"MZ",2)) return false;
    pe = U32(p+60);
    if (pe > n-26 || memcmp(p+pe,"PE\0\0",4) || U16(p+pe+4) != 0x8664 ||
        U16(p+pe+24) != 0x20b) return false;
    if (memcmp(p+n-32,"XRTPEND\0",8)) return false;
    archive = U64(p+n-24);
    return archive >= 112 && archive <= n-pe-26 && !memcmp(p+n-(size_t)archive,"XRTPACK\0",8);
}
static bool ApkValid(const unsigned char* p, size_t n)
{
    static const char* const Required[] = {
        "AndroidManifest.xml","classes.dex","assets/app.xrtpack","lib/arm64-v8a/libxs.so"};
    size_t eocd, begin, pos, end, i; uint16 count, entry; unsigned found = 0;
    if (n < 22 || memcmp(p,"PK\003\004",4)) return false;
    begin = n > 65557 ? n-65557 : 0; eocd = n-22;
    for (;;) {
        if (U32(p+eocd) == 0x06054b50 && eocd+22+U16(p+eocd+20) == n) break;
        if (eocd == begin) return false;
        --eocd;
    }
    count = U16(p+eocd+10);
    if (!count || count > 4096 || U16(p+eocd+4) || U16(p+eocd+6) ||
        U16(p+eocd+8) != count) return false;
    pos = U32(p+eocd+16);
    if (pos > eocd || U32(p+eocd+12) > eocd-pos) return false;
    end = pos+U32(p+eocd+12);
    for (entry = 0; entry < count; ++entry) {
        size_t len, record;
        if (end-pos < 46 || U32(p+pos) != 0x02014b50) return false;
        len = U16(p+pos+28); record = 46+len+U16(p+pos+30)+U16(p+pos+32);
        if (record > end-pos || (U16(p+pos+8)&1) || U16(p+pos+34)) return false;
        for (i = 0; i < 4; ++i) {
            if (len == strlen(Required[i]) && !memcmp(p+pos+46,Required[i],len)) {
                if ((found&(1u<<i)) || !U32(p+pos+24)) return false;
                found |= 1u<<i;
            }
        }
        pos += record;
    }
    return pos == end && found == 15;
}
static xvalue* PackageData(int platform, const UpdatePackage* package, bool private_data)
{
    char url[128]; xvalue* v = xrtValueObject(); bool ok;
    if (!v) return NULL;
    snprintf(url,sizeof(url),"/update/download/%s/%s",Platforms[platform],package->Hash);
    ok = PutText(v,"platform",Platforms[platform]) && PutText(v,"sha256",package->Hash) &&
        PutUInt(v,"size",package->Size) && PutText(v,"url",url) &&
        PutText(v,"filename",package->Name) && PutText(v,"notes",package->Notes) &&
        PutUInt(v,"updated_at",package->UpdatedAt) &&
        xrtValueObjectSetNew(v,XRT_STR_LITERAL("required"),xrtValueBool(package->Required));
    if (ok && private_data) ok = PutText(v,"previous",package->Previous);
    if (!ok) { xrtValueRelease(v); return NULL; }
    return v;
}
static bool SaveMetadata(const UpdatePackage values[2])
{
    int i; xvalue* root = xrtValueObject(); size_t n = 0; char* bytes = NULL; bool ok = root != NULL;
    for (i = 0; ok && i < 2; ++i) if (values[i].Hash[0]) {
        xvalue* v = PackageData(i,&values[i],true);
        ok = v && xrtValueObjectSetNew(root,xrtStrView(Platforms[i]),v);
    }
    if (ok) bytes = xrtJsonStringify(root,false,&n);
    ok = bytes && n <= UPDATE_META_MAX && xrtFileWriteAtomic(MetadataPath,(xbytesview){(cbytes)bytes,n});
    xrtFree(bytes); xrtValueRelease(root); return ok;
}
static bool LoadMetadata(void)
{
    xfileinfo info; size_t n = 0; unsigned char* bytes; xvalue* root;
    xjsonreadconfig limits; int i; size_t known = 0; bool ok = true;
    memset(Packages,0,sizeof(Packages));
    if (!xrtPathStat(MetadataPath,false,&info)) {
        const xerror* e = xrtGetError();
        if (e && xrtErrorKind(e) == XERR_NOT_FOUND) { xrtClearError(); return true; }
        return false;
    }
    if (info.Type != XFILE_TYPE_FILE || info.Size > UPDATE_META_MAX) return false;
    bytes = xrtFileReadAllLimit(MetadataPath,UPDATE_META_MAX,&n);
    if (!bytes) return false;
    xrtJsonReadConfigInit(&limits); limits.MaxInputBytes = UPDATE_META_MAX;
    limits.MaxDepth = 4; limits.MaxValues = 64;
    root = xrtJsonRead(xrtStrViewN((char*)bytes,n),&limits); xrtFree(bytes);
    if (!root || xrtValueType(root) != XVALUE_OBJECT || xrtValueCount(root) > 2) ok = false;
    for (i = 0; ok && i < 2; ++i) {
        UpdatePackage* v = &Packages[i]; xvalue* item = xrtValueObjectGet(root,xrtStrView(Platforms[i]));
        if (!item) continue;
        ++known;
        ok = ReadText(item,"sha256",v->Hash,sizeof(v->Hash)) && HashValid(v->Hash) &&
            ReadText(item,"previous",v->Previous,sizeof(v->Previous)) &&
            (!v->Previous[0] || HashValid(v->Previous)) &&
            ReadText(item,"filename",v->Name,sizeof(v->Name)) &&
            ReadText(item,"notes",v->Notes,sizeof(v->Notes)) &&
            ReadUInt(item,"size",&v->Size) && v->Size && v->Size <= UPDATE_MAX_BYTES &&
            ReadUInt(item,"updated_at",&v->UpdatedAt) && ReadRequired(item,&v->Required);
    }
    if (ok && known != xrtValueCount(root)) ok = false;
    xrtValueRelease(root);
    if (!ok) memset(Packages,0,sizeof(Packages));
    return ok;
}
static void Current(XS_ServerObject s, XS_HostObject h, XS_RequestObject req,
    XS_ResponseObject resp, xvalue* session)
{
    char platform[32] = ""; int index; (void)s; (void)h; (void)session;
    if (!(xsReqMethodID(req)&(XHTTP_METHOD_GET|XHTTP_METHOD_HEAD))) { Error(resp,405,"Method not allowed"); return; }
    if (MetadataInvalid) { Error(resp,503,"Package metadata is invalid"); return; }
    int count = xsReqQueryValue(req,"platform",platform,sizeof(platform));
    if (count < 0) snprintf(platform,sizeof(platform),"%s",Platforms[0]);
    index = Platform(platform);
    if (index < 0) { Error(resp,400,"Unsupported platform"); return; }
    if (!Packages[index].Hash[0]) { Error(resp,404,"No update package available"); return; }
    Reply(resp,200,PackageData(index,&Packages[index],false));
}
static void Admin(XS_ServerObject s, XS_HostObject h, XS_RequestObject req,
    XS_ResponseObject resp, xvalue* session)
{
    (void)s; (void)h; (void)session;
    if (xsReqMethodID(req) != XHTTP_METHOD_GET) { Error(resp,405,"Method not allowed"); return; }
    XAdmin_LoadPluginPage(Handle,resp,200,
        "Content-Type: text/html; charset=utf-8\r\nCache-Control: no-store\r\n","index.html");
}
static void Inventory(XS_ServerObject s, XS_HostObject h, XS_RequestObject req,
    XS_ResponseObject resp, xvalue* session)
{
    xvalue *root, *data; const char* csrf; int i; (void)s; (void)h;
    if (xsReqMethodID(req) != XHTTP_METHOD_GET) { Error(resp,405,"Method not allowed"); return; }
    csrf = XAdmin_AdminCSRFToken(session);
    if (!csrf) { Error(resp,503,"CSRF token unavailable"); return; }
    root = xrtValueObject(); data = xrtValueObject();
    PutText(data,"csrf_token",csrf); PutUInt(data,"max_size",UPDATE_MAX_BYTES);
    xrtValueObjectSetNew(data,xrtStrView("metadata_invalid"),xrtValueBool(MetadataInvalid));
    for (i = 0; i < 2; ++i)
        xrtValueObjectSetNew(data,xrtStrView(Platforms[i]),Packages[i].Hash[0]
            ? PackageData(i,&Packages[i],false) : xrtValueNull());
    PutUInt(root,"code",0); xrtValueObjectSetNew(root,xrtStrView("data"),data); Reply(resp,200,root);
}
static bool PartName(const XAdminMultipartPart* part, const char* name)
{ return part->name && part->nameLen == strlen(name) && !memcmp(part->name,name,part->nameLen); }
static bool MultipartClosed(const char* body, size_t n, size_t offset, const char* boundary)
{
    size_t len = strlen(boundary);
    if (offset > n || n-offset < len+6 || memcmp(body+offset,"\r\n--",4) ||
        memcmp(body+offset+4,boundary,len) || memcmp(body+offset+4+len,"--",2)) return false;
    offset += len+6;
    return offset == n || (n-offset == 2 && !memcmp(body+offset,"\r\n",2));
}
static void Upload(XS_ServerObject s, XS_HostObject h, XS_RequestObject req,
    XS_ResponseObject resp, xvalue* session)
{
    char boundary[74], platform[32] = ""; XAdminMultipartPart part;
    const char* body = XAdmin_ReqBody(req); const char* payload = NULL;
    size_t n = XAdmin_ReqBodyLen(req), offset = 0, payload_size = 0;
    bool seen_platform = false, seen_notes = false, seen_required = false, ok = true;
    UpdatePackage next = {0}, values[2]; int index; char* path = NULL; char disk_hash[65]; uint64 disk_size;
    (void)s; (void)h;
    if (xsReqMethodID(req) != XHTTP_METHOD_POST) { Error(resp,405,"Method not allowed"); return; }
    if (!XAdmin_CheckAdminCSRF(req,session)) { Error(resp,403,"CSRF verification failed"); return; }
    if (MetadataInvalid) { Error(resp,409,"Repair invalid current.json before uploading"); return; }
    if (n > UPDATE_MAX_BYTES+8192) { Error(resp,413,"Package exceeds 32 MiB"); return; }
    if (!body || !XAdmin_MultipartBoundary(XAdmin_PluginReqHeader(req,"Content-Type"),boundary,sizeof(boundary))) {
        Error(resp,400,"Expected multipart upload"); return;
    }
    while (ok && XAdmin_MultipartNext(body,n,boundary,strlen(boundary),&offset,&part)) {
        if (PartName(&part,"platform") && !seen_platform && !part.filename) {
            seen_platform = true; ok = CopyText(platform,sizeof(platform),part.data,part.size);
        } else if (PartName(&part,"notes") && !seen_notes && !part.filename) {
            seen_notes = true; ok = CopyText(next.Notes,sizeof(next.Notes),part.data,part.size);
        } else if (PartName(&part,"required") && !seen_required && !part.filename) {
            seen_required = true;
            ok = (part.size == 4 && !memcmp(part.data,"true",4)) ||
                (part.size == 5 && !memcmp(part.data,"false",5));
            next.Required = part.size == 4;
        } else if (PartName(&part,"file") && !payload && part.filename && part.filenameLen) {
            payload = part.data; payload_size = part.size;
            /* Display only the basename; paths never determine storage names. */
            size_t start = 0, i;
            for (i = 0; i < part.filenameLen; ++i)
                if (part.filename[i] == '/' || part.filename[i] == '\\') start = i+1;
            ok = part.filenameLen > start && CopyText(next.Name,sizeof(next.Name),part.filename+start,part.filenameLen-start);
            for (i = 0; ok && next.Name[i]; ++i) if ((unsigned char)next.Name[i] < 32) ok = false;
        } else ok = false;
    }
    index = Platform(platform);
    if (!ok || index < 0 || !payload_size || payload_size > UPDATE_MAX_BYTES ||
        !MultipartClosed(body,n,offset,boundary) ||
        !(index == 0 ? ExeValid((const unsigned char*)payload,payload_size) : ApkValid((const unsigned char*)payload,payload_size))) {
        Error(resp,422,"Invalid package, platform or multipart fields"); return;
    }
    /* A private staging file is never advertised. Hash the bytes actually saved. */
    path = xrtPathJoin(Directory,"upload.part");
    ok = path && xrtFileWriteAtomic(path,(xbytesview){(cbytes)payload,payload_size}) &&
        FileHash(path,next.Hash,&next.Size);
    if (!ok) { xrtFree(path); Error(resp,503,"Unable to save uploaded package"); return; }
    char* destination = ObjectPath(index,next.Hash);
    if (destination && xrtFileExists(destination))
        ok = FileHash(destination,disk_hash,&disk_size) && disk_size == next.Size && !strcmp(disk_hash,next.Hash);
    else ok = destination && xrtPathRename(path,destination,false);
    if (path) xrtFileDelete(path);
    xrtFree(destination); xrtFree(path);
    if (!ok) { Error(resp,503,"Unable to publish package object"); return; }
    if (!strcmp(next.Hash,Packages[index].Hash)) {
        /* Retrying identical bytes is idempotent. Policy changes use the
         * explicit compare-and-set endpoint, never a repeated upload. */
        Reply(resp,200,PackageData(index,&Packages[index],false)); return;
    }
    next.UpdatedAt = (uint64)time(NULL);
    snprintf(next.Previous,sizeof(next.Previous),"%s",Packages[index].Hash);
    memcpy(values,Packages,sizeof(values)); values[index] = next;
    if (!SaveMetadata(values)) { Error(resp,503,"Unable to publish current package"); return; }
    /* Requests are serialized by the host. No download still borrows this file.
     * Cleanup is best effort and only follows a successful metadata commit. */
    if (Packages[index].Previous[0] && strcmp(Packages[index].Previous,next.Hash)) {
        char* obsolete = ObjectPath(index,Packages[index].Previous);
        if (obsolete) xrtFileDelete(obsolete);
        xrtFree(obsolete);
    }
    memcpy(Packages,values,sizeof(values));
    Reply(resp,200,PackageData(index,&Packages[index],false));
}
static void Policy(XS_ServerObject s, XS_HostObject h, XS_RequestObject req,
    XS_ResponseObject resp, xvalue* session)
{
    (void)s; (void)h;
    if (xsReqMethodID(req) != XHTTP_METHOD_POST) { Error(resp,405,"Method not allowed"); return; }
    if (!XAdmin_CheckAdminCSRF(req,session)) { Error(resp,403,"CSRF verification failed"); return; }
    if (MetadataInvalid) { Error(resp,409,"Repair invalid current.json first"); return; }
    size_t n = XAdmin_ReqBodyLen(req); const char* body = XAdmin_ReqBody(req);
    if (!body || !n || n > 1024) { Error(resp,400,"Invalid policy body"); return; }
    xjsonreadconfig limits; xrtJsonReadConfigInit(&limits);
    limits.MaxInputBytes = 1024; limits.MaxDepth = 2; limits.MaxValues = 8;
    xvalue* root = xrtJsonRead(xrtStrViewN(body,n),&limits);
    char platform[32], hash[65]; bool required = false;
    bool ok = root && xrtValueType(root) == XVALUE_OBJECT && xrtValueCount(root) == 3 &&
        ReadText(root,"platform",platform,sizeof(platform)) &&
        ReadText(root,"sha256",hash,sizeof(hash)) && HashValid(hash) &&
        xrtValueGetBool(xrtValueObjectGet(root,XRT_STR_LITERAL("required")),&required);
    xrtValueRelease(root);
    int index = ok ? Platform(platform) : -1;
    if (index < 0) { Error(resp,422,"Expected platform, sha256 and boolean required"); return; }
    if (strcmp(hash,Packages[index].Hash)) { Error(resp,409,"Current package changed; refresh before saving"); return; }
    if (Packages[index].Required != required) {
        UpdatePackage values[2]; memcpy(values,Packages,sizeof(values));
        values[index].Required = required; values[index].UpdatedAt = (uint64)time(NULL);
        if (!SaveMetadata(values)) { Error(resp,503,"Unable to save update policy"); return; }
        memcpy(Packages,values,sizeof(values));
    }
    Reply(resp,200,PackageData(index,&Packages[index],false));
}
static void DownloadSend(XS_ServerObject s, XS_HostObject h, XS_RequestObject req,
    XS_ResponseObject resp, xvalue* session)
{
    char platform[32], hash[65], expected[128], etag[68]; int index;
    char* path; unsigned char* body; size_t n = 0; unsigned char digest[32]; char actual[65]; size_t i;
    (void)s; (void)h; (void)session;
    if (!(xsReqMethodID(req)&(XHTTP_METHOD_GET|XHTTP_METHOD_HEAD))) { Error(resp,405,"Method not allowed"); return; }
    const char* route = XAdmin_ReqPath(req);
    const char* slash = !strncmp(route,"/update/download/",17) ? strchr(route+17,'/') : NULL;
    if (!slash || !CopyText(platform,sizeof(platform),route+17,(size_t)(slash-route-17)) ||
        !CopyText(hash,sizeof(hash),slash+1,strlen(slash+1)) ||
        (index = Platform(platform)) < 0 || !HashValid(hash)) { Error(resp,404,"Package not found"); return; }
    snprintf(expected,sizeof(expected),"/update/download/%s/%s",platform,hash);
    if (strcmp(XAdmin_ReqPath(req),expected)) { Error(resp,404,"Package not found"); return; }
    if (MetadataInvalid || (strcmp(hash,Packages[index].Hash) && strcmp(hash,Packages[index].Previous))) {
        Error(resp,404,"Package is no longer available"); return;
    }
    path = ObjectPath(index,hash); xfileinfo info;
    if (!path || !xrtPathStat(path,false,&info) || info.Type != XFILE_TYPE_FILE || info.Size > UPDATE_MAX_BYTES) {
        xrtFree(path); Error(resp,404,"Package not found"); return;
    }
    body = xrtFileReadAllLimit(path,UPDATE_MAX_BYTES,&n); xrtFree(path);
    bool ok = body && xrtSha256(body,n,digest);
    for (i = 0; ok && i < 32; ++i) snprintf(actual+i*2,3,"%02x",(unsigned)digest[i]);
    if (!ok || strcmp(actual,hash)) { xrtFree(body); Error(resp,503,"Stored package checksum failed"); return; }
    snprintf(etag,sizeof(etag),"\"%s\"",hash);
    xhttpfield fields[] = {
        {XRT_STR_LITERAL("Content-Type"),xrtStrView(index ? "application/vnd.android.package-archive" : "application/octet-stream")},
        {XRT_STR_LITERAL("Content-Disposition"),xrtStrView(index ? "attachment; filename=\"mdo-arm64-v8a.apk\"" : "attachment; filename=\"mdo.exe\"")},
        {XRT_STR_LITERAL("Cache-Control"),XRT_STR_LITERAL("public, max-age=86400, immutable")},
        {XRT_STR_LITERAL("ETag"),xrtStrView(etag)}
    };
    XAdmin_ReplyBinary(req,200,fields,4,body,n,120000); xrtFree(body);
}
static void Download(XS_ServerObject s, XS_HostObject h, XS_RequestObject req,
    XS_ResponseObject resp, xvalue* session)
{
    (void)s; (void)h;
    if (XAdmin_DeferRoute(Handle,req,session,DownloadSend))
        Error(resp,503,"Download slots unavailable; retry later");
}
static int Load(XAdminPluginHandle* handle)
{ Handle = handle ? *handle : NULL; return Handle ? 0 : -1; }
static void Stop(XAdminPluginHandle handle)
{
    (void)handle; xrtFree(Directory); xrtFree(MetadataPath);
    Directory = MetadataPath = NULL; memset(Packages,0,sizeof(Packages)); MetadataInvalid = false;
}
static int Start(XAdminPluginHandle handle)
{
    int group_id = 0, auth_id = 0; size_t i; Handle = handle;
    if (!Host || Host->abi_version != XADMIN_ABI_VERSION ||
        Host->size < sizeof(XAdminHostContext) || !Host->plugin_data_path) return -1;
    Directory = xrtPathJoin(Host->plugin_data_path,"packages");
    MetadataPath = xrtPathJoin(Host->plugin_data_path,"current.json");
    if (!Directory || !MetadataPath || !xrtDirCreateAll(Directory)) return -1;
    MetadataInvalid = !LoadMetadata();
    XAdminAuthGroupDecl group = {XADMIN_AUTH_SCOPE_ADMIN,"mdo-update","墨斗更新","墨斗安装包管理",990020};
    if (XAdmin_RegisterAuthGroup(handle,&group,&group_id,NULL)) return -1;
    XAdminAuthDecl auth = {XADMIN_AUTH_SCOPE_ADMIN,"mdo-update.manage",group_id,"管理墨斗更新","上传墨斗 EXE/APK",990020};
    if (XAdmin_RegisterAuth(handle,&auth,&auth_id,NULL)) return -1;
    XAdminRouteDecl routes[] = {
        {"/admin/mdo-update",Admin,true,true,auth_id,0},
        {"/admin/api/mdo-update",Inventory,true,true,auth_id,0},
        {"/admin/api/mdo-update/upload",Upload,true,true,auth_id,0},
        {"/admin/api/mdo-update/policy",Policy,true,true,auth_id,0},
        {"/update/version",Current,false,false,0,0}
    };
    for (i = 0; i < sizeof(routes)/sizeof(routes[0]); ++i)
        if (XAdmin_RegisterRoute(handle,&routes[i],NULL)) return -1;
    XAdminDynamicRouteDecl download = {0};
    download.path = download.pattern = "/update/download/{platform}/{hash}";
    download.proc = Download; download.method = XHTTP_METHOD_GET|XHTTP_METHOD_HEAD;
    if (XAdmin_RegisterDynamicRoute(handle,&download,NULL)) return -1;
    XAdminMenuDecl menu = {0};
    menu.key = "mdo-update"; menu.title = "墨斗更新"; menu.icon = "layui-icon layui-icon-upload";
    menu.type = 1; menu.open_type = "_iframe"; menu.href = "/admin/mdo-update";
    menu.sort = 990020; menu.visible = true;
    return XAdmin_RegisterMenu(handle,&menu,NULL,NULL);
}
static int Health(XAdminPluginHandle handle, XAdminHealthReport* report)
{
    (void)handle;
    if (report) { report->status_code = MetadataInvalid ? 1 : 0;
        report->message = MetadataInvalid ? "Invalid current.json" : "ready"; }
    return 0;
}
static XAdminPluginDescriptor Descriptor = {
    XADMIN_ABI_VERSION,sizeof(XAdminPluginDescriptor),"mdo-update","1.1.0","墨斗更新",
    Load,NULL,Start,NULL,Health,Stop,NULL
};
XADMIN_DECLARE_PLUGIN(Descriptor)
