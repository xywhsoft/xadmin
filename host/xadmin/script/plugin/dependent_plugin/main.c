// ============================================
// 依赖插件示例
// 演示如何使用其他插件的导出函数
// ============================================

#include "plugin.h"

// 全局上下文
PluginContext* ctx;

// 插件设置
xvalue g_tblSettings = NULL;

// base_plugin 导出函数指针
typedef int (*FnCalculate)(int, int);
typedef int (*FnGetData)();
FnCalculate g_fnCalculate = NULL;
FnGetData g_fnGetData = NULL;



// 接收主系统传递的全局数据
void Plugin_SetGlobalData(int idx, void* ptr)
{
    if ( idx == 1 ) {
        ctx = (PluginContext*)ptr;
    } else if ( idx == 2 ) {
        g_tblSettings = (xvalue)ptr;
    }
}



// ==================== 路由处理 ====================

// API: 使用base_plugin的计算功能
void PLUGIN_API(Calculate)(void* objServer, void* objHost,
                           XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
    // 解析参数
    xvalue form = ctx->JsonParse((str)xsReqBody(objReq), xsReqBodyLen(objReq));
    if ( !form ) {
        ctx->SendJson(objResp, 200, "{\"result\":false,\"message\":\"参数错误\"}", 0);
        return;
    }

    int iA = xvoTableGetInt(form, "a", 1);
    int iB = xvoTableGetInt(form, "b", 1);
    xvoUnref(form);

    // 获取乘数
    int iMultiplier = 2;
    if ( g_tblSettings ) {
        iMultiplier = xvoTableGetInt(g_tblSettings, "multiplier", 10);
    }

    // 调用 base_plugin 的导出函数
    int iSum = 0;
    if ( g_fnCalculate ) {
        iSum = g_fnCalculate(iA, iB);
    } else {
        ctx->Log(LOG_WARN, "[DependentPlugin] Calculate function not available");
    }

    // 应用乘数
    int iResult = iSum * iMultiplier;

    xvalue tblRet = xvoCreateTable();
    xvoTableSetBool(tblRet, "result", 6, TRUE);
    xvoTableSetText(tblRet, "plugin", 6, "dependent_plugin", 0, FALSE);
    xvoTableSetInt(tblRet, "a", 1, iA);
    xvoTableSetInt(tblRet, "b", 1, iB);
    xvoTableSetInt(tblRet, "sum", 3, iSum);
    xvoTableSetInt(tblRet, "multiplier", 10, iMultiplier);
    xvoTableSetInt(tblRet, "result_value", 12, iResult);

    size_t iSize = 0;
    str sJson = ctx->JsonStringify(tblRet, &iSize);
    ctx->SendJson(objResp, 200, sJson, iSize);
    ctx->Free(sJson);
    xvoUnref(tblRet);
}

// API: 获取base_plugin的数据
void PLUGIN_API(GetBaseData)(void* objServer, void* objHost,
                             XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
    // 调用 base_plugin 的导出函数
    int iData = 0;
    if ( g_fnGetData ) {
        iData = g_fnGetData();
    } else {
        ctx->Log(LOG_WARN, "[DependentPlugin] GetData function not available");
    }

    xvalue tblRet = xvoCreateTable();
    xvoTableSetBool(tblRet, "result", 6, TRUE);
    xvoTableSetText(tblRet, "source", 6, "base_plugin", 0, FALSE);
    xvoTableSetInt(tblRet, "data", 4, iData);

    size_t iSize = 0;
    str sJson = ctx->JsonStringify(tblRet, &iSize);
    ctx->SendJson(objResp, 200, sJson, iSize);
    ctx->Free(sJson);
    xvoUnref(tblRet);
}



// ==================== 路由注册 ====================

void DependentPlugin_RegisterRoutes()
{
    ctx->AddRoute("/api/dependent/calculate", PLUGIN_API(Calculate), FALSE, FALSE, 0, 0);
    ctx->AddRoute("/api/dependent/getBaseData", PLUGIN_API(GetBaseData), FALSE, FALSE, 0, 0);
}

void DependentPlugin_UnregisterRoutes()
{
    ctx->RemoveRoute("/api/dependent/calculate");
    ctx->RemoveRoute("/api/dependent/getBaseData");
}



// ==================== 事件处理 ====================

void DependentPlugin_OnSystemReady(xvalue eventData)
{
    ctx->Log(LOG_INFO, "[DependentPlugin] System ready, importing exports...");

    // 获取 base_plugin 的导出函数
    g_fnCalculate = (FnCalculate)ctx->GetPluginExport("base_plugin", "Calculate");
    g_fnGetData = (FnGetData)ctx->GetPluginExport("base_plugin", "GetData");

    if ( g_fnCalculate ) {
        ctx->Log(LOG_INFO, "[DependentPlugin] Successfully imported: Calculate");
    } else {
        ctx->Log(LOG_ERROR, "[DependentPlugin] Failed to import: Calculate");
    }

    if ( g_fnGetData ) {
        ctx->Log(LOG_INFO, "[DependentPlugin] Successfully imported: GetData");
    } else {
        ctx->Log(LOG_ERROR, "[DependentPlugin] Failed to import: GetData");
    }
}



// ==================== 插件入口 ====================

// 插件初始化（启用时调用）
void Plugin_dependent_plugin_Init()
{
    ctx->Log(LOG_INFO, "[DependentPlugin] Initializing...");

    // 验证依赖（依赖管理器会确保 base_plugin 已加载）
    ctx->Log(LOG_INFO, "[DependentPlugin] Dependency 'base_plugin' verified by system");

    DependentPlugin_RegisterRoutes();

    // 监听系统就绪事件
    ctx->OnEvent(EVENT_SYSTEM_READY, DependentPlugin_OnSystemReady);

    ctx->Log(LOG_INFO, "[DependentPlugin] Initialized successfully!");
}



// 插件卸载（禁用时调用）
void Plugin_dependent_plugin_Unit()
{
    ctx->Log(LOG_INFO, "[DependentPlugin] Unloading...");

    // 取消事件监听
    ctx->OffEvent(EVENT_SYSTEM_READY, DependentPlugin_OnSystemReady);

    DependentPlugin_UnregisterRoutes();

    // 清空导出函数指针
    g_fnCalculate = NULL;
    g_fnGetData = NULL;

    ctx->Log(LOG_INFO, "[DependentPlugin] Unloaded successfully!");
}
