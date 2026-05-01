# 插件系统资源目录增强 SPEC

## 1. 目标

本 SPEC 用于跟踪插件系统资源目录增强工作。该增强是后续 xadmin 内置内容生成器输出业务插件前的前置任务。

目标是让插件能够标准使用 xadmin 已有的页面、模板、配置、动态表单和 TCC 编译辅助能力，同时不破坏现有权限系统。

核心结论：

- 插件 `page/` 是受控页面目录，不自动映射 HTTP 路由。
- 插件必须自己注册路由，并在路由中完成鉴权后再加载 `page/` 页面。
- 插件 `static/` 是公开静态资源目录，由宿主统一只读映射。
- 插件 `template/`、`option/` 不公开暴露，只能通过宿主 API 或插件代码使用。
- 插件 `inc/`、`lib/`、`src/` 用于 TCC 编译环境。

## 2. 标准插件目录

目标目录：

```text
hosts/xadmin/plugin/<xid>/
  plugin.json
  main.c
  page/
  template/
  option/
  static/
  inc/
  lib/
  src/
  data/
```

目录语义：

- `page/`：受控 HTML 页面，需要插件注册路由并鉴权后加载。
- `template/`：服务端模板，不直接暴露 URL。
- `option/`：插件选项 schema/defaults，不直接暴露 URL。
- `static/`：公开静态资源，由宿主统一映射到 `/plugin-static/<xid>/...`。
- `inc/`：第三方 `.h`。
- `lib/`：第三方 `.def` 或 TCC library path。
- `src/`：插件自身拆分源码，主要放 `.h`，也可放 include-style `.c`。
- `data/`：插件种子数据或只读资源，可选，不公开。

进度：

- [x] 在插件脚手架中创建 `page/`。
- [x] 在插件脚手架中创建 `template/`。
- [x] 在插件脚手架中创建 `option/`。
- [x] 在插件脚手架中创建 `static/`。
- [x] 在插件脚手架中创建 `inc/`。
- [x] 在插件脚手架中创建 `lib/`。
- [x] 在插件脚手架中创建 `src/`。
- [x] 在插件脚手架中创建可选 `data/`。
- [x] 更新插件包导入/导出逻辑，保留上述目录。

## 3. Manifest 约定

默认资源目录如下，`plugin.json` 不写也按该约定处理：

```json
{
  "resources": {
    "pageDir": "page",
    "templateDir": "template",
    "optionDir": "option",
    "staticDir": "static",
    "includeDirs": ["inc", "src"],
    "libraryDirs": ["lib"]
  }
}
```

进度：

- [x] manifest 解析支持 `resources.pageDir`。
- [x] manifest 解析支持 `resources.templateDir`。
- [x] manifest 解析支持 `resources.optionDir`。
- [x] manifest 解析支持 `resources.staticDir`。
- [x] manifest 解析支持 `resources.includeDirs`。
- [x] manifest 解析支持 `resources.libraryDirs`。
- [x] 缺省时自动使用标准目录。
- [x] 资源目录只能是插件根目录内相对路径。

## 4. TCC 编译路径增强

插件编译时默认加入：

```text
-I plugin/<xid>/
-I plugin/<xid>/inc
-I plugin/<xid>/src
-L plugin/<xid>/
-L plugin/<xid>/lib
```

规则：

- 默认只编译 `build.entry`。
- 不自动编译 `src/*.c`。
- `src/*.c` 可由 `main.c` 通过 `#include "src/foo.c"` 使用。
- 如需多源编译，仍显式配置 `build.sources`。
- 目录不存在时忽略，不报错。

进度：

- [x] 默认加入插件根目录 include path。
- [x] 默认加入 `inc/` include path。
- [x] 默认加入 `src/` include path。
- [x] 默认加入插件根目录 library path。
- [x] 默认加入 `lib/` library path。
- [x] 保留 `build.includeDirs` 追加能力。
- [x] 保留 `build.libraryDirs` 追加能力。
- [x] 文档说明 `src/*.c` 不自动编译。

## 5. Page 机制

`page/` 是受控页面目录，不自动暴露。

插件必须自己注册路由：

```c
XAdmin_RegisterRoute(hPlugin, "/admin/view/plugin/comment", Request_Comment_Admin);
```

路由处理函数中自行鉴权，再加载页面：

```c
void Request_Comment_Admin(..., xvalue objSession)
{
    if (!Comment_CanView(objSession)) {
        xsHttpReplyAuto(objResp, 403, HTTP_CT_HTML, "Forbidden", 0);
        return;
    }
    XAdmin_PluginLoadPage(hPlugin, objResp, 200, "admin.html");
}
```

目标 API：

```c
bool XAdmin_PluginLoadPage(
    XAdminPluginHandle hPlugin,
    XS_ResponseObject objResp,
    int iCode,
    const char* sPage
);

char* XAdmin_PluginReadPage(
    XAdminPluginHandle hPlugin,
    const char* sPage,
    size_t* pSize
);
```

安全要求：

- [x] 禁止 `../`。
- [x] 禁止绝对路径。
- [x] 禁止反斜杠跨平台绕过。
- [x] 真实路径必须位于插件 `page/` 目录内。
- [x] 仅插件代码主动调用，不做自动路由映射。

进度：

- [x] 实现插件 page 路径解析。
- [x] 实现 `XAdmin_PluginReadPage`。
- [x] 实现 `XAdmin_PluginLoadPage`。
- [x] 将 API 导出给插件 TCC。
- [x] 添加示例插件页面加载示例。

## 6. Static 机制

插件公开静态资源放在：

```text
plugin/<xid>/static/
```

HTTP 映射：

```text
/plugin-static/<xid>/<asset_path>
=> hosts/xadmin/plugin/<xid>/static/<asset_path>
```

示例：

```html
<link rel="stylesheet" href="/plugin-static/comment-system/css/admin.css">
<script src="/plugin-static/comment-system/js/admin.js"></script>
```

### 6.1 实现位置

当前 xadmin HTTP 路由表是精确匹配，不适合为每个静态文件创建路由。应在 `RequestProc()` 精确路由查找前加入前缀处理：

```c
const char* sPath = xsReqPath(objReq);

if (PS_TryServePluginStatic(objServer, objHost, objReq, objResp, sPath)) {
    return TRUE;
}

const RouteInfo* pRoute = xrtDictGet(G_StaticRouteTableHTTP, sLookupPath, strlen(sLookupPath));
```

建议新增：

```text
hosts/xadmin/script/plugin_system/ps_static.h
```

目标函数：

```c
bool PS_TryServePluginStatic(
    XS_ServerObject objServer,
    XS_HostObject objHost,
    XS_RequestObject objReq,
    XS_ResponseObject objResp,
    const char* sPath
);
```

### 6.2 处理流程

- [x] 判断前缀是否为 `/plugin-static/`。
- [x] 若不是该前缀，返回 `FALSE`，继续正常路由。
- [x] 解析 `<xid>` 和 `<asset_path>`。
- [x] 只允许 `GET`、`HEAD`。
- [x] 校验插件 xid。
- [x] URL decode 静态资源路径。
- [x] 拒绝空路径。
- [x] 拒绝绝对路径。
- [x] 拒绝 `..`。
- [x] 拒绝反斜杠。
- [x] 确认插件存在。
- [x] 确认插件已启用。
- [x] 拼接 `plugin/<xid>/static/<asset_path>`。
- [x] 规范化真实路径。
- [x] 确认真实路径仍在 static 根目录内。
- [x] 检查文件存在且是普通文件。
- [x] 检查 MIME 白名单。
- [x] 输出文件。

### 6.3 MIME 白名单

第一阶段允许：

```text
.css   text/css; charset=utf-8
.js    application/javascript; charset=utf-8
.mjs   application/javascript; charset=utf-8
.png   image/png
.jpg   image/jpeg
.jpeg  image/jpeg
.gif   image/gif
.svg   image/svg+xml
.webp  image/webp
.ico   image/x-icon
.woff  font/woff
.woff2 font/woff2
.ttf   font/ttf
.eot   application/vnd.ms-fontobject
.map   application/json; charset=utf-8
```

进度：

- [x] 实现 MIME 判断。
- [x] 不在白名单内返回 404。
- [x] 可配置是否允许 `.map`。
- [x] 默认不公开 `.json`。

### 6.4 缓存策略

第一阶段：

```http
Cache-Control: public, max-age=3600
```

后续可加入：

- [x] `Last-Modified`
- [x] `ETag`
- [x] 版本化资源长缓存
- [x] HEAD 请求只返回头
- [x] 最大文件大小限制，建议 16MB

## 7. Template 机制

`template/` 不自动暴露 URL，只允许插件主动渲染。

目标 API：

```c
char* XAdmin_PluginRenderTemplate(
    XAdminPluginHandle hPlugin,
    const char* sTemplate,
    xvalue tblData,
    size_t* pSize,
    char** psError
);
```

查找顺序：

1. 插件 `template/`
2. xadmin 全局 `data/template/`

进度：

- [x] 实现插件模板路径解析。
- [x] 插件模板渲染复用 xadmin template engine。
- [x] 支持插件模板 include。
- [x] include 优先查插件模板目录。
- [x] include 找不到再查全局模板目录。
- [x] 将 API 导出给插件 TCC。

## 8. Option 机制

插件选项定义放：

```text
plugin/<xid>/option/<name>.json
```

用户保存值写入：

```text
hosts/xadmin/data/plugin/<xid>/option/<name>.json
```

加载优先级：

1. 用户保存值。
2. 插件默认值。

目标 API：

```c
xvalue XAdmin_PluginOptionLoad(XAdminPluginHandle hPlugin, const char* sName);
bool XAdmin_PluginOptionSave(XAdminPluginHandle hPlugin, const char* sName, xvalue tblValues);
xvalue XAdmin_PluginOptionSchema(XAdminPluginHandle hPlugin, const char* sName);
```

进度：

- [x] 定义插件 option 文件格式。
- [x] 实现默认 option 读取。
- [x] 实现用户 option 读取。
- [x] 实现用户 option 保存。
- [x] 保存时不改插件目录。
- [x] 将 API 导出给插件 TCC。
- [x] 插件禁用时不暴露 option 设置页。

## 9. 动态表单下发

当前 xadmin 已有动态表单和 xform 前端能力，但插件需要专用 source。

新增建议路由：

```text
/admin/plugin/form?plugin=<xid>&file=<name>.json
/admin/view/plugin/<xid>/option?file=<name>.json
```

注意：该设置页不是自动为所有插件创建路由；插件可以选择注册自己的设置路由，也可以调用宿主统一页面。

进度：

- [x] 支持 `source=plugin-option`。
- [x] 支持按插件 xid 读取 option schema。
- [x] 支持按插件 xid 保存 option values。
- [x] 权限检查使用插件注册的设置路由或宿主统一设置权限。
- [x] 前端 xform 可直接渲染插件 option schema。
- [x] 内容系统生成插件默认可生成设置页入口。

## 10. 下发给插件的宿主能力

优先级：

- [x] 插件 page 加载。
- [x] 插件 template 渲染。
- [x] 插件 option 加载/保存。
- [x] 插件动态表单 source。
- [x] 插件私有数据库路径和打开工具。
- [x] JSON 响应工具。
- [x] 当前 session / 当前管理员信息读取。
- [x] 日志写入。
- [x] 附件上传、选择、访问辅助。
- [x] 权限 URI 注册辅助。
- [x] 事件/Hook/Service 机制继续保持。
- [x] 插件资源路径解析工具。

原则：

- 不做大而全 function table。
- 只把宿主管理型资源做成稳定导出 API。
- 普通 C 函数仍走 native direct call。

## 11. 对内容系统生成插件的要求

内容系统后续生成业务插件时，默认输出：

```text
cms.article/
  plugin.json
  main.c
  page/
    admin.html
    edit.html
    public.html
  static/
    css/admin.css
    js/admin.js
  template/
    list_item.html
    detail.html
  option/
    runtime.json
  src/
    model.h
    render.h
    db.h
  inc/
  lib/
```

生成插件必须：

- [x] 自己注册后台页面路由。
- [x] 自己注册前台页面路由。
- [x] 路由中完成鉴权后加载 `page/`。
- [x] 静态资源引用 `/plugin-static/<xid>/...`。
- [x] 不把插件静态资源写入 xadmin `wwwroot`。
- [x] 使用 `option/` 定义设置项。
- [x] 使用 `template/` 存放可复用模板。
- [x] 使用 `src/` 拆分生成代码。

## 12. 实施阶段

### Phase 1：目录和编译路径

- [x] 插件脚手架生成标准目录。
- [x] TCC 默认加入 `inc/`。
- [x] TCC 默认加入 `src/`。
- [x] TCC 默认加入 `lib/`。
- [x] Manifest 支持 resources 缺省值。

完成标准：

- [x] 插件无需手写 includeDirs 即可 `#include "model.h"` 或 `#include "src/model.h"`。

### Phase 2：插件 page API

- [x] 实现 page 安全路径解析。
- [x] 实现 page 读取。
- [x] 实现 page HTTP 输出。
- [x] 导出 API。

完成标准：

- [x] 插件注册路由后能加载自己的 `page/admin.html`。
- [x] 未注册路由时 `page/` 文件不能被访问。

### Phase 3：插件 static 映射

- [x] 新增 `ps_resource.h`，统一承载 page/static/option 资源能力。
- [x] HTTP 入口接入 `/plugin-static/` 前缀处理。
- [x] 实现插件 xid 校验。
- [x] 实现路径安全校验。
- [x] 实现启用状态检查。
- [x] 实现 MIME 白名单。
- [x] 实现缓存头。

完成标准：

- [x] `/plugin-static/<xid>/...` 可访问启用插件的静态资源。（已用 `cms.article` 生成的 `/plugin-static/cms.article/content/1.html` 验证。）
- [x] 禁用插件返回 404。
- [x] 访问 `page/`、`option/`、`template/` 返回 404。
- [x] 路径穿越返回 404。

### Phase 4：插件 template API

- [x] 实现插件模板渲染。
- [x] 实现 include 查找顺序。
- [x] 导出 API。

完成标准：

- [x] 插件可渲染自己的 `template/*.html`。
- [x] 插件模板可复用全局模板。

### Phase 5：插件 option + form

- [x] 实现插件 option load。
- [x] 实现插件 option save。
- [x] 实现 plugin option form source。
- [x] 实现统一设置页或示例设置页。

完成标准：

- [x] 插件配置可由动态表单渲染。
- [x] 用户保存值写入 `data/plugin/<xid>/option/`。

### Phase 6：文档和生成器接入

- [x] 更新插件系统文档。
- [x] 更新内容系统内置化 SPEC。
- [x] 更新生成插件模板。
- [x] 新生成业务插件使用标准目录。

完成标准：

- [x] 内容系统生成插件不再把静态资源写入 xadmin `wwwroot`。
- [x] 新生成插件使用 page/template/option/static/src 标准结构。

## 13. 验收标准

- [x] 插件具备标准资源目录。
- [x] `page/` 不自动映射 HTTP 路由。
- [x] `static/` 通过 `/plugin-static/<xid>/...` 映射。
- [x] 静态映射不进入权限 URI 表。
- [x] 静态映射不能访问 `page/`、`template/`、`option/`、`src/`、`inc/`、`lib/`。
- [x] 禁用插件后静态资源不可访问。
- [x] 插件可通过 API 加载 page。
- [x] 插件可通过 API 渲染 template。
- [x] 插件可通过 API 读取和保存 option。
- [x] 插件 option 可接入动态表单。
- [x] TCC 默认支持 `inc/`、`src/`、`lib/`。
- [x] 内容系统生成插件使用增强后的插件系统。
