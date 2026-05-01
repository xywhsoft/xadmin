layui.use(['form'], function() {
	layuiForm = layui.form;
	layuiForm.render();
});

Array.prototype.forEach.call(document.querySelectorAll('.tab-btn'), function(btn) {
	btn.onclick = function() { setTab(btn.getAttribute('data-tab')); };
});

$('field_new').onclick = newField;
$('field_apply').onclick = applyField;
$('field_delete').onclick = deleteField;
$('field_copy').onclick = copyField;
$('field_up').onclick = function() { moveField(-1); };
$('field_down').onclick = function() { moveField(1); };
$('field_option_add').onclick = addFieldOption;
$('content_save').onclick = saveModel;
$('content_preflight').onclick = preflight;
$('content_generate').onclick = generatePlugin;
$('content_template').onclick = applyArticleTemplate;

function articleTemplateSpec() {
	return {
		xid: 'cms.article',
		name: 'article',
		title: '文章',
		pluginTitle: '文章',
		menuTitle: '文章内容',
		namespace: 'cms',
		tableName: 'cms_article',
		generatedPluginXid: 'cms.article',
		description: '基础文章内容模型',
		note: '',
		fields: [
			{name:'title', title:'标题', type:'text', storage:{type:'text'}, component:{type:'text', props:{placeholder:'请输入标题'}}, semantic:{role:'title'}, required:true, list:true, detail:true, showInForm:true, showInList:true, showInDetail:true, searchable:true, description:'文章标题'},
			{name:'summary', title:'摘要', type:'textarea', storage:{type:'text'}, component:{type:'textarea', props:{height:160, placeholder:'请输入摘要'}}, semantic:{role:'summary'}, required:false, list:true, detail:true, showInForm:true, showInList:true, showInDetail:true, searchable:true, description:'列表和详情页摘要'},
			{name:'content', title:'正文', type:'editor_md', storage:{type:'text'}, component:{type:'editor_md', props:{height:420}}, semantic:{role:'content'}, required:false, list:false, detail:true, showInForm:true, showInList:false, showInDetail:true, layoutSpan:2, description:'文章正文内容'}
		],
		pages: {
			admin: true,
			public: true,
			detail: true,
			listColumns: 'title,summary,updateTime',
			detailFields: 'title,summary,content',
			defaultSort: 'id_desc',
			pageSize: 20,
			displayGroups: [
				{title:'内容', fields:['title','summary','content']}
			],
			fieldGroups: [
				{title:'内容', fields:['title','summary','content']}
			]
		},
		policies: {
			softDelete: true,
			auditTime: true,
			statusFlow: false,
			createRole: '',
			manageRole: ''
		}
	};
}

function cloneSpec(value) {
	return JSON.parse(JSON.stringify(value));
}

function setModelFormValue(form, name, value) {
	if (form.elements[name]) form.elements[name].value = value == null ? '' : value;
}

function applyArticleTemplate() {
	var spec = articleTemplateSpec();
	var form = document.querySelector('[lay-filter="content_model_form"]');
	setModelFormValue(form, 'xid', spec.xid);
	setModelFormValue(form, 'name', spec.name);
	setModelFormValue(form, 'title', spec.title);
	setModelFormValue(form, 'namespace', spec.namespace);
	setModelFormValue(form, 'pluginTitle', spec.pluginTitle);
	setModelFormValue(form, 'menuTitle', spec.menuTitle);
	setModelFormValue(form, 'tableName', spec.tableName);
	setModelFormValue(form, 'generatedPluginXid', spec.generatedPluginXid);
	setModelFormValue(form, 'description', spec.description);
	setModelFormValue(form, 'note', spec.note);

	contentState.fields = cloneSpec(spec.fields);
	contentState.enabledCapabilities = {};
	contentState.capabilityConfig = {};
	writePageControls(cloneSpec(spec.pages));
	writePolicyControls(cloneSpec(spec.policies));
	selectField(0);
	renderCapabilities();
	updateImpact();

	$('content_title').innerText = spec.title;
	$('content_meta').innerText = '修订 0 / 已应用 0';
	$('content_state').innerText = '已载入基础文章模型模板';
	if (layuiForm) layuiForm.render();
	if (window.layer && layer.msg) layer.msg('已载入基础文章模型模板');
	setTab('overview');
}

['page_admin','page_public','page_detail','page_list_columns','page_detail_fields','page_default_sort','page_page_size','page_display_groups','page_field_groups'].forEach(function(id) {
	var el = $(id);
	el.onchange = function() { readPageControls(); updateImpact(); };
	el.oninput = function() { readPageControls(); updateImpact(); };
});
['policy_soft_delete','policy_audit_time','policy_status_flow','policy_create_role','policy_manage_role'].forEach(function(id) {
	var el = $(id);
	el.onchange = function() { readPolicyControls(); updateImpact(); };
	el.oninput = function() { readPolicyControls(); updateImpact(); };
});

selectField(0);
writePageControls(contentState.pages);
writePolicyControls(contentState.policies);
contentLoadCapabilities();
loadModel();
setTab('overview');
