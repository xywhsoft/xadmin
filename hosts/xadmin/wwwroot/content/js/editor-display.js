function setTab(tab) {
	var tabTitles = {
		overview: '概览',
		fields: '字段',
		pages: '页面',
		capabilities: '能力',
		policies: '策略',
		generate: '生成',
		advanced: '高级'
	};
	contentState.tab = tab;
	Array.prototype.forEach.call(document.querySelectorAll('.tab-btn'), function(btn) {
		btn.classList.toggle('active', btn.getAttribute('data-tab') === tab);
	});
	['overview','fields','pages','capabilities','policies','generate','advanced'].forEach(function(name) {
		$('tab_' + name).classList.toggle('hidden', name !== tab);
	});
	$('side_overview').classList.toggle('hidden', tab !== 'overview');
	$('side_fields').classList.toggle('hidden', tab !== 'fields');
	$('side_capabilities').classList.toggle('hidden', tab !== 'capabilities');
	$('side_default').classList.toggle('hidden', ['overview','fields','capabilities'].indexOf(tab) >= 0);
	$('field_new').classList.toggle('hidden', tab !== 'fields');
	$('side_title').innerText = tabTitles[tab] || tab;
	if (tab === 'advanced') $('advanced_spec').value = JSON.stringify(buildSpec(), null, 2);
	if (tab === 'generate') updateGeneratePanel();
}

function readPageControls() {
	contentState.pages = {
		admin: $('page_admin').checked,
		public: $('page_public').checked,
		detail: $('page_detail').checked,
		listColumns: $('page_list_columns').value,
		detailFields: $('page_detail_fields').value,
		defaultSort: $('page_default_sort').value,
		pageSize: Number($('page_page_size').value || 20),
		maxScanRows: Number($('page_max_scan_rows').value || 5000),
		displayGroups: parseGroupLines($('page_display_groups').value),
		fieldGroups: parseGroupLines($('page_field_groups').value)
	};
}

function writePageControls(pages) {
	pages = pages || {};
	$('page_admin').checked = pages.admin !== false;
	$('page_public').checked = pages.public !== false;
	$('page_detail').checked = pages.detail !== false;
	$('page_list_columns').value = pages.listColumns || '';
	$('page_detail_fields').value = pages.detailFields || '';
	$('page_default_sort').value = pages.defaultSort || 'id_desc';
	$('page_page_size').value = pages.pageSize || 20;
	$('page_max_scan_rows').value = pages.maxScanRows || 5000;
	$('page_display_groups').value = stringifyGroupLines(pages.displayGroups);
	$('page_field_groups').value = stringifyGroupLines(pages.fieldGroups);
	readPageControls();
}

function parseGroupLines(text) {
	return String(text || '').split('\n').map(function(line) {
		var pos = line.indexOf(':');
		var title = (pos >= 0 ? line.slice(0, pos) : line).trim();
		var fields = (pos >= 0 ? line.slice(pos + 1) : '').split(',').map(function(item) { return item.trim(); }).filter(Boolean);
		return title ? {title:title, fields:fields} : null;
	}).filter(Boolean);
}

function stringifyGroupLines(groups) {
	if (!groups) return '';
	if (typeof groups === 'string') return groups;
	if (!Array.isArray(groups)) return '';
	return groups.map(function(group) {
		return (group.title || group.key || '') + ':' + ((group.fields || []).join(','));
	}).join('\n');
}

function readPolicyControls() {
	contentState.policies = {
		softDelete: $('policy_soft_delete').checked,
		auditTime: $('policy_audit_time').checked,
		statusFlow: $('policy_status_flow').checked,
		createRole: $('policy_create_role').value,
		manageRole: $('policy_manage_role').value
	};
}

function writePolicyControls(policies) {
	policies = policies || {};
	$('policy_soft_delete').checked = policies.softDelete !== false;
	$('policy_audit_time').checked = policies.auditTime !== false;
	$('policy_status_flow').checked = !!policies.statusFlow;
	$('policy_create_role').value = policies.createRole || '';
	$('policy_manage_role').value = policies.manageRole || '';
	readPolicyControls();
}

function updateGeneratePanel() {
	var spec = buildSpec();
	var pluginXid = spec.generatedPluginXid || spec.xid || '-';
	var caps = spec.capabilities.map(function(item) { return item.key; });
	readPageControls();
	readPolicyControls();
	$('generate_preflight').innerText = '生成前将检查 ' + contentState.fields.length + ' 个字段及已选择的页面、能力。';
	$('generate_output').innerText = '插件：' + pluginXid + '\n文件：plugin.json, main.c, page/admin.html, page/public.html, static/css/admin.css, option/runtime.json, src/model.h, src/db.h。';
	$('generate_resources').innerText = '使用标准插件目录：page, template, option, static, src, inc, lib, data。';
	$('generate_database').innerText = '业务记录写入插件私有数据库；xadmin 主库只保存模型、修订和生成历史。';
	$('generate_capabilities').innerText = caps.length ? caps.join(', ') : '未选择能力。';
	updateImpact();
}

function fillForm(spec) {
	var form = document.querySelector('[lay-filter="content_model_form"]');
	form.elements.xid.value = spec.xid || '';
	form.elements.name.value = spec.name || '';
	form.elements.title.value = spec.title || '';
	form.elements.pluginTitle.value = spec.pluginTitle || '';
	form.elements.menuTitle.value = spec.menuTitle || '';
	form.elements.namespace.value = spec.namespace || '';
	form.elements.tableName.value = spec.tableName || '';
	form.elements.generatedPluginXid.value = spec.generatedPluginXid || '';
	form.elements.description.value = spec.description || '';
	form.elements.note.value = '';
	if (spec.specJson) {
		try {
			var parsed = JSON.parse(spec.specJson);
			form.elements.pluginTitle.value = parsed.pluginTitle || spec.pluginTitle || '';
			form.elements.menuTitle.value = parsed.menuTitle || spec.menuTitle || '';
			contentState.fields = parsed.fields || [];
			contentState.enabledCapabilities = {};
			contentState.capabilityConfig = {};
			(parsed.capabilities || []).forEach(function(item) {
				var key = normalizeCapabilityKey(item.key);
				if (item.enabled !== false) contentState.enabledCapabilities[key] = true;
				contentState.capabilityConfig[key] = item.config || {};
			});
			writePageControls(parsed.pages);
			writePolicyControls(parsed.policies);
		} catch (err) {}
	}
	$('content_title').innerText = spec.title || '新建模型';
	$('content_meta').innerText = '修订 ' + (spec.currentRevision || 0) + ' / 已应用 ' + (spec.appliedRevision || 0);
	$('content_state').innerText = '目标插件：' + (spec.generatedPluginXid || spec.xid || '-');
	renderFields();
	renderCapabilities();
	if (layuiForm) layuiForm.render();
}

function loadModel() {
	if (!contentState.xid) {
		renderFields();
		updateImpact();
		return;
	}
	ContentApi.getModel(contentState.xid).then(function(ret) {
		if (ret && ret.result && ret.data) {
			fillForm(ret.data);
			loadRevisions(ret.data.xid);
			loadGenerations(ret.data.xid);
		} else {
			$('content_state').innerText = ret && ret.message ? ret.message : '模型加载失败';
		}
	});
}
