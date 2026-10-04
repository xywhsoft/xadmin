#include "providers/webhook.c"
#include "providers/aliyun.c"
#include "providers/tencent.c"
#include "providers/huawei.c"
#include "providers/baidu.c"
#include "providers/yunpian.c"
#include "providers/chuanglan.c"
#include "providers/ronglian.c"
#include "providers/submail.c"
static bool XA_SmsRegisterBuiltins(void)
{
    const XASmsProvider* providers[] = {&XA_SmsWebhook,&XA_SmsAliyun,&XA_SmsTencent,&XA_SmsHuawei,
        &XA_SmsBaidu,&XA_SmsYunpian,&XA_SmsChuanglan,&XA_SmsRonglian,&XA_SmsSubmail};
    size_t i;
    for(i=0;i<sizeof(providers)/sizeof(providers[0]);++i)if(!XA_SmsRegister(providers[i]))return false;
    return true;
}
