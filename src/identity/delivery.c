#include "../../include/xadmin/identity_delivery.h"
static XAIdentityDeliveryProc G_IdentityDelivery;
static void* G_IdentityDeliveryContext;
static bool XA_SetIdentityDelivery(XAIdentityDeliveryProc proc,void* context)
{
    if(G_Ready)return false;G_IdentityDelivery=proc;G_IdentityDeliveryContext=context;return true;
}
typedef struct XADeliveryJob {
    XAIdentityMessage message;
    XASmsConfig sms;
    bool registered_sms;
    char sms_url[1025],sms_token[513];
    char smtp_host[257],smtp_user[257],smtp_password[513],smtp_sender[255];
    int smtp_port;bool starttls,smtp_auth;
} XADeliveryJob;
static bool XA_DeliveryPrepare(const XAIdentityMessage* message,XADeliveryJob* job)
{
    memset(job,0,sizeof(*job));job->message=*message;
    if(G_IdentityDelivery)return true;
    if(!strcmp(message->channel,"phone")){
        xvalue* value=XA_SmsConfigValue(&G_Identity.sms,false);
        bool configured=ValueBool(value,"enabled");xrtValueRelease(value);
        if(configured){
            job->sms=G_Identity.sms;job->registered_sms=true;
            return XA_SmsAvailable(&job->sms,message->security_notice?"contact_changed":"verification");
        }
        strcpy(job->sms_url,G_Identity.sms_url);strcpy(job->sms_token,G_Identity.sms_token);
        return job->sms_url[0]&&job->sms_token[0];
    }
#if XADMIN_WITH_SMTP
    if(!Mail_Enabled())return false;
    const char* secure=Mail_GetText("smtp_secure","ssl");
    if(strcmp(secure,"ssl")&&strcmp(secure,"starttls"))return false;
    job->starttls=!strcmp(secure,"starttls");job->smtp_auth=Mail_GetBool("smtp_auth",true);
    job->smtp_port=Mail_GetInt("smtp_port",465);
    const char* values[]={Mail_GetText("smtp_host",""),Mail_GetText("smtp_username",""),Mail_GetText("smtp_password",""),Mail_GetText("mail_from_email","")};
    char* outputs[]={job->smtp_host,job->smtp_user,job->smtp_password,job->smtp_sender};
    size_t caps[]={sizeof(job->smtp_host),sizeof(job->smtp_user),sizeof(job->smtp_password),sizeof(job->smtp_sender)};int i;
    for(i=0;i<4;i++){if(strlen(values[i])>=caps[i]||strpbrk(values[i],"\r\n"))return false;strcpy(outputs[i],values[i]);}
    return job->smtp_host[0]&&job->smtp_sender[0]&&job->smtp_port>0&&job->smtp_port<=65535&&
        (!job->smtp_auth||(job->smtp_user[0]&&job->smtp_password[0]));
#else
    return false;
#endif
}
/* Called under the request lock, like Prepare. This probe never sends a message. */
static bool XA_DeliveryAvailable(const char* channel)
{
    XAIdentityMessage message={0};XADeliveryJob job;message.channel=channel;
    bool available=XA_DeliveryPrepare(&message,&job);xrtSecureZero(&job,sizeof(job));return available;
}
/* Transports use owned option snapshots outside the application lock. SMTP
 * verifies the system trust chain and never downgrades to cleartext. */
static XAIdentityDeliveryResult XA_Deliver(XADeliveryJob* job)
{
    const XAIdentityMessage* m=&job->message;
    if(G_IdentityDelivery)return G_IdentityDelivery(m,G_IdentityDeliveryContext);
    if(!m->borrowed_engine)return XA_DELIVERY_FAILED;
    if(!strcmp(m->channel,"phone")){
        if(job->registered_sms){
            xvalue* params=ValueObject();char seconds[16],minutes[16];
            snprintf(seconds,sizeof(seconds),"%u",m->expires_in);snprintf(minutes,sizeof(minutes),"%u",(m->expires_in+59)/60);
            bool ok=params!=NULL;
            if(!m->security_notice)ok=ok&&ValueSetText(params,"code",m->code)&&ValueSetText(params,"purpose",m->purpose)&&
                ValueSetText(params,"expires_in",seconds)&&ValueSetText(params,"minutes",minutes);
            XASmsMessage message={m->security_notice?XA_SMS_NOTIFICATION:XA_SMS_VERIFICATION,m->target,
                m->security_notice?"contact_changed":"verification",params,m->challenge_id};
            XASmsContext context={m->borrowed_engine,G_SmsTransport,G_SmsTransportContext,NULL};XASmsReceipt receipt;
            XASmsStatus status=ok?XA_SmsSend(&job->sms,&context,&message,&receipt):XA_SMS_FAILED;
            xrtValueRelease(params);
            return status==XA_SMS_ACCEPTED?XA_DELIVERY_SENT:status==XA_SMS_UNKNOWN?XA_DELIVERY_UNKNOWN:XA_DELIVERY_FAILED;
        }
        xoauth2httpxrt* http=xoauth2HttpXrtCreate(m->borrowed_engine,NULL,15000000);
        if(!http)return XA_DELIVERY_FAILED;
        char* target=xoauth2UrlEncode(m->target);char* form=target?xrtFormat("challenge_id=%s&phone=%s&code=%s&purpose=%s&expires_in=%u&type=%s",m->challenge_id,target,m->code,m->purpose,m->expires_in,m->security_notice?"security_notice":"verification"):NULL;
        char* auth=xrtFormat("Bearer %s",job->sms_token);char* response=NULL;int status=0;
        bool transported=form&&auth&&xoauth2HttpXrt("POST",job->sms_url,form,auth,&response,&status,http);
        xvalue* value=transported&&status>=200&&status<300&&response?xrtJsonParse(xrtStrView(response)):NULL;
        XAIdentityDeliveryResult result=value&&ValueBool(value,"sent")?XA_DELIVERY_SENT:
            (status>=400&&status<500?XA_DELIVERY_FAILED:XA_DELIVERY_UNKNOWN);
        if(form)xrtSecureZero(form,strlen(form));xrtFree(form);xrtFree(target);xrtFree(auth);xrtFree(response);xrtValueRelease(value);
        /* Borrowed engines have no retirement loop; Cleanup joins its resolver
         * and releases transport resources. It never destroys xs' engine. */
        bool cleaned=xoauth2HttpXrtCleanup(http);if(cleaned)xoauth2HttpXrtDestroy(http);
        return cleaned?result:XA_DELIVERY_UNKNOWN;
    }
#if XADMIN_WITH_SMTP
    xnetresolver* resolver=NULL;xtlscontext* tls=NULL;xtlsverifier* verifier=NULL;xsmtpclient* client=NULL;
    xx509store* store=NULL;char* composed=NULL;size_t size=0;bool sent=false,submitted=false;
    xnetresolverconfig resolver_config;xrtNetResolverConfigInit(&resolver_config);resolver=xrtNetResolverCreate(&resolver_config);
    xtlscontextconfig tls_config;xrtTlsContextConfigInit(&tls_config);tls=xrtTlsContextCreate(&tls_config);
    xtlsverifierconfig verify_config;xrtTlsVerifierConfigInit(&verify_config);store=xrtX509StoreSystem();
    if(store){verify_config.Store=store;verifier=xrtTlsVerifierCreate(&verify_config);}xrtX509StoreFree(store);
    if(resolver&&tls&&verifier){
        xsmtpclientconfig config;xrtSmtpClientConfigInit(&config);config.Net.Engine=m->borrowed_engine;
        config.Net.Resolver=resolver;config.Net.Host=job->smtp_host;config.Net.Port=(uint16)job->smtp_port;
        config.Net.Security=job->starttls?XMAIL_SECURITY_STARTTLS:XMAIL_SECURITY_TLS;
        config.Net.Tls.Context=tls;config.Net.Tls.Verifier=verifier;
        XAdminDeadline deadline=XAdmin_DeadlineAfterMs(15000);client=xrtSmtpClientOpen(&config,XAdmin_DeadlineRemainingMs(deadline),NULL);
        bool authenticated=client!=NULL;
        if(client&&job->smtp_auth){xsmtpauthconfig auth;xrtSmtpAuthConfigInit(&auth);
            auth.Method=XSMTP_AUTH_PLAIN;auth.Username=xrtStrView(job->smtp_user);auth.Secret=xrtStrView(job->smtp_password);
            authenticated=job->smtp_user[0]&&job->smtp_password[0]&&xrtSmtpClientAuth(client,&auth,XAdmin_DeadlineRemainingMs(deadline),NULL);}
        if(authenticated){
            char text[256];
            if(m->security_notice)snprintf(text,sizeof(text),"Your verified account contact was changed. If this was not you, sign in using another login method and review your sessions.\r\n");
            else snprintf(text,sizeof(text),"Verification code: %s\r\nPurpose: %s\r\nExpires in: %u seconds\r\n",m->code,m->purpose,m->expires_in);
            xmailmessage mail;xrtMailMessageInit(&mail);xmailaddress to={0};to.Address=xrtStrView(m->target);to.Name=xrtStrView("");
            mail.From.Address=xrtStrView(job->smtp_sender);mail.From.Name=xrtStrView("Account service");mail.To=&to;mail.ToCount=1;
            mail.Subject=xrtStrView(m->security_notice?"Account security notification":"Account verification");mail.Text=xrtStrView(text);composed=xrtMailCompose(&mail,&size);
            if(composed&&xrtSmtpClientMail(client,xrtStrView(job->smtp_sender),xrtStrView(""),XAdmin_DeadlineRemainingMs(deadline),NULL)&&
                xrtSmtpClientRcpt(client,xrtStrView(m->target),xrtStrView(""),XAdmin_DeadlineRemainingMs(deadline),NULL)){
                submitted=true;sent=xrtSmtpClientData(client,xrtStrViewN(composed,size),XAdmin_DeadlineRemainingMs(deadline),NULL);}
            xrtSecureZero(text,sizeof(text));
        }
        if(client)xrtSmtpClientQuit(client,XAdmin_DeadlineRemainingMs(deadline),NULL);
    }
    if(composed)xrtSecureZero(composed,size);xrtFree(composed);
    if(client)xrtSmtpClientDestroy(client);if(resolver)xrtNetResolverDestroy(resolver);
    if(tls)xrtTlsContextRelease(tls);if(verifier)xrtTlsVerifierRelease(verifier);
    return sent?XA_DELIVERY_SENT:submitted?XA_DELIVERY_UNKNOWN:XA_DELIVERY_FAILED;
#else
    return XA_DELIVERY_FAILED;
#endif
}
