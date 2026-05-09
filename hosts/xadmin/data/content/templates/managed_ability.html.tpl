<style>
	.managed-ability-page{padding:16px;background:#f2f2f2;color:#1f2937;min-height:100%}
	.managed-ability-page .x-shell{background:#fff;border:1px solid #e6e6e6}
	.managed-ability-page .x-head{padding:14px 16px;border-bottom:1px solid #e6e6e6;display:flex;align-items:center;justify-content:space-between;gap:12px}
	.managed-ability-page .x-title{font-size:16px;font-weight:600}
	.managed-ability-page .x-sub{font-size:12px;color:#6b7280;margin-top:4px}
	.managed-ability-page .x-body{padding:15px}
	.managed-ability-page .x-toolbar{display:flex;align-items:center;justify-content:space-between;gap:12px;margin-bottom:12px}
	.managed-ability-page .x-form{background:#fafafa;border:1px solid #eeeeee;padding:12px;margin-bottom:12px}
	.managed-ability-page .x-form .layui-form-item{margin-bottom:10px}
	.managed-ability-page .x-form .layui-form-label{width:86px}
	.managed-ability-page .x-form .layui-input-block{margin-left:116px}
	.managed-ability-page .x-muted{color:#6b7280}
	.managed-ability-page .x-empty{padding:40px 0;text-align:center;color:#9ca3af}
	.managed-ability-page .x-code{white-space:pre-wrap;background:#fafafa;border:1px solid #eee;padding:10px;max-height:180px;overflow:auto}
	.managed-ability-page .x-grid{display:grid;grid-template-columns:repeat(4,1fr);gap:10px;margin-bottom:12px}
	.managed-ability-page .x-stat{border:1px solid #e6e6e6;background:#fff;padding:12px}
	.managed-ability-page .x-stat b{display:block;font-size:22px;margin-top:6px}
	.managed-ability-dialog{padding:16px 18px 0 18px}
	.managed-ability-dialog .layui-form-label{width:92px}
	.managed-ability-dialog .layui-input-block{margin-left:122px}
	.managed-ability-dialog .x-dialog-actions{border-top:1px solid #e6e6e6;margin:18px -18px 0 -18px;padding:12px 18px;text-align:right;background:#fff}
	@media(max-width:900px){.managed-ability-page .x-grid{grid-template-columns:repeat(2,1fr)}.managed-ability-page .x-toolbar{display:block}.managed-ability-page .x-toolbar .layui-btn-container{margin-top:10px}}
</style>
<div class="managed-ability-page">
	<div class="x-shell">
		<div class="x-head">
			<div>
				<div class="x-title" id="pageTitle">能力包管理</div>
				<div class="x-sub" id="pageSub">@@PLUGIN_XID@@</div>
			</div>
			<div class="layui-btn-container">
				<button class="layui-btn layui-btn-primary layui-btn-sm" id="btnReload"><i class="layui-icon layui-icon-refresh"></i> 刷新</button>
			</div>
		</div>
		<div class="x-body">
			<div id="summary" class="x-grid"></div>
			<div id="editor"></div>
			<table id="dataTable" lay-filter="dataTable"></table>
			<div id="detail" style="margin-top:12px"></div>
		</div>
	</div>

	<script type="text/html" id="rowActions">
		{{# if(d.__ops && d.__ops.indexOf('diff') >= 0){ }}
		<a class="layui-btn layui-btn-xs" lay-event="diff">Diff</a>
		{{# } }}
		{{# if(d.__ops && d.__ops.indexOf('restore') >= 0){ }}
		<a class="layui-btn layui-btn-warm layui-btn-xs" lay-event="restore">Restore</a>
		{{# } }}
		{{# if(d.__ops && d.__ops.indexOf('audit') >= 0){ }}
		<a class="layui-btn layui-btn-xs" lay-event="approve">通过</a>
		<a class="layui-btn layui-btn-danger layui-btn-xs" lay-event="reject">驳回</a>
		{{# } }}
		{{# if(d.__ops && d.__ops.indexOf('hide') >= 0){ }}
		<a class="layui-btn layui-btn-warm layui-btn-xs" lay-event="hide">隐藏</a>
		{{# } }}
		{{# if(d.__ops && d.__ops.indexOf('process') >= 0){ }}
		<a class="layui-btn layui-btn-normal layui-btn-xs" lay-event="process">Processed</a>
		{{# } }}
		{{# if(d.__ops && d.__ops.indexOf('download') >= 0){ }}
		<a class="layui-btn layui-btn-xs" lay-event="download">Download</a>
		{{# } }}
		{{# if(d.__ops && d.__ops.indexOf('edit') >= 0){ }}
		<a class="layui-btn layui-btn-xs" lay-event="edit">编辑</a>
		{{# } }}
		{{# if(d.__ops && d.__ops.indexOf('delete') >= 0){ }}
		<a class="layui-btn layui-btn-danger layui-btn-xs" lay-event="delete">删除</a>
		{{# } }}
		{{# if(d.__ops && d.__ops.indexOf('unbind') >= 0){ }}
		<a class="layui-btn layui-btn-danger layui-btn-xs" lay-event="unbind">解绑</a>
		{{# } }}
	</script>
	<script>
	(function(){
		var pluginXid = '@@PLUGIN_XID@@';
		var currentScript = document.currentScript;
		var pendingRoots = document.querySelectorAll('.managed-ability-page:not([data-ability-bound])');
		var root = currentScript && currentScript.closest ? currentScript.closest('.managed-ability-page') : null;
		if(!root) root = pendingRoots[pendingRoots.length - 1] || document.querySelector('.managed-ability-page');
		if(root && root.setAttribute) root.setAttribute('data-ability-bound', '1');
		var safeToPack = {
			content_comment:'content.comment',
			content_tag:'content.tag',
			content_topic:'content.topic',
			content_sensitive:'content.sensitive',
			content_static:'content.static',
			content_like:'content.like',
			content_view_stat:'content.view-stat',
			content_slug:'content.slug',
			content_redirect:'content.redirect',
			content_media:'content.media',
			content_revision:'content.revision',
			content_workflow:'content.workflow',
			content_search:'content.search',
			content_sitemap:'content.sitemap',
			content_related:'content.related',
			content_form:'content.form',
			content_access:'content.access',
			content_audit_log:'content.audit-log',
			content_import_export:'content.import-export'
		};
		var pageKey = '@@ABILITY_PAGE_KEY@@' || location.pathname.split('/').pop() || 'overview';
		var packId = safeToPack[pageKey] || '';
		var instanceKey = (pluginXid + '_' + pageKey + '_' + Math.random().toString(36).slice(2)).replace(/[^A-Za-z0-9_]/g, '_');
		var tableFilter = 'dataTable_' + instanceKey;
		var tableSelector = '#dataTable_' + instanceKey;
		var actionTplId = 'rowActions_' + instanceKey;
		var tableEl = root.querySelector('#dataTable');
		var actionTpl = root.querySelector('#rowActions');
		if(tableEl){
			tableEl.id = 'dataTable_' + instanceKey;
			tableEl.setAttribute('lay-filter', tableFilter);
		}
		if(actionTpl) actionTpl.id = actionTplId;
		var packTitles = {
			'content.comment':'评论管理',
			'content.tag':'标签管理',
			'content.topic':'专题管理',
			'content.sensitive':'敏感词管理',
			'content.static':'静态化管理',
			'content.like':'点赞统计',
			'content.view-stat':'访问统计',
			'content.redirect':'跳转规则',
			'content.media':'媒体资源',
			'content.revision':'内容版本',
			'content.workflow':'审核流程',
			'content.search':'内容搜索'
		};
		var tableIns = null;
		packTitles['content.sitemap'] = '站点地图';
		packTitles['content.related'] = '相关推荐';
		packTitles['content.form'] = '内容表单';
		packTitles['content.access'] = '阅读权限';
		packTitles['content.audit-log'] = '操作审计';
		packTitles['content.import-export'] = '导入导出';
		packTitles['content.slug'] = '固定链接';
		var activeViews = {};
		var importExportLastExport = null;
		function byId(id){ return root.querySelector('#' + id); }
		function scopedCols(cols){
			return (cols || []).map(function(col){
				var next = {};
				Object.keys(col).forEach(function(key){ next[key] = col[key]; });
				if(next.toolbar === '#rowActions') next.toolbar = '#' + actionTplId;
				return next;
			});
		}
		function api(path){ return '/admin/api/plugin/' + pluginXid + path; }
		function pub(path){ return '/api/plugin/' + pluginXid + path; }
		function esc(s){ return String(s == null ? '' : s).replace(/[&<>"]/g,function(c){return {'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c];}); }
		function post(url, data){
			return fetch(url,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(data||{})}).then(function(r){return r.json();});
		}
		function get(url){ return fetch(url,{cache:'no-store'}).then(function(r){return r.json();}); }
		function importExportFields(){
			var input = byId('importExportFields');
			return input && input.value ? input.value.split(',').map(function(s){return s.trim();}).filter(Boolean) : [];
		}
		function importExportConflictMode(){
			var input = byId('importExportConflictMode');
			var value = input && input.value ? input.value.trim() : 'insert';
			return /^(insert|update|skip)$/.test(value) ? value : 'insert';
		}
		function importExportChunkSize(){
			var input = byId('importExportChunkSize');
			var size = input ? Number(input.value || 50) : 50;
			if(!size || size < 1) size = 50;
			if(size > 200) size = 200;
			if(input) input.value = size;
			return size;
		}
		function importExportExportPayload(){
			var limitInput = byId('importExportLimit');
			var offsetInput = byId('importExportOffset');
			var payload = {fields:importExportFields()};
			var limit = limitInput ? Number(limitInput.value || 0) : 0;
			var offset = offsetInput ? Number(offsetInput.value || 0) : 0;
			if(limit > 0) payload.limit = limit;
			if(offset > 0) payload.offset = offset;
			return payload;
		}
		function showImportExportResult(ret){
			var target = byId('importExportResult');
			if(target) target.textContent = JSON.stringify(ret && ret.data ? ret.data : ret, null, 2);
			importExportLastExport = ret || null;
			if(ret && ret.hasMore && byId('importExportOffset')) byId('importExportOffset').value = ret.nextOffset || 0;
			message(ret);
			reload();
		}
		function runImportExportExport(nextPage){
			var payload = importExportExportPayload();
			if(nextPage && importExportLastExport && importExportLastExport.hasMore) {
				payload.offset = importExportLastExport.nextOffset || payload.offset || 0;
			}
			post(api('/import-export/export/json'), payload).then(showImportExportResult);
		}
		function importExportItems(){
			var input = byId('importExportItems');
			var text = input && input.value ? input.value : '[{}]';
			try {
				var rows = JSON.parse(text);
				return Array.isArray(rows) ? rows : [];
			} catch(e) {
				message({result:false,error:'invalid import json'});
				return [];
			}
		}
		function runImportExportCommit(confirm){
			var rows = importExportItems();
			if(!rows.length) return;
			var payload = {sourceName:'manual-json',fields:importExportFields(),items:rows};
			if(confirm) payload.confirm = true;
			if(confirm) payload.conflictMode = importExportConflictMode();
			post(api(confirm ? '/import-export/import/commit' : '/import-export/import/preview'), payload).then(function(ret){message(ret);reload();});
		}
		function runImportExportChunked(confirm){
			var rows = importExportItems();
			var size = importExportChunkSize();
			var totalChunks = Math.ceil(rows.length / size);
			var results = [];
			var target = byId('importExportResult');
			if(!rows.length) return;
			function runChunk(index){
				if(index >= totalChunks){
					if(target) target.textContent = JSON.stringify({result:true,total:rows.length,chunks:results}, null, 2);
					reload();
					return;
				}
				var chunk = rows.slice(index * size, Math.min(rows.length, (index + 1) * size));
				var payload = {sourceName:'manual-json-chunk-' + (index + 1),fields:importExportFields(),items:chunk};
				if(confirm) payload.confirm = true;
				if(confirm) payload.conflictMode = importExportConflictMode();
				if(target) target.textContent = 'Running chunk ' + (index + 1) + '/' + totalChunks + ' (' + chunk.length + ' rows)...';
				post(api(confirm ? '/import-export/import/commit' : '/import-export/import/preview'), payload).then(function(ret){
					results.push({chunk:index + 1,count:chunk.length,result:ret && ret.result !== false,summary:ret && ret.data ? ret.data : ret});
					if(ret && ret.result === false){
						if(target) target.textContent = JSON.stringify({result:false,failedChunk:index + 1,chunks:results}, null, 2);
						message(ret);
						return;
					}
					runChunk(index + 1);
				});
			}
			runChunk(0);
		}
		function runImportReplay(){
			var input = byId('importReplayJobId');
			var id = input ? Number(input.value || 0) : 0;
			if(!id){ message({result:false,error:'import job id is required'}); return; }
			post(api('/import-export/import/replay'), {id:id}).then(function(ret){
				var target = byId('importExportItems');
				if(ret && ret.result !== false && target) target.value = JSON.stringify(ret.items || [], null, 2);
				message(ret);
			});
		}
		function mediaBatchIds(){
			var input = byId('mediaBatchIds');
			return input && input.value ? input.value.split(',').map(function(s){return parseInt(s.trim(),10);}).filter(function(n){return n>0;}) : [];
		}
		function runMediaBatch(action){
			var ids = mediaBatchIds();
			if(!ids.length){ message({result:false,error:'media ids required'}); return; }
			post(api('/media/batch'), {action:action,ids:ids}).then(function(ret){message(ret);reload();});
		}
		function formSchemaElement(){
			var form = byId('abilityForm');
			return form ? form.querySelector('[name="schemaJson"]') : null;
		}
		function readFormSchema(){
			var input = formSchemaElement();
			var text = input && input.value ? input.value : '{}';
			return JSON.parse(text);
		}
		function writeFormSchema(schema){
			var input = formSchemaElement();
			if(input) input.value = JSON.stringify(schema || {}, null, 2);
		}
		function appendFormSchemaField(){
			var nameInput = byId('formDesignerName');
			var typeInput = byId('formDesignerType');
			var labelInput = byId('formDesignerLabel');
			var requiredInput = byId('formDesignerRequired');
			var placeholderInput = byId('formDesignerPlaceholder');
			var minInput = byId('formDesignerMin');
			var maxInput = byId('formDesignerMax');
			var patternInput = byId('formDesignerPattern');
			var optionsInput = byId('formDesignerOptions');
			var name = nameInput ? nameInput.value.trim() : '';
			if(!name){ message({result:false,error:'field name is required'}); return; }
			try {
				var schema = readFormSchema();
				var fieldType = (typeInput && typeInput.value) || 'text';
				var field = {name:name,label:(labelInput && labelInput.value) || name,type:fieldType};
				if(!Array.isArray(schema.fields)) schema.fields = [];
				if(placeholderInput && placeholderInput.value) field.placeholder = placeholderInput.value;
				if(patternInput && patternInput.value) field.pattern = patternInput.value;
				if(optionsInput && optionsInput.value) field.options = optionsInput.value.split(',').map(function(s){return s.trim();}).filter(Boolean);
				if(minInput && minInput.value !== ''){
					if(fieldType === 'number') field.min = Number(minInput.value);
					else field.minLength = Number(minInput.value);
				}
				if(maxInput && maxInput.value !== ''){
					if(fieldType === 'number') field.max = Number(maxInput.value);
					else field.maxLength = Number(maxInput.value);
				}
				schema.fields.push(field);
				if(requiredInput && requiredInput.checked){
					if(!Array.isArray(schema.required)) schema.required = [];
					if(schema.required.indexOf(name) < 0) schema.required.push(name);
				}
				writeFormSchema(schema);
				message({result:true,message:'field appended'});
			} catch(e) {
				message({result:false,error:'invalid schema json'});
			}
		}
		function findFormSchemaFieldIndex(fields, name){
			if(!Array.isArray(fields) || !fields.length) return -1;
			if(!name) return fields.length - 1;
			for(var i=0;i<fields.length;i++){
				if(fields[i] && fields[i].name === name) return i;
			}
			return -1;
		}
		function removeFormSchemaField(){
			var nameInput = byId('formDesignerName');
			var name = nameInput ? nameInput.value.trim() : '';
			try {
				var schema = readFormSchema();
				var idx = findFormSchemaFieldIndex(schema.fields, name);
				if(idx < 0){ message({result:false,error:'field not found'}); return; }
				var removed = schema.fields.splice(idx, 1)[0];
				if(Array.isArray(schema.required) && removed && removed.name){
					schema.required = schema.required.filter(function(item){ return item !== removed.name; });
				}
				writeFormSchema(schema);
				message({result:true,message:'field removed'});
			} catch(e) {
				message({result:false,error:'invalid schema json'});
			}
		}
		function moveFormSchemaField(step){
			var nameInput = byId('formDesignerName');
			var name = nameInput ? nameInput.value.trim() : '';
			try {
				var schema = readFormSchema();
				var fields = Array.isArray(schema.fields) ? schema.fields : [];
				var idx = findFormSchemaFieldIndex(fields, name);
				var next = idx + step;
				if(idx < 0 || next < 0 || next >= fields.length){ message({result:false,error:'field cannot move'}); return; }
				var tmp = fields[idx];
				fields[idx] = fields[next];
				fields[next] = tmp;
				schema.fields = fields;
				writeFormSchema(schema);
				message({result:true,message:'field moved'});
			} catch(e) {
				message({result:false,error:'invalid schema json'});
			}
		}
		function formatFormSchema(){
			try { writeFormSchema(readFormSchema()); message({result:true,message:'schema formatted'}); }
			catch(e){ message({result:false,error:'invalid schema json'}); }
		}
		function validateFormSchema(){
			try { readFormSchema(); message({result:true,message:'schema json is valid'}); }
			catch(e){ message({result:false,error:'invalid schema json'}); }
		}
		function message(ret, fallback){
			var text = (ret && (ret.message || ret.error)) || fallback || '操作完成';
			var warning = ret && (ret.warning || (ret.data && ret.data.warning));
			if(warning) text += '；Warning: ' + warning;
			if(window.layer) layer.msg(text, {icon: ret && ret.result === false ? 2 : 1});
		}
		function showJsonResult(targetId, ret){
			var target = byId(targetId);
			if(target) target.textContent = JSON.stringify(ret && ret.data ? ret.data : ret, null, 2);
			message(ret);
		}
		function currentConfig(){
			return configs[packId] || configs.overview;
		}
		function currentView(){
			var cfg = currentConfig();
			var key = activeViews[packId] || 'main';
			if(cfg.views && cfg.views[key]) return cfg.views[key];
			return cfg;
		}
		function renderSummary(meta){
			var p = meta && meta.data && meta.data.pack ? meta.data.pack : {};
			var effects = p.effects || {};
			var contracts = p.contracts || {};
			var items = [
				['数据表', (effects.tables || contracts.tables || []).length],
				['后台页', (effects.adminPages || contracts.adminPages || []).length],
				['前端接口', (effects.publicApis || contracts.publicApis || []).length],
				['权限点', (effects.permissions || contracts.permissions || []).length]
			];
			byId('summary').innerHTML = items.map(function(item){
				return '<div class="x-stat"><span class="x-muted">'+item[0]+'</span><b>'+item[1]+'</b></div>';
			}).join('');
		}
		function renderEditor(){
			var cfg = currentConfig();
			byId('editor').innerHTML = cfg.form ? cfg.form() : '';
			if(window.layui && layui.form) layui.form.render();
			bindEditor();
		}
		function bindEditor(){
			var btnSave = byId('btnSaveAbility');
			var btnGenerate = byId('btnStaticGenerate');
			var btnSlugCheck = byId('btnSlugCheck');
			var btnSlugPreview = byId('btnSlugPreview');
			var btnSlugRepairPreview = byId('btnSlugRepairPreview');
			var btnSlugRepairConfirm = byId('btnSlugRepairConfirm');
			var btnAuditCleanup = byId('btnAuditCleanup');
			if(btnSave) btnSave.onclick = saveEditor;
			if(btnGenerate) btnGenerate.onclick = runStaticGenerate;
			if(btnSlugCheck) btnSlugCheck.onclick = runSlugCheck;
			if(btnSlugPreview) btnSlugPreview.onclick = runSlugPreview;
			if(btnSlugRepairPreview) btnSlugRepairPreview.onclick = function(){ runSlugRepair(false); };
			if(btnSlugRepairConfirm) btnSlugRepairConfirm.onclick = function(){ runSlugRepair(true); };
			if(btnAuditCleanup) btnAuditCleanup.onclick = runAuditCleanup;
			Array.prototype.forEach.call(root.querySelectorAll('[data-view]'), function(btn){
				btn.onclick = function(){
					activeViews[packId] = btn.getAttribute('data-view') || 'main';
					reload();
				};
			});
		}
		function formData(formId){
			var form = byId(formId);
			var data = {};
			Array.prototype.forEach.call(form ? form.elements : [], function(el){
				if(!el.name) return;
				if(el.type === 'number') data[el.name] = Number(el.value || 0);
				else data[el.name] = el.value;
			});
			return data;
		}
		function saveEditor(){
			var cfg = currentConfig();
			if(!cfg.saveApi) return;
			post(api(cfg.saveApi), formData('abilityForm')).then(function(ret){ message(ret); reload(); });
		}
		function runStaticGenerate(){
			post(api('/static/generate'), formData('staticGenerateForm')).then(function(ret){ message(ret); reload(); });
		}
		function slugQuery(){
			var slug = byId('slugProbe') ? byId('slugProbe').value : '';
			var id = byId('slugProbeId') ? byId('slugProbeId').value : '';
			return '?slug=' + encodeURIComponent(slug || '') + (id ? '&id=' + encodeURIComponent(id) : '');
		}
		function showSlugRet(ret){
			showJsonResult('slugProbeResult', ret);
		}
		function runSlugCheck(){
			get(api('/slug/check' + slugQuery())).then(showSlugRet);
		}
		function runSlugPreview(){
			get(api('/slug/preview' + slugQuery())).then(showSlugRet);
		}
		function runSlugRepair(confirm){
			post(api('/slug/repair'), {confirm:!!confirm,limit:500}).then(function(ret){ showSlugRet(ret); reload(); });
		}
		function runAuditCleanup(){
			var days = byId('auditKeepDays') ? Number(byId('auditKeepDays').value || 0) : 0;
			if(!days || days < 1){ message({result:false,error:'keepDays is required'}); return; }
			post(api('/audit-log/cleanup'), {keepDays:days}).then(function(ret){ message(ret); reload(); });
		}
		function redirectImportItems(){
			var input = byId('redirectImportJson');
			var text = input && input.value ? input.value : '[{"sourcePath":"/old","targetUrl":"/new","statusCode":301}]';
			try {
				var rows = JSON.parse(text);
				return Array.isArray(rows) ? rows : [];
			} catch(e) {
				message({result:false,error:'invalid import json'});
				return [];
			}
		}
		function runRedirectImport(confirm){
			var rows = redirectImportItems();
			if(!rows.length) return;
			post(api('/redirect/import'), {confirm:!!confirm,items:rows}).then(function(ret){ showJsonResult('redirectImportResult', ret); reload(); });
		}
		function renderRevisionValue(value, peer){
			var text = String(value == null ? '' : value);
			var peerText = String(peer == null ? '' : peer);
			var isLong = text.length > 160 || peerText.length > 160 || text.indexOf('\n') >= 0 || peerText.indexOf('\n') >= 0;
			if(!isLong) return '<pre class="x-code">' + esc(text) + '</pre>';
			var lines = text.split(/\r?\n/);
			var peerLines = peerText.split(/\r?\n/);
			var max = Math.min(lines.length, 200);
			var html = '<div class="revision-line-diff">';
			for(var i=0;i<max;i++){
				var changed = lines[i] !== peerLines[i];
				html += '<div style="display:flex;gap:8px;background:' + (changed ? '#fff7ed' : 'transparent') + ';border-bottom:1px solid #f2f4f7;"><span style="width:42px;text-align:right;color:#98a2b3;flex:none;">' + (i + 1) + '</span><pre class="x-code" style="margin:0;flex:1;border:0;background:transparent;">' + esc(lines[i]) + '</pre></div>';
			}
			if(lines.length > max) html += '<div class="x-muted">... ' + esc(lines.length - max) + ' more lines omitted</div>';
			return html + '</div>';
		}
		function renderRevisionChangeTable(changes){
			var rows = Array.isArray(changes) ? changes : [];
			var html = '<table class="layui-table"><thead><tr><th style="width:160px">Field</th><th>Before</th><th>After</th></tr></thead><tbody>';
			rows.forEach(function(row){
				html += '<tr><td><b>' + esc(row.title || row.field || '') + '</b><div class="x-muted">' + esc(row.field || '') + '</div></td><td>' + renderRevisionValue(row.before, row.after) + '</td><td>' + renderRevisionValue(row.after, row.before) + '</td></tr>';
			});
			if(!rows.length) html += '<tr><td colspan="3" style="text-align:center;color:#667085;">No field changes</td></tr>';
			return html + '</tbody></table>';
		}
		function showRevisionDiff(row){
			get(api('/revision/diff?id=' + encodeURIComponent(row.id))).then(function(ret){
				var data = ret && ret.data ? ret.data : {};
				var changes = data.changes || [];
				byId('detail').innerHTML = '<div class="x-form"><div class="x-muted">Revision diff #' + esc(row.id) + ' - ' + esc(data.changeCount || changes.length || 0) + ' changed fields</div>' + renderRevisionChangeTable(changes) + '</div>';
				message(ret);
			});
		}
		function restoreRevision(row){
			post(api('/revision/restore'), {id:row.id}).then(function(ret){ message(ret); reload(); });
		}
		function showRevisionRestorePreview(row){
			get(api('/revision/restore-preview?id=' + encodeURIComponent(row.id))).then(function(ret){
				var data = ret && ret.data ? ret.data : {};
				var changes = data.changes || [];
				var html = '<div class="x-form"><div class="x-muted">Restore preview #' + esc(row.id) + ' - ' + esc(data.changeCount || changes.length || 0) + ' changed fields</div>' + renderRevisionChangeTable(changes) + '<div class="x-dialog-actions"><button type="button" class="layui-btn layui-btn-primary" id="btnRevisionRestoreCancel">Cancel</button><button type="button" class="layui-btn layui-btn-danger" id="btnRevisionRestoreConfirm">Restore</button></div></div>';
				byId('detail').innerHTML = html;
				var cancelBtn = byId('btnRevisionRestoreCancel');
				var restoreBtn = byId('btnRevisionRestoreConfirm');
				if(cancelBtn) cancelBtn.onclick = function(){ byId('detail').innerHTML = ''; };
				if(restoreBtn) restoreBtn.onclick = function(){ restoreRevision(row); };
				message(ret);
			});
		}
		function showSearchStats(){
			get(api('/search/stats')).then(function(ret){
				byId('detail').innerHTML = '<div class="x-form"><div class="x-muted">Search index stats</div><pre class="x-code">' + esc(JSON.stringify(ret && ret.data ? ret.data : ret, null, 2)) + '</pre></div>';
				message(ret);
			});
		}
		function rebuildSearchIndex(){
			post(api('/search/rebuild'), {}).then(function(ret){
				byId('detail').innerHTML = '<div class="x-form"><div class="x-muted">Search rebuild result</div><pre class="x-code">' + esc(JSON.stringify(ret, null, 2)) + '</pre></div>';
				message(ret);
				reload();
			});
		}
		function runSearchProbe(){
			var qInput = byId('searchProbeQuery');
			var limitInput = byId('searchProbeLimit');
			var q = qInput && qInput.value ? qInput.value : '';
			var limit = Number(limitInput && limitInput.value || 10);
			if(!q){ message({result:false,error:'search query is required'}); return; }
			if(!limit || limit < 1) limit = 10;
			if(limit > 20) limit = 20;
			if(limitInput) limitInput.value = limit;
			get(api('/search?q=' + encodeURIComponent(q) + '&limit=' + encodeURIComponent(limit))).then(function(ret){
				var data = ret && ret.data ? ret.data : ret;
				var target = byId('searchProbeResult') || byId('detail');
				target.innerHTML = esc(JSON.stringify(data, null, 2));
				message(ret);
			});
		}
		function renderSitemapStats(data){
			data = data || {};
			var rows = [
				['Enabled entries', data.enabledCount],
				['Disabled entries', data.disabledCount],
				['Total entries', data.totalCount],
				['Cache policy', data.cachePolicy || 'write-through'],
				['Cache file', data.cacheFile || 'sitemap/cache.json'],
				['Cache entry count', data.cacheEntryCount],
				['Cache TTL seconds', data.cacheTtlSeconds],
				['Cache expires at', data.cacheExpiresAt || '-'],
				['Cache expired', data.cacheExpired ? 'yes' : 'no'],
				['Last update', data.lastUpdateText || data.lastUpdate || '-']
			];
			var html = '<div class="x-form" id="sitemapStatsTable"><div class="x-muted">Sitemap cache metadata</div><table class="layui-table"><thead><tr><th style="width:220px">Metric</th><th>Value</th></tr></thead><tbody>';
			rows.forEach(function(row){
				html += '<tr><td>' + esc(row[0]) + '</td><td>' + esc(row[1] == null ? '' : row[1]) + '</td></tr>';
			});
			return html + '</tbody></table></div>';
		}
		function showSitemapStats(){
			get(api('/sitemap/stats')).then(function(ret){
				byId('detail').innerHTML = renderSitemapStats(ret && ret.data ? ret.data : ret);
				message(ret);
			});
		}
		function refreshSitemapEntries(){
			post(api('/sitemap/refresh'), {}).then(function(ret){
				byId('detail').innerHTML = '<div class="x-form"><div class="x-muted">Sitemap refresh result</div><pre class="x-code">' + esc(JSON.stringify(ret, null, 2)) + '</pre></div>';
				message(ret);
				reload();
			});
		}
		function showMetricStats(path, title){
			get(api(path)).then(function(ret){
				var data = ret && ret.data ? ret.data : ret;
				var html = '<div class="x-form"><div class="x-muted">' + esc(title) + '</div><table class="layui-table"><thead><tr><th style="width:220px">Metric</th><th>Value</th></tr></thead><tbody>';
				Object.keys(data || {}).forEach(function(key){
					html += '<tr><td>' + esc(key) + '</td><td>' + esc(data[key]) + '</td></tr>';
				});
				byId('detail').innerHTML = html + '</tbody></table></div>';
				message(ret);
			});
		}
		function rebuildRelatedRules(){
			var idInput = byId('relatedRebuildContentId');
			var limitInput = byId('relatedRebuildLimit');
			var limit = Number(limitInput && limitInput.value || 5);
			if(!limit || limit < 1) limit = 5;
			if(limit > 20) limit = 20;
			if(limitInput) limitInput.value = limit;
			post(api('/related/rebuild'), {contentId:Number(idInput && idInput.value || 0),limit:limit}).then(function(ret){
				var data = ret && ret.data ? ret.data : ret;
				var target = byId('relatedRebuildResult') || byId('detail');
				target.innerHTML = esc(JSON.stringify(data, null, 2));
				message(ret);
				reload();
			});
		}
		function exportFormSubmissions(){
			get(api('/form/submission/export')).then(function(ret){
				var data = ret && ret.data ? ret.data : ret;
				byId('detail').innerHTML = '<div class="x-form"><div class="x-muted">Form submission export</div><pre class="x-code">' + esc(JSON.stringify(data, null, 2)) + '</pre></div>';
				message(ret);
			});
		}
		function markFormSubmissionProcessed(row){
			post(api('/form/submission/status'), {id:row.id,status:2}).then(function(ret){
				message(ret);
				reload();
			});
		}
		function runWorkflowAction(action){
			var idInput = byId('workflowContentId');
			var reasonInput = byId('workflowReason');
			var publishAtInput = byId('workflowPublishAt');
			var assigneeInput = byId('workflowAssigneeId');
			var id = Number(idInput && idInput.value || 0);
			var payload = {id:id,action:action,reason:reasonInput && reasonInput.value || ''};
			if(!id){ message({result:false,error:'content id is required'}); return; }
			if(assigneeInput && assigneeInput.value) payload.assigneeId = Number(assigneeInput.value || 0);
			if(action === 'schedule'){
				payload.publishAt = Number(publishAtInput && publishAtInput.value || 0);
				if(!payload.publishAt){ message({result:false,error:'publishAt is required'}); return; }
			}
			post(api('/workflow/action'), payload).then(function(ret){
				byId('detail').innerHTML = '<div class="x-form"><div class="x-muted">Workflow action result</div><pre class="x-code">' + esc(JSON.stringify(ret && ret.data ? ret.data : ret, null, 2)) + '</pre></div>';
				message(ret);
				reload();
			});
		}
		function runWorkflowScheduledPublish(){
			var limitInput = byId('workflowScheduledLimit');
			var limit = Number(limitInput && limitInput.value || 50);
			if(!limit || limit < 1) limit = 50;
			if(limit > 200) limit = 200;
			if(limitInput) limitInput.value = limit;
			post(api('/workflow/scheduled-publish'), {limit:limit}).then(function(ret){
				var data = ret && ret.data ? ret.data : ret;
				var target = byId('workflowScheduledResult') || byId('detail');
				target.innerHTML = esc(JSON.stringify(data, null, 2));
				message(ret);
				reload();
			});
		}
		function loadRows(){
			var cfg = currentConfig();
			var view = currentView();
			var url = view.listApi ? api(view.listApi) : api('/pack/list?pack=' + encodeURIComponent(packId));
			return get(url).then(function(ret){
				var rows = ret && ret.data ? ret.data : [];
				var ops = view.ops || cfg.ops || '';
				rows.forEach(function(row){ row.__ops = ops; });
				return rows;
			});
		}
		function renderTable(rows){
			var cfg = currentConfig();
			var view = currentView();
			layui.table.render({
				elem:tableSelector,
				data: rows,
				page:true,
				limit:20,
				cols:[scopedCols(view.cols || cfg.cols)],
				text:{none:'暂无数据'}
			});
		}
		function reload(){
			if(!packId){
				renderOverview();
				return;
			}
			byId('pageTitle').innerText = packTitles[packId] || packId;
			byId('pageSub').innerText = packId + ' / ' + pluginXid;
			renderEditor();
			get(api('/pack/meta?pack=' + encodeURIComponent(packId))).then(renderSummary);
			loadRows().then(renderTable);
		}
		function renderOverview(){
			byId('pageTitle').innerText = '能力包管理';
			byId('editor').innerHTML = '';
			get(api('/contracts')).then(function(ret){
				var packs = ret && ret.data && ret.data.contracts && ret.data.contracts.abilityPacks || [];
				byId('summary').innerHTML = '';
				byId('detail').innerHTML = '<div class="layui-row layui-col-space12">' + packs.map(function(p){
					return '<div class="layui-col-md4"><div class="layui-card"><div class="layui-card-header">'+esc(p.title || p.packId)+'</div><div class="layui-card-body"><div class="x-muted">'+esc(p.packId)+'</div><div class="x-code">'+esc(JSON.stringify(p.instanceConfig || {}, null, 2))+'</div></div></div></div>';
				}).join('') + '</div>';
			});
		}
		function simpleForm(fields, buttons){
			return '<div class="x-form"><form class="layui-form" id="abilityForm">' + fields.map(function(f){
				var control = f.type === 'textarea'
					? '<textarea name="'+f.name+'" placeholder="'+esc(f.placeholder||'')+'" class="layui-textarea">'+esc(f.value||'')+'</textarea>'
					: '<input name="'+f.name+'" type="'+(f.type||'text')+'" value="'+esc(f.value||'')+'" placeholder="'+esc(f.placeholder||'')+'" class="layui-input">';
				return '<div class="layui-form-item"><label class="layui-form-label">'+f.label+'</label><div class="layui-input-block">'+control+'</div></div>';
			}).join('') + '<div class="layui-form-item"><div class="layui-input-block">'+(buttons || '<button type="button" class="layui-btn layui-btn-sm" id="btnSaveAbility"><i class="layui-icon layui-icon-ok"></i> 保存</button>')+'</div></div></form></div>';
		}
		function viewSwitch(buttons){
			return '<div class="layui-btn-container" style="margin-bottom:10px">' + buttons.map(function(b){
				var active = (activeViews[packId] || 'main') === b.key;
				return '<button type="button" class="layui-btn layui-btn-sm '+(active ? '' : 'layui-btn-primary')+'" data-view="'+b.key+'">'+b.text+'</button>';
			}).join('') + '</div>';
		}
		function openRedirectDialog(row){
			row = row || {};
			var html = '<div class="managed-ability-dialog"><form class="layui-form" id="redirectForm_'+instanceKey+'">'
				+ '<input type="hidden" name="id" value="'+esc(row.id || 0)+'">'
				+ '<div class="layui-form-item"><label class="layui-form-label">来源路径</label><div class="layui-input-block"><input name="sourcePath" class="layui-input" value="'+esc(row.sourcePath || '')+'" placeholder="/old-url"></div></div>'
				+ '<div class="layui-form-item"><label class="layui-form-label">目标地址</label><div class="layui-input-block"><input name="targetUrl" class="layui-input" value="'+esc(row.targetUrl || '')+'" placeholder="/new-url 或 https://example.com"></div></div>'
				+ '<div class="layui-form-item"><label class="layui-form-label">状态码</label><div class="layui-input-block"><select name="statusCode"><option value="301" '+((row.statusCode || 301) == 301 ? 'selected' : '')+'>301 永久跳转</option><option value="302" '+(row.statusCode == 302 ? 'selected' : '')+'>302 临时跳转</option></select></div></div>'
				+ '<div class="layui-form-item"><label class="layui-form-label">启用</label><div class="layui-input-block"><input type="checkbox" name="statusEnabled" lay-skin="switch" lay-text="ON|OFF" '+(row.status === 0 ? '' : 'checked')+'></div></div>'
				+ '<div class="x-dialog-actions"><button type="button" class="layui-btn layui-btn-primary" id="btnRedirectCancel_'+instanceKey+'">取消</button><button type="button" class="layui-btn" id="btnRedirectSave_'+instanceKey+'">保存</button></div>'
				+ '</form></div>';
			var index = layer.open({type:1,title:row.id ? '编辑跳转规则' : '新增跳转规则',area:['560px','420px'],content:html,success:function(dom){
				layui.form.render();
				dom.find('#btnRedirectCancel_'+instanceKey).on('click', function(){ layer.close(index); });
				dom.find('#btnRedirectSave_'+instanceKey).on('click', function(){
					var form = dom.find('#redirectForm_'+instanceKey)[0];
					var data = formDataFromElement(form);
					data.status = form.elements.statusEnabled && form.elements.statusEnabled.checked ? 1 : 0;
					delete data.statusEnabled;
					post(api('/redirect/save'), data).then(function(ret){ message(ret); if(ret && ret.result !== false){ layer.close(index); reload(); } });
				});
			}});
		}
		function formDataFromElement(form){
			var data = {};
			Array.prototype.forEach.call(form ? form.elements : [], function(el){
				if(!el.name) return;
				if(el.type === 'number' || el.name === 'id' || el.name === 'statusCode') data[el.name] = Number(el.value || 0);
				else if(el.type === 'checkbox') data[el.name] = el.checked ? 1 : 0;
				else data[el.name] = el.value;
			});
			return data;
		}
		function openMediaDialog(row){
			row = row || {};
			var html = '<div class="managed-ability-dialog"><form class="layui-form" id="mediaForm_'+instanceKey+'">'
				+ '<input type="hidden" name="id" value="'+esc(row.id || 0)+'">'
				+ '<div class="layui-form-item"><label class="layui-form-label">标题</label><div class="layui-input-block"><input name="title" class="layui-input" value="'+esc(row.title || '')+'" placeholder="封面图"></div></div>'
				+ '<div class="layui-form-item"><label class="layui-form-label">URL</label><div class="layui-input-block"><input name="url" class="layui-input" value="'+esc(row.url || '')+'" placeholder="/uploads/demo.png"></div></div>'
				+ '<div class="layui-form-item"><label class="layui-form-label">MIME</label><div class="layui-input-block"><input name="mime" class="layui-input" value="'+esc(row.mime || '')+'" placeholder="image/png"></div></div>'
				+ '<div class="layui-form-item"><label class="layui-form-label">扩展名</label><div class="layui-input-block"><input name="ext" class="layui-input" value="'+esc(row.ext || '')+'" placeholder="png"></div></div>'
				+ '<div class="layui-form-item"><label class="layui-form-label">大小</label><div class="layui-input-block"><input name="size" type="number" class="layui-input" value="'+esc(row.size || 0)+'" placeholder="bytes"></div></div>'
				+ '<div class="layui-form-item"><label class="layui-form-label">尺寸</label><div class="layui-input-block" style="display:flex;gap:8px"><input name="width" type="number" class="layui-input" value="'+esc(row.width || 0)+'" placeholder="width"><input name="height" type="number" class="layui-input" value="'+esc(row.height || 0)+'" placeholder="height"><button type="button" class="layui-btn layui-btn-primary" id="btnMediaDetect_'+instanceKey+'">识别</button></div></div>'
				+ '<div class="layui-form-item"><label class="layui-form-label">启用</label><div class="layui-input-block"><input type="checkbox" name="statusEnabled" lay-skin="switch" lay-text="ON|OFF" '+(row.status === 0 ? '' : 'checked')+'></div></div>'
				+ '<div class="x-dialog-actions"><button type="button" class="layui-btn layui-btn-primary" id="btnMediaCancel_'+instanceKey+'">取消</button><button type="button" class="layui-btn" id="btnMediaSave_'+instanceKey+'">保存</button></div>'
				+ '</form></div>';
			var index = layer.open({type:1,title:row.id ? '编辑媒体资源' : '新增媒体资源',area:['620px','620px'],content:html,success:function(dom){
				function detectImageSize(){
					var form = dom.find('#mediaForm_'+instanceKey)[0];
					var url = form && form.elements.url ? form.elements.url.value : '';
					if(!url){ message({result:false,error:'url is required'}); return; }
					var img = new Image();
					img.onload = function(){
						form.elements.width.value = img.naturalWidth || img.width || 0;
						form.elements.height.value = img.naturalHeight || img.height || 0;
						message({result:true,message:'image size detected'});
					};
					img.onerror = function(){ message({result:false,error:'image size detect failed'}); };
					img.src = (/^(https?:)?\/\//i.test(url) || url.charAt(0) === '/') ? url : '/' + url;
				}
				layui.form.render();
				dom.find('#btnMediaCancel_'+instanceKey).on('click', function(){ layer.close(index); });
				dom.find('#btnMediaDetect_'+instanceKey).on('click', detectImageSize);
				dom.find('#btnMediaSave_'+instanceKey).on('click', function(){
					var form = dom.find('#mediaForm_'+instanceKey)[0];
					var data = formDataFromElement(form);
					data.status = form.elements.statusEnabled && form.elements.statusEnabled.checked ? 1 : 0;
					delete data.statusEnabled;
					post(api('/media/save'), data).then(function(ret){ message(ret); if(ret && ret.result !== false){ layer.close(index); reload(); } });
				});
			}});
		}
		function openSeoDialog(row){
			row = row || {};
			var html = '<div class="managed-ability-dialog"><form class="layui-form" id="seoForm_'+instanceKey+'">'
				+ '<input type="hidden" name="id" value="'+esc(row.id || 0)+'">'
				+ '<div class="layui-form-item"><label class="layui-form-label">Content ID</label><div class="layui-input-block"><input name="contentId" type="number" class="layui-input" value="'+esc(row.contentId || '')+'" placeholder="content id"></div></div>'
				+ '<div class="layui-form-item"><label class="layui-form-label">SEO Title</label><div class="layui-input-block"><input name="seoTitle" class="layui-input" value="'+esc(row.seoTitle || '')+'" placeholder="title override"></div></div>'
				+ '<div class="layui-form-item"><label class="layui-form-label">Keywords</label><div class="layui-input-block"><input name="seoKeywords" class="layui-input" value="'+esc(row.seoKeywords || '')+'" placeholder="keyword1,keyword2"></div></div>'
				+ '<div class="layui-form-item"><label class="layui-form-label">Description</label><div class="layui-input-block"><textarea name="seoDescription" class="layui-textarea" placeholder="meta description">'+esc(row.seoDescription || '')+'</textarea></div></div>'
				+ '<div class="layui-form-item"><label class="layui-form-label">Canonical</label><div class="layui-input-block"><input name="canonical" class="layui-input" value="'+esc(row.canonical || '')+'" placeholder="/plugin/{{PLUGIN_XID}}?id=1"></div></div>'
				+ '<div class="layui-form-item"><label class="layui-form-label">Enabled</label><div class="layui-input-block"><input type="checkbox" name="statusEnabled" lay-skin="switch" lay-text="ON|OFF" '+(row.status === 0 ? '' : 'checked')+'></div></div>'
				+ '<div class="x-dialog-actions"><button type="button" class="layui-btn layui-btn-primary" id="btnSeoCancel_'+instanceKey+'">Cancel</button><button type="button" class="layui-btn" id="btnSeoSave_'+instanceKey+'">Save</button></div>'
				+ '</form></div>';
			var index = layer.open({type:1,title:row.id ? 'Edit SEO Meta' : 'Add SEO Meta',area:['680px','560px'],content:html,success:function(dom){
				layui.form.render();
				dom.find('#btnSeoCancel_'+instanceKey).on('click', function(){ layer.close(index); });
				dom.find('#btnSeoSave_'+instanceKey).on('click', function(){
					var form = dom.find('#seoForm_'+instanceKey)[0];
					var data = formDataFromElement(form);
					data.id = Number(data.id || 0);
					data.contentId = Number(data.contentId || 0);
					data.status = form.elements.statusEnabled && form.elements.statusEnabled.checked ? 1 : 0;
					delete data.statusEnabled;
					post(api('/seo/save'), data).then(function(ret){ message(ret); if(ret && ret.result !== false){ layer.close(index); reload(); } });
				});
			}});
		}
		var configs = {
			overview:{cols:[{field:'packId',title:'能力包'}]},
			'content.seo':{
				ops:'edit,delete',
				listApi:'/seo/list',
				cols:[{field:'id',title:'ID',width:80},{field:'contentId',title:'Content ID',width:110},{field:'contentTitle',title:'Content Title',minWidth:180},{field:'seoTitle',title:'SEO Title',minWidth:180},{field:'seoKeywords',title:'Keywords',minWidth:160},{field:'canonical',title:'Canonical',minWidth:220},{field:'status',title:'Enabled',width:90},{field:'updateTimeText',title:'Updated At',width:170},{title:'Actions',toolbar:'#rowActions',width:140}],
				form:function(){return '<div class="x-toolbar"><div class="x-muted">SEO meta only loads when content.seo is enabled. Content save/import keeps this table in sync, and this manager is for manual overrides.</div><div class="layui-btn-container"><button type="button" class="layui-btn layui-btn-sm" id="btnAddSeo"><i class="layui-icon layui-icon-add-1"></i> Add Meta</button></div></div>';}
			},
			'content.comment':{
				ops:'audit,hide,delete',
				cols:[{field:'id',title:'ID',width:80},{field:'content_id',title:'内容ID',width:100},{field:'author_name',title:'作者',width:140},{field:'body',title:'评论内容'},{field:'status',title:'状态',width:90},{field:'create_time',title:'创建时间',width:160},{title:'操作',toolbar:'#rowActions',width:140}],
				views:{
					main:{cols:[{field:'id',title:'ID',width:80},{field:'content_id',title:'内容ID',width:100},{field:'author_name',title:'作者',width:140},{field:'body',title:'评论内容'},{field:'status',title:'状态',width:90},{field:'create_time',title:'创建时间',width:160},{title:'操作',toolbar:'#rowActions',width:140}]},
					audit:{listApi:'/pack/list?pack=content.comment&status=0',ops:'audit,hide,delete',cols:[{field:'id',title:'ID',width:80},{field:'content_id',title:'内容ID',width:100},{field:'author_name',title:'作者',width:140},{field:'body',title:'待审评论'},{field:'status',title:'状态',width:90},{field:'create_time',title:'提交时间',width:160},{title:'审核',toolbar:'#rowActions',width:140}]}
				},
				form:function(){return viewSwitch([{key:'main',text:'全部评论'},{key:'audit',text:'审核'}]) + '<div class="x-form x-muted">评论由前端接口提交，可在列表中审核通过、驳回或隐藏。</div>';}
			},
			'content.tag':{
				ops:'delete',
				saveApi:'/tag/save',
				cols:[{field:'id',title:'ID',width:80},{field:'name',title:'标签名'},{field:'slug',title:'别名'},{field:'content_count',title:'内容数',width:100},{field:'status',title:'状态',width:90},{title:'操作',toolbar:'#rowActions',width:100}],
				views:{
					main:{cols:[{field:'id',title:'ID',width:80},{field:'name',title:'标签名'},{field:'slug',title:'别名'},{field:'content_count',title:'内容数',width:100},{field:'status',title:'状态',width:90},{title:'操作',toolbar:'#rowActions',width:100}]},
					contents:{listApi:'/tag/content/list', ops:'unbind', cols:[{field:'id',title:'ID',width:80},{field:'tagId',title:'标签ID',width:100},{field:'tagName',title:'标签名',width:160},{field:'contentId',title:'内容ID',width:100},{field:'contentTitle',title:'内容标题'},{field:'sort',title:'排序',width:90},{field:'createTime',title:'绑定时间',width:160},{title:'操作',toolbar:'#rowActions',width:100}]}
				},
				form:function(){return viewSwitch([{key:'main',text:'标签'},{key:'contents',text:'内容关联'}]) + simpleForm([{name:'name',label:'标签名',placeholder:'请输入标签名'},{name:'slug',label:'别名',placeholder:'例如 news'},{name:'status',label:'状态',type:'number',value:'1'}]);}
			},
			'content.topic':{
				ops:'delete',
				saveApi:'/topic/save',
				cols:[{field:'id',title:'ID',width:80},{field:'title',title:'专题标题'},{field:'slug',title:'别名'},{field:'content_count',title:'内容数',width:100},{field:'status',title:'状态',width:90},{title:'操作',toolbar:'#rowActions',width:100}],
				views:{
					main:{cols:[{field:'id',title:'ID',width:80},{field:'title',title:'专题标题'},{field:'slug',title:'别名'},{field:'content_count',title:'内容数',width:100},{field:'status',title:'状态',width:90},{title:'操作',toolbar:'#rowActions',width:100}]},
					contents:{listApi:'/topic/content/list', ops:'unbind', cols:[{field:'id',title:'ID',width:80},{field:'topicId',title:'专题ID',width:100},{field:'topicTitle',title:'专题标题',width:180},{field:'contentId',title:'内容ID',width:100},{field:'contentTitle',title:'内容标题'},{field:'sort',title:'排序',width:90},{field:'createTime',title:'绑定时间',width:160},{title:'操作',toolbar:'#rowActions',width:100}]}
				},
				form:function(){return viewSwitch([{key:'main',text:'专题'},{key:'contents',text:'内容关联'}]) + simpleForm([{name:'title',label:'专题标题',placeholder:'请输入专题标题'},{name:'slug',label:'别名',placeholder:'例如 product'},{name:'status',label:'状态',type:'number',value:'1'}]);}
			},
			'content.sensitive':{
				ops:'delete',
				saveApi:'/sensitive/word/save',
				cols:[{field:'id',title:'ID',width:80},{field:'word',title:'敏感词'},{field:'level',title:'级别',width:90},{field:'scope',title:'作用域',width:120},{field:'replacement',title:'替换词'},{field:'status',title:'状态',width:90},{title:'操作',toolbar:'#rowActions',width:100}],
				views:{
					main:{cols:[{field:'id',title:'ID',width:80},{field:'word',title:'敏感词'},{field:'level',title:'级别',width:90},{field:'scope',title:'作用域',width:120},{field:'replacement',title:'替换词'},{field:'status',title:'状态',width:90},{title:'操作',toolbar:'#rowActions',width:100}]},
					logs:{listApi:'/sensitive/log/list', cols:[{field:'id',title:'ID',width:80},{field:'targetType',title:'目标类型',width:110},{field:'targetId',title:'目标ID',width:100},{field:'word',title:'命中词'},{field:'fieldName',title:'字段',width:120},{field:'action',title:'动作',width:100},{field:'createTime',title:'命中时间',width:160}]}
				},
				form:function(){return viewSwitch([{key:'main',text:'词库'},{key:'logs',text:'命中日志'}]) + simpleForm([{name:'word',label:'敏感词',placeholder:'请输入敏感词'},{name:'level',label:'级别',type:'number',value:'1'},{name:'scope',label:'作用域',value:'content'},{name:'replacement',label:'替换词',value:'***'},{name:'status',label:'状态',type:'number',value:'1'}]);}
			},
			'content.slug':{
				ops:'',
				listApi:'/slug/history',
				cols:[{field:'id',title:'ID',width:80},{field:'contentId',title:'Content ID',width:110},{field:'oldSlug',title:'Old Slug',minWidth:180},{field:'newSlug',title:'New Slug',minWidth:180},{field:'status',title:'Status',width:90},{field:'createTimeText',title:'Changed At',width:170}],
				form:function(){return '<div class="x-toolbar"><div><div class="x-muted">Slug only loads when content.slug is enabled; it provides save-time uniqueness checks, URL preview, change history and batch repair.</div><div style="display:flex;gap:8px;margin-top:8px;max-width:720px"><input id="slugProbe" class="layui-input" placeholder="slug"><input id="slugProbeId" class="layui-input" style="width:140px" placeholder="exclude id"></div><pre id="slugProbeResult" class="x-code" style="margin-top:8px"></pre></div><div class="layui-btn-container"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnSlugCheck"><i class="layui-icon layui-icon-search"></i> Check</button><button type="button" class="layui-btn layui-btn-sm" id="btnSlugPreview"><i class="layui-icon layui-icon-link"></i> Preview URL</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnSlugRepairPreview"><i class="layui-icon layui-icon-list"></i> Preview Repair</button><button type="button" class="layui-btn layui-btn-danger layui-btn-sm" id="btnSlugRepairConfirm"><i class="layui-icon layui-icon-ok"></i> Apply Repair</button></div></div>';}
			},
			'content.redirect':{
				ops:'edit,delete',
				listApi:'/redirect/list',
				cols:[{field:'id',title:'ID',width:80},{field:'sourcePath',title:'来源路径',minWidth:220},{field:'targetUrl',title:'目标地址',minWidth:260},{field:'statusCode',title:'状态码',width:90},{field:'hitCount',title:'命中',width:90},{field:'lastHitTimeText',title:'最后命中',width:160},{field:'status',title:'启用',width:80},{title:'操作',toolbar:'#rowActions',width:140}],
				form:function(){return '<div class="x-toolbar"><div class="x-muted">跳转规则只在启用 content.redirect 后加载，不接管全局路由。</div><div class="layui-btn-container"><button type="button" class="layui-btn layui-btn-sm" id="btnAddRedirect"><i class="layui-icon layui-icon-add-1"></i> 新增规则</button></div></div>';}
			},
			'content.media':{
				ops:'edit,delete',
				listApi:'/media/list',
				cols:[{field:'id',title:'ID',width:80},{field:'title',title:'标题',minWidth:160},{field:'url',title:'URL',minWidth:260},{field:'mime',title:'MIME',width:130},{field:'size',title:'大小',width:100},{field:'width',title:'宽',width:80},{field:'height',title:'高',width:80},{field:'status',title:'启用',width:80},{field:'updateTimeText',title:'更新时间',width:160},{title:'操作',toolbar:'#rowActions',width:140}],
				form:function(){return '<div class="x-toolbar"><div class="x-muted">媒体能力包管理内容资源登记和引用关系；真实上传复用系统附件能力。</div><div class="layui-btn-container"><button type="button" class="layui-btn layui-btn-sm" id="btnAddMedia"><i class="layui-icon layui-icon-add-1"></i> 新增资源</button></div></div>';}
			},
			'content.revision':{
				ops:'',
				listApi:'/revision/list',
				cols:[{field:'id',title:'ID',width:80},{field:'contentId',title:'内容ID',width:100},{field:'revisionNo',title:'版本号',width:100},{field:'title',title:'标题',minWidth:180},{field:'status',title:'状态',width:90},{field:'isDraft',title:'草稿',width:80},{field:'action',title:'动作',width:100},{field:'createTimeText',title:'创建时间',width:170}],
				form:function(){return '<div class="x-toolbar"><div class="x-muted">内容保存时自动生成版本快照；恢复版本请通过内容编辑页的版本入口执行。</div></div>';}
			},
			'content.workflow':{
				ops:'',
				listApi:'/workflow/log/list',
				cols:[{field:'id',title:'ID',width:80},{field:'contentId',title:'内容ID',width:100},{field:'action',title:'动作',width:100},{field:'fromStatus',title:'原状态',width:90},{field:'toStatus',title:'新状态',width:90},{field:'fromDraft',title:'原草稿',width:90},{field:'toDraft',title:'新草稿',width:90},{field:'assigneeId',title:'Assignee',width:100},{field:'reason',title:'原因',minWidth:180},{field:'createTimeText',title:'时间',width:170}],
				views:{
					main:{ops:'',listApi:'/workflow/log/list',cols:[{field:'id',title:'ID',width:80},{field:'contentId',title:'内容ID',width:100},{field:'action',title:'动作',width:100},{field:'fromStatus',title:'原状态',width:90},{field:'toStatus',title:'新状态',width:90},{field:'fromDraft',title:'原草稿',width:90},{field:'toDraft',title:'新草稿',width:90},{field:'assigneeId',title:'Assignee',width:100},{field:'reason',title:'原因',minWidth:180},{field:'createTimeText',title:'时间',width:170}]},
					todo:{ops:'',listApi:'/workflow/todo/list',cols:[{field:'id',title:'内容ID',width:100},{field:'title',title:'标题',minWidth:200},{field:'status',title:'状态',width:90},{field:'isDraft',title:'草稿',width:80},{field:'assigneeId',title:'Assignee',width:100},{field:'lastAction',title:'最后动作',width:110},{field:'lastReason',title:'最后原因',minWidth:180},{field:'logTimeText',title:'流转时间',width:170},{field:'updateTimeText',title:'更新时间',width:170}]}
				},
				form:function(){return viewSwitch([{key:'main',text:'流转日志'},{key:'todo',text:'审核待办'}]) + '<div class="x-toolbar"><div><div class="x-muted">Workflow actions are registered only when content.workflow is enabled. Assignee records reviewer handoff; publishAt is a unix timestamp for scheduled publish. Due-run limit is clamped to 1-200.</div><div style="display:flex;gap:8px;margin-top:8px;max-width:1080px"><input id="workflowContentId" class="layui-input" style="width:140px" placeholder="content id"><input id="workflowAssigneeId" class="layui-input" style="width:150px" placeholder="assignee id"><input id="workflowPublishAt" class="layui-input" style="width:190px" placeholder="publishAt timestamp"><input id="workflowScheduledLimit" class="layui-input" type="number" min="1" max="200" style="width:120px" value="50" placeholder="due limit"><input id="workflowReason" class="layui-input" placeholder="reason"></div><pre id="workflowScheduledResult" class="x-code" style="margin-top:8px"></pre></div><div class="layui-btn-container"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnWorkflowSubmit"><i class="layui-icon layui-icon-upload"></i> Submit</button><button type="button" class="layui-btn layui-btn-sm" id="btnWorkflowApprove"><i class="layui-icon layui-icon-ok"></i> Approve</button><button type="button" class="layui-btn layui-btn-normal layui-btn-sm" id="btnWorkflowSchedule"><i class="layui-icon layui-icon-date"></i> Schedule</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnWorkflowRunScheduled"><i class="layui-icon layui-icon-refresh"></i> Run Due</button><button type="button" class="layui-btn layui-btn-warm layui-btn-sm" id="btnWorkflowReject"><i class="layui-icon layui-icon-close"></i> Reject</button><button type="button" class="layui-btn layui-btn-danger layui-btn-sm" id="btnWorkflowOffline"><i class="layui-icon layui-icon-down"></i> Offline</button></div></div>';}
			},
			'content.search':{
				ops:'',
				listApi:'/search',
				cols:[{field:'id',title:'ID',width:80},{field:'title',title:'标题',minWidth:180},{field:'slug',title:'Slug',width:160},{field:'summary',title:'摘要',minWidth:220},{field:'status',title:'状态',width:90},{field:'isDraft',title:'草稿',width:80},{field:'updateTimeText',title:'更新时间',width:170}],
				form:function(){return '<div class="x-toolbar"><div class="x-muted">搜索能力使用 q 参数检索标题、slug、摘要和 searchable 字段；生产级索引将在后续补强。</div></div>';}
			},
			'content.sitemap':{
				ops:'',
				listApi:'/sitemap/entry/list',
				cols:[{field:'contentId',title:'内容ID',width:100},{field:'title',title:'标题',minWidth:180},{field:'loc',title:'URL',minWidth:260},{field:'changefreq',title:'更新频率',width:110},{field:'priority',title:'权重',width:90},{field:'updateTimeText',title:'更新时间',width:170}],
				form:function(){return '<div class="x-toolbar"><div class="x-muted">站点地图能力启用后提供 sitemap.xml、rss.xml 和 robots.txt；当前按已发布内容实时输出，增量生成将在后续补强。</div><div class="layui-btn-container"><a class="layui-btn layui-btn-primary layui-btn-sm" target="_blank" href="'+pub('/sitemap.xml')+'">sitemap.xml</a><a class="layui-btn layui-btn-primary layui-btn-sm" target="_blank" href="'+pub('/rss.xml')+'">rss.xml</a><a class="layui-btn layui-btn-primary layui-btn-sm" target="_blank" href="'+pub('/robots.txt')+'">robots.txt</a></div></div>';}
			},
			'content.related':{
				ops:'delete',
				saveApi:'/related/save',
				listApi:'/related/list',
				cols:[{field:'id',title:'ID',width:80},{field:'sourceContentId',title:'源内容ID',width:110},{field:'sourceTitle',title:'源标题',minWidth:160},{field:'relatedContentId',title:'关联内容ID',width:120},{field:'relatedTitle',title:'关联标题',minWidth:160},{field:'relationType',title:'类型',width:100},{field:'weight',title:'权重',width:90},{field:'status',title:'状态',width:80},{field:'updateTimeText',title:'更新时间',width:170},{title:'操作',toolbar:'#rowActions',width:100}],
				form:function(){return simpleForm([{name:'sourceContentId',label:'源内容ID',type:'number',placeholder:'例如 1'},{name:'relatedContentId',label:'关联内容ID',type:'number',placeholder:'例如 2'},{name:'relationType',label:'类型',value:'manual',placeholder:'manual 或 rule'},{name:'weight',label:'权重',type:'number',value:'0'},{name:'status',label:'状态',type:'number',value:'1'}]);}
			},
			'content.form':{
				ops:'delete',
				saveApi:'/form/save',
				listApi:'/form/list',
				cols:[{field:'id',title:'ID',width:80},{field:'contentId',title:'内容ID',width:100},{field:'formKey',title:'表单Key',width:160},{field:'title',title:'标题',minWidth:180},{field:'schemaJson',title:'Schema',minWidth:260},{field:'status',title:'状态',width:80},{field:'updateTimeText',title:'更新时间',width:170},{title:'操作',toolbar:'#rowActions',width:100}],
				views:{
					main:{listApi:'/form/list', cols:[{field:'id',title:'ID',width:80},{field:'contentId',title:'内容ID',width:100},{field:'formKey',title:'表单Key',width:160},{field:'title',title:'标题',minWidth:180},{field:'schemaJson',title:'Schema',minWidth:260},{field:'status',title:'状态',width:80},{field:'updateTimeText',title:'更新时间',width:170},{title:'操作',toolbar:'#rowActions',width:100}]},
					submissions:{listApi:'/form/submission/list', ops:'', cols:[{field:'id',title:'ID',width:80},{field:'formId',title:'表单ID',width:90},{field:'formKey',title:'表单Key',width:140},{field:'formTitle',title:'表单标题',width:160},{field:'contentId',title:'内容ID',width:100},{field:'dataJson',title:'提交数据',minWidth:300},{field:'ip',title:'IP',width:120},{field:'status',title:'状态',width:80},{field:'createTimeText',title:'提交时间',width:170}]}
				},
				form:function(){return viewSwitch([{key:'main',text:'表单'},{key:'submissions',text:'提交'}]) + simpleForm([{name:'contentId',label:'内容ID',type:'number',value:'0'},{name:'formKey',label:'表单Key',placeholder:'contact'},{name:'title',label:'标题',placeholder:'联系表单'},{name:'schemaJson',label:'Schema',value:'{}',placeholder:'JSON schema'},{name:'status',label:'状态',type:'number',value:'1'}]);}
			},
			'content.access':{
				ops:'delete',
				saveApi:'/access/rule/save',
				listApi:'/access/rule/list',
				cols:[{field:'id',title:'ID',width:80},{field:'contentId',title:'内容ID',width:100},{field:'contentTitle',title:'标题',minWidth:180},{field:'accessMode',title:'模式',width:110},{field:'requiredReadLevel',title:'阅读等级',width:110},{field:'memberGroupIds',title:'会员组',width:140},{field:'price',title:'价格',width:90},{field:'status',title:'状态',width:80},{field:'updateTimeText',title:'更新时间',width:170},{title:'操作',toolbar:'#rowActions',width:100}],
				form:function(){return simpleForm([{name:'contentId',label:'内容ID',type:'number',placeholder:'例如 1'},{name:'accessMode',label:'模式',value:'public',placeholder:'public/login/level/group/password/private'},{name:'requiredReadLevel',label:'阅读等级',type:'number',value:'0'},{name:'memberGroupIds',label:'会员组',placeholder:'group 模式，例如 1,2'},{name:'password',label:'密码',placeholder:'password 模式使用'},{name:'price',label:'价格',type:'number',value:'0'},{name:'status',label:'状态',type:'number',value:'1'}]);}
			},
			'content.audit-log':{
				ops:'',
				listApi:'/audit-log/list',
				cols:[{field:'id',title:'ID',width:80},{field:'targetType',title:'对象类型',width:120},{field:'targetId',title:'对象ID',width:100},{field:'action',title:'动作',width:150},{field:'summary',title:'摘要',minWidth:200},{field:'operatorType',title:'操作者类型',width:120},{field:'operatorId',title:'操作者ID',width:110},{field:'createTimeText',title:'时间',width:170}],
				form:function(){return '<div class="x-toolbar"><div class="x-muted">审计日志由内容保存、删除、版本恢复、工作流和权限规则变更自动写入；当前页面只提供最近记录查看。</div></div>';}
			},
			'content.import-export':{
				ops:'',
				listApi:'/import-export/import/jobs',
				views:{
					imports:{listApi:'/import-export/import/jobs', cols:[{field:'id',title:'ID',width:80},{field:'sourceName',title:'来源',minWidth:180},{field:'status',title:'状态',width:100},{field:'totalCount',title:'总数',width:90},{field:'successCount',title:'通过',width:90},{field:'failCount',title:'失败',width:90},{field:'reportJson',title:'报告',minWidth:260},{field:'createTimeText',title:'创建时间',width:170}]},
					exports:{listApi:'/import-export/export/jobs', cols:[{field:'id',title:'ID',width:80},{field:'exportType',title:'类型',width:100},{field:'status',title:'状态',width:100},{field:'totalCount',title:'总数',width:90},{field:'filterJson',title:'筛选',minWidth:180},{field:'resultJson',title:'结果',minWidth:260},{field:'createTimeText',title:'创建时间',width:170}]}
				},
				form:function(){return viewSwitch([{key:'imports',text:'导入预检'},{key:'exports',text:'导出任务'}]) + '<div class="x-toolbar"><div><div class="x-muted">当前支持字段白名单、JSON 导出、导入预检和确认导入；确认导入需要调用 commit API 并显式传 confirm=true。</div><div style="display:flex;gap:8px;margin-top:8px;max-width:720px"><input id="importExportFields" class="layui-input" style="width:360px" placeholder="字段白名单，逗号分隔；留空表示全部字段"><input id="importExportLimit" class="layui-input" style="width:120px" placeholder="limit"><input id="importExportOffset" class="layui-input" style="width:120px" placeholder="offset"></div><pre id="importExportResult" class="x-code" style="margin-top:8px"></pre></div><div class="layui-btn-container"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnImportPreview"><i class="layui-icon layui-icon-list"></i> 预检示例</button><button type="button" class="layui-btn layui-btn-sm" id="btnExportJson"><i class="layui-icon layui-icon-export"></i> 导出 JSON</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnExportJsonNext"><i class="layui-icon layui-icon-next"></i> 下一片</button></div></div>';}
			},
			'content.static':{
				ops:'delete',
				saveApi:'/static/rule/save',
				cols:[{field:'id',title:'ID',width:80},{field:'name',title:'规则名'},{field:'pathPattern',title:'路径规则'},{field:'templateName',title:'模板'},{field:'status',title:'状态',width:90},{title:'操作',toolbar:'#rowActions',width:100}],
				listApi:'/static/rule/list',
				views:{
					main:{listApi:'/static/rule/list', cols:[{field:'id',title:'ID',width:80},{field:'name',title:'规则名'},{field:'pathPattern',title:'路径规则'},{field:'templateName',title:'模板'},{field:'status',title:'状态',width:90},{title:'操作',toolbar:'#rowActions',width:100}]},
					tasks:{listApi:'/static/task/list', cols:[{field:'id',title:'ID',width:80},{field:'ruleId',title:'规则ID',width:100},{field:'targetId',title:'内容ID',width:100},{field:'status',title:'状态',width:90},{field:'message',title:'消息'},{field:'createTime',title:'创建时间',width:160},{field:'finishTime',title:'完成时间',width:160}]},
					artifacts:{listApi:'/static/artifact/list', cols:[{field:'id',title:'ID',width:80},{field:'ruleId',title:'规则ID',width:100},{field:'targetId',title:'内容ID',width:100},{field:'path',title:'输出路径'},{field:'hash',title:'Hash',width:120},{field:'updateTime',title:'更新时间',width:160}]}
				},
				form:function(){return viewSwitch([{key:'main',text:'规则'},{key:'tasks',text:'任务'},{key:'artifacts',text:'产物'}]) + simpleForm([{name:'name',label:'规则名',placeholder:'详情页静态化'},{name:'pathPattern',label:'路径规则',placeholder:'/article/{id}.html'},{name:'templateName',label:'模板',placeholder:'detail'},{name:'status',label:'状态',type:'number',value:'1'}]) + '<div class="x-form"><form class="layui-form" id="staticGenerateForm"><div class="layui-form-item"><label class="layui-form-label">内容ID</label><div class="layui-input-block"><input name="targetId" type="number" class="layui-input" value="0"></div></div><div class="layui-form-item"><label class="layui-form-label">规则ID</label><div class="layui-input-block"><input name="ruleId" type="number" class="layui-input" value="0"></div></div><div class="layui-form-item"><label class="layui-form-label">输出路径</label><div class="layui-input-block"><input name="path" class="layui-input" placeholder="/article/1.html"></div></div><div class="layui-form-item"><div class="layui-input-block"><button type="button" class="layui-btn layui-btn-normal layui-btn-sm" id="btnStaticGenerate"><i class="layui-icon layui-icon-release"></i> 生成静态页</button></div></div></form></div>';}
			},
			'content.like':{
				listApi:'/like/counter/list',
				cols:[{field:'contentId',title:'内容ID',width:120},{field:'likeCount',title:'点赞数',width:120},{field:'updateTime',title:'更新时间'}],
				views:{
					main:{listApi:'/like/counter/list', cols:[{field:'contentId',title:'内容ID',width:120},{field:'likeCount',title:'点赞数',width:120},{field:'updateTime',title:'更新时间'}]},
					records:{listApi:'/like/list', cols:[{field:'id',title:'ID',width:80},{field:'contentId',title:'内容ID',width:100},{field:'actorId',title:'用户ID',width:120},{field:'actorKey',title:'去重键'},{field:'ip',title:'IP',width:140},{field:'status',title:'状态',width:90},{field:'updateTime',title:'更新时间',width:160}]}
				},
				form:function(){return viewSwitch([{key:'main',text:'计数'},{key:'records',text:'记录'}]) + '<div class="x-toolbar"><div class="x-muted">点赞通过前端接口记录，后台可查看内容维度计数、明细记录和汇总统计。</div><div class="layui-btn-container"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnLikeStats"><i class="layui-icon layui-icon-chart"></i> Stats</button></div></div>';}
			},
			'content.view-stat':{
				listApi:'/view/counter/list',
				cols:[{field:'contentId',title:'内容ID',width:120},{field:'viewCount',title:'访问量',width:120},{field:'uniqueViewCount',title:'独立访问',width:120},{field:'lastViewTime',title:'最后访问时间'}],
				views:{
					main:{listApi:'/view/counter/list', cols:[{field:'contentId',title:'内容ID',width:120},{field:'viewCount',title:'访问量',width:120},{field:'uniqueViewCount',title:'独立访问',width:120},{field:'lastViewTime',title:'最后访问时间'}]},
					logs:{listApi:'/view/log/list', cols:[{field:'id',title:'ID',width:80},{field:'contentId',title:'内容ID',width:100},{field:'visitorKey',title:'访客标识'},{field:'ip',title:'IP',width:140},{field:'referer',title:'来源'},{field:'createTime',title:'访问时间',width:160}]},
					daily:{listApi:'/view/daily/list', cols:[{field:'id',title:'ID',width:80},{field:'contentId',title:'内容ID',width:100},{field:'statDate',title:'日期',width:130},{field:'viewCount',title:'访问量',width:120},{field:'uniqueViewCount',title:'独立访问',width:120}]}
				},
				form:function(){return viewSwitch([{key:'main',text:'计数'},{key:'logs',text:'日志'},{key:'daily',text:'日统计'}]) + '<div class="x-toolbar"><div class="x-muted">访问统计通过前端接口记录，后台可查看内容计数、访问日志、日统计和汇总统计。</div><div class="layui-btn-container"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnViewStats"><i class="layui-icon layui-icon-chart"></i> Stats</button></div></div>';}
			}
		};
		if(configs['content.import-export'] && configs['content.import-export'].views && configs['content.import-export'].views.exports){
			var importExportBaseForm = configs['content.import-export'].form;
			configs['content.import-export'].form = function(){
				return importExportBaseForm() + '<div class="x-toolbar"><div><div class="x-muted">Import conflictMode supports insert, update, and skip. Update/skip match by id or contentId. Replay loads failed rows from an import job into the JSON editor. Chunked import splits the JSON array into bounded API calls.</div><div style="display:flex;gap:8px;max-width:540px;margin-top:8px"><input id="importExportConflictMode" class="layui-input" style="width:160px" value="insert" placeholder="insert/update/skip"><input id="importReplayJobId" class="layui-input" style="width:160px" placeholder="Replay job ID"><input id="importExportChunkSize" class="layui-input" type="number" min="1" max="200" style="width:140px" value="50" placeholder="chunk size"></div><textarea id="importExportItems" class="layui-textarea" style="width:520px;margin-top:8px" placeholder="JSON array items">[{}]</textarea></div><div class="layui-btn-container"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnImportReplay"><i class="layui-icon layui-icon-refresh"></i> Replay Failed</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnImportManualPreview"><i class="layui-icon layui-icon-list"></i> Preview JSON</button><button type="button" class="layui-btn layui-btn-normal layui-btn-sm" id="btnImportManualCommit"><i class="layui-icon layui-icon-upload"></i> Confirm Import</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnImportChunkPreview"><i class="layui-icon layui-icon-template-1"></i> Chunk Preview</button><button type="button" class="layui-btn layui-btn-normal layui-btn-sm" id="btnImportChunkCommit"><i class="layui-icon layui-icon-upload-drag"></i> Chunk Import</button></div></div>';
			};
			configs['content.import-export'].views.exports.ops = 'download';
			configs['content.import-export'].views.exports.cols.push({title:'Actions',toolbar:'#rowActions',width:100});
		}
		if(configs['content.access']){
			configs['content.access'].cols = [{field:'id',title:'ID',width:80},{field:'targetType',title:'Target',width:100},{field:'targetId',title:'Target ID',width:100},{field:'contentTitle',title:'Title/Category',minWidth:180},{field:'accessMode',title:'Mode',width:110},{field:'requiredReadLevel',title:'Read Level',width:110},{field:'memberGroupIds',title:'Groups',width:140},{field:'price',title:'Price',width:90},{field:'status',title:'Status',width:80},{field:'updateTimeText',title:'Updated At',width:170},{title:'Actions',toolbar:'#rowActions',width:100}];
			configs['content.access'].form = function(){return simpleForm([{name:'targetType',label:'Target Type',value:'content',placeholder:'content/category'},{name:'targetId',label:'Target ID',type:'number',placeholder:'content id or category id'},{name:'contentId',label:'Content ID',type:'number',placeholder:'optional when targetType=content'},{name:'categoryId',label:'Category ID',type:'number',placeholder:'optional when targetType=category'},{name:'accessMode',label:'Mode',value:'public',placeholder:'public/login/level/group/password/paid/private'},{name:'requiredReadLevel',label:'Read Level',type:'number',value:'0'},{name:'memberGroupIds',label:'Groups',placeholder:'for group mode, e.g. 1,2'},{name:'password',label:'Password',placeholder:'for password mode'},{name:'price',label:'Price',type:'number',value:'0'},{name:'status',label:'Status',type:'number',value:'1'}]);};
		}
		if(configs['content.audit-log']){
			configs['content.audit-log'].cols = [{field:'id',title:'ID',width:80},{field:'targetType',title:'Target Type',width:120},{field:'targetId',title:'Target ID',width:100},{field:'action',title:'Action',width:150},{field:'summary',title:'Summary',minWidth:200},{field:'operatorType',title:'Operator Type',width:120},{field:'operatorId',title:'Operator ID',width:110},{field:'ip',title:'IP',width:130},{field:'createTimeText',title:'Time',width:170}];
			configs['content.audit-log'].form = function(){return '<div class="x-toolbar"><div><div class="x-muted">Audit logs are written by content save, delete, revision restore, workflow, and rule changes.</div><input id="auditKeepDays" type="number" min="1" class="layui-input" style="width:180px;margin-top:8px" value="180" placeholder="Keep days"></div><div class="layui-btn-container"><button type="button" class="layui-btn layui-btn-danger layui-btn-sm" id="btnAuditCleanup"><i class="layui-icon layui-icon-delete"></i> Cleanup</button></div></div>';};
		}
		if(configs['content.redirect']){
			configs['content.redirect'].cols = [{field:'id',title:'ID',width:80},{field:'sourcePath',title:'Source Path',minWidth:220},{field:'targetUrl',title:'Target URL',minWidth:260},{field:'statusCode',title:'Code',width:90},{field:'hitCount',title:'Hits',width:90},{field:'lastHitTimeText',title:'Last Hit',width:160},{field:'status',title:'Enabled',width:80},{title:'Actions',toolbar:'#rowActions',width:140}];
			configs['content.redirect'].form = function(){return '<div class="x-toolbar"><div><div class="x-muted">Redirect only loads when content.redirect is enabled; batch import requires confirm=true before writing. Save-time and import-time warnings are shown before route rules are made effective.</div><textarea id="redirectImportJson" class="layui-textarea" style="width:520px;margin-top:8px" placeholder=\'[{\"sourcePath\":\"/old\",\"targetUrl\":\"/new\",\"statusCode\":301}]\'></textarea><pre id="redirectImportResult" class="x-code" style="margin-top:8px"></pre></div><div class="layui-btn-container"><button type="button" class="layui-btn layui-btn-sm" id="btnAddRedirect"><i class="layui-icon layui-icon-add-1"></i> Add Rule</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnRedirectPreview"><i class="layui-icon layui-icon-list"></i> Preview Import</button><button type="button" class="layui-btn layui-btn-normal layui-btn-sm" id="btnRedirectImport"><i class="layui-icon layui-icon-upload"></i> Confirm Import</button></div></div>';};
		}
		if(configs['content.media']){
			configs['content.media'].views = {
				main:{listApi:'/media/list', ops:'edit,delete', cols:[{field:'id',title:'ID',width:80},{field:'title',title:'Title',minWidth:160},{field:'url',title:'URL',minWidth:260},{field:'mime',title:'MIME',width:130},{field:'size',title:'Size',width:100},{field:'width',title:'W',width:80},{field:'height',title:'H',width:80},{field:'refCount',title:'Refs',width:90},{field:'status',title:'Enabled',width:90},{field:'updateTimeText',title:'Updated At',width:170},{title:'Actions',toolbar:'#rowActions',width:140}]},
				refs:{listApi:'/media/ref/list', ops:'', cols:[{field:'id',title:'ID',width:80},{field:'mediaId',title:'Media ID',width:100},{field:'mediaTitle',title:'Media Title',minWidth:160},{field:'contentId',title:'Content ID',width:110},{field:'contentTitle',title:'Content Title',minWidth:180},{field:'refType',title:'Ref Type',width:100},{field:'createTimeText',title:'Bound At',width:170}]}
			};
			configs['content.media'].form = function(){return viewSwitch([{key:'main',text:'Resources'},{key:'refs',text:'References'}]) + '<div class="x-toolbar"><div><div class="x-muted">Media references are synced from content save and import, and referenced media cannot be deleted. Batch delete keeps the same reference guard.</div><input id="mediaBatchIds" class="layui-input" style="width:320px;margin-top:8px" placeholder="IDs, comma separated"></div><div class="layui-btn-container"><button type="button" class="layui-btn layui-btn-sm" id="btnAddMedia"><i class="layui-icon layui-icon-add-1"></i> Add Resource</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnMediaBatchEnable">Enable</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnMediaBatchDisable">Disable</button><button type="button" class="layui-btn layui-btn-danger layui-btn-sm" id="btnMediaBatchDelete">Delete</button></div></div>';};
		}
		if(configs['content.revision']){
			configs['content.revision'].ops = 'diff,restore';
			configs['content.revision'].cols = [{field:'id',title:'ID',width:80},{field:'contentId',title:'Content ID',width:100},{field:'revisionNo',title:'Revision',width:100},{field:'title',title:'Title',minWidth:180},{field:'status',title:'Status',width:90},{field:'isDraft',title:'Draft',width:80},{field:'action',title:'Action',width:100},{field:'createTimeText',title:'Created At',width:170},{title:'Actions',toolbar:'#rowActions',width:100}];
			configs['content.revision'].form = function(){return '<div class="x-toolbar"><div class="x-muted">Revision snapshots are created on content writes. Use Diff to compare a snapshot with its previous revision, and Restore to preview current-vs-target changes before applying.</div></div>';};
		}
		if(configs['content.search']){
			configs['content.search'].cols = [{field:'id',title:'ID',width:80},{field:'title',title:'Title',minWidth:180},{field:'slug',title:'Slug',width:160},{field:'summary',title:'Summary',minWidth:220},{field:'status',title:'Status',width:90},{field:'isDraft',title:'Draft',width:80},{field:'updateTimeText',title:'Updated At',width:170}];
			configs['content.search'].form = function(){return '<div class="x-toolbar"><div><div class="x-muted">Search can rebuild its independent index from content rows; content save/import/delete also syncs the index when this pack is enabled. Use Probe to verify indexed query output through the same admin search API.</div><div style="display:flex;gap:8px;margin-top:8px;max-width:620px"><input id="searchProbeQuery" class="layui-input" placeholder="search query"><input id="searchProbeLimit" class="layui-input" type="number" min="1" max="20" style="width:120px" value="10"></div><pre id="searchProbeResult" class="x-code" style="margin-top:8px"></pre></div><div class="layui-btn-container"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnSearchProbe"><i class="layui-icon layui-icon-search"></i> Probe</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnSearchStats"><i class="layui-icon layui-icon-chart"></i> Stats</button><button type="button" class="layui-btn layui-btn-sm" id="btnSearchRebuild"><i class="layui-icon layui-icon-refresh"></i> Rebuild Index</button></div></div>';};
		}
		if(configs['content.sitemap']){
			configs['content.sitemap'].cols = [{field:'contentId',title:'Content ID',width:100},{field:'title',title:'Title',minWidth:180},{field:'loc',title:'URL',minWidth:260},{field:'changefreq',title:'Changefreq',width:110},{field:'priority',title:'Priority',width:90},{field:'updateTimeText',title:'Updated At',width:170}];
			configs['content.sitemap'].form = function(){return '<div class="x-toolbar"><div class="x-muted">Sitemap entries can be refreshed into content_sitemap_entry and are also synced when content is saved, imported or deleted. Set content.sitemap.siteUrl to output absolute URLs. sitemap-index.xml links paged sitemap.xml?page=N shards for large sites.</div><div class="layui-btn-container"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnSitemapStats"><i class="layui-icon layui-icon-chart"></i> Stats</button><button type="button" class="layui-btn layui-btn-sm" id="btnSitemapRefresh"><i class="layui-icon layui-icon-refresh"></i> Refresh Entries</button><a class="layui-btn layui-btn-primary layui-btn-sm" target="_blank" href="'+pub('/sitemap-index.xml')+'">sitemap-index.xml</a><a class="layui-btn layui-btn-primary layui-btn-sm" target="_blank" href="'+pub('/sitemap.xml')+'">sitemap.xml</a><a class="layui-btn layui-btn-primary layui-btn-sm" target="_blank" href="'+pub('/rss.xml')+'">rss.xml</a><a class="layui-btn layui-btn-primary layui-btn-sm" target="_blank" href="'+pub('/robots.txt')+'">robots.txt</a></div></div>';};
		}
		if(configs['content.related']){
			configs['content.related'].cols = [{field:'id',title:'ID',width:80},{field:'sourceContentId',title:'Source ID',width:110},{field:'sourceTitle',title:'Source Title',minWidth:160},{field:'relatedContentId',title:'Related ID',width:120},{field:'relatedTitle',title:'Related Title',minWidth:160},{field:'relationType',title:'Type',width:100},{field:'weight',title:'Weight',width:90},{field:'status',title:'Status',width:80},{field:'updateTimeText',title:'Updated At',width:170},{title:'Actions',toolbar:'#rowActions',width:100}];
			configs['content.related'].form = function(){return simpleForm([{name:'sourceContentId',label:'Source ID',type:'number',placeholder:'1'},{name:'relatedContentId',label:'Related ID',type:'number',placeholder:'2'},{name:'relationType',label:'Type',value:'manual',placeholder:'manual or rule'},{name:'weight',label:'Weight',type:'number',value:'0'},{name:'status',label:'Status',type:'number',value:'1'}]) + '<div class="x-toolbar"><div><div class="x-muted">Rule rebuild creates relationType=rule entries from content in the same category. Content save/import also refreshes the saved item; set content.related.ruleLimit to tune the default. Limit is clamped to 1-20 to keep rebuild work bounded.</div><div style="display:flex;gap:8px;margin-top:8px;max-width:420px"><input id="relatedRebuildContentId" class="layui-input" placeholder="content id, empty for all"><input id="relatedRebuildLimit" class="layui-input" type="number" min="1" max="20" style="width:120px" value="5"></div><pre id="relatedRebuildResult" class="x-code" style="margin-top:8px"></pre></div><div class="layui-btn-container"><button type="button" class="layui-btn layui-btn-sm" id="btnRelatedRebuild"><i class="layui-icon layui-icon-refresh"></i> Rebuild Rules</button></div></div>';};
		}
		if(configs['content.form']){
			configs['content.form'].views = {
				main:{listApi:'/form/list', ops:'delete', cols:[{field:'id',title:'ID',width:80},{field:'contentId',title:'Content ID',width:100},{field:'formKey',title:'Form Key',width:160},{field:'title',title:'Title',minWidth:180},{field:'schemaJson',title:'Schema',minWidth:260},{field:'status',title:'Status',width:80},{field:'updateTimeText',title:'Updated At',width:170},{title:'Actions',toolbar:'#rowActions',width:120}]},
				submissions:{listApi:'/form/submission/list', ops:'process', cols:[{field:'id',title:'ID',width:80},{field:'formId',title:'Form ID',width:90},{field:'formKey',title:'Form Key',width:140},{field:'formTitle',title:'Form Title',width:160},{field:'contentId',title:'Content ID',width:100},{field:'dataJson',title:'Submission Data',minWidth:300},{field:'ip',title:'IP',width:120},{field:'status',title:'Status',width:80},{field:'createTimeText',title:'Submitted At',width:170},{title:'Actions',toolbar:'#rowActions',width:110}]}
			};
			configs['content.form'].form = function(){return viewSwitch([{key:'main',text:'Forms'},{key:'submissions',text:'Submissions'}]) + simpleForm([{name:'contentId',label:'Content ID',type:'number',value:'0'},{name:'formKey',label:'Form Key',placeholder:'contact'},{name:'title',label:'Title',placeholder:'Contact Form'},{name:'schemaJson',label:'Schema',type:'textarea',value:'{}',placeholder:'JSON schema with required or fields[]'},{name:'status',label:'Status',type:'number',value:'1'}]) + '<div class="x-toolbar"><div><div class="x-muted">Submissions validate required fields, fields[].type, and fields[].format before storage; exported records can be marked processed. Designer buttons edit fields[] in the schema textarea directly.</div><div style="display:flex;gap:8px;align-items:center;flex-wrap:wrap;margin-top:8px"><input id="formDesignerName" class="layui-input" style="width:140px" placeholder="field name"><input id="formDesignerLabel" class="layui-input" style="width:150px" placeholder="label"><select id="formDesignerType" class="layui-select" style="width:120px"><option value="text">text</option><option value="email">email</option><option value="number">number</option><option value="textarea">textarea</option><option value="checkbox">checkbox</option><option value="select">select</option></select><input id="formDesignerPlaceholder" class="layui-input" style="width:150px" placeholder="placeholder"><input id="formDesignerMin" class="layui-input" type="number" style="width:100px" placeholder="min"><input id="formDesignerMax" class="layui-input" type="number" style="width:100px" placeholder="max"><input id="formDesignerPattern" class="layui-input" style="width:160px" placeholder="pattern"><input id="formDesignerOptions" class="layui-input" style="width:180px" placeholder="options comma list"><input id="formDesignerRequired" type="checkbox" title="required"></div></div><div class="layui-btn-container"><button type="button" class="layui-btn layui-btn-sm" id="btnFormDesignerAdd"><i class="layui-icon layui-icon-add-1"></i> Add Field</button><button type="button" class="layui-btn layui-btn-danger layui-btn-sm" id="btnFormDesignerRemove"><i class="layui-icon layui-icon-delete"></i> Remove Field</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnFormDesignerUp"><i class="layui-icon layui-icon-up"></i> Move Up</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnFormDesignerDown"><i class="layui-icon layui-icon-down"></i> Move Down</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnFormSchemaFormat"><i class="layui-icon layui-icon-fonts-code"></i> Format</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnFormSchemaValidate"><i class="layui-icon layui-icon-ok"></i> Validate</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnFormExport"><i class="layui-icon layui-icon-export"></i> Export Submissions</button></div></div>';};
		}
		function startAbilityPage(){
			layui.use(['table','form','layer'],function(){
				layui.table.on('tool(' + tableFilter + ')', function(obj){
					if(obj.event === 'approve') post(api('/comment/status'), {id:obj.data.id,status:1}).then(function(ret){message(ret);reload();});
					if(obj.event === 'reject') post(api('/comment/status'), {id:obj.data.id,status:2}).then(function(ret){message(ret);reload();});
					if(obj.event === 'hide') post(pub('/comment/hide'), {id:obj.data.id}).then(function(ret){message(ret);reload();});
					if(obj.event === 'edit'){
						if(packId === 'content.seo') openSeoDialog(obj.data);
						if(packId === 'content.redirect') openRedirectDialog(obj.data);
						if(packId === 'content.media') openMediaDialog(obj.data);
					}
					if(obj.event === 'diff'){
						if(packId === 'content.revision') showRevisionDiff(obj.data);
					}
					if(obj.event === 'restore'){
						if(packId === 'content.revision') showRevisionRestorePreview(obj.data);
					}
					if(obj.event === 'process'){
						if(packId === 'content.form') markFormSubmissionProcessed(obj.data);
					}
					if(obj.event === 'download'){
						if(packId === 'content.import-export') window.open(api('/import-export/export/download?id=' + encodeURIComponent(obj.data.id)), '_blank');
					}
					if(obj.event === 'delete'){
						var cfg = currentConfig();
						var delApi = packId === 'content.comment' ? '/comment/delete' : packId === 'content.tag' ? '/tag/delete' : packId === 'content.topic' ? '/topic/delete' : packId === 'content.sensitive' ? '/sensitive/word/delete' : packId === 'content.static' ? '/static/rule/delete' : packId === 'content.seo' ? '/seo/delete' : packId === 'content.redirect' ? '/redirect/delete' : packId === 'content.media' ? '/media/delete' : packId === 'content.related' ? '/related/delete' : packId === 'content.form' ? '/form/delete' : packId === 'content.access' ? '/access/rule/delete' : '';
						if(delApi) post(api(delApi), {id:obj.data.id}).then(function(ret){message(ret);reload();});
					}
					if(obj.event === 'unbind'){
						var unbindApi = packId === 'content.tag' ? '/tag/unbind' : packId === 'content.topic' ? '/topic/unbind' : '';
						if(unbindApi) post(api(unbindApi), {id:obj.data.id}).then(function(ret){message(ret);reload();});
					}
				});
				byId('btnReload').onclick = reload;
				reload();
				document.addEventListener('click', function(ev){
					if(!root || !root.contains(ev.target)) return;
					var btn = ev.target && ev.target.closest ? ev.target.closest('#btnAddRedirect') : null;
					if(btn) openRedirectDialog();
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnAddSeo') : null;
					if(btn) openSeoDialog();
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnAddMedia') : null;
					if(btn) openMediaDialog();
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnMediaBatchEnable') : null;
					if(btn) runMediaBatch('enable');
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnMediaBatchDisable') : null;
					if(btn) runMediaBatch('disable');
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnMediaBatchDelete') : null;
					if(btn) runMediaBatch('delete');
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnExportJson') : null;
					if(btn) runImportExportExport(false);
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnExportJsonNext') : null;
					if(btn) runImportExportExport(true);
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnImportPreview') : null;
					if(btn) post(api('/import-export/import/preview'), {sourceName:'sample-preview',fields:importExportFields(),items:[{}]}).then(function(ret){message(ret);reload();});
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnImportManualPreview') : null;
					if(btn) runImportExportCommit(false);
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnImportManualCommit') : null;
					if(btn) runImportExportCommit(true);
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnImportChunkPreview') : null;
					if(btn) runImportExportChunked(false);
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnImportChunkCommit') : null;
					if(btn) runImportExportChunked(true);
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnImportReplay') : null;
					if(btn) runImportReplay();
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnRedirectPreview') : null;
					if(btn) runRedirectImport(false);
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnRedirectImport') : null;
					if(btn) runRedirectImport(true);
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnSearchStats') : null;
					if(btn) showSearchStats();
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnSearchProbe') : null;
					if(btn) runSearchProbe();
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnSearchRebuild') : null;
					if(btn) rebuildSearchIndex();
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnSitemapStats') : null;
					if(btn) showSitemapStats();
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnSitemapRefresh') : null;
					if(btn) refreshSitemapEntries();
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnRelatedRebuild') : null;
					if(btn) rebuildRelatedRules();
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnLikeStats') : null;
					if(btn) showMetricStats('/like/stats', 'Like stats');
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnViewStats') : null;
					if(btn) showMetricStats('/view/stats', 'View stats');
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnFormExport') : null;
					if(btn) exportFormSubmissions();
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnFormDesignerAdd') : null;
					if(btn) appendFormSchemaField();
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnFormDesignerRemove') : null;
					if(btn) removeFormSchemaField();
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnFormDesignerUp') : null;
					if(btn) moveFormSchemaField(-1);
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnFormDesignerDown') : null;
					if(btn) moveFormSchemaField(1);
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnFormSchemaFormat') : null;
					if(btn) formatFormSchema();
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnFormSchemaValidate') : null;
					if(btn) validateFormSchema();
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnWorkflowSubmit') : null;
					if(btn) runWorkflowAction('submit');
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnWorkflowApprove') : null;
					if(btn) runWorkflowAction('approve');
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnWorkflowSchedule') : null;
					if(btn) runWorkflowAction('schedule');
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnWorkflowRunScheduled') : null;
					if(btn) runWorkflowScheduledPublish();
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnWorkflowReject') : null;
					if(btn) runWorkflowAction('reject');
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnWorkflowOffline') : null;
					if(btn) runWorkflowAction('offline');
				});
			});
		}
		if(window.layui){
			startAbilityPage();
		} else {
			var css = document.createElement('link');
			css.rel = 'stylesheet';
			css.href = '/layui/css/layui.css';
			document.head.appendChild(css);
			var script = document.createElement('script');
			script.src = '/layui/layui.js';
			script.onload = startAbilityPage;
			document.head.appendChild(script);
		}
	})();
	</script>
</div>
