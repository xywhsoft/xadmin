#include <xs_plugin.h>
#include <value_util.h>
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
#include "src/internal.h"
#include "src/storage.c"
#include "src/protocol.c"
#include "src/api.c"
#include "src/tickets.c"

XADMIN_EXPORT void XAdmin_PluginSetGlobalData(int slot,void* value)
{if(slot==XADMIN_GLOBAL_HOST_CONTEXT)Host=value;}
static bool Relay_ConfigInt(xvalue* value,const char* key,int min,int max,int* out)
{int64 number=0;if(!xrtValueGetInt(ValueGet(value,key),&number)||number<min||number>max)return false;*out=(int)number;return true;}
static int Relay_ConfigChanged(XAdminPluginHandle handle,xvalue* value)
{
    (void)handle;RelayConfig next={0};const char* fields[]={"enabled","max_devices_per_member","max_viewers_per_device","ticket_ttl_seconds"};
    if(!Relay_Fields(value,fields,4)||!xrtValueGetBool(ValueGet(value,"enabled"),&next.enabled)||
        !Relay_ConfigInt(value,"max_devices_per_member",1,32,&next.devices)||
        !Relay_ConfigInt(value,"max_viewers_per_device",0,4,&next.viewers)||
        !Relay_ConfigInt(value,"ticket_ttl_seconds",5,60,&next.ttl))return -1;
    Config=next;if(!Config.enabled){size_t i;
        for(i=0;i<RELAY_TICKETS;i++)Relay_TicketClear(&Tickets[i]);
        for(i=0;i<RELAY_CONNECTIONS;i++)if(Connections[i].channel)XAdmin_ChannelClose(Handle,Connections[i].channel,1001);
    }return 0;
}
static int Relay_Load(XAdminPluginHandle* out){Handle=out?*out:NULL;return Handle?0:-1;}
static int Relay_Start(XAdminPluginHandle handle)
{
    Handle=handle;if(!Host||!Host->plugin_private_db_path||sqlite3_open(Host->plugin_private_db_path,&Database)!=SQLITE_OK)return -1;
    sqlite3_busy_timeout(Database,1000);if(!Relay_StorageInit())return -1;
#if !defined(_WIN32) && !defined(_WIN64)
    if(!xrtPathSetMode(Host->plugin_private_db_path,false,0600))return -1;
#endif
    XAdminRouteDecl routes[]={
        {"/api/v1/devices",Relay_List,false,false,0,0},
        {"/api/v1/devices/register",Relay_Register,false,false,0,0},
        {"/api/v1/devices/revoke",Relay_RemoveOrRevoke,false,false,0,0},
        {"/api/v1/devices/remove",Relay_RemoveOrRevoke,false,false,0,0},
        {"/api/v1/devices/ticket",Relay_Ticket,false,false,0,0},
        {"/api/v1/devices/connect",Relay_Connect,false,false,0,0}
    };
    size_t i;for(i=0;i<sizeof(routes)/sizeof(routes[0]);i++)if(XAdmin_RegisterRoute(handle,&routes[i],NULL)!=0)return -1;
    return 0;
}
static void Relay_Stop(XAdminPluginHandle handle)
{
    (void)handle;size_t i;/* Host has already joined every channel callback. */
    for(i=0;i<RELAY_TICKETS;i++)Relay_TicketClear(&Tickets[i]);
    xrtSecureZero(Connections,sizeof(Connections));sqlite3_close(Database);Database=NULL;
}
static int Relay_Health(XAdminPluginHandle handle,XAdminHealthReport* report)
{(void)handle;if(report){report->status_code=Database?0:1;report->message=Database?"ready":"storage unavailable";}return 0;}
static const XAdminPluginDescriptor Descriptor={XADMIN_ABI_VERSION,sizeof(XAdminPluginDescriptor),
    "device-relay","1.0.0","会员设备中继",Relay_Load,NULL,Relay_Start,Relay_ConfigChanged,Relay_Health,Relay_Stop,NULL};
XADMIN_DECLARE_PLUGIN(Descriptor)
