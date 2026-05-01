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
	updateImpact();
	ContentApi.preflight(buildSpec()).then(function(ret) {
		if (!ret || !ret.result) {
			$('content_state').innerText = ret && ret.message ? ret.message : '预检失败';
			return;
		}
		var data = ret && ret.data ? ret.data : {};
		var items = data.items || [];
		$('content_state').innerText = data.status === 'ok' ? '预检通过' : '预检发现 ' + (data.errorCount || items.length) + ' 个问题';
		$('content_revisions').innerHTML = items.length ? items.map(function(item) {
			return esc(item.level || 'info') + ': ' + esc(item.message || '');
		}).join('<br>') : $('content_revisions').innerHTML;
	});
}

function generatePlugin() {
	var spec = buildSpec();
	if (!spec.xid) { alert('请先保存模型'); return; }
	ContentApi.generate(spec.xid).then(function(ret) {
		if (!ret || !ret.result) { alert(ret && ret.message ? ret.message : '生成失败'); return; }
		$('content_state').innerText = '已生成插件 ' + ret.data.pluginXid + '，修订 ' + ret.data.revision;
		loadGenerations(spec.xid);
	});
}
