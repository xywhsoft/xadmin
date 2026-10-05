/* Fingerprints are structural: JSON whitespace/object key order do not create
 * another operation. Array order and scalar types remain significant. Bounds
 * keep malformed schema trees from consuming an unbounded application lock. */
typedef struct GatewayHashKey {xstrview key;xvalue* value;} GatewayHashKey;
static int Gateway_KeyCompare(xstrview a,xstrview b)
{
    size_t n=a.Size<b.Size?a.Size:b.Size;int result=memcmp(a.Data,b.Data,n);
    return result?result:a.Size<b.Size?-1:a.Size>b.Size?1:0;
}
static bool Gateway_HashPart(xsha256* hash,const void* bytes,size_t size)
{
    char length[32];int n=snprintf(length,sizeof(length),"%llu:",(unsigned long long)size);
    return n>0 && xrtSha256Update(hash,length,(size_t)n) && (!size || xrtSha256Update(hash,bytes,size));
}
static bool Gateway_HashValue(xsha256* hash,const xvalue* value,int depth,int* nodes)
{
    if(!value || depth>32 || ++*nodes>20000)return false;
    int type=(int)xrtValueType(value);unsigned char type_tag=(unsigned char)type;
    if(!Gateway_HashPart(hash,&type_tag,sizeof(type_tag)))return false;
    if(type==XVALUE_OBJECT || type==XVALUE_ARRAY){char count[32];int n=snprintf(count,sizeof(count),"%llu",(unsigned long long)ValueCount(value));
        if(n<1 || !Gateway_HashPart(hash,count,(size_t)n))return false;}
    if(type==XVALUE_OBJECT){
        size_t count=ValueCount(value);if(count>512)return false;
        GatewayHashKey* keys=count?xrtCalloc(count,sizeof(*keys)):NULL;if(count&&!keys)return false;
        xvalueiter it={0};xvaluekey key;xvalue* child;size_t used=0,i;bool ok=true;
        if(xrtValueIterBegin(value,&it)){while((child=xrtValueIterNext(&it,&key))){
            if(used>=count || key.Type!=XVALUE_KEY_STRING){ok=false;break;}
            keys[used++]=(GatewayHashKey){key.String,child};}xrtValueIterEnd(&it);}
        if(used!=count)ok=false;
        for(i=1;ok && i<count;i++){GatewayHashKey current=keys[i];size_t j=i;
            while(j && Gateway_KeyCompare(keys[j-1].key,current.key)>0){keys[j]=keys[j-1];j--;}keys[j]=current;}
        for(i=0;ok && i<count;i++)ok=Gateway_HashPart(hash,keys[i].key.Data,keys[i].key.Size) && Gateway_HashValue(hash,keys[i].value,depth+1,nodes);
        xrtFree(keys);return ok;
    }
    if(type==XVALUE_ARRAY){size_t i;for(i=0;i<ValueCount(value);i++)if(!Gateway_HashValue(hash,xrtValueArrayGet(value,i),depth+1,nodes))return false;return true;}
    size_t size=0;char* text=xrtJsonStringify(value,false,&size);bool ok=text && Gateway_HashPart(hash,text,size);
    if(text)xrtSecureZero(text,size);xrtFree(text);return ok;
}
static bool Gateway_Fingerprint(const xvalue* body,char out[65])
{
    xsha256 hash;unsigned char digest[32];int nodes=0;size_t i;xrtSha256Init(&hash);
    bool ok=Gateway_HashValue(&hash,body,0,&nodes) && xrtSha256Final(&hash,digest);
    if(ok)for(i=0;i<32;i++)snprintf(out+i*2,3,"%02x",digest[i]);
    xrtSecureZero(&hash,sizeof(hash));xrtSecureZero(digest,sizeof(digest));return ok;
}
static bool Gateway_ListContains(const char* list,const char* word)
{
    if(!list || !word || !*word)return false;size_t n=strlen(word);const char* p=list;
    while(*p){const char* end=strchr(p,',');if(!end)end=p+strlen(p);if((size_t)(end-p)==n && !memcmp(p,word,n))return true;if(!*end)break;p=end+1;}return false;
}
static bool Gateway_InputTree(const xvalue* value,bool vision,int depth,int* nodes)
{
    if(!value || depth>32 || ++*nodes>20000)return false;
    if(xrtValueType(value)==XVALUE_OBJECT){
        const char* type=ValueText(value,"type");
        if(type){
            if(!strcmp(type,"input_file") || !strcmp(type,"document") || !strcmp(type,"input_audio") || !strcmp(type,"audio"))return false;
            if(!strcmp(type,"image") || !strcmp(type,"image_url") || !strcmp(type,"input_image")){
                if(!vision || ValueHas(value,"file_id") || ValueHas(ValueGet(value,"source"),"file_id"))return false;}
        }
        xvalueiter it={0};xvaluekey key;xvalue* child;bool ok=true;
        if(xrtValueIterBegin(value,&it)){while((child=xrtValueIterNext(&it,&key)))if(!Gateway_InputTree(child,vision,depth+1,nodes)){ok=false;break;}xrtValueIterEnd(&it);}return ok;
    }
    if(xrtValueType(value)==XVALUE_ARRAY){size_t i;for(i=0;i<ValueCount(value);i++)if(!Gateway_InputTree(xrtValueArrayGet(value,i),vision,depth+1,nodes))return false;}
    return true;
}
static bool Gateway_Tools(const GatewayCall* call,const xvalue* body)
{
    xvalue* tools=ValueGet(body,"tools"),*functions=ValueGet(body,"functions");
    if((tools || functions) && !call->model.tool_calling)return false;
    if(functions && (strcmp(call->protocol,"chat") || xrtValueType(functions)!=XVALUE_ARRAY || ValueCount(functions)>128))return false;
    if(!tools)return true;if(xrtValueType(tools)!=XVALUE_ARRAY || ValueCount(tools)>128)return false;
    uint32 i;for(i=0;i<ValueCount(tools);i++){
        xvalue* tool=xrtValueArrayGet(tools,i);if(!tool || xrtValueType(tool)!=XVALUE_OBJECT)return false;
        const char* type=ValueText(tool,"type");
        if(!strcmp(call->protocol,"anthropic")){if(ValueHas(tool,"type") && (!type || strcmp(type,"custom")))return false;}
        else if(!type || (strcmp(type,"function") && (strcmp(call->protocol,"responses") || strcmp(type,"custom"))))return false;
    }return true;
}
static bool Gateway_ValidateBody(GatewayCall* call,xvalue* body,char project[129],char session[129],char task[129])
{
    const char* fields[]={"model","messages","input","instructions","system","temperature","top_p","top_k","stop","stop_sequences","stream","stream_options","max_tokens","max_completion_tokens","max_output_tokens","tools","tool_choice","parallel_tool_calls","functions","function_call","reasoning","reasoning_effort","thinking","text","response_format","seed","logprobs","top_logprobs","logit_bias","presence_penalty","frequency_penalty","n","user","metadata","store","background","previous_response_id","conversation","include","truncation","service_tier","safety_identifier","xadmin_expected_price_version","xadmin_metadata"};
    if(!XP_Fields(body,fields,sizeof(fields)/sizeof(fields[0])) || !Gateway_Tools(call,body))return false;
    xvalue* input=ValueGet(body,!strcmp(call->protocol,"responses")?"input":"messages");int nodes=0;
    if(!input || (!strcmp(call->protocol,"responses")?xrtValueType(input)!=XVALUE_ARRAY && xrtValueType(input)!=XVALUE_STRING:xrtValueType(input)!=XVALUE_ARRAY) ||
        !Gateway_InputTree(input,call->model.vision,0,&nodes))return false;
    if(xrtValueType(input)==XVALUE_ARRAY && !ValueCount(input))return false;
    call->stream=false;if(ValueHas(body,"stream") && !xrtValueGetBool(ValueGet(body,"stream"),&call->stream))return false;
    if(ValueHas(body,"n")){int64_t count;if(!XP_Int(body,"n",1,1,&count))return false;}
    const char* flags[]={"store","background"};size_t i;
    for(i=0;i<2;i++)if(ValueHas(body,flags[i])){bool enabled;if(!xrtValueGetBool(ValueGet(body,flags[i]),&enabled) || enabled)return false;}
    if(ValueHas(body,"previous_response_id") || ValueHas(body,"conversation"))return false;
    const char* limits[]={"max_tokens","max_completion_tokens","max_output_tokens"};
    const char* selected=!strcmp(call->protocol,"responses")?"max_output_tokens":!strcmp(call->protocol,"anthropic")?"max_tokens":call->model.output_field;
    call->output_limit=call->model.max_output;
    for(i=0;i<3;i++)if(ValueHas(body,limits[i])){int64_t count;
        if(strcmp(selected,limits[i]) || !XP_Int(body,limits[i],1,call->model.max_output,&count))return false;call->output_limit=(int)count;}
    if(ValueHas(body,"reasoning_effort")){
        const char* effort=XP_Text(body,"reasoning_effort",16);if(!Gateway_ListContains(call->model.reasoning,effort))return false;}
    xvalue* reasoning=ValueGet(body,"reasoning");if(ValueHas(reasoning,"effort")){
        const char* effort=XP_Text(reasoning,"effort",16);if(!Gateway_ListContains(call->model.reasoning,effort))return false;}
    xvalue* thinking=ValueGet(body,"thinking");if(ValueHas(thinking,"budget_tokens")){
        int64_t count;if(!XP_Int(thinking,"budget_tokens",1,call->output_limit-1,&count))return false;}
    if(ValueHas(body,"service_tier")){const char* tier=XP_Text(body,"service_tier",16);if(!tier || strcmp(tier,"default"))return false;}
    xvalue* metadata=ValueGet(body,"xadmin_metadata");const char* names[]={"project_id","session_id","task_id"};
    if(metadata && !XP_Fields(metadata,names,3))return false;
    char* destinations[]={project,session,task};
    for(i=0;i<3;i++){destinations[i][0]=0;if(ValueHas(metadata,names[i])){
        const char* text=XP_Text(metadata,names[i],128);if(!text)return false;snprintf(destinations[i],129,"%s",text);}}
    /* Force actual generation bounds and disable upstream-owned state. These
     * are declared gateway limits, not silent cross-protocol conversions. */
    if(!ValueSetInt(body,selected,call->output_limit) || !ValueSetText(body,"model",call->wire_model))return false;
    if(!strcmp(call->protocol,"responses") && !ValueSetBool(body,"store",false))return false;
    if(!strcmp(call->protocol,"chat") && call->stream){
        xvalue* options=ValueGet(body,"stream_options");
        if(options && xrtValueType(options)!=XVALUE_OBJECT)return false;
        if(!options){options=ValueObject();if(!ValueSetOwn(body,"stream_options",options))return false;}
        if(!ValueSetBool(options,"include_usage",true))return false;
    }
    xrtValueObjectRemove(body,XRT_STR_LITERAL("xadmin_metadata"));xrtValueObjectRemove(body,XRT_STR_LITERAL("xadmin_expected_price_version"));return true;
}
