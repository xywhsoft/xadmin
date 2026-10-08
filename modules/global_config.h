/* 内建全局配置的定义、默认值与服务器校验共用一份规格。 */
typedef struct GlobalSettingSpec {
	const char* name;
	const char* group;
	const char* title;
	const char* desc;
	int fallback, minimum, maximum;
	bool boolean;
} GlobalSettingSpec;
static const GlobalSettingSpec G_GlobalSettings[] = {
	{"admin_session_timeout_minutes", "后台登录", "后台登录保持时间（分钟）", "未勾选记住登录时的空闲期限；有效操作续期，保存后立即调整现有普通会话。", 120, 1, 43200, false},
	{"admin_remember_days", "后台登录", "记住登录保留时间（天）", "勾选记住登录后，Cookie 和服务器会话均从登录时起保留指定天数，操作不会延长到期日；仅用于后续登录。服务重启或重载仍需重新登录。", 7, 1, 30, false},
	{"admin_session_limit", "后台登录", "同账号最多登录数量", "超出上限时退出最久未活跃的后台会话；降低上限后立即执行。", 5, 1, 100, false},
	{"admin_login_failure_limit", "后台登录保护", "登录失败锁定次数", "同一 IP 的后台登录失败达到此次数后进入冷却；登录成功清零。", 5, 1, 100, false},
	{"admin_login_cooldown_seconds", "后台登录保护", "登录失败初始冷却（秒）", "首次锁定的冷却时间；连续再次触发时按触发次数递增。新锁定使用当前配置。", 300, 1, 86400, false},
	{"verification_code_ttl_seconds", "短信与邮箱验证码", "验证码有效期（秒）", "新短信、邮箱验证码的有效期，同时用于发送内容和接口返回。", 300, 30, 3600, false},
	{"verification_send_interval_seconds", "短信与邮箱验证码", "同一目标发送间隔（秒）", "同一手机号或邮箱在此间隔内最多发送一次；已有频率窗口到期后使用新间隔。", 60, 1, 3600, false},
	{"verification_ip_limit_per_hour", "短信与邮箱验证码", "每 IP 每小时发送次数", "限制同一 IP 每小时申请短信、邮箱验证码的次数。", 20, 1, 10000, false},
	{"admin_log_retention_days", "后台日志", "操作日志保留天数", "手动清理和每日自动清理共用，按日志时间删除超过保留期限的后台操作日志。", 7, 1, 3650, false},
	{"admin_log_auto_cleanup", "后台日志", "每天自动清理操作日志", "开启后自动维护系统计划任务；关闭后停止自动清理，仍可手动清理。", 0, 0, 1, true},
	{"admin_log_cleanup_hour", "后台日志", "每日清理时间（小时）", "按服务器本地时区每天此小时整点执行；范围 0–23。停机错过后启动补跑一次，执行记录可在计划任务中查看。", 3, 0, 23, false}
};
#define GLOBAL_SETTING_COUNT (sizeof(G_GlobalSettings) / sizeof(G_GlobalSettings[0]))
static const GlobalSettingSpec* Global_Setting(const char* name)
{
	for (size_t i = 0; i < GLOBAL_SETTING_COUNT; i++)
		if (!strcmp(G_GlobalSettings[i].name, name)) return &G_GlobalSettings[i];
	return NULL;
}
static bool Global_ValidSetting(const GlobalSettingSpec* spec, xvalue* value)
{
	int64 n = 0;
	return spec->boolean ? xrtValueType(value) == XVALUE_BOOL :
		xrtValueType(value) == XVALUE_INT && xrtValueGetInt(value, &n) && n >= spec->minimum && n <= spec->maximum;
}
static int Global_Int(const char* name)
{
	const GlobalSettingSpec* spec = Global_Setting(name);
	xvalue* value = ValueGet(ValueGet(G_Option, "global"), name);
	return spec && Global_ValidSetting(spec, value) ? (spec->boolean ? ValueBoolOf(value) : (int)ValueIntOf(value)) : spec ? spec->fallback : 0;
}
