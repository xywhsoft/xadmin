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
	.managed-ability-page .topic-arrange{border:1px solid #e6e6e6;background:#fff;margin-top:8px;max-height:280px;overflow:auto}
	.managed-ability-page .topic-arrange-row{display:grid;grid-template-columns:70px 110px 1fr 90px 124px;gap:8px;align-items:center;padding:8px 10px;border-bottom:1px solid #f0f0f0}
	.managed-ability-page .topic-arrange-row:last-child{border-bottom:0}
	.managed-ability-page .topic-arrange-head{background:#fafafa;font-weight:600;color:#333}
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
		<a class="layui-btn layui-btn-xs" lay-event="diff">对比</a>
		{{# } }}
		{{# if(d.__ops && d.__ops.indexOf('restore') >= 0){ }}
		<a class="layui-btn layui-btn-warm layui-btn-xs" lay-event="restore">恢复</a>
		{{# } }}
		{{# if(d.__ops && d.__ops.indexOf('detail') >= 0){ }}
		<a class="layui-btn layui-btn-xs" lay-event="detail">详情</a>
		{{# } }}
		{{# if(d.__ops && d.__ops.indexOf('audit') >= 0){ }}
		<a class="layui-btn layui-btn-xs" lay-event="approve">通过</a>
		<a class="layui-btn layui-btn-danger layui-btn-xs" lay-event="reject">驳回</a>
		{{# } }}
		{{# if(d.__ops && d.__ops.indexOf('hide') >= 0){ }}
		<a class="layui-btn layui-btn-warm layui-btn-xs" lay-event="hide">隐藏</a>
		{{# } }}
		{{# if(d.__ops && d.__ops.indexOf('process') >= 0){ }}
		<a class="layui-btn layui-btn-normal layui-btn-xs" lay-event="process">已处理</a>
		{{# } }}
		{{# if(d.__ops && d.__ops.indexOf('markRead') >= 0){ }}
		<a class="layui-btn layui-btn-xs" lay-event="markRead">已读</a>
		{{# } }}
		{{# if(d.__ops && d.__ops.indexOf('markUnread') >= 0){ }}
		<a class="layui-btn layui-btn-primary layui-btn-xs" lay-event="markUnread">未读</a>
		{{# } }}
		{{# if(d.__ops && d.__ops.indexOf('download') >= 0){ }}
		<a class="layui-btn layui-btn-xs" lay-event="download">下载</a>
		{{# } }}
		{{# if(d.__ops && d.__ops.indexOf('retry') >= 0){ }}
		<a class="layui-btn layui-btn-normal layui-btn-xs" lay-event="retry">重试</a>
		{{# } }}
		{{# if(d.__ops && d.__ops.indexOf('replay') >= 0){ }}
		<a class="layui-btn layui-btn-normal layui-btn-xs" lay-event="replay">重发</a>
		{{# } }}
		{{# if(d.__ops && d.__ops.indexOf('edit') >= 0){ }}
		<a class="layui-btn layui-btn-xs" lay-event="edit">编辑</a>
		{{# } }}
		{{# if(d.__ops && d.__ops.indexOf('routeRuleEdit') >= 0){ }}
		<a class="layui-btn layui-btn-xs" lay-event="routeRuleEdit">编辑</a>
		{{# } }}
		{{# if(d.__ops && d.__ops.indexOf('routeRuleToggle') >= 0){ }}
		<a class="layui-btn layui-btn-primary layui-btn-xs" lay-event="routeRuleToggle">启停</a>
		{{# } }}
		{{# if(d.__ops && d.__ops.indexOf('routeRuleSort') >= 0){ }}
		<a class="layui-btn layui-btn-normal layui-btn-xs" lay-event="routeRuleSortUp">上移</a>
		<a class="layui-btn layui-btn-normal layui-btn-xs" lay-event="routeRuleSortDown">下移</a>
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
			content_seo:'content.seo',
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
		packTitles['content.seo'] = '搜索引擎优化 SEO';
		packTitles['content.category'] = '栏目管理';
		var activeViews = {};
		var abilityListFilters = {};
		var importExportLastExport = null;
		var topicArrangeRows = [];
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
			if(target) target.innerHTML = renderImportExportExportResult(ret);
			importExportLastExport = ret || null;
			if(ret && ret.hasMore && byId('importExportOffset')) byId('importExportOffset').value = ret.nextOffset || 0;
			message(ret);
			reload();
		}
		function renderImportExportExportRows(rows){
			rows = Array.isArray(rows) ? rows.slice(0, 10) : [];
			var html = '';
			rows.forEach(function(row){
				html += '<tr><td>'+esc(row.id || '')+'</td><td>'+esc(row.title || row.name || '')+'</td><td>'+esc(row.status || '')+'</td><td>'+esc(row.isDraft ? '是' : '否')+'</td><td>'+esc(row.categoryId || '')+'</td></tr>';
			});
			if(!rows.length) html += '<tr><td colspan="5" style="text-align:center;color:#667085;">暂无导出行</td></tr>';
			return html;
		}
		function renderImportExportExportResult(ret){
			var rows = ret && Array.isArray(ret.data) ? ret.data : [];
			var plan = ret && ret.fieldPlan ? ret.fieldPlan : {};
			var exportedFields = Array.isArray(plan.exportedFields) ? plan.exportedFields : [];
			var ignoredFields = Array.isArray(plan.ignoredFields) ? plan.ignoredFields : [];
			var html = '<div class="x-muted">JSON 导出按 maxExportRows 分片返回；表格只预览前 10 行，完整数据保留在 JSON 诊断中。</div>';
			html += '<table class="layui-table"><thead><tr><th>任务ID</th><th>任务已保存</th><th>总数</th><th>本片导出</th><th>offset</th><th>limit</th><th>下一片</th><th>还有更多</th><th>警告</th></tr></thead><tbody>';
			html += '<tr><td>'+esc(ret && ret.jobId || '')+'</td><td>'+esc(ret && ret.jobSaved ? '是' : '否')+'</td><td>'+esc(ret && ret.totalCount || 0)+'</td><td>'+esc(ret && ret.exportedCount || rows.length)+'</td><td>'+esc(ret && ret.offset || 0)+'</td><td>'+esc(ret && ret.limit || '')+'</td><td>'+esc(ret && ret.nextOffset || '')+'</td><td>'+esc(ret && ret.hasMore ? '是' : '否')+'</td><td>'+esc(ret && ret.warning || '')+'</td></tr>';
			html += '</tbody></table><table class="layui-table"><thead><tr><th>实际导出字段</th><th>忽略字段</th><th>默认上限</th><th>最大导出行</th></tr></thead><tbody>';
			html += '<tr><td>'+esc(exportedFields.join(', ') || '全部字段')+'</td><td>'+esc(ignoredFields.join(', ') || '无')+'</td><td>'+esc(ret && ret.defaultCapped ? '是' : '否')+'</td><td>'+esc(ret && ret.maxExportRows || '')+'</td></tr></tbody></table>';
			html += '<table class="layui-table"><thead><tr><th>ID</th><th>标题</th><th>状态</th><th>草稿</th><th>栏目</th></tr></thead><tbody>' + renderImportExportExportRows(rows) + '</tbody></table>';
			html += '<pre class="x-code">' + esc(JSON.stringify(ret || {}, null, 2)) + '</pre>';
			return html;
		}
		function renderImportExportFieldPlanResult(ret){
			var data = ret && ret.data ? ret.data : ret || {};
			var available = Array.isArray(data.availableFields) ? data.availableFields : [];
			var requested = Array.isArray(data.requestedFields) ? data.requestedFields : [];
			var exported = Array.isArray(data.exportedFields) ? data.exportedFields : [];
			var ignored = Array.isArray(data.ignoredFields) ? data.ignoredFields : [];
			var html = '<div class="x-muted">字段计划只解析当前导出字段配置，不执行导入、导出或下载。</div>';
			html += '<table class="layui-table"><thead><tr><th>可用字段数</th><th>请求字段数</th><th>实际导出字段数</th><th>忽略字段数</th><th>全部字段</th></tr></thead><tbody>';
			html += '<tr><td>'+esc(available.length)+'</td><td>'+esc(requested.length)+'</td><td>'+esc(exported.length)+'</td><td>'+esc(ignored.length)+'</td><td>'+esc(data.allFields ? '是' : '否')+'</td></tr>';
			html += '</tbody></table>';
			html += '<table class="layui-table"><thead><tr><th>类型</th><th>字段</th></tr></thead><tbody>';
			html += '<tr><td>实际导出</td><td>'+esc(exported.join(', ') || '无')+'</td></tr>';
			html += '<tr><td>已忽略</td><td>'+esc(ignored.join(', ') || '无')+'</td></tr>';
			html += '<tr><td>可用字段</td><td>'+esc(available.join(', ') || '无')+'</td></tr>';
			html += '</tbody></table><pre class="x-code">' + esc(JSON.stringify(data, null, 2)) + '</pre>';
			return html;
		}
		function renderImportExportImportRows(rows){
			rows = Array.isArray(rows) ? rows : [];
			var html = '';
			rows.forEach(function(row){
				html += '<tr><td>'+esc(row.rowIndex || 0)+'</td><td>'+esc(row.ok ? '通过' : '失败')+'</td><td>'+esc(row.contentId || '')+'</td><td>'+esc(row.message || '')+'</td></tr>';
			});
			if(!rows.length) html += '<tr><td colspan="4" style="text-align:center;color:#667085;">暂无行结果</td></tr>';
			return html;
		}
		function renderImportExportImportResult(ret, mode){
			var rows = ret && Array.isArray(ret.data) ? ret.data : [];
			var html = '<div class="x-muted">'+esc(mode === 'commit' ? '确认导入会写入内容和派生数据；成功数只统计完整通过校验、写入和同步的行。' : '导入预检只校验字段和数据，不写入内容。')+'</div>';
			html += '<table class="layui-table"><thead><tr><th>模式</th><th>任务ID</th><th>任务已保存</th><th>任务状态</th><th>总数</th><th>成功</th><th>失败</th><th>警告</th></tr></thead><tbody>';
			html += '<tr><td>'+esc(mode === 'commit' ? '确认导入' : '预检')+'</td><td>'+esc(ret && ret.jobId || '')+'</td><td>'+esc(ret && ret.jobSaved ? '是' : '否')+'</td><td>'+esc(ret && ret.jobStatus || '')+'</td><td>'+esc(ret && ret.totalCount || 0)+'</td><td>'+esc(ret && ret.successCount || 0)+'</td><td>'+esc(ret && ret.failCount || 0)+'</td><td>'+esc(ret && ret.warning || '')+'</td></tr>';
			html += '</tbody></table><table class="layui-table"><thead><tr><th>行号</th><th>结果</th><th>内容ID</th><th>消息</th></tr></thead><tbody>';
			html += renderImportExportImportRows(rows) + '</tbody></table><pre class="x-code">' + esc(JSON.stringify(ret || {}, null, 2)) + '</pre>';
			return html;
		}
		function renderImportExportChunkResult(ret){
			var chunks = ret && Array.isArray(ret.chunks) ? ret.chunks : [];
			var html = '<div class="x-muted">分片导入按固定 chunkSize 拆分为有界 API 调用；每片仍使用导入预检/确认导入接口。</div>';
			html += '<table class="layui-table"><thead><tr><th>总行数</th><th>分片数</th><th>失败分片</th><th>结果</th></tr></thead><tbody>';
			html += '<tr><td>'+esc(ret && ret.total || 0)+'</td><td>'+esc(chunks.length)+'</td><td>'+esc(ret && ret.failedChunk || '')+'</td><td>'+esc(ret && ret.result === false ? '失败' : '完成')+'</td></tr>';
			html += '</tbody></table><table class="layui-table"><thead><tr><th>分片</th><th>行数</th><th>结果</th><th>成功</th><th>失败</th><th>任务ID</th></tr></thead><tbody>';
			chunks.forEach(function(row){
				var summary = row.summary || {};
				html += '<tr><td>'+esc(row.chunk || 0)+'</td><td>'+esc(row.count || 0)+'</td><td>'+esc(row.result ? '通过' : '失败')+'</td><td>'+esc(summary.successCount || 0)+'</td><td>'+esc(summary.failCount || 0)+'</td><td>'+esc(summary.jobId || '')+'</td></tr>';
			});
			if(!chunks.length) html += '<tr><td colspan="6" style="text-align:center;color:#667085;">暂无分片</td></tr>';
			html += '</tbody></table><pre class="x-code">' + esc(JSON.stringify(ret || {}, null, 2)) + '</pre>';
			return html;
		}
		function runImportExportExport(nextPage){
			var payload = importExportExportPayload();
			if(nextPage && importExportLastExport && importExportLastExport.hasMore) {
				payload.offset = importExportLastExport.nextOffset || payload.offset || 0;
			}
			post(api('/import-export/export/json'), payload).then(showImportExportResult);
		}
		function runImportExportFieldPlan(){
			post(api('/import-export/field-plan'), {fields:importExportFields()}).then(function(ret){
				var target = byId('importExportResult');
				if(target) target.innerHTML = renderImportExportFieldPlanResult(ret);
				message(ret);
			});
		}
		function showImportExportStats(){
			get(api('/import-export/stats')).then(function(ret){
				var data = ret && ret.data ? ret.data : ret;
				var importRows = Array.isArray(data && data.importStatus) ? data.importStatus : [];
				var exportRows = Array.isArray(data && data.exportStatus) ? data.exportStatus : [];
				var failureRows = Array.isArray(data && data.importFailureSamples) ? data.importFailureSamples : [];
				function statusTable(rows, emptyText){
					var html = '<table class="layui-table"><thead><tr><th>状态</th><th>任务数</th><th>总行数</th><th>成功</th><th>失败</th><th>最近完成</th></tr></thead><tbody>';
					rows.forEach(function(row){ html += '<tr><td>'+esc(row.status || '')+'</td><td>'+esc(row.jobCount || 0)+'</td><td>'+esc(row.totalCount || 0)+'</td><td>'+esc(row.successCount || 0)+'</td><td>'+esc(row.failCount || 0)+'</td><td>'+esc(row.lastFinishTimeText || '')+'</td></tr>'; });
					if(!rows.length) html += '<tr><td colspan="6" style="text-align:center;color:#667085;">'+esc(emptyText)+'</td></tr>';
					return html + '</tbody></table>';
				}
				function failureTable(rows){
					var html = '<table class="layui-table"><thead><tr><th>任务ID</th><th>来源</th><th>状态</th><th>总行数</th><th>成功</th><th>失败</th><th>完成时间</th></tr></thead><tbody>';
					rows.forEach(function(row){ html += '<tr><td>'+esc(row.id || 0)+'</td><td>'+esc(row.sourceName || '')+'</td><td>'+esc(row.status || '')+'</td><td>'+esc(row.totalCount || 0)+'</td><td>'+esc(row.successCount || 0)+'</td><td>'+esc(row.failCount || 0)+'</td><td>'+esc(row.finishTimeText || '')+'</td></tr>'; });
					if(!rows.length) html += '<tr><td colspan="7" style="text-align:center;color:#667085;">暂无失败导入任务</td></tr>';
					return html + '</tbody></table>';
				}
				var html = '<div class="x-muted">导入导出统计只读取任务表聚合结果，不执行导入、导出或下载。</div>';
				html += '<table class="layui-table"><thead><tr><th>导入任务</th><th>导入总行</th><th>导入成功</th><th>导入失败</th><th>最近导入完成</th><th>导出任务</th><th>导出总行</th><th>最近导出完成</th></tr></thead><tbody><tr><td>'+esc(data.importJobCount || 0)+'</td><td>'+esc(data.importTotalCount || 0)+'</td><td>'+esc(data.importSuccessCount || 0)+'</td><td>'+esc(data.importFailCount || 0)+'</td><td>'+esc(data.importLastFinishTimeText || '')+'</td><td>'+esc(data.exportJobCount || 0)+'</td><td>'+esc(data.exportTotalCount || 0)+'</td><td>'+esc(data.exportLastFinishTimeText || '')+'</td></tr></tbody></table>';
				html += '<div class="x-muted">导入状态分布</div>' + statusTable(importRows, '暂无导入任务');
				html += '<div class="x-muted">导出状态分布</div>' + statusTable(exportRows, '暂无导出任务');
				html += '<div class="x-muted">最近失败导入任务（最多 '+esc(data.failureSampleLimit || 0)+' 条）</div>' + failureTable(failureRows);
				html += '<pre class="x-code">' + esc(JSON.stringify(data, null, 2)) + '</pre>';
				var target = byId('importExportResult');
				if(target) target.innerHTML = html;
				message(ret);
			});
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
			post(api(confirm ? '/import-export/import/commit' : '/import-export/import/preview'), payload).then(function(ret){
				var target = byId('importExportResult');
				if(target) target.innerHTML = renderImportExportImportResult(ret, confirm ? 'commit' : 'preview');
				message(ret);
				reload();
			});
		}
		function runImportStage(){
			var rows = importExportItems();
			var payload = {sourceName:'manual-json-stage',fields:importExportFields(),items:rows};
			var target = byId('importExportResult');
			post(api('/import-export/import/stage'), payload).then(function(ret){
				if(target) target.innerHTML = '<div class="x-muted">导入载荷已落盘并记录后台任务；后续仍通过预检、确认导入或分片导入执行。</div><pre class="x-code">' + esc(JSON.stringify(ret || {}, null, 2)) + '</pre>';
				message(ret);
				reload();
			});
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
					if(target) target.innerHTML = renderImportExportChunkResult({result:true,total:rows.length,chunks:results});
					reload();
					return;
				}
				var chunk = rows.slice(index * size, Math.min(rows.length, (index + 1) * size));
				var payload = {sourceName:'manual-json-chunk-' + (index + 1),fields:importExportFields(),items:chunk};
				if(confirm) payload.confirm = true;
				if(confirm) payload.conflictMode = importExportConflictMode();
				if(target) target.textContent = 'Running chunk ' + (index + 1) + '/' + totalChunks + ' (' + chunk.length + ' rows)...';
				post(api(confirm ? '/import-export/import/commit' : '/import-export/import/preview'), payload).then(function(ret){
					results.push({chunk:index + 1,count:chunk.length,result:ret && ret.result !== false,summary:ret || {}});
					if(ret && ret.result === false){
						if(target) target.innerHTML = renderImportExportChunkResult({result:false,total:rows.length,failedChunk:index + 1,chunks:results});
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
		function tagBatchIds(){
			var input = byId('tagBatchIds');
			return input && input.value ? input.value.split(',').map(function(s){return parseInt(s.trim(),10);}).filter(function(n){return n>0;}) : [];
		}
		function topicBatchIds(){
			var input = byId('topicBatchIds');
			return input && input.value ? input.value.split(',').map(function(s){return parseInt(s.trim(),10);}).filter(function(n){return n>0;}) : [];
		}
		function commentBatchIds(){
			var input = byId('commentBatchIds');
			return input && input.value ? input.value.split(',').map(function(s){return parseInt(s.trim(),10);}).filter(function(n){return n>0;}) : [];
		}
		function renderGenericBatchRows(rows){
			rows = Array.isArray(rows) ? rows : [];
			var html = '';
			rows.forEach(function(row){
				html += '<tr><td>'+esc(row.id || 0)+'</td><td>'+esc(row.ok ? '成功' : '失败')+'</td><td>'+esc(row.message || '')+'</td></tr>';
			});
			if(!rows.length) html += '<tr><td colspan="3" style="text-align:center;color:#667085;">暂无明细</td></tr>';
			return html;
		}
		function renderGenericBatchResult(ret, title, actionText){
			var rows = ret && Array.isArray(ret.data) ? ret.data : [];
			var html = '<div class="x-form"><div class="x-muted">'+esc(title || '批量操作结果')+'</div>';
			html += '<table class="layui-table"><thead><tr><th>操作</th><th>目标状态</th><th>总数</th><th>成功</th><th>失败</th></tr></thead><tbody>';
			html += '<tr><td>'+esc(actionText || '')+'</td><td>'+esc(ret && Object.prototype.hasOwnProperty.call(ret, 'status') ? ret.status : '')+'</td><td>'+esc(ret && ret.totalCount || 0)+'</td><td>'+esc(ret && ret.successCount || 0)+'</td><td>'+esc(ret && ret.failCount || 0)+'</td></tr></tbody></table>';
			html += '<table class="layui-table"><thead><tr><th>ID</th><th>结果</th><th>消息</th></tr></thead><tbody>' + renderGenericBatchRows(rows) + '</tbody></table>';
			html += '<pre class="x-code">' + esc(JSON.stringify(ret || {}, null, 2)) + '</pre></div>';
			return html;
		}
		function renderRowActionResult(ret, title, row, actionText){
			ret = ret || {};
			row = row || {};
			var html = '<div class="x-form"><div class="x-muted">' + esc(title || '行操作结果') + '</div>';
			html += '<table class="layui-table"><thead><tr><th>操作</th><th>目标 ID</th><th>结果</th><th>消息</th></tr></thead><tbody><tr>'
				+ '<td>' + esc(actionText || '') + '</td>'
				+ '<td>' + esc(row.id || ret.id || '') + '</td>'
				+ '<td>' + esc(ret.result === false ? '失败' : '成功') + '</td>'
				+ '<td>' + esc(ret.message || ret.error || '') + '</td></tr></tbody></table>';
			html += '<table class="layui-table"><thead><tr><th>backgroundTaskId</th><th>queued</th></tr></thead><tbody><tr><td>' + esc(ret.backgroundTaskId || '') + '</td><td>' + esc(ret.queued ? 'yes' : 'no') + '</td></tr></tbody></table>';
			html += '<pre class="x-code">' + esc(JSON.stringify(ret, null, 2)) + '</pre></div>';
			return html;
		}
		function showRowActionResult(ret, title, row, actionText){
			var target = byId('detail');
			if(target) target.innerHTML = renderRowActionResult(ret, title, row, actionText);
		}
		function runTagBatchStatus(status){
			var ids = tagBatchIds();
			if(!ids.length){ message({result:false,error:'tag ids required'}); return; }
			post(api('/tag/batch-status'), {ids:ids,status:status}).then(function(ret){
				byId('detail').innerHTML = renderGenericBatchResult(ret, '标签批量状态结果', status ? '启用' : '停用');
				message(ret);
				reload();
			});
		}
		function runTopicBatchStatus(status){
			var ids = topicBatchIds();
			if(!ids.length){ message({result:false,error:'topic ids required'}); return; }
			post(api('/topic/batch-status'), {ids:ids,status:status}).then(function(ret){
				byId('detail').innerHTML = renderGenericBatchResult(ret, '专题批量状态结果', status ? '启用' : '停用');
				message(ret);
				reload();
			});
		}
		function runCommentStatusBatch(status){
			var ids = commentBatchIds();
			if(!ids.length){ message({result:false,error:'comment ids required'}); return; }
			post(api('/comment/status-batch'), {ids:ids,status:status}).then(function(ret){
				var target = byId('commentBatchResult');
				if(target) target.innerHTML = renderCommentBatchResult(ret, status);
				message(ret);
				reload();
			});
		}
		function renderCommentBatchRows(rows){
			rows = Array.isArray(rows) ? rows : [];
			var html = '';
			rows.forEach(function(row){
				html += '<tr><td>'+esc(row.id || 0)+'</td><td>'+esc(row.contentId || 0)+'</td><td>'+esc(row.updated ? '已更新' : '未更新')+'</td></tr>';
			});
			if(!rows.length) html += '<tr><td colspan="3" style="text-align:center;color:#667085;">暂无明细</td></tr>';
			return html;
		}
		function renderCommentBatchResult(ret, status){
			var data = ret && ret.data ? ret.data : ret || {};
			var html = '<div class="x-muted">批量审核只接受通过或驳回，单次数量受 content.comment.maxAdminListRows 限制。</div>';
			html += '<table class="layui-table"><thead><tr><th>目标状态</th><th>提交数量</th><th>更新数量</th><th>失败数量</th><th>单次上限</th></tr></thead><tbody>';
			html += '<tr><td>'+esc(Number(status) === 1 ? '通过' : '驳回')+'</td><td>'+esc(data.total || 0)+'</td><td>'+esc(data.updated || 0)+'</td><td>'+esc(data.failed || 0)+'</td><td>'+esc(data.limit || '')+'</td></tr>';
			html += '</tbody></table><table class="layui-table"><thead><tr><th>评论ID</th><th>内容ID</th><th>结果</th></tr></thead><tbody>';
			html += renderCommentBatchRows(data.rows) + '</tbody></table><pre class="x-code">' + esc(JSON.stringify(data, null, 2)) + '</pre>';
			return html;
		}
		function showCommentModerationStats(){
			get(api('/comment/moderation-stats')).then(function(ret){
				var data = ret && ret.data ? ret.data : ret;
				var html = '<table class="layui-table"><thead><tr><th>总数</th><th>待审</th><th>通过</th><th>驳回</th><th>隐藏</th><th>删除</th><th>近期待审</th><th>统计窗口</th></tr></thead><tbody>';
				html += '<tr><td>'+esc(data.totalCount || 0)+'</td><td>'+esc(data.pendingCount || 0)+'</td><td>'+esc(data.approvedCount || 0)+'</td><td>'+esc(data.rejectedCount || 0)+'</td><td>'+esc(data.hiddenCount || 0)+'</td><td>'+esc(data.deletedCount || 0)+'</td><td>'+esc(data.recentPendingCount || 0)+'</td><td>'+esc(data.recentDays || 0)+' 天</td></tr>';
				html += '</tbody></table><div class="x-muted">待审风险分布只扫描待审队列前 '+esc(data.riskScanLimit || 0)+' 条，本次扫描 '+esc(data.riskScanned || 0)+' 条。</div>';
				html += '<table class="layui-table"><thead><tr><th>风险分段</th><th>分数范围</th><th>数量</th></tr></thead><tbody>';
				(Array.isArray(data.riskStats) ? data.riskStats : []).forEach(function(row){
					html += '<tr><td>'+esc(row.bucket || '')+'</td><td>'+esc((row.minScore || 0) + ' - ' + (row.maxScore || 0))+'</td><td>'+esc(row.count || 0)+'</td></tr>';
				});
				if(!Array.isArray(data.riskStats) || !data.riskStats.length) html += '<tr><td colspan="3" class="x-muted">暂无风险统计</td></tr>';
				html += '</tbody></table><pre class="x-code">' + esc(JSON.stringify(data, null, 2)) + '</pre>';
				var target = byId('commentStatsResult');
				if(target) target.innerHTML = html;
				message(ret);
			});
		}
		function mediaRowFromAttachmentData(data){
			data = data || {};
			return {
				attachmentXid: data.xid || '',
				title: data.filename || data.xid || '',
				url: data.url || (data.xid ? '/attachment?xid=' + encodeURIComponent(data.xid) : ''),
				ext: data.ext || '',
				size: data.size || 0,
				status: 1
			};
		}
		function runMediaBatch(action){
			var ids = mediaBatchIds();
			if(!ids.length){ message({result:false,error:'media ids required'}); return; }
			post(api('/media/batch'), {action:action,ids:ids}).then(function(ret){
				byId('detail').innerHTML = renderGenericBatchResult(ret, '媒体批量操作结果', action);
				message(ret);
				reload();
			});
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
			renderFormSchemaDesignerPreview();
		}
		function analyzeFormSchema(schema){
			var fields = Array.isArray(schema.fields) ? schema.fields : [];
			var required = Array.isArray(schema.required) ? schema.required : [];
			var seen = {};
			var fieldNames = {};
			var issues = [];
			fields.forEach(function(field, idx){
				var name = field && field.name ? String(field.name).trim() : '';
				if(!name){
					issues.push({index:idx, message:'字段名为空'});
					return;
				}
				if(seen[name]){
					issues.push({index:idx, message:'字段名重复'});
				}
				seen[name] = true;
				fieldNames[name] = idx;
			});
			required.forEach(function(name){
				name = name == null ? '' : String(name).trim();
				if(name && fieldNames[name] == null){
					issues.push({index:-1, message:'必填字段不存在: ' + name});
				}
			});
			var requiredSeen = {};
			required.forEach(function(name){
				name = name == null ? '' : String(name).trim();
				if(!name) return;
				if(requiredSeen[name]){
					issues.push({index:-1, message:'必填字段重复: ' + name});
				}
				requiredSeen[name] = true;
			});
			return issues;
		}
		function renderFormSchemaDesignerPreview(){
			var target = byId('formDesignerPreview');
			if(!target) return;
			try {
				var schema = readFormSchema();
				var fields = Array.isArray(schema.fields) ? schema.fields : [];
				var required = Array.isArray(schema.required) ? schema.required : [];
				var issues = analyzeFormSchema(schema);
				var issueMap = {};
				issues.forEach(function(issue){ issueMap[issue.index] = issue.message; });
				var html = '<table class="layui-table"><thead><tr><th style="width:70px">顺序</th><th>字段</th><th style="width:120px">类型</th><th style="width:90px">必填</th><th style="width:120px">结构提示</th><th style="width:90px">操作</th></tr></thead><tbody>';
				if(!fields.length) html += '<tr><td colspan="6" style="text-align:center;color:#999;">暂无字段</td></tr>';
				fields.forEach(function(field, idx){
					field = field || {};
					var name = field.name || '';
					html += '<tr><td>' + (idx + 1) + '</td><td><div><b>' + esc(name) + '</b></div><div class="x-muted">' + esc(field.label || name) + '</div></td><td>' + esc(field.type || 'text') + '</td><td>' + (required.indexOf(name) >= 0 ? '是' : '否') + '</td><td>' + (issueMap[idx] ? '<span style="color:#d93026">' + esc(issueMap[idx]) + '</span>' : '<span class="x-muted">正常</span>') + '</td><td><button type="button" class="layui-btn layui-btn-primary layui-btn-xs" data-form-designer-pick="' + idx + '">选择</button></td></tr>';
				});
				issues.filter(function(issue){ return issue.index < 0; }).forEach(function(issue){
					html += '<tr><td>-</td><td colspan="4"><span style="color:#d93026">' + esc(issue.message) + '</span></td><td></td></tr>';
				});
				target.innerHTML = html + '</tbody></table>';
			} catch(e) {
				target.innerHTML = '<div class="x-muted">schema JSON 无法解析</div>';
			}
		}
		function fillFormDesignerField(index){
			try {
				var schema = readFormSchema();
				var fields = Array.isArray(schema.fields) ? schema.fields : [];
				var field = fields[Number(index || 0)] || {};
				var required = Array.isArray(schema.required) ? schema.required : [];
				var name = field.name || '';
				if(byId('formDesignerName')) byId('formDesignerName').value = name;
				if(byId('formDesignerLabel')) byId('formDesignerLabel').value = field.label || name;
				if(byId('formDesignerType')) byId('formDesignerType').value = field.type || 'text';
				if(byId('formDesignerPlaceholder')) byId('formDesignerPlaceholder').value = field.placeholder || '';
				if(byId('formDesignerPattern')) byId('formDesignerPattern').value = field.pattern || '';
				if(byId('formDesignerOptions')) byId('formDesignerOptions').value = Array.isArray(field.options) ? field.options.join(',') : '';
				if(byId('formDesignerMin')) byId('formDesignerMin').value = field.min != null ? field.min : (field.minLength != null ? field.minLength : '');
				if(byId('formDesignerMax')) byId('formDesignerMax').value = field.max != null ? field.max : (field.maxLength != null ? field.maxLength : '');
				if(byId('formDesignerRequired')) byId('formDesignerRequired').checked = required.indexOf(name) >= 0;
				if(window.layui && layui.form) layui.form.render();
			} catch(e) {
				message({result:false,error:'invalid schema json'});
			}
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
			if(!name){ message({result:false,error:'字段名不能为空'}); return; }
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
				message({result:true,message:'字段已添加'});
			} catch(e) {
				message({result:false,error:'invalid schema json'});
			}
		}
		function updateFormSchemaField(){
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
				var fields = Array.isArray(schema.fields) ? schema.fields : [];
				var idx = findFormSchemaFieldIndex(fields, name);
				if(idx < 0){ message({result:false,error:'field not found'}); return; }
				var fieldType = (typeInput && typeInput.value) || 'text';
				var field = {name:name,label:(labelInput && labelInput.value) || name,type:fieldType};
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
				fields[idx] = field;
				schema.fields = fields;
				if(!Array.isArray(schema.required)) schema.required = [];
				schema.required = schema.required.filter(function(item){ return item !== name; });
				if(requiredInput && requiredInput.checked) schema.required.push(name);
				writeFormSchema(schema);
				message({result:true,message:'field updated'});
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
				if(idx < 0){ message({result:false,error:'未找到字段'}); return; }
				var removed = schema.fields.splice(idx, 1)[0];
				if(Array.isArray(schema.required) && removed && removed.name){
					schema.required = schema.required.filter(function(item){ return item !== removed.name; });
				}
				writeFormSchema(schema);
				message({result:true,message:'字段已删除'});
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
				if(idx < 0 || next < 0 || next >= fields.length){ message({result:false,error:'字段无法移动'}); return; }
				var tmp = fields[idx];
				fields[idx] = fields[next];
				fields[next] = tmp;
				schema.fields = fields;
				writeFormSchema(schema);
				message({result:true,message:'字段已移动'});
			} catch(e) {
				message({result:false,error:'invalid schema json'});
			}
		}
		function formatFormSchema(){
			try { writeFormSchema(readFormSchema()); message({result:true,message:'schema formatted'}); }
			catch(e){ message({result:false,error:'invalid schema json'}); }
		}
		function validateFormSchema(){
			try {
				var schema = readFormSchema();
				var issues = analyzeFormSchema(schema);
				renderFormSchemaDesignerPreview();
				if(issues.length){
					message({result:false,error:issues[0].message});
					return;
				}
				message({result:true,message:'schema json is valid'});
			}
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
			if(targetId === 'seoPreviewResult' && target) {
				target.innerHTML = renderSeoPreviewResult(ret, 'content');
			} else if(targetId === 'seoCategoryPreviewResult' && target) {
				target.innerHTML = renderSeoPreviewResult(ret, 'category');
			} else if(target) {
				target.textContent = JSON.stringify(ret && ret.data ? ret.data : ret, null, 2);
			}
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
			var btnStaticClean = byId('btnStaticClean');
			var btnStaticRulePreview = byId('btnStaticRulePreview');
			var btnStaticRuleFilter = byId('btnStaticRuleFilter');
			var btnStaticRuleFilterClear = byId('btnStaticRuleFilterClear');
			var btnStaticTaskFilter = byId('btnStaticTaskFilter');
			var btnStaticTaskFilterClear = byId('btnStaticTaskFilterClear');
			var btnStaticRetryFailed = byId('btnStaticRetryFailed');
			var btnStaticArtifactFilter = byId('btnStaticArtifactFilter');
			var btnStaticArtifactFilterClear = byId('btnStaticArtifactFilterClear');
			var btnSitemapFilter = byId('btnSitemapFilter');
			var btnSitemapFilterClear = byId('btnSitemapFilterClear');
			var btnFormSubmissionFilter = byId('btnFormSubmissionFilter');
			var btnFormSubmissionFilterClear = byId('btnFormSubmissionFilterClear');
			var btnFormNotificationFilter = byId('btnFormNotificationFilter');
			var btnFormNotificationFilterClear = byId('btnFormNotificationFilterClear');
			var btnImportExportJobFilter = byId('btnImportExportJobFilter');
			var btnImportExportJobFilterClear = byId('btnImportExportJobFilterClear');
			var btnAccessFilter = byId('btnAccessFilter');
			var btnAccessFilterClear = byId('btnAccessFilterClear');
			var btnMediaFilter = byId('btnMediaFilter');
			var btnMediaFilterClear = byId('btnMediaFilterClear');
			var btnRelatedFilter = byId('btnRelatedFilter');
			var btnRelatedFilterClear = byId('btnRelatedFilterClear');
			var btnSlugCheck = byId('btnSlugCheck');
			var btnSlugPreview = byId('btnSlugPreview');
			var btnSlugRuleExplain = byId('btnSlugRuleExplain');
			var btnUnifiedRouteRuleExplain = byId('btnUnifiedRouteRuleExplain');
			var btnUnifiedRouteRuleValidate = byId('btnUnifiedRouteRuleValidate');
			var btnRouteRuleFilter = byId('btnRouteRuleFilter');
			var btnRouteRuleFilterClear = byId('btnRouteRuleFilterClear');
			var btnRouteRuleRefresh = byId('btnRouteRuleRefresh');
			var btnSlugRepairPreview = byId('btnSlugRepairPreview');
			var btnSlugRepairConfirm = byId('btnSlugRepairConfirm');
			var btnSlugHistoryFilter = byId('btnSlugHistoryFilter');
			var btnSlugHistoryFilterClear = byId('btnSlugHistoryFilterClear');
			var btnAuditFilter = byId('btnAuditFilter');
			var btnAuditFilterClear = byId('btnAuditFilterClear');
			var btnAuditCleanup = byId('btnAuditCleanup');
			var btnTagLinkFilter = byId('btnTagLinkFilter');
			var btnTagLinkFilterClear = byId('btnTagLinkFilterClear');
			var btnTopicLinkFilter = byId('btnTopicLinkFilter');
			var btnTopicLinkFilterClear = byId('btnTopicLinkFilterClear');
			var btnTagMerge = byId('btnTagMerge');
			var btnTopicSort = byId('btnTopicSort');
			var btnTopicArrangeLoad = byId('btnTopicArrangeLoad');
			var btnSensitiveImportPreview = byId('btnSensitiveImportPreview');
			var btnSensitiveImportConfirm = byId('btnSensitiveImportConfirm');
			var btnSensitiveCheck = byId('btnSensitiveCheck');
			var btnSensitiveStats = byId('btnSensitiveStats');
			var btnSensitiveCleanup = byId('btnSensitiveCleanup');
			var btnSensitiveLogFilter = byId('btnSensitiveLogFilter');
			var btnSensitiveLogFilterClear = byId('btnSensitiveLogFilterClear');
			if(btnSave) btnSave.onclick = saveEditor;
			if(btnGenerate) btnGenerate.onclick = runStaticGenerate;
			if(btnStaticClean) btnStaticClean.onclick = runStaticClean;
			if(btnStaticRulePreview) btnStaticRulePreview.onclick = runStaticRulePreview;
			if(btnStaticRuleFilter) btnStaticRuleFilter.onclick = function(){
				var pathInput = byId('staticRulePathFilter');
				var statusInput = byId('staticRuleStatusFilter');
				abilityListFilters.staticRulePathPattern = pathInput ? String(pathInput.value || '').trim() : '';
				abilityListFilters.staticRuleStatus = statusInput ? String(statusInput.value || '').trim() : '';
				reload();
			};
			if(btnStaticRuleFilterClear) btnStaticRuleFilterClear.onclick = function(){
				abilityListFilters.staticRulePathPattern = '';
				abilityListFilters.staticRuleStatus = '';
				['staticRulePathFilter','staticRuleStatusFilter'].forEach(function(id){
					var input = byId(id);
					if(input) input.value = '';
				});
				reload();
			};
			if(btnStaticTaskFilter) btnStaticTaskFilter.onclick = function(){
				var statusInput = byId('staticTaskStatusFilter');
				var ruleInput = byId('staticTaskRuleIdFilter');
				var targetInput = byId('staticTaskTargetIdFilter');
				abilityListFilters.staticTaskStatus = statusInput ? String(statusInput.value || '').trim() : '';
				abilityListFilters.staticTaskRuleId = ruleInput ? String(ruleInput.value || '').trim() : '';
				abilityListFilters.staticTaskTargetId = targetInput ? String(targetInput.value || '').trim() : '';
				reload();
			};
			if(btnStaticTaskFilterClear) btnStaticTaskFilterClear.onclick = function(){
				abilityListFilters.staticTaskStatus = '';
				abilityListFilters.staticTaskRuleId = '';
				abilityListFilters.staticTaskTargetId = '';
				var statusInput = byId('staticTaskStatusFilter');
				var ruleInput = byId('staticTaskRuleIdFilter');
				var targetInput = byId('staticTaskTargetIdFilter');
				if(statusInput) statusInput.value = '';
				if(ruleInput) ruleInput.value = '';
				if(targetInput) targetInput.value = '';
				reload();
			};
			if(btnStaticRetryFailed) btnStaticRetryFailed.onclick = runStaticRetryFailed;
			if(btnStaticArtifactFilter) btnStaticArtifactFilter.onclick = function(){
				var targetInput = byId('staticArtifactTargetIdFilter');
				var pathInput = byId('staticArtifactPathFilter');
				abilityListFilters.staticArtifactTargetId = targetInput ? String(targetInput.value || '').trim() : '';
				abilityListFilters.staticArtifactPath = pathInput ? String(pathInput.value || '').trim() : '';
				reload();
			};
			if(btnStaticArtifactFilterClear) btnStaticArtifactFilterClear.onclick = function(){
				abilityListFilters.staticArtifactTargetId = '';
				abilityListFilters.staticArtifactPath = '';
				var targetInput = byId('staticArtifactTargetIdFilter');
				var pathInput = byId('staticArtifactPathFilter');
				if(targetInput) targetInput.value = '';
				if(pathInput) pathInput.value = '';
				reload();
			};
			if(btnSitemapFilter) btnSitemapFilter.onclick = function(){
				var contentInput = byId('sitemapContentIdFilter');
				var statusInput = byId('sitemapStatusFilter');
				abilityListFilters.sitemapContentId = contentInput ? String(contentInput.value || '').trim() : '';
				abilityListFilters.sitemapStatus = statusInput ? String(statusInput.value || '').trim() : '';
				reload();
			};
			if(btnSitemapFilterClear) btnSitemapFilterClear.onclick = function(){
				abilityListFilters.sitemapContentId = '';
				abilityListFilters.sitemapStatus = '';
				['sitemapContentIdFilter','sitemapStatusFilter'].forEach(function(id){
					var input = byId(id);
					if(input) input.value = '';
				});
				reload();
			};
			if(btnFormSubmissionFilter) btnFormSubmissionFilter.onclick = function(){
				var formInput = byId('formSubmissionFormId');
				var contentInput = byId('formSubmissionContentId');
				var statusInput = byId('formSubmissionStatus');
				abilityListFilters.formSubmissionFormId = formInput ? String(formInput.value || '').trim() : '';
				abilityListFilters.formSubmissionContentId = contentInput ? String(contentInput.value || '').trim() : '';
				abilityListFilters.formSubmissionStatus = statusInput ? String(statusInput.value || '').trim() : '';
				reload();
			};
			if(btnFormSubmissionFilterClear) btnFormSubmissionFilterClear.onclick = function(){
				abilityListFilters.formSubmissionFormId = '';
				abilityListFilters.formSubmissionContentId = '';
				abilityListFilters.formSubmissionStatus = '';
				['formSubmissionFormId','formSubmissionContentId','formSubmissionStatus'].forEach(function(id){
					var input = byId(id);
					if(input) input.value = '';
				});
				reload();
			};
			if(btnFormNotificationFilter) btnFormNotificationFilter.onclick = function(){
				var formInput = byId('formNotificationFormId');
				var contentInput = byId('formNotificationContentId');
				var statusInput = byId('formNotificationStatus');
				var eventInput = byId('formNotificationEvent');
				abilityListFilters.formNotificationFormId = formInput ? String(formInput.value || '').trim() : '';
				abilityListFilters.formNotificationContentId = contentInput ? String(contentInput.value || '').trim() : '';
				abilityListFilters.formNotificationStatus = statusInput ? String(statusInput.value || '').trim() : '';
				abilityListFilters.formNotificationEvent = eventInput ? String(eventInput.value || '').trim() : '';
				reload();
			};
			if(btnFormNotificationFilterClear) btnFormNotificationFilterClear.onclick = function(){
				abilityListFilters.formNotificationFormId = '';
				abilityListFilters.formNotificationContentId = '';
				abilityListFilters.formNotificationStatus = '';
				abilityListFilters.formNotificationEvent = '';
				['formNotificationFormId','formNotificationContentId','formNotificationStatus','formNotificationEvent'].forEach(function(id){
					var input = byId(id);
					if(input) input.value = '';
				});
				reload();
			};
			if(btnImportExportJobFilter) btnImportExportJobFilter.onclick = function(){
				var input = byId('importExportJobStatus');
				abilityListFilters.importExportJobStatus = input ? String(input.value || '').trim() : '';
				reload();
			};
			if(btnImportExportJobFilterClear) btnImportExportJobFilterClear.onclick = function(){
				abilityListFilters.importExportJobStatus = '';
				var input = byId('importExportJobStatus');
				if(input) input.value = '';
				reload();
			};
			if(btnAccessFilter) btnAccessFilter.onclick = function(){
				var targetTypeInput = byId('accessFilterTargetType');
				var targetIdInput = byId('accessFilterTargetId');
				var modeInput = byId('accessFilterMode');
				var statusInput = byId('accessFilterStatus');
				abilityListFilters.accessTargetType = targetTypeInput ? String(targetTypeInput.value || '').trim() : '';
				abilityListFilters.accessTargetId = targetIdInput ? String(targetIdInput.value || '').trim() : '';
				abilityListFilters.accessMode = modeInput ? String(modeInput.value || '').trim() : '';
				abilityListFilters.accessStatus = statusInput ? String(statusInput.value || '').trim() : '';
				reload();
			};
			if(btnAccessFilterClear) btnAccessFilterClear.onclick = function(){
				abilityListFilters.accessTargetType = '';
				abilityListFilters.accessTargetId = '';
				abilityListFilters.accessMode = '';
				abilityListFilters.accessStatus = '';
				['accessFilterTargetType','accessFilterTargetId','accessFilterMode','accessFilterStatus'].forEach(function(id){
					var input = byId(id);
					if(input) input.value = '';
				});
				reload();
			};
			if(btnMediaFilter) btnMediaFilter.onclick = function(){
				var mimeInput = byId('mediaFilterMime');
				var statusInput = byId('mediaFilterStatus');
				abilityListFilters.mediaMime = mimeInput ? String(mimeInput.value || '').trim() : '';
				abilityListFilters.mediaStatus = statusInput ? String(statusInput.value || '').trim() : '';
				reload();
			};
			if(btnMediaFilterClear) btnMediaFilterClear.onclick = function(){
				abilityListFilters.mediaMime = '';
				abilityListFilters.mediaStatus = '';
				['mediaFilterMime','mediaFilterStatus'].forEach(function(id){
					var input = byId(id);
					if(input) input.value = '';
				});
				reload();
			};
			if(btnRelatedFilter) btnRelatedFilter.onclick = function(){
				var sourceInput = byId('relatedFilterSourceContentId');
				var relatedInput = byId('relatedFilterRelatedContentId');
				var typeInput = byId('relatedFilterRelationType');
				var statusInput = byId('relatedFilterStatus');
				abilityListFilters.relatedSourceContentId = sourceInput ? String(sourceInput.value || '').trim() : '';
				abilityListFilters.relatedRelatedContentId = relatedInput ? String(relatedInput.value || '').trim() : '';
				abilityListFilters.relatedRelationType = typeInput ? String(typeInput.value || '').trim() : '';
				abilityListFilters.relatedStatus = statusInput ? String(statusInput.value || '').trim() : '';
				reload();
			};
			if(btnRelatedFilterClear) btnRelatedFilterClear.onclick = function(){
				abilityListFilters.relatedSourceContentId = '';
				abilityListFilters.relatedRelatedContentId = '';
				abilityListFilters.relatedRelationType = '';
				abilityListFilters.relatedStatus = '';
				['relatedFilterSourceContentId','relatedFilterRelatedContentId','relatedFilterRelationType','relatedFilterStatus'].forEach(function(id){
					var input = byId(id);
					if(input) input.value = '';
				});
				reload();
			};
			if(btnSlugCheck) btnSlugCheck.onclick = runSlugCheck;
			if(btnSlugPreview) btnSlugPreview.onclick = runSlugPreview;
			if(btnSlugRuleExplain) btnSlugRuleExplain.onclick = runSlugRuleExplain;
			if(btnUnifiedRouteRuleExplain) btnUnifiedRouteRuleExplain.onclick = function(){ runUnifiedRouteRuleExplain('slugProbeResult'); };
			if(btnUnifiedRouteRuleValidate) btnUnifiedRouteRuleValidate.onclick = function(){ runUnifiedRouteRuleValidate('slugProbeResult'); };
			if(btnRouteRuleFilter) btnRouteRuleFilter.onclick = function(){
				var packInput = byId('routeRulePackId');
				var typeInput = byId('routeRuleType');
				var statusInput = byId('routeRuleStatus');
				var warningOnlyInput = byId('routeRuleWarningOnly');
				var keywordInput = byId('routeRuleKeyword');
				abilityListFilters.routeRulePackId = packInput ? String(packInput.value || '').trim() : '';
				abilityListFilters.routeRuleType = typeInput ? String(typeInput.value || '').trim() : '';
				abilityListFilters.routeRuleStatus = statusInput ? String(statusInput.value || '').trim() : '';
				abilityListFilters.routeRuleWarningOnly = warningOnlyInput ? String(warningOnlyInput.value || '').trim() : '';
				abilityListFilters.routeRuleKeyword = keywordInput ? String(keywordInput.value || '').trim() : '';
				reload();
			};
			if(btnRouteRuleFilterClear) btnRouteRuleFilterClear.onclick = function(){
				abilityListFilters.routeRulePackId = '';
				abilityListFilters.routeRuleType = '';
				abilityListFilters.routeRuleStatus = '';
				abilityListFilters.routeRuleWarningOnly = '';
				abilityListFilters.routeRuleKeyword = '';
				['routeRulePackId','routeRuleType','routeRuleStatus','routeRuleWarningOnly','routeRuleKeyword'].forEach(function(id){
					var input = byId(id);
					if(input) input.value = '';
				});
				reload();
			};
			if(btnRouteRuleRefresh) btnRouteRuleRefresh.onclick = function(){ runRouteRuleRefresh('redirectImportResult'); };
			var btnRedirectFilter = byId('btnRedirectFilter');
			var btnRedirectFilterClear = byId('btnRedirectFilterClear');
			if(btnRedirectFilter) btnRedirectFilter.onclick = function(){
				var sourceInput = byId('redirectSourcePathFilter');
				var targetInput = byId('redirectTargetUrlFilter');
				var statusInput = byId('redirectStatusFilter');
				abilityListFilters.redirectSourcePath = sourceInput ? String(sourceInput.value || '').trim() : '';
				abilityListFilters.redirectTargetUrl = targetInput ? String(targetInput.value || '').trim() : '';
				abilityListFilters.redirectStatus = statusInput ? String(statusInput.value || '').trim() : '';
				reload();
			};
			if(btnRedirectFilterClear) btnRedirectFilterClear.onclick = function(){
				abilityListFilters.redirectSourcePath = '';
				abilityListFilters.redirectTargetUrl = '';
				abilityListFilters.redirectStatus = '';
				['redirectSourcePathFilter','redirectTargetUrlFilter','redirectStatusFilter'].forEach(function(id){
					var input = byId(id);
					if(input) input.value = '';
				});
				reload();
			};
			if(btnSlugRepairPreview) btnSlugRepairPreview.onclick = function(){ runSlugRepair(false); };
			if(btnSlugRepairConfirm) btnSlugRepairConfirm.onclick = function(){ runSlugRepair(true); };
			if(btnSlugHistoryFilter) btnSlugHistoryFilter.onclick = function(){
				var contentInput = byId('slugHistoryContentId');
				var oldInput = byId('slugHistoryOldSlug');
				var newInput = byId('slugHistoryNewSlug');
				var statusInput = byId('slugHistoryStatus');
				abilityListFilters.slugHistoryContentId = contentInput ? String(contentInput.value || '').trim() : '';
				abilityListFilters.slugHistoryOldSlug = oldInput ? String(oldInput.value || '').trim() : '';
				abilityListFilters.slugHistoryNewSlug = newInput ? String(newInput.value || '').trim() : '';
				abilityListFilters.slugHistoryStatus = statusInput ? String(statusInput.value || '').trim() : '';
				reload();
			};
			if(btnSlugHistoryFilterClear) btnSlugHistoryFilterClear.onclick = function(){
				abilityListFilters.slugHistoryContentId = '';
				abilityListFilters.slugHistoryOldSlug = '';
				abilityListFilters.slugHistoryNewSlug = '';
				abilityListFilters.slugHistoryStatus = '';
				['slugHistoryContentId','slugHistoryOldSlug','slugHistoryNewSlug','slugHistoryStatus'].forEach(function(id){
					var input = byId(id);
					if(input) input.value = '';
				});
				reload();
			};
			if(btnAuditFilter) btnAuditFilter.onclick = function(){
				var targetTypeInput = byId('auditTargetType');
				var targetIdInput = byId('auditTargetId');
				var actionInput = byId('auditAction');
				abilityListFilters.auditTargetType = targetTypeInput ? String(targetTypeInput.value || '').trim() : '';
				abilityListFilters.auditTargetId = targetIdInput ? String(targetIdInput.value || '').trim() : '';
				abilityListFilters.auditAction = actionInput ? String(actionInput.value || '').trim() : '';
				reload();
			};
			if(btnAuditFilterClear) btnAuditFilterClear.onclick = function(){
				abilityListFilters.auditTargetType = '';
				abilityListFilters.auditTargetId = '';
				abilityListFilters.auditAction = '';
				['auditTargetType','auditTargetId','auditAction'].forEach(function(id){
					var input = byId(id);
					if(input) input.value = '';
				});
				reload();
			};
			if(btnAuditCleanup) btnAuditCleanup.onclick = runAuditCleanup;
			if(btnTagLinkFilter) btnTagLinkFilter.onclick = function(){
				var tagInput = byId('tagLinkTagId');
				var contentInput = byId('tagLinkContentId');
				abilityListFilters.tagLinkTagId = tagInput ? String(tagInput.value || '').trim() : '';
				abilityListFilters.tagLinkContentId = contentInput ? String(contentInput.value || '').trim() : '';
				reload();
			};
			if(btnTagLinkFilterClear) btnTagLinkFilterClear.onclick = function(){
				abilityListFilters.tagLinkTagId = '';
				abilityListFilters.tagLinkContentId = '';
				['tagLinkTagId','tagLinkContentId'].forEach(function(id){
					var input = byId(id);
					if(input) input.value = '';
				});
				reload();
			};
			if(btnTopicLinkFilter) btnTopicLinkFilter.onclick = function(){
				var topicInput = byId('topicLinkTopicId');
				var contentInput = byId('topicLinkContentId');
				abilityListFilters.topicLinkTopicId = topicInput ? String(topicInput.value || '').trim() : '';
				abilityListFilters.topicLinkContentId = contentInput ? String(contentInput.value || '').trim() : '';
				reload();
			};
			if(btnTopicLinkFilterClear) btnTopicLinkFilterClear.onclick = function(){
				abilityListFilters.topicLinkTopicId = '';
				abilityListFilters.topicLinkContentId = '';
				['topicLinkTopicId','topicLinkContentId'].forEach(function(id){
					var input = byId(id);
					if(input) input.value = '';
				});
				reload();
			};
			if(btnTagMerge) btnTagMerge.onclick = runTagMerge;
			if(btnTopicSort) btnTopicSort.onclick = runTopicSort;
			if(btnTopicArrangeLoad) btnTopicArrangeLoad.onclick = loadTopicArrange;
			if(btnSensitiveImportPreview) btnSensitiveImportPreview.onclick = function(){ runSensitiveImport(false); };
			if(btnSensitiveImportConfirm) btnSensitiveImportConfirm.onclick = function(){ runSensitiveImport(true); };
			if(btnSensitiveCheck) btnSensitiveCheck.onclick = runSensitiveCheck;
			if(btnSensitiveStats) btnSensitiveStats.onclick = showSensitiveStats;
			if(btnSensitiveCleanup) btnSensitiveCleanup.onclick = runSensitiveCleanup;
			if(btnSensitiveLogFilter) btnSensitiveLogFilter.onclick = function(){
				var targetTypeInput = byId('sensitiveLogTargetType');
				var targetIdInput = byId('sensitiveLogTargetId');
				var wordInput = byId('sensitiveLogWord');
				var fieldInput = byId('sensitiveLogFieldName');
				var actionInput = byId('sensitiveLogAction');
				abilityListFilters.sensitiveLogTargetType = targetTypeInput ? String(targetTypeInput.value || '').trim() : '';
				abilityListFilters.sensitiveLogTargetId = targetIdInput ? String(targetIdInput.value || '').trim() : '';
				abilityListFilters.sensitiveLogWord = wordInput ? String(wordInput.value || '').trim() : '';
				abilityListFilters.sensitiveLogFieldName = fieldInput ? String(fieldInput.value || '').trim() : '';
				abilityListFilters.sensitiveLogAction = actionInput ? String(actionInput.value || '').trim() : '';
				reload();
			};
			if(btnSensitiveLogFilterClear) btnSensitiveLogFilterClear.onclick = function(){
				abilityListFilters.sensitiveLogTargetType = '';
				abilityListFilters.sensitiveLogTargetId = '';
				abilityListFilters.sensitiveLogWord = '';
				abilityListFilters.sensitiveLogFieldName = '';
				abilityListFilters.sensitiveLogAction = '';
				['sensitiveLogTargetType','sensitiveLogTargetId','sensitiveLogWord','sensitiveLogFieldName','sensitiveLogAction'].forEach(function(id){
					var input = byId(id);
					if(input) input.value = '';
				});
				reload();
			};
			var btnSensitiveGroupFilter = byId('btnSensitiveGroupFilter');
			var btnSensitiveGroupFilterClear = byId('btnSensitiveGroupFilterClear');
			if(btnSensitiveGroupFilter) btnSensitiveGroupFilter.onclick = function(){
				var input = byId('sensitiveGroupFilter');
				abilityListFilters.sensitiveGroupKey = input ? String(input.value || '').trim() : '';
				reload();
			};
			if(btnSensitiveGroupFilterClear) btnSensitiveGroupFilterClear.onclick = function(){
				abilityListFilters.sensitiveGroupKey = '';
				var input = byId('sensitiveGroupFilter');
				if(input) input.value = '';
				reload();
			};
			var btnRevisionFilter = byId('btnRevisionFilter');
			var btnRevisionFilterClear = byId('btnRevisionFilterClear');
			if(btnRevisionFilter) btnRevisionFilter.onclick = function(){
				var contentInput = byId('revisionFilterContentId');
				var actionInput = byId('revisionFilterAction');
				var statusInput = byId('revisionFilterStatus');
				abilityListFilters.revisionContentId = contentInput ? String(contentInput.value || '').trim() : '';
				abilityListFilters.revisionAction = actionInput ? String(actionInput.value || '').trim() : '';
				abilityListFilters.revisionStatus = statusInput ? String(statusInput.value || '').trim() : '';
				reload();
			};
			if(btnRevisionFilterClear) btnRevisionFilterClear.onclick = function(){
				abilityListFilters.revisionContentId = '';
				abilityListFilters.revisionAction = '';
				abilityListFilters.revisionStatus = '';
				['revisionFilterContentId','revisionFilterAction','revisionFilterStatus'].forEach(function(id){
					var input = byId(id);
					if(input) input.value = '';
				});
				reload();
			};
			var btnSearchFilter = byId('btnSearchFilter');
			var btnSearchFilterClear = byId('btnSearchFilterClear');
			if(btnSearchFilter) btnSearchFilter.onclick = function(){
				var qInput = byId('searchProbeQuery');
				var categoryInput = byId('searchCategoryId');
				abilityListFilters.searchQuery = qInput ? String(qInput.value || '').trim() : '';
				abilityListFilters.searchCategoryId = categoryInput ? String(categoryInput.value || '').trim() : '';
				reload();
			};
			if(btnSearchFilterClear) btnSearchFilterClear.onclick = function(){
				abilityListFilters.searchQuery = '';
				abilityListFilters.searchCategoryId = '';
				var qInput = byId('searchProbeQuery');
				var categoryInput = byId('searchCategoryId');
				if(qInput) qInput.value = '';
				if(categoryInput) categoryInput.value = '';
				reload();
			};
			var btnWorkflowFilter = byId('btnWorkflowFilter');
			var btnWorkflowFilterClear = byId('btnWorkflowFilterClear');
			if(btnWorkflowFilter) btnWorkflowFilter.onclick = function(){
				var contentInput = byId('workflowContentId');
				var assigneeInput = byId('workflowAssigneeId');
				var actionInput = byId('workflowAction');
				var statusInput = byId('workflowStatus');
				var readStatusInput = byId('workflowReadStatus');
				abilityListFilters.workflowContentId = contentInput ? String(contentInput.value || '').trim() : '';
				abilityListFilters.workflowAssigneeId = assigneeInput ? String(assigneeInput.value || '').trim() : '';
				abilityListFilters.workflowAction = actionInput ? String(actionInput.value || '').trim() : '';
				abilityListFilters.workflowStatus = statusInput ? String(statusInput.value || '').trim() : '';
				abilityListFilters.workflowReadStatus = readStatusInput ? String(readStatusInput.value || '').trim() : '';
				reload();
			};
			if(btnWorkflowFilterClear) btnWorkflowFilterClear.onclick = function(){
				abilityListFilters.workflowContentId = '';
				abilityListFilters.workflowAssigneeId = '';
				abilityListFilters.workflowAction = '';
				abilityListFilters.workflowStatus = '';
				abilityListFilters.workflowReadStatus = '';
				['workflowContentId','workflowAssigneeId','workflowAction','workflowStatus','workflowReadStatus'].forEach(function(id){
					var input = byId(id);
					if(input) input.value = '';
				});
				reload();
			};
			var btnCommentAuditFilter = byId('btnCommentAuditFilter');
			var btnCommentAuditFilterClear = byId('btnCommentAuditFilterClear');
			var btnCommentQueueFilter = byId('btnCommentQueueFilter');
			var btnCommentQueueFilterClear = byId('btnCommentQueueFilterClear');
			if(btnCommentQueueFilter) btnCommentQueueFilter.onclick = function(){
				var contentInput = byId('commentQueueContentId');
				var authorInput = byId('commentQueueAuthorName');
				var bodyInput = byId('commentQueueBody');
				var ipInput = byId('commentQueueIp');
				abilityListFilters.commentQueueContentId = contentInput ? String(contentInput.value || '').trim() : '';
				abilityListFilters.commentQueueAuthorName = authorInput ? String(authorInput.value || '').trim() : '';
				abilityListFilters.commentQueueBody = bodyInput ? String(bodyInput.value || '').trim() : '';
				abilityListFilters.commentQueueIp = ipInput ? String(ipInput.value || '').trim() : '';
				reload();
			};
			if(btnCommentQueueFilterClear) btnCommentQueueFilterClear.onclick = function(){
				abilityListFilters.commentQueueContentId = '';
				abilityListFilters.commentQueueAuthorName = '';
				abilityListFilters.commentQueueBody = '';
				abilityListFilters.commentQueueIp = '';
				['commentQueueContentId','commentQueueAuthorName','commentQueueBody','commentQueueIp'].forEach(function(id){
					var input = byId(id);
					if(input) input.value = '';
				});
				reload();
			};
			if(btnCommentAuditFilter) btnCommentAuditFilter.onclick = function(){
				var commentInput = byId('commentAuditCommentId');
				var actionInput = byId('commentAuditAction');
				abilityListFilters.commentAuditCommentId = commentInput ? String(commentInput.value || '').trim() : '';
				abilityListFilters.commentAuditAction = actionInput ? String(actionInput.value || '').trim() : '';
				reload();
			};
			if(btnCommentAuditFilterClear) btnCommentAuditFilterClear.onclick = function(){
				abilityListFilters.commentAuditCommentId = '';
				abilityListFilters.commentAuditAction = '';
				['commentAuditCommentId','commentAuditAction'].forEach(function(id){
					var input = byId(id);
					if(input) input.value = '';
				});
				reload();
			};
			var btnCommentNotificationFilter = byId('btnCommentNotificationFilter');
			var btnCommentNotificationFilterClear = byId('btnCommentNotificationFilterClear');
			if(btnCommentNotificationFilter) btnCommentNotificationFilter.onclick = function(){
				var commentInput = byId('commentNotificationCommentId');
				var contentInput = byId('commentNotificationContentId');
				var statusInput = byId('commentNotificationStatus');
				var eventInput = byId('commentNotificationEvent');
				abilityListFilters.commentNotificationCommentId = commentInput ? String(commentInput.value || '').trim() : '';
				abilityListFilters.commentNotificationContentId = contentInput ? String(contentInput.value || '').trim() : '';
				abilityListFilters.commentNotificationStatus = statusInput ? String(statusInput.value || '').trim() : '';
				abilityListFilters.commentNotificationEvent = eventInput ? String(eventInput.value || '').trim() : '';
				reload();
			};
			if(btnCommentNotificationFilterClear) btnCommentNotificationFilterClear.onclick = function(){
				abilityListFilters.commentNotificationCommentId = '';
				abilityListFilters.commentNotificationContentId = '';
				abilityListFilters.commentNotificationStatus = '';
				abilityListFilters.commentNotificationEvent = '';
				['commentNotificationCommentId','commentNotificationContentId','commentNotificationStatus','commentNotificationEvent'].forEach(function(id){
					var input = byId(id);
					if(input) input.value = '';
				});
				reload();
			};
			var btnCommentBatchApprove = byId('btnCommentBatchApprove');
			var btnCommentBatchReject = byId('btnCommentBatchReject');
			var btnCommentModerationStats = byId('btnCommentModerationStats');
			if(btnCommentBatchApprove) btnCommentBatchApprove.onclick = function(){ runCommentStatusBatch(1); };
			if(btnCommentBatchReject) btnCommentBatchReject.onclick = function(){ runCommentStatusBatch(2); };
			if(btnCommentModerationStats) btnCommentModerationStats.onclick = function(){ showCommentModerationStats(); };
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
			post(api(cfg.saveApi), formData('abilityForm')).then(function(ret){
				showRowActionResult(ret, '保存结果', {}, '保存');
				message(ret);
				reload();
			});
		}
		function appendQuery(url, query){
			if(!query) return url;
			if(query.charAt(0) === '?') query = query.slice(1);
			if(!query) return url;
			return url + (url.indexOf('?') >= 0 ? '&' : '?') + query;
		}
		function queryFromPairs(pairs){
			var out = [];
			(pairs || []).forEach(function(pair){
				if(pair && pair[1] !== undefined && pair[1] !== null && String(pair[1]).trim() !== ''){
					out.push(encodeURIComponent(pair[0]) + '=' + encodeURIComponent(String(pair[1]).trim()));
				}
			});
			return out.join('&');
		}
		function runStaticGenerate(){
			post(api('/static/generate'), formData('staticGenerateForm')).then(function(ret){
				var data = ret && ret.data ? ret.data : ret;
				var target = byId('staticCleanResult') || byId('detail');
				target.innerHTML = renderStaticGenerateResult(data);
				message(ret);
				reload();
			});
		}
		function runStaticClean(){
			post(api('/static/clean'), formData('staticCleanForm')).then(function(ret){
				var data = ret && ret.data ? ret.data : ret;
				var target = byId('staticCleanResult') || byId('detail');
				target.innerHTML = renderStaticCleanResult(data);
				message(ret);
				reload();
			});
		}
		function runStaticRetryFailed(){
			var limitInput = byId('staticRetryFailedLimit');
			var limit = limitInput ? Number(limitInput.value || 20) : 20;
			post(api('/static/task/retry-failed'), {limit:limit}).then(function(ret){
				var data = ret && ret.data ? ret.data : ret;
				var target = byId('staticTaskRetryFailedResult') || byId('detail');
				target.innerHTML = renderStaticRetryFailedResult(data);
				message(ret);
				reload();
			});
		}
		function showStaticStats(){
			get(api('/static/stats')).then(function(ret){
				var data = ret && ret.data ? ret.data : ret;
				var target = byId('staticTaskRetryFailedResult') || byId('detail');
				if(target && target.id === 'staticTaskRetryFailedResult'){
					target.outerHTML = '<div id="staticTaskRetryFailedResult" style="margin-top:8px">' + renderStaticStats(data) + '</div>';
				}else if(target){
					target.innerHTML = renderStaticStats(data);
				}
				message(ret);
			});
		}
		function renderStaticStatusRows(rows, emptyText){
			var html = '';
			rows = Array.isArray(rows) ? rows : [];
			if(rows.length){
				rows.forEach(function(row){
					html += '<tr><td>' + esc(row.status) + '</td><td>' + esc(row.count || 0) + '</td><td>' + esc(row.lastTimeText || '-') + '</td></tr>';
				});
			}else{
				html += '<tr><td colspan="3" class="x-muted">' + esc(emptyText || '暂无状态统计') + '</td></tr>';
			}
			return html;
		}
		function renderStaticStats(data){
			data = data || {};
			var html = '<div class="x-form"><div class="x-muted">静态化统计</div>';
			html += '<table class="layui-table"><thead><tr><th>规则数</th><th>启用规则</th><th>停用规则</th><th>最近规则更新</th><th>产物数</th><th>覆盖对象</th><th>最近产物更新</th></tr></thead><tbody><tr>'
				+ '<td>' + esc(data.ruleCount || 0) + '</td>'
				+ '<td>' + esc(data.enabledRuleCount || 0) + '</td>'
				+ '<td>' + esc(data.disabledRuleCount || 0) + '</td>'
				+ '<td>' + esc(data.lastRuleUpdateTimeText || '-') + '</td>'
				+ '<td>' + esc(data.artifactCount || 0) + '</td>'
				+ '<td>' + esc(data.artifactTargetCount || 0) + '</td>'
				+ '<td>' + esc(data.lastArtifactUpdateTimeText || '-') + '</td></tr></tbody></table>';
			html += '<table class="layui-table"><thead><tr><th>任务数</th><th>成功任务</th><th>失败任务</th><th>待处理任务</th><th>最近创建</th><th>最近完成</th></tr></thead><tbody><tr>'
				+ '<td>' + esc(data.taskCount || 0) + '</td>'
				+ '<td>' + esc(data.successTaskCount || 0) + '</td>'
				+ '<td>' + esc(data.failedTaskCount || 0) + '</td>'
				+ '<td>' + esc(data.pendingTaskCount || 0) + '</td>'
				+ '<td>' + esc(data.lastTaskCreateTimeText || '-') + '</td>'
				+ '<td>' + esc(data.lastTaskFinishTimeText || '-') + '</td></tr></tbody></table>';
			html += '<div class="x-muted" style="margin-top:12px">规则状态分布</div><table class="layui-table"><thead><tr><th>状态</th><th>数量</th><th>最近时间</th></tr></thead><tbody>' + renderStaticStatusRows(data.ruleStatusStats, '暂无规则状态统计') + '</tbody></table>';
			html += '<div class="x-muted" style="margin-top:12px">任务状态分布</div><table class="layui-table"><thead><tr><th>状态</th><th>数量</th><th>最近时间</th></tr></thead><tbody>' + renderStaticStatusRows(data.taskStatusStats, '暂无任务状态统计') + '</tbody></table>';
			html += '<pre class="x-code">' + esc(JSON.stringify(data, null, 2)) + '</pre></div>';
			return html;
		}
		function renderStaticCleanResult(data){
			data = data || {};
			var rows = Array.isArray(data.rows) ? data.rows : [];
			var rowHtml = rows.map(function(row){
				return '<tr><td>' + esc(row.artifactId || 0) + '</td><td>' + esc(row.ruleId || 0) + '</td><td>' + esc(row.targetId || 0) + '</td><td>' + esc(row.path || '') + '</td><td>' + esc(row.fileDeleteAttempted ? '已尝试' : '未尝试') + '</td></tr>';
			}).join('');
			if(!rowHtml) rowHtml = '<tr><td colspan="5" class="x-muted">暂无清理产物</td></tr>';
			var html = '<div class="x-form"><div class="x-muted">静态产物清理结果</div>';
			html += '<table class="layui-table"><thead><tr><th>清理数量</th><th>本次上限</th><th>最大上限</th><th>内容筛选</th><th>规则筛选</th></tr></thead><tbody><tr>'
				+ '<td>' + esc(data.cleaned || 0) + '</td>'
				+ '<td>' + esc(data.limit || 0) + '</td>'
				+ '<td>' + esc(data.maxCleanRows || 0) + '</td>'
				+ '<td>' + esc(data.targetId || '-') + '</td>'
				+ '<td>' + esc(data.ruleId || '-') + '</td></tr></tbody></table>';
			html += '<table class="layui-table"><thead><tr><th>产物 ID</th><th>规则 ID</th><th>内容 ID</th><th>路径</th><th>文件删除</th></tr></thead><tbody>' + rowHtml + '</tbody></table>';
			html += '<div class="x-muted">清理只按固定上限删除已记录的静态产物，写入 content.static.clean 后台任务记录，不改变请求期静态文件查找顺序。</div>';
			html += '<pre class="x-code">' + esc(JSON.stringify(data, null, 2)) + '</pre></div>';
			return html;
		}
		function renderStaticGenerateResult(data){
			data = data || {};
			var html = '<div class="x-form"><div class="x-muted">静态化生成结果</div>';
			html += '<table class="layui-table"><thead><tr><th>任务ID</th><th>产物路径</th><th>结果</th><th>消息</th></tr></thead><tbody><tr>'
				+ '<td>' + esc(data.taskId || '') + '</td>'
				+ '<td>' + esc(data.path || '') + '</td>'
				+ '<td>' + esc(data.result === false ? '失败' : '成功') + '</td>'
				+ '<td>' + esc(data.message || data.error || '') + '</td></tr></tbody></table>';
			html += '<div class="x-muted">生成操作只在 content.static 启用时创建静态任务和产物，不改变请求期静态文件查找顺序。</div>';
			html += '<pre class="x-code">' + esc(JSON.stringify(data, null, 2)) + '</pre></div>';
			return html;
		}
		function renderStaticRetryRows(rows){
			var html = '';
			rows = Array.isArray(rows) ? rows : [];
			if(rows.length){
				rows.forEach(function(row){
					html += '<tr><td>' + esc(row.sourceTaskId || '') + '</td><td>' + esc(row.taskId || '') + '</td><td>' + esc(row.targetId || '') + '</td><td>' + esc(row.result ? '成功' : '失败') + '</td><td>' + esc(row.path || '') + '</td></tr>';
				});
			}else{
				html += '<tr><td colspan="5" class="x-muted">没有待重试失败任务</td></tr>';
			}
			return html;
		}
		function renderStaticRetryFailedResult(data){
			data = data || {};
			var rows = Array.isArray(data.data) ? data.data : [];
			var html = '<div class="x-form"><div class="x-muted">失败任务批量重试结果</div>';
			html += '<table class="layui-table"><thead><tr><th>本次扫描</th><th>重试成功</th><th>重试失败</th><th>本次上限</th><th>最大上限</th></tr></thead><tbody><tr>'
				+ '<td>' + esc(data.retried || 0) + '</td>'
				+ '<td>' + esc(data.succeeded || 0) + '</td>'
				+ '<td>' + esc(data.failed || 0) + '</td>'
				+ '<td>' + esc(data.limit || 0) + '</td>'
				+ '<td>' + esc(data.maxTaskRetryRows || 0) + '</td></tr></tbody></table>';
			html += '<table class="layui-table"><thead><tr><th>原任务</th><th>新任务</th><th>内容 ID</th><th>结果</th><th>产物路径</th></tr></thead><tbody>' + renderStaticRetryRows(rows) + '</tbody></table>';
			html += '<pre class="x-code">' + esc(JSON.stringify(data, null, 2)) + '</pre></div>';
			return html;
		}
		function renderStaticRulePreviewResult(data){
			data = data || {};
			var html = '<div class="x-form"><div class="x-muted">静态规则预览</div>';
			html += '<table class="layui-table"><thead><tr><th>路径规则</th><th>模板</th><th>内容 ID</th><th>内容已加载</th><th>相对路径</th><th>产物 URL</th><th>风险提示</th></tr></thead><tbody><tr>'
				+ '<td>' + esc(data.pathPattern || '') + '</td>'
				+ '<td>' + esc(data.templateName || '') + '</td>'
				+ '<td>' + esc(data.targetId || '') + '</td>'
				+ '<td>' + esc(data.targetLoaded ? '是' : '否') + '</td>'
				+ '<td>' + esc(data.relativePath || '') + '</td>'
				+ '<td>' + esc(data.artifactUrl || '') + '</td>'
				+ '<td>' + esc(data.warning || '-') + '</td></tr></tbody></table>';
			html += '<div class="x-muted">预览仅在管理期展开 pathPattern 和风险提示，不给请求路由热路径增加额外检查。</div>';
			html += '<pre class="x-code">' + esc(JSON.stringify(data, null, 2)) + '</pre></div>';
			return html;
		}
		function runStaticRulePreview(){
			var data = formData('abilityForm');
			var targetInput = byId('staticRulePreviewTargetId');
			var pathInput = byId('staticRulePreviewPath');
			data.targetId = targetInput ? Number(targetInput.value || 0) : 0;
			if(pathInput && pathInput.value) data.path = pathInput.value;
			post(api('/static/rule/preview'), data).then(function(ret){
				var data = ret && ret.data ? ret.data : ret;
				var target = byId('staticRulePreviewResult') || byId('detail');
				target.innerHTML = renderStaticRulePreviewResult(data);
				message(ret);
			});
		}
		function slugQuery(){
			var slug = byId('slugProbe') ? byId('slugProbe').value : '';
			var id = byId('slugProbeId') ? byId('slugProbeId').value : '';
			return '?slug=' + encodeURIComponent(slug || '') + (id ? '&id=' + encodeURIComponent(id) : '');
		}
		function renderSlugCheckResult(data){
			data = data || {};
			var html = '<div class="x-form"><div class="x-muted">Slug 检测/预览结果</div>';
			html += '<table class="layui-table"><thead><tr><th>Slug</th><th>可用</th><th>冲突内容</th><th>规范 URL</th><th>解析 API</th><th>风险提示</th></tr></thead><tbody><tr>'
				+ '<td>' + esc(data.slug || '') + '</td>'
				+ '<td>' + esc(data.available ? '是' : '否') + '</td>'
				+ '<td>' + esc(data.conflictId || '-') + '</td>'
				+ '<td>' + esc(data.canonicalUrl || '-') + '</td>'
				+ '<td>' + esc(data.resolveApi || '-') + '</td>'
				+ '<td>' + esc(data.warning || '-') + '</td></tr></tbody></table>';
			html += '<div class="x-muted">检测只读取后台数据并返回提交期风险，不改变动态路由匹配热路径。</div>';
			html += '<pre class="x-code">' + esc(JSON.stringify(data, null, 2)) + '</pre></div>';
			return html;
		}
		function renderSlugRepairRows(rows){
			var html = '';
			rows = Array.isArray(rows) ? rows : [];
			if(rows.length){
				rows.forEach(function(row){
					html += '<tr><td>' + esc(row.contentId || '') + '</td><td>' + esc(row.title || '') + '</td><td>' + esc(row.oldSlug || '') + '</td><td>' + esc(row.newSlug || '') + '</td><td>' + esc(row.action || '') + '</td><td>' + esc(row.changed ? '是' : '否') + '</td><td>' + esc(row.saved ? '是' : '否') + '</td><td>' + esc(row.conflictId || '-') + '</td><td>' + esc(row.warning || '') + '</td></tr>';
				});
			}else{
				html += '<tr><td colspan="9" class="x-muted">没有需要修复的 slug</td></tr>';
			}
			return html;
		}
		function renderSlugRepairResult(data){
			data = data || {};
			var rows = Array.isArray(data.rows) ? data.rows : [];
			var html = '<div class="x-form"><div class="x-muted">Slug 批量修复结果</div>';
			html += '<table class="layui-table"><thead><tr><th>确认写入</th><th>扫描数量</th><th>需变更</th><th>已保存</th><th>本次上限</th><th>最大上限</th></tr></thead><tbody><tr>'
				+ '<td>' + esc(data.confirm ? '是' : '否') + '</td>'
				+ '<td>' + esc(data.total || 0) + '</td>'
				+ '<td>' + esc(data.changed || 0) + '</td>'
				+ '<td>' + esc(data.saved || 0) + '</td>'
				+ '<td>' + esc(data.limit || 0) + '</td>'
				+ '<td>' + esc(data.maxRepairRows || 0) + '</td></tr></tbody></table>';
			html += '<table class="layui-table"><thead><tr><th>规则快照同步</th><th>运行规则刷新</th><th>运行规则数</th><th>刷新错误</th></tr></thead><tbody><tr>'
				+ '<td>' + esc(data.syncedRouteRules || 0) + '</td>'
				+ '<td>' + esc(data.runtimeRefreshed ? '成功' : (data.confirm ? '失败/未执行' : '预览未执行')) + '</td>'
				+ '<td>' + esc(data.runtimeRouteCount || 0) + '</td>'
				+ '<td>' + esc(data.routeRefreshError || '') + '</td></tr></tbody></table>';
			html += '<table class="layui-table"><thead><tr><th>内容 ID</th><th>标题</th><th>旧 slug</th><th>新 slug</th><th>动作</th><th>变更</th><th>已保存</th><th>冲突内容</th><th>风险提示</th></tr></thead><tbody>' + renderSlugRepairRows(rows) + '</tbody></table>';
			html += '<pre class="x-code">' + esc(JSON.stringify(data, null, 2)) + '</pre></div>';
			return html;
		}
		function showSlugRet(ret){
			var data = ret && ret.data ? ret.data : ret;
			var target = byId('slugProbeResult') || byId('detail');
			target.innerHTML = data && data.rows ? renderSlugRepairResult(data) : renderSlugCheckResult(data);
			message(ret);
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
		function normalizeAbilityRoutePrefix(value, fallback){
			var text = String(value || fallback || '').trim();
			text = text.replace(/\{pluginXid\}/g, pluginXid).replace(/@@PLUGIN_XID@@/g, pluginXid);
			if(!text || text.charAt(0) !== '/') text = '/' + text;
			while(text.length > 1 && text.charAt(text.length - 1) === '/') text = text.slice(0, -1);
			return text;
		}
		function routePrefixWarnings(prefix){
			var warnings = [];
			if(!prefix || prefix === '/') warnings.push('route prefix covers site root');
			if(prefix === '/admin' || prefix.indexOf('/admin/') === 0 || prefix === '/api' || prefix.indexOf('/api/') === 0) warnings.push('route prefix overlaps admin/API prefix');
			if(/\.(css|js|png|jpg|jpeg|gif|svg|ico|webp|woff2?|ttf|map)$/i.test(prefix)) warnings.push('route prefix looks like a static resource path');
			return warnings;
		}
		function routePrefixConflictWarnings(slugPrefix, redirectPrefix){
			var warnings = [];
			if(slugPrefix === redirectPrefix){
				warnings.push('slug and redirect use the same dynamic prefix; one-segment URLs can be claimed by slug before redirect');
			}
			if(slugPrefix !== '/' && redirectPrefix.indexOf(slugPrefix + '/') === 0){
				warnings.push('redirect prefix is nested under slug prefix; keep redirect URLs at two or more segments to avoid one-segment slug ambiguity');
			}
			if(redirectPrefix !== '/' && slugPrefix.indexOf(redirectPrefix + '/') === 0){
				warnings.push('slug prefix is nested under redirect prefix; check redirect source paths before enabling broad redirect rules');
			}
			return warnings;
		}
		function mergeRouteWarnings(){
			var out = [];
			for(var i=0;i<arguments.length;i++){
				var item = arguments[i];
				if(!item) continue;
				if(Array.isArray(item)){
					for(var j=0;j<item.length;j++) if(item[j]) out.push(item[j]);
				} else if(typeof item === 'string') {
					out.push(item);
				}
			}
			return out;
		}
		function loadAbilityPackContract(packKey){
			return get(api('/contracts')).then(function(ret){
				var packs = ret && ret.data && ret.data.contracts && ret.data.contracts.abilityPacks || [];
				for(var i=0;i<packs.length;i++){
					if(packs[i] && packs[i].packId === packKey) return packs[i];
				}
				return null;
			});
		}
		function runSlugRuleExplain(){
			Promise.all([get(api('/slug/preview' + slugQuery())), loadAbilityPackInstanceConfig('content.slug')]).then(function(items){
				var ret = items[0] || {};
				var config = items[1] || {};
				var prefix = normalizeAbilityRoutePrefix(config.slugRoutePrefix, '/{pluginXid}');
				var sampleSlug = (byId('slugProbe') && byId('slugProbe').value) || 'demo-slug';
				var warnings = mergeRouteWarnings(ret.warning, ret.data && ret.data.warning, routePrefixWarnings(prefix));
				showJsonResult('slugProbeResult', {
					result: ret.result !== false,
					data: {
						packId: 'content.slug',
						routeRulePreview: true,
						routePrefix: prefix,
						dynamicPattern: '^' + prefix + '/([^/]+)$',
						sampleUrl: prefix + '/' + encodeURIComponent(sampleSlug || 'demo-slug'),
						preview: ret.data || ret,
						warnings: warnings,
						ruleConflictExplain: 'Exact static routes are matched first. Slug dynamic route is checked only after static routes and stays gated by content.slug plus enablePrettySlugRoute.'
					}
				});
			});
		}
		function renderRouteRulePlan(data){
			data = data || {};
			var packs = data.packsMounted || {};
			var slug = data.slug || {};
			var redirect = data.redirect || {};
			var staticRule = data.staticRule || {};
			var staticRows = Array.isArray(staticRule.rules) ? staticRule.rules : [];
			var html = '<div class="x-form"><div class="x-muted">统一 URL 规则计划</div>';
			html += '<table class="layui-table"><thead><tr><th>路由模式</th><th>热路径策略</th><th>快照表</th><th>本次同步</th><th>持久规则</th><th>Slug</th><th>Redirect</th><th>Static</th></tr></thead><tbody><tr>'
				+ '<td>' + esc(data.routeMode || '') + '</td>'
				+ '<td>' + esc(data.hotPathPolicy || '') + '</td>'
				+ '<td>' + esc(data.independentRouteRuleStore ? '启用' : '未启用') + '</td>'
				+ '<td>' + esc(data.syncedRouteRules || 0) + '</td>'
				+ '<td>' + esc(data.persistedRouteRules || 0) + '</td>'
				+ '<td>' + esc(packs.slug ? '已挂载' : '未挂载') + '</td>'
				+ '<td>' + esc(packs.redirect ? '已挂载' : '未挂载') + '</td>'
				+ '<td>' + esc(packs.static ? '已挂载' : '未挂载') + '</td></tr></tbody></table>';
			html += '<table class="layui-table"><thead><tr><th>规则来源</th><th>启用</th><th>前缀/输出</th><th>匹配规则</th><th>来源说明</th></tr></thead><tbody>';
			html += '<tr><td>content.slug</td><td>' + esc(slug.enabled ? '是' : '否') + '</td><td>' + esc(slug.routePrefix || '') + '</td><td>' + esc(slug.dynamicPattern || '') + '</td><td>' + esc(slug.source || '') + '</td></tr>';
			html += '<tr><td>content.redirect</td><td>' + esc(redirect.enabled ? '是' : '否') + '</td><td>' + esc(redirect.routePrefix || '') + '</td><td>' + esc(redirect.dynamicPattern || '') + '</td><td>' + esc(redirect.source || '') + '</td></tr>';
			html += '<tr><td>content.static</td><td>' + esc(staticRule.enabled ? '是' : '否') + '</td><td>' + esc(staticRule.outputPrefix || '') + '</td><td>-</td><td>' + esc(staticRule.ruleSource || '') + '</td></tr></tbody></table>';
			html += '<table class="layui-table"><thead><tr><th>ID</th><th>规则名</th><th>路径规则</th><th>模板</th><th>状态</th><th>风险提示</th></tr></thead><tbody>';
			if(staticRows.length){
				staticRows.forEach(function(row){
					html += '<tr><td>' + esc(row.id || '') + '</td><td>' + esc(row.name || '') + '</td><td>' + esc(row.pathPattern || '') + '</td><td>' + esc(row.templateName || '') + '</td><td>' + esc(row.status) + '</td><td>' + esc(row.warning || '') + '</td></tr>';
				});
			}else{
				html += '<tr><td colspan="6" class="x-muted">暂无静态规则快照</td></tr>';
			}
			html += '</tbody></table><pre class="x-code">' + esc(JSON.stringify(data, null, 2)) + '</pre></div>';
			return html;
		}
		function renderRouteRuleValidation(data){
			data = data || {};
			var rows = Array.isArray(data.warnings) ? data.warnings : [];
			var html = '<div class="x-form"><div class="x-muted">URL 规则批量验证</div>';
			html += '<table class="layui-table"><thead><tr><th>管理期检查</th><th>检查上限</th><th>静态规则</th><th>风险数量</th><th>快照表</th><th>本次同步</th><th>持久规则</th></tr></thead><tbody><tr>'
				+ '<td>' + esc(data.adminTimeOnly ? '是' : '否') + '</td>'
				+ '<td>' + esc(data.syncLimit || data.limit || 0) + '</td>'
				+ '<td>' + esc(data.checkedStaticRules || 0) + '</td>'
				+ '<td>' + esc(data.warningCount || 0) + '</td>'
				+ '<td>' + esc(data.independentRouteRuleStore ? '启用' : '未启用') + '</td>'
				+ '<td>' + esc(data.syncedRouteRules || 0) + '</td>'
				+ '<td>' + esc(data.persistedRouteRules || 0) + '</td></tr></tbody></table>';
			html += '<table class="layui-table"><thead><tr><th>能力包</th><th>规则类型</th><th>来源ID</th><th>规则键</th><th>风险提示</th></tr></thead><tbody>';
			if(rows.length){
				rows.forEach(function(row){
					html += '<tr><td>' + esc(row.packId || '') + '</td><td>' + esc(row.ruleType || '') + '</td><td>' + esc(row.ruleId || '') + '</td><td>' + esc(row.ruleKey || '') + '</td><td>' + esc(row.message || row.warning || '') + '</td></tr>';
				});
			}else{
				html += '<tr><td colspan="5" class="x-muted">未发现 URL 规则风险</td></tr>';
			}
			html += '</tbody></table><div class="x-muted">验证只在管理期按固定上限读取规则，不改变静态优先、动态兜底的请求路径。</div><pre class="x-code">' + esc(JSON.stringify(data, null, 2)) + '</pre></div>';
			return html;
		}
		function runUnifiedRouteRuleExplain(targetId){
			get(api('/route-rule/plan')).then(function(planRet){
				if(planRet && planRet.result !== false && planRet.data){
					var target = byId(targetId || 'slugProbeResult') || byId('detail');
					if(target) target.innerHTML = renderRouteRulePlan(planRet.data);
					message(planRet);
					return;
				}
			Promise.all([
				loadAbilityPackContract('content.slug'),
				loadAbilityPackContract('content.redirect'),
				loadAbilityPackContract('content.static'),
				loadAbilityPackInstanceConfig('content.slug'),
				loadAbilityPackInstanceConfig('content.redirect'),
				loadAbilityPackInstanceConfig('content.static')
			]).then(function(items){
				var slugPack = items[0] || null;
				var redirectPack = items[1] || null;
				var staticPack = items[2] || null;
				var slugConfig = items[3] || {};
				var redirectConfig = items[4] || {};
				var staticConfig = items[5] || {};
				var slugPrefix = normalizeAbilityRoutePrefix(slugConfig.slugRoutePrefix, '/{pluginXid}');
				var redirectPrefix = normalizeAbilityRoutePrefix(redirectConfig.redirectRoutePrefix, '/{pluginXid}/r');
				var staticOutputPrefix = normalizeAbilityRoutePrefix(staticConfig.outputDir || 'content', '/content');
				var warnings = mergeRouteWarnings(routePrefixWarnings(slugPrefix), routePrefixWarnings(redirectPrefix), routePrefixConflictWarnings(slugPrefix, redirectPrefix));
				showJsonResult(targetId || 'slugProbeResult', {
					result: true,
					data: {
						routeMode: 'static-first dynamic-second',
						hotPathPolicy: 'request path uses static route lookup first, then precompiled dynamic route matching; conflict checks stay submit-time/admin-time only',
						unifiedRulePlan: true,
						packsMounted: {
							slug: !!slugPack,
							redirect: !!redirectPack,
							static: !!staticPack
						},
						slug: {
							enabled: !!slugPack && slugConfig.enablePrettySlugRoute !== false,
							routePrefix: slugPrefix,
							dynamicPattern: '^' + slugPrefix + '/([^/]+)$',
							sampleUrl: slugPrefix + '/demo-slug',
							source: 'content.slug canonical URL'
						},
						redirect: {
							enabled: !!redirectPack && redirectConfig.enablePrettyRedirectRoute !== false,
							routePrefix: redirectPrefix,
							dynamicPattern: '^' + redirectPrefix + '/[^?#]+$',
							samplePath: redirectPrefix + '/old',
							source: 'content.redirect source_path'
						},
						staticRule: {
							enabled: !!staticPack,
							dynamicRoute: false,
							outputPrefix: staticOutputPrefix,
							ruleSource: 'content.static static_rule.pathPattern',
							submitCheck: 'static/rule/save validates URL risk before writing rules',
							samplePath: staticOutputPrefix + '/demo.html'
						},
						priority: ['static route', 'slug dynamic route', 'redirect dynamic route', 'plugin static files', 'site static files'],
						warnings: warnings,
						crossAbilityRuleCheck: true
					}
				});
			});
			});
		}
		function showAccessStats(){
			get(api('/access/rule/stats')).then(function(ret){
				var data = ret && ret.data ? ret.data : ret;
				var rows = Array.isArray(data && data.modeStats) ? data.modeStats : [];
				var html = '<div class="x-form"><div class="x-muted">权限规则统计只读取规则表聚合结果，不改变运行期权限判断。</div>';
				html += '<table class="layui-table"><thead><tr><th>总数</th><th>启用</th><th>停用</th><th>内容规则</th><th>栏目规则</th><th>最近更新</th></tr></thead><tbody><tr><td>'+esc(data.totalCount || 0)+'</td><td>'+esc(data.enabledCount || 0)+'</td><td>'+esc(data.disabledCount || 0)+'</td><td>'+esc(data.contentRuleCount || 0)+'</td><td>'+esc(data.categoryRuleCount || 0)+'</td><td>'+esc(data.lastUpdateTimeText || '')+'</td></tr></tbody></table>';
				html += '<table class="layui-table"><thead><tr><th>权限模式</th><th>总数</th><th>启用</th><th>停用</th><th>最近更新</th></tr></thead><tbody>';
				rows.forEach(function(row){ html += '<tr><td>'+esc(row.accessMode || '')+'</td><td>'+esc(row.totalCount || 0)+'</td><td>'+esc(row.enabledCount || 0)+'</td><td>'+esc(row.disabledCount || 0)+'</td><td>'+esc(row.lastUpdateTimeText || '')+'</td></tr>'; });
				if(!rows.length) html += '<tr><td colspan="5" style="text-align:center;color:#667085;">暂无权限模式统计</td></tr>';
				html += '</tbody></table><pre class="x-code">' + esc(JSON.stringify(data, null, 2)) + '</pre></div>';
				byId('detail').innerHTML = html;
				message(ret);
			});
		}
		function runAuditCleanup(){
			var days = byId('auditKeepDays') ? Number(byId('auditKeepDays').value || 0) : 0;
			if(!days || days < 1){ message({result:false,error:'keepDays is required'}); return; }
			post(api('/audit-log/cleanup'), {keepDays:days}).then(function(ret){
				var target = byId('detail');
				if(target) target.innerHTML = renderAuditCleanupResult(ret);
				message(ret);
				reload();
			});
		}
		function renderAuditCleanupResult(ret){
			ret = ret || {};
			var html = '<div class="x-form"><div class="x-muted">审计日志清理结果</div>';
			html += '<table class="layui-table"><thead><tr><th>删除数量</th><th>保留天数</th><th>清理前时间</th><th>本次上限</th><th>最大上限</th><th>结果</th></tr></thead><tbody><tr>'
				+ '<td>' + esc(ret.deletedCount || 0) + '</td>'
				+ '<td>' + esc(ret.keepDays || '') + '</td>'
				+ '<td>' + esc(ret.beforeTime || '') + '</td>'
				+ '<td>' + esc(ret.limit || 0) + '</td>'
				+ '<td>' + esc(ret.maxCleanupRows || 0) + '</td>'
				+ '<td>' + esc(ret.result === false ? '失败' : '成功') + '</td></tr></tbody></table>';
			html += '<div class="x-muted">清理按保留天数和 maxCleanupRows 固定上限执行，只影响后台审计日志表。</div>';
			html += '<pre class="x-code">' + esc(JSON.stringify(ret, null, 2)) + '</pre></div>';
			return html;
		}
		function showAuditStats(){
			get(api('/audit-log/stats')).then(function(ret){
				var data = ret && ret.data ? ret.data : ret;
				var actionRows = Array.isArray(data && data.actionStats) ? data.actionStats : [];
				var targetRows = Array.isArray(data && data.targetTypeStats) ? data.targetTypeStats : [];
				var operatorRows = Array.isArray(data && data.operatorStats) ? data.operatorStats : [];
				function auditStatTable(rows, key, label, emptyText){
					var html = '<table class="layui-table"><thead><tr><th>'+esc(label)+'</th><th>数量</th><th>最近时间</th></tr></thead><tbody>';
					rows.forEach(function(row){ html += '<tr><td>'+esc(row[key] || '')+'</td><td>'+esc(row.count || 0)+'</td><td>'+esc(row.lastTimeText || '')+'</td></tr>'; });
					if(!rows.length) html += '<tr><td colspan="3" style="text-align:center;color:#667085;">'+esc(emptyText)+'</td></tr>';
					return html + '</tbody></table>';
				}
				function auditOperatorTable(rows){
					var html = '<table class="layui-table"><thead><tr><th>操作者类型</th><th>操作者ID</th><th>数量</th><th>最近时间</th></tr></thead><tbody>';
					rows.forEach(function(row){ html += '<tr><td>'+esc(row.operatorType || '')+'</td><td>'+esc(row.operatorId || 0)+'</td><td>'+esc(row.count || 0)+'</td><td>'+esc(row.lastTimeText || '')+'</td></tr>'; });
					if(!rows.length) html += '<tr><td colspan="4" style="text-align:center;color:#667085;">暂无操作者统计</td></tr>';
					return html + '</tbody></table>';
				}
				var html = '<div class="x-form"><div class="x-muted">审计统计只读取审计日志聚合结果，不改变审计写入和清理策略。</div>';
				html += '<table class="layui-table"><thead><tr><th>日志总数</th><th>操作者数</th><th>对象数</th><th>统计上限</th><th>最近时间</th></tr></thead><tbody><tr><td>'+esc(data.totalCount || 0)+'</td><td>'+esc(data.operatorCount || 0)+'</td><td>'+esc(data.targetCount || 0)+'</td><td>'+esc(data.statLimit || 0)+'</td><td>'+esc(data.lastTimeText || '')+'</td></tr></tbody></table>';
				html += '<div class="x-muted">动作分布</div>' + auditStatTable(actionRows, 'action', '动作', '暂无动作统计');
				html += '<div class="x-muted">操作者分布</div>' + auditOperatorTable(operatorRows);
				html += '<div class="x-muted">对象类型分布</div>' + auditStatTable(targetRows, 'targetType', '对象类型', '暂无对象类型统计');
				html += '<pre class="x-code">' + esc(JSON.stringify(data, null, 2)) + '</pre></div>';
				byId('detail').innerHTML = html;
				message(ret);
			});
		}
		function showAuditLogDetail(row){
			get(api('/audit-log/detail?id=' + encodeURIComponent(row && row.id || 0))).then(function(ret){
				var data = ret && ret.data ? ret.data : ret;
				var detail = data && data.detail ? data.detail : (data && data.detailJson ? data.detailJson : {});
				byId('detail').innerHTML = '<div class="x-form"><div class="x-muted">审计详情 #' + esc(data && data.id || '') + '</div><table class="layui-table"><tbody><tr><td>目标</td><td>' + esc((data && data.targetType || '') + '#' + (data && data.targetId || '')) + '</td></tr><tr><td>动作</td><td>' + esc(data && data.action || '') + '</td></tr><tr><td>摘要</td><td>' + esc(data && data.summary || '') + '</td></tr><tr><td>操作者</td><td>' + esc((data && data.operatorType || '') + '#' + (data && data.operatorId || '')) + '</td></tr><tr><td>IP</td><td>' + esc(data && data.ip || '') + '</td></tr><tr><td>时间</td><td>' + esc(data && data.createTimeText || '') + '</td></tr></tbody></table><pre class="x-code">' + esc(JSON.stringify(detail, null, 2)) + '</pre></div>';
				message(ret);
			});
		}
		function runTagMerge(){
			var source = byId('tagMergeSourceId') ? Number(byId('tagMergeSourceId').value || 0) : 0;
			var target = byId('tagMergeTargetId') ? Number(byId('tagMergeTargetId').value || 0) : 0;
			var deleteSource = byId('tagMergeDeleteSource') ? !!byId('tagMergeDeleteSource').checked : true;
			if(!source || !target || source === target){ message({result:false,error:'source and target tag ids are required'}); return; }
			post(api('/tag/merge'), {sourceTagId:source,targetTagId:target,deleteSource:deleteSource}).then(function(ret){
				var target = byId('tagMergeResult') || byId('detail');
				target.innerHTML = renderTagMergeResult(ret);
				message(ret);
				reload();
			});
		}
		function renderTagMergeResult(ret){
			ret = ret || {};
			var html = '<div class="x-form"><div class="x-muted">标签合并结果</div>';
			html += '<table class="layui-table"><thead><tr><th>来源标签</th><th>目标标签</th><th>新增目标绑定</th><th>移除来源绑定</th><th>删除来源标签</th></tr></thead><tbody><tr>'
				+ '<td>' + esc(ret.sourceTagId || '') + '</td>'
				+ '<td>' + esc(ret.targetTagId || '') + '</td>'
				+ '<td>' + esc(ret.movedCount || 0) + '</td>'
				+ '<td>' + esc(ret.deletedBindingCount || 0) + '</td>'
				+ '<td>' + esc(ret.sourceDeleted ? '是' : '否') + '</td></tr></tbody></table>';
			html += '<div class="x-muted">合并使用 INSERT OR IGNORE 保留既有目标绑定，随后移除来源绑定并刷新标签计数。</div>';
			html += '<pre class="x-code">' + esc(JSON.stringify(ret, null, 2)) + '</pre></div>';
			return html;
		}
		function runTopicSort(){
			var input = byId('topicSortItems');
			var text = input && input.value ? input.value : '[]';
			var items = [];
			try {
				items = JSON.parse(text);
			} catch(e) {
				message({result:false,error:'invalid sort json'});
				return;
			}
			if(!Array.isArray(items) || items.length > 500){ message({result:false,error:'items must be an array with <= 500 rows'}); return; }
			post(api('/topic/content/sort'), {items:items}).then(function(ret){
				var target = byId('topicSortResult') || byId('detail');
				target.innerHTML = renderTopicSortResult(ret);
				message(ret);
				reload();
			});
		}
		function renderTopicSortResult(ret){
			ret = ret || {};
			var html = '<div class="x-form"><div class="x-muted">专题内容排序结果</div>';
			html += '<table class="layui-table"><thead><tr><th>提交行数</th><th>更新行数</th><th>单次上限</th><th>结果</th></tr></thead><tbody><tr>'
				+ '<td>' + esc(ret.itemCount || 0) + '</td>'
				+ '<td>' + esc(ret.updatedCount || 0) + '</td>'
				+ '<td>500</td>'
				+ '<td>' + esc(ret.result === false ? '失败' : '成功') + '</td></tr></tbody></table>';
			html += '<div class="x-muted">排序只更新 topic_content.sort，单次最多 500 行，适合管理页拖拽后保存。</div>';
			html += '<pre class="x-code">' + esc(JSON.stringify(ret, null, 2)) + '</pre></div>';
			return html;
		}
		function renderTopicArrangeRows(rows){
			topicArrangeRows = Array.isArray(rows) ? rows.slice() : [];
			var target = byId('topicArrangeList');
			var input = byId('topicSortItems');
			if(input){
				input.value = JSON.stringify(topicArrangeRows.map(function(row, index){
					return {id:Number(row.id || 0),sort:(index + 1) * 10};
				}), null, 2);
			}
			if(!target) return;
			if(!topicArrangeRows.length){
				target.innerHTML = '<div class="x-empty">No topic contents loaded</div>';
				return;
			}
			target.innerHTML = '<div class="topic-arrange-row topic-arrange-head"><span>ID</span><span>Content ID</span><span>Title</span><span>Sort</span><span>Move</span></div>' + topicArrangeRows.map(function(row, index){
				return '<div class="topic-arrange-row" data-index="'+index+'"><span>'+esc(row.id)+'</span><span>'+esc(row.contentId)+'</span><span>'+esc(row.contentTitle || '')+'</span><span>'+esc(row.sort)+'</span><span><button type="button" class="layui-btn layui-btn-primary layui-btn-xs" data-topic-move="-1" data-index="'+index+'">Up</button><button type="button" class="layui-btn layui-btn-primary layui-btn-xs" data-topic-move="1" data-index="'+index+'">Down</button></span></div>';
			}).join('');
		}
		function loadTopicArrange(){
			var topicIdInput = byId('topicArrangeTopicId');
			var topicId = topicIdInput ? Number(topicIdInput.value || 0) : 0;
			var url = api('/topic/content/list' + (topicId > 0 ? '?topicId=' + encodeURIComponent(topicId) : ''));
			get(url).then(function(ret){
				var rows = ret && ret.data ? ret.data : [];
				renderTopicArrangeRows(rows);
				message(ret);
			});
		}
		function moveTopicArrange(index, step){
			index = Number(index);
			step = Number(step);
			var next = index + step;
			if(index < 0 || next < 0 || index >= topicArrangeRows.length || next >= topicArrangeRows.length) return;
			var row = topicArrangeRows[index];
			topicArrangeRows[index] = topicArrangeRows[next];
			topicArrangeRows[next] = row;
			renderTopicArrangeRows(topicArrangeRows);
		}
		function sensitiveImportItems(){
			var input = byId('sensitiveImportJson');
			var text = input && input.value ? input.value : '[]';
			try {
				var rows = JSON.parse(text);
				return Array.isArray(rows) ? rows : [];
			} catch(e) {
				message({result:false,error:'invalid sensitive import json'});
				return [];
			}
		}
		function runSensitiveImport(confirm){
			var rows = sensitiveImportItems();
			if(!rows.length || rows.length > 1000){ message({result:false,error:'items must be 1-1000 rows'}); return; }
			post(api('/sensitive/word/import'), {confirm:!!confirm,items:rows}).then(function(ret){
				var target = byId('sensitiveImportResult') || byId('detail');
				target.innerHTML = renderSensitiveImportResult(ret);
				message(ret);
				reload();
			});
		}
		function renderSensitiveImportRows(rows){
			var html = '';
			rows = Array.isArray(rows) ? rows : [];
			if(rows.length){
				rows.forEach(function(row){
					html += '<tr><td>' + esc(row.word || '') + '</td><td>' + esc(row.level || '') + '</td><td>' + esc(row.scope || '') + '</td><td>' + esc(row.groupKey || '') + '</td><td>' + esc(row.status) + '</td><td>' + esc(row.result || '') + '</td><td>' + esc(row.reason || '') + '</td></tr>';
				});
			}else{
				html += '<tr><td colspan="7" class="x-muted">没有导入行</td></tr>';
			}
			return html;
		}
		function renderSensitiveImportResult(ret){
			ret = ret || {};
			var rows = Array.isArray(ret.data) ? ret.data : [];
			var html = '<div class="x-form"><div class="x-muted">敏感词导入结果</div>';
			html += '<table class="layui-table"><thead><tr><th>确认写入</th><th>总行数</th><th>已插入</th><th>已跳过</th><th>最大行数</th></tr></thead><tbody><tr>'
				+ '<td>' + esc(ret.confirmed ? '是' : '否') + '</td>'
				+ '<td>' + esc(ret.itemCount || rows.length || 0) + '</td>'
				+ '<td>' + esc(ret.insertedCount || 0) + '</td>'
				+ '<td>' + esc(ret.skippedCount || 0) + '</td>'
				+ '<td>' + esc(ret.maxImportRows || 0) + '</td></tr></tbody></table>';
			html += '<table class="layui-table"><thead><tr><th>词条</th><th>级别</th><th>作用域</th><th>分组</th><th>状态</th><th>结果</th><th>原因</th></tr></thead><tbody>' + renderSensitiveImportRows(rows) + '</tbody></table>';
			html += '<pre class="x-code">' + esc(JSON.stringify(ret, null, 2)) + '</pre></div>';
			return html;
		}
		function renderSensitiveHitRows(rows){
			var html = '';
			rows = Array.isArray(rows) ? rows : [];
			if(rows.length){
				rows.forEach(function(row){
					html += '<tr><td>' + esc(row.wordId || '') + '</td><td>' + esc(row.word || '') + '</td><td>' + esc(row.level || '') + '</td><td>' + esc(row.field || '') + '</td><td>' + esc(row.matchMode || '') + '</td><td>' + esc(row.replacement || '') + '</td></tr>';
				});
			}else{
				html += '<tr><td colspan="6" class="x-muted">未命中敏感词</td></tr>';
			}
			return html;
		}
		function renderSensitiveCheckResult(ret){
			ret = ret || {};
			var rows = Array.isArray(ret.hits) ? ret.hits : [];
			var html = '<div class="x-form"><div class="x-muted">敏感词检测结果</div>';
			html += '<table class="layui-table"><thead><tr><th>是否通过</th><th>命中数量</th><th>检测大小上限</th></tr></thead><tbody><tr>'
				+ '<td>' + esc(ret.passed ? '是' : '否') + '</td>'
				+ '<td>' + esc(ret.hitCount || 0) + '</td>'
				+ '<td>' + esc(ret.maxCheckBytes || 0) + '</td></tr></tbody></table>';
			html += '<table class="layui-table"><thead><tr><th>词 ID</th><th>词条</th><th>级别</th><th>字段</th><th>匹配模式</th><th>替换值</th></tr></thead><tbody>' + renderSensitiveHitRows(rows) + '</tbody></table>';
			html += '<pre class="x-code">' + esc(JSON.stringify(ret, null, 2)) + '</pre></div>';
			return html;
		}
		function runSensitiveCheck(){
			var target = byId('sensitiveCheckText');
			var scope = byId('sensitiveCheckScope');
			var text = target && target.value ? target.value : '';
			if(!text){ message({result:false,error:'check text is required'}); return; }
			post(pub('/sensitive/check'), {scope:(scope && scope.value) || 'check', text:text}).then(function(ret){
				var target = byId('sensitiveCheckResult') || byId('detail');
				target.innerHTML = renderSensitiveCheckResult(ret);
				message(ret);
			});
		}
		function showSensitiveStats(){
			get(api('/sensitive/stats')).then(function(ret){
				var data = ret && ret.data ? ret.data : ret;
				var target = byId('sensitiveStatsResult') || byId('detail');
				if(target && target.id === 'sensitiveStatsResult'){
					target.outerHTML = '<div id="sensitiveStatsResult" style="margin-top:8px">' + renderSensitiveStats(data) + '</div>';
				}else if(target){
					target.innerHTML = renderSensitiveStats(data);
				}
				message(ret);
			});
		}
		function renderSensitiveStats(data){
			data = data || {};
			var html = '<div class="x-form"><div class="x-muted">词库统计</div>';
			html += '<table class="layui-table"><thead><tr><th>词条总数</th><th>启用词条</th><th>停用词条</th><th>分组数</th><th>作用域数</th></tr></thead><tbody><tr>'
				+ '<td>' + esc(data.wordCount || 0) + '</td>'
				+ '<td>' + esc(data.enabledWordCount || 0) + '</td>'
				+ '<td>' + esc(data.disabledWordCount || 0) + '</td>'
				+ '<td>' + esc(data.groupCount || 0) + '</td>'
				+ '<td>' + esc(data.scopeCount || 0) + '</td></tr></tbody></table>';
			html += '<table class="layui-table"><thead><tr><th>命中日志</th><th>近期命中</th><th>统计窗口</th><th>窗口起点</th><th>最近命中</th></tr></thead><tbody><tr>'
				+ '<td>' + esc(data.hitLogCount || 0) + '</td>'
				+ '<td>' + esc(data.recentHitCount || 0) + '</td>'
				+ '<td>' + esc(data.recentDays || 0) + ' 天</td>'
				+ '<td>' + esc(data.recentSinceText || '-') + '</td>'
				+ '<td>' + esc(data.lastHitTimeText || '-') + '</td></tr></tbody></table>';
			html += '<div class="x-muted">近期高频命中词（最多 ' + esc(data.topLimit || 0) + ' 条）</div><table class="layui-table"><thead><tr><th>敏感词</th><th>命中次数</th><th>最近命中</th></tr></thead><tbody>';
			(Array.isArray(data.topWords) ? data.topWords : []).forEach(function(row){
				html += '<tr><td>' + esc(row.word || '') + '</td><td>' + esc(row.count || 0) + '</td><td>' + esc(row.lastHitTimeText || '-') + '</td></tr>';
			});
			if(!Array.isArray(data.topWords) || !data.topWords.length) html += '<tr><td colspan="3" class="x-muted">暂无近期命中词</td></tr>';
			html += '</tbody></table><div class="x-muted">近期高频分组</div><table class="layui-table"><thead><tr><th>分组</th><th>命中次数</th><th>最近命中</th></tr></thead><tbody>';
			(Array.isArray(data.topGroups) ? data.topGroups : []).forEach(function(row){
				html += '<tr><td>' + esc(row.groupKey || '') + '</td><td>' + esc(row.count || 0) + '</td><td>' + esc(row.lastHitTimeText || '-') + '</td></tr>';
			});
			if(!Array.isArray(data.topGroups) || !data.topGroups.length) html += '<tr><td colspan="3" class="x-muted">暂无近期命中分组</td></tr>';
			html += '</tbody></table>';
			html += '<div class="x-muted">统计只读取词库和命中日志聚合值，不改变内容保存、评论提交或检测接口的扫描路径。</div>';
			html += '<pre class="x-code">' + esc(JSON.stringify(data, null, 2)) + '</pre></div>';
			return html;
		}
		function runSensitiveCleanup(){
			var daysInput = byId('sensitiveCleanupDays');
			var limitInput = byId('sensitiveCleanupLimit');
			var keepDays = daysInput ? Number(daysInput.value || 0) : 0;
			var limit = limitInput ? Number(limitInput.value || 0) : 0;
			if(keepDays <= 0){ message({result:false,error:'keep days is required'}); return; }
			post(api('/sensitive/log/cleanup'), {keepDays:keepDays,limit:limit}).then(function(ret){
				var target = byId('sensitiveCleanupResult') || byId('detail');
				target.innerHTML = renderSensitiveCleanupResult(ret);
				message(ret);
				reload();
			});
		}
		function renderSensitiveCleanupResult(ret){
			ret = ret || {};
			var html = '<div class="x-form"><div class="x-muted">敏感词命中日志清理结果</div>';
			html += '<table class="layui-table"><thead><tr><th>删除数量</th><th>保留天数</th><th>清理时间点</th><th>本次上限</th><th>最大上限</th></tr></thead><tbody><tr>'
				+ '<td>' + esc(ret.deleted || 0) + '</td>'
				+ '<td>' + esc(ret.keepDays || 0) + '</td>'
				+ '<td>' + esc(ret.beforeTime || 0) + '</td>'
				+ '<td>' + esc(ret.limit || 0) + '</td>'
				+ '<td>' + esc(ret.maxCleanupRows || 0) + '</td></tr></tbody></table>';
			html += '<div class="x-muted">清理只按固定上限删除旧命中日志，不改变内容保存、评论提交或检测接口的扫描路径。</div>';
			html += '<pre class="x-code">' + esc(JSON.stringify(ret, null, 2)) + '</pre></div>';
			return html;
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
			post(api('/redirect/import'), {confirm:!!confirm,items:rows}).then(function(ret){
				var target = byId('redirectImportResult') || byId('detail');
				target.innerHTML = renderRedirectImportResult(ret);
				message(ret);
				reload();
			});
		}
		function renderRedirectImportRows(rows){
			var html = '';
			rows = Array.isArray(rows) ? rows : [];
			if(rows.length){
				rows.forEach(function(row){
					html += '<tr><td>' + esc(row.rowIndex || '') + '</td><td>' + esc(row.sourcePath || '') + '</td><td>' + esc(row.targetUrl || '') + '</td><td>' + esc(row.statusCode || '') + '</td><td>' + esc(row.ok ? '是' : '否') + '</td><td>' + esc(row.saved ? '是' : '否') + '</td><td>' + esc(row.message || '') + '</td><td>' + esc(row.warning || '') + '</td></tr>';
				});
			}else{
				html += '<tr><td colspan="8" class="x-muted">没有导入行</td></tr>';
			}
			return html;
		}
		function renderRedirectImportResult(data){
			data = data || {};
			var rows = Array.isArray(data.data) ? data.data : (Array.isArray(data) ? data : []);
			var html = '<div class="x-form"><div class="x-muted">跳转规则导入结果</div>';
			html += '<table class="layui-table"><thead><tr><th>总行数</th><th>有效行</th><th>已保存</th><th>失败行</th><th>确认写入</th></tr></thead><tbody><tr>'
				+ '<td>' + esc(data.total || rows.length || 0) + '</td>'
				+ '<td>' + esc(data.valid || 0) + '</td>'
				+ '<td>' + esc(data.saved || 0) + '</td>'
				+ '<td>' + esc(data.failed || 0) + '</td>'
				+ '<td>' + esc((data.saved || 0) > 0 ? '是' : '否') + '</td></tr></tbody></table>';
			html += '<table class="layui-table"><thead><tr><th>同步 URL 规则</th><th>运行时刷新</th><th>动态路由数</th><th>刷新错误</th></tr></thead><tbody><tr>'
				+ '<td>' + esc(data.syncedRouteRules || 0) + '</td>'
				+ '<td>' + esc(data.runtimeRefreshed ? '成功' : (data.confirm ? '失败/未执行' : '预览未执行')) + '</td>'
				+ '<td>' + esc(data.runtimeRouteCount || 0) + '</td>'
				+ '<td>' + esc(data.routeRefreshError || '') + '</td></tr></tbody></table>';
			html += '<table class="layui-table"><thead><tr><th>行号</th><th>来源路径</th><th>目标 URL</th><th>状态码</th><th>有效</th><th>已保存</th><th>消息</th><th>风险提示</th></tr></thead><tbody>' + renderRedirectImportRows(rows) + '</tbody></table>';
			html += '<div class="x-muted">导入预检和跳转链路风险只在管理期执行，不在请求路由热路径追加全站检查。</div>';
			html += '<pre class="x-code">' + esc(JSON.stringify(data, null, 2)) + '</pre></div>';
			return html;
		}
		function runRedirectRuleExplain(){
			loadAbilityPackInstanceConfig('content.redirect').then(function(config){
				config = config || {};
				var prefix = normalizeAbilityRoutePrefix(config.redirectRoutePrefix, '/{pluginXid}/r');
				var rows = redirectImportItems();
				if(!rows.length) rows = [{sourcePath:prefix + '/old',targetUrl:'/new',statusCode:301,status:1}];
				post(api('/redirect/import'), {confirm:false,items:rows}).then(function(ret){
					var warnings = mergeRouteWarnings(ret && ret.warning, ret && ret.data && ret.data.warning, routePrefixWarnings(prefix));
					showJsonResult('redirectImportResult', {
						result: ret && ret.result !== false,
						data: {
							packId: 'content.redirect',
							routeRulePreview: true,
							routePrefix: prefix,
							dynamicPattern: '^' + prefix + '/[^?#]+$',
							samplePath: rows[0] && rows[0].sourcePath ? rows[0].sourcePath : prefix + '/old',
							preview: ret && ret.data ? ret.data : ret,
							warnings: warnings,
							ruleConflictExplain: 'Exact static routes are matched first. Redirect dynamic route is checked after static routes and before plugin/site static files; chain and loop warnings are submit-time checks only.'
						}
					});
				});
			});
		}
		function runUnifiedRouteRuleValidate(targetId){
			get(api('/route-rule/validate')).then(function(ret){
				var data = ret && ret.data ? ret.data : ret;
				var target = byId(targetId || 'slugProbeResult') || byId('detail');
				if(target) target.innerHTML = renderRouteRuleValidation(data);
				message(ret);
			});
		}
		function renderRouteRuleStats(data){
			data = data || {};
			var packRows = Array.isArray(data.packStats) ? data.packStats : [];
			var typeRows = Array.isArray(data.ruleTypeStats) ? data.ruleTypeStats : [];
			var html = '<div class="x-form"><div class="x-muted">URL 规则统计</div>';
			html += '<table class="layui-table"><thead><tr><th>规则总数</th><th>启用</th><th>停用</th><th>风险规则</th><th>本次同步</th><th>统计上限</th><th>最近更新</th></tr></thead><tbody><tr>'
				+ '<td>' + esc(data.totalCount || 0) + '</td>'
				+ '<td>' + esc(data.enabledCount || 0) + '</td>'
				+ '<td>' + esc(data.disabledCount || 0) + '</td>'
				+ '<td>' + esc(data.warningCount || 0) + '</td>'
				+ '<td>' + esc(data.syncedRouteRules || 0) + '</td>'
				+ '<td>' + esc(data.limit || 0) + '</td>'
				+ '<td>' + esc(data.lastUpdateTimeText || '') + '</td></tr></tbody></table>';
			function statTable(rows, key, label, emptyText){
				var part = '<table class="layui-table"><thead><tr><th>' + esc(label) + '</th><th>规则数</th><th>风险数</th><th>最近更新</th></tr></thead><tbody>';
				if(rows.length){
					rows.forEach(function(row){ part += '<tr><td>' + esc(row[key] || '') + '</td><td>' + esc(row.count || 0) + '</td><td>' + esc(row.warningCount || 0) + '</td><td>' + esc(row.lastUpdateTimeText || '') + '</td></tr>'; });
				}else{
					part += '<tr><td colspan="4" class="x-muted">' + esc(emptyText) + '</td></tr>';
				}
				return part + '</tbody></table>';
			}
			html += '<div class="x-muted">按能力包分布</div>' + statTable(packRows, 'packId', '能力包', '暂无能力包统计');
			html += '<div class="x-muted">按规则类型分布</div>' + statTable(typeRows, 'ruleType', '规则类型', '暂无规则类型统计');
			html += '<pre class="x-code">' + esc(JSON.stringify(data, null, 2)) + '</pre></div>';
			return html;
		}
		function showRouteRuleStats(targetId){
			get(api('/route-rule/stats')).then(function(ret){
				var data = ret && ret.data ? ret.data : ret;
				var target = byId(targetId || 'redirectImportResult') || byId('detail');
				if(target) target.innerHTML = renderRouteRuleStats(data);
				message(ret);
			});
		}
		function renderRouteRuleRefreshResult(ret){
			ret = ret || {};
			var html = '<div class="x-form"><div class="x-muted">URL 规则快照刷新结果</div>';
			html += '<table class="layui-table"><thead><tr><th>管理期操作</th><th>同步前</th><th>本次同步</th><th>同步后</th><th>同步上限</th><th>最大上限</th></tr></thead><tbody><tr>'
				+ '<td>' + esc(ret.adminTimeOnly ? '是' : '否') + '</td>'
				+ '<td>' + esc(ret.beforeCount || 0) + '</td>'
				+ '<td>' + esc(ret.syncedRouteRules || 0) + '</td>'
				+ '<td>' + esc(ret.afterCount || 0) + '</td>'
				+ '<td>' + esc(ret.limit || 0) + '</td>'
				+ '<td>' + esc(ret.maxListRows || 0) + '</td></tr></tbody></table>';
			html += '<div class="x-muted">' + esc(ret.hotPathPolicy || '刷新只更新管理期 URL 规则快照表，不改变请求期路由匹配顺序。') + '</div>';
			html += '<pre class="x-code">' + esc(JSON.stringify(ret, null, 2)) + '</pre></div>';
			return html;
		}
		function runRouteRuleRefresh(targetId){
			post(api('/route-rule/refresh'), {}).then(function(ret){
				var target = byId(targetId || 'redirectImportResult') || byId('detail');
				if(target) target.innerHTML = renderRouteRuleRefreshResult(ret);
				message(ret);
				reload();
			});
		}
		function revisionValueText(value){
			if(value && typeof value === 'object'){
				try { return JSON.stringify(value, null, 2); } catch(e) {}
			}
			return String(value == null ? '' : value);
		}
		function revisionRowValue(row, key){
			if(!row) return '';
			var rawKey = key + 'Value';
			return Object.prototype.hasOwnProperty.call(row, rawKey) ? row[rawKey] : row[key];
		}
		function revisionFieldText(row){ return String((row && (row.field || row.title)) || '').toLowerCase(); }
		function isRevisionStructuredValue(value){ return value && typeof value === 'object'; }
		function renderRevisionStructuredValue(value, peer){
			var valueObj = isRevisionStructuredValue(value);
			var peerObj = isRevisionStructuredValue(peer);
			var keys = [];
			var seen = {};
			if(!valueObj && !peerObj) return '';
			if(Array.isArray(value) || Array.isArray(peer)){
				return '<pre class="x-code revision-structured-diff">' + esc(revisionValueText(value)) + '</pre>';
			}
			Object.keys(valueObj ? value : {}).forEach(function(k){ seen[k]=true; keys.push(k); });
			Object.keys(peerObj ? peer : {}).forEach(function(k){ if(!seen[k]){ seen[k]=true; keys.push(k); } });
			if(!keys.length) return '<pre class="x-code revision-structured-diff">' + esc(revisionValueText(value)) + '</pre>';
			return '<table class="layui-table revision-structured-diff"><thead><tr><th style="width:150px">Key</th><th>Value</th></tr></thead><tbody>' + keys.map(function(k){
				var hasValue = valueObj && Object.prototype.hasOwnProperty.call(value, k);
				var hasPeer = peerObj && Object.prototype.hasOwnProperty.call(peer, k);
				var changed = hasValue && hasPeer && revisionValueText(value[k]) !== revisionValueText(peer[k]);
				var state = !hasPeer ? 'added' : (!hasValue ? 'removed' : (changed ? 'changed' : 'same'));
				return '<tr data-state="' + state + '"><td>' + esc(k) + '</td><td><pre class="x-code">' + esc(hasValue ? revisionValueText(value[k]) : '') + '</pre></td></tr>';
			}).join('') + '</tbody></table>';
		}
		function isRevisionUrlValue(text){
			return /^https?:\/\//i.test(text) || text.indexOf('/') === 0 || text.indexOf('plugin-static/') === 0 || text.indexOf('static/') === 0;
		}
		function isRevisionImageValue(text, row){
			var path = String(text || '').split(/[?#]/)[0].toLowerCase();
			var field = revisionFieldText(row);
			return isRevisionUrlValue(text) && (/\.(png|jpe?g|gif|webp|svg|avif)$/.test(path) || field.indexOf('image') >= 0 || field.indexOf('cover') >= 0);
		}
		function renderRevisionMediaValue(value, peer, row){
			var text = revisionValueText(value);
			var field = revisionFieldText(row);
			if(!text || !isRevisionUrlValue(text)) return '';
			if(isRevisionImageValue(text, row)){
				return '<div class="revision-media-diff"><img src="' + esc(text) + '" alt="" style="display:block;max-width:220px;max-height:140px;margin-bottom:6px;border:1px solid #eaecf0;background:#f8fafc;"><pre class="x-code">' + esc(text) + '</pre></div>';
			}
			if(/url|link|path|file|media|asset|cover/.test(field)){
				return '<div class="revision-url-diff"><a href="' + esc(text) + '" target="_blank" rel="noopener">打开 URL</a><pre class="x-code">' + esc(text) + '</pre></div>';
			}
			return '';
		}
		function renderRevisionValue(value, peer, row, resolvedLabel){
			if(row && resolvedLabel){
				var labelClass = row.diffMode === 'relation' ? 'revision-relation-diff' : 'revision-enum-diff';
				return '<div class="' + labelClass + '"><pre class="x-code">' + esc(revisionValueText(value)) + '</pre><div class="x-muted">label: ' + esc(resolvedLabel) + '</div></div>';
			}
			if(row && row.diffMode === 'numeric' && Object.prototype.hasOwnProperty.call(row, 'numberDelta')){
				return '<div class="revision-number-diff"><pre class="x-code">' + esc(revisionValueText(value)) + '</pre><div class="x-muted">delta: ' + esc(row.numberDelta) + '</div></div>';
			}
			var structuredHtml = renderRevisionStructuredValue(value, peer);
			if(structuredHtml) return structuredHtml;
			var mediaHtml = renderRevisionMediaValue(value, peer, row);
			if(mediaHtml) return mediaHtml;
			var text = revisionValueText(value);
			var peerText = revisionValueText(peer);
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
			if(lines.length > max) html += '<div class="x-muted">... 还有 ' + esc(lines.length - max) + ' 行未显示</div>';
			return html + '</div>';
		}
		function renderRevisionChangeTable(changes){
			var rows = Array.isArray(changes) ? changes : [];
			var html = '<table class="layui-table"><thead><tr><th style="width:160px">字段</th><th>变更前</th><th>变更后</th></tr></thead><tbody>';
			rows.forEach(function(row){
				var beforeValue = revisionRowValue(row, 'before');
				var afterValue = revisionRowValue(row, 'after');
				var summary = (row.changeKind || 'modified') + ' / ' + (row.beforeKind || '') + ' -> ' + (row.afterKind || '');
				if(row.typeChanged) summary += ' / 类型变化';
				summary += ' / ' + (row.beforeLength || 0) + ' -> ' + (row.afterLength || 0) + ' 字节';
				if(row.diffMode) summary += ' / ' + row.diffMode;
				html += '<tr><td><b>' + esc(row.title || row.field || '') + '</b><div class="x-muted">' + esc(row.field || '') + '</div><div class="x-muted">' + esc(row.fieldType || '') + '</div><div class="x-muted">' + esc(summary) + '</div></td><td>' + renderRevisionValue(beforeValue, afterValue, row, row.beforeLabel) + '</td><td>' + renderRevisionValue(afterValue, beforeValue, row, row.afterLabel) + '</td></tr>';
			});
			if(!rows.length) html += '<tr><td colspan="3" style="text-align:center;color:#667085;">没有字段变更</td></tr>';
			return html + '</tbody></table>';
		}
		function showRevisionDiff(row){
			get(api('/revision/diff?id=' + encodeURIComponent(row.id))).then(function(ret){
				var data = ret && ret.data ? ret.data : {};
				var changes = data.changes || [];
				byId('detail').innerHTML = '<div class="x-form"><div class="x-muted">版本差异 #' + esc(row.id) + ' - ' + esc(data.changeCount || changes.length || 0) + ' 个字段变更</div>' + renderRevisionChangeTable(changes) + '</div>';
				message(ret);
			});
		}
		function restoreRevision(row){
			post(api('/revision/restore'), {id:row.id}).then(function(ret){
				var target = byId('detail');
				if(target) target.innerHTML = renderRevisionRestoreResult(ret);
				message(ret);
				reload();
			});
		}
		function renderRevisionRestoreResult(ret){
			ret = ret || {};
			var html = '<div class="x-form"><div class="x-muted">版本恢复结果</div>';
			html += '<table class="layui-table"><thead><tr><th>版本ID</th><th>内容ID</th><th>标题</th><th>状态</th><th>栏目ID</th><th>草稿</th><th>更新时间</th><th>结果</th></tr></thead><tbody><tr>'
				+ '<td>' + esc(ret.revisionId || '') + '</td>'
				+ '<td>' + esc(ret.contentId || '') + '</td>'
				+ '<td>' + esc(ret.title || '') + '</td>'
				+ '<td>' + esc(ret.status || '') + '</td>'
				+ '<td>' + esc(ret.categoryId || '') + '</td>'
				+ '<td>' + esc(ret.isDraft ? '是' : '否') + '</td>'
				+ '<td>' + esc(ret.updateTime || '') + '</td>'
				+ '<td>' + esc(ret.result === false ? '失败' : '成功') + '</td></tr></tbody></table>';
			html += '<div class="x-muted">恢复会同步内容主表、栏目绑定、派生数据、静态生成和审计记录。</div>';
			html += '<pre class="x-code">' + esc(JSON.stringify(ret, null, 2)) + '</pre></div>';
			return html;
		}
		function showRevisionRestorePreview(row){
			get(api('/revision/restore-preview?id=' + encodeURIComponent(row.id))).then(function(ret){
				var data = ret && ret.data ? ret.data : {};
				var changes = data.changes || [];
				var html = '<div class="x-form"><div class="x-muted">恢复预览 #' + esc(row.id) + ' - ' + esc(data.changeCount || changes.length || 0) + ' 个字段变更</div>' + renderRevisionChangeTable(changes) + '<div class="x-dialog-actions"><button type="button" class="layui-btn layui-btn-primary" id="btnRevisionRestoreCancel">取消</button><button type="button" class="layui-btn layui-btn-danger" id="btnRevisionRestoreConfirm">恢复</button></div></div>';
				byId('detail').innerHTML = html;
				var cancelBtn = byId('btnRevisionRestoreCancel');
				var restoreBtn = byId('btnRevisionRestoreConfirm');
				if(cancelBtn) cancelBtn.onclick = function(){ byId('detail').innerHTML = ''; };
				if(restoreBtn) restoreBtn.onclick = function(){ restoreRevision(row); };
				message(ret);
			});
		}
		function showRevisionStats(){
			get(api('/revision/stats')).then(function(ret){
				var data = ret && ret.data ? ret.data : ret;
				var actionRows = Array.isArray(data && data.actionStats) ? data.actionStats : [];
				var statusRows = Array.isArray(data && data.statusStats) ? data.statusStats : [];
				function revisionStatRows(rows, key, label, emptyText){
					var html = '<table class="layui-table"><thead><tr><th>'+esc(label)+'</th><th>数量</th><th>最近时间</th></tr></thead><tbody>';
					rows.forEach(function(row){ html += '<tr><td>'+esc(row[key] || '')+'</td><td>'+esc(row.count || 0)+'</td><td>'+esc(row.lastTimeText || '')+'</td></tr>'; });
					if(!rows.length) html += '<tr><td colspan="3" style="text-align:center;color:#667085;">'+esc(emptyText)+'</td></tr>';
					return html + '</tbody></table>';
				}
				var html = '<div class="x-form"><div class="x-muted">版本统计只读取版本表聚合结果，不改变快照保留和恢复流程。</div>';
				html += '<table class="layui-table"><thead><tr><th>版本总数</th><th>内容数</th><th>最大版本号</th><th>最近快照</th></tr></thead><tbody><tr><td>'+esc(data.totalCount || 0)+'</td><td>'+esc(data.contentCount || 0)+'</td><td>'+esc(data.maxRevisionNo || 0)+'</td><td>'+esc(data.lastCreateTimeText || '')+'</td></tr></tbody></table>';
				html += '<div class="x-muted">动作分布</div>' + revisionStatRows(actionRows, 'action', '动作', '暂无动作统计');
				html += '<div class="x-muted">状态分布</div>' + revisionStatRows(statusRows, 'status', '状态', '暂无状态统计');
				html += '<pre class="x-code">' + esc(JSON.stringify(data, null, 2)) + '</pre></div>';
				byId('detail').innerHTML = html;
				message(ret);
			});
		}
		function showTagStats(){
			get(api('/tag/stats')).then(function(ret){
				var data = ret && ret.data ? ret.data : ret;
				byId('detail').innerHTML = renderTaxonomyStats(data, {
					title: '标签统计',
					idLabel: '标签ID',
					nameLabel: '标签',
					idKey: 'tagId',
					nameKey: 'name',
					topKey: 'topTagStats',
					emptyStatus: '暂无标签状态统计',
					emptyTop: '暂无标签关联统计'
				});
				message(ret);
			});
		}
		function showTopicStats(){
			get(api('/topic/stats')).then(function(ret){
				var data = ret && ret.data ? ret.data : ret;
				byId('detail').innerHTML = renderTaxonomyStats(data, {
					title: '专题统计',
					idLabel: '专题ID',
					nameLabel: '专题',
					idKey: 'topicId',
					nameKey: 'title',
					topKey: 'topTopicStats',
					emptyStatus: '暂无专题状态统计',
					emptyTop: '暂无专题关联统计'
				});
				message(ret);
			});
		}
		function renderTaxonomyStats(data, opts){
			data = data || {};
			opts = opts || {};
			var statusRows = Array.isArray(data.statusStats) ? data.statusStats : [];
			var topRows = Array.isArray(data[opts.topKey]) ? data[opts.topKey] : [];
			var html = '<div class="x-form"><div class="x-muted">' + esc(opts.title || '统计') + '</div>';
			html += '<table class="layui-table"><thead><tr><th>总数</th><th>启用</th><th>停用</th><th>内容计数</th><th>关联数</th><th>Top 上限</th><th>最近更新</th></tr></thead><tbody><tr>'
				+ '<td>' + esc(data.totalCount || 0) + '</td>'
				+ '<td>' + esc(data.enabledCount || 0) + '</td>'
				+ '<td>' + esc(data.disabledCount || 0) + '</td>'
				+ '<td>' + esc(data.totalContentCount || 0) + '</td>'
				+ '<td>' + esc(data.relationCount || 0) + '</td>'
				+ '<td>' + esc(data.topStatLimit || 0) + '</td>'
				+ '<td>' + esc(data.lastUpdateTimeText || '-') + '</td></tr></tbody></table>';
			html += '<div class="x-muted" style="margin-top:12px">状态分布</div><table class="layui-table"><thead><tr><th>状态</th><th>数量</th><th>最近更新</th></tr></thead><tbody>';
			if(statusRows.length){
				statusRows.forEach(function(row){
					html += '<tr><td>' + esc(row.status) + '</td><td>' + esc(row.count || 0) + '</td><td>' + esc(row.lastUpdateTimeText || '-') + '</td></tr>';
				});
			}else{
				html += '<tr><td colspan="3" class="x-muted">' + esc(opts.emptyStatus || '暂无状态统计') + '</td></tr>';
			}
			html += '</tbody></table><div class="x-muted" style="margin-top:12px">Top 关联项</div><table class="layui-table"><thead><tr><th>' + esc(opts.idLabel || 'ID') + '</th><th>' + esc(opts.nameLabel || '名称') + '</th><th>Slug</th><th>内容计数</th><th>关联数</th><th>状态</th><th>更新时间</th></tr></thead><tbody>';
			if(topRows.length){
				topRows.forEach(function(row){
					html += '<tr><td>' + esc(row[opts.idKey]) + '</td><td>' + esc(row[opts.nameKey] || '') + '</td><td>' + esc(row.slug || '') + '</td><td>' + esc(row.contentCount || 0) + '</td><td>' + esc(row.linkCount || 0) + '</td><td>' + esc(row.status) + '</td><td>' + esc(row.updateTimeText || '-') + '</td></tr>';
				});
			}else{
				html += '<tr><td colspan="7" class="x-muted">' + esc(opts.emptyTop || '暂无关联统计') + '</td></tr>';
			}
			html += '</tbody></table><pre class="x-code">' + esc(JSON.stringify(data, null, 2)) + '</pre></div>';
			return html;
		}
		function showSearchStats(){
			get(api('/search/stats')).then(function(ret){
				byId('detail').innerHTML = renderSearchStats(ret && ret.data ? ret.data : ret);
				message(ret);
			});
		}
		function renderSearchStats(data){
			data = data || {};
			var contentCount = Number(data.contentCount || 0);
			var indexCount = Number(data.indexCount || 0);
			var missingCount = Number(data.missingCount || 0);
			var statusText = missingCount > 0 ? '需要重建' : '索引完整';
			var html = '<div class="x-form"><div class="x-muted">搜索索引统计</div>';
			html += '<table class="layui-table"><thead><tr><th>内容总数</th><th>索引条数</th><th>缺失条数</th><th>索引状态</th><th>最近更新时间</th></tr></thead><tbody><tr>'
				+ '<td>' + esc(contentCount) + '</td>'
				+ '<td>' + esc(indexCount) + '</td>'
				+ '<td>' + esc(missingCount) + '</td>'
				+ '<td>' + esc(statusText) + '</td>'
				+ '<td>' + esc(data.lastUpdateText || '-') + '</td></tr></tbody></table>';
			html += '<div class="x-muted">缺失条数仅用于后台判断是否需要重建索引，不改变公开搜索查询路径。</div>';
			html += '<pre class="x-code">' + esc(JSON.stringify(data, null, 2)) + '</pre></div>';
			return html;
		}
		function rebuildSearchIndex(){
			var limitInput = byId('searchRebuildLimit');
			var offsetInput = byId('searchRebuildOffset');
			var limit = Number(limitInput && limitInput.value || 1000);
			var offset = Number(offsetInput && offsetInput.value || 0);
			if(!limit || limit < 1) limit = 1000;
			if(limit > 10000) limit = 10000;
			if(!offset || offset < 0) offset = 0;
			if(limitInput) limitInput.value = limit;
			if(offsetInput) offsetInput.value = offset;
			post(api('/search/rebuild?offset=' + encodeURIComponent(offset) + '&limit=' + encodeURIComponent(limit)), {}).then(function(ret){
				var data = ret && ret.data ? ret.data : ret;
				if(offsetInput && data && data.hasMore) offsetInput.value = data.nextOffset || (offset + (data.total || 0));
				byId('detail').innerHTML = renderSearchRebuildResult(data);
				message(ret);
				reload();
			});
		}
		function renderSearchRebuildResult(data){
			data = data || {};
			var html = '<div class="x-form"><div class="x-muted">搜索索引重建结果</div>';
			html += '<table class="layui-table"><thead><tr><th>扫描内容</th><th>写入索引</th><th>本批偏移</th><th>本批上限</th><th>最大上限</th><th>是否还有下一批</th><th>下一偏移</th><th>后台任务</th><th>任务状态</th></tr></thead><tbody><tr>'
				+ '<td>' + esc(data.total || 0) + '</td>'
				+ '<td>' + esc(data.indexed || 0) + '</td>'
				+ '<td>' + esc(data.offset || 0) + '</td>'
				+ '<td>' + esc(data.limit || 0) + '</td>'
				+ '<td>' + esc(data.maxRebuildRows || 0) + '</td>'
				+ '<td>' + esc(data.hasMore ? '是' : '否') + '</td>'
				+ '<td>' + esc(data.nextOffset || 0) + '</td>'
				+ '<td>' + esc(data.backgroundTaskId || '-') + '</td>'
				+ '<td>' + esc(data.queued ? '已记录' : '未记录') + '</td></tr></tbody></table>';
			html += '<div class="x-muted">重建按 offset/limit 分片执行，并写入 content.search.rebuild 后台任务记录；只影响后台索引维护流程，不改变公开搜索请求路径。</div>';
			html += '<pre class="x-code">' + esc(JSON.stringify(data, null, 2)) + '</pre></div>';
			return html;
		}
		function renderSearchProbeResult(data){
			data = data || {};
			var rows = Array.isArray(data.data) ? data.data : [];
			var html = '<div class="x-form"><div class="x-muted">搜索测试结果</div>';
			html += '<table class="layui-table"><thead><tr><th>查询词</th><th>归一化查询</th><th>总数</th><th>页码</th><th>返回数</th><th>栏目过滤</th><th>CJK 宽松 LIKE</th></tr></thead><tbody><tr>'
				+ '<td>' + esc(data.searchTerm || '') + '</td>'
				+ '<td>' + esc(data.normalizedQuery || '') + '</td>'
				+ '<td>' + esc(data.count || 0) + '</td>'
				+ '<td>' + esc(data.page || 1) + '</td>'
				+ '<td>' + esc(data.pageSize || 0) + '</td>'
				+ '<td>' + esc(data.categoryFilter ? '是' : '否') + '</td>'
				+ '<td>' + esc(data.cjkLooseLike ? '是' : '否') + '</td></tr></tbody></table>';
			html += '<table class="layui-table"><thead><tr><th>ID</th><th>标题</th><th>总分</th><th>基础分</th><th>短语分</th><th>覆盖分</th><th>CJK 分</th><th>新鲜度</th><th>摘要</th></tr></thead><tbody>';
			if(rows.length){
				rows.forEach(function(row){
					html += '<tr><td>' + esc(row.id || '') + '</td><td>' + esc(row.title || '') + '</td><td>' + esc(row.searchScore || 0) + '</td><td>' + esc(row.searchBaseScore || 0) + '</td><td>' + esc(row.exactPhraseScore || 0) + '</td><td>' + esc(row.termCoverageScore || 0) + '</td><td>' + esc(row.cjkBigramScore || 0) + '</td><td>' + esc(row.freshnessScore || 0) + '</td><td>' + esc(row.searchSnippet || '') + '</td></tr>';
				});
			}else{
				html += '<tr><td colspan="9" class="x-muted">没有匹配结果</td></tr>';
			}
			html += '</tbody></table><pre class="x-code">' + esc(JSON.stringify(data, null, 2)) + '</pre></div>';
			return html;
		}
		function runSearchProbe(){
			var qInput = byId('searchProbeQuery');
			var limitInput = byId('searchProbeLimit');
			var categoryInput = byId('searchCategoryId');
			var q = qInput && qInput.value ? qInput.value : '';
			var limit = Number(limitInput && limitInput.value || 10);
			var categoryId = categoryInput && categoryInput.value ? Number(categoryInput.value || 0) : 0;
			if(!q){ message({result:false,error:'search query is required'}); return; }
			if(!limit || limit < 1) limit = 10;
			if(limit > 20) limit = 20;
			if(limitInput) limitInput.value = limit;
			get(api('/search?q=' + encodeURIComponent(q) + '&limit=' + encodeURIComponent(limit) + (categoryId > 0 ? '&categoryId=' + encodeURIComponent(categoryId) : ''))).then(function(ret){
				var data = ret && ret.data ? ret.data : ret;
				var target = byId('searchProbeResult') || byId('detail');
				target.innerHTML = renderSearchProbeResult(data);
				message(ret);
			});
		}
		function renderSearchExplainResult(explain, raw){
			explain = explain || {};
			raw = raw || {};
			var weights = explain.weights || {};
			var guards = explain.guards || {};
			var rows = Array.isArray(explain.topResults) ? explain.topResults : [];
			var html = '<div class="x-form"><div class="x-muted">搜索排序解释</div>';
			html += '<table class="layui-table"><thead><tr><th>原始查询</th><th>归一化查询</th><th>最短查询</th><th>返回上限</th><th>查询过短</th><th>栏目过滤</th><th>CJK 宽松 LIKE</th></tr></thead><tbody><tr>'
				+ '<td>' + esc(explain.query || '') + '</td>'
				+ '<td>' + esc(explain.normalizedQuery || '') + '</td>'
				+ '<td>' + esc(guards.minQueryLength || 0) + '</td>'
				+ '<td>' + esc(guards.maxResultLimit || 0) + '</td>'
				+ '<td>' + esc(guards.queryTooShort ? '是' : '否') + '</td>'
				+ '<td>' + esc(raw.categoryFilter ? '是' : '否') + '</td>'
				+ '<td>' + esc(raw.cjkLooseLike ? '是' : '否') + '</td></tr></tbody></table>';
			html += '<table class="layui-table"><thead><tr><th>标题精确</th><th>标题前缀</th><th>标题 LIKE</th><th>关键词</th><th>摘要</th><th>正文</th><th>短语</th><th>词覆盖</th><th>CJK</th><th>新鲜度</th><th>新鲜窗口</th></tr></thead><tbody><tr>'
				+ '<td>' + esc(weights.exactTitleWeight || 0) + '</td>'
				+ '<td>' + esc(weights.prefixTitleWeight || 0) + '</td>'
				+ '<td>' + esc(weights.titleWeight || 0) + '</td>'
				+ '<td>' + esc(weights.keywordWeight || 0) + '</td>'
				+ '<td>' + esc(weights.summaryWeight || 0) + '</td>'
				+ '<td>' + esc(weights.bodyWeight || 0) + '</td>'
				+ '<td>' + esc(weights.exactPhraseWeight || 0) + '</td>'
				+ '<td>' + esc(weights.termCoverageWeight || 0) + '</td>'
				+ '<td>' + esc(weights.cjkBigramWeight || 0) + '</td>'
				+ '<td>' + esc(weights.freshnessWeight || 0) + '</td>'
				+ '<td>' + esc(weights.freshnessWindowDays || 0) + '</td></tr></tbody></table>';
			html += '<table class="layui-table"><thead><tr><th>ID</th><th>标题</th><th>总分</th><th>基础分</th><th>短语分</th><th>覆盖分</th><th>CJK 分</th><th>新鲜度</th><th>摘要</th></tr></thead><tbody>';
			if(rows.length){
				rows.forEach(function(row){
					html += '<tr><td>' + esc(row.id || '') + '</td><td>' + esc(row.title || '') + '</td><td>' + esc(row.searchScore || 0) + '</td><td>' + esc(row.searchBaseScore || 0) + '</td><td>' + esc(row.exactPhraseScore || 0) + '</td><td>' + esc(row.termCoverageScore || 0) + '</td><td>' + esc(row.cjkBigramScore || 0) + '</td><td>' + esc(row.freshnessScore || 0) + '</td><td>' + esc(row.searchSnippet || '') + '</td></tr>';
				});
			}else{
				html += '<tr><td colspan="9" class="x-muted">没有可解释的搜索结果</td></tr>';
			}
			html += '</tbody></table><pre class="x-code">' + esc(JSON.stringify(explain, null, 2)) + '</pre></div>';
			return html;
		}
		function runSearchExplain(){
			var qInput = byId('searchProbeQuery');
			var limitInput = byId('searchProbeLimit');
			var categoryInput = byId('searchCategoryId');
			var q = qInput && qInput.value ? qInput.value : '';
			var limit = Number(limitInput && limitInput.value || 10);
			var categoryId = categoryInput && categoryInput.value ? Number(categoryInput.value || 0) : 0;
			if(!q){ message({result:false,error:'search query is required'}); return; }
			if(!limit || limit < 1) limit = 10;
			if(limit > 20) limit = 20;
			get(api('/search?q=' + encodeURIComponent(q) + '&limit=' + encodeURIComponent(limit) + (categoryId > 0 ? '&categoryId=' + encodeURIComponent(categoryId) : ''))).then(function(ret){
				var data = ret && ret.data ? ret.data : ret;
				var rows = data && data.data ? data.data : [];
				var explain = {
					query: q,
					normalizedQuery: data ? data.normalizedQuery : '',
					guards: {
						minQueryLength: data ? data.minQueryLength : 0,
						maxResultLimit: data ? data.maxResultLimit : 0,
						queryTooShort: !!(data && data.queryTooShort)
					},
					weights: {
						exactTitleWeight: data ? data.exactTitleWeight : 0,
						prefixTitleWeight: data ? data.prefixTitleWeight : 0,
						titleWeight: data ? data.titleWeight : 0,
						keywordWeight: data ? data.keywordWeight : 0,
						summaryWeight: data ? data.summaryWeight : 0,
						bodyWeight: data ? data.bodyWeight : 0,
						termCoverageWeight: data ? data.termCoverageWeight : 0,
						cjkBigramWeight: data ? data.cjkBigramWeight : 0,
						freshnessWeight: data ? data.freshnessWeight : 0,
						freshnessWindowDays: data ? data.freshnessWindowDays : 0
					},
					topResults: rows.slice(0, 5).map(function(row){
						return {
							id: row.id,
							title: row.title,
							searchScore: row.searchScore,
							searchBaseScore: row.searchBaseScore,
							exactPhraseScore: row.exactPhraseScore,
							termCoverageScore: row.termCoverageScore,
							cjkBigramScore: row.cjkBigramScore,
							freshnessScore: row.freshnessScore,
							searchSnippet: row.searchSnippet
						};
					})
				};
				var target = byId('searchProbeResult') || byId('detail');
				target.innerHTML = renderSearchExplainResult(explain, data);
				message(ret);
			});
		}
		function renderSitemapStats(data){
			data = data || {};
			var rows = [
				['启用条目', data.enabledCount],
				['停用条目', data.disabledCount],
				['全部条目', data.totalCount],
				['缓存策略', data.cachePolicy || 'write-through'],
				['缓存文件', data.cacheFile || 'sitemap/cache.json'],
				['脏标记文件', data.dirtyFile || 'sitemap/dirty.json'],
				['缓存脏标记', data.cacheDirty ? '是' : '否'],
				['脏标记原因', data.dirtyReason || '-'],
				['脏标记内容ID', data.dirtyContentId || '-'],
				['脏标记时间', data.dirtyUpdateTimeText || data.dirtyUpdateTime || '-'],
				['缓存条目数', data.cacheEntryCount],
				['缓存 TTL 秒数', data.cacheTtlSeconds],
				['缓存过期时间', data.cacheExpiresAt || '-'],
				['缓存已过期', data.cacheExpired ? '是' : '否'],
				['最后更新', data.lastUpdateText || data.lastUpdate || '-']
			];
			var html = '<div class="x-form" id="sitemapStatsTable"><div class="x-muted">站点地图缓存信息</div><table class="layui-table"><thead><tr><th style="width:220px">指标</th><th>值</th></tr></thead><tbody>';
			rows.forEach(function(row){
				html += '<tr><td>' + esc(row[0]) + '</td><td>' + esc(row[1] == null ? '' : row[1]) + '</td></tr>';
			});
			html += '</tbody></table>';
			if(Array.isArray(data.cacheFiles) && data.cacheFiles.length){
				html += '<div class="x-muted" style="margin-top:12px">缓存文件状态</div><table class="layui-table"><thead><tr><th>文件</th><th style="width:90px">存在</th><th style="width:120px">字节数</th><th style="width:170px">更新时间</th></tr></thead><tbody>';
				data.cacheFiles.forEach(function(file){
					html += '<tr><td>' + esc(file.path || '') + '</td><td>' + esc(file.exists ? '是' : '否') + '</td><td>' + esc(file.sizeBytes == null ? 0 : file.sizeBytes) + '</td><td>' + esc(file.updateTimeText || file.updateTime || '-') + '</td></tr>';
				});
				html += '</tbody></table>';
			}
			return html + '</div>';
		}
		function showSitemapStats(){
			get(api('/sitemap/stats')).then(function(ret){
				byId('detail').innerHTML = renderSitemapStats(ret && ret.data ? ret.data : ret);
				message(ret);
			});
		}
		function showSitemapRefreshPlan(){
			get(api('/sitemap/refresh-plan' + sitemapRefreshQuery())).then(function(ret){
				byId('detail').innerHTML = renderSitemapRefreshPlan(ret && ret.data ? ret.data : ret);
				message(ret);
			});
		}
		function renderSitemapRefreshPlan(data){
			data = data || {};
			var html = '<div class="x-form"><div class="x-muted">站点地图刷新计划</div>';
			html += '<table class="layui-table"><thead><tr><th>候选内容</th><th>当前条目</th><th>本次刷新</th><th>请求上限</th><th>最大上限</th><th>是否截断</th></tr></thead><tbody><tr>'
				+ '<td>' + esc(data.eligibleCount || 0) + '</td>'
				+ '<td>' + esc(data.currentEntryCount || 0) + '</td>'
				+ '<td>' + esc(data.willRefreshRows || 0) + '</td>'
				+ '<td>' + esc(data.limit || 0) + '</td>'
				+ '<td>' + esc(data.maxRefreshRows || 0) + '</td>'
				+ '<td>' + esc(data.willTruncate ? '是' : '否') + '</td></tr></tbody></table>';
			html += '<table class="layui-table"><thead><tr><th>写入模式</th><th>会写数据</th><th>缓存策略</th><th>缓存 TTL</th><th>访问过滤</th></tr></thead><tbody><tr>'
				+ '<td>' + esc(data.writeMode || 'preview-only') + '</td>'
				+ '<td>' + esc(data.writesData ? '是' : '否') + '</td>'
				+ '<td>' + esc(data.cachePolicy || '-') + '</td>'
				+ '<td>' + esc(data.cacheTtlSeconds || 0) + '</td>'
				+ '<td>' + esc(data.accessFiltered ? '是' : '否') + '</td></tr></tbody></table>';
			html += '<div class="x-muted">刷新计划只读预检，不写 sitemap 条目和缓存文件。</div>';
			html += '<pre class="x-code">' + esc(JSON.stringify(data, null, 2)) + '</pre></div>';
			return html;
		}
		function sitemapRefreshQuery(){
			var input = byId('sitemapRefreshLimit');
			var limit = input ? Number(input.value || 0) : 0;
			return limit > 0 ? ('?limit=' + encodeURIComponent(limit)) : '';
		}
		function renderSitemapRefreshResult(ret){
			var data = ret && ret.data ? ret.data : ret || {};
			var html = '<div class="x-form"><div class="x-muted">站点地图刷新结果</div>';
			html += '<table class="layui-table"><thead><tr><th>候选/扫描</th><th>已保存</th><th>请求上限</th><th>最大上限</th><th>缓存写入</th><th>警告</th></tr></thead><tbody><tr>';
			html += '<td>'+esc(data.total || data.candidateCount || 0)+'</td><td>'+esc(data.saved || data.refreshed || 0)+'</td><td>'+esc(data.limit || 0)+'</td><td>'+esc(data.maxRefreshRows || 0)+'</td><td>'+esc(data.cacheWritten ? '是' : '否')+'</td><td>'+esc(data.warning || (ret && ret.warning) || '')+'</td></tr></tbody></table>';
			html += '<div class="x-muted">刷新会按上限重建 sitemap 条目并写穿缓存；若缓存写入失败，dirty metadata 会保留用于重试诊断。</div>';
			html += '<pre class="x-code">' + esc(JSON.stringify(ret || {}, null, 2)) + '</pre></div>';
			return html;
		}
		function refreshSitemapEntries(){
			post(api('/sitemap/refresh' + sitemapRefreshQuery()), {}).then(function(ret){
				byId('detail').innerHTML = renderSitemapRefreshResult(ret);
				message(ret);
				reload();
			});
		}
		function showMetricStats(path, title){
			get(api(path)).then(function(ret){
				var data = ret && ret.data ? ret.data : ret;
				var html = '<div class="x-form"><div class="x-muted">' + esc(title) + '</div><table class="layui-table"><thead><tr><th style="width:220px">指标</th><th>值</th></tr></thead><tbody>';
				Object.keys(data || {}).forEach(function(key){
					html += '<tr><td>' + esc(key) + '</td><td>' + esc(data[key]) + '</td></tr>';
				});
				byId('detail').innerHTML = html + '</tbody></table></div>';
				message(ret);
			});
		}
		function renderCategoryBindSampleRows(rows, emptyText){
			rows = Array.isArray(rows) ? rows : [];
			var html = '';
			rows.forEach(function(row){
				row = row || {};
				html += '<tr><td>' + esc(row.contentId || '') + '</td><td>' + esc(row.title || '') + '</td><td>' + esc(row.legacyCategoryId || '') + '</td><td>' + esc(row.bindCategoryId || '') + '</td></tr>';
			});
			if(!rows.length) html += '<tr><td colspan="4" class="x-muted">' + esc(emptyText || '暂无样本') + '</td></tr>';
			return html;
		}
		function renderCategoryBindStatus(data){
			data = data || {};
			var missingBindRows = Number(data.missingBindRows || 0);
			var mismatchRows = Number(data.mismatchRows || 0);
			var hasDrift = missingBindRows > 0 || mismatchRows > 0;
			var html = '<div class="x-form"><div class="x-muted">栏目绑定迁移状态</div>';
			html += '<table class="layui-table"><thead><tr><th>旧字段内容数</th><th>绑定表有效数</th><th>缺失绑定数</th><th>不一致数</th><th>建议动作</th></tr></thead><tbody>';
			html += '<tr><td>' + esc(data.legacyContentRows || 0) + '</td><td>' + esc(data.activeBindRows || 0) + '</td><td>' + esc(missingBindRows) + '</td><td>' + esc(mismatchRows) + '</td><td>' + esc(hasDrift ? '执行回填绑定表后再继续移除 legacy mirror' : '绑定表已与 legacy mirror 对齐') + '</td></tr></tbody></table>';
			html += '<table class="layui-table"><thead><tr><th>源表</th><th>目标表</th><th>迁移开关</th><th>兼容镜像</th><th>管理期入口</th></tr></thead><tbody>';
			html += '<tr><td>' + esc(data.sourceTable || 'content_item.category_id') + '</td><td>' + esc(data.targetTable || 'content_category_bind') + '</td><td>' + esc(data.categoryBindMigration ? '是' : '否') + '</td><td>' + esc(data.legacyMirror ? '是' : '否') + '</td><td>' + esc(data.adminTimeOnly ? '是' : '否') + '</td></tr></tbody></table>';
			html += '<div class="x-muted">缺失绑定样本（最多 ' + esc(data.sampleLimit || 10) + ' 行）</div>';
			html += '<table class="layui-table"><thead><tr><th>内容ID</th><th>标题</th><th>旧栏目ID</th><th>绑定栏目ID</th></tr></thead><tbody>' + renderCategoryBindSampleRows(data.missingSamples, '暂无缺失绑定样本') + '</tbody></table>';
			html += '<div class="x-muted">栏目不一致样本（最多 ' + esc(data.sampleLimit || 10) + ' 行）</div>';
			html += '<table class="layui-table"><thead><tr><th>内容ID</th><th>标题</th><th>旧栏目ID</th><th>绑定栏目ID</th></tr></thead><tbody>' + renderCategoryBindSampleRows(data.mismatchSamples, '暂无不一致样本') + '</tbody></table>';
			html += '<pre class="x-code">' + esc(JSON.stringify(data, null, 2)) + '</pre></div>';
			return html;
		}
		function renderCategoryBindBackfillResult(data, ret){
			data = data || {};
			var ok = ret && ret.result;
			var html = '<div class="x-form"><div class="x-muted">栏目绑定回填结果</div>';
			html += '<table class="layui-table"><thead><tr><th>执行结果</th><th>缺失绑定数</th><th>不一致数</th><th>接口消息</th></tr></thead><tbody>';
			html += '<tr><td>' + esc(ok ? '成功' : '失败') + '</td><td>' + esc(data.missingBindRows || 0) + '</td><td>' + esc(data.mismatchRows || 0) + '</td><td>' + esc((ret && (ret.message || ret.error)) || '') + '</td></tr></tbody></table>';
			html += '<pre class="x-code">' + esc(JSON.stringify(data, null, 2)) + '</pre></div>';
			return html;
		}
		function showCategoryBindStatus(){
			get(api('/category/bind/status')).then(function(ret){
				var data = ret && ret.data ? ret.data : ret;
				byId('detail').innerHTML = renderCategoryBindStatus(data);
				message(ret);
			});
		}
		function runCategoryBindBackfill(){
			post(api('/category/bind/backfill'), {}).then(function(ret){
				var data = ret && ret.data ? ret.data : ret;
				var target = byId('categoryBindStatusResult') || byId('detail');
				target.innerHTML = renderCategoryBindBackfillResult(data, ret);
				message(ret);
				showCategoryBindStatus();
				reload();
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
				target.innerHTML = renderRelatedRebuildResult(data);
				message(ret);
				reload();
			});
		}
		function renderRelatedRebuildResult(data){
			data = data || {};
			var html = '<div class="x-form"><div class="x-muted">相关推荐规则重建结果</div>';
			html += '<table class="layui-table"><thead><tr><th>来源内容数</th><th>创建规则数</th><th>全量来源上限</th><th>单源候选上限</th></tr></thead><tbody><tr>'
				+ '<td>' + esc(data.sources || 0) + '</td>'
				+ '<td>' + esc(data.created || 0) + '</td>'
				+ '<td>' + esc(data.maxRebuildSources || 0) + '</td>'
				+ '<td>' + esc((byId('relatedRebuildLimit') && byId('relatedRebuildLimit').value) || '') + '</td></tr></tbody></table>';
			html += '<div class="x-muted">重建只写 relationType=rule 的推荐关系，手工关联不会被覆盖。</div>';
			html += '<pre class="x-code">' + esc(JSON.stringify(data, null, 2)) + '</pre></div>';
			return html;
		}
		function renderRelatedRulePreview(data){
			data = data || {};
			var rows = Array.isArray(data.candidates) ? data.candidates : [];
			var html = '<div class="x-form"><div class="x-muted">规则分值预览，不写入推荐关系。</div>';
			html += '<table class="layui-table"><thead><tr><th>排序</th><th>候选内容</th><th>栏目分</th><th>共享标签</th><th>标签分</th><th>共享专题</th><th>专题分</th><th>总分</th><th>原因</th></tr></thead><tbody>';
			if(!rows.length){
				html += '<tr><td colspan="9" class="x-muted">没有匹配的候选内容</td></tr>';
			}
			rows.forEach(function(row){
				html += '<tr><td>'+esc(row.rank || '')+'</td><td>'+esc((row.relatedTitle || '') + ' #' + (row.relatedContentId || ''))+'</td><td>'+esc(row.categoryScore || 0)+'</td><td>'+esc(row.sharedTagCount || 0)+'</td><td>'+esc(row.tagScore || 0)+'</td><td>'+esc(row.sharedTopicCount || 0)+'</td><td>'+esc(row.topicScore || 0)+'</td><td>'+esc(row.weight || 0)+'</td><td>'+esc(row.reasonText || '')+'</td></tr>';
			});
			html += '</tbody></table><div class="x-muted">公式：'+esc(data.ruleFormula || '')+'</div><pre class="x-code">'+esc(JSON.stringify(data, null, 2))+'</pre></div>';
			return html;
		}
		function previewRelatedRules(){
			var idInput = byId('relatedRebuildContentId');
			var limitInput = byId('relatedRebuildLimit');
			var limit = Number(limitInput && limitInput.value || 5);
			var contentId = Number(idInput && idInput.value || 0);
			if(!contentId){ message({result:false,error:'内容ID必填'}); return; }
			if(!limit || limit < 1) limit = 5;
			if(limit > 20) limit = 20;
			if(limitInput) limitInput.value = limit;
			post(api('/related/rule/preview'), {contentId:contentId,limit:limit}).then(function(ret){
				var data = ret && ret.data ? ret.data : ret;
				var target = byId('relatedRebuildResult') || byId('detail');
				target.innerHTML = renderRelatedRulePreview(data);
				message(ret);
			});
		}
		function showRelatedStats(){
			get(api('/related/stats')).then(function(ret){
				var data = ret && ret.data ? ret.data : ret;
				var target = byId('relatedRebuildResult') || byId('detail');
				var rule = data && data.ruleConfig ? data.ruleConfig : {};
				var rows = Array.isArray(data && data.typeStats) ? data.typeStats : [];
				var html = '<div class="x-form"><div class="x-muted">关联统计与规则诊断只读取 content_related 和当前实例配置，不写入推荐关系。</div>';
				html += '<table class="layui-table"><thead><tr><th>总数</th><th>启用</th><th>停用</th><th>手工</th><th>规则</th><th>最近更新</th></tr></thead><tbody><tr><td>'+esc(data.totalCount || 0)+'</td><td>'+esc(data.enabledCount || 0)+'</td><td>'+esc(data.disabledCount || 0)+'</td><td>'+esc(data.manualCount || 0)+'</td><td>'+esc(data.ruleCount || 0)+'</td><td>'+esc(data.lastUpdateTimeText || '')+'</td></tr></tbody></table>';
				html += '<table class="layui-table"><thead><tr><th>规则上限</th><th>发布刷新上限</th><th>全量重建来源上限</th><th>栏目权重</th><th>标签权重</th><th>专题权重</th></tr></thead><tbody><tr><td>'+esc(rule.ruleLimit || 0)+'</td><td>'+esc(rule.publishRefreshLimit || 0)+'</td><td>'+esc(rule.maxRebuildSources || 0)+'</td><td>'+esc(rule.categoryWeight || 0)+'</td><td>'+esc(rule.tagWeight || 0)+'</td><td>'+esc(rule.topicWeight || 0)+'</td></tr></tbody></table>';
				html += '<div class="x-muted">公式：'+esc(rule.ruleFormula || '')+'</div>';
				html += '<table class="layui-table"><thead><tr><th>类型</th><th>总数</th><th>启用</th><th>停用</th><th>平均权重</th><th>最近更新</th></tr></thead><tbody>';
				rows.forEach(function(row){ html += '<tr><td>'+esc(row.relationType || '')+'</td><td>'+esc(row.count || 0)+'</td><td>'+esc(row.enabledCount || 0)+'</td><td>'+esc(row.disabledCount || 0)+'</td><td>'+esc(row.avgWeight || 0)+'</td><td>'+esc(row.lastUpdateTimeText || '')+'</td></tr>'; });
				if(!rows.length) html += '<tr><td colspan="6" style="text-align:center;color:#667085;">暂无关联数据</td></tr>';
				html += '</tbody></table><pre class="x-code">'+esc(JSON.stringify(data, null, 2))+'</pre></div>';
				target.innerHTML = html;
				message(ret);
			});
		}
		function markFormNotificationStatus(row, status){
			post(api('/form/notification/status'), {id:row && row.id, status:status}).then(function(ret){
				showRowActionResult(ret, '表单通知状态结果', row, status ? '已读' : '未读');
				message(ret);
				reload();
			});
		}
		function replayFormNotification(row){
			post(api('/form/notification/replay'), {id:row && row.id}).then(function(ret){
				showRowActionResult(ret, '表单通知重放结果', row, '重放');
				message(ret);
				reload();
			});
		}
		function showFormNotificationStats(){
			get(api('/form/notification/stats')).then(function(ret){
				var data = ret && ret.data ? ret.data : ret;
				var rows = Array.isArray(data && data.eventStats) ? data.eventStats : [];
				var html = '<div class="x-form"><div class="x-muted">表单通知统计只读取本地通知收件箱，不会触发外部投递。</div>';
				html += '<table class="layui-table"><thead><tr><th>总数</th><th>未读</th><th>已读</th><th>投递策略</th><th>本地通知</th></tr></thead><tbody><tr><td>'+esc(data.totalCount || 0)+'</td><td>'+esc(data.unreadCount || 0)+'</td><td>'+esc(data.readCount || 0)+'</td><td>'+esc(data.deliveryPolicy || '')+'</td><td>'+esc(data.localNotificationOnly ? '是' : '否')+'</td></tr></tbody></table>';
				html += '<table class="layui-table"><thead><tr><th>事件</th><th>总数</th><th>未读</th><th>最近时间</th></tr></thead><tbody>';
				rows.forEach(function(row){ html += '<tr><td>'+esc(row.event || '')+'</td><td>'+esc(row.count || 0)+'</td><td>'+esc(row.unreadCount || 0)+'</td><td>'+esc(row.lastCreateTimeText || '')+'</td></tr>'; });
				if(!rows.length) html += '<tr><td colspan="4" style="text-align:center;color:#667085;">暂无通知事件统计</td></tr>';
				html += '</tbody></table><pre class="x-code">' + esc(JSON.stringify(data, null, 2)) + '</pre></div>';
				byId('detail').innerHTML = html;
				message(ret);
			});
		}
		function renderFormSubmissionExportRows(rows){
			rows = Array.isArray(rows) ? rows.slice(0, 10) : [];
			var html = '';
			rows.forEach(function(row){
				html += '<tr><td>'+esc(row.id || '')+'</td><td>'+esc(row.formId || '')+'</td><td>'+esc(row.formKey || '')+'</td><td>'+esc(row.contentId || '')+'</td><td>'+esc(row.status || '')+'</td><td>'+esc(row.createTimeText || '')+'</td></tr>';
			});
			if(!rows.length) html += '<tr><td colspan="6" style="text-align:center;color:#667085;">暂无提交记录</td></tr>';
			return html;
		}
		function renderFormSubmissionExportResult(ret){
			var rows = ret && Array.isArray(ret.data) ? ret.data : [];
			var html = '<div class="x-form"><div class="x-muted">表单提交导出按 maxExportRows 返回 JSON；表格只预览前 10 行，完整 JSON 保留在诊断区。</div>';
			html += '<table class="layui-table"><thead><tr><th>导出类型</th><th>导出数量</th><th>导出上限</th><th>JSON 字节</th></tr></thead><tbody>';
			html += '<tr><td>'+esc(ret && ret.exportType || 'json')+'</td><td>'+esc(ret && ret.count || rows.length)+'</td><td>'+esc(ret && ret.limit || '')+'</td><td>'+esc(ret && ret.json ? String(ret.json).length : 0)+'</td></tr></tbody></table>';
			html += '<table class="layui-table"><thead><tr><th>ID</th><th>表单ID</th><th>表单Key</th><th>内容ID</th><th>状态</th><th>提交时间</th></tr></thead><tbody>' + renderFormSubmissionExportRows(rows) + '</tbody></table>';
			html += '<pre class="x-code">' + esc(JSON.stringify(ret || {}, null, 2)) + '</pre></div>';
			return html;
		}
		function renderFormSubmissionStatusRows(rows){
			rows = Array.isArray(rows) ? rows : [];
			var html = '';
			rows.forEach(function(row){
				html += '<tr><td>'+esc(row.status || 0)+'</td><td>'+esc(row.count || 0)+'</td><td>'+esc(row.lastCreateTimeText || '')+'</td></tr>';
			});
			if(!rows.length) html += '<tr><td colspan="3" style="text-align:center;color:#667085;">暂无状态统计</td></tr>';
			return html;
		}
		function renderFormSubmissionFormRows(rows){
			rows = Array.isArray(rows) ? rows : [];
			var html = '';
			rows.forEach(function(row){
				html += '<tr><td>'+esc(row.formId || '')+'</td><td>'+esc(row.formKey || '')+'</td><td>'+esc(row.formTitle || '')+'</td><td>'+esc(row.count || 0)+'</td><td>'+esc(row.processedCount || 0)+'</td><td>'+esc(row.lastCreateTimeText || '')+'</td></tr>';
			});
			if(!rows.length) html += '<tr><td colspan="6" style="text-align:center;color:#667085;">暂无表单统计</td></tr>';
			return html;
		}
		function renderFormSubmissionStats(data){
			data = data || {};
			var html = '<div class="x-form"><div class="x-muted">提交统计只读取后台提交表聚合数据，不改变公开提交或处理流程。</div>';
			html += '<table class="layui-table"><thead><tr><th>提交总数</th><th>表单数</th><th>已处理</th><th>未处理</th><th>表单统计上限</th><th>最近提交</th></tr></thead><tbody>';
			html += '<tr><td>'+esc(data.totalCount || 0)+'</td><td>'+esc(data.formCount || 0)+'</td><td>'+esc(data.processedCount || 0)+'</td><td>'+esc(data.unprocessedCount || 0)+'</td><td>'+esc(data.formStatLimit || '')+'</td><td>'+esc(data.lastCreateTimeText || '')+'</td></tr></tbody></table>';
			html += '<div class="x-muted">状态分布</div><table class="layui-table"><thead><tr><th>状态</th><th>数量</th><th>最近提交</th></tr></thead><tbody>' + renderFormSubmissionStatusRows(data.statusStats) + '</tbody></table>';
			html += '<div class="x-muted">表单分布</div><table class="layui-table"><thead><tr><th>表单ID</th><th>Key</th><th>标题</th><th>提交数</th><th>已处理</th><th>最近提交</th></tr></thead><tbody>' + renderFormSubmissionFormRows(data.formStats) + '</tbody></table>';
			html += '<pre class="x-code">' + esc(JSON.stringify(data, null, 2)) + '</pre></div>';
			return html;
		}
		function exportFormSubmissions(){
			get(api('/form/submission/export')).then(function(ret){
				byId('detail').innerHTML = renderFormSubmissionExportResult(ret);
				message(ret);
			});
		}
		function showFormSubmissionStats(){
			get(api('/form/submission/stats')).then(function(ret){
				var data = ret && ret.data ? ret.data : ret;
				byId('detail').innerHTML = renderFormSubmissionStats(data);
				message(ret);
			});
		}
		function markFormSubmissionProcessed(row){
			post(api('/form/submission/status'), {id:row.id,status:2}).then(function(ret){
				showRowActionResult(ret, '表单提交处理结果', row, '标记已处理');
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
				byId('detail').innerHTML = renderWorkflowActionResult(ret && ret.data ? ret.data : ret, action);
				message(ret);
				reload();
			});
		}
		function renderWorkflowActionResult(data, action){
			data = data || {};
			var html = '<div class="x-form"><div class="x-muted">工作流动作结果</div>';
			html += '<table class="layui-table"><thead><tr><th>动作</th><th>内容 ID</th><th>原状态</th><th>目标状态</th><th>已更新</th><th>日志</th><th>通知</th><th>审批数</th><th>待继续审批</th></tr></thead><tbody><tr>'
				+ '<td>' + esc(data.action || action || '') + '</td>'
				+ '<td>' + esc(data.id || data.contentId || '') + '</td>'
				+ '<td>' + esc(data.fromStatus || '') + '</td>'
				+ '<td>' + esc(data.toStatus || '') + '</td>'
				+ '<td>' + esc(data.updated || data.changed ? '是' : '否') + '</td>'
				+ '<td>' + esc(data.logSaved === false ? '失败' : '已记录') + '</td>'
				+ '<td>' + esc(data.notificationSaved === false ? '失败' : '已记录') + '</td>'
				+ '<td>' + esc(data.approvalCount || 0) + '/' + esc(data.requiredApprovals || 0) + '</td>'
				+ '<td>' + esc(data.approvalPending ? '是' : '否') + '</td></tr></tbody></table>';
			html += '<div class="x-muted">动作结果用于后台审核排障，工作流仍按能力包启用状态和权限点隔离。</div>';
			html += '<pre class="x-code">' + esc(JSON.stringify(data, null, 2)) + '</pre></div>';
			return html;
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
				target.innerHTML = renderWorkflowScheduledResult(data);
				message(ret);
				reload();
			});
		}
		function renderWorkflowScheduledResult(data){
			data = data || {};
			var html = '<div class="x-form"><div class="x-muted">到期定时发布执行结果</div>';
			html += '<table class="layui-table"><thead><tr><th>本次发布</th><th>执行上限</th><th>处理时间</th><th>任务类型</th></tr></thead><tbody><tr>'
				+ '<td>' + esc(data.published || 0) + '</td>'
				+ '<td>' + esc(data.limit || 0) + '</td>'
				+ '<td>' + esc(data.processTimeText || '-') + '</td>'
				+ '<td>scheduled-publish</td></tr></tbody></table>';
			html += '<div class="x-muted">该操作只在管理期按上限扫描到期草稿，执行后同步搜索、站点地图、相关推荐等派生数据。</div>';
			html += '<pre class="x-code">' + esc(JSON.stringify(data, null, 2)) + '</pre></div>';
			return html;
		}
		function showWorkflowStats(){
			get(api('/workflow/stats')).then(function(ret){
				var data = ret && ret.data ? ret.data : ret;
				var target = byId('workflowScheduledResult') || byId('detail');
				if(target && target.id === 'workflowScheduledResult'){
					target.outerHTML = '<div id="workflowScheduledResult" style="margin-top:8px">' + renderWorkflowStats(data) + '</div>';
				}else if(target){
					target.innerHTML = renderWorkflowStats(data);
				}
				message(ret);
			});
		}
		function renderWorkflowActionStats(rows, emptyText){
			var html = '';
			rows = Array.isArray(rows) ? rows : [];
			if(rows.length){
				rows.forEach(function(row){
					html += '<tr><td>' + esc(row.action || '') + '</td><td>' + esc(row.count || 0) + '</td><td>' + esc(row.lastTimeText || '-') + '</td></tr>';
				});
			}else{
				html += '<tr><td colspan="3" class="x-muted">' + esc(emptyText || '暂无数据') + '</td></tr>';
			}
			return html;
		}
		function renderWorkflowPlanRows(rows, type){
			var html = '';
			rows = Array.isArray(rows) ? rows : [];
			if(rows.length){
				rows.forEach(function(row){
					if(type === 'transition'){
						html += '<tr><td>' + esc(row.action || '') + '</td><td>' + esc(row.fromNode || '') + '</td><td>' + esc(row.toNode || '') + '</td><td>' + esc(row.permission || '') + '</td><td>' + esc(row.automatic ? '是' : '否') + '</td></tr>';
					}else{
						html += '<tr><td>' + esc(row.key || '') + '</td><td>' + esc(row.title || '') + '</td><td>' + esc(row.ownerPolicy || '') + '</td><td>' + esc(row.status || 0) + '</td><td>' + esc(row.isDraft ? '是' : '否') + '</td></tr>';
					}
				});
			}else{
				html += '<tr><td colspan="5" class="x-muted">暂无流程模型</td></tr>';
			}
			return html;
		}
		function renderWorkflowStats(data){
			data = data || {};
			var approval = data.approvalConfig || {};
			var due = data.scheduledDue || {};
			var plan = data.workflowPlan || {};
			var html = '<div class="x-form"><div class="x-muted">工作流统计</div>';
			html += '<table class="layui-table"><thead><tr><th>待办数</th><th>日志数</th><th>通知数</th><th>未读通知</th><th>最近日志</th><th>最近通知</th></tr></thead><tbody><tr>'
				+ '<td>' + esc(data.todoCount || 0) + '</td>'
				+ '<td>' + esc(data.logCount || 0) + '</td>'
				+ '<td>' + esc(data.notificationCount || 0) + '</td>'
				+ '<td>' + esc(data.unreadNotificationCount || 0) + '</td>'
				+ '<td>' + esc(data.lastLogTimeText || '-') + '</td>'
				+ '<td>' + esc(data.lastNotificationTimeText || '-') + '</td></tr></tbody></table>';
			html += '<table class="layui-table"><thead><tr><th>审批次数</th><th>要求不同审核人</th><th>多审启用</th><th>到期数</th><th>扫描草稿</th><th>扫描上限</th><th>是否截断</th></tr></thead><tbody><tr>'
				+ '<td>' + esc(approval.requiredApprovals || 1) + '</td>'
				+ '<td>' + esc(approval.requireDistinctApprovers ? '是' : '否') + '</td>'
				+ '<td>' + esc(approval.multiApprovalEnabled ? '是' : '否') + '</td>'
				+ '<td>' + esc(due.dueCount || 0) + '</td>'
				+ '<td>' + esc(due.scannedDraftCount || 0) + '</td>'
				+ '<td>' + esc(due.scanLimit || 0) + '</td>'
				+ '<td>' + esc(due.truncated ? '是' : '否') + '</td></tr></tbody></table>';
			html += '<div class="x-muted" style="margin-top:12px">流程节点模型：' + esc(plan.modelVersion || '') + '</div><table class="layui-table"><thead><tr><th>节点</th><th>名称</th><th>处理人策略</th><th>状态</th><th>草稿</th></tr></thead><tbody>' + renderWorkflowPlanRows(plan.nodes, 'node') + '</tbody></table>';
			html += '<div class="x-muted" style="margin-top:12px">流转模型：兼容 requiredApprovals=' + esc(plan.requiredApprovals || approval.requiredApprovals || 1) + '</div><table class="layui-table"><thead><tr><th>动作</th><th>来源节点</th><th>目标节点</th><th>权限</th><th>自动</th></tr></thead><tbody>' + renderWorkflowPlanRows(plan.transitions, 'transition') + '</tbody></table>';
			html += '<div class="x-muted" style="margin-top:12px">流转动作分布</div><table class="layui-table"><thead><tr><th>动作</th><th>数量</th><th>最近时间</th></tr></thead><tbody>' + renderWorkflowActionStats(data.actionStats, '暂无流转日志') + '</tbody></table>';
			html += '<div class="x-muted" style="margin-top:12px">通知动作分布</div><table class="layui-table"><thead><tr><th>动作</th><th>数量</th><th>最近时间</th></tr></thead><tbody>' + renderWorkflowActionStats(data.notificationActionStats, '暂无通知记录') + '</tbody></table>';
			html += '<pre class="x-code">' + esc(JSON.stringify(data, null, 2)) + '</pre></div>';
			return html;
		}
		function loadRows(){
			var cfg = currentConfig();
			var view = currentView();
			var url = view.listApi ? api(view.listApi) : api('/pack/list?pack=' + encodeURIComponent(packId));
			if(typeof view.listQuery === 'function') url = appendQuery(url, view.listQuery());
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
		window.addEventListener('message', function(ev){
			var data = ev && ev.data ? ev.data : null;
			if(ev.origin !== window.location.origin || packId !== 'content.media' || !data || data.type !== 'xadminAttachmentUploaded') return;
			openMediaDialog(mediaRowFromAttachmentData(data.data || {}));
		});
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
				+ '<div class="layui-form-item"><label class="layui-form-label">保存反馈</label><div class="layui-input-block"><pre class="x-code" id="redirectSaveResult_'+instanceKey+'">保存后会同步统一 URL 规则并刷新动态路由。</pre></div></div>'
				+ '<div class="x-dialog-actions"><button type="button" class="layui-btn layui-btn-primary" id="btnRedirectCancel_'+instanceKey+'">取消</button><button type="button" class="layui-btn" id="btnRedirectSave_'+instanceKey+'">保存</button></div>'
				+ '</form></div>';
			var index = layer.open({type:1,title:row.id ? '编辑跳转规则' : '新增跳转规则',area:['620px','520px'],content:html,success:function(dom){
				layui.form.render();
				dom.find('#btnRedirectCancel_'+instanceKey).on('click', function(){ layer.close(index); });
				dom.find('#btnRedirectSave_'+instanceKey).on('click', function(){
					var form = dom.find('#redirectForm_'+instanceKey)[0];
					var data = formDataFromElement(form);
					data.status = form.elements.statusEnabled && form.elements.statusEnabled.checked ? 1 : 0;
					delete data.statusEnabled;
					post(api('/redirect/save'), data).then(function(ret){
						var result = dom.find('#redirectSaveResult_'+instanceKey);
						if(result.length) result.text(JSON.stringify({
							warning: ret && ret.warning || '',
							syncedRouteRules: ret && ret.syncedRouteRules || 0,
							runtimeRefreshed: !!(ret && ret.runtimeRefreshed),
							runtimeRouteCount: ret && ret.runtimeRouteCount || 0,
							routeRefreshError: ret && ret.routeRefreshError || ''
						}, null, 2));
						message(ret);
						if(ret && ret.result !== false){ layer.close(index); reload(); }
					});
				});
			}});
		}
		function routeRuleLocalWarning(data){
			var pattern = String(data && data.matchPattern || '');
			var target = String(data && data.targetPath || '');
			if(pattern === '^/.*$' || pattern === '^/(.*)$' || pattern === '^/([^/]+)$') return '规则匹配范围较宽，可能吞掉后续动态规则；这是保存期提示，不进入请求热路径。';
			if(pattern.indexOf('^/admin') === 0 || pattern.indexOf('^/api') === 0 || pattern.indexOf('^/uploads') === 0 || pattern.indexOf('^/static') === 0 || target.indexOf('/admin') === 0 || target.indexOf('/api') === 0) return '规则涉及后台、API 或静态资源前缀，请确认不会覆盖已有静态路由。';
			return '未发现明显保存期风险，最终以服务端校验结果为准。';
		}
		function openRouteRuleDialog(row){
			row = row || {};
			var html = '<div class="managed-ability-dialog"><form class="layui-form" id="routeRuleForm_'+instanceKey+'">'
				+ '<input type="hidden" name="id" value="'+esc(row.id || 0)+'">'
				+ '<div class="layui-form-item"><label class="layui-form-label">规则类型</label><div class="layui-input-block"><select name="ruleType"><option value="slug" '+((row.ruleType || '') === 'slug' ? 'selected' : '')+'>slug</option><option value="redirect" '+((row.ruleType || '') === 'redirect' ? 'selected' : '')+'>redirect</option><option value="static" '+((row.ruleType || '') === 'static' ? 'selected' : '')+'>static</option></select></div></div>'
				+ '<div class="layui-form-item"><label class="layui-form-label">来源能力包</label><div class="layui-input-block"><select name="sourcePack"><option value="content.slug" '+((row.sourcePack || row.packId || '') === 'content.slug' ? 'selected' : '')+'>content.slug</option><option value="content.redirect" '+((row.sourcePack || row.packId || '') === 'content.redirect' ? 'selected' : '')+'>content.redirect</option><option value="content.static" '+((row.sourcePack || row.packId || '') === 'content.static' ? 'selected' : '')+'>content.static</option></select></div></div>'
				+ '<div class="layui-form-item"><label class="layui-form-label">规则键</label><div class="layui-input-block"><input name="ruleKey" class="layui-input" value="'+esc(row.ruleKey || '')+'" placeholder="唯一规则键，例如 slug:/article/{id}"></div></div>'
				+ '<div class="layui-form-item"><label class="layui-form-label">匹配规则</label><div class="layui-input-block"><input name="matchPattern" class="layui-input" value="'+esc(row.matchPattern || row.pattern || '')+'" placeholder="^/article/([^/]+)$"></div></div>'
				+ '<div class="layui-form-item"><label class="layui-form-label">目标路径</label><div class="layui-input-block"><input name="targetPath" class="layui-input" value="'+esc(row.targetPath || row.source || '')+'" placeholder="/plugin/{{PLUGIN_XID}}/detail"></div></div>'
				+ '<div class="layui-form-item"><label class="layui-form-label">优先级</label><div class="layui-input-block"><input name="priority" type="number" class="layui-input" value="'+esc(row.priority || 0)+'" placeholder="0"></div></div>'
				+ '<div class="layui-form-item"><label class="layui-form-label">状态</label><div class="layui-input-block"><select name="status"><option value="1" '+(row.status === 0 ? '' : 'selected')+'>启用</option><option value="0" '+(row.status === 0 ? 'selected' : '')+'>停用</option></select></div></div>'
				+ '<div class="layui-form-item"><label class="layui-form-label">保存提示</label><div class="layui-input-block"><pre class="x-code" id="routeRuleDialogWarning_'+instanceKey+'">'+esc(row.warning || '填写后保存前会显示保存期风险提示。')+'</pre></div></div>'
				+ '<div class="x-dialog-actions"><button type="button" class="layui-btn layui-btn-primary" id="btnRouteRuleCancel_'+instanceKey+'">取消</button><button type="button" class="layui-btn" id="btnRouteRuleSave_'+instanceKey+'">保存</button></div>'
				+ '</form></div>';
			var index = layer.open({type:1,title:row.id ? '编辑 URL 规则' : '新增 URL 规则',area:['720px','680px'],content:html,success:function(dom){
				layui.form.render();
				function readRouteRuleForm(){
					var form = dom.find('#routeRuleForm_'+instanceKey)[0];
					var data = formDataFromElement(form);
					data.id = Number(data.id || 0);
					data.priority = Number(data.priority || 0);
					data.status = Number(data.status || 0) ? 1 : 0;
					data.managedFlag = 1;
					return data;
				}
				function updateWarning(){
					var warn = dom.find('#routeRuleDialogWarning_'+instanceKey);
					if(warn.length) warn.text(routeRuleLocalWarning(readRouteRuleForm()));
				}
				dom.find('input,select').on('change keyup', updateWarning);
				updateWarning();
				dom.find('#btnRouteRuleCancel_'+instanceKey).on('click', function(){ layer.close(index); });
				dom.find('#btnRouteRuleSave_'+instanceKey).on('click', function(){
					var data = readRouteRuleForm();
					updateWarning();
					post(api('/route-rule/save'), data).then(function(ret){
						var warn = dom.find('#routeRuleDialogWarning_'+instanceKey);
						if(warn.length) warn.text((ret && ret.warning) ? ret.warning : (ret && ret.message ? ret.message : routeRuleLocalWarning(data)));
						message(ret);
						if(ret && ret.result !== false){ layer.close(index); reload(); }
					});
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
				+ '<div class="layui-form-item"><label class="layui-form-label">附件 XID</label><div class="layui-input-block" style="display:flex;gap:8px"><input name="attachmentXid" class="layui-input" value="'+esc(row.attachmentXid || '')+'" placeholder="系统附件上传后返回的 xid"><button type="button" class="layui-btn layui-btn-primary" id="btnMediaFill_'+instanceKey+'">读取附件</button></div></div>'
				+ '<div class="layui-form-item"><label class="layui-form-label">URL</label><div class="layui-input-block"><input name="url" class="layui-input" value="'+esc(row.url || '')+'" placeholder="/uploads/demo.png"></div></div>'
				+ '<div class="layui-form-item"><label class="layui-form-label">MIME</label><div class="layui-input-block"><input name="mime" class="layui-input" value="'+esc(row.mime || '')+'" placeholder="image/png"></div></div>'
				+ '<div class="layui-form-item"><label class="layui-form-label">扩展名</label><div class="layui-input-block"><input name="ext" class="layui-input" value="'+esc(row.ext || '')+'" placeholder="png"></div></div>'
				+ '<div class="layui-form-item"><label class="layui-form-label">大小</label><div class="layui-input-block"><input name="size" type="number" class="layui-input" value="'+esc(row.size || 0)+'" placeholder="bytes"></div></div>'
				+ '<div class="layui-form-item"><label class="layui-form-label">尺寸</label><div class="layui-input-block" style="display:flex;gap:8px"><input name="width" type="number" class="layui-input" value="'+esc(row.width || 0)+'" placeholder="width"><input name="height" type="number" class="layui-input" value="'+esc(row.height || 0)+'" placeholder="height"><button type="button" class="layui-btn layui-btn-primary" id="btnMediaDetect_'+instanceKey+'">识别</button></div></div>'
				+ '<div class="layui-form-item"><label class="layui-form-label">启用</label><div class="layui-input-block"><input type="checkbox" name="statusEnabled" lay-skin="switch" lay-text="ON|OFF" '+(row.status === 0 ? '' : 'checked')+'></div></div>'
				+ '<div class="x-dialog-actions"><button type="button" class="layui-btn layui-btn-primary" id="btnMediaCancel_'+instanceKey+'">取消</button><button type="button" class="layui-btn" id="btnMediaSave_'+instanceKey+'">保存</button></div>'
				+ '</form></div>';
			var index = layer.open({type:1,title:row.id ? '编辑媒体资源' : '新增媒体资源',area:['620px','680px'],content:html,success:function(dom){
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
				function fillFromAttachment(){
					var form = dom.find('#mediaForm_'+instanceKey)[0];
					var xid = form && form.elements.attachmentXid ? String(form.elements.attachmentXid.value || '').trim() : '';
					if(!xid){ message({result:false,error:'附件 XID 必填'}); return; }
					get('/admin/attachment/get?xid=' + encodeURIComponent(xid)).then(function(ret){
						var data = ret && ret.data ? ret.data : null;
						if(!ret || ret.result === false || !data){ message(ret || {result:false,error:'附件不存在'}); return; }
						if(form.elements.title && !form.elements.title.value) form.elements.title.value = data.filename || xid;
						if(form.elements.url) form.elements.url.value = '/attachment?xid=' + encodeURIComponent(data.xid || xid);
						if(form.elements.mime) form.elements.mime.value = data.mime || '';
						if(form.elements.ext) form.elements.ext.value = data.ext || '';
						if(form.elements.size) form.elements.size.value = data.size || 0;
						message({result:true,message:'附件信息已回填'});
						detectImageSize();
					});
				}
				layui.form.render();
				dom.find('#btnMediaCancel_'+instanceKey).on('click', function(){ layer.close(index); });
				dom.find('#btnMediaFill_'+instanceKey).on('click', fillFromAttachment);
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
				+ '<div class="layui-form-item"><label class="layui-form-label">内容ID</label><div class="layui-input-block"><input name="contentId" type="number" class="layui-input" value="'+esc(row.contentId || '')+'" placeholder="内容ID"></div></div>'
				+ '<div class="layui-form-item"><label class="layui-form-label">SEO 标题</label><div class="layui-input-block"><input name="seoTitle" class="layui-input" value="'+esc(row.seoTitle || '')+'" placeholder="标题覆盖值"></div></div>'
				+ '<div class="layui-form-item"><label class="layui-form-label">关键词</label><div class="layui-input-block"><input name="seoKeywords" class="layui-input" value="'+esc(row.seoKeywords || '')+'" placeholder="关键词1,关键词2"></div></div>'
				+ '<div class="layui-form-item"><label class="layui-form-label">描述</label><div class="layui-input-block"><textarea name="seoDescription" class="layui-textarea" placeholder="meta 描述">'+esc(row.seoDescription || '')+'</textarea></div></div>'
				+ '<div class="layui-form-item"><label class="layui-form-label">规范链接</label><div class="layui-input-block"><input name="canonical" class="layui-input" value="'+esc(row.canonical || '')+'" placeholder="/plugin/{{PLUGIN_XID}}?id=1"></div></div>'
				+ '<div class="layui-form-item"><label class="layui-form-label">启用</label><div class="layui-input-block"><input type="checkbox" name="statusEnabled" lay-skin="switch" lay-text="开|关" '+(row.status === 0 ? '' : 'checked')+'></div></div>'
				+ '<div class="x-dialog-actions"><button type="button" class="layui-btn layui-btn-primary" id="btnSeoCancel_'+instanceKey+'">取消</button><button type="button" class="layui-btn" id="btnSeoSave_'+instanceKey+'">保存</button></div>'
				+ '</form></div>';
			var index = layer.open({type:1,title:row.id ? '编辑 SEO 元信息' : '新增 SEO 元信息',area:['680px','560px'],content:html,success:function(dom){
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
		function runSeoPreview(){
			var idInput = byId('seoPreviewContentId');
			var slugInput = byId('seoPreviewSlug');
			var contentId = idInput ? String(idInput.value || '').trim() : '';
			var slug = slugInput ? String(slugInput.value || '').trim() : '';
			var query = '';
			if(contentId) query = '?id=' + encodeURIComponent(contentId);
			else if(slug) query = '?slug=' + encodeURIComponent(slug);
			else { message({result:false,error:'内容ID或 slug 必填'}); return; }
			get(pub('/seo/meta' + query)).then(function(ret){
				if(ret && ret.data) attachSeoTemplateWarnings(ret.data);
				var target = byId('seoPreviewResult');
				if(target) target.innerHTML = renderSeoPreviewResult(ret, 'content');
				message(ret);
			});
		}
		function renderSeoVariableRows(values){
			values = values || {};
			var keys = Object.keys(values);
			var html = '';
			keys.forEach(function(key){
				html += '<tr><td>'+esc(key)+'</td><td>'+esc(values[key])+'</td></tr>';
			});
			if(!keys.length) html += '<tr><td colspan="2" style="text-align:center;color:#667085;">暂无变量</td></tr>';
			return html;
		}
		function renderSeoPreviewResult(ret, scope){
			var data = ret && ret.data ? ret.data : ret || {};
			var warnings = Array.isArray(data.warnings) ? data.warnings : [];
			var note = scope === 'category' ? '栏目 SEO 预览只在 content.category 和 content.seo 启用时用于校验最终输出。' : '内容 SEO 预览通过公开 seo.meta 接口读取最终生效结果，并受 content.access 保护。';
			var html = '<div class="x-muted">'+esc(note)+'</div>';
			html += '<table class="layui-table"><thead><tr><th>标题</th><th>关键词</th><th>描述</th><th>规范链接</th><th>警告</th></tr></thead><tbody>';
			html += '<tr><td>'+esc(data.title || '')+'</td><td>'+esc(data.keywords || '')+'</td><td>'+esc(data.description || '')+'</td><td>'+esc(data.canonical || '')+'</td><td>'+esc(warnings.join('；') || '无')+'</td></tr>';
			html += '</tbody></table><div class="x-muted">模板变量</div><table class="layui-table"><thead><tr><th>变量</th><th>当前值</th></tr></thead><tbody>';
			html += renderSeoVariableRows(data.templateVariables) + '</tbody></table><pre class="x-code">' + esc(JSON.stringify(data, null, 2)) + '</pre>';
			return html;
		}
		function abilityApplyTextTemplate(template, values){
			return String(template || '').replace(/\{([A-Za-z0-9_]+)\}/g, function(all, key){
				return Object.prototype.hasOwnProperty.call(values || {}, key) ? String(values[key] || '') : all;
			});
		}
		function collectTemplateTokens(text, out){
			String(text || '').replace(/\{([A-Za-z0-9_]+)\}/g, function(all){
				if(out.indexOf(all) < 0) out.push(all);
				return all;
			});
		}
		function attachSeoTemplateWarnings(meta){
			var warnings = [];
			if(meta && typeof meta === 'object'){
				collectTemplateTokens(meta.title, warnings);
				collectTemplateTokens(meta.keywords, warnings);
				collectTemplateTokens(meta.description, warnings);
				collectTemplateTokens(meta.canonical, warnings);
				if(warnings.length) meta.warnings = ['存在未替换的模板变量：' + warnings.join(' ')];
			}
			return meta;
		}
		function loadAbilityPackInstanceConfig(packKey){
			return get(api('/contracts')).then(function(ret){
				var packs = ret && ret.data && ret.data.contracts && ret.data.contracts.abilityPacks || [];
				for(var i=0;i<packs.length;i++){
					if(packs[i] && packs[i].packId === packKey) return packs[i].instanceConfig || {};
				}
				return {};
			});
		}
		function buildSeoCategoryPreview(category, config){
			var pluginTitle = '@@PLUGIN_TITLE@@' || pluginXid;
			var values = {
				categoryId: category.id || '',
				categoryTitle: category.title || '',
				categorySlug: category.slug || '',
				categoryDescription: category.description || '',
				siteName: pluginTitle,
				pluginTitle: pluginTitle,
				pluginXid: pluginXid
			};
			return {
				title: category.seoTitle || abilityApplyTextTemplate(config.categoryTitleTemplate, values) || category.title || '',
				keywords: category.seoKeywords || abilityApplyTextTemplate(config.categoryKeywordsTemplate, values) || '',
				description: category.seoDescription || abilityApplyTextTemplate(config.categoryDescriptionTemplate, values) || category.description || '',
				canonical: abilityApplyTextTemplate(config.categoryCanonicalTemplate, values) || (category.slug ? '/plugin/' + pluginXid + '?categoryId=' + encodeURIComponent(category.id) : ''),
				templateVariables: values
			};
		}
		function runSeoCategoryPreview(){
			var idInput = byId('seoPreviewCategoryId');
			var slugInput = byId('seoPreviewCategorySlug');
			var categoryId = idInput ? String(idInput.value || '').trim() : '';
			var slug = slugInput ? String(slugInput.value || '').trim() : '';
			var query = '';
			if(categoryId) query = '?id=' + encodeURIComponent(categoryId);
			else if(slug) query = '?slug=' + encodeURIComponent(slug);
			else { message({result:false,error:'栏目ID或 slug 必填'}); return; }
			Promise.all([get(pub('/category/detail' + query)), loadAbilityPackInstanceConfig('content.seo')]).then(function(items){
				var ret = items[0];
				var category = ret && ret.data ? ret.data : null;
				if(!ret || ret.result === false || !category){ showJsonResult('seoCategoryPreviewResult', ret || {result:false,error:'栏目不存在'}); return; }
				showJsonResult('seoCategoryPreviewResult', {result:true,data:attachSeoTemplateWarnings(buildSeoCategoryPreview(category, items[1] || {}))});
			});
		}
		var configs = {
			overview:{cols:[{field:'packId',title:'能力包'}]},
			'content.seo':{
				ops:'edit,delete',
				listApi:'/seo/list',
				cols:[{field:'id',title:'ID',width:80},{field:'contentId',title:'内容ID',width:110},{field:'contentTitle',title:'内容标题',minWidth:180},{field:'seoTitle',title:'SEO 标题',minWidth:180},{field:'seoKeywords',title:'关键词',minWidth:160},{field:'canonical',title:'规范链接',minWidth:220},{field:'status',title:'启用',width:90},{field:'updateTimeText',title:'更新时间',width:170},{title:'操作',toolbar:'#rowActions',width:140}],
				form:function(){return '<div class="x-toolbar"><div><div class="x-muted">SEO 元信息仅在 content.seo 启用时加载。内容保存和导入会同步基础数据，这里用于人工覆盖。</div><div class="x-muted" style="margin-top:6px">内容模板变量：{id} {contentId} {title} {keywords} {description} {summary} {slug} {canonical} {categoryId} {siteName} {pluginXid} {seoTitle} {seoKeywords} {seoDescription}</div><div class="x-muted" style="margin-top:4px">栏目模板变量：{categoryId} {categoryTitle} {categorySlug} {categoryDescription} {siteName} {pluginTitle} {pluginXid}</div><div style="display:flex;gap:8px;margin-top:8px;max-width:620px"><input id="seoPreviewContentId" class="layui-input" type="number" placeholder="内容ID"><input id="seoPreviewSlug" class="layui-input" placeholder="内容 slug"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnSeoPreview"><i class="layui-icon layui-icon-search"></i> 预览内容</button></div><pre id="seoPreviewResult" class="x-code" style="margin-top:8px"></pre><div style="display:flex;gap:8px;margin-top:8px;max-width:620px"><input id="seoPreviewCategoryId" class="layui-input" type="number" placeholder="栏目ID"><input id="seoPreviewCategorySlug" class="layui-input" placeholder="栏目 slug"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnSeoCategoryPreview"><i class="layui-icon layui-icon-search"></i> 预览栏目</button></div><pre id="seoCategoryPreviewResult" class="x-code" style="margin-top:8px"></pre></div><div class="layui-btn-container"><button type="button" class="layui-btn layui-btn-sm" id="btnAddSeo"><i class="layui-icon layui-icon-add-1"></i> 新增元信息</button></div></div>';}
			},
			'content.comment':{
				ops:'audit,hide,delete',
				cols:[{field:'id',title:'ID',width:80},{field:'content_id',title:'内容ID',width:100},{field:'author_name',title:'作者',width:140},{field:'body',title:'评论内容'},{field:'status',title:'状态',width:90},{field:'create_time',title:'创建时间',width:160},{title:'操作',toolbar:'#rowActions',width:140}],
				views:{
					main:{cols:[{field:'id',title:'ID',width:80},{field:'content_id',title:'内容ID',width:100},{field:'author_name',title:'作者',width:140},{field:'body',title:'评论内容'},{field:'status',title:'状态',width:90},{field:'create_time',title:'创建时间',width:160},{title:'操作',toolbar:'#rowActions',width:140}]},
					audit:{listApi:'/comment/moderation-queue',ops:'audit,hide,delete',listQuery:function(){return queryFromPairs([['contentId', abilityListFilters.commentQueueContentId], ['authorName', abilityListFilters.commentQueueAuthorName], ['body', abilityListFilters.commentQueueBody], ['ip', abilityListFilters.commentQueueIp]]);},cols:[{field:'id',title:'ID',width:80},{field:'contentId',title:'内容ID',width:100},{field:'contentTitle',title:'内容标题',minWidth:160},{field:'authorName',title:'作者',width:140},{field:'body',title:'待审评论',minWidth:240},{field:'riskScore',title:'风险分',width:90},{field:'riskFlags',title:'风险原因',minWidth:160},{field:'bodyLength',title:'字节',width:80},{field:'linkCount',title:'链接',width:80},{field:'ip',title:'IP',width:130},{field:'auditCount',title:'审核次数',width:90},{field:'createTimeText',title:'提交时间',width:170},{title:'审核',toolbar:'#rowActions',width:140}]},
					auditLogs:{listApi:'/comment/audit-log/list',ops:'',listQuery:function(){return queryFromPairs([['commentId', abilityListFilters.commentAuditCommentId], ['action', abilityListFilters.commentAuditAction]]);},cols:[{field:'id',title:'ID',width:80},{field:'commentId',title:'评论ID',width:100},{field:'action',title:'动作',width:130},{field:'operatorId',title:'操作人',width:120},{field:'note',title:'备注',minWidth:220},{field:'createTimeText',title:'时间',width:170}]},
					notifications:{listApi:'/comment/notification/list',ops:'',listQuery:function(){return queryFromPairs([['commentId', abilityListFilters.commentNotificationCommentId], ['contentId', abilityListFilters.commentNotificationContentId], ['status', abilityListFilters.commentNotificationStatus], ['event', abilityListFilters.commentNotificationEvent]]);},cols:[{field:'id',title:'ID',width:80},{field:'commentId',title:'评论ID',width:100},{field:'contentId',title:'内容ID',width:100},{field:'event',title:'事件',width:170},{field:'title',title:'标题',width:160},{field:'body',title:'内容'},{field:'status',title:'状态',width:90},{field:'createTimeText',title:'时间',width:170}]}
				},
				form:function(){return viewSwitch([{key:'main',text:'全部评论'},{key:'audit',text:'审核'},{key:'auditLogs',text:'审核日志'},{key:'notifications',text:'通知'}]) + '<div class="x-toolbar"><div><div class="x-muted">评论由前端接口提交，可在列表中审核通过、驳回或隐藏；审核队列、审核日志和通知视图筛选只影响后台表格查询。</div><div style="display:flex;gap:8px;margin-top:8px;max-width:1100px;flex-wrap:wrap"><input id="commentQueueContentId" class="layui-input" type="number" style="width:150px" placeholder="待审内容ID" value="'+esc(abilityListFilters.commentQueueContentId || '')+'"><input id="commentQueueAuthorName" class="layui-input" style="width:150px" placeholder="待审作者" value="'+esc(abilityListFilters.commentQueueAuthorName || '')+'"><input id="commentQueueBody" class="layui-input" style="width:190px" placeholder="待审正文包含" value="'+esc(abilityListFilters.commentQueueBody || '')+'"><input id="commentQueueIp" class="layui-input" style="width:150px" placeholder="待审 IP" value="'+esc(abilityListFilters.commentQueueIp || '')+'"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnCommentQueueFilter"><i class="layui-icon layui-icon-search"></i> 筛选待审</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnCommentQueueFilterClear"><i class="layui-icon layui-icon-close"></i> 清空待审</button></div><div style="display:flex;gap:8px;margin-top:8px;max-width:1100px;flex-wrap:wrap"><input id="commentAuditCommentId" class="layui-input" type="number" style="width:150px" placeholder="日志评论ID" value="'+esc(abilityListFilters.commentAuditCommentId || '')+'"><input id="commentAuditAction" class="layui-input" style="width:150px" placeholder="日志动作" value="'+esc(abilityListFilters.commentAuditAction || '')+'"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnCommentAuditFilter"><i class="layui-icon layui-icon-search"></i> 筛选日志</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnCommentAuditFilterClear"><i class="layui-icon layui-icon-close"></i> 清空日志</button></div><div style="display:flex;gap:8px;margin-top:8px;max-width:1100px;flex-wrap:wrap"><input id="commentNotificationCommentId" class="layui-input" type="number" style="width:150px" placeholder="通知评论ID" value="'+esc(abilityListFilters.commentNotificationCommentId || '')+'"><input id="commentNotificationContentId" class="layui-input" type="number" style="width:150px" placeholder="通知内容ID" value="'+esc(abilityListFilters.commentNotificationContentId || '')+'"><input id="commentNotificationStatus" class="layui-input" style="width:120px" placeholder="通知状态" value="'+esc(abilityListFilters.commentNotificationStatus || '')+'"><input id="commentNotificationEvent" class="layui-input" style="width:180px" placeholder="通知事件" value="'+esc(abilityListFilters.commentNotificationEvent || '')+'"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnCommentNotificationFilter"><i class="layui-icon layui-icon-search"></i> 筛选通知</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnCommentNotificationFilterClear"><i class="layui-icon layui-icon-close"></i> 清空通知</button></div><div style="display:flex;gap:8px;margin-top:8px;max-width:900px;flex-wrap:wrap"><input id="commentBatchIds" class="layui-input" style="width:260px" placeholder="批量评论ID，逗号分隔"><button type="button" class="layui-btn layui-btn-sm" id="btnCommentBatchApprove"><i class="layui-icon layui-icon-ok"></i> 批量通过</button><button type="button" class="layui-btn layui-btn-warm layui-btn-sm" id="btnCommentBatchReject"><i class="layui-icon layui-icon-close"></i> 批量驳回</button></div><pre id="commentBatchResult" class="x-code" style="margin-top:8px"></pre></div></div>';}
			},
			'content.tag':{
				ops:'delete',
				saveApi:'/tag/save',
				cols:[{field:'id',title:'ID',width:80},{field:'name',title:'标签名'},{field:'slug',title:'别名'},{field:'content_count',title:'内容数',width:100},{field:'status',title:'状态',width:90},{title:'操作',toolbar:'#rowActions',width:100}],
				views:{
					main:{cols:[{field:'id',title:'ID',width:80},{field:'name',title:'标签名'},{field:'slug',title:'别名'},{field:'content_count',title:'内容数',width:100},{field:'status',title:'状态',width:90},{title:'操作',toolbar:'#rowActions',width:100}]},
					contents:{listApi:'/tag/content/list', ops:'unbind', listQuery:function(){return queryFromPairs([['tagId', abilityListFilters.tagLinkTagId], ['contentId', abilityListFilters.tagLinkContentId]]);}, cols:[{field:'id',title:'ID',width:80},{field:'tagId',title:'标签ID',width:100},{field:'tagName',title:'标签名',width:160},{field:'contentId',title:'内容ID',width:100},{field:'contentTitle',title:'内容标题'},{field:'sort',title:'排序',width:90},{field:'createTime',title:'绑定时间',width:160},{title:'操作',toolbar:'#rowActions',width:100}]}
				},
				form:function(){return viewSwitch([{key:'main',text:'标签'},{key:'contents',text:'内容关联'}]) + simpleForm([{name:'name',label:'标签名',placeholder:'请输入标签名'},{name:'slug',label:'别名',placeholder:'例如 news'},{name:'status',label:'状态',type:'number',value:'1'}]) + '<div class="x-toolbar"><div><div class="x-muted">内容关联筛选只读取后台绑定表，结果继续受 maxAdminLinkRows 限制。</div><div style="display:flex;gap:8px;align-items:center;flex-wrap:wrap;margin-top:8px"><input id="tagLinkTagId" class="layui-input" type="number" style="width:130px" value="'+esc(abilityListFilters.tagLinkTagId || '')+'" placeholder="标签ID"><input id="tagLinkContentId" class="layui-input" type="number" style="width:130px" value="'+esc(abilityListFilters.tagLinkContentId || '')+'" placeholder="内容ID"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnTagLinkFilter"><i class="layui-icon layui-icon-search"></i> 筛选关联</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnTagLinkFilterClear"><i class="layui-icon layui-icon-close"></i> 清空</button></div></div></div>';}
			},
			'content.topic':{
				ops:'delete',
				saveApi:'/topic/save',
				cols:[{field:'id',title:'ID',width:80},{field:'title',title:'专题标题'},{field:'slug',title:'别名'},{field:'content_count',title:'内容数',width:100},{field:'status',title:'状态',width:90},{title:'操作',toolbar:'#rowActions',width:100}],
				views:{
					main:{cols:[{field:'id',title:'ID',width:80},{field:'title',title:'专题标题'},{field:'slug',title:'别名'},{field:'content_count',title:'内容数',width:100},{field:'status',title:'状态',width:90},{title:'操作',toolbar:'#rowActions',width:100}]},
					contents:{listApi:'/topic/content/list', ops:'unbind', listQuery:function(){return queryFromPairs([['topicId', abilityListFilters.topicLinkTopicId], ['contentId', abilityListFilters.topicLinkContentId]]);}, cols:[{field:'id',title:'ID',width:80},{field:'topicId',title:'专题ID',width:100},{field:'topicTitle',title:'专题标题',width:180},{field:'contentId',title:'内容ID',width:100},{field:'contentTitle',title:'内容标题'},{field:'sort',title:'排序',width:90},{field:'createTime',title:'绑定时间',width:160},{title:'操作',toolbar:'#rowActions',width:100}]}
				},
				form:function(){return viewSwitch([{key:'main',text:'专题'},{key:'contents',text:'内容关联'}]) + simpleForm([{name:'title',label:'专题标题',placeholder:'请输入专题标题'},{name:'slug',label:'别名',placeholder:'例如 product'},{name:'status',label:'状态',type:'number',value:'1'}]) + '<div class="x-toolbar"><div><div class="x-muted">内容关联筛选只读取后台绑定表，结果继续受 maxAdminLinkRows 限制。</div><div style="display:flex;gap:8px;align-items:center;flex-wrap:wrap;margin-top:8px"><input id="topicLinkTopicId" class="layui-input" type="number" style="width:130px" value="'+esc(abilityListFilters.topicLinkTopicId || '')+'" placeholder="专题ID"><input id="topicLinkContentId" class="layui-input" type="number" style="width:130px" value="'+esc(abilityListFilters.topicLinkContentId || '')+'" placeholder="内容ID"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnTopicLinkFilter"><i class="layui-icon layui-icon-search"></i> 筛选关联</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnTopicLinkFilterClear"><i class="layui-icon layui-icon-close"></i> 清空</button></div></div></div><div class="x-toolbar"><div><div class="x-muted">加载专题内容列表后，可使用上移和下移调整顺序，再通过有上限的排序 API 保存。专题 ID 留空时加载当前关联列表。</div><div style="display:flex;gap:8px;margin-top:8px;max-width:520px"><input id="topicArrangeTopicId" class="layui-input" type="number" placeholder="专题ID"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnTopicArrangeLoad"><i class="layui-icon layui-icon-refresh"></i> 加载</button></div><div id="topicArrangeList" class="topic-arrange"></div><textarea id="topicSortItems" class="layui-textarea" style="margin-top:8px" placeholder=\'[{\"id\":1,\"sort\":10}]\'></textarea><pre id="topicSortResult" class="x-code" style="margin-top:8px"></pre></div><div class="layui-btn-container"><button type="button" class="layui-btn layui-btn-sm" id="btnTopicSort"><i class="layui-icon layui-icon-ok"></i> 保存排序</button></div></div>';}
			},
			'content.sensitive':{
				ops:'delete',
				saveApi:'/sensitive/word/save',
				cols:[{field:'id',title:'ID',width:80},{field:'word',title:'敏感词'},{field:'level',title:'级别',width:90},{field:'scope',title:'作用域',width:120},{field:'group_key',title:'词库分组',width:130},{field:'replacement',title:'替换词'},{field:'status',title:'状态',width:90},{title:'操作',toolbar:'#rowActions',width:100}],
				views:{
					main:{listQuery:function(){return queryFromPairs([['groupKey', abilityListFilters.sensitiveGroupKey]]);},cols:[{field:'id',title:'ID',width:80},{field:'word',title:'敏感词'},{field:'level',title:'级别',width:90},{field:'scope',title:'作用域',width:120},{field:'group_key',title:'词库分组',width:130},{field:'replacement',title:'替换词'},{field:'status',title:'状态',width:90},{title:'操作',toolbar:'#rowActions',width:100}]},
					logs:{listApi:'/sensitive/log/list', listQuery:function(){return queryFromPairs([['targetType', abilityListFilters.sensitiveLogTargetType], ['targetId', abilityListFilters.sensitiveLogTargetId], ['word', abilityListFilters.sensitiveLogWord], ['fieldName', abilityListFilters.sensitiveLogFieldName], ['action', abilityListFilters.sensitiveLogAction]]);}, cols:[{field:'id',title:'ID',width:80},{field:'targetType',title:'目标类型',width:110},{field:'targetId',title:'目标ID',width:100},{field:'word',title:'命中词'},{field:'fieldName',title:'字段',width:120},{field:'action',title:'动作',width:100},{field:'createTime',title:'命中时间',width:160}]}
				},
				form:function(){return viewSwitch([{key:'main',text:'词库'},{key:'logs',text:'命中日志'}]) + simpleForm([{name:'word',label:'敏感词',placeholder:'请输入敏感词'},{name:'level',label:'级别',type:'number',value:'1'},{name:'scope',label:'作用域',value:'content'},{name:'group_key',label:'词库分组',value:'default',placeholder:'default'},{name:'replacement',label:'替换词',value:'***'},{name:'status',label:'状态',type:'number',value:'1'}]) + '<div class="x-toolbar"><div><div class="x-muted">命中日志筛选只影响后台表格查询，扫描热路径仍然只按 scope 加载词库。</div><div style="display:flex;gap:8px;align-items:center;flex-wrap:wrap;margin-top:8px"><input id="sensitiveLogTargetType" class="layui-input" style="width:130px" value="'+esc(abilityListFilters.sensitiveLogTargetType || '')+'" placeholder="targetType"><input id="sensitiveLogTargetId" class="layui-input" type="number" style="width:120px" value="'+esc(abilityListFilters.sensitiveLogTargetId || '')+'" placeholder="targetId"><input id="sensitiveLogWord" class="layui-input" style="width:140px" value="'+esc(abilityListFilters.sensitiveLogWord || '')+'" placeholder="word"><input id="sensitiveLogFieldName" class="layui-input" style="width:130px" value="'+esc(abilityListFilters.sensitiveLogFieldName || '')+'" placeholder="fieldName"><input id="sensitiveLogAction" class="layui-input" style="width:120px" value="'+esc(abilityListFilters.sensitiveLogAction || '')+'" placeholder="action"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnSensitiveLogFilter"><i class="layui-icon layui-icon-search"></i> 筛选日志</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnSensitiveLogFilterClear"><i class="layui-icon layui-icon-close"></i> 清空</button></div></div></div>';}
			},
			'content.slug':{
				ops:'',
				listApi:'/slug/history',
				listQuery:function(){return queryFromPairs([['contentId', abilityListFilters.slugHistoryContentId], ['oldSlug', abilityListFilters.slugHistoryOldSlug], ['newSlug', abilityListFilters.slugHistoryNewSlug], ['status', abilityListFilters.slugHistoryStatus]]);},
				cols:[{field:'id',title:'ID',width:80},{field:'contentId',title:'内容ID',width:110},{field:'oldSlug',title:'旧 slug',minWidth:180},{field:'newSlug',title:'新 slug',minWidth:180},{field:'status',title:'状态',width:90},{field:'createTimeText',title:'变更时间',width:170}],
				form:function(){return '<div class="x-toolbar"><div><div class="x-muted">固定链接仅在 content.slug 启用时加载，提供保存期唯一性检查、URL 预览、历史记录和批量修复。</div><div style="display:flex;gap:8px;margin-top:8px;max-width:720px"><input id="slugProbe" class="layui-input" placeholder="slug"><input id="slugProbeId" class="layui-input" style="width:140px" placeholder="排除内容ID"></div><div style="display:flex;gap:8px;margin-top:8px;max-width:860px;flex-wrap:wrap"><input id="slugHistoryContentId" class="layui-input" type="number" style="width:130px" value="'+esc(abilityListFilters.slugHistoryContentId || '')+'" placeholder="内容ID"><input id="slugHistoryOldSlug" class="layui-input" style="width:180px" value="'+esc(abilityListFilters.slugHistoryOldSlug || '')+'" placeholder="旧 slug"><input id="slugHistoryNewSlug" class="layui-input" style="width:180px" value="'+esc(abilityListFilters.slugHistoryNewSlug || '')+'" placeholder="新 slug"><input id="slugHistoryStatus" class="layui-input" style="width:110px" value="'+esc(abilityListFilters.slugHistoryStatus || '')+'" placeholder="状态"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnSlugHistoryFilter"><i class="layui-icon layui-icon-search"></i> 筛选历史</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnSlugHistoryFilterClear"><i class="layui-icon layui-icon-close"></i> 清空历史</button></div><pre id="slugProbeResult" class="x-code" style="margin-top:8px"></pre></div><div class="layui-btn-container"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnSlugCheck"><i class="layui-icon layui-icon-search"></i> 检查</button><button type="button" class="layui-btn layui-btn-sm" id="btnSlugPreview"><i class="layui-icon layui-icon-link"></i> 预览 URL</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnSlugRuleExplain"><i class="layui-icon layui-icon-read"></i> 规则说明</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnUnifiedRouteRuleExplain"><i class="layui-icon layui-icon-template"></i> 统一计划</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnUnifiedRouteRuleValidate"><i class="layui-icon layui-icon-vercode"></i> 批量验证</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnSlugRepairPreview"><i class="layui-icon layui-icon-list"></i> 预览修复</button><button type="button" class="layui-btn layui-btn-danger layui-btn-sm" id="btnSlugRepairConfirm"><i class="layui-icon layui-icon-ok"></i> 应用修复</button></div></div>';}
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
				cols:[{field:'id',title:'ID',width:80},{field:'attachmentXid',title:'附件 XID',width:140},{field:'title',title:'标题',minWidth:160},{field:'url',title:'URL',minWidth:260},{field:'mime',title:'MIME',width:130},{field:'size',title:'大小',width:100},{field:'width',title:'宽',width:80},{field:'height',title:'高',width:80},{field:'status',title:'启用',width:80},{field:'updateTimeText',title:'更新时间',width:160},{title:'操作',toolbar:'#rowActions',width:140}],
				form:function(){return '<div class="x-toolbar"><div class="x-muted">媒体能力包管理内容资源登记和引用关系；真实上传复用系统附件能力。</div><div class="layui-btn-container"><a class="layui-btn layui-btn-primary layui-btn-sm" target="_blank" href="/admin/view/attachment/upload"><i class="layui-icon layui-icon-upload"></i> 上传附件</a><button type="button" class="layui-btn layui-btn-sm" id="btnAddMedia"><i class="layui-icon layui-icon-add-1"></i> 新增资源</button></div></div>';}
			},
			'content.revision':{
				ops:'',
				listApi:'/revision/list',
				cols:[{field:'id',title:'ID',width:80},{field:'contentId',title:'内容ID',width:100},{field:'revisionNo',title:'版本号',width:100},{field:'title',title:'标题',minWidth:180},{field:'status',title:'状态',width:90},{field:'isDraft',title:'草稿',width:80},{field:'action',title:'动作',width:100},{field:'createTimeText',title:'创建时间',width:170}],
				listQuery:function(){
					return queryFromPairs([['contentId', abilityListFilters.revisionContentId], ['action', abilityListFilters.revisionAction], ['status', abilityListFilters.revisionStatus]]);
				},
				form:function(){return '<div class="x-toolbar"><div><div class="x-muted">内容保存时自动生成版本快照；可按内容 ID、动作和状态过滤历史版本，恢复前会先展示当前内容与目标版本的差异。</div><div style="display:flex;gap:8px;margin-top:8px;max-width:760px"><input id="revisionFilterContentId" class="layui-input" type="number" style="width:140px" value="'+esc(abilityListFilters.revisionContentId || '')+'" placeholder="内容ID"><input id="revisionFilterAction" class="layui-input" style="width:150px" value="'+esc(abilityListFilters.revisionAction || '')+'" placeholder="动作"><input id="revisionFilterStatus" class="layui-input" style="width:120px" value="'+esc(abilityListFilters.revisionStatus || '')+'" placeholder="状态"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnRevisionFilter"><i class="layui-icon layui-icon-search"></i> 筛选</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnRevisionFilterClear"><i class="layui-icon layui-icon-close"></i> 清空</button></div></div></div>';}
			},
			'content.workflow':{
				ops:'',
				listApi:'/workflow/log/list',
				cols:[{field:'id',title:'ID',width:80},{field:'contentId',title:'内容ID',width:100},{field:'action',title:'动作',width:100},{field:'fromStatus',title:'原状态',width:90},{field:'toStatus',title:'新状态',width:90},{field:'fromDraft',title:'原草稿',width:90},{field:'toDraft',title:'新草稿',width:90},{field:'assigneeId',title:'审核人',width:100},{field:'reason',title:'原因',minWidth:180},{field:'createTimeText',title:'时间',width:170}],
				views:{
					main:{ops:'',listApi:'/workflow/log/list',listQuery:function(){return queryFromPairs([['contentId', abilityListFilters.workflowContentId], ['assigneeId', abilityListFilters.workflowAssigneeId], ['action', abilityListFilters.workflowAction]]);},cols:[{field:'id',title:'ID',width:80},{field:'contentId',title:'内容ID',width:100},{field:'action',title:'动作',width:100},{field:'fromStatus',title:'原状态',width:90},{field:'toStatus',title:'新状态',width:90},{field:'fromDraft',title:'原草稿',width:90},{field:'toDraft',title:'新草稿',width:90},{field:'assigneeId',title:'审核人',width:100},{field:'reason',title:'原因',minWidth:180},{field:'createTimeText',title:'时间',width:170}]},
					todo:{ops:'',listApi:'/workflow/todo/list',listQuery:function(){return queryFromPairs([['assigneeId', abilityListFilters.workflowAssigneeId], ['status', abilityListFilters.workflowStatus]]);},cols:[{field:'id',title:'内容ID',width:100},{field:'title',title:'标题',minWidth:200},{field:'status',title:'状态',width:90},{field:'isDraft',title:'草稿',width:80},{field:'assigneeId',title:'审核人',width:100},{field:'lastAction',title:'最后动作',width:110},{field:'lastReason',title:'最后原因',minWidth:180},{field:'logTimeText',title:'流转时间',width:170},{field:'updateTimeText',title:'更新时间',width:170}]},
					notifications:{ops:'',listApi:'/workflow/notification/list',listQuery:function(){return queryFromPairs([['contentId', abilityListFilters.workflowContentId], ['assigneeId', abilityListFilters.workflowAssigneeId], ['action', abilityListFilters.workflowAction], ['readStatus', abilityListFilters.workflowReadStatus]]);},cols:[{field:'id',title:'ID',width:80},{field:'contentId',title:'内容ID',width:100},{field:'action',title:'动作',width:120},{field:'message',title:'通知内容',minWidth:260},{field:'assigneeId',title:'审核人',width:100},{field:'readStatus',title:'已读',width:80},{field:'createTimeText',title:'时间',width:170}]}
				},
				form:function(){return viewSwitch([{key:'main',text:'流转日志'},{key:'todo',text:'审核待办'},{key:'notifications',text:'通知'}]) + '<div class="x-toolbar"><div><div class="x-muted">工作流动作只在 content.workflow 启用时注册。审核人用于记录转交对象；publishAt 使用 Unix 时间戳表示定时发布；到期任务执行数量会被限制在 1-200。</div><div style="display:flex;gap:8px;margin-top:8px;max-width:1080px;flex-wrap:wrap"><input id="workflowContentId" class="layui-input" style="width:140px" value="'+esc(abilityListFilters.workflowContentId || '')+'" placeholder="内容ID"><input id="workflowAssigneeId" class="layui-input" style="width:150px" value="'+esc(abilityListFilters.workflowAssigneeId || '')+'" placeholder="审核人ID"><input id="workflowAction" class="layui-input" style="width:140px" value="'+esc(abilityListFilters.workflowAction || '')+'" placeholder="动作"><input id="workflowStatus" class="layui-input" style="width:120px" value="'+esc(abilityListFilters.workflowStatus || '')+'" placeholder="待办状态"><input id="workflowReadStatus" class="layui-input" style="width:120px" value="'+esc(abilityListFilters.workflowReadStatus || '')+'" placeholder="通知已读"><input id="workflowPublishAt" class="layui-input" style="width:190px" placeholder="发布时间戳"><input id="workflowScheduledLimit" class="layui-input" type="number" min="1" max="200" style="width:120px" value="50" placeholder="执行上限"><input id="workflowReason" class="layui-input" placeholder="原因"></div><pre id="workflowScheduledResult" class="x-code" style="margin-top:8px"></pre></div><div class="layui-btn-container"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnWorkflowFilter"><i class="layui-icon layui-icon-search"></i> 筛选列表</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnWorkflowFilterClear"><i class="layui-icon layui-icon-close"></i> 清空筛选</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnWorkflowSubmit"><i class="layui-icon layui-icon-upload"></i> 提交审核</button><button type="button" class="layui-btn layui-btn-sm" id="btnWorkflowApprove"><i class="layui-icon layui-icon-ok"></i> 审核通过</button><button type="button" class="layui-btn layui-btn-normal layui-btn-sm" id="btnWorkflowSchedule"><i class="layui-icon layui-icon-date"></i> 定时发布</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnWorkflowRunScheduled"><i class="layui-icon layui-icon-refresh"></i> 执行到期</button><button type="button" class="layui-btn layui-btn-warm layui-btn-sm" id="btnWorkflowReject"><i class="layui-icon layui-icon-close"></i> 驳回</button><button type="button" class="layui-btn layui-btn-danger layui-btn-sm" id="btnWorkflowOffline"><i class="layui-icon layui-icon-down"></i> 下线</button></div></div>';}
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
					imports:{listApi:'/import-export/import/jobs', listQuery:function(){return queryFromPairs([['status', abilityListFilters.importExportJobStatus]]);}, cols:[{field:'id',title:'ID',width:80},{field:'sourceName',title:'来源',minWidth:180},{field:'status',title:'状态',width:100},{field:'totalCount',title:'总数',width:90},{field:'successCount',title:'通过',width:90},{field:'failCount',title:'失败',width:90},{field:'reportJson',title:'报告',minWidth:260},{field:'createTimeText',title:'创建时间',width:170}]},
					exports:{listApi:'/import-export/export/jobs', listQuery:function(){return queryFromPairs([['status', abilityListFilters.importExportJobStatus]]);}, cols:[{field:'id',title:'ID',width:80},{field:'exportType',title:'类型',width:100},{field:'status',title:'状态',width:100},{field:'totalCount',title:'总数',width:90},{field:'filterJson',title:'筛选',minWidth:180},{field:'resultJson',title:'结果',minWidth:260},{field:'createTimeText',title:'创建时间',width:170}]}
				},
				form:function(){return viewSwitch([{key:'imports',text:'导入预检'},{key:'exports',text:'导出任务'}]) + '<div class="x-toolbar"><div><div class="x-muted">任务状态筛选只影响后台任务表格，不改变导入、导出执行逻辑。</div><div style="display:flex;gap:8px;align-items:center;flex-wrap:wrap;margin-top:8px"><input id="importExportJobStatus" class="layui-input" style="width:160px" value="'+esc(abilityListFilters.importExportJobStatus || '')+'" placeholder="状态，例如 finished"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnImportExportJobFilter"><i class="layui-icon layui-icon-search"></i> 筛选任务</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnImportExportJobFilterClear"><i class="layui-icon layui-icon-close"></i> 清空</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnImportExportStats"><i class="layui-icon layui-icon-chart"></i> 任务统计</button></div></div></div><div class="x-toolbar"><div><div class="x-muted">当前支持字段白名单、JSON 导出、导入预检和确认导入；确认导入需要调用 commit API 并显式传 confirm=true。</div><div style="display:flex;gap:8px;margin-top:8px;max-width:720px"><input id="importExportFields" class="layui-input" style="width:360px" placeholder="字段白名单，逗号分隔；留空表示全部字段"><input id="importExportLimit" class="layui-input" style="width:120px" placeholder="limit"><input id="importExportOffset" class="layui-input" style="width:120px" placeholder="offset"></div><pre id="importExportResult" class="x-code" style="margin-top:8px"></pre></div><div class="layui-btn-container"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnImportPreview"><i class="layui-icon layui-icon-list"></i> 预检示例</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnImportExportFieldPlan"><i class="layui-icon layui-icon-form"></i> 字段计划</button><button type="button" class="layui-btn layui-btn-sm" id="btnExportJson"><i class="layui-icon layui-icon-export"></i> 导出 JSON</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnExportJsonNext"><i class="layui-icon layui-icon-next"></i> 下一片</button></div></div>';}
			},
			'content.static':{
				ops:'delete',
				saveApi:'/static/rule/save',
				cols:[{field:'id',title:'ID',width:80},{field:'name',title:'规则名'},{field:'pathPattern',title:'路径规则'},{field:'templateName',title:'模板'},{field:'status',title:'状态',width:90},{title:'操作',toolbar:'#rowActions',width:100}],
				listApi:'/static/rule/list',
				views:{
					main:{listApi:'/static/rule/list', listQuery:function(){return queryFromPairs([['pathPattern', abilityListFilters.staticRulePathPattern], ['status', abilityListFilters.staticRuleStatus]]);}, cols:[{field:'id',title:'ID',width:80},{field:'name',title:'规则名'},{field:'pathPattern',title:'路径规则'},{field:'templateName',title:'模板'},{field:'status',title:'状态',width:90},{title:'操作',toolbar:'#rowActions',width:100}]},
					tasks:{listApi:'/static/task/list', ops:'retry', listQuery:function(){return queryFromPairs([['status', abilityListFilters.staticTaskStatus], ['ruleId', abilityListFilters.staticTaskRuleId], ['targetId', abilityListFilters.staticTaskTargetId]]);}, cols:[{field:'id',title:'ID',width:80},{field:'ruleId',title:'规则ID',width:100},{field:'targetId',title:'内容ID',width:100},{field:'status',title:'状态',width:90},{field:'message',title:'消息'},{field:'createTime',title:'创建时间',width:160},{field:'finishTime',title:'完成时间',width:160},{title:'操作',toolbar:'#rowActions',width:90}]},
					artifacts:{listApi:'/static/artifact/list', listQuery:function(){return queryFromPairs([['targetId', abilityListFilters.staticArtifactTargetId], ['path', abilityListFilters.staticArtifactPath]]);}, cols:[{field:'id',title:'ID',width:80},{field:'ruleId',title:'规则ID',width:100},{field:'targetId',title:'内容ID',width:100},{field:'path',title:'输出路径'},{field:'hash',title:'Hash',width:120},{field:'updateTime',title:'更新时间',width:160}]}
				},
				form:function(){return viewSwitch([{key:'main',text:'规则'},{key:'tasks',text:'任务'},{key:'artifacts',text:'产物'}]) + simpleForm([{name:'name',label:'规则名',placeholder:'详情页静态化'},{name:'pathPattern',label:'路径规则',placeholder:'/article/{id}.html'},{name:'templateName',label:'模板',placeholder:'detail'},{name:'status',label:'状态',type:'number',value:'1'}]) + '<div class="x-toolbar"><div><div class="x-muted">规则、任务和产物筛选只影响后台表格查询，不改变请求热路径。</div><div style="display:flex;gap:8px;margin-top:8px;max-width:760px;flex-wrap:wrap"><input id="staticRulePathFilter" class="layui-input" style="width:180px" value="'+esc(abilityListFilters.staticRulePathPattern || '')+'" placeholder="规则路径"><input id="staticRuleStatusFilter" class="layui-input" style="width:120px" value="'+esc(abilityListFilters.staticRuleStatus || '')+'" placeholder="规则状态"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnStaticRuleFilter"><i class="layui-icon layui-icon-search"></i> 筛选规则</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnStaticRuleFilterClear"><i class="layui-icon layui-icon-close"></i> 清空规则</button></div><div style="display:flex;gap:8px;margin-top:8px;max-width:760px;flex-wrap:wrap"><input id="staticTaskStatusFilter" class="layui-input" style="width:150px" value="'+esc(abilityListFilters.staticTaskStatus || '')+'" placeholder="任务状态：1 或 -1"><input id="staticTaskRuleIdFilter" class="layui-input" style="width:120px" value="'+esc(abilityListFilters.staticTaskRuleId || '')+'" placeholder="规则ID"><input id="staticTaskTargetIdFilter" class="layui-input" style="width:120px" value="'+esc(abilityListFilters.staticTaskTargetId || '')+'" placeholder="内容ID"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnStaticTaskFilter"><i class="layui-icon layui-icon-search"></i> 筛选任务</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnStaticTaskFilterClear"><i class="layui-icon layui-icon-close"></i> 清空任务</button><input id="staticRetryFailedLimit" class="layui-input" type="number" style="width:120px" value="20" placeholder="重试上限"><button type="button" class="layui-btn layui-btn-warm layui-btn-sm" id="btnStaticRetryFailed"><i class="layui-icon layui-icon-refresh"></i> 重试失败任务</button></div><pre id="staticTaskRetryFailedResult" class="x-code" style="margin-top:8px"></pre><div style="display:flex;gap:8px;margin-top:8px;max-width:620px"><input id="staticArtifactTargetIdFilter" class="layui-input" style="width:150px" value="'+esc(abilityListFilters.staticArtifactTargetId || '')+'" placeholder="产物内容ID"><input id="staticArtifactPathFilter" class="layui-input" style="width:220px" value="'+esc(abilityListFilters.staticArtifactPath || '')+'" placeholder="产物路径"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnStaticArtifactFilter"><i class="layui-icon layui-icon-search"></i> 筛选产物</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnStaticArtifactFilterClear"><i class="layui-icon layui-icon-close"></i> 清空产物</button></div></div></div><div class="x-form"><form class="layui-form" id="staticGenerateForm"><div class="layui-form-item"><label class="layui-form-label">内容ID</label><div class="layui-input-block"><input name="targetId" type="number" class="layui-input" value="0"></div></div><div class="layui-form-item"><label class="layui-form-label">规则ID</label><div class="layui-input-block"><input name="ruleId" type="number" class="layui-input" value="0"></div></div><div class="layui-form-item"><label class="layui-form-label">输出路径</label><div class="layui-input-block"><input name="path" class="layui-input" placeholder="/article/1.html"></div></div><div class="layui-form-item"><div class="layui-input-block"><button type="button" class="layui-btn layui-btn-normal layui-btn-sm" id="btnStaticGenerate"><i class="layui-icon layui-icon-release"></i> 生成静态页</button></div></div></form></div><div class="x-form"><form class="layui-form" id="staticCleanForm"><div class="layui-form-item"><label class="layui-form-label">内容ID</label><div class="layui-input-block"><input name="targetId" type="number" class="layui-input" value="0"></div></div><div class="layui-form-item"><label class="layui-form-label">规则ID</label><div class="layui-input-block"><input name="ruleId" type="number" class="layui-input" value="0"></div></div><div class="layui-form-item"><label class="layui-form-label">清理上限</label><div class="layui-input-block"><input name="limit" type="number" class="layui-input" value="100"></div></div><div class="layui-form-item"><div class="layui-input-block"><button type="button" class="layui-btn layui-btn-danger layui-btn-sm" id="btnStaticClean"><i class="layui-icon layui-icon-delete"></i> 清理产物</button></div></div><pre id="staticCleanResult" class="x-code" style="margin-top:8px"></pre></form></div>';}
			},
			'content.like':{
				listApi:'/like/counter/list',
				cols:[{field:'contentId',title:'内容ID',width:120},{field:'likeCount',title:'点赞数',width:120},{field:'updateTime',title:'更新时间'}],
				views:{
					main:{listApi:'/like/counter/list', cols:[{field:'contentId',title:'内容ID',width:120},{field:'likeCount',title:'点赞数',width:120},{field:'updateTime',title:'更新时间'}]},
					records:{listApi:'/like/list', cols:[{field:'id',title:'ID',width:80},{field:'contentId',title:'内容ID',width:100},{field:'actorId',title:'用户ID',width:120},{field:'actorKey',title:'去重键'},{field:'ip',title:'IP',width:140},{field:'status',title:'状态',width:90},{field:'updateTime',title:'更新时间',width:160}]}
				},
				form:function(){return viewSwitch([{key:'main',text:'计数'},{key:'records',text:'记录'}]) + '<div class="x-toolbar"><div class="x-muted">点赞通过前端接口记录，后台可查看内容维度计数、明细记录和汇总统计。</div><div class="layui-btn-container"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnLikeStats"><i class="layui-icon layui-icon-chart"></i> 汇总统计</button></div></div>';}
			},
			'content.view-stat':{
				listApi:'/view/counter/list',
				cols:[{field:'contentId',title:'内容ID',width:120},{field:'viewCount',title:'访问量',width:120},{field:'uniqueViewCount',title:'独立访问',width:120},{field:'lastViewTime',title:'最后访问时间'}],
				views:{
					main:{listApi:'/view/counter/list', cols:[{field:'contentId',title:'内容ID',width:120},{field:'viewCount',title:'访问量',width:120},{field:'uniqueViewCount',title:'独立访问',width:120},{field:'lastViewTime',title:'最后访问时间'}]},
					logs:{listApi:'/view/log/list', cols:[{field:'id',title:'ID',width:80},{field:'contentId',title:'内容ID',width:100},{field:'visitorKey',title:'访客标识'},{field:'ip',title:'IP',width:140},{field:'referer',title:'来源'},{field:'createTime',title:'访问时间',width:160}]},
					daily:{listApi:'/view/daily/list', cols:[{field:'id',title:'ID',width:80},{field:'contentId',title:'内容ID',width:100},{field:'statDate',title:'日期',width:130},{field:'viewCount',title:'访问量',width:120},{field:'uniqueViewCount',title:'独立访问',width:120}]}
				},
				form:function(){return viewSwitch([{key:'main',text:'计数'},{key:'logs',text:'日志'},{key:'daily',text:'日统计'}]) + '<div class="x-toolbar"><div class="x-muted">访问统计通过前端接口记录，后台可查看内容计数、访问日志、日统计和汇总统计。</div><div class="layui-btn-container"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnViewStats"><i class="layui-icon layui-icon-chart"></i> 汇总统计</button></div></div>';}
			}
		};
		configs['content.category'] = {
			ops:'',
			listApi:'/category/list',
			cols:[{field:'id',title:'ID',width:80},{field:'parentId',title:'父级',width:90},{field:'title',title:'栏目标题',minWidth:180},{field:'slug',title:'Slug',width:160},{field:'path',title:'路径',minWidth:200},{field:'contentCount',title:'内容数',width:100},{field:'childCount',title:'子栏目',width:100},{field:'status',title:'状态',width:90}],
			form:function(){return '<div class="x-toolbar"><div><div class="x-muted">启用栏目能力包时，栏目关系优先使用 content_category_bind；旧 content_item.category_id 只作为迁移镜像。</div><pre id="categoryBindStatusResult" class="x-code" style="margin-top:8px"></pre></div><div class="layui-btn-container"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnCategoryBindStatus"><i class="layui-icon layui-icon-search"></i> 迁移状态</button><button type="button" class="layui-btn layui-btn-normal layui-btn-sm" id="btnCategoryBindBackfill"><i class="layui-icon layui-icon-refresh"></i> 回填绑定表</button></div></div>';}
		};
		if(configs['content.workflow']){
			var workflowBaseForm = configs['content.workflow'].form;
			configs['content.workflow'].form = function(){
				return workflowBaseForm() + '<div class="x-toolbar"><div><div class="x-muted">工作流统计只读取待办、通知和日志聚合，不改变状态流转。</div></div><div class="layui-btn-container"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnWorkflowStats"><i class="layui-icon layui-icon-chart"></i> 工作流统计</button></div></div>';
			};
		}
		if(configs['content.static']){
			var staticBaseForm = configs['content.static'].form;
			if(configs['content.static'].views && configs['content.static'].views.main && configs['content.static'].views.main.cols){
				configs['content.static'].views.main.cols.splice(configs['content.static'].views.main.cols.length - 1, 0, {field:'warning',title:'Warning',minWidth:240});
			}
			configs['content.static'].form = function(){
				return staticBaseForm() + '<div class="x-toolbar"><div><div class="x-muted">Static rule preview expands pathPattern with a bounded content sample and only runs during admin editing.</div><div style="display:flex;gap:8px;margin-top:8px;max-width:760px;flex-wrap:wrap"><input id="staticRulePreviewTargetId" class="layui-input" type="number" style="width:150px" placeholder="Content ID"><input id="staticRulePreviewPath" class="layui-input" style="width:260px" placeholder="Optional explicit output path"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnStaticRulePreview"><i class="layui-icon layui-icon-search"></i> Preview static rule</button></div><pre id="staticRulePreviewResult" class="x-code" style="margin-top:8px"></pre></div></div>';
			};
			var staticStatsBaseForm = configs['content.static'].form;
			configs['content.static'].form = function(){
				return staticStatsBaseForm() + '<div class="x-toolbar"><div><div class="x-muted">静态化统计只读取规则、任务和产物聚合结果，不改变生成与清理流程。</div></div><div class="layui-btn-container"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnStaticStats"><i class="layui-icon layui-icon-chart"></i> 静态化统计</button></div></div>';
			};
		}
		if(configs['content.comment']){
			var commentStatsBaseForm = configs['content.comment'].form;
			configs['content.comment'].form = function(){
				return commentStatsBaseForm() + '<div class="x-toolbar"><div><div class="x-muted">审核统计按当前评论表聚合待审、通过、驳回、隐藏和删除数量，只在后台点击时读取。</div><div id="commentStatsResult" style="margin-top:8px"></div></div><div class="layui-btn-container"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnCommentModerationStats"><i class="layui-icon layui-icon-chart"></i> 审核统计</button></div></div>';
			};
		}
		if(configs['content.tag']){
			var tagBaseForm = configs['content.tag'].form;
			configs['content.tag'].form = function(){
				return tagBaseForm() + '<div class="x-toolbar"><div><div class="x-muted">将重复标签合并到目标标签。已有目标绑定会通过 INSERT OR IGNORE 保留，随后移除来源标签绑定。</div><div style="display:flex;gap:8px;align-items:center;flex-wrap:wrap;margin-top:8px;max-width:760px"><input id="tagMergeSourceId" class="layui-input" type="number" min="1" style="width:150px" placeholder="来源标签ID"><input id="tagMergeTargetId" class="layui-input" type="number" min="1" style="width:150px" placeholder="目标标签ID"><label style="display:flex;align-items:center;gap:6px;color:#475467;"><input id="tagMergeDeleteSource" type="checkbox" checked> 删除来源标签</label></div><pre id="tagMergeResult" class="x-code" style="margin-top:8px"></pre></div><div class="layui-btn-container"><button type="button" class="layui-btn layui-btn-warm layui-btn-sm" id="btnTagMerge"><i class="layui-icon layui-icon-transfer"></i> 合并标签</button></div></div><div class="x-toolbar"><div><div class="x-muted">批量启用或停用标签，单次行数受 maxBatchRows 限制。</div><input id="tagBatchIds" class="layui-input" style="width:320px;margin-top:8px" placeholder="标签 ID 列表，逗号分隔"></div><div class="layui-btn-container"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnTagBatchEnable">批量启用</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnTagBatchDisable">批量停用</button></div></div><div class="x-toolbar"><div><div class="x-muted">标签统计只读取 tag 和 content_tag 聚合结果，不改变公开标签输出。</div></div><div class="layui-btn-container"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnTagStats"><i class="layui-icon layui-icon-chart"></i> 标签统计</button></div></div>';
			};
		}
		if(configs['content.topic']){
			var topicBaseForm = configs['content.topic'].form;
			configs['content.topic'].form = function(){
				return topicBaseForm() + '<div class="x-toolbar"><div><div class="x-muted">使用有上限的 JSON 数组调整专题内容排序。每行必须包含 id 和 sort，接口最多接受 500 行。</div><textarea id="topicSortItems" class="layui-textarea" style="width:520px;margin-top:8px" placeholder=\'[{\"id\":1,\"sort\":10}]\'>[]</textarea><pre id="topicSortResult" class="x-code" style="margin-top:8px"></pre></div><div class="layui-btn-container"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnTopicSort"><i class="layui-icon layui-icon-template-1"></i> 保存排序</button></div></div><div class="x-toolbar"><div><div class="x-muted">批量启用或停用专题，单次行数受 maxBatchRows 限制。</div><input id="topicBatchIds" class="layui-input" style="width:320px;margin-top:8px" placeholder="专题 ID 列表，逗号分隔"></div><div class="layui-btn-container"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnTopicBatchEnable">批量启用</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnTopicBatchDisable">批量停用</button></div></div><div class="x-toolbar"><div><div class="x-muted">专题统计只读取 topic 和 topic_content 聚合结果，不改变公开专题输出。</div></div><div class="layui-btn-container"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnTopicStats"><i class="layui-icon layui-icon-chart"></i> 专题统计</button></div></div>';
			};
		}
		if(configs['content.sensitive']){
			var sensitiveBaseForm = configs['content.sensitive'].form;
			configs['content.sensitive'].form = function(){
				return sensitiveBaseForm() + '<div class="x-toolbar"><div><div class="x-muted">词库分组只用于后台运营筛选；扫描仍按 scope 和启用状态执行，不增加请求期分组判断。</div><div style="display:flex;gap:8px;margin-top:8px;max-width:520px"><input id="sensitiveGroupFilter" class="layui-input" placeholder="词库分组，例如 default" value="'+esc(abilityListFilters.sensitiveGroupKey || '')+'"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnSensitiveGroupFilter"><i class="layui-icon layui-icon-search"></i> 筛选分组</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnSensitiveGroupFilterClear"><i class="layui-icon layui-icon-close"></i> 清空</button></div></div></div><div class="x-toolbar"><div><div class="x-muted">批量导入支持预览和确认写入。每行可包含 word、level、scope、groupKey、replacement 和 status，接口最多接受 1000 行。</div><textarea id="sensitiveImportJson" class="layui-textarea" style="width:560px;margin-top:8px" placeholder=\'[{\"word\":\"demo\",\"level\":1,\"scope\":\"content\",\"groupKey\":\"default\",\"replacement\":\"***\",\"status\":1}]\'>[]</textarea><pre id="sensitiveImportResult" class="x-code" style="margin-top:8px"></pre></div><div class="layui-btn-container"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnSensitiveImportPreview"><i class="layui-icon layui-icon-list"></i> 预览导入</button><button type="button" class="layui-btn layui-btn-normal layui-btn-sm" id="btnSensitiveImportConfirm"><i class="layui-icon layui-icon-upload"></i> 确认导入</button></div></div><div class="x-toolbar"><div><div class="x-muted">统计只读取词库和命中日志聚合值，近期窗口由 statsRecentDays 配置控制。</div><pre id="sensitiveStatsResult" class="x-code" style="margin-top:8px"></pre></div><div class="layui-btn-container"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnSensitiveStats"><i class="layui-icon layui-icon-chart"></i> 词库统计</button></div></div><div class="x-toolbar"><div><div class="x-muted">可在不保存内容的情况下按启用词库检测文本。清理命中日志时会按行数上限执行，避免单次任务过重。</div><div style="display:flex;gap:8px;align-items:center;flex-wrap:wrap;margin-top:8px;max-width:760px"><input id="sensitiveCheckScope" class="layui-input" style="width:140px" value="check" placeholder="检测范围"><input id="sensitiveCleanupDays" class="layui-input" type="number" min="1" style="width:140px" value="90" placeholder="保留天数"><input id="sensitiveCleanupLimit" class="layui-input" type="number" min="1" max="10000" style="width:140px" value="1000" placeholder="清理上限"></div><textarea id="sensitiveCheckText" class="layui-textarea" style="width:560px;margin-top:8px" placeholder="待检测文本"></textarea><pre id="sensitiveCheckResult" class="x-code" style="margin-top:8px"></pre><pre id="sensitiveCleanupResult" class="x-code" style="margin-top:8px"></pre></div><div class="layui-btn-container"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnSensitiveCheck"><i class="layui-icon layui-icon-search"></i> 检测文本</button><button type="button" class="layui-btn layui-btn-danger layui-btn-sm" id="btnSensitiveCleanup"><i class="layui-icon layui-icon-delete"></i> 清理日志</button></div></div>';
			};
		}
		if(configs['content.import-export'] && configs['content.import-export'].views && configs['content.import-export'].views.exports){
			var importExportBaseForm = configs['content.import-export'].form;
			configs['content.import-export'].form = function(){
				return importExportBaseForm() + '<div class="x-toolbar"><div><div class="x-muted">导入 conflictMode 支持 insert、update 和 skip。update/skip 会按 id 或 contentId 匹配；失败重放会把导入任务中的失败行载入 JSON 编辑区；分片导入会把 JSON 数组拆成有上限的 API 调用。</div><div style="display:flex;gap:8px;max-width:540px;margin-top:8px"><input id="importExportConflictMode" class="layui-input" style="width:160px" value="insert" placeholder="insert/update/skip"><input id="importReplayJobId" class="layui-input" style="width:160px" placeholder="重放任务ID"><input id="importExportChunkSize" class="layui-input" type="number" min="1" max="200" style="width:140px" value="50" placeholder="分片大小"></div><textarea id="importExportItems" class="layui-textarea" style="width:520px;margin-top:8px" placeholder="JSON 数组数据">[{}]</textarea></div><div class="layui-btn-container"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnImportStage"><i class="layui-icon layui-icon-upload-drag"></i> 载荷落盘</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnImportReplay"><i class="layui-icon layui-icon-refresh"></i> 重放失败</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnImportManualPreview"><i class="layui-icon layui-icon-list"></i> 预览 JSON</button><button type="button" class="layui-btn layui-btn-normal layui-btn-sm" id="btnImportManualCommit"><i class="layui-icon layui-icon-upload"></i> 确认导入</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnImportChunkPreview"><i class="layui-icon layui-icon-template-1"></i> 分片预览</button><button type="button" class="layui-btn layui-btn-normal layui-btn-sm" id="btnImportChunkCommit"><i class="layui-icon layui-icon-upload-drag"></i> 分片导入</button></div></div>';
			};
			configs['content.import-export'].views.exports.ops = 'download';
			configs['content.import-export'].views.exports.cols.push({title:'操作',toolbar:'#rowActions',width:100});
		}
		if(configs['content.access']){
			configs['content.access'].listQuery = function(){return queryFromPairs([['targetType', abilityListFilters.accessTargetType], ['targetId', abilityListFilters.accessTargetId], ['accessMode', abilityListFilters.accessMode], ['status', abilityListFilters.accessStatus]]);};
			configs['content.access'].cols = [{field:'id',title:'ID',width:80},{field:'targetType',title:'目标类型',width:100},{field:'targetId',title:'目标ID',width:100},{field:'contentTitle',title:'内容/栏目',minWidth:180},{field:'accessMode',title:'权限模式',width:110},{field:'requiredReadLevel',title:'阅读等级',width:110},{field:'memberGroupIds',title:'会员组',width:140},{field:'price',title:'价格',width:90},{field:'status',title:'状态',width:80},{field:'updateTimeText',title:'更新时间',width:170},{title:'操作',toolbar:'#rowActions',width:100}];
			configs['content.access'].form = function(){return simpleForm([{name:'targetType',label:'目标类型',value:'content',placeholder:'content/category'},{name:'targetId',label:'目标ID',type:'number',placeholder:'内容ID或栏目ID'},{name:'contentId',label:'内容ID',type:'number',placeholder:'targetType=content 时可选'},{name:'categoryId',label:'栏目ID',type:'number',placeholder:'targetType=category 时可选'},{name:'accessMode',label:'权限模式',value:'public',placeholder:'public/login/level/group/password/paid/private'},{name:'requiredReadLevel',label:'阅读等级',type:'number',value:'0'},{name:'memberGroupIds',label:'会员组',placeholder:'group 模式，例如 1,2'},{name:'password',label:'访问密码',placeholder:'password 模式使用'},{name:'price',label:'价格',type:'number',value:'0'},{name:'status',label:'状态',type:'number',value:'1'}]) + '<div class="x-toolbar"><div><div class="x-muted">规则筛选只影响后台表格查询，运行期权限判断仍走内容/栏目规则缓存和固定检查流程。</div><div style="display:flex;gap:8px;align-items:center;flex-wrap:wrap;margin-top:8px"><input id="accessFilterTargetType" class="layui-input" style="width:140px" value="'+esc(abilityListFilters.accessTargetType || '')+'" placeholder="content/category"><input id="accessFilterTargetId" type="number" class="layui-input" style="width:130px" value="'+esc(abilityListFilters.accessTargetId || '')+'" placeholder="目标ID"><input id="accessFilterMode" class="layui-input" style="width:150px" value="'+esc(abilityListFilters.accessMode || '')+'" placeholder="权限模式"><input id="accessFilterStatus" type="number" class="layui-input" style="width:110px" value="'+esc(abilityListFilters.accessStatus || '')+'" placeholder="状态"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnAccessFilter"><i class="layui-icon layui-icon-search"></i> 筛选规则</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnAccessFilterClear"><i class="layui-icon layui-icon-close"></i> 清空</button></div></div></div>';};
			var accessStatsBaseForm = configs['content.access'].form;
			configs['content.access'].form = function(){return accessStatsBaseForm() + '<div class="x-toolbar"><div><div class="x-muted">权限统计只读取 content_access_rule 聚合结果，不改变运行期权限判断。</div></div><div class="layui-btn-container"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnAccessStats"><i class="layui-icon layui-icon-chart"></i> 权限统计</button></div></div>';};
		}
		if(configs['content.audit-log']){
			configs['content.audit-log'].ops = 'detail';
			configs['content.audit-log'].listQuery = function(){return queryFromPairs([['targetType', abilityListFilters.auditTargetType], ['targetId', abilityListFilters.auditTargetId], ['action', abilityListFilters.auditAction]]);};
			configs['content.audit-log'].cols = [{field:'id',title:'ID',width:80},{field:'targetType',title:'对象类型',width:120},{field:'targetId',title:'对象ID',width:100},{field:'action',title:'动作',width:150},{field:'summary',title:'摘要',minWidth:200},{field:'operatorType',title:'操作者类型',width:120},{field:'operatorId',title:'操作者ID',width:110},{field:'ip',title:'IP',width:130},{field:'createTimeText',title:'时间',width:170},{title:'操作',toolbar:'#rowActions',width:90}];
			configs['content.audit-log'].form = function(){return '<div class="x-toolbar"><div><div class="x-muted">审计日志由内容保存、删除、版本恢复、工作流和规则变更自动写入；列表筛选只影响后台表格查询。</div><div style="display:flex;gap:8px;align-items:center;flex-wrap:wrap;margin-top:8px"><input id="auditTargetType" class="layui-input" style="width:140px" value="'+esc(abilityListFilters.auditTargetType || '')+'" placeholder="对象类型"><input id="auditTargetId" type="number" class="layui-input" style="width:130px" value="'+esc(abilityListFilters.auditTargetId || '')+'" placeholder="对象ID"><input id="auditAction" class="layui-input" style="width:170px" value="'+esc(abilityListFilters.auditAction || '')+'" placeholder="动作"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnAuditFilter"><i class="layui-icon layui-icon-search"></i> 筛选日志</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnAuditFilterClear"><i class="layui-icon layui-icon-close"></i> 清空</button></div><input id="auditKeepDays" type="number" min="1" class="layui-input" style="width:180px;margin-top:8px" value="180" placeholder="保留天数"></div><div class="layui-btn-container"><button type="button" class="layui-btn layui-btn-danger layui-btn-sm" id="btnAuditCleanup"><i class="layui-icon layui-icon-delete"></i> 清理日志</button></div></div>';};
			var auditStatsBaseForm = configs['content.audit-log'].form;
			configs['content.audit-log'].form = function(){return auditStatsBaseForm() + '<div class="x-toolbar"><div><div class="x-muted">统计只读取 content_audit_log 聚合结果，不改变审计写入和清理策略。</div></div><div class="layui-btn-container"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnAuditStats"><i class="layui-icon layui-icon-chart"></i> 审计统计</button></div></div>';};
		}
		if(configs['content.redirect']){
			configs['content.redirect'].cols = [{field:'id',title:'ID',width:80},{field:'sourcePath',title:'来源路径',minWidth:220},{field:'targetUrl',title:'目标地址',minWidth:260},{field:'statusCode',title:'状态码',width:90},{field:'hitCount',title:'命中次数',width:90},{field:'lastHitTimeText',title:'最后命中',width:160},{field:'status',title:'启用',width:80},{title:'操作',toolbar:'#rowActions',width:140}];
			configs['content.redirect'].views = {
				main:{listApi:'/redirect/list', ops:'edit,delete', listQuery:function(){return queryFromPairs([['sourcePath', abilityListFilters.redirectSourcePath], ['targetUrl', abilityListFilters.redirectTargetUrl], ['status', abilityListFilters.redirectStatus]]);}, cols:configs['content.redirect'].cols},
				routeRules:{listApi:'/route-rule/list', ops:'routeRuleEdit,routeRuleToggle,routeRuleSort', listQuery:function(){return queryFromPairs([['packId', abilityListFilters.routeRulePackId], ['sourcePack', abilityListFilters.routeRulePackId], ['ruleType', abilityListFilters.routeRuleType], ['status', abilityListFilters.routeRuleStatus], ['warningOnly', abilityListFilters.routeRuleWarningOnly], ['keyword', abilityListFilters.routeRuleKeyword]]);}, cols:[{field:'id',title:'ID',width:80},{field:'sourcePack',title:'能力包',width:150},{field:'ruleType',title:'规则类型',width:110},{field:'ruleKey',title:'规则键',minWidth:150},{field:'matchPattern',title:'匹配规则',minWidth:240},{field:'targetPath',title:'目标路径',minWidth:180},{field:'priority',title:'优先级',width:90},{field:'status',title:'状态',width:80},{field:'warning',title:'风险提示',minWidth:220},{field:'compileStatus',title:'编译',width:80},{field:'compileMessage',title:'编译消息',minWidth:160},{field:'managedFlag',title:'人工',width:80},{field:'updateTimeText',title:'更新时间',width:170},{title:'操作',toolbar:'#rowActions',width:220}]}
			};
			configs['content.redirect'].form = function(){return viewSwitch([{key:'main',text:'跳转规则'},{key:'routeRules',text:'URL 规则'}]) + '<div class="x-toolbar"><div><div class="x-muted">跳转规则仅在 content.redirect 启用时加载；URL 规则视图读取 slug/redirect/static 的统一快照，只在管理期同步，不改变请求热路径。</div><div style="display:flex;gap:8px;margin-top:8px;max-width:860px;flex-wrap:wrap"><input id="redirectSourcePathFilter" class="layui-input" style="width:190px" value="'+esc(abilityListFilters.redirectSourcePath || '')+'" placeholder="来源路径"><input id="redirectTargetUrlFilter" class="layui-input" style="width:210px" value="'+esc(abilityListFilters.redirectTargetUrl || '')+'" placeholder="目标地址"><input id="redirectStatusFilter" class="layui-input" style="width:110px" value="'+esc(abilityListFilters.redirectStatus || '')+'" placeholder="启用 1/0"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnRedirectFilter"><i class="layui-icon layui-icon-search"></i> 筛选跳转</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnRedirectFilterClear"><i class="layui-icon layui-icon-close"></i> 清空跳转</button></div><div style="display:flex;gap:8px;margin-top:8px;max-width:900px;flex-wrap:wrap"><input id="routeRulePackId" class="layui-input" style="width:170px" value="'+esc(abilityListFilters.routeRulePackId || '')+'" placeholder="能力包ID"><input id="routeRuleType" class="layui-input" style="width:150px" value="'+esc(abilityListFilters.routeRuleType || '')+'" placeholder="规则类型"><input id="routeRuleStatus" class="layui-input" style="width:120px" value="'+esc(abilityListFilters.routeRuleStatus || '')+'" placeholder="状态"><input id="routeRuleKeyword" class="layui-input" style="width:180px" value="'+esc(abilityListFilters.routeRuleKeyword || '')+'" placeholder="规则关键词"></div><textarea id="redirectImportJson" class="layui-textarea" style="width:520px;margin-top:8px" placeholder=\'[{\"sourcePath\":\"/old\",\"targetUrl\":\"/new\",\"statusCode\":301}]\'></textarea><pre id="redirectImportResult" class="x-code" style="margin-top:8px"></pre></div><div class="layui-btn-container"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnRouteRuleFilter"><i class="layui-icon layui-icon-search"></i> 筛选 URL 规则</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnRouteRuleFilterClear"><i class="layui-icon layui-icon-close"></i> 清空 URL 筛选</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnRouteRuleRefresh"><i class="layui-icon layui-icon-refresh"></i> 刷新快照</button><button type="button" class="layui-btn layui-btn-sm" id="btnAddRouteRule"><i class="layui-icon layui-icon-add-1"></i> 新增 URL 规则</button><button type="button" class="layui-btn layui-btn-sm" id="btnAddRedirect"><i class="layui-icon layui-icon-add-1"></i> 新增跳转</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnRedirectRuleExplain"><i class="layui-icon layui-icon-read"></i> 规则说明</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnRedirectUnifiedRuleExplain"><i class="layui-icon layui-icon-template"></i> 统一计划</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnRedirectUnifiedRuleValidate"><i class="layui-icon layui-icon-vercode"></i> 批量验证</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnRedirectPreview"><i class="layui-icon layui-icon-list"></i> 预览导入</button><button type="button" class="layui-btn layui-btn-normal layui-btn-sm" id="btnRedirectImport"><i class="layui-icon layui-icon-upload"></i> 确认导入</button></div></div>';};
		}
		if(configs['content.redirect']){
			var redirectRuleBaseForm = configs['content.redirect'].form;
			configs['content.redirect'].form = function(){
				return redirectRuleBaseForm() + '<div class="x-toolbar"><div><div class="x-muted">URL 规则统计只读取管理期快照表，展示规则总量、warning 数和按能力包/规则类型分布。风险筛选输入 1 时只显示带 warning 的规则。</div><input id="routeRuleWarningOnly" class="layui-input" style="width:160px;margin-top:8px" value="'+esc(abilityListFilters.routeRuleWarningOnly || '')+'" placeholder="仅风险规则 1/0"></div><div class="layui-btn-container"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnRouteRuleStats"><i class="layui-icon layui-icon-chart"></i> URL 规则统计</button></div></div>';
			};
		}
		if(configs['content.media']){
			configs['content.media'].views = {
				main:{listApi:'/media/list', ops:'edit,delete', listQuery:function(){return queryFromPairs([['mime', abilityListFilters.mediaMime], ['status', abilityListFilters.mediaStatus]]);}, cols:[{field:'id',title:'ID',width:80},{field:'attachmentXid',title:'附件XID',width:150},{field:'title',title:'标题',minWidth:160},{field:'url',title:'URL',minWidth:260},{field:'mime',title:'MIME',width:130},{field:'size',title:'大小',width:100},{field:'width',title:'宽度',width:80},{field:'height',title:'高度',width:80},{field:'refCount',title:'引用数',width:90},{field:'status',title:'启用',width:90},{field:'updateTimeText',title:'更新时间',width:170},{title:'操作',toolbar:'#rowActions',width:140}]},
				refs:{listApi:'/media/ref/list', ops:'', cols:[{field:'id',title:'ID',width:80},{field:'mediaId',title:'媒体ID',width:100},{field:'mediaTitle',title:'媒体标题',minWidth:160},{field:'contentId',title:'内容ID',width:110},{field:'contentTitle',title:'内容标题',minWidth:180},{field:'refType',title:'引用类型',width:100},{field:'createTimeText',title:'绑定时间',width:170}]}
			};
			configs['content.media'].form = function(){return viewSwitch([{key:'main',text:'媒体资源'},{key:'refs',text:'引用关系'}]) + '<div class="x-toolbar"><div><div class="x-muted">媒体引用会在内容保存和导入时同步，已被引用的媒体不可删除。文件上传仍走系统附件模块，这里登记返回的 xid 和 URL。</div><div style="display:flex;gap:8px;align-items:center;flex-wrap:wrap;margin-top:8px"><input id="mediaFilterMime" class="layui-input" style="width:160px" value="'+esc(abilityListFilters.mediaMime || '')+'" placeholder="MIME"><input id="mediaFilterStatus" type="number" class="layui-input" style="width:110px" value="'+esc(abilityListFilters.mediaStatus || '')+'" placeholder="状态"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnMediaFilter"><i class="layui-icon layui-icon-search"></i> 筛选资源</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnMediaFilterClear"><i class="layui-icon layui-icon-close"></i> 清空</button></div><input id="mediaBatchIds" class="layui-input" style="width:320px;margin-top:8px" placeholder="ID 列表，逗号分隔"></div><div class="layui-btn-container"><a class="layui-btn layui-btn-primary layui-btn-sm" target="_blank" href="/admin/view/attachment/upload"><i class="layui-icon layui-icon-upload"></i> 上传附件</a><button type="button" class="layui-btn layui-btn-sm" id="btnAddMedia"><i class="layui-icon layui-icon-add-1"></i> 新增资源</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnMediaBatchEnable">批量启用</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnMediaBatchDisable">批量停用</button><button type="button" class="layui-btn layui-btn-danger layui-btn-sm" id="btnMediaBatchDelete">批量删除</button></div></div>';};
		}
		if(configs['content.revision']){
			configs['content.revision'].ops = 'diff,restore';
			configs['content.revision'].cols = [{field:'id',title:'ID',width:80},{field:'contentId',title:'内容ID',width:100},{field:'revisionNo',title:'版本号',width:100},{field:'title',title:'标题',minWidth:180},{field:'status',title:'状态',width:90},{field:'isDraft',title:'草稿',width:80},{field:'action',title:'动作',width:100},{field:'createTimeText',title:'创建时间',width:170},{title:'操作',toolbar:'#rowActions',width:100}];
			configs['content.revision'].form = function(){return '<div class="x-toolbar"><div><div class="x-muted">版本快照会在内容写入时创建。使用差异查看对比相邻版本，恢复前会先展示当前内容与目标版本的差异。</div><div style="display:flex;gap:8px;margin-top:8px;max-width:760px"><input id="revisionFilterContentId" class="layui-input" type="number" style="width:140px" value="'+esc(abilityListFilters.revisionContentId || '')+'" placeholder="内容ID"><input id="revisionFilterAction" class="layui-input" style="width:150px" value="'+esc(abilityListFilters.revisionAction || '')+'" placeholder="动作"><input id="revisionFilterStatus" class="layui-input" style="width:120px" value="'+esc(abilityListFilters.revisionStatus || '')+'" placeholder="状态"></div></div><div class="layui-btn-container"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnRevisionFilter"><i class="layui-icon layui-icon-search"></i> 筛选版本</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnRevisionFilterClear"><i class="layui-icon layui-icon-close"></i> 清空筛选</button></div></div>';};
		}
		if(configs['content.revision']){
			var revisionStatsBaseForm = configs['content.revision'].form;
			configs['content.revision'].form = function(){return revisionStatsBaseForm() + '<div class="x-toolbar"><div><div class="x-muted">版本统计只读取 content_revision 聚合结果，不改变快照保留和恢复流程。</div></div><div class="layui-btn-container"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnRevisionStats"><i class="layui-icon layui-icon-chart"></i> 版本统计</button></div></div>';};
		}
		if(configs['content.search']){
			configs['content.search'].cols = [{field:'id',title:'ID',width:80},{field:'title',title:'标题',minWidth:180},{field:'slug',title:'Slug',width:160},{field:'summary',title:'摘要',minWidth:220},{field:'status',title:'状态',width:90},{field:'isDraft',title:'草稿',width:80},{field:'updateTimeText',title:'更新时间',width:170}];
			configs['content.search'].listQuery = function(){return queryFromPairs([['q', abilityListFilters.searchQuery], ['categoryId', abilityListFilters.searchCategoryId]]);};
			configs['content.search'].form = function(){return '<div class="x-toolbar"><div><div class="x-muted">搜索能力启用后会维护独立索引；内容保存、导入和删除会同步索引，也可以按分页参数重建。排序权重由标题、关键词、摘要、正文、完整短语和查询词覆盖度共同控制。</div><div style="display:flex;gap:8px;margin-top:8px;max-width:620px"><input id="searchProbeQuery" class="layui-input" value="'+esc(abilityListFilters.searchQuery || '')+'" placeholder="搜索词"><input id="searchProbeLimit" class="layui-input" type="number" min="1" max="20" style="width:120px" value="10"></div><div style="display:flex;gap:8px;margin-top:8px;max-width:620px"><input id="searchRebuildOffset" class="layui-input" type="number" min="0" style="width:140px" value="0" placeholder="重建起点"><input id="searchRebuildLimit" class="layui-input" type="number" min="1" max="10000" style="width:140px" value="1000" placeholder="重建数量"></div><pre id="searchProbeResult" class="x-code" style="margin-top:8px"></pre></div><div class="layui-btn-container"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnSearchFilter"><i class="layui-icon layui-icon-search"></i> 筛选列表</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnSearchFilterClear"><i class="layui-icon layui-icon-close"></i> 清空筛选</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnSearchProbe"><i class="layui-icon layui-icon-search"></i> 测试搜索</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnSearchExplain"><i class="layui-icon layui-icon-survey"></i> 解释排序</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnSearchStats"><i class="layui-icon layui-icon-chart"></i> 索引统计</button><button type="button" class="layui-btn layui-btn-sm" id="btnSearchRebuild"><i class="layui-icon layui-icon-refresh"></i> 重建索引</button></div></div>';};
		}
		if(configs['content.search']){
			var searchCategoryBaseForm = configs['content.search'].form;
			configs['content.search'].form = function(){
				return searchCategoryBaseForm().replace('id="searchProbeLimit"', 'id="searchCategoryId" class="layui-input" type="number" min="1" style="width:140px" value="'+esc(abilityListFilters.searchCategoryId || '')+'" placeholder="栏目ID"><input id="searchProbeLimit"');
			};
		}
		if(configs['content.sitemap']){
			configs['content.sitemap'].cols = [{field:'contentId',title:'内容ID',width:100},{field:'title',title:'标题',minWidth:180},{field:'loc',title:'URL',minWidth:260},{field:'changefreq',title:'更新频率',width:110},{field:'priority',title:'优先级',width:90},{field:'updateTimeText',title:'更新时间',width:170}];
			configs['content.sitemap'].listQuery = function(){return queryFromPairs([['contentId', abilityListFilters.sitemapContentId], ['status', abilityListFilters.sitemapStatus]]);};
			configs['content.sitemap'].form = function(){return '<div class="x-toolbar"><div><div class="x-muted">站点地图条目可以刷新到 content_sitemap_entry，内容保存、导入和删除时也会同步。设置 content.sitemap.siteUrl 后会输出绝对 URL；大站点通过 sitemap-index.xml 关联分页 sitemap.xml?page=N 分片。</div><div style="display:flex;gap:8px;margin-top:8px;max-width:760px;flex-wrap:wrap"><input id="sitemapContentIdFilter" class="layui-input" style="width:150px" value="'+esc(abilityListFilters.sitemapContentId || '')+'" placeholder="内容ID"><input id="sitemapStatusFilter" class="layui-input" style="width:120px" value="'+esc(abilityListFilters.sitemapStatus || '')+'" placeholder="状态"><input id="sitemapRefreshLimit" class="layui-input" type="number" style="width:140px" value="5000" placeholder="刷新上限"></div></div><div class="layui-btn-container"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnSitemapFilter"><i class="layui-icon layui-icon-search"></i> 筛选条目</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnSitemapFilterClear"><i class="layui-icon layui-icon-close"></i> 清空筛选</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnSitemapStats"><i class="layui-icon layui-icon-chart"></i> 条目统计</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnSitemapRefreshPlan"><i class="layui-icon layui-icon-survey"></i> 刷新计划</button><button type="button" class="layui-btn layui-btn-sm" id="btnSitemapRefresh"><i class="layui-icon layui-icon-refresh"></i> 刷新条目</button><a class="layui-btn layui-btn-primary layui-btn-sm" target="_blank" href="'+pub('/sitemap-index.xml')+'">sitemap-index.xml</a><a class="layui-btn layui-btn-primary layui-btn-sm" target="_blank" href="'+pub('/sitemap.xml')+'">sitemap.xml</a><a class="layui-btn layui-btn-primary layui-btn-sm" target="_blank" href="'+pub('/rss.xml')+'">rss.xml</a><a class="layui-btn layui-btn-primary layui-btn-sm" target="_blank" href="'+pub('/robots.txt')+'">robots.txt</a></div></div>';};
		}
		if(configs['content.related']){
			configs['content.related'].cols = [{field:'id',title:'ID',width:80},{field:'sourceContentId',title:'来源ID',width:110},{field:'sourceTitle',title:'来源标题',minWidth:160},{field:'relatedContentId',title:'关联ID',width:120},{field:'relatedTitle',title:'关联标题',minWidth:160},{field:'relationType',title:'类型',width:100},{field:'weight',title:'权重',width:90},{field:'status',title:'状态',width:80},{field:'updateTimeText',title:'更新时间',width:170},{title:'操作',toolbar:'#rowActions',width:100}];
			configs['content.related'].listQuery = function(){return queryFromPairs([['sourceContentId', abilityListFilters.relatedSourceContentId], ['relatedContentId', abilityListFilters.relatedRelatedContentId], ['relationType', abilityListFilters.relatedRelationType], ['status', abilityListFilters.relatedStatus]]);};
			configs['content.related'].form = function(){return simpleForm([{name:'sourceContentId',label:'来源内容ID',type:'number',placeholder:'1'},{name:'relatedContentId',label:'关联内容ID',type:'number',placeholder:'2'},{name:'relationType',label:'关联类型',value:'manual',placeholder:'manual 或 rule'},{name:'weight',label:'权重',type:'number',value:'0'},{name:'status',label:'状态',type:'number',value:'1'}]) + '<div class="x-toolbar"><div><div class="x-muted">规则重建会按同栏目内容生成 relationType=rule 的关联关系。内容保存和导入会刷新当前内容，发布工作流会按上限刷新同栏目的相邻内容；可通过 content.related.ruleLimit 与 content.related.publishRefreshLimit 控制每次工作量。</div><div style="display:flex;gap:8px;margin-top:8px;max-width:760px"><input id="relatedFilterSourceContentId" class="layui-input" style="width:140px" value="'+esc(abilityListFilters.relatedSourceContentId || '')+'" placeholder="来源内容ID"><input id="relatedFilterRelatedContentId" class="layui-input" style="width:140px" value="'+esc(abilityListFilters.relatedRelatedContentId || '')+'" placeholder="关联内容ID"><input id="relatedFilterRelationType" class="layui-input" style="width:150px" value="'+esc(abilityListFilters.relatedRelationType || '')+'" placeholder="manual/rule"><input id="relatedFilterStatus" class="layui-input" style="width:120px" value="'+esc(abilityListFilters.relatedStatus || '')+'" placeholder="状态"></div><div style="display:flex;gap:8px;margin-top:8px;max-width:420px"><input id="relatedRebuildContentId" class="layui-input" placeholder="内容ID，预览时必填"><input id="relatedRebuildLimit" class="layui-input" type="number" min="1" max="20" style="width:120px" value="5"></div><pre id="relatedRebuildResult" class="x-code" style="margin-top:8px"></pre></div><div class="layui-btn-container"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnRelatedFilter"><i class="layui-icon layui-icon-search"></i> 筛选列表</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnRelatedFilterClear"><i class="layui-icon layui-icon-close"></i> 清空筛选</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnRelatedRulePreview"><i class="layui-icon layui-icon-survey"></i> 预览规则分值</button><button type="button" class="layui-btn layui-btn-sm" id="btnRelatedRebuild"><i class="layui-icon layui-icon-refresh"></i> 重建规则关联</button></div></div>';};
			var relatedStatsBaseForm = configs['content.related'].form;
			configs['content.related'].form = function(){return relatedStatsBaseForm() + '<div class="x-toolbar"><div><div class="x-muted">关联统计只读取 content_related 聚合结果，不改变推荐规则计算。</div></div><div class="layui-btn-container"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnRelatedStats"><i class="layui-icon layui-icon-chart"></i> 关联统计</button></div></div>';};
		}
		if(configs['content.form']){
			configs['content.form'].views = {
				main:{listApi:'/form/list', ops:'delete', cols:[{field:'id',title:'ID',width:80},{field:'contentId',title:'内容ID',width:100},{field:'formKey',title:'表单Key',width:160},{field:'title',title:'标题',minWidth:180},{field:'schemaJson',title:'表单结构',minWidth:260},{field:'status',title:'状态',width:80},{field:'updateTimeText',title:'更新时间',width:170},{title:'操作',toolbar:'#rowActions',width:120}]},
				submissions:{listApi:'/form/submission/list', ops:'process', listQuery:function(){return queryFromPairs([['formId', abilityListFilters.formSubmissionFormId], ['contentId', abilityListFilters.formSubmissionContentId], ['status', abilityListFilters.formSubmissionStatus]]);}, cols:[{field:'id',title:'ID',width:80},{field:'formId',title:'表单ID',width:90},{field:'formKey',title:'表单Key',width:140},{field:'formTitle',title:'表单标题',width:160},{field:'contentId',title:'内容ID',width:100},{field:'dataJson',title:'提交数据',minWidth:300},{field:'ip',title:'IP',width:120},{field:'status',title:'状态',width:80},{field:'createTimeText',title:'提交时间',width:170},{title:'操作',toolbar:'#rowActions',width:110}]},
				notifications:{listApi:'/form/notification/list', ops:'markRead,markUnread,replay', listQuery:function(){return queryFromPairs([['formId', abilityListFilters.formNotificationFormId], ['contentId', abilityListFilters.formNotificationContentId], ['status', abilityListFilters.formNotificationStatus], ['event', abilityListFilters.formNotificationEvent]]);}, cols:[{field:'id',title:'ID',width:80},{field:'submissionId',title:'提交ID',width:120},{field:'formId',title:'表单ID',width:90},{field:'contentId',title:'内容ID',width:100},{field:'event',title:'事件',width:160},{field:'title',title:'标题',width:180},{field:'body',title:'内容',minWidth:260},{field:'status',title:'已读',width:80},{field:'readTimeText',title:'已读时间',width:170},{field:'createTimeText',title:'时间',width:170},{title:'操作',toolbar:'#rowActions',width:150}]}
			};
			configs['content.form'].form = function(){return viewSwitch([{key:'main',text:'表单定义'},{key:'submissions',text:'提交记录'},{key:'notifications',text:'通知记录'}]) + simpleForm([{name:'contentId',label:'内容ID',type:'number',value:'0'},{name:'formKey',label:'表单Key',placeholder:'contact'},{name:'title',label:'标题',placeholder:'联系表单'},{name:'schemaJson',label:'表单结构',type:'textarea',value:'{}',placeholder:'JSON schema，支持 required 或 fields[]'},{name:'status',label:'状态',type:'number',value:'1'}]) + '<div class="x-toolbar"><div><div class="x-muted">提交入库前会校验必填字段、fields[].type 和 fields[].format；提交记录筛选只影响后台表格查询。</div><div style="display:flex;gap:8px;align-items:center;flex-wrap:wrap;margin-top:8px"><input id="formSubmissionFormId" class="layui-input" style="width:130px" value="'+esc(abilityListFilters.formSubmissionFormId || '')+'" placeholder="表单ID"><input id="formSubmissionContentId" class="layui-input" style="width:130px" value="'+esc(abilityListFilters.formSubmissionContentId || '')+'" placeholder="内容ID"><input id="formSubmissionStatus" class="layui-input" style="width:120px" value="'+esc(abilityListFilters.formSubmissionStatus || '')+'" placeholder="状态"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnFormSubmissionFilter"><i class="layui-icon layui-icon-search"></i> 筛选提交</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnFormSubmissionFilterClear"><i class="layui-icon layui-icon-close"></i> 清空</button></div></div></div><div class="x-toolbar"><div><div class="x-muted">通知记录筛选只影响后台表格查询。</div><div style="display:flex;gap:8px;align-items:center;flex-wrap:wrap;margin-top:8px"><input id="formNotificationFormId" class="layui-input" style="width:130px" value="'+esc(abilityListFilters.formNotificationFormId || '')+'" placeholder="表单ID"><input id="formNotificationContentId" class="layui-input" style="width:130px" value="'+esc(abilityListFilters.formNotificationContentId || '')+'" placeholder="内容ID"><input id="formNotificationStatus" class="layui-input" style="width:120px" value="'+esc(abilityListFilters.formNotificationStatus || '')+'" placeholder="已读状态"><input id="formNotificationEvent" class="layui-input" style="width:150px" value="'+esc(abilityListFilters.formNotificationEvent || '')+'" placeholder="事件"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnFormNotificationFilter"><i class="layui-icon layui-icon-search"></i> 筛选通知</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnFormNotificationFilterClear"><i class="layui-icon layui-icon-close"></i> 清空</button></div></div></div><div class="x-toolbar"><div><div class="x-muted">设计器按钮会直接编辑表单结构文本框中的 fields[]。</div><div style="display:flex;gap:8px;align-items:center;flex-wrap:wrap;margin-top:8px"><input id="formDesignerName" class="layui-input" style="width:140px" placeholder="字段名"><input id="formDesignerLabel" class="layui-input" style="width:150px" placeholder="显示名称"><select id="formDesignerType" class="layui-select" style="width:120px"><option value="text">文本</option><option value="email">邮箱</option><option value="number">数字</option><option value="textarea">多行文本</option><option value="checkbox">复选框</option><option value="select">下拉选择</option></select><input id="formDesignerPlaceholder" class="layui-input" style="width:150px" placeholder="占位提示"><input id="formDesignerMin" class="layui-input" type="number" style="width:100px" placeholder="最小值"><input id="formDesignerMax" class="layui-input" type="number" style="width:100px" placeholder="最大值"><input id="formDesignerPattern" class="layui-input" style="width:160px" placeholder="正则规则"><input id="formDesignerOptions" class="layui-input" style="width:180px" placeholder="选项，逗号分隔"><input id="formDesignerRequired" type="checkbox" title="必填"></div></div><div class="layui-btn-container"><button type="button" class="layui-btn layui-btn-sm" id="btnFormDesignerAdd"><i class="layui-icon layui-icon-add-1"></i> 添加字段</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnFormDesignerUpdate"><i class="layui-icon layui-icon-edit"></i> 更新字段</button><button type="button" class="layui-btn layui-btn-danger layui-btn-sm" id="btnFormDesignerRemove"><i class="layui-icon layui-icon-delete"></i> 删除字段</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnFormDesignerUp"><i class="layui-icon layui-icon-up"></i> 上移</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnFormDesignerDown"><i class="layui-icon layui-icon-down"></i> 下移</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnFormSchemaFormat"><i class="layui-icon layui-icon-fonts-code"></i> 格式化</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnFormSchemaValidate"><i class="layui-icon layui-icon-ok"></i> 校验结构</button><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnFormExport"><i class="layui-icon layui-icon-export"></i> 导出提交</button></div></div>';};
		}
		if(configs['content.form']){
			var formBaseForm = configs['content.form'].form;
			configs['content.form'].form = function(){
				return formBaseForm() + '<div class="x-toolbar"><div><div class="x-muted">表单通知统计观察本地通知收件箱和后台投递任务，公开提交不会执行请求期外部 HTTP。</div></div><div class="layui-btn-container"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnFormNotificationStats"><i class="layui-icon layui-icon-chart"></i> 通知统计</button></div></div>';
			};
			var formSubmissionStatsBaseForm = configs['content.form'].form;
			configs['content.form'].form = function(){
				return formSubmissionStatsBaseForm() + '<div class="x-toolbar"><div><div class="x-muted">提交统计只读取 content_form_submission 聚合结果，不改变公开提交和处理流程。</div></div><div class="layui-btn-container"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnFormSubmissionStats"><i class="layui-icon layui-icon-chart"></i> 提交统计</button></div></div>';
			};
			var formDesignerPreviewBaseForm = configs['content.form'].form;
			configs['content.form'].form = function(){
				return formDesignerPreviewBaseForm() + '<div class="x-toolbar"><div><div class="x-muted">字段预览从当前 schemaJson 直接解析，选择一行会回填到设计器输入框，便于继续编辑。</div><div id="formDesignerPreview" style="margin-top:8px"></div></div><div class="layui-btn-container"><button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="btnFormDesignerPreview"><i class="layui-icon layui-icon-list"></i> 刷新字段预览</button></div></div>';
			};
		}
		function startAbilityPage(){
			layui.use(['table','form','layer'],function(){
				layui.table.on('tool(' + tableFilter + ')', function(obj){
					if(obj.event === 'approve') post(api('/comment/status'), {id:obj.data.id,status:1}).then(function(ret){showRowActionResult(ret, '评论审核结果', obj.data, '通过');message(ret);reload();});
					if(obj.event === 'reject') post(api('/comment/status'), {id:obj.data.id,status:2}).then(function(ret){showRowActionResult(ret, '评论审核结果', obj.data, '驳回');message(ret);reload();});
					if(obj.event === 'hide') post(pub('/comment/hide'), {id:obj.data.id}).then(function(ret){showRowActionResult(ret, '评论隐藏结果', obj.data, '隐藏');message(ret);reload();});
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
					if(obj.event === 'detail'){
						if(packId === 'content.audit-log') showAuditLogDetail(obj.data);
					}
					if(obj.event === 'process'){
						if(packId === 'content.form') markFormSubmissionProcessed(obj.data);
					}
					if(obj.event === 'markRead'){
						if(packId === 'content.form') markFormNotificationStatus(obj.data, 1);
					}
					if(obj.event === 'markUnread'){
						if(packId === 'content.form') markFormNotificationStatus(obj.data, 0);
					}
					if(obj.event === 'download'){
						if(packId === 'content.import-export') window.open(api('/import-export/export/download?id=' + encodeURIComponent(obj.data.id)), '_blank');
					}
					if(obj.event === 'retry'){
						if(packId === 'content.static') post(api('/static/task/retry'), {id:obj.data.id}).then(function(ret){showRowActionResult(ret, '静态任务重试结果', obj.data, '重试');message(ret);reload();});
					}
					if(obj.event === 'replay'){
						if(packId === 'content.form') replayFormNotification(obj.data);
					}
					if(obj.event === 'routeRuleEdit'){
						openRouteRuleDialog(obj.data);
					}
					if(obj.event === 'routeRuleToggle'){
						post(api('/route-rule/status'), {id:obj.data.id,status:obj.data.status ? 0 : 1}).then(function(ret){showRowActionResult(ret, 'URL 规则启停结果', obj.data, obj.data.status ? '停用' : '启用');message(ret);reload();});
					}
					if(obj.event === 'routeRuleSortUp' || obj.event === 'routeRuleSortDown'){
						var delta = obj.event === 'routeRuleSortUp' ? -1 : 1;
						var nextPriority = (parseInt(obj.data.priority || 0, 10) || 0) + delta;
						post(api('/route-rule/sort'), {items:[{id:obj.data.id,priority:nextPriority}]}).then(function(ret){showRowActionResult(ret, 'URL 规则排序结果', obj.data, delta < 0 ? '上移' : '下移');message(ret);reload();});
					}
					if(obj.event === 'delete'){
						var cfg = currentConfig();
						var delApi = packId === 'content.comment' ? '/comment/delete' : packId === 'content.tag' ? '/tag/delete' : packId === 'content.topic' ? '/topic/delete' : packId === 'content.sensitive' ? '/sensitive/word/delete' : packId === 'content.static' ? '/static/rule/delete' : packId === 'content.seo' ? '/seo/delete' : packId === 'content.redirect' ? '/redirect/delete' : packId === 'content.media' ? '/media/delete' : packId === 'content.related' ? '/related/delete' : packId === 'content.form' ? '/form/delete' : packId === 'content.access' ? '/access/rule/delete' : '';
						if(delApi) post(api(delApi), {id:obj.data.id}).then(function(ret){showRowActionResult(ret, '删除结果', obj.data, '删除');message(ret);reload();});
					}
					if(obj.event === 'unbind'){
						var unbindApi = packId === 'content.tag' ? '/tag/unbind' : packId === 'content.topic' ? '/topic/unbind' : '';
						if(unbindApi) post(api(unbindApi), {id:obj.data.id}).then(function(ret){showRowActionResult(ret, '解绑结果', obj.data, '解绑');message(ret);reload();});
					}
				});
				byId('btnReload').onclick = reload;
				reload();
				document.addEventListener('click', function(ev){
					if(!root || !root.contains(ev.target)) return;
					var btn = ev.target && ev.target.closest ? ev.target.closest('#btnAddRedirect') : null;
					if(btn) openRedirectDialog();
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnAddRouteRule') : null;
					if(btn) openRouteRuleDialog();
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnAddSeo') : null;
					if(btn) openSeoDialog();
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnSeoPreview') : null;
					if(btn) runSeoPreview();
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnSeoCategoryPreview') : null;
					if(btn) runSeoCategoryPreview();
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnCategoryBindStatus') : null;
					if(btn) showCategoryBindStatus();
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnCategoryBindBackfill') : null;
					if(btn) runCategoryBindBackfill();
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnAddMedia') : null;
					if(btn) openMediaDialog();
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnMediaBatchEnable') : null;
					if(btn) runMediaBatch('enable');
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnMediaBatchDisable') : null;
					if(btn) runMediaBatch('disable');
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnMediaBatchDelete') : null;
					if(btn) runMediaBatch('delete');
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnTagBatchEnable') : null;
					if(btn) runTagBatchStatus(1);
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnTagBatchDisable') : null;
					if(btn) runTagBatchStatus(0);
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnTopicBatchEnable') : null;
					if(btn) runTopicBatchStatus(1);
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnTopicBatchDisable') : null;
					if(btn) runTopicBatchStatus(0);
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnExportJson') : null;
					if(btn) runImportExportExport(false);
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnExportJsonNext') : null;
					if(btn) runImportExportExport(true);
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnImportExportFieldPlan') : null;
					if(btn) runImportExportFieldPlan();
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnImportExportStats') : null;
					if(btn) showImportExportStats();
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnImportPreview') : null;
					if(btn) post(api('/import-export/import/preview'), {sourceName:'sample-preview',fields:importExportFields(),items:[{}]}).then(function(ret){
						var target = byId('importExportResult');
						if(target) target.innerHTML = renderImportExportImportResult(ret, 'preview');
						message(ret);
						reload();
					});
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnImportManualPreview') : null;
					if(btn) runImportExportCommit(false);
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnImportStage') : null;
					if(btn) runImportStage();
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
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnRedirectRuleExplain') : null;
					if(btn) runRedirectRuleExplain();
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnRedirectUnifiedRuleExplain') : null;
					if(btn) runUnifiedRouteRuleExplain('redirectImportResult');
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnRedirectUnifiedRuleValidate') : null;
					if(btn) runUnifiedRouteRuleValidate('redirectImportResult');
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnRouteRuleStats') : null;
					if(btn) showRouteRuleStats('redirectImportResult');
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnRedirectImport') : null;
					if(btn) runRedirectImport(true);
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnSearchStats') : null;
					if(btn) showSearchStats();
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnSearchProbe') : null;
					if(btn) runSearchProbe();
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnSearchExplain') : null;
					if(btn) runSearchExplain();
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnSearchRebuild') : null;
					if(btn) rebuildSearchIndex();
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnStaticStats') : null;
					if(btn) showStaticStats();
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnSitemapStats') : null;
					if(btn) showSitemapStats();
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnSitemapRefreshPlan') : null;
					if(btn) showSitemapRefreshPlan();
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnSitemapRefresh') : null;
					if(btn) refreshSitemapEntries();
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnRelatedRebuild') : null;
					if(btn) rebuildRelatedRules();
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnRelatedRulePreview') : null;
					if(btn) previewRelatedRules();
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnRelatedStats') : null;
					if(btn) showRelatedStats();
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnAuditStats') : null;
					if(btn) showAuditStats();
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnAccessStats') : null;
					if(btn) showAccessStats();
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnRevisionStats') : null;
					if(btn) showRevisionStats();
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnTagStats') : null;
					if(btn) showTagStats();
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnTopicStats') : null;
					if(btn) showTopicStats();
					btn = ev.target && ev.target.closest ? ev.target.closest('[data-topic-move]') : null;
					if(btn) moveTopicArrange(btn.getAttribute('data-index'), btn.getAttribute('data-topic-move'));
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnLikeStats') : null;
					if(btn) showMetricStats('/like/stats', '点赞统计');
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnViewStats') : null;
					if(btn) showMetricStats('/view/stats', '访问统计');
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnFormExport') : null;
					if(btn) exportFormSubmissions();
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnFormSubmissionStats') : null;
					if(btn) showFormSubmissionStats();
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnFormNotificationStats') : null;
					if(btn) showFormNotificationStats();
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnFormDesignerAdd') : null;
					if(btn) appendFormSchemaField();
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnFormDesignerUpdate') : null;
					if(btn) updateFormSchemaField();
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
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnFormDesignerPreview') : null;
					if(btn) renderFormSchemaDesignerPreview();
					btn = ev.target && ev.target.closest ? ev.target.closest('[data-form-designer-pick]') : null;
					if(btn) fillFormDesignerField(btn.getAttribute('data-form-designer-pick'));
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnWorkflowSubmit') : null;
					if(btn) runWorkflowAction('submit');
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnWorkflowApprove') : null;
					if(btn) runWorkflowAction('approve');
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnWorkflowSchedule') : null;
					if(btn) runWorkflowAction('schedule');
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnWorkflowRunScheduled') : null;
					if(btn) runWorkflowScheduledPublish();
					btn = ev.target && ev.target.closest ? ev.target.closest('#btnWorkflowStats') : null;
					if(btn) showWorkflowStats();
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
