/* Normalize disjoint billing categories. In particular cached input is a
 * subset of OpenAI input, but Anthropic input excludes both cache categories.
 * Anthropic output deltas are cumulative, never additive. */
static bool Gateway_Count(const xvalue* value,const char* name,int64_t* out)
{return XP_Int(value,name,0,2000000000LL,out);}
static bool Gateway_OptionalCount(const xvalue* value,const char* name,int64_t* out)
{*out=0;return !ValueHas(value,name) || Gateway_Count(value,name,out);}
static void Gateway_Reported(GatewayCall* call,const xvalue* usage)
{
    if(!call->reported_usage)call->reported_usage=ValueObject();
    const char* names[]={"input_tokens","prompt_tokens","output_tokens","completion_tokens","cache_read_input_tokens","cache_creation_input_tokens","prompt_cache_hit_tokens","prompt_cache_miss_tokens"};
    size_t i;for(i=0;i<sizeof(names)/sizeof(names[0]);i++)if(ValueHas(usage,names[i])){
        int64_t count;if(Gateway_Count(usage,names[i],&count))ValueSetInt(call->reported_usage,names[i],count);}
    const char* objects[]={"prompt_tokens_details","input_tokens_details","completion_tokens_details","output_tokens_details","cache_creation"};
    const char* fields[]={"cached_tokens","reasoning_tokens","ephemeral_5m_input_tokens","ephemeral_1h_input_tokens"};
    for(i=0;i<5;i++){xvalue* source=ValueGet(usage,objects[i]);if(!source)continue;xvalue* target=ValueObject();size_t j;
        for(j=0;j<4;j++)if(ValueHas(source,fields[j])){int64_t n;if(Gateway_Count(source,fields[j],&n))ValueSetInt(target,fields[j],n);}
        ValueSetOwn(call->reported_usage,objects[i],target);}
}
static bool Gateway_OpenAIUsage(GatewayCall* call,const xvalue* usage)
{
    GatewayUsage* out=&call->usage;int64_t input,output,cache=0,reasoning=0;
    bool responses=!strcmp(call->protocol,"responses");
    if(!Gateway_Count(usage,responses?"input_tokens":"prompt_tokens",&input) ||
        !Gateway_Count(usage,responses?"output_tokens":"completion_tokens",&output))return false;
    xvalue* details=ValueGet(usage,responses?"input_tokens_details":"prompt_tokens_details");
    if(ValueHas(details,"cached_tokens")){if(!Gateway_Count(details,"cached_tokens",&cache))return false;}
    else if(ValueHas(usage,"prompt_cache_hit_tokens") && !Gateway_Count(usage,"prompt_cache_hit_tokens",&cache))return false;
    if(!Gateway_OptionalCount(ValueGet(usage,responses?"output_tokens_details":"completion_tokens_details"),"reasoning_tokens",&reasoning) || cache>input || reasoning>output)return false;
    out->input=input-cache;out->cache_read=cache;out->output=output;out->reasoning=reasoning;
    out->input_known=out->output_known=true;Gateway_Reported(call,usage);return true;
}
static bool Gateway_AnthropicUsage(GatewayCall* call,const xvalue* usage)
{
    GatewayUsage* out=&call->usage;
    if(ValueHas(usage,"input_tokens")){
        int64_t input,read,write,five=0,hour=0;
        if(!Gateway_Count(usage,"input_tokens",&input) || !Gateway_OptionalCount(usage,"cache_read_input_tokens",&read) ||
            !Gateway_OptionalCount(usage,"cache_creation_input_tokens",&write))return false;
        xvalue* detail=ValueGet(usage,"cache_creation");
        if(detail){if(!Gateway_OptionalCount(detail,"ephemeral_5m_input_tokens",&five) ||
            !Gateway_OptionalCount(detail,"ephemeral_1h_input_tokens",&hour) || five+hour!=write)return false;}
        else five=write;
        out->input=input;out->cache_read=read;out->cache_write_5m=five;out->cache_write_1h=hour;out->input_known=true;
    }
    if(ValueHas(usage,"output_tokens")){
        int64_t output;if(!Gateway_Count(usage,"output_tokens",&output) || (out->output_known && output<out->output))return false;
        out->output=output;out->output_known=true;
    }
    Gateway_Reported(call,usage);return true;
}
static void Gateway_MeterValue(GatewayCall* call,const xvalue* value,bool stream)
{
    const char* type=ValueText(value,"type");xvalue* usage=NULL;bool final=false;
    if(ValueHas(value,"error") || (type && !strcmp(type,"error"))){call->upstream_error=true;call->usage.terminal=true;}
    if(!strcmp(call->protocol,"chat")){
        usage=ValueGet(value,"usage");
        xvalue* choices=ValueGet(value,"choices");
        final=!stream || (choices && xrtValueType(choices)==XVALUE_ARRAY && !ValueCount(choices));
        if(stream && choices && xrtValueType(choices)==XVALUE_ARRAY){uint32 i;
            for(i=0;i<ValueCount(choices);i++){xvalue* choice=xrtValueArrayGet(choices,i),*reason=ValueGet(choice,"finish_reason");
                if(reason && xrtValueType(reason)!=XVALUE_NULL && usage)final=true;}}
    }else if(!strcmp(call->protocol,"responses")){
        if(!stream){usage=ValueGet(value,"usage");final=true;call->usage.terminal=true;}
        else if(type && (!strcmp(type,"response.completed") || !strcmp(type,"response.incomplete") || !strcmp(type,"response.failed"))){
            usage=ValueGet(ValueGet(value,"response"),"usage");final=true;call->usage.terminal=true;
            if(!strcmp(type,"response.failed"))call->upstream_error=true;}
    }else{
        if(!stream){usage=ValueGet(value,"usage");final=true;call->usage.terminal=true;}
        else if(type && !strcmp(type,"message_start"))usage=ValueGet(ValueGet(value,"message"),"usage");
        else if(type && !strcmp(type,"message_delta")){
            usage=ValueGet(value,"usage");xvalue* reason=ValueGet(ValueGet(value,"delta"),"stop_reason");
            final=reason && xrtValueType(reason)!=XVALUE_NULL;
        }else if(type && !strcmp(type,"message_stop")){
            call->usage.terminal=true;if(call->usage.input_known && call->usage.output_known)call->usage.authoritative=true;}
    }
    if(usage && xrtValueType(usage)!=XVALUE_NULL){
        bool valid=xrtValueType(usage)==XVALUE_OBJECT && (!strcmp(call->protocol,"anthropic")?
            Gateway_AnthropicUsage(call,usage):Gateway_OpenAIUsage(call,usage));
        if(!valid)call->usage.invalid=true;
        else if(final && call->usage.input_known && call->usage.output_known)call->usage.authoritative=true;
    }
}
static bool Gateway_MeterEvent(GatewayCall* call)
{
    size_t n=call->event.Size;const char* bytes=call->event.Data;
    while(n && (bytes[n-1]==' ' || bytes[n-1]=='\t' || bytes[n-1]=='\n'))n--;
    if(!n)return true;
    if(n==6 && !memcmp(bytes,"[DONE]",6)){
        call->usage.terminal=true;
        if(call->usage.input_known && call->usage.output_known)call->usage.authoritative=true;
        return true;
    }
    xvalue* value=xrtJsonParse(xrtStrViewN(bytes,n));
    if(!value || xrtValueType(value)!=XVALUE_OBJECT){xrtValueRelease(value);return false;}
    Gateway_MeterValue(call,value,true);xrtValueRelease(value);return true;
}
static bool Gateway_MeterLine(GatewayCall* call)
{
    const char* bytes=call->line.Data;size_t n=call->line.Size;
    if(call->sse_first_line){call->sse_first_line=false;if(n>=3 && !memcmp(bytes,"\xef\xbb\xbf",3)){bytes+=3;n-=3;}}
    if(!n){bool ok=Gateway_MeterEvent(call);call->event.Size=0;return ok;}
    if(n>=5 && !memcmp(bytes,"data:",5)){
        bytes+=5;n-=5;if(n && bytes[0]==' '){bytes++;n--;}
        if(call->event.Size+n+1>1048576)return false;
        return xrtBufferAppend(&call->event,(xbytesview){(cbytes)bytes,n}) &&
            xrtBufferAppend(&call->event,(xbytesview){(cbytes)"\n",1});
    }return true;
}
static bool Gateway_MeterFeed(GatewayCall* call,const void* data,size_t size)
{
    if(!call->sse)return call->json.Size+size<=1048576 && xrtBufferAppend(&call->json,(xbytesview){data,size});
    const unsigned char* bytes=data;size_t i=0;
    while(i<size){
        unsigned char c=bytes[i];if(call->previous_cr && c=='\n'){call->previous_cr=false;i++;continue;}call->previous_cr=false;
        if(c=='\r' || c=='\n'){if(!Gateway_MeterLine(call))return false;call->line.Size=0;call->previous_cr=c=='\r';i++;}
        else{size_t start=i;while(i<size && bytes[i]!='\r' && bytes[i]!='\n'){if(!bytes[i])return false;i++;}
            size_t n=i-start;if(call->line.Size+n>1048576 || !xrtBufferAppend(&call->line,(xbytesview){bytes+start,n}))return false;}
    }return true;
}
static bool Gateway_MeterFinish(GatewayCall* call)
{
    if(call->sse)return call->usage.terminal && !call->line.Size && !call->event.Size;
    xvalue* value=xrtJsonParse(xrtStrViewN(call->json.Data,call->json.Size));
    bool valid=value && xrtValueType(value)==XVALUE_OBJECT;
    if(valid)Gateway_MeterValue(call,value,false);xrtValueRelease(value);return valid;
}
static bool Gateway_Calculate(const GatewayUsage* usage,const GatewayRates* rates,int discount,int64_t* amount)
{
    const int64_t counts[]={usage->input,usage->cache_read,usage->cache_write_5m,usage->cache_write_1h,usage->output};
    const int64_t prices[]={rates->input,rates->cache_read,rates->cache_write_5m,rates->cache_write_1h,rates->output};
    int64_t sum=0;size_t i;if(discount<1 || discount>10000)return false;
    for(i=0;i<5;i++){
        if(counts[i]<0 || prices[i]<0 || (prices[i] && counts[i]>(INT64_MAX-sum)/prices[i]))return false;
        sum+=counts[i]*prices[i];
    }
    /* Divide before multiplying to avoid 128-bit/compiler-specific arithmetic.
     * Retain the remainder and round UP once for the entire request. */
    const int64_t denominator=10000000000LL;
    int64_t whole=sum/denominator*discount,fraction=sum%denominator*discount;
    *amount=whole+fraction/denominator+(fraction%denominator!=0);return *amount<=XBILL_MAX_AMOUNT;
}
static bool Gateway_ReserveAmount(const GatewayCall* call,int64_t* amount)
{
    GatewayRates prices=call->model.price.sale;int64_t maximum=prices.input;
    if(prices.cache_read>maximum)maximum=prices.cache_read;if(prices.cache_write_5m>maximum)maximum=prices.cache_write_5m;if(prices.cache_write_1h>maximum)maximum=prices.cache_write_1h;
    GatewayUsage bound={0};bound.input=call->model.context_window;bound.output=call->output_limit;prices.input=maximum;
    return Gateway_Calculate(&bound,&prices,call->discount_bps,amount);
}
static xvalue* Gateway_UsageValue(const GatewayCall* call,bool verified)
{
    xvalue* value=ValueObject();ValueSetText(value,"usage_source",verified?"provider_reported":"unresolved");
    if(call->usage.input_known){ValueSetInt(value,"input_tokens",call->usage.input);ValueSetInt(value,"cache_read_tokens",call->usage.cache_read);
        ValueSetInt(value,"cache_write_5m_tokens",call->usage.cache_write_5m);ValueSetInt(value,"cache_write_1h_tokens",call->usage.cache_write_1h);}
    if(call->usage.output_known){ValueSetInt(value,"output_tokens",call->usage.output);ValueSetInt(value,"reasoning_tokens",call->usage.reasoning);}
    if(call->reported_usage)ValueSetRef(value,"reported_usage",call->reported_usage);return value;
}
