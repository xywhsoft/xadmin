// ============================================
// 基础插件示例
// 演示依赖管理和命名空间功能
// ============================================

#include "plugin.h"

// 全局上下文
PluginContext* ctx;

// 插件设置
xvalue g_tblSettings = NULL;

// 内部数据
int g_iCounter = 0;



// 接收主系统传递的全局数据
void Plugin_SetGlobalData(int idx, void* ptr)
{
    if ( idx == 1 ) {
        ctx = (PluginContext*)ptr;
    } else if ( idx == 2 ) {
        g_tblSettings = (xvalue)ptr;
    }
}



// ==================== 导出函数（供其他插件使用） ====================

// 计算加法
int PLUGIN_FUNC(Calculate)(int a, int b)
{
    return a + b;
}

// 获取内部数据
int PLUGIN_FUNC(GetData)()
{
    return g_iCounter;
}



// ==================== 路由处理 ====================

// API: 获取插件信息
void PLUGIN_API(GetInfo)(void* objServer, void* objHost,
                        struct mg_connection* c, struct mg_http_message* hm)
{
    xvalue tblRet = xvoCreateTable();
    xvoTableSetBool(tblRet, "result", 6, TRUE);
    xvoTableSetText(tblRet, "name", 4, "base_plugin", 0, FALSE);
    xvoTableSetText(tblRet, "version", 7, "1.0.0", 0, FALSE);
    xvoTableSetInt(tblRet, "counter", 7, g_iCounter);

    size_t iSize = 0;
    str sJson = ctx->JsonStringify(tblRet, &iSize);
    ctx->SendJson(c, 200, sJson, iSize);
    ctx->Free(sJson);
    xvoUnref(tblRet);
}

// API: 增加计数器
void PLUGIN_API(Increment)(void* objServer, void* objHost,
                         struct mg_connection* c, struct mg_http_message* hm)
{
    g_iCounter++;

    xvalue tblRet = xvoCreateTable();
    xvoTableSetBool(tblRet, "result", 6, TRUE);
    xvoTableSetInt(tblRet, "counter", 7, g_iCounter);

    size_t iSize = 0;
    str sJson = ctx->JsonStringify(tblRet, &iSize);
    ctx->SendJson(c, 200, sJson, iSize);
    ctx->Free(sJson);
    xvoUnref(tblRet);
}



// ==================== 路由注册 ====================

void BasePlugin_RegisterRoutes()
{
    ctx->AddRoute("/api/base/info", PLUGIN_API(GetInfo), FALSE, FALSE, 0, 0);
    ctx->AddRoute("/api/base/increment", PLUGIN_API(Increment), FALSE, FALSE, 0, 0);
}

void BasePlugin_UnregisterRoutes()
{
    ctx->RemoveRoute("/api/base/info");
    ctx->RemoveRoute("/api/base/increment");
}



// ==================== 事件处理 ====================

void BasePlugin_OnSystemReady(xvalue eventData)
{
    ctx->Log(LOG_INFO, "[BasePlugin] System ready, registering exports...");

    // 注册导出函数（供其他插件调用）
    ctx->SetPluginExport("base_plugin", "Calculate", PLUGIN_FUNC(Calculate));
    ctx->SetPluginExport("base_plugin", "GetData", PLUGIN_FUNC(GetData));

    ctx->Log(LOG_INFO, "[BasePlugin] Exports registered successfully!");
}



// ==================== 插件入口 ====================

// 插件初始化（启用时调用）
void Plugin_base_plugin_Init()
{
    ctx->Log(LOG_INFO, "[BasePlugin] Initializing...");

    BasePlugin_RegisterRoutes();

    // 监听系统就绪事件，在事件中注册导出
    ctx->OnEvent(EVENT_SYSTEM_READY, BasePlugin_OnSystemReady);

    // 初始化计数器
    if ( g_tblSettings ) {
        g_iCounter = xvoTableGetInt(g_tblSettings, "maxItems", 9);
    } else {
        g_iCounter = 100;
    }

    ctx->Log(LOG_INFO, "[BasePlugin] Initialized successfully!");
}



// 插件卸载（禁用时调用）
void Plugin_base_plugin_Unit()
{
    ctx->Log(LOG_INFO, "[BasePlugin] Unloading...");

    // 取消事件监听
    ctx->OffEvent(EVENT_SYSTEM_READY, BasePlugin_OnSystemReady);

    BasePlugin_UnregisterRoutes();

    ctx->Log(LOG_INFO, "[BasePlugin] Unloaded successfully!");
}
