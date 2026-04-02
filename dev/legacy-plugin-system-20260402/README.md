# Legacy Plugin System Backup

备份时间：2026-04-02

本目录用于保存重构前的旧插件系统实现，供后续对照和回溯使用。

备份来源：

- `host/xadmin/script/module/plugin_ctx.h`
- `host/xadmin/script/module/plugin_mgr.h`
- `host/xadmin/script/module/plugin_page.h`

说明：

- 这是纯备份目录，不再作为新插件系统的实现基础。
- 新插件系统不要求兼容旧的 `PluginContext / Plugin_SetGlobalData / Plugin_{name}_Init / Unit` 体系。
- 后续重构请以 `docs/插件系统重构SPEC.md` 为准。
