var contentState = {
	xid: new URLSearchParams(location.search).get('xid') || '',
	tab: 'overview',
	selectedField: -1,
	fields: [
		{name:'title', title:'标题', type:'text', required:true, list:true, detail:true},
		{name:'summary', title:'摘要', type:'textarea', required:false, list:true, detail:true},
		{name:'status', title:'状态', type:'integer', required:false, list:true, detail:true}
	],
	capabilities: [],
	selectedCapability: '',
	enabledCapabilities: {},
	capabilityConfig: {},
	pages: {
		admin: true,
		public: true,
		detail: true,
		listColumns: '',
		detailFields: '',
		defaultSort: 'id_desc',
		pageSize: 20,
		maxScanRows: 5000,
		displayGroups: '',
		fieldGroups: ''
	},
	policies: {
		softDelete: true,
		auditTime: true,
		statusFlow: false,
		createRole: '',
		manageRole: ''
	}
};
var layuiForm = null;

function $(id) { return document.getElementById(id); }

function esc(text) {
	return String(text || '').replace(/[&<>"']/g, function(ch) {
		return ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[ch]);
	});
}

function formData(filter) {
	var form = document.querySelector('[lay-filter="' + filter + '"]');
	var data = {};
	Array.prototype.forEach.call(form.elements, function(el) {
		if (!el.name) return;
		if (el.type === 'checkbox') data[el.name] = el.checked;
		else data[el.name] = el.value;
	});
	return data;
}

function normalizeCapabilityKey(key) {
	var map = {
		comment: 'content.comment',
		tag: 'content.tag',
		topic: 'content.topic',
		sensitive: 'content.sensitive',
		static: 'content.static',
		like: 'content.like',
		view: 'content.view-stat',
		'view-stat': 'content.view-stat'
	};
	return map[key] || key;
}

function buildSpec() {
	var data = formData('content_model_form');
	readPageControls();
	readPolicyControls();
	var used = {};
	var capabilities = Object.keys(contentState.enabledCapabilities).filter(function(key) {
		return contentState.enabledCapabilities[key];
	}).map(function(key) {
		var packId = normalizeCapabilityKey(key);
		if (used[packId]) return null;
		used[packId] = true;
		return {key:packId, enabled:true, config:contentState.capabilityConfig[packId] || contentState.capabilityConfig[key] || {}, mount:{}};
	}).filter(function(item) {
		return !!item;
	});
	return {
		xid: data.xid,
		name: data.name || data.xid,
		title: data.title,
		pluginTitle: data.pluginTitle,
		menuTitle: data.menuTitle,
		namespace: data.namespace,
		tableName: data.tableName,
		generatedPluginXid: data.generatedPluginXid,
		description: data.description,
		note: data.note,
		fields: contentState.fields,
		pages: contentState.pages,
		policies: contentState.policies,
		capabilities: capabilities
	};
}

function updateImpact() {
	var capCount = Object.keys(contentState.enabledCapabilities).filter(function(key) { return contentState.enabledCapabilities[key]; }).length;
	var pageCount = ['admin','public','detail'].filter(function(key) { return contentState.pages[key] !== false; }).length;
	$('content_impact').innerText = contentState.fields.length + ' 个字段，' + pageCount + ' 个页面，' + capCount + ' 个能力。生成插件将使用 page/static/option 等标准目录。';
}
