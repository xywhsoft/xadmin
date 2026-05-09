function loadRevisions(xid) {
	if (!xid) return;
	ContentApi.getRevisions(xid).then(function(ret) {
		if (!ret || !ret.result) {
			$('content_revisions').innerText = ret && ret.message ? ret.message : '修订加载失败';
			return;
		}
		var rows = ret && ret.data ? ret.data : [];
		$('content_revisions').innerHTML = rows.length ? rows.slice(0, 5).map(function(row) {
			return '修订 ' + row.revision + (row.note ? ': ' + esc(row.note) : '');
		}).join('<br>') : '暂无修订。';
	});
}

function loadGenerations(xid) {
	if (!xid) return;
	ContentApi.getGenerations(xid).then(function(ret) {
		if (!ret || !ret.result) {
			$('content_generations').innerText = ret && ret.message ? ret.message : '生成记录加载失败';
			return;
		}
		var rows = ret && ret.data ? ret.data : [];
		$('content_generations').innerHTML = rows.length ? rows.slice(0, 5).map(function(row) {
			return esc(row.status) + ' r' + row.targetRevision + ' -> ' + esc(row.pluginXid);
		}).join('<br>') : '暂无生成记录。';
	});
}

function renderAcceptancePaths(items, pluginXid) {
	var el = $('generate_acceptance');
	var rows = (items || []).filter(function(item) {
		return item && item.kind === 'acceptance';
	});
	function fillPath(path) {
		return String(path || '').replace(/\{pluginXid\}/g, pluginXid || '');
	}
	if (!el) return;
	if (!rows.length) {
		el.innerText = '未选择能力包，暂无额外验收路径。';
		return;
	}
	el.innerHTML = rows.map(function(item) {
		var apiPath = fillPath(item.apiPath);
		var viewPath = fillPath(item.viewPath);
		return '<div><strong>' + esc(item.packId || item.field || '-') + '</strong>'
			+ '<br>API: <code>' + esc(apiPath || '-') + '</code>'
			+ '<br>页面: <code>' + esc(viewPath || '-') + '</code></div>';
	}).join('<hr style="border:0;border-top:1px solid #e6e6e6;margin:8px 0;">');
}

function renderAdvisorItems(data, pluginXid) {
	var items = (data && data.items) || [];
	$('content_state').innerText = data && data.status === 'ok' ? '预检通过' : '预检发现 ' + ((data && data.errorCount) || items.length) + ' 个问题';
	$('content_revisions').innerHTML = items.length ? items.map(function(item) {
		return esc(item.level || 'info') + ': ' + esc(item.message || '');
	}).join('<br>') : $('content_revisions').innerHTML;
	renderAcceptancePaths(items, pluginXid);
}

function runPreflightForSpec(spec) {
	var pluginXid = spec.generatedPluginXid || spec.xid || '';
	updateImpact();
	return ContentApi.preflight(spec).then(function(ret) {
		if (!ret || !ret.result) {
			$('content_state').innerText = ret && ret.message ? ret.message : '预检失败';
			return {ok:false, message:ret && ret.message ? ret.message : '预检失败', data:null};
		}
		var data = ret && ret.data ? ret.data : {};
		renderAdvisorItems(data, pluginXid);
		return {ok:data.status === 'ok', message:'', data:data};
	});
}

function saveModel() {
	var spec = buildSpec();
	if (!spec.xid) { alert('XID 不能为空'); return; }
	if (!spec.title) { alert('标题不能为空'); return; }
	ContentApi.saveModel(spec).then(function(ret) {
		if (!ret || !ret.result) { alert(ret && ret.message ? ret.message : '保存失败'); return; }
		contentState.xid = ret.data.xid;
		$('content_title').innerText = spec.title;
		$('content_meta').innerText = '修订 ' + ret.data.revision + ' / 已应用 0';
		$('content_state').innerText = '已保存修订 ' + ret.data.revision;
		history.replaceState(null, '', '/admin/view/content/editor?xid=' + encodeURIComponent(ret.data.xid));
		loadRevisions(ret.data.xid);
		loadGenerations(ret.data.xid);
	});
}

function preflight() {
	var spec = buildSpec();
	runPreflightForSpec(spec);
}

function generatePlugin() {
	var spec = buildSpec();
	if (!spec.xid) { alert('请先保存模型'); return; }
	runPreflightForSpec(spec).then(function(preflightRet) {
		var items = preflightRet && preflightRet.data && preflightRet.data.items ? preflightRet.data.items : [];
		var warnings = items.filter(function(item) { return item && item.level === 'warning'; });
		if (!preflightRet || !preflightRet.ok) {
			alert(preflightRet && preflightRet.message ? preflightRet.message : '预检未通过，已停止生成');
			return;
		}
		if (warnings.length && !confirm('预检包含 ' + warnings.length + ' 条风险提示，确认继续生成？')) {
			$('content_state').innerText = '已取消生成';
			return;
		}
		ContentApi.generate(spec.xid).then(function(ret) {
			if (!ret || !ret.result) { alert(ret && ret.message ? ret.message : '生成失败'); return; }
			$('content_state').innerText = '已生成插件 ' + ret.data.pluginXid + '，修订 ' + ret.data.revision;
			loadGenerations(spec.xid);
		});
	});
}
