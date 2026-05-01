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
$('content_template').onclick = function() {
	contentState.fields = [
		{name:'title', title:'标题', type:'text', required:true, list:true, detail:true},
		{name:'summary', title:'摘要', type:'textarea', required:false, list:true, detail:true},
		{name:'status', title:'状态', type:'integer', required:false, list:true, detail:true}
	];
	selectField(0);
};

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
