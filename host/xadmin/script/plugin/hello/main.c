
// Hello World 示例插件
// 用于演示和测试插件系统功能

#include "plugin.h"

// 全局上下文
PluginContext* ctx;

// 插件设置
xvalue g_tblSettings = NULL;

// 菜单ID
int g_iMenuId = 0;



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

// API: 获取问候语
void API_Hello_Greeting(void* objServer, void* objHost, struct mg_connection* c, struct mg_http_message* hm)
{
    // 获取设置
    str sMessage = "Hello World!";
    bool bShowTime = FALSE;
    
    if ( g_tblSettings ) {
        str sCustomMsg = xvoTableGetText(g_tblSettings, "welcomeMessage", 14);
        if ( sCustomMsg && strlen(sCustomMsg) > 0 ) {
            sMessage = sCustomMsg;
        }
        bShowTime = xvoTableGetBool(g_tblSettings, "showTime", 8);
    }
    
    // 构建响应
    xvalue tblRet = xvoCreateTable();
    xvoTableSetBool(tblRet, "result", 6, TRUE);
    xvoTableSetText(tblRet, "message", 7, sMessage, 0, FALSE);
    
    if ( bShowTime ) {
        int64 iNow = ctx->TimeNow();
        xvoTableSetInt(tblRet, "time", 4, iNow);
    }
    
    size_t iSize = 0;
    str sJson = ctx->JsonStringify(tblRet, &iSize);
    ctx->SendJson(c, 200, sJson, iSize);
    ctx->Free(sJson);
    xvoUnref(tblRet);
}



// API: 插件信息
void API_Hello_Info(void* objServer, void* objHost, struct mg_connection* c, struct mg_http_message* hm)
{
    ctx->SendJson(c, 200, "{\"result\":true,\"name\":\"hello\",\"title\":\"Hello World 示例插件\",\"version\":\"1.0.0\"}", 0);
}



// 视图: 插件页面
void View_Hello_Page(void* objServer, void* objHost, struct mg_connection* c, struct mg_http_message* hm)
{
    str sHtml = "<div style='padding:30px;text-align:center;'>"
                "<h1 style='color:#1e9fff;'>Hello World Plugin</h1>"
                "<p>这是一个用于测试插件系统的示例插件。</p>"
                "<p>插件版本: <code>1.0.0</code></p>"
                "<button class='layui-btn' onclick='testApi()'>测试 API</button>"
                "<pre id='result' style='margin-top:20px;text-align:left;background:#f8f8f8;padding:15px;border-radius:4px;'></pre>"
                "</div>"
                "<script>"
                "function testApi() {"
                "  fetch('/api/plugin/hello/greeting')"
                "  .then(r=>r.json())"
                "  .then(data=>{"
                "    document.getElementById('result').innerText = JSON.stringify(data, null, 2);"
                "  });"
                "}"
                "</script>";
    ctx->SendHtml(c, 200, sHtml);
}



// ==================== 路由注册 ====================

void Hello_RegisterRoutes()
{
    // 前台 API
    ctx->AddRoute("/api/plugin/hello/greeting", API_Hello_Greeting, FALSE, FALSE, 0, 0);
    ctx->AddRoute("/api/plugin/hello/info", API_Hello_Info, FALSE, FALSE, 0, 0);
    
    // 后台视图
    ctx->AddRoute("/admin/view/plugin/hello", View_Hello_Page, TRUE, TRUE, 0, 0);
}

void Hello_UnregisterRoutes()
{
    ctx->RemoveRoute("/api/plugin/hello/greeting");
    ctx->RemoveRoute("/api/plugin/hello/info");
    ctx->RemoveRoute("/admin/view/plugin/hello");
}



// ==================== 菜单注册 ====================

void Hello_RegisterMenus()
{
    // 在插件管理菜单下添加 Hello 子菜单（可选）
    // 这里演示如何动态添加菜单
    // g_iMenuId = ctx->AddMenu(0, "Hello插件", "layui-icon layui-icon-face-smile", 1, "_component", "/admin/view/plugin/hello", 999000, TRUE);
}

void Hello_UnregisterMenus()
{
    if ( g_iMenuId > 0 ) {
        ctx->RemoveMenu(g_iMenuId);
        g_iMenuId = 0;
    }
}



// ==================== 事件处理 ====================

void Hello_OnSystemReady(xvalue eventData)
{
    ctx->Log(1, "[Hello Plugin] System ready event received!");
}



// ==================== 插件入口 ====================

// 插件初始化（启用时调用）
void Plugin_hello_Init()
{
    ctx->Log(1, "[Hello Plugin] Initializing...");
    
    Hello_RegisterRoutes();
    Hello_RegisterMenus();
    
    // 监听系统就绪事件
    ctx->OnEvent(EVENT_SYSTEM_READY, Hello_OnSystemReady);
    
    ctx->Log(1, "[Hello Plugin] Initialized successfully!");
}



// 插件卸载（禁用时调用）
void Plugin_hello_Unit()
{
    ctx->Log(1, "[Hello Plugin] Unloading...");
    
    // 取消事件监听
    ctx->OffEvent(EVENT_SYSTEM_READY, Hello_OnSystemReady);
    
    Hello_UnregisterRoutes();
    Hello_UnregisterMenus();
    
    ctx->Log(1, "[Hello Plugin] Unloaded successfully!");
}
