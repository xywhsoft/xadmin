/* 能力页签：左侧点击切换属性页，中间展示该能力包的 xform 渲染表单；
 * 启用动作收敛到表单区的「启用此能力包」开关。表单 HTML 由
 * /admin/content/pack/form 服务端渲染（标准属性接口），客户端叠加
 * 当前配置值并按 name 收集回 contentState.capabilityConfig。 */

var capSwitchBound = false;

function packKey(pack) {
	return pack && (pack.packId || pack.key || pack.name) ? (pack.packId || pack.key || pack.name) : '';
}

function selectedPack() {
	var key = contentState.selectedCapability;
	for (var i = 0; i < (contentState.capabilities || []).length; i++) {
		if (packKey(contentState.capabilities[i]) === key) return contentState.capabilities[i];
	}
	return null;
}

function renderCapabilities() {
	var side = $('side_capabilities');
	var packs = contentState.capabilities || [];

	if (!packs.length) {
		side.innerHTML = '<div class="side-item"><div class="side-item-meta">暂无能力包</div></div>';
		$('capability_workspace').innerHTML = '<div class="status-text">暂无可用能力包。</div>';
		return;
	}
	if (!contentState.selectedCapability || !selectedPack()) {
		contentState.selectedCapability = packKey(packs[0]);
	}

	side.innerHTML = packs.map(function(pack) {
		var key = packKey(pack);
		var enabled = !!contentState.enabledCapabilities[key];
		var active = key === contentState.selectedCapability;
		return '<div class="side-item ' + (active ? 'active' : '') + '" data-cap="' + esc(key) + '">'
			+ '<div class="side-item-title">' + esc(pack.title || key) + '</div>'
			+ '<div class="side-item-meta">' + esc(key) + (enabled ? ' <span class="layui-badge layui-bg-green">已启用</span>' : '') + '</div>'
			+ '</div>';
	}).join('');

	Array.prototype.forEach.call(side.querySelectorAll('.side-item'), function(item) {
		item.onclick = function() { selectCapability(item.getAttribute('data-cap')); };
	});

	bindCapabilitySwitchOnce();
	renderCapabilityWorkspace();
}

/* 启用开关事件只在页面生命周期注册一次（layui form.on 按 filter 委派，
 * 重复注册会叠加），回调里实时读当前选中能力。 */
function bindCapabilitySwitchOnce() {
	if (capSwitchBound) return;
	capSwitchBound = true;
	layui.use('form', function() {
		layui.form.on('switch(cap-enable-switch)', function(data) {
			var key = contentState.selectedCapability;
			if (!key) return;
			togglePack(key, data.elem.checked);
			var host = $('cap_form_host');
			if (host) host.className = data.elem.checked ? '' : 'cap-form-disabled';
		});
	});
}

function selectCapability(key) {
	contentState.selectedCapability = key;
	Array.prototype.forEach.call(document.querySelectorAll('#side_capabilities .side-item'), function(item) {
		item.className = 'side-item' + (item.getAttribute('data-cap') === key ? ' active' : '');
	});
	renderCapabilityWorkspace();
}

function togglePack(key, enabled) {
	contentState.enabledCapabilities[key] = enabled;
	if (enabled && !contentState.capabilityConfig[key]) {
		contentState.capabilityConfig[key] = capabilityDefaults(key);
	}
	var meta = document.querySelector('#side_capabilities .side-item[data-cap="' + key + '"] .side-item-meta');
	if (meta) {
		meta.innerHTML = esc(key) + (enabled ? ' <span class="layui-badge layui-bg-green">已启用</span>' : '');
	}
	updateImpact();
}

function renderCapabilityWorkspace() {
	var main = $('capability_workspace');
	var pack = selectedPack();
	if (!pack) { main.innerHTML = '<div class="status-text">请选择能力包。</div>'; return; }
	var key = packKey(pack);
	var enabled = !!contentState.enabledCapabilities[key];
	var effects = readPackEffects(pack.effectsJson);
	main.innerHTML = '<div class="cap-head">'
		+ '<div class="cap-title">' + esc(pack.title || key) + '</div>'
		+ '<div class="cap-meta">' + esc(key) + ' | ' + esc(pack.version || 'v0.0.0') + ' | ' + esc(pack.installType || 'local') + '</div>'
		+ (pack.description ? '<div class="cap-meta">' + esc(pack.description) + '</div>' : '')
		+ '<div class="cap-meta">生成影响：' + esc(effects || '未声明') + '</div>'
		+ '</div>'
		+ '<div class="layui-form cap-enable-row">'
		+ '<input type="checkbox" lay-skin="switch" lay-text="启用|停用" lay-filter="cap-enable-switch"' + (enabled ? ' checked' : '') + '>'
		+ '<span class="cap-enable-label">启用此能力包</span>'
		+ '</div>'
		+ '<div id="cap_form_host"' + (enabled ? '' : ' class="cap-form-disabled"') + '><div class="cap-form-empty">正在加载属性表单...</div></div>';
	if (layuiForm) layuiForm.render('checkbox');
	loadCapabilityForm(key);
}

function loadCapabilityForm(key) {
	var host = $('cap_form_host');
	if (!host) return;
	ContentApi.request('/admin/content/pack/form?packId=' + encodeURIComponent(key))
		.then(function(ret) {
			/* 异步回来时已切换到其他能力——丢弃过期响应 */
			if (contentState.selectedCapability !== key || !host.isConnected) return;
			if (!ret || !ret.result) {
				host.innerHTML = '<div class="cap-form-empty">' + esc(ret && ret.message || '加载属性表单失败') + '</div>';
				return;
			}
			var data = ret.data || {};
			host.innerHTML = data.html || '<div class="cap-form-empty">该能力包没有可配置属性。</div>';
			applyCapabilityValues(host, contentState.capabilityConfig[key] || {});
			host.onchange = function() { collectCapabilityConfig(host, key); };
		})
		.catch(function() {
			if (contentState.selectedCapability !== key || !host.isConnected) return;
			host.innerHTML = '<div class="cap-form-empty">加载属性表单失败</div>';
		});
}

/* 把当前配置叠加进渲染产物（值形态与收集端约定一致） */
function applyCapabilityValues(host, cfg) {
	Array.prototype.forEach.call(host.querySelectorAll('[name]'), function(el) {
		var name = el.getAttribute('name');
		if (!name) return;
		var base = name;
		var side = -1;
		if (name.length > 6 && name.slice(-6) === '_start') { base = name.slice(0, -6); side = 0; }
		else if (name.length > 4 && name.slice(-4) === '_end') { base = name.slice(0, -4); side = 1; }
		var value = cfg[base];
		if (side >= 0) {
			value = (value && typeof value === 'object' && typeof value.length === 'number') ? value[side] : undefined;
			if (value === undefined || value === null) return;
			el.value = value;
			return;
		}
		if (value === undefined || value === null) return;
		if (el.type === 'checkbox') el.checked = !!value;
		else if (el.type === 'radio') el.checked = (String(value) === el.value);
		else el.value = value;
	});
}

/* 从渲染产物收集全部字段值（checkbox→布尔，range 双输入→数组，其余→字符串；
 * 与旧迷你渲染器的值形态保持一致，生成器消费端无需变化） */
function collectCapabilityConfig(host, key) {
	var cfg = {};
	var seen = {};
	Array.prototype.forEach.call(host.querySelectorAll('[name]'), function(el) {
		var name = el.getAttribute('name');
		if (!name) return;
		var base = name;
		var side = -1;
		if (name.length > 6 && name.slice(-6) === '_start') { base = name.slice(0, -6); side = 0; }
		else if (name.length > 4 && name.slice(-4) === '_end') { base = name.slice(0, -4); side = 1; }
		if (side >= 0) {
			if (!cfg[base] || !Array.isArray(cfg[base])) cfg[base] = [];
			cfg[base][side] = el.value;
			seen[base] = 1;
			return;
		}
		if (el.type === 'radio') {
			if (el.checked) { cfg[base] = el.value; seen[base] = 1; }
			return;
		}
		if (el.type === 'checkbox') {
			var group = host.querySelectorAll('[name="' + name + '"]');
			if (group.length > 1) {
				if (!cfg[base] || !Array.isArray(cfg[base])) cfg[base] = [];
				if (el.checked && cfg[base].indexOf(el.value) < 0) cfg[base].push(el.value);
			} else {
				cfg[base] = el.checked;
			}
			seen[base] = 1;
			return;
		}
		cfg[base] = el.value;
		seen[base] = 1;
	});
	if (Object.keys(seen).length) {
		contentState.capabilityConfig[key] = cfg;
		updateImpact();
	}
}

function capabilityDefaults(key) {
	for (var i = 0; i < (contentState.capabilities || []).length; i++) {
		var pack = contentState.capabilities[i];
		if (packKey(pack) !== key) continue;
		return readPackFormDefaults(pack.instanceFormJson);
	}
	return {};
}

function readPackFormDefaults(text) {
	var defaults = {};
	try {
		var form = JSON.parse(text || '{}');
		var fields = [];
		if (Array.isArray(form.fields)) fields = fields.concat(form.fields);
		if (Array.isArray(form.groups)) {
			form.groups.forEach(function(group) {
				if (group && Array.isArray(group.fields)) fields = fields.concat(group.fields);
			});
		}
		fields.forEach(function(field) {
			if (!field || !field.name) return;
			if (field.defaultValue !== undefined) defaults[field.name] = field.defaultValue;
			else if (field.value !== undefined) defaults[field.name] = field.value;
		});
	} catch (err) {
		return {};
	}
	return defaults;
}

function readPackEffects(text) {
	try {
		var value = JSON.parse(text || '{}');
		var parts = [];
		if (Array.isArray(value.adminPages) && value.adminPages.length) parts.push('后台页面 ' + value.adminPages.length);
		if (Array.isArray(value.publicApis) && value.publicApis.length) parts.push('前端接口 ' + value.publicApis.length);
		if (Array.isArray(value.tables) && value.tables.length) parts.push('数据表 ' + value.tables.length);
		if (Array.isArray(value.files) && value.files.length) parts.push('生成文件 ' + value.files.length);
		return parts.join(' / ');
	} catch (err) {
		return '';
	}
}

function contentLoadCapabilities() {
	ContentApi.getPacks().then(function(ret) {
		if (!ret || !ret.result) {
			contentState.capabilities = [];
			renderCapabilities();
			$('content_state').innerText = ret && ret.message ? ret.message : '能力包加载失败';
			return;
		}
		contentState.capabilities = ret && ret.data ? ret.data : [];
		renderCapabilities();
		updateImpact();
	});
}
