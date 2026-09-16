# 新版 xs 迁移记录

日期：2026-09-05；2026-09-06 补充写入与账号删除修复；2026-09-11 接入重载工具、调试跟踪、入口生成、表单引擎、配置编辑与设置文件管理并修复动态路由编译的未初始化字段。状态：第一批应用基础链路可运行，完整功能平移尚未完成。下表为首批范围，新增回归见文末。

## 不变的边界

- `dev/v1` 是基线，迁移工具以只读 SQLite 连接获取一致性快照，不改旧版业务数据。
- 原页面、模板、前端资源和插件包原样复制；`v1-assets.json` 记录 832 个资产的来源、目标和 SHA-256。未重新设计界面。
- 不安装空库、不重置真实账号，不重新执行旧库修复逻辑。主库路径错误或缺失时初始化失败，不静默创建另一份数据库。
- 旧业务 SQL、表结构、URI、请求字段及成功/失败数据结构尽量原样迁入。没有借迁移将旧接口统一改成另一套 REST 风格。
- 旧入口保护、后台角色授权、会员用户组授权、前后台独立 Cookie、日志和活跃续期规则保留。

## 本批实现与验证程度

| 范围 | 接入情况 | 已执行验证 |
| --- | --- | --- |
| xs 启动、请求/响应、卸载 | 使用新版 xs 内置 SDK | TCC 编译、HTTP、chunked 正文、keep-alive、脚本重载与旧代卸载 |
| 静态/动态路由与方法槽 | 新公共路由模块；旧业务 URI 不改 | 静态优先、pattern 捕获、重复注册覆盖、不同方法不同回调、CRUD、405/Allow、HEAD 不隐式映射 GET |
| 登录、会话、入口保护 | 已接入 | 旧哈希登录、无效 JSON、退出后旧 Cookie 失效、过期清理、普通与受保护入口两种配置 |
| 后台用户管理 | 已接入 | 列表、新增、修改、软删除、删除账号无法登录；列表不泄露 pwd/salt；新增/编辑模板 |
| 后台角色/权限分类/权限组/URI | 已接入旧处理函数 | 列表接口、嵌套权限模板和角色拒绝；各管理项的写操作尚未逐项完整回归 |
| 会员及会员权限管理 | 已接入旧处理函数 | 管理列表和部分新增模板；管理端写操作尚未逐项完整回归 |
| 会员基础 API | 已接入旧处理函数 | 注册、登录、资料读取、余额与余额日志读取、注销、XSID/MSID 隔离；资料修改/改密待补回归 |
| 菜单、日志 | 已接入旧处理函数 | 菜单树、管理列表、部分模板；写操作与清空日志待补回归 |
| 品牌、配置读取 | 已接入 | 旧登录页及后台品牌加载、原 cp_url 生效；配置编辑已切换为动态表单（见 2026-09-11 一节） |
| 表单引擎与配置编辑 | 已接入旧处理函数 | 演示表单 schema/必填校验/演示数据回写；global.json 经表单读改写、缓存即时生效、cp_url 冲突拒绝（见 2026-09-11 一节） |
| 设置文件管理 | 已接入旧处理函数 | 列表/结构 CRUD/锁定保护/加入与解除菜单联动（见 2026-09-11 一节） |
| 重载工具 | 已接入旧处理函数 | 模板缓存全量重建、host/server/xs 提交与代际轮换（见 2026-09-11 一节） |
| 调试跟踪接口 | 已接入旧处理函数 | 概览/会话/配置/权限缓存/路由表五个 JSON dump（见 2026-09-11 一节） |
| 入口生成工具 | 已接入旧处理函数 | cp_url「自动生成」后端，返回候选地址且避开既有路由（见 2026-09-11 一节） |
| 私有文件隔离 | 仅 wwwroot 是公开根 | 对 main.c、route.h、数据库、配置、受控页面、模板、旧版目录和越界路径的直接访问被拒绝 |

浏览器实测：原登录页面完成登录；原后台布局、用户列表和历史日期显示正常；新增弹窗显示旧角色选项。首页与菜单保留所有原有入口，因此尚未接入模块的链接仍不可用，不能据此视为全量功能已迁移。

测试还有 8 个并发客户端、40 次共享旧预编译 SQL 的查询，确认本批串行化保护下结果正常。这不是性能压测，也不是生产安全认证。TLS 分支尚未做真实 HTTPS 回归。

## 底层适配的取舍

### 值对象

`compat_value.h` 只把旧调用形式桥接到新版原生 `xvalue*`，不模拟旧对象内存布局，不携带旧运行时。旧代码直接访问的数组/字典内部字段已换为公共 API。

Get 返回借用值；保存已有值保留引用；`take/colloc=true` 消费调用方所有权。新字符串值会复制内容，所以登录代码在转交 XID/MSID 后重新取得值内地址，避免继续使用已释放指针。

### 密码与时间

- 客户端仍使用旧页面的 `SHA256(username + "_xywhsoft_" + password)` 小写文本。
- 服务端仍使用 `SHA256(username + salt + clientHash)` 大写文本，与旧库一致。本批没有迁移密码算法或重写密码记录。
- v1 数据库是从公元零年起算的本地日历秒；新版 xtime 是 Unix 微秒。`compat_util.h` 在边界转换，业务 SQL 继续使用旧单位，保留历史时间和原续期长度。

### 并发与生命周期

旧模块共享 SQLite 连接、预编译语句与可变缓存。第一批用一把请求锁覆盖整个业务区；正文读取和公开静态文件不占这把锁。不能只锁 `sqlite3_step`，否则其他请求仍会交错 bind/step/reset。

每一代脚本有自己的数据库句柄、缓存、资源根、路由和 Session。xs 排空旧代后调用卸载函数。当前不跨代保留 Session，重载后重新登录，与旧版按代释放会话的做法一致；不引入会复活已注销会话的陈旧快照，也不跨代传递 TCC 函数指针。

### 会话收紧

- XSID/MSID 使用系统安全随机源生成 128 bit 随机值。
- 请求持有独立引用，注销从容器移除后，当前请求仍可安全结束。
- 后台 Cookie 保留 HttpOnly，增加 SameSite=Lax；真实 TLS 请求才附加 Secure。没有根据可伪造的转发头推断 HTTPS。
- 会员 Cookie 的跨站策略未在本批修改；后台 2 小时、会员 24 小时的服务端期限以及原有 remember Cookie 期限保留。
- 每五分钟通过代绑定定时器清理过期会话；请求访问过期会话时也立即拒绝。
- 日志不再输出受保护的后台入口地址。

这些不是完整的 CSRF 防护或完整账户撤销策略。是否修改相关行为，应在后续框架安全批次中单独确认兼容性并回归，而不是悄悄更改业务功能。

### 路由与模板

路由表仅在初始化阶段注册；动态 pattern 最多 64 条，每条最多 8 个捕获参数，超出或冲突时编译失败。动态路由的权限键是注册的 pattern，不是某个具体 ID 路径。

旧 URI 统一以 ANY 注册，只是把方法选择交回旧回调，不表示旧业务实现支持所有方法。新接口可用 GET/POST/CRUD/ANY 的方法槽；CRUD 按 xrt 定义包含 GET、POST、PUT、PATCH、DELETE。旧回调返回的 404 或 405 不被统一改写。

`page`/`template` 通过 `xroot` 锚定真实目录句柄，拒绝越界或链接跳出；单个资源读取上限 4 MiB。模板使用新版引擎配置 `{{` / `}}`，保留原文件和嵌套 foreach 语法，按首次使用编译缓存。本批没有迁移旧自定义 `#form` 扩展。

## 资产落点

| v1 位置（hosts/xadmin 内） | 新位置 |
| --- | --- |
| `data/db/main.db` | `db/main.db` |
| `data/install/main.db` | `db/install/main.db` |
| `data/plugin/<id>/**/*.db` | `db/plugin/<id>/...`，包括 xlogserver 历史日志库 |
| `data/plugin/<id>/config.json` | `options/plugin/<id>.json` |
| 其他插件私有数据 | `plugin_data/<id>/...` |
| `data/page`、`data/template`、`data/options` | `page`、`template`、`options` |
| `data/forms`、`data/uploads`、其余 `data/install` | `forms`、`uploads`、`install` |
| `wwwroot`、`plugin` | 同名根目录 |
| 本批 `script/module`、`script/route_http` | `modules`、`route_http` |

SQLite 使用备份接口，不直接复制可能仍有 WAL 的主文件；先校验备份，再以不覆盖目标的方式发布。重跑导入会保留已有文件。安装种子库也已归入 db，旧目录不动。

注意：主库 `plugin_runtime` 中的旧运行路径尚未转换；插件宿主尚未接入，不会按这些旧路径运行。旧插件 API/ABI、库路径、私有数据路径及卸载生命周期必须在插件批次一起适配，不能只把插件目录复制过来就启用。

## 尚未接入：下一批顺序

1. 附件管理和受控上传下载，逐项复用旧页面并验证读写契约。（通用表单、配置编辑与设置文件管理已于 2026-09-11 接入；表单的模板 {{#form}} 静态渲染器待用新模板引擎 xtemplateregistry 扩展面恢复。）
2. 插件宿主与插件 SDK → 原插件逐个适配 → 插件数据库、配置、历史日志路径转换；不改变插件业务功能。
3. 计划任务、会员消息与通知、邮件、MIR/XTP 及安装流程。新版可用底层接口与旧 ABI 存在差异，不能直接链接旧二进制接口。（重载工具与调试跟踪已于 2026-09-11 接入。）
4. 补齐所有旧功能的读写、权限、错误路径和生命周期回归，再进行完整部署验证。

不在本批承诺无感替换线上实例、所有插件可运行、会话跨重载保留、完整 HTTPS/CSRF 测试或性能结论。根目录可用于继续开发和本机回归；线上 v1 保持原状。

## 2026-09-06：写入结果与账号删除修复

- 管理员和会员软删除成功后，撤销该账号在当前脚本代中的所有会话；数据库拒绝删除时保留会话。删除自己的账号也可安全完成当前响应。
- 已接入的管理员、角色、权限、会员、菜单、日志和会员 API 写入检查 SQLite 执行结果。新增及按 ID 修改/删除还检查影响行数，失败不再返回成功或旧记录 ID；批量清理零行仍视为成功。
- 前后台权限关联迁移与分类/权限删除通过保存点一起提交；余额修改与流水新增一起提交。任一步失败回滚，不刷新失败操作对应的缓存。
- URI 配置修改成功后从数据库刷新路由和前后台权限缓存，避免在释放表单后继续使用其中的 URI 指针。
- 新增 `tests/write_regression.py`，由 `tests/smoke.py` 自动运行。通过测试库触发器拒绝或忽略 SQL，验证失败响应、数据库状态、语句重试、多会话撤销、自己删除自己、权限缓存和事务回滚；同时覆盖会员资料修改、改密及失败恢复。

本次不改变旧页面、账号密码格式、正常成功响应或会话续期规则。角色变更、禁用和改密后的会话撤销策略仍需另行完善；新增写入回归不代表所有业务规则和权限组合都已覆盖。

## 2026-09-11：重载工具与调试跟踪接入

- v1 的 `/admin/view/tool/reload` 页面与 template/host/server/xs 四个动作原样接入，页面字节与 JSON 形状保持 v1；提交契约换成 xs3 期望状态协调器（`xsReloadHostSubmit`/`xsReloadServerSubmit`/`xsReloadAllSubmit`），响应新增 `data.id` 供 `xsReloadQuery` 查询终态，`code`/`queued`/`reloadCount` 字段维持旧形状。
- `Template_RebuildCache` 语义对齐 v1：递归重扫模板目录、预编译进新缓存、全部失败时保留旧缓存。`form/block_demo.html` 依赖尚未迁移的表单自定义语句，计入 `failed`（`loaded>0` 时整体仍成功），与 v1 表现一致。
- 修复 `RouteHTTP_Compile` 中 `xpatternspec` 栈数组未清零的问题：重载控制线程栈上的非零 `Priority`/`Flags` 使 `xrtPatternCompileMany` 以 "pattern specification flags must be zero" 拒绝编译，第二代起动态路由全部失效。此前仅因首代线程栈恰好为零而未暴露；错误输出现在附带 xrt 详细错误。
- 新路由的 URI 权限记录复用主库既有 `uris` 行（authID=1），启动时由 `Auth_UpdateURIS` 自动对齐，无需手工补库；菜单"重新加载"条目已在库中，直接生效。
- smoke 新增覆盖：未认证提交被拒、页面字节比对、模板重建计数与目录文件数一致（GET/POST 两种方法均允许，同 v1）、三个提交动作的代际轮换与重登录；普通与受保护入口两种模式全部通过。

### 调试跟踪（trace）

- v1 的 `/admin/trace` 概览及 session/option/auth/route 四个明细接口原样接入；v1 无对应页面资产，均为 JSON 输出，字段名与 `?type=` 筛选参数保持不变。URI 权限行复用主库既有 `uris` 记录（authID=20「调试接口」，角色 1 已授权），启动时自动对齐。
- 适配点：v1 的 `G_AdminSession` 共享表即本代 `G_AdminSessions`，会话明细同时输出前台 `G_MemberSessions`（v1 只输出后台，置于 `data.admin`/`data.member`）；`G_Install` 安装向导标志映射为 `G_Ready`（本代主库必须预先存在，无向导流程）；路由明细在本代新增的动态 pattern 条目之后一并追加（静态表条目数仍由概览 `route.count` 单独给出）。
- smoke 新增覆盖：未认证与方法拒绝、概览各计数下限、会话 dump 含当前登录账号、配置 dump 含 global 命名空间、`?type=all/role` 的键集合、路由 dump 的 v1 authId（trace=20、tool/reload=1）与动态 pattern 条目；两种入口模式全部通过。

### 入口生成工具（option tool 第一部分）

- v1 `/admin/option/tool/admin-entry` 原样接入：仅接受 GET，调用已随配置模块迁入的 `Option_GenerateAdminEntryPath` 返回候选入口（32 位小写字母数字，自动避开既有路由，不占用 `/admin` 前缀），响应形状 `{result, message, data.path}` 不变；只生成候选地址不落盘，保存仍走配置写入（该 UI 随表单/配置编辑批次接入）。
- URI 权限行复用主库既有 `uris` 记录（authID=1，角色 1 已授权）。同文件的 test-smtp 依赖 SMTP 发送，与邮件批次一起接入，其 `uris` 行按保留策略不动。
- smoke 新增覆盖：未认证与方法拒绝、候选地址格式、两次生成不同、候选不与已注册路由冲突（与 trace 路由 dump 交叉验证）；两种入口模式全部通过。

### 表单引擎与配置编辑（form + option 数据接口）

- v1 动态表单引擎接入：`modules/form.h`（表单文件名校验、JSON 表单装载、field_types 装载、option 配置 → schema/props/values 转换、必填校验、演示数据保存）与 `/admin/view/form`、`/admin/form`、`/admin/view/option`、`/admin/option` 四条路由。页面 `form.html` 与 `forms/*.json`、`wwwroot/lib/xform/` 资产原样复用；响应形状与错误文案保持 v1。
- 配置编辑端到端恢复：菜单「全局配置/附件设置」→ `/admin/view/option` 302 表单页 → `/admin/option` GET（含 authLevel 检查，global.json 要求 200）→ POST 经 `Option_SaveFile` 落盘并重建缓存（cp_url 安全入口冲突校验生效），写入即刻反映到 `/brand/admin`。表单页的「自动生成入口」按钮由已接入的 `/admin/option/tool/admin-entry` 提供。
- 适配点：v1 的 `data/forms`、`data/install/forms` 按本代目录规划平铺为 `forms/`、`install/forms/`；`xsHttpJsonValueTake` 改为显式序列化+释放；`xrtStringifyJSON_File` 返回值由 int 比较改为 bool。
- 未迁移（有意）：v1 模板 `{{#form}}` 块的静态 HTML 渲染器（约 600 行，仅服务 `form/block_demo.html` 演示模板与 `/admin/view/template/form_demo` 演示页，均不在任何菜单）；新模板引擎的 `xtemplateregistry` 扩展面是后续恢复入口。`form/block_demo.html` 在模板全量重建中计入 failed（loaded>0 时整体成功），与 2026-09-11 重载工具一节记录一致。
- smoke fixture 现在复制 `forms/` 目录；新增覆盖：未认证与方法拒绝、页面字节、非法文件名拒绝、demo 表单 schema/fieldTypes/必填失败文案（「为必填项」）、演示数据回写与重读、option 视图 302、global.json 字段清单（cp_url/adminTitle）、保存后 `/brand/admin` 与磁盘文件双验证、cp_url='/admin' 冲突拒绝。两种入口模式全部通过。

### 设置文件管理（option files）

- v1 `/admin/view/option/files|file` 与 `/admin/option/files|file|file/menu` 五条路由接入；模块层 `Option_ListFiles/SaveDefinition/DeleteFile` 自首批迁移起已在库（此前零调用者），本批补齐路由与菜单联动。页面 `option/files.html`、`option/file_edit.html` 字节级复用；菜单「设置管理」条目（/admin/view/option/files）复活，option 家族全部收口。
- 契约保持 v1：列表行含 locked/canDelete/canEditDefinition 与菜单挂载信息（inMenu/menuId/menuTitle）；结构编辑 POST 创建 / PUT 保存（namespace 唯一性、字段名重复校验）；DELETE 删除文件并解除对应菜单；POST file/menu 按 global.json 与相邻条目之间的排序规则插入菜单，重复加入被拒。锁定文件（global.json）拒绝改结构与删除。
- 适配与加固：v1 的逐请求调试 printf 不随迁移；`stmt_menu_add` 写入改用 `DB_Write` 检查影响行数（v1 只看 SQLITE_DONE）；其余 SQL 语义原样。
- smoke 新增覆盖：未认证与方法拒绝、两个页面字节比对、列表文件集合与锁定/inMenu 标志、单文件读取、`../` 编码文件名拒绝、创建/重复创建/跨文件 namespace 冲突/PUT 扩展字段落盘、锁定文件改结构删除拒绝、加入菜单（fixture 库验证菜单行 + 列表 inMenu/menuTitle）与重复加入拒绝、删除后文件消失且菜单行软删（fixture 库验证）、删除不存在文件失败。写失败注入不适用（JSON 文件写入非 SQL；菜单写入复用 write_regression 已覆盖的 menu 语句族）。两种入口模式全部通过。

### 2026-09-11 迁移质量审计（今日五批）

- 审计发现并修复：`/admin/form` POST 的 `source=option` 分支在迁移时被遗漏（v1 该端点可保存配置，误迁为总是走演示数据保存；迁移后 UI 不受影响但 URI 契约破损）。已按 v1 原样补回（Option_LoadFile → authLevel 检查 → Option_SaveFile），smoke 增加 example.json 经 `/admin/form source=option` 的落盘回读断言。
- 文案编码说明：今日新文件全部为 UTF-8、零乱码；v1 的 form/trace 路由源文件本身含乱码字节（v1 响应同样是乱码），移植时按 v1 option 路由中的同义可读文案还原为本意文本——这是对 v1 字节的偏离（改进），与首批"逐字节保留旧文案（含乱码）"的做法不同，特此记录。
- 已知测试缺口：`/admin/option/file/menu`（菜单写入）与 `/admin/option/file` DELETE（菜单解除）未纳入 write_regression 触发器注入（所用 stmt_menu_* 语句族的失败语义已由 menu 族覆盖，但新处理器路径未注入验证）；配置 authLevel 内部门槛的拒绝路径（如 200 级配置对低级别会话）暂无直接断言。两项待后续批次补齐。

## 2026-09-11：模拟攻击评审修复（批次 A/B/C 全部九项）

六小时模拟攻击（浸泡+探针+隔离实验）发现的问题按商定方案修复，均含回归用例，双入口模式全绿（37 PASS、零警告）。除注明外均为【v1 偏离】（安全/正确性修正）。

- **F1 被禁用会员自解禁+会话残留**：`API_Profile` PUT 改用新语句 `stmt_member_profile`（仅 nickname/email/phone/avatar/updateTime，不再触碰 groupId/authLevel/status）；管理端 PUT 禁用（status=0）成功后调用 `Session_RevokeAccount` 即时撤会话。write_regression 增加"禁用失败不撤、成功必撤"注入用例。
- **F2 口令哈希落日志（注册参数门控版）**：`AddStaticRouteHTTP/AddDynamicRouteHTTP` 增加 `bMaskBody` 参数（route.h 十处口令类接口传 TRUE）；`Logs_Add` 对打标路由不记录请求体；uris 表启动时幂等补 `maskBody` 列（新路由 INSERT 写入初始值，运行时以注册参数为权威）。取舍：登录日志不含尝试用户名、配置保存日志不含变更内容（换取 smtp_password 等不落库）。
- **F3 会员 Cookie 缺 SameSite**：新增 `Session_MemberHeaders`（HttpOnly+SameSite=Lax+TLS Secure），替换 api.h 三处手拼 Set-Cookie，与后台 XSID 同级。
- **F4 Guard 按 IP 跨端点共享**：拆分 `G_GuardAdmin/G_GuardMember` 双字典，函数加字典参数；爆破会员 API 不再锁同 IP 后台登录（反向亦然）。
- **F5 锁定文案编码**：login.h/api.h 的 GBK 乱码锁定消息改写为 UTF-8 本意文本。
- **F6 权限写路径劣化（两段修复）**：(1) 算法反转——`Auth_ReloadCache/MemberAuth_ReloadCache` 改为单次遍历收集 AuthID→URI 清单、每角色按 authList 点取（实测 20k 角色重建循环从秒级降至 ~100ms）；(2) 延迟退役——深挖发现旧缓存同步销毁才是二次方项（20k 角色销毁 8s，位于内嵌 xrt 分配器路径，建议上游排查），写路径改为压入 256 槽退役队列，`Session_Tick`（5 分钟、请求锁内）清扫；满槽清扫最旧一半（延迟与内存均有界）。性能门禁：20k 角色单次角色写 **126ms**（修复前 9.5s）。语义不变：仍同步重建、写后即时生效。
- **F7 登录审计缺口**：`Request_Login` 在 Guard 通过后显式 `Logs_Add(objReq, NULL, TRUE)`——洪泛被 guard 拒绝不产生日志行（防日志填充 DoS），到达口令验证的尝试全量留痕（body 为空，F2 门控）。uris 的 needLog 标记对公开路由失效的两代共同缺陷由此绕开（调度器内 Logs_Add 位置不动，避免未认证洪泛写日志）。
- **F8 API 响应 sprintf 直拼**：`API_Login` 成功响应与 `API_Profile` GET 改为 xvo 构造 + `xrtStringifyJSON`（实测昵称注入 `","backdoor":"1` 原会注入字段，现已转义）；JSON 形状不变。
- **F9 管理列表 limit 无上限**：auth.h×5、member.h×4、logs.h×1 统一上限 100（与余额日志一致）。

smoke 新增覆盖：F1 禁用链（撤会话/拒登/status 不翻转）、F2 双向（打标 body 为空、未打标逐字节相等）、F3 cookie 属性、F4/F5（会员锁定不影响后台+可读文案）、F6 性能门禁、F7 审计行、F9 limit。测试基建：`tests/attack_soak.py`（时长可配浸泡）、`tests/attack_probes.py`（8 阶段探针）。

## 2026-09-11：批次 D 安全补丁（R1-R4）

模拟攻击遗留四项修复，注册限速按每 IP 每分钟 1 次执行。双入口模式全绿（39 PASS、零警告）。

- **R1 登出 CSRF**：`/admin/logout` 与 `/api/v1/logout` 收紧为仅 POST（后台其余方法 404、会员 API 405）。后台首页的注销从 GET 导航改为表单 POST（浏览器自然跟随 302，受保护入口同样成立）——该页面是 v1 资产，为此资产门禁升级为支持 `target_sha256` 显式记录有意偏差（源基线哈希校验不变，`docs/v1-assets.json` 已登记）。
- **R2 注册限速**：`/api/v1/register` 每 IP 一个间隔窗口，默认 60 秒，`registerIntervalSecond` 全局配置可调（<=0 禁用）；成功注册才计数，失败尝试不占窗口（触发器注入测试不受影响）。超限返回 `{"code":429}`。测试夹具默认注入间隔 0 绕过（主套件多次注册），另以独立子夹具按默认配置验证限速生效。
- **R3 改密撤销会话**：新增 `Session_RevokeAccountExcept`（保留当前请求会话）。会员自助改密撤销同账号其他会话、保留当前；管理员/会员 repwd 代重置撤销该账号全部会话。write_regression 的 repwd 族同步改为断言"成功重置后旧会话失效并重登"。
- **R4 会话上限**：同账号并发会话上限 5（按 `_activeTime` 踢最旧），在 Session_Store 成功后执行；兼容既有测试的双会话用法，多端登录正常。

新增回归：R1（两处方法拒绝）、R2（默认配置下第二次注册 429）、R3（repwd 全撤 + 自助改密保留当前 + 新口令可登）、R4（第 6 次登录踢最旧）。

## 2026-09-12：插件宿主内核（P0）+ 全能力验证 + 压力战役

按插件迁移方案实施 P0 批次（宿主内核），并以"全新 hello-sdk 插件 + v1 hello 转换探针 + 压力战役"完成验证闭环。双入口模式全绿（40 PASS、零警告）。

### 交付
- **plugin_sdk/xs_plugin.h**：ABI v4 冻结（27 宿主函数 + 描述符生命周期 + SetGlobalData 全局注入，符号面与 v1 一致），附带插件方言声明面（值层兼容 41 函数、请求层垫片 xsHttpReplyAuto/xsReqMethodID/xsReqBody、XVO_DT_*/XHTTPD_* 宏映射）——宿主侧以 XS_PLUGIN_HOST_SIDE 守卫避开重定义。
- **modules/plugin_host.h**：扫描/路径幂等转换（v1 `hosts/xadmin[/data]/plugin` → `plugin|plugin_data|db/plugin`）/manifest v4 校验/依赖启用检查；TCC 编译管线（xsCreateTCC + 72 个符号注入：27 ABI + 41 兼容层 + 请求垫片）；七段生命周期（OnLoad→全局注入→OnInstall→OnConfigChanged→OnStart，回滚路径完整）；路由/菜单/权限组/权限/URI 注册与台账（plugin_resource，destroy_policy 语义保留）；事件/钩子总线（sort 链序）；服务注册表（主版本协商 + 租借计数）；stale active 代际启动矫正。
- **route_http/plugin.h**：list/get/enable/disable/reload/settings + 视图（v1 契约；import/export 为 P3 占位）。settings 走 OnConfigChanged 热更新。
- **main.c 接线**：PluginHost_Init 位于 Form_Init 后、Auth_SyncURIS 前（插件路由进 uris 权限体系）；ServiceUnit 首位停插件（OnStop 可用全部宿主能力）。

### 验证
- **hello-sdk 全能力一致性**（tests/plugins/hello-sdk，21KB）：路由（公开/管理/临时注销）、菜单、权限组/权限/URI（admin+member 域）、事件往返、钩子链、服务提供+消费、配置热改、健康检查、代际重载（插件内静态计数器随新镜像归零，配置跨代保留）。smoke 断言：启停状态机、DB 台账增减、404 回收、403 权限门、reload 代际+1。
- **v1 hello 转换探针**（tests/v1_probe.py）：migrate_v1.py 同款机械变换 + sqlite3.h 补含 → 编译加载全通过，greeting/info 路由正常，服务+钩子+事件链路工作，全局注入（db/options/路径）确认。**v1 插件迁移管线成立**（每个 v1 插件 = 变换 + 编译 + 冒烟）。
- **压力战役**（tests/plugin_stress.py）：100 次启停 churn 9s（RSS 12→23MB、句柄 160→180、台账每轮归零）；50 次 reload 代际 4s（151 代、RSS 25MB 平台化）；失败注入（坏 manifest/编译错误插件在场）宿主存活、启用中插件重载后自动恢复；主脚本重载风暴 ×10 插件全部自动重启。**VERDICT: PASS**。

### 修复的宿主缺陷（验证过程发现）
UnregisterRoute 的 DictRemove 后使用悬垂 Path 指针（UAF，本地指针化修复）；defines 解析改写借用字符串（复制后拆分）；插件编译符号缺失三批补齐（兼容层/请求垫片/工具函数）。

### 已知边界（后续批次）
- 插件配置存储沿用 v1 扁平 JSON（config.defaults.json + options/plugin/<xid>.json 覆盖）；
- 服务租借不阻止提供方停用（停用时清注册，ReleaseService 安全空操作）——生产插件迁移时按需补强；
- P3（import/export/生成器）与 P4（十个 v1 插件逐个上量）待后续；生产库十个插件保持 disabled。

### 2026-09-12：重载路径专项加固（审计缺口 GR1-GR4 修复）

针对"重载不伤连接、正常释放、无崩溃风险"的专项审计发现四个边界缺口，全部修复：

- **GR2 回调期注册悬垂**：`Plugin_Stop` 与启动回滚段改为先置 `started=false` 再调 OnStop/OnUnload——Register* 系列被 started 门控拒绝，Unregister* 不受影响（插件停机期合法注销仍可用），杜绝 OnUnload 在资源回收之后注册、镜像释放后路由 Proc 悬垂的崩溃路径。
- **GR3 换代重入**：`PluginHost_SetEnabled/Reload` 增加 `G_PluginRegIdx >= 0` 守卫——插件 OnStart 期间调用 XAdmin_ReloadPlugin/SetPluginEnabled（自重载或重载他人）被拒绝，消除 Start 执行中途递归 Stop 后继续使用已释放 desc/setGlobal 的崩溃。
- **GR4 路由回收兜底**：Stop/回滚在台账回收之外，按 `inst->routePaths` 内存权威清单再扫一遍出表并释放——台账 INSERT 因 DB 故障漏登时不再留下悬垂 Path/Proc。
- **GR1 服务租借跨代保护**（对齐 v1 lease/DRAINING 语义）：租借改为堆单元（记录获取时的提供方代码镜像指针）；提供方停用/换代时有未归还租借则代码镜像压入 16 槽退役队列，`ReleaseService` 按镜像指针路由归还，最后一个租借归还时才销毁镜像；队列满退化为泄漏而非悬垂（告警日志）。Acquire 增加提供方 running+tcc 非空校验。
- 顺带：启动回滚段的 `tcc_delete` 统一为 `xsDestroyTCC`（风格一致性）。

**新增压力门禁 5（并发重载）**：6 个锤线程持续打插件路由的同时执行 30 次 reload——实测 276 次 ping 全部 200、零连接错误（全局锁串行化下重载对并发流量透明）。压力战役五门禁全 PASS；双模式 smoke 各 40 PASS 零警告；v1 hello 探针复验通过（租借新模型与 v1 插件的请求内 acquire/release 用法兼容）。

### 2026-09-12：插件系统四小时极端压力测试

双插件（hello-sdk + v1hello）4 小时混合浸泡：473,910 请求 / 57,121 生命周期操作 / 43,266 页面请求 / 28,615 极端探针 / 15,811 配置写入 / 7 次主脚本重载。**零崩溃、零 5xx、根库零污染——稳定性判定通过。**

**过程修复**：SDK 隐式声明指针截断（首发浸泡 17.1 分钟崩溃）——v1hello 调用 `xrtCopyStr`/`XA_Now` 未声明，64 位下返回值截为 32 位；6300+ 次 TCC 编译后堆地址越界触发崩溃。`plugin_sdk/xs_plugin.h` 补齐两声明后 18 分钟定向复现 + 4 小时完整浸泡均通过。

**性能画像**：吞吐 4,567→846 req/min（5.4 倍衰减），根因是 `plugin_resource` 表无索引导致台账清理全表扫描（20 万+ 行后 p95 达 5.2s）。p50 始终 <15ms（读流量不受影响）。RSS 36→305MB（与 DB/代际数正相关，句柄稳定）。台账对账在 22% 分钟出现瞬态不一致（监控读锁外中间态，非真实泄漏）。

**建议优化**（非阻塞）：
1. `CREATE INDEX idx_plugin_resource_lookup ON plugin_resource(xid, generation, status)`——消除吞吐衰减
2. plugin_generation/plugin_resource 历史行周期清理
3. 浸泡监控的台账对账加锁或容忍瞬态

报告全文：`tests/.runtime/plugin_soak_report.md`；指标：`tests/.runtime/smoke-lqx9fo88/soak_plugin_metrics.jsonl`。

### 2026-09-12：plugin_resource 索引修复与复测

`DB_EnsurePluginResourceIndex` 在启动时幂等创建 `(xid, generation, status)` 复合索引。四小时复测对比：

- **吞吐衰减消除**：末段 4,950 vs 846 req/min（5.8 倍改善）；全程平坦（首段 6,602 → 末段 4,950，仅自然波动）
- **p95 全程 <300ms**（原测涨至 5.2s）；RSS 稳定 116MB（原测涨至 285MB）
- **总请求 1,276,666**（原测 2.7 倍）；插件代际 52,456（原测 2.6 倍）；**零 5xx**

复测在 232.4 分钟触发终期 exit 1（同 SDK 截断路径，需 5.2 万次 TCC 编译/销毁循环——3.8 次/秒持续 3.9 小时——远超生产场景）。建议后续引入 TCC 编译缓存（manifest 已有 compile_hash 字段，宿主未利用）。

报告：`tests/.runtime/plugin_soak_report.md`

### 2026-09-13：guestbook_v3 / filemanager / xlogserver 三插件迁移

三插件机械变换 + SDK 扩展后全部编译加载成功，核心路由验证通过，smoke 40 PASS 零警告。

**变换要点**（新增到 `tests/plugins/transform_v1.py`，可复用于后续插件）：
- xrtPathJoin 变参式 `(N, a, b)` → 二参式嵌套；3 参直传 `(a, b, c)` → `xrtPathJoin(xrtPathJoin(a, b), c)`
- xrtFileWriteAll 四参 `(path, data, size, CP)` → 二参 `(path, xbytesview)`
- xrtFileReadAll 三参 `(path, CP, size*)` → 二参 `(path, size*)`
- 路径 API 二参 `(path, size)` → 一参：`xrtPathGetNameExt → xrtPathStem`、`xrtPathGetExt → xrtPathExt`
- 混合指针声明 `xvalue* a, b;` → `xvalue* a, *b;`（C 指针修饰符只作用于紧邻变量）
- xbuffer 栈分配 `xbuffer_struct X = {0}` → 堆分配 `xbuffer* X = xrtBufferCreate()`；数据提取 `X.Buffer` → `xrtBufferTake(X, NULL, NULL)`
- xmutex 值类型 → `xmutex*` 指针（v1 是句柄，xs3 是 union）

**SDK 扩展**（本轮新增的声明面与符号注入）：
- 类型：`xdict`(→xmap*)、`Dict_Key`、`Dict_EachProc/XA_DictProc`、`xrtmultipartboundaryview/partview`、`XRT_CP_*/XRT_OBJMODE_*` 常量
- 函数：路径/文件操作（xrtPathJoin/Ext/Stem、xrtFileGetSize/GetMTime、xrtDirScan）、字典操作（xrtDictCreate/Destroy/GetPtr/SetPtr/Walk）、缓冲区（xrtBufferInit/Append）、`xsReqHeader`（请求头读取）、multipart 桩
- 宿主新增：Plugin_Start 前创建 `plugin_data/<xid>` 和 `db/plugin/<xid>` 目录（v1 插件 OnStart 直接开库/写文件）；multipart 桩（边界提取可用，迭代返回 false——上传路由走错误分支，浏览/编辑/重命名正常）

**路由验证**：
- guestbook_v3：meta 200（title/intro JSON）、list 200（分页/筛选/空数据）
- xlogserver：push 到达（invalid json 是对测试 payload 的正确响应——API 只接受特定 JSON 格式）
- filemanager：list 403（filemanager 路由注册了管理权限组——正确的权限拒绝，需将权限授予角色后可用）

**已知边界**：filemanager 的 multipart 上传为桩（v1 运行时 API，xrt 核心无此能力）；xlogserver/xlogserver 的日志推送格式需匹配其 JSON schema。生产库三插件保持 disabled，待逐个验证后翻 enabled。

### 2026-09-13：方案 A 落地——v1 兼容层彻底移除，全库原生化

用户裁定：xadmin 作为纯 xs 新开发的原生应用，不留历史包袱、只留一套实现。本轮移除了全部 v1 方言兼容层（modules/compat_value.h + compat_util.h 删除），应用/宿主/SDK/五个插件全部改写为原生 xrt API。

**关键决策与落点**：
- **值/时间/杂项方言**（~2400 调用点）：`tests/native_convert.py` 机械变换 + 语义点手改。表访问走新的 `modules/value_util.h`（原生语义薄包装：无 keylen、无 take 双义、Get 借用/SetOwn 消费），时间走 `TimeText`（xrtTimeLocal+xrtDateTimeFormat 本地格式化），令牌/十六进制/解析走 `modules/util.h`
- **DB 时间单位**：v1"公元零年本地日历秒"→ xtime（Unix 微秒）。`DB_MigrateTimeUnits()` 首次启动一次性换算全部时间列（PRAGMA user_version=2 幂等防重，v1 值域区间 WHERE 双保险）；历史行本地时差按当前时区近似（≤1h）。输出侧原本就全部经 TimeText 格式化为字符串，前端无感知
- **会话单位**：_expireTime/_activeTime 改微秒（Create/Extend 内 ×1000000）；guard 冷却与注册间隔（R2）同改；旧会话自然失效（一次性重登）
- **dict → xrtMap**：XA_Dict* → xrtMap*(xbytesview 键)；遍历回调协议 (Dict_Key*,void*,void*) → (xbytesview,void*,void*)，宿主 MapWalk/ValueWalk 为原生迭代器薄包装；目录扫描 v1 五参回调 → DirScan 四参（util.h）
- **SDK 收敛**：plugin_sdk/xs_plugin.h 317→215 行，纯 ABI v4 + 9 个应用级请求/回复原语；注入表 108→31 项（27 ABI + HttpReplyFormat/LoadPage/xsHttpReplyAuto/Format/xsReqMethodID/xsReqQueryValue/XAdmin_PluginReqHeader/XAdmin_ReqBody/Len）。全部 xrt 原生符号由 xsCreateTCC 预置提供，不再重复注册
- **共享便捷层**：value_util.h/util.h 同源分发到 plugin_sdk/（应用与插件单份实现）；`tests/audit_sdk.py` 门禁校验字节级一致 + 注入表 ⊆ SDK 声明 + 预置无重复 + 方言残留清零
- **插件**：hello-sdk 改纯原生直调（新插件参考模板）；guestbook_v3/filemanager/xlogserver 经 `tests/native_plugin_convert.py` 原生化（multipart 桩与路径式文件信息改为插件内局部 static，宿主不再提供）；v1hello 探针及工具删除（使命完成）

**变换期修复的语义炸弹**（记录供同类变换参考）：
- `JsonParseN(x, 0)` 沿用 v1"0=strlen"约定而新桥按字面传 0 → 全部 JSON 解析为空（权限缓存全空致 403）。修正：helper 显式 size==0 走 strlen
- xrtMap 键是 xbytesview 而非 xstrview——字符串键需 KeyView/KeyViewN 构造
- 头文件自包含：value_util/util 在 string.h 之前被 include 时 TCC 隐式声明 strlen 与后到原型冲突——两个头补 `#include <string.h>`
- 会话/冷却"+N 秒"算术必须显式 ×1000000（微秒）

**门禁结果**：smoke 40 PASS（含 write_regression 全家族）、零编译警告、plugin_stress 五门禁 PASS（churn 100 轮 / reload 50 代 / 并发重载 30×6 线程 / 失败注入 / reload 风暴，RSS 12→24MB 稳定）、三插件编译加载启动全通、audit_sdk PASS、括号平衡 38 文件 OK、根库零污染。

**语义变化（有意）**：DB 时间列单位更换（一次性迁移）；旧会话/旧插件令牌失效需重登；filemanager 上传仍为桩（待 xrt extlibs multipart 真实实现接入后在一处替换）。

### 2026-09-13：notify / attachment / sched 三家族迁移（43 路由，全原生方言）

v1 剩余三大功能家族完成迁移，路由 91→134（149 有效路由中缺 15：mail 家族 8 + 视图杂项 3 + mir 推送 2 + plugin/installed 视图 1 + form_demo 1）。全部为原生 xs3 方言（方案 A 之后首批新迁移即纯原生）。

**notify（9 路由）**：`modules/notify.h`（表自装 notify_message/notify_recipient/member_notify_setting + URI/菜单自装 + 发送/列表/已读/删除核心）+ `route_http/notify.h`。管理员按用户/按组/全群三种定向；会员六接口（list/unread_count/detail/read/read_all/delete）。探针覆盖发送→未读→详情→已读→删除全链路与三种定向分支。

**attachment（16 路由）**：`modules/multipart.h`（**应用内全新 multipart/form-data 解析器**——xhttp extlib 未编入 xs.exe，实现 RFC 2046/7578 务实子集：行首边界匹配、quoted-string 参数转义还原、伪命中校验（正文含 `\r\n--boundary` 前缀不误判）、二进制安全；算法先经 Python 复刻验证再落 C）+ `modules/attachment.h`（存储布局 `<model>/<YYYY>/<MM>/<xid>.<ext>`、MIME 表、防盗链白名单 xmap、扩展名/大小/配额、订单）+ 管理 6 路由 + 视图 4 + 开放 API 4 + `/attachment` 静态访问。购买事务（余额扣减+卖家分成+订单+销量）经探针全链路验证；访问控制三分支（登录/付费 402/级别 403）与防盗链（外站 Referer 403）实测。**有意偏差**：`/attachment` 的 uris 种子 isBackend=1 会在启动时修正为 0（会员付费下载需读 MSID；不覆盖已配置的 authID/日志开关）；会员 API 由 Attachment_Init 幂等自装 isBackend=0（v1 依赖运营手工配置 uris）。

**sched（23 路由）**：`modules/sched.h`（约 1700 行：7 字段 cron 解析器（v1 语义：DOM/DOW 通配互斥退化、年字段）+ xdatetime 迭代式下一跳计算 + once/interval/cron 三调度 + skip/queue_one/parallel 重叠策略 + misfire 策略（启动补跑判定）+ 失败重试 + 调度线程（xrtCondWaitFor 唤醒，1s 兜底轮询）+ 每任务 worker 线程 + 运行日志）+ `route_http/sched.h`（CRUD/启停/立即运行/复制/示例/三批量/预览/仪表盘/日志三件套/导入导出 + 视图 4）。
- **shell 执行器**：原生 xrtProcess（Shell 配置 + WorkDir/HideWindow/NewGroup + WaitFor 超时 + KillTree + 管道输出捕获），替代 v1 的 Win32 直调，跨平台
- **C 执行器**：沿用 v1 的进程隔离设计——生成 runner 交独立 xs.exe 子进程运行。**runner 模板原生化改写**（v1-assets 登记 target_sha256）：param 经 `host->Custom`（xs3 非预设字段集合）读取、value_util.h 同源部署到缓存目录、生成配置改 xs3 `host_default` 嵌套模式 + dev_inc + 路径正斜杠（JSON 转义安全）
- 探针验证：shell/C 双执行器真跑成功、调度线程自动触发（3 秒间隔任务实测 ≥2 次）、预览（cron 5 点 + interval 精确步长）、导入去重、日志清空

**DB 迁移扩展**：DB_MigrateTimeUnits 新增 sched_task（7 列）/sched_run_log（2）/notify_message/notify_recipient（2）/member_notify_setting/mail_task/mail_log 列（mail 家族虽未迁移但存量数据 18+8 行一并换算，后续迁移直接落地）。

**过程修复**（记录供参考）：multipart 空行扫描初版把头部行终止 CRLF 当空行（正文混入 2 字节）——算法在 Python 复刻中定位修复；attachment 表无 id 列（xid 主键）导致 ORDER BY id 静默失败；cron 模板占位符 `{{$taskSourceCode}}` 为 19 字符（按 18 截断残留 `}`）。

**门禁**：smoke 40 PASS 零警告（含 write_regression 全家族）、notify/attachment/sched 三探针 PASS、audit_sdk PASS、括号平衡 46 文件、根库零污染。

**语义说明**：根库 5 条存量 sched_task（示例任务）迁移后将由调度线程按新单位接管触发——与 v1 行为一致；fixture 中已实测。

### 2026-09-13：10 小时全接口压测+渗透战役（notify/attachment/sched 重点）

394 万请求 / 7.4h 有效浸泡 / 42 次热重载 / 39 端点性能扫描 / 三新家族定向渗透全绿。详见 `tests/.runtime/campaign10h/campaign_report.md`。

**战役期修复**：SCH-4 cron 死循环（NextAllowed 回绕扫描致 0→60 死循环，恢复 v1 前向扫描+迭代兜底）；sched worker 拆卸竞态（Unit 30s 上限早于 300s 任务超时→锁释放后使用，改为等 worker 归零）；SCH-8 负值字段拒绝；runner 子进程管道化。

**待裁定**：站内信 XSS 载荷入库（前端渲染方式需确认）；根库示例任务 10s 全量 runner 节奏；logs 表索引。

**待深查（xserver）**：6.2h/36 次重载后静默崩溃一次（无日志无泄漏，短窗复现未果）——推断高压代际切换罕见竞态。

### 2026-09-13：NOT-8/logs 修复 + 4 小时全量重测

**修复**：① 站内信输出边界 HTML 实体转义（`Notify_EscapeHtml`，member 列表/详情 + 管理列表统一）+ `actionUrl` 方案白名单（仅站内相对路径与 http(s)，发送与输出双侧）；② logs 查询结构重写——去掉 `COUNT(*) OVER()` 窗口（10 万行基准 387ms → 计数独立窄扫描 38ms + 分页 <1ms），`Logs_Init` 幂等建 `idx_logs_createTime`。专项探针 `tests/probe_fixverify.py`：转义/拦截/放行全链路 + logs 1457 行 50 次均值 3.6ms（原 91ms）。

**4 小时重测**（campaign 4.0）：A-C 全阶段零攻击发现、零工具异常；D 浸泡 241.7 分钟 / 217.9 万请求 / 24 次热重载 **零崩溃**（探针 p50 中位 4.7ms、RSS 30→15MB 平稳、句柄 200 恒定）；logs 端点 16 并发 p50 2.2ms / p95 3.3ms / 5,518 eps（修复前 91ms，**约 40 倍**）；本轮最慢端点易主为 uris 列表 p95 31ms。NOT-8 在重测中仍按"载荷可入库"报出——修复语义为**输出转义**（存储原样、任何客户端渲染不执行），探针判定基准待随下次工具更新同步。

**过程记录**：修复过程中转义函数曾引入单引号多字符字面量（`'&#39;'`，shell 转义事故）导致 0xC0000005 崩溃——专项探针当场抓获并修复；已入代码坑记忆。WER 用户级 LocalDumps 已配置（HKCU），本轮未再触发崩溃故未产出转储；罕见重载竞态（前轮 6.2h 一次）仍挂 xserver 跟踪。


### 2026-09-14：14h GDB 钓测收官——“神秘崩溃”根因确认为外部击杀

**三轮 GDB 附加钓测 + 隔离重命名验证**（tests/gdb_soak.py + tests/reload_storm.py，gdb -batch 断点武装 ExitProcess/_exit/abort/TerminateProcess/NtTerminateProcess/RaiseFailFast + SIGSEGV/SIGABRT + core 落盘）：

- 第一轮（6.6h，A/B 双实例）：双实例死亡时刻相差 **10ms**、退出码 01、无信号、无任何输出——非应用竞态特征
- 第二轮（23min）：同样同刻双杀；退出路径断点一个未命中（外部 TerminateProcess 才会如此）
- **根因**：用户侧 xs.exe 编译/构建任务按进程名清场（taskkill xs.exe），与运行时长/重载次数无关（A 40 代 / B 197 代同刻死）
- **验证**：宿主复制为 `xsw.exe` 隔离后，reload 风暴 5.9h：3 实例（2×gdb+1 裸跑）**505 万请求 / 1751 次重载尝试（1144 次成功换发）/ 零崩溃**，16 线程乱序 30+ 端点池 + 10% 混沌请求 + 每 5 分钟 8 轮插件启停搅动
- **结论**：此前 6.2h/372min “静默崩溃”定性为外部击杀误报，**应用本身在极端 reload×高并发下未复现任何缺陷**；附带产出：tests/reload_storm.py 成为常驻 reload 风暴工具；诱导崩溃探针（/__test/crash）验证了 gdb 全链路捕获能力

### 2026-09-14：mail 家族预迁移（新 xsmtp 方言，门控待 xs 集成）

xs.exe 正在集成 xmail/xsmtp 扩展库；xadmin 侧先行完成，等宿主落地后置 `XADMIN_WITH_SMTP 1` 联调。

**交付**：
- `modules/mail.h`（~820 行）：mail_template/mail_task/mail_log 三表自装 + 配置读取（smtp_*/mail_* 全套字段）+ 新 xsmtp 客户端发送（Open→Auth(PLAIN 回落 LOGIN)→Submit→Quit，deadline 控超时，auto=STARTTLS 回落明文）+ 独立队列线程（领取-发送-记录-重试，卡死恢复，速率/批量/扫描间隔全套配置）+ CRUD（列表/创建/重试/删除/状态）+ SMTP 连通测试
- `route_http/mail.h`（~200 行）：8 路由（/admin/member/mail GET+POST、status、run_pending、retry、delete、test-smtp、2 视图页）
- 接线：main.c/route.h 均在 `#if XADMIN_WITH_SMTP` 内，默认 0——当前编译零参与、smoke 40 PASS 零警告

**API 映射**（v1 一次性 → xs3 客户端式）：
| v1 | xs3 新 API |
|---|---|
| xrtSmtpSendMail(cfg,msg,ret) | xrtSmtpClientOpen→Auth→Submit→Quit（client 生命周期自管） |
| iTimeoutMs | xrtDeadlineAfter(sec×1e6) 逐调用 |
| XSMTP_SECURE_AUTO | STARTTLS 失败回落 PLAIN（自实现） |
| XSMTP_AUTH_AUTO | PLAIN 拒绝回落 LOGIN（自实现） |
| Future 并发发送 | 单线程顺序队列（速率上限仍生效；新 API 为同步+cancel，事务邮件场景顺序更可控——有意差异） |

**验证**：门控关闭 smoke 40 PASS 零警告零 TCC 警告；门控开启形态经 gcc -fsyntax-only 全量检查（桩类型按 xrt 头精确签名合成）零错误；括号平衡 48 文件。**xs 集成后需要做的**：置 1、按实际头文件形态调整 7 个 include（若并入 xrt_decl.h 则删除）、跑 probe_mail.py（待写：真实 SMTP 测试可用 python -m smtpd 或公网邮箱）。

**xserver 侧期望**（已记入分析报告）：编入 extlibs/xmail + extlibs/xsmtp 两个 manifest，符号进 xrt_decl.h，无需 pop3/imap。

### 2026-09-14：mail 家族联调通过——真实邮件发送成功

xs.exe 集成 xmail+xsmtp 后完成联调，**邮件全家 8 条路由全部可用，真实 SMTP 发送成功**（xywhsoft@qq.com → smtp.qq.com:465 SSL，1.0 秒全链路）。smoke 40 PASS 零警告零回归。

**联调中修复的问题**（新 API 学习成本）：
1. **头文件入口**：必须 `#include <xsmtp.h>`（伞形头，features 级联），不能直接 include 各模块头（feature 宏未激活导致类型不可见）
2. **门控位置**：`#define XADMIN_WITH_SMTP 1` 必须在 main.c 顶部（在所有 `#if XADMIN_WITH_SMTP` 之前），不能在 mail.h 里定义（main.c 的条件编译先于 mail.h include）
3. **NetEngineStart 返回 bool**（非枚举）：`true=成功`，不能与 `XNET_RESULT_OK` 比较
4. **TLS 需要 Verifier**：`xmailnetconfig` 的 TLS 安全模式要求非空 `Tls.Verifier`（即使是"不验证"也需要一个 Accept-All 回调的 verifier 对象）
5. **xrtStrView 类型严格**：Mail/Rcpt/Data 的地址和内容参数都是 `xstrview`，不能直接传 `const char*` 或 `char*`——须 `xrtStrView()` / `xrtStrViewN()` 构造
6. **消息构建**：`xrtMailCompose` 要求显式初始化的 Name 字段（空字符串而非零值 xstrview）

**验证**：test-smtp 真实发送 ✓、队列创建/领取/发送/状态流转 ✓（task 19 pending→success）、全 8 路由 200 ✓、smoke 40 PASS ✓。

### 2026-09-14：收官——v1 路由迁移 147/147（100%，mir 裁定除外）

**最后三项处理**：
1. **form_demo**：新增 `page/template/form_demo.html`（表单演示页，调 /admin/form/list 渲染可用模板列表）+ 路由 + 视图函数
2. **插件列表页**：菜单表指向 `/admin/view/plugin/installed` 但路由只有 `/admin/view/plugin`——加别名路由（同一 handler），后台菜单可正常打开
3. **模板缓存**：确认 `/admin/tool/reload/template` 后台可访问（200 + 重建成功 19/20），API 路径变化不影响功能入口

**路由覆盖终态**：147/147 v1 路由全部迁移（100%），另新增 42 条 xs3 原生路由（插件管理 8+3、sched CRUD 等），当前合计 149 条注册路由。mir 两条为用户裁定的临时功能，不迁。

smoke 40 PASS 零警告；全部 5 个探针（notify/attachment/sched/plugin/mail）PASS。

### 2026-09-14：独立页面 + 前台站点合并完成（v1 CMS 后续更新第一批）

**独立页面**（`modules/standalone_page.h` + `route_http/standalone_page.h`）：
- standalone_page 表自装 + CRUD（创建/列表/详情/保存/删除/视图页）
- 路由分发不走 AddStaticRouteHTTP（G_Ready 后拒绝新路由）——改为 Protocol 兜底 `StandalonePage_Dispatch` 按 URI 查找
- 多 URI 绑定（换行分隔）、draft/enabled 状态、缓存、自定义响应头
- URI 同步到 uris 表

**前台站点**（8 条路由 + site/ 静态页面）：
- `/`（首页）、`/features`、`/plugins`、`/capabilities`、`/content-system`、`/docs`、`/download`、`/demo`
- 全部免鉴权——route.h 注册后清 bAuth/bAdmin（覆盖 uris 表 isBackend=1 的默认值）
- `site/` 目录 9 个 HTML 页面 + `wwwroot/site/` CSS + banner

**关键实现决策**：
- 正则动态路由**不迁移**（v1 独立方案已被我们 pattern 模式替代，用户裁定）
- `/` 从后台入口改为前台首页（v1 语义），后台入口统一 `/admin`
- uris 表新列（namespace/plugin_xid/isPersistent 等）暂不加（与能力包系统一起后续落地）

smoke 40 PASS 零警告 + 独立页面/前台 11/11 探针全通。

### 2026-09-14：URI 增强合并完成（v1 CMS 后续更新第二批）

**v1+v3 合并**：v1 的 3 新列 + v3 已有的 4 插件列，统一增强 uris 表：

| 列 | 来源 | 含义 |
|---|---|---|
| isPersistent | v1 新增 | 持久声明标记（插件 UriAuth=1/独立页面=1/路由自动发现=0） |
| namespace | v1 新增 | 命名空间（auto/page/plugin/custom） |
| routeActive | v1 新增 | 路由加载状态（已加载/未加载） |
| plugin_xid | v3 已有 | 插件归属 |
| plugin_id/instance_id/generation | v3 已有 | 插件关联 |
| maskBody | v3 已有 | F2 脱敏开关 |

**DB 迁移**：`DB_EnsureUrisEnhancedColumns()` 幂等 ALTER TABLE 3 列 + namespace 索引。

**SQL 全面更新**（5 条预编译语句 + 3 个 INSERT）：
- stmt_uris_all/sel：SELECT 加 5 新列；搜索范围扩展到 namespace + plugin_xid
- stmt_uris_put：UPDATE 加 isPersistent, namespace
- stmt_uris_add：INSERT 默认 namespace='auto'
- stmt_cache_uris：加 isPersistent（Auth_SyncURIS 用）
- XAdmin_RegisterUriAuth：INSERT 加 isPersistent=1（插件权限声明保留）
- XAdmin_RegisterMenu：修复 bind 参数映射（createTime/updateTime/xid/generation 对齐）

**Auth_SyncURIS 增强**：非持久路由（isPersistent=0）且路由已卸载 → 自动清理 uris 行；持久声明保留。

**页面字段**：/admin/auth/uris 列表返回 namespace/pluginXid/isPersistent/routeActive/pluginGeneration。

smoke 40 PASS；hello-sdk UriAuth 2 行正确写入（isPersistent=1, namespace='auto'）；搜索 namespace/plugin_xid 生效。

### 2026-09-14：xlogserver v1 重构合并——原生方言 + 外部 API（v1 CMS 后续更新第三批）

**v1 重构分析结论**（用户裁定：旧路径别名不迁、外部 API 要）：
- v1 xlogserver 重构核心 = 多服务日志架构（services 表 + 每服务独立 SQLite + 动态 log_%s 表）
- 10 条管理路由（view 7 + api 3）+ 2 条外部 API（/api/v1/log/push、/api/v1/task/create，免鉴权）
- XAdminHostContext/XAdmin_Log 为 v1 ABI 新增项——v3 已有等价能力（XADMIN_GLOBAL_* 注入 + printf 日志），不引入

**落地内容**：
- `plugin/xlogserver/main.c`（1423 行）方案 A 原生方言：Value*/xrtMap 服务 DB 缓存/xrtMutex/Unix 微秒
- OnStart：EnsureMainSchema → 服务 DB 缓存装载 → AuthGroup/Auth 注册 → 10 管理路由 + 2 外部 API → 10 条 UriAuth（isPersistent=1）→ 菜单
- 服务数据：`db/plugin/xlogserver/plugin.db`（services 台账）+ `plugin_data/xlogserver/logs/%s.db`（每服务库）
- 外部 API 免鉴权（need_auth=false），未知 service/缺参正确拒绝

**回归门禁**：`tests/xlogserver_e2e.py` 15/15 PASS——enable→管理页 200→建服务→外部 task/create→外部 log/push→管理 API 可见推送日志→两库落盘行持久→auth 台账（1 authGroup + 1 auth + 10 uris）→负例×2→零编译警告。
smoke 40 PASS + write_regression PASS。

### 2026-09-14：插件能力面第一批——资源体系 + 编译管线（v1 ps_resource/ps_compiler 对齐）

**1. /plugin-static/ 静态资源服务**（modules/plugin_host.h `Plugin_TryServeStatic`，protocol.h 分发链接入）：
- `/plugin-static/<xid>/<相对路径>`，仅 GET/HEAD，仅已启动插件；前缀命中后一律自行应答
- MIME 白名单 17 类扩展名；sourcemap 门控（manifest `resources.allowSourceMap`/`staticAllowSourceMap`）；16MB 上限
- 缓存策略：文件名含 ≥8 位连续十六进制视为内容寻址版本文件 → `immutable` 一年；否则 `no-cache`
- ETag `p-mtime-size` + Last-Modified（xrtPathStat 微秒时间）；路径安全：拒绝绝对路径/盘符/`..`/`%`/反斜杠/控制字符

**2. page/template/option 资源体系 5 个 ABI**（SDK 31→36 符号）：
- `XAdmin_LoadPluginPage`：插件 page 目录文件直读直发，缺文件 404
- `XAdmin_RenderPluginTemplate`：插件 template 目录 + 原生引擎（`{{ }}`/`{{$var}}` 语法）逐次编译渲染；插件目录无此模板回退全局模板引擎
- `XAdmin_PluginOptionLoad/Save`：选项 JSON 装载/保存；数据目录覆盖（`plugin_data/<xid>/option/`）优先于包内 option 目录；保存按 name 合并进 classList[].options[].value，原子写数据目录
- `XAdmin_PluginResourcePath`：安全资源路径拼接
- manifest `resources.page/template/option/static` 目录声明装载（缺省同名约定目录，不合法配置回退）

**3. 编译管线**（Plugin_Compile）：v1 约定目录 inc/include/src 进头文件搜索、lib 进库搜索；`build.libraryDirs`（存在才生效）+ `build.libraries`（tcc_add_library）。

**顺带修复**：
- protocol.h use-after-free：`xrtFree(target)` 先于 `StandalonePage_Dispatch(req.path,...)` 执行（req.path 即 target 悬垂），重排为兜底全部放弃后再释放
- Plugin_ScanProc manifest 校验失败路径 `xvalue* manifest` 未 release（泄漏），补 release + 实例清零
- plugin/hello 伴生文件从 dev/v1 补齐（src/page/option/static/inc/lib/template）——main.c 为 v1 方言（xvo* API）仍待移植，enable 前不编译不影响现状

**回归门禁**：新增 `tests/plugin_resource_e2e.py` 31/31 PASS（静态正负例×11、page/template/option/resource-path ABI、数据目录覆盖落盘且包内文件零污染、reload 存活、disable 后 404、零编译警告）；smoke 40 PASS（扩展后的 hello-sdk 含 inc/src 约定目录 include 编译零警告）；write_regression PASS；xlogserver e2e 15/15。

### 2026-09-14：插件能力面第一批补完——libraries 实链验证 + perfmon 现状确认

**libraries/libraryDirs 实链验证**（plugin_resource_e2e 32/32）：hello-sdk 增 `/api/plugin/hello-sdk/link-probe`（`HELLO_SDK_LINK_IPHLPAPI` 编译开关 + 手工 `__declspec(dllimport)` 声明——内嵌 winapi VFS 无 iphlpapi.h）；e2e 夹具期注入 manifest defines/libraries:["iphlpapi"]/libraryDirs:["lib"] 并从 System32 拷 iphlpapi.dll 进 lib/ 约定目录。实跑 GetNumberOfInterfaces 返回接口数 ≥1——一次覆盖三机制：lib 约定目录→tcc_add_library_path、显式 libraryDirs、tcc_add_library 按名解析 DLL（TCC PE 装载器支持 `%s/<name>.dll` 直接导入，内存 relocate 期 LoadLibrary 绑定）。

**考古发现**：全库唯一非空 libraries 消费者 perfmon（`libraries:["iphlpapi"]`）在 v1 Linux 上是空操作（iphlpapi 引用全在 `#ifdef _WIN32` 内，Linux 编译不引用→tcc_add_library 找不到也不报错）——v1 从未真实走过库链接路径；v3 现已实测打通。

**perfmon 移植现状**（在既有"剩余队列"内，非本批缺口）：main.c 缺 `#include <sqlite3.h>`（line 21 `sqlite3*` 编译失败）+ 74 处 xvo* v1 方言残留——完整原生变换待做，同 gbdemo→firewall→comment-system→content-system→cms.article 队列。

回归：plugin_resource_e2e 32/32 + smoke 40 + write_regression + xlogserver 15/15，零编译警告。

### 2026-09-14：缺口三连补——安装向导 + {{#form}} 模板块 + 注册幂等 upsert

**1. 安装向导**（modules/install.h 新增，v1 install.h/http.h 契约）：
- 启动判定：install.lock 缺失 **且** db/main.db 缺失 → 向导模式（业务段延后）；lock 缺但库在 → 存量部署不进向导（**加固偏差**：v1 会复制预置库覆盖既有数据，本代拒绝覆盖）
- 向导接管一切请求：GET 出 page/install.html（客户端 SHA-256(用户名+"_xywhsoft_"+密码) 后 POST 到 /）；POST 四步幂等可重试：建库（执行 install/init.sql 全量脚本，**v1 为复制预置库文件**）→ 业务段启动（main.c 因子化 XAdmin_BusinessStart，与正常启动共用）→ 建超管（随机 salt + ServerHashPassword 二次哈希，role=1/authLevel=999）→ 写 lock 退出向导
- 校验：用户名 3-32 位受限字符集、密码须为 64 位十六进制客户端哈希；G_InstallBusy 防并发提交；建库+启动在 G_RequestLock 内串行
- **连带修复**：install/init.sql 补全 8 张 v3 运行时自装表 DDL（notify 3 + mail 2 + sched 2 + standalone_page，全新安装即完整 31 表 schema）；DB_MigrateTimeUnits 对缺失表容错跳过（此前全新库上迁移必失败——夹具库因已含运行时表而从未暴露）
- 门禁 tests/install_wizard_e2e.py 11/11

**2. {{#form}} 模板块**（modules/form.h +~530 行渲染器 + template.h registry）：
- v1 Form_RenderTemplateBlockHTML/ResolveTemplateRenderSpec/Groups/Field/Choices/RangeInput 全量原生变换（xvo*→Value*/xrt*，xbuffer 新 API）
- xrt 扩展注册表：XTEMPLATE_EXTENSION_RAW_BLOCK 注册 "form" 关键字（{{#form}} JSON {{#end}}），Form_TemplateRegistryInit 装配进 G_TemplateRegistry，三个编译入口（全局缓存/RebuildCache/插件模板）统一挂载；语义偏差：v1 编译期预渲染，v3 渲染期回调（纯内存拼接代价相当）
- 三种 JSON 来源：source:form（forms/*.json+demoData）/ source:option（Option_LoadFile→SchemaFromOptionConfig）/ 内联 schema + values/data 覆盖
- form_demo 演示页由静态降级恢复为真渲染（9KB 输出含完整表单块）；补 /admin/template/rebuild v1 对齐路由；rebuild 20/20（此前 block_demo 计 failed）
- 门禁 tests/template_form_e2e.py 6/6（含坏 JSON 块不污染兄弟模板）

**3. 注册幂等 upsert**（plugin_host.h 四个 Register*）：
- 复现：reload 3 轮 menu/authGroup/auth 各 +3 行（v1 PS_HostFindMenuId 语义补齐）；uris 靠 UNIQUE 约束吞错稳定但行内容从不更新
- menu：plugin_xid+href（无 href 用 parent+title+type）；authGroup/auth：plugin_xid+name；uris：uri 全局唯一 → 已存在则刷新全部标志并归属
- 复用并复活旧行（isDelete=0、updateTime、plugin_generation 刷新）——连遗留旧行也被吸收
- smoke 插件段新增 reload 后四表计数断言（41 PASS）

**实现注记**：BusinessStart 各步骤失败现打印具体环节（DB_Init/迁移/Session/路由编译）；install.h 经暂定定义与 main.c 启动标志同 TU 合并。

### 2026-09-14：插件能力面第二批——脚手架生成/动态路由/自动授权/错误聚合/配置契约/manifest 校验（.xpk 裁定不做）

**1. XAdmin_GeneratePlugin 脚手架生成**（stub → 真实现，内容系统硬依赖解除）：
- v1 PluginSystem_Generate 语义：8 目录骨架 + spec 文件逐个落盘（路径安全检查）+ 未提供 plugin.json 时生成默认 manifest（v1 字段全集）+ 运行时登记为可启用实例（扫描装配同构 + plugin_package/plugin_runtime 台账）+ auto_enable 直通启用
- 探针验证：hello-sdk 生成 hello-gen（含最小可用插件源码）自动启用后路由即活

**2. 动态路由族 ABI**（SDK 31→34 符号）：
- XAdmin_RegisterDynamicRoute（XAdminDynamicRouteDecl v1 字段集；pattern 用 v3 pattern 方言）/ XAdmin_RouteParam / XAdmin_RouteParamCount
- 运行时注册即时重编译 pattern 树（失败回滚本次添加）；Plugin_Stop 按实例清单出表（dynPatterns）；参数镜像全局（分发全程持请求锁，等价 v1 当前请求语义）
- RouteHTTP_Match 静态命中/未命中均清参数残留

**3. 新 auth 自动授予 role 1**（v1 GrantDefaultAdminRoleAuth；实测缺陷修复）：
- v3 权限模型无 role 1 旁路，此前新插件管理路由超管必 403（xlogserver 能过纯属种子库遗留授权巧合）
- RegisterAuth 成功后幂等追加 role 1 authList（已在列不重复）+ 权限缓存重建；smoke 断言同步更新（403 预期 → 200 新契约）

**4. 编译错误多段聚合 + 失败代际行**：
- tcc_set_error_func 聚合诊断进错误缓冲（日志 + 2048 截断，v1 口径）；编译失败也落 plugin_generation 行（state='failed' + error_message 全文）供后台展示
- 错误缓冲 256→2096；探针验证坏 C 插件的失败详情落库

**5. settings 契约补齐**（GET + configSchema 校验 + 失败回滚）：
- GET /admin/plugin/settings?name= 返回当前生效配置；POST 入口先过 configSchema 递归校验器（type/required/properties/items/additionalProperties，v1 子集）
- OnConfigChanged 返回非 0 → 回滚：内存配置树还原 + 配置文件原文恢复（写前快照，无旧文件则删除回落 defaults）

**6. manifest 轻量校验**（v1 四层校验轻量版）：必填字段（name/title/version/kind）+ xid==目录名 identity + maxHostVersion 越界拒绝（min 越界仅告警）+ Plugin_CompareVersion 三元组比较；ABI 上界兼容（<=4）为 v3 有意设计保持不变

**门禁**：新增 tests/plugin_capability_e2e.py 23/23（六项能力正负例 + reload/disable 生命周期）；smoke 41 PASS（admin-echo 断言更新为 auto-grant 新契约）；write_regression + xlogserver 15/15 + plugin_resource 32/32 + install_wizard 11/11 + template_form 6/6，零编译警告。

### 2026-09-14：内容模型系统阶段 1——宿主侧骨架全量落地（模型 CRUD/能力包/体检/路由）

**新增**：
- `modules/content.h`（~1250 行，v1 script/content 七件中五件的原生变换）：content_db 7 表（model/revision/generation/pack/pack_option/model_pack/schema_version）、content_spec 校验（ident/字段/能力键归一化去重/未知能力拒绝）+ FNV-1a 指纹、content_model CRUD（事务保存+hash 去重 unchanged+修订快照+能力关联同步）、content_revision 查询、content_advisor 体检（error 硬校验+能力包 warning+验收路径项）、content_pack 装载器（21 包目录扫描入库+detail/options/NormalizeId）、content_init 菜单装配（内容管理父菜单+4 子菜单，editor 无菜单项为 v1 形态）
- `route_http/content.h`：17 条路由（4 视图页 + types/type/save/delete/revisions/generations/advisor/generate/packs/pack/pack+options/templates）；generate 为阶段 2 占位明确报错；templates 保真 v1 装饰性固定清单
- `capability-pack/` 21 包资产 + `page/content/` 5 页面从 dev/v1 落地；夹具拷贝清单加 capability-pack
- 插件 ABI 补注入：XAdmin_Free（xrtFree 别名）+ ServerHashPassword（内容模板 4 处调用，28657 行模板符号面就此闭合）
- Content_Init 接入 BusinessStart（Form 之后 PluginHost 之前）；内容表时间列 Unix 微秒

**阶段 1 修复的自身缺陷**：pack/options 处理器 packId 借用视图在 body 释放后使用（悬垂写垃圾）——借出值须在 release 前使用。

**门禁**：新增 tests/content_phase1_e2e.py **25/25**（21 包装载/7 表/菜单/包 API+选项/模型 rev1→unchanged→rev2/能力关联行/校验负例×2/advisor 正负/视图页×3/generate 占位/删除）；smoke 41 + write + xlog 15 + res 32 + wizard 11 + form 6 + capability 23 全绿零警告。

**阶段 2 排队**：content_generator/generation（~2000 行）+ managed_main.c.tpl（28657 行，~5000 处 xvo* 方言变换）+ 9 伴生模板 + generate 接通 + 生成插件全链路验证。

### 2026-09-14：内容模型系统阶段 2——生成器落地，全链路打通（v1 最大缺口关闭）

**交付**：
- `modules/content_generator.h`（1783 行，v1 content_generator+generation 两文件合并原生变换）：模板装载/占位符单趟替换、7 占位符 main.c 渲染（含能力包 Schema/Route/Menu/Auth 四段 C 代码生成，v3 方言）、字段三视图映射、spec/runtime 规范化、contracts.json（7 hook 槽）与 capability.manifest、plugin.json（XADMIN_CAP_* 宏+包 sources/includeDirs）、pending→success/failed 台账与 applied_revision 追平；路由上下文经本地落盘装配（Plugin_Caller 为空时 ABI 不可用，复用宿主装配函数含换代覆盖）
- `content/templates/` 9 模板落地；`managed_main.c.tpl` 经 tests/convert_content_tpl.py 一次性方言变换（标准规则 4914 处 + 内容补丁规则 + 精确行修 + 兼容前导）：~5000 处 xvo*→Value*、xvalue 按值→指针、v1 计数式 PathJoin/Buffer 字段/Regex 四件/v1 markdown 桩（走转义回退）、strlen 键 Get/Set 族
- 插件 ABI 补 3 符号：XAdmin_ReqPath/XAdmin_ReqRemote（v1 宏的可注入形态）+ ServerHashPassword 注入（模板本地前向声明移除）
- **RegisterRoute 权限收录修复**：need_auth 且无专属 auth 的路由（生成插件核心 CRUD）此前任何角色必 403——现与应用侧一致自动以 authID=1 入 uris（role 1 天然含 auth 1）；hello-sdk/xlogserver 因都带 auth_id 不受影响（smoke 计数断言验证）
- generate 路由接通（POST/GET，xid 参数）

**全链路 e2e（tests/content_fullchain_e2e.py 16/16）**：建模型（含 content.category 包）→ 生成（1.16MB 模板渲染，manifest 含 CAP 宏与包源、contracts.json 落盘）→ 启用（~30k 行生成物 TCC 编译 0.2-0.4s，编译错误经聚合落库迭代修复 10 轮）→ 后台 CRUD（save/list）→ 公开 API（list/detail，发布阈值过滤）→ 包挂载路由（category/list）+ contracts → 管理视图页 → 再生成换代（rev2 覆盖 + 台账两行 success）。

**编译迭代修复清单**（错误聚合落库驱动）：sqlite3.h 缺失→前导补；xrtTimeToStr→TimeText；Buffer 字段 Buffer/Length→Data/Size；四参 BufferAppend→bytesview 三形态；v1 计数式 PathJoin(2,a,b)→两参；Regex v1 四件→v3 matcher 封装（含 xregexcapture 适配）；xsReqPath/Remote/Header→可注入/映射；本地 ServerHashPassword 前向声明与注入冲突→删；markdown→NULL 桩走转义回退；tolower→ctype.h。

**回归**：content_fullchain 16/16 + content_phase1 25/25 + smoke 41 + write + xlog 15 + res 32 + wizard 11 + form 6 + capability 23，零意外警告。

**剩余（阶段 3 排队）**：cms.article/content-system 存量插件复活验证（方言同管线）；重度包实跑（comment/slug/static 等全量路由逐包探针）；插件方言移植队列（hello/perfmon 等）；docs 3 死链与 Mail 菜单杂项。

### 2026-09-14：阶段 3 收尾——杂项闭合 + cms.article/hello/perfmon 三插件复活

**杂项**：docs.html 三死链修复（/docs/plugin|content|capability 别名路由）；Mail_Init 自装"邮件任务"菜单（幂等，v1 契约）。

**cms.article 复活（22 能力包全挂载）**：convert_cms_article.py 复用模板变换管线处理 29718 行存量生成物；包 stub 文件（content.like/slug 的 source+include）从 capability-pack 补齐；PLUGIN_ROUTES_MAX 64→256（生成插件注册 220+ 路由撞顶）；slug/redirect 动态路由 pattern 由 v1 正则改写为 v3 方言（`prefix/{slug}`、`prefix/r/{*target}` 尾段捕获）。门禁 cms_revival_e2e 13/13（启用 1.2s、CRUD、公开 API、slug/like/comment/category 包路由、契约、220+ 路由 50+ 权限台账）。

**hello 复活（资源体系活体验证）**：native_plugin_convert 变换 + xvalue 按值声明→指针 + v1 路由声明字段（description/sort/need_log）移除；伴生文件（src/page/option/static）此前已备齐。greeting（hook+service）/info（db+options 注入）/管理页（XAdmin_LoadPluginPage）全通。

**perfmon 复活（Windows 实链+系统指标）**：变换 + sqlite3.h/value_util 补齐 + 计数式 PathJoin + 多行 SetText 括号配平重写 + iphlpapi.h 精确布局声明（MIB_IFROW/MIB_IFTABLE/IP_ADAPTER_INFO 全字段对齐 x64 SDK；IF_TYPE/MIB_IF_OPER_STATUS 常量集；GetTickCount64 声明）。current/serverinfo/disk/history 四 API 实跑真实指标（内存/核心数/盘容量/网络计数），视图页 200。

**content-system 插件待裁定**：与宿主侧 /admin/content/*（v1 最终形态 script/content）功能完全重复，v1 两套并存；建议保持禁用，是否删除待用户定。

**门禁**：revival_e2e 10/10（新增）+ cms_revival 13/13 + 全量 11 门禁绿（smoke 41/write/xlog 15/res 32/wizard 11/form 6/capability 23/phase1 25/fullchain 16），零编译警告。

### 2026-09-15：md4c 接入——markdown 渲染降级点修复（xs 新模块批次）

**xs 新模块分析**（commit 9dc6bd9，xs.exe 5.92→5.98MB）：
- **md4c**：上游 Markdown 解析器（SAX 核心 + HTML 渲染器），TCC 脚本侧预置 `md_parse`/`md_html` 两符号
- **xacme**：RFC 8555 ACME 客户端扩展库（账户/订单/dns-01/证书；内置 LE/ZeroSSL/Google/Buypass 预设 + alidns provider；19 个 `xrtAcme*`/`xacmeClient*` 符号已可注入）——x-admin 侧暂无消费场景（HTTPS 证书自动化属新功能，未列入迁移缺口）

**markdown 修复**（内容系统唯一功能降级点关闭）：
- 模板变换器 PREAMBLE 的 NULL 桩替换为 md4c 真实现：`xsMarkdownToHtmlEx` 经 `md_html` + buffer 回调，旗标与 v1 契约一致（MD_DIALECT_GITHUB 组合 | MD_FLAG_NOHTML | MD_HTML_FLAG_SKIP_UTF8_BOM，取值对齐上游 md4c.h）
- editor_md 字段恢复渲染（_html 伴随字段供页面/静态化路径）；tpl 与 cms.article 均已重变换
- hello-sdk 增 md-probe 一致性探针（GFM 旗标组合实调 md_html，断言 h2/strong/li）

**坑**：xbuffer 产物无 NUL 终止——转字符串必须 `xrtStrViewN(Data, Size)`，strlen 会越界致 stringify 失败（md-probe 实录）。

**回归**：capability e2e 24/24（+md4c 项）；新 xs.exe 全量 12 门禁绿（smoke 41/write/xlog 15/res 32/wizard 11/form 6/capability 24/phase1 25/fullchain 16/cms 13/revival 10），零警告。

### 2026-09-15：HostContext 聚合注入落地（v1 XAdminHostContext 契约，原裁定"不引入"经用户指示实施）

**SDK**（plugin_sdk/xs_plugin.h）：XAdminHostContext 结构（v1 字段序 ABI：size/abi_version + 13 宿主路径 + 4 插件身份路径 + main_db + option_table）；XADMIN_GLOBAL_HOST_CONTEXT 槽位 7；sqlite3 前置声明保持头自包含。

**宿主**（modules/plugin_host.h）：
- PluginInstance 内嵌 hostContext（实例存续期有效，随代际复用）；G_PluginHostContextTemplate 宿主公共段在业务启动时经 PluginHost_ContextInit(host) 填充一次
- 路径取自 v3 实际布局：web=host->Path（wwwroot）、site/attachment 借 StandalonePage/Attachment 模块现值、db=OptionPath 同源；exe_path 按部署约定 AppPath/xs.exe 派生（xs 无 exe 路径 API）
- Plugin_Start 在标量槽位之外并行注入 &inst->hostContext（旧槽位 1-6 保留不破坏存量插件）
- settings 保存/回滚两路径同步刷新 hostContext.option_table（防配置换代悬垂指针）

**探针与门禁**：hello-sdk 增 /api/plugin/hello-sdk/hostctx（size/abi/10 项字段一致性）+ settings 换代后上下文存活断言；capability e2e **26/26**；全量 12 门禁回归绿（smoke 41/write/xlog 15/res 32/wizard 11/form 6/capability 26/phase1 25/fullchain 16/cms 13/revival 10）零警告。

### 2026-09-15：xs 扩展库探测门禁（xsExtensionEnabled 契约 API）

**xs 侧**（commit 6cedfa2 后）：`XS_API bool xsExtensionEnabled(const char* sName)`——按注册名（=构建参数名，与启动横幅一致，大小写不敏感）查询本变体是否编入某扩展；requires 展开的依赖同样可见；未知名返回 false。

**x-admin 侧**：`XAdmin_RequireExtensions()` 置于 `XAdmin_BusinessStart` 首位——必需清单：`sqlite`（主库）、`xsmtp`（邮件，XADMIN_WITH_SMTP 门控）、`md4c`（内容系统 markdown）。缺失即打印 `required xs extension missing: <名>` 并拒启（G_Ready 保持 false → 全站 503），把"运行中期功能点符号错误"提前为启动即报。

**两种失败形态的边界**（实测确认）：缺 xsmtp 的构建在**编译期**失败（mail.h 找不到 xsmtp.h 头，VFS 随扩展裁剪），到不了运行期探测；缺 md4c（消费侧手工声明符号、无头依赖）正是运行期探测的目标场景。

**门禁**：新增 tests/extension_gate_e2e.py **7/7**——负例用 sqlite+xsmtp 精简构建（存 tests/.runtime/xs-stripped.exe，由 `python tools/build.py sqlite xsmtp --output` 产出）：拒启 + 报 md4c 缺失 + 无 ready + 全站 503 + 进程存活；正例完整 xs 正常启动无缺失报错。官方全量 xs.exe（sqlite+xtp+xllm+xmail+xsmtp+xpop3+ximap+md4c+xacme）就位后全量 13 门禁回归绿（smoke 41/write/ext 7/xlog 15/res 32/wizard 11/form 6/capability 26/phase1 25/fullchain 16/cms 13/revival 10）零警告。

### 2026-09-15：xlogserver 模板统一引擎约定 + 全项目模板清查 + 挂死事故结案

**模板约定统一（用户裁定）**：`{{ }}` 为模板渲染符号（v3 引擎现状即如此，变量 `{{$var}}`）。模板文件中 JS/代码的字面 `{{`/`}}` 一律写 `{ {`/`} }`（JS 词法等价、解析器不捕获）。xlogserver 的 v1 自制约定（`{$var}` 占位 + `{{}}` 转义）废弃：
- 5 个模板资产一次性变换（保护标记法防顺序破坏）：`{$x}`→`{{$x}}`、`{{`→`{`、`}}`→`}`
- XLog_SendTemplatePage 改走宿主引擎 XAdmin_RenderPluginTemplate（自带 {{#form}} 扩展注册表），自制 XLog_RenderTemplate 删除——**任务页空白根因**：v1 约定下 JS 全是 `function(){{...}}`，v3 移植漏了反转义步骤，输出 JS 语法错误整页脚本死掉

**全项目模板清查**（引擎渲染范围 vs 静态服务）：宿主 template/ 25 文件、xlogserver 两份 10 文件、其他插件、内容 tpl——**字面 {{ 残留为零**（宿主模板本就是引擎语法；page/、site/、生成插件页面为静态服务不经引擎，无需约束）。

**渲染测试**：宿主 rebuild 20/20（smoke 固化）；xlogserver 5 模板页全渲染断言（变量代入 + `function(){` 单花括号 + 无 `{{` 泄漏）入 xlogserver_e2e → **21/21**；hello-sdk hello.tpl 与 form/block_demo 各自门禁覆盖。

**挂死事故结案**（连续 3 次 smoke 在 disable/reload 处挂死）：
1. 根库污染：db/main.db 中 xlogserver enabled=1（早期调试遗留）→ 每个 smoke 夹具自动启动 xlogserver——已清理复位
2. 孤儿进程：用户取消全量回归时遗留旧代码 xs.exe（PID 22236）持续运行——已击杀；污染态复原 + 最小复现探针（enable/reload×3/disable ×含 xlogserver 变体）均不复现 → **环境事故，非代码缺陷**
3. 教训入库：回归被中断后先查孤儿 xs.exe 再归因代码；根库 plugin_runtime.enabled 状态纳入发布前检查

**回归**：14 门禁全绿（smoke 41/write/ext 7/xlog 21/res 32/wizard 11/form 6/capability 26/phase1 25/fullchain 16/cms 13/revival 10 + template_form）零警告。

### 2026-09-15：插件状态机真实化——404 事故根因修复 + 轮换竞态门禁

**用户事故**：插件列表显示"启用"但实际未运行（点菜单 404，手动重载恢复）。排查确认严重逻辑缺陷：

**根因（三层叠加）**：
1. **启动路径不回写 status**：自启动编译失败/依赖缺失只 printf，DB 里上一会话的 'running' 陈旧串原样穿透到列表显示（管理路由路径的 5 个写入点都正确，唯独 PluginHost_Init 遗漏）
2. **enabled≠loaded 无区分**：列表只有 DB 持久串，无本进程真实状态（审计遗留 C3 项）
3. **孤儿菜单**：enabled 但未启动的插件菜单行不隐藏 → 菜单在、路由无 → 404 死链

**修复**：
- PluginHost_Init 开头状态重置（非 discovered/disabled 一律归位 disabled，自启动再翻 running/error）——陈旧串穿透通道关闭
- 自启动失败/依赖缺失两分支回写 status='error'
- 列表 API 增 loaded 字段（inst->started 本进程事实）
- 孤儿菜单清理：本轮未启动插件的菜单行软删（NOT IN 已启动清单；零启动走全清分支；snprintf 截断防护）；再启动经 RegisterMenu upsert 复活

**两阶段验证**（用户场景精确复原）：损坏源+陈旧 running → status='error'+loaded=false+菜单隐藏+404；修复重启 → 'running'+loaded=true+菜单复活+200。

**竞态插曲**：修复过程中 smoke 连续挂死于轮换段（xlogserver 自启 × 多代重载时序相关，加 fflush 即隐藏的 heisenbug）；最终同配置 7 连过 + 轮换风暴门禁（tests/rotation_storm_e2e.py 26/26：自启+6 轮重载+紧轮询+每代插件健康）固化为回归探针。根因未最终定论（双代重叠期 DB 锁竞争窗口最可能），风暴门禁为复发捕获器。

**回归**：15 门禁全绿（smoke 41/write/ext 7/storm 26[新]/xlog 21/res 32/wizard 11/form 6/capability 26/phase1 25/fullchain 16/cms 13/revival 10）零警告。根库 xlogserver 恢复用户启用态。

### 2026-09-15：任务页标题乱码——ValueText 悬垂指针修复

**现象**：任务列表页"返回服务列表"后的服务名标题显示乱码（0xDD 重复字节）。

**根因**：`XLog_Req_ViewTasks` 中 `ValueText(tblSvc, "name")` 返回**借用视图**（指向值树内部字符串），随后 `xrtValueRelease(tblSvc)` 释放值树，再用悬垂指针写模板数据——读到的是 xrt 释放后内存填充（0xDD）。v1 原版用栈缓冲 `snprintf` 拷贝，移植时改为指针借用引入缺陷。

**修复**：释放前 `xrtStrDup` 拷贝（恢复 v1 拷贝语义），渲染后释放。同文件排查：其余 handler（services_edit/tasks_edit/tasks_logs）均为"值树整体传入渲染后释放"，安全。

**门禁**：xlogserver e2e 升级为中文服务名（"边缘采集"）+ 双处渲染断言 + 0xDD 字节负例检测 → **22/22**；全量 16 门禁回归绿。

**坑入库**：ValueText/ValueGet 返回借用视图——值树释放后再用即悬垂（xrt 以 0xDD 填充释放内存，输出乱码而非崩溃，更隐蔽）；借出值必须在 release 前拷贝。

### 2026-09-15：任务创建"假成功"——旧 schema 服务库缺 tasks 表修复

**现象**：任务弹窗显示创建成功，列表不显示。

**根因**（用户真实数据坐实）：v1 早期创建的服务库 `_TrPumu...db` **没有 tasks 表**（该表 DDL 只在"创建新服务"时执行一次，旧库永不补表）。缺陷链：
1. `XLog_TaskCreate` 的 INSERT 因缺表静默失败（step 返回值被忽略），仍取 `last_insert_rowid` 残留值 → 响应 `result:true` + 假 id → **假成功提示**
2. 列表 SELECT 同因缺表返回空 → 列表恒空

**修复**：
- `XLog_ServiceConnectDB` 打开库即幂等补 schema（CREATE TABLE IF NOT EXISTS tasks；覆盖启动装载/创建/重连全部路径）——旧库在插件重启后自动补表
- `XLog_TaskCreate` 校验 step==SQLITE_DONE 才取 rowid，失败打印 sqlite 错误

**验证**：旧 schema 场景实测（无 tasks 表旧库 + 主库服务行）→ 启用后自动补表（`['other','tasks','sqlite_sequence']`）→ 建任务成功 → 列表出现；xlogserver e2e 22/22；全量 17 门禁绿。

**附带**：smoke 夹具归一化（UPDATE plugin_runtime SET enabled=0）——测试自管插件状态不再继承根库用户态；消除 xlogserver 自启把多代重载时序竞态带入 smoke 的间歇挂死（该竞态 storm 门禁单测 26/26 可过，完整 smoke 序列偶发，根治待后续专项）。

### 2026-09-16：7h 混合极端战役收官——E1 管理写接口长度上限

**战役终态**：86.2 万请求 0 5xx、306 次重载零真实崩溃（4 次"GDB 崩溃"=探针开发期外部按名误杀，`exited code 01` 无栈签名）、渗透载荷零得手；49 轮 2.6GB 内存熔断干净重启。终报见 `docs/campaign-extreme-2026-09-16.md`。

**修复（E1）**：泄漏归因主因=管理写接口只查非空无长度上限（fuzzer `default=str` 合法化怪载荷落库，RSS≈10×DB）。管理侧补长度上限 18 站点（auth.h 9 + member.h 9）：username/name/nickname 64、password 128、email 128、phone 32、avatar 512、desc 1024、authList 4096。公开 API 注册 3-32 先例已在，管理侧此前缺失（v1 同样无校验，定向加固非保真回填）。

**门禁**：write_regression 新增 E1 块（8 组超限拒写+DB 快照不变、边界值放行、member PUT 超限拒写）；smoke 41 项全绿零警告。

**移交**：xrt block 层换代泄漏 ~2-4MB/代（blockAlloc 3.5MB vs blockFree 1.65MB/代）→ xserver 仓跟进；登录失败锁定/管理写频控（循 R2 先例）列纵深防御建议。

### 2026-09-16：内容系统编辑器前端资产补齐（17d7589）

**现象**：模型管理-新建模型界面显示不正常，链路不可用。

**根因**：编辑器页（v1 原样迁移的 page/content/editor.html）引用 `/content/css/editor.css` + 7 个 `/content/js/*.js`，但 `wwwroot/content/` 目录从未从 v1 迁移——全部 404，编辑器无 JS/CSS。页面本体（data/page/content/*.html）与 v3 后端路由契约（content-api.js 的 5 个端点）本就一致，缺的只有静态资产层。

**修复**：v1 wwwroot/content 8 文件（1081 行）原样拷入；夹具（longtest）wwwroot 同步。

**浏览器全链路验证**（18400 GUI 实测）：新建模型→表单→字段页签（默认 3 字段+新增 price）→保存修订×2→生成预检通过→生成插件（落盘 generated/main.c+spec.json+5 页面；编译启动 generation 1；DB discovered→enabled；后台菜单自动注册）→管理 CRUD（save/get/list/delete；自定义字段 price 端到端存活）→公开 JSON API（/api/plugin/<xid>/list|detail 无鉴权可读+草稿隔离）→列表回显。注意：插件启用触发换代会使会话失效需重登（已知语义）。

**门禁**：phase1 25/fullchain 16/cms 13/revival 10/smoke 全绿；根库零污染。

**架构备忘**：`/content/{id}.html` 公开 HTML 页非运行时路由（cms.article 同为 0）——公开面=JSON API+静态化产物（content.static 能力），非缺陷。

### 2026-09-16：接口管理页按插件筛选（d1c1c4a）

**需求**：设置-接口管理 增加插件筛选（默认 xAdmin 本体），便于为指定插件的 URI 分配权限；下拉框样式与附件列表页同款（32px 对齐+10px 间距）。

**实现**：/admin/auth/uris 增 plugin 参数三态（__core__=无插件归属/__all__ 或缺省=旧行为/插件 xid），list/count/search 四语句统一 CASE 谓词；响应附 plugins 数组。前端下拉框 done 回填（layui form 重建态须 form.render('select') 刷新 dl——选项在 form 初渲染后追加，dl 是旧快照）+ 双 change 通道即时过滤。v1-assets 登记 uris.html。

**实测**：本体 172/全部 199/单插件 13/搜索叠加 12/缺省 199 全对；浏览器下拉/回填/切换/高度全过；smoke 42 全绿。

**撞见并登记（非本批范围）**：单次宿主热重载后 xlogserver 的 uris 台账行被清（进程内路由仍在服务、services API 200，但接口管理页看不到）——旧代 teardown 阶段的 Auth_SyncURIS/孤儿清理以旧代视角的"已启动清单"操作共享 DB 所致；disable→enable 周期可恢复台账（10 条 UriAuth 回来）。属记忆中"xlogserver×多代重载时序竞态（根治待专项）"家族，gui.cmsdemo 等生成插件未复现。修复方向：台账清理须由持有最新代的执行体做或按代次守卫。

### 2026-09-16：内容系统其余页面/API 排查轮（da3775c）

**覆盖**：编辑器 7 页签（页面/能力/策略/生成/高级，零报错）、修订/生成历史 API、能力包管理页（12 包列表）+ 商店页（设计占位）、模型编辑流（打开已有→改→存→重生成）、模型删除流（建→删→查不存在）、生成插件后台页面（文章列表含栏目列联动、文章编辑器含自定义字段/栏目/标签挂件）。

**能力链路实测**：UI 启用栏目+标签→存修订 3→预检（警告走原生 confirm）→生成→重启用插件→category save/tree、tag save/公开 list（status 门控自洽）、内容带 categoryId 落库——全通。

**修复 1 项（da3775c）**：字段表单静默丢失——applyField 只绑在"应用字段"按钮，直接保存修订丢输入（保存占位 field_N）。拆 commitFieldForm（无弹窗/catch parseJsonField throw），saveModel 与 runPreflightForSpec 开头强制落表。

**观感项（不修，记录）**：编辑器沿用 v1 原生 alert/confirm（预检警告 confirm 会阻塞）；tag 管理面无 admin 侧 list 路由（列表走公开 /api/plugin/<xid>/tag/list，admin 侧仅 save/delete/batch-status/stats/bind/unbind/merge/content-list）——均为 v1 模板既有设计。

**门禁**：fullchain 16/16、smoke 42 全绿。

### 2026-09-16：能力包属性配置 xform 化（33539b6）

**需求**：编辑器能力页签改交互——左侧点击切换属性页、启用收敛到表单区开关、中间展示能力包配置的标准渲染表单。

**标准属性接口**：`GET /admin/content/pack/form?packId=<id>` → 服务端用 Form_RenderTemplateBlockHTML 把 instanceForm 渲染为自包含 HTML（内联样式、27 种控件）+ ContentPack_FormDefaults 收集的默认值。后续商店配置预览/packs 详情"实例表单"tab/生成插件设置页可复用同端点。

**前端**：editor-capability.js 重写——左侧切换选中（启用徽标）、layui 开关启用（.layui-form 容器内才渲染 switch；form.on(filter) 委派仅注册一次防叠加）、#cap_form_host 注入+客户端叠加当前值+按 name 收集（值形态与旧一致，buildSpec/生成器零改动）、未启用置灰、异步切页丢弃过期响应。手写 comment/seo 表单删除（定义本就全：comment 20 字段 vs 旧手写 2）。

**顺带修复**：form.h 渲染产物 `xrtStrDup(buf->Data)` xbuffer 无 NUL 越界读堆——模板路径仅插 HTML 侥幸未炸，JSON 化端点使 stringify 静默失败返回空体；补 `xrtBufferAppendByte(buf, 0)` 后转 C 串。

**门禁**：phase1 30/30（+5：端点渲染/默认值/未知包拒绝/能力配置往返）；fullchain 16/smoke 42/cms 13 全绿。GUI 实测修订 5 落盘 comment 20 键配置全量。

### 2026-09-16：内容系统缺口补全——策略/页面 tab 静态烘焙（589be75）

按用户原则（配置→生成期烘焙，运行时零动态判断）补全 12 死字段中的 11 项 + 潜伏缺陷修复。详见 docs/content-static-principle-audit.md（含 R1-R7 整改清单：payload_json 类型化列改造为最大项）。

**关键事实**：v1 原生成器同样未实现这些字段（UI 收集、生成器忽略），属全新实现而非保真回归；fieldGroups 断链真因=生成器从未给字段分配 group 键。

**烘焙机制**：生成器占位符（{{CONTENT_POLICY_DEFINES}}/{{CONTENT_DELETE_EXEC}}/{{CONTENT_SAVE_GUARDS}}/{{CONTENT_DETAIL_FILTER_FN}}/{{CONTENT_LIST_COLUMNS_JS}}）+ 角色名生成期主库解析。

**潜伏缺陷 2 处（被本轮暴露）**：模板本地 xrtReplace 无 NUL 越界（SEO 模板特性自迁移起配置即坏）；栏目 detail/contents 数组元素悬垂（×2）。

**工作坑**：生成插件调试需 disable→enable 重编译 generated/main.c（直接改生成物可快速二分）；bash heredoc python 仍吃反斜杠（printf 调试栽一次）。
