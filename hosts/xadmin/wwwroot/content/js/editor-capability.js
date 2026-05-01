function renderCapabilities() {
	var side = $('side_capabilities');
	var main = $('capability_workspace');
	if (!contentState.capabilities.length) {
		side.innerHTML = '<div class="side-item"><div class="side-item-meta">暂无能力</div></div>';
		main.innerHTML = '<div class="status-text">暂无能力。</div>';
		return;
	}
	var sideHtml = contentState.capabilities.map(function(cap) {
		var enabled = contentState.enabledCapabilities[cap.key];
		return '<div class="side-item ' + (enabled ? 'active' : '') + '" data-cap="' + esc(cap.key) + '">'
			+ '<div class="side-item-title">' + esc(cap.title || cap.key) + '</div>'
			+ '<div class="side-item-meta">' + esc(cap.key) + ' | ' + (enabled ? '已启用' : '未启用') + '</div>'
			+ '</div>';
	}).join('');
	var detailHtml = contentState.capabilities.map(function(cap) {
		var checked = contentState.enabledCapabilities[cap.key] ? 'checked' : '';
		var cfg = contentState.capabilityConfig[cap.key] || {};
		var surfaces = readJsonArrayText(cap.surfacesJson);
		var defaults = readJsonObjectText(cap.defaultsJson);
		var configHtml = '';
		if (cap.key === 'comment' && contentState.enabledCapabilities[cap.key]) {
			configHtml = '<div class="cap-config">'
				+ '<label>审核</label><select class="cap-config-input" data-key="comment" data-name="moderation">'
				+ '<option value="manual" ' + ((cfg.moderation || 'manual') === 'manual' ? 'selected' : '') + '>人工</option>'
				+ '<option value="auto" ' + (cfg.moderation === 'auto' ? 'selected' : '') + '>自动</option>'
				+ '</select>'
				+ '<label><input type="checkbox" class="cap-config-input" data-key="comment" data-name="allowPublicPost" ' + (cfg.allowPublicPost !== false ? 'checked' : '') + '> 允许前台提交</label>'
				+ '</div>';
		}
		return '<div class="cap-item">'
			+ '<div><div class="cap-title">' + esc(cap.title || cap.key) + '</div><div class="cap-meta">' + esc(cap.key) + ' | ' + esc(cap.providerKind) + '</div>'
			+ '<div class="cap-meta">挂载面：' + esc(surfaces || '-') + '</div>'
			+ '<div class="cap-meta">默认配置：' + esc(defaults || '{}') + '</div>'
			+ configHtml + '</div>'
			+ '<input type="checkbox" class="cap-check" data-key="' + esc(cap.key) + '" ' + checked + '>'
			+ '</div>';
	}).join('');
	side.innerHTML = sideHtml;
	main.innerHTML = detailHtml;
	Array.prototype.forEach.call(side.querySelectorAll('.side-item'), function(item) {
		item.onclick = function() {
			var key = item.getAttribute('data-cap');
			contentState.enabledCapabilities[key] = !contentState.enabledCapabilities[key];
			if (contentState.enabledCapabilities[key] && !contentState.capabilityConfig[key]) {
				contentState.capabilityConfig[key] = capabilityDefaults(key);
			}
			renderCapabilities();
			updateImpact();
		};
	});
	Array.prototype.forEach.call(main.querySelectorAll('.cap-check'), function(chk) {
		chk.onchange = function() {
				contentState.enabledCapabilities[chk.getAttribute('data-key')] = chk.checked;
				if (chk.checked && !contentState.capabilityConfig[chk.getAttribute('data-key')]) {
					contentState.capabilityConfig[chk.getAttribute('data-key')] = capabilityDefaults(chk.getAttribute('data-key'));
				}
				renderCapabilities();
				updateImpact();
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

function capabilityDefaults(key) {
	for (var i = 0; i < contentState.capabilities.length; i++) {
		var cap = contentState.capabilities[i];
		if (cap.key !== key) continue;
		try {
			return JSON.parse(cap.defaultsJson || '{}') || {};
		} catch (err) {
			return {};
		}
	}
	return {};
}

function readJsonArrayText(text) {
	try {
		var value = JSON.parse(text || '[]');
		return Array.isArray(value) ? value.join(', ') : '';
	} catch (err) {
		return '';
	}
}

function readJsonObjectText(text) {
	try {
		var value = JSON.parse(text || '{}');
		return Object.keys(value).map(function(key) { return key + '=' + value[key]; }).join(', ');
	} catch (err) {
		return '';
	}
}

function contentLoadCapabilities() {
	ContentApi.getCapabilities().then(function(ret) {
		if (!ret || !ret.result) {
			contentState.capabilities = [];
			renderCapabilities();
			$('content_state').innerText = ret && ret.message ? ret.message : '能力加载失败';
			return;
		}
		contentState.capabilities = ret && ret.data ? ret.data : [];
		renderCapabilities();
		updateImpact();
	});
}
