function packKey(pack) {
	return pack && (pack.packId || pack.key || pack.name) ? (pack.packId || pack.key || pack.name) : '';
}

function renderCapabilities() {
	var side = $('side_capabilities');
	var main = $('capability_workspace');
	var packs = contentState.capabilities || [];

	if (!packs.length) {
		side.innerHTML = '<div class="side-item"><div class="side-item-meta">暂无能力包</div></div>';
		main.innerHTML = '<div class="status-text">暂无可用能力包。</div>';
		return;
	}

	side.innerHTML = packs.map(function(pack) {
		var key = packKey(pack);
		var enabled = !!contentState.enabledCapabilities[key];
		return '<div class="side-item ' + (enabled ? 'active' : '') + '" data-cap="' + esc(key) + '">'
			+ '<div class="side-item-title">' + esc(pack.title || key) + '</div>'
			+ '<div class="side-item-meta">' + esc(key) + ' | ' + (enabled ? '已启用' : '未启用') + '</div>'
			+ '</div>';
	}).join('');

	main.innerHTML = packs.map(function(pack) {
		var key = packKey(pack);
		var enabled = !!contentState.enabledCapabilities[key];
		var checked = enabled ? 'checked' : '';
		var cfg = contentState.capabilityConfig[key] || {};
		var effects = readPackEffects(pack.effectsJson);
		var configHtml = renderPackConfig(key, cfg, enabled);
		return '<div class="cap-item">'
			+ '<div>'
			+ '<div class="cap-title">' + esc(pack.title || key) + '</div>'
			+ '<div class="cap-meta">' + esc(key) + ' | ' + esc(pack.version || 'v0.0.0') + ' | ' + esc(pack.installType || 'local') + '</div>'
			+ '<div class="cap-meta">' + esc(pack.description || '用于增强生成内容插件的能力包。') + '</div>'
			+ '<div class="cap-meta">生成影响：' + esc(effects || '未声明') + '</div>'
			+ configHtml
			+ '</div>'
			+ '<input type="checkbox" class="cap-check" data-key="' + esc(key) + '" ' + checked + '>'
			+ '</div>';
	}).join('');

	Array.prototype.forEach.call(side.querySelectorAll('.side-item'), function(item) {
		item.onclick = function() {
			var key = item.getAttribute('data-cap');
			togglePack(key, !contentState.enabledCapabilities[key]);
		};
	});

	Array.prototype.forEach.call(main.querySelectorAll('.cap-check'), function(chk) {
		chk.onchange = function() {
			togglePack(chk.getAttribute('data-key'), chk.checked);
		};
	});

	Array.prototype.forEach.call(main.querySelectorAll('.cap-config-input'), function(input) {
		input.onchange = function() {
			var key = input.getAttribute('data-key');
			var name = input.getAttribute('data-name');
			if (!contentState.capabilityConfig[key]) contentState.capabilityConfig[key] = {};
			contentState.capabilityConfig[key][name] = input.type === 'checkbox' ? input.checked : input.value;
			updateImpact();
		};
	});
}

function togglePack(key, enabled) {
	contentState.enabledCapabilities[key] = enabled;
	if (enabled && !contentState.capabilityConfig[key]) {
		contentState.capabilityConfig[key] = capabilityDefaults(key);
	}
	renderCapabilities();
	updateImpact();
}

function renderPackConfig(key, cfg, enabled) {
	if (!enabled) return '';

	if (key === 'content.comment') {
		return '<div class="cap-config">'
			+ '<label>审核方式</label><select class="cap-config-input" data-key="' + esc(key) + '" data-name="moderation">'
			+ '<option value="manual" ' + ((cfg.moderation || 'manual') === 'manual' ? 'selected' : '') + '>人工审核</option>'
			+ '<option value="auto" ' + (cfg.moderation === 'auto' ? 'selected' : '') + '>自动通过</option>'
			+ '</select>'
			+ '<label><input type="checkbox" class="cap-config-input" data-key="' + esc(key) + '" data-name="allowPublicPost" ' + (cfg.allowPublicPost !== false ? 'checked' : '') + '> 允许前台提交</label>'
			+ '</div>';
	}

	return '<div class="cap-config"><div class="cap-meta">该能力包的实例配置将由 XForm 配置面板渲染。</div></div>';
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
		var fields = form.fields || [];
		if (!Array.isArray(fields)) return defaults;
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
