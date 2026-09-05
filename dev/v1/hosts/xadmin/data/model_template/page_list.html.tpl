<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{{MODEL_TITLE}} - 列表</title>
<link rel="stylesheet" href="/pear/component/layui/css/layui.css">
<link rel="stylesheet" href="/pear/admin/css/admin.css">
<link rel="stylesheet" href="/css/custom.css">
</head>
<body class="pear-container">

<div class="layui-card">
	<div class="layui-card-body">
		<!-- 搜索栏 -->
		<form class="layui-form" lay-filter="searchForm">
			<div class="layui-form-item">
				{{SEARCH_FIELDS}}
				<div class="layui-inline">
					<button class="layui-btn layui-btn-primary" lay-submit lay-filter="search">
						<i class="layui-icon layui-icon-search"></i> 搜索
					</button>
					<button type="reset" class="layui-btn layui-btn-default">
						<i class="layui-icon layui-icon-refresh"></i> 重置
					</button>
				</div>
			</div>
		</form>
	</div>
</div>

<div class="layui-card">
	<div class="layui-card-body">
		<!-- 工具栏 -->
		<div class="layui-btn-group table-toolbar">
			<button class="layui-btn layui-btn-sm" id="btnAdd">
				<i class="layui-icon layui-icon-add-1"></i> 添加
			</button>
			<button class="layui-btn layui-btn-sm layui-btn-danger" id="btnBatchDel">
				<i class="layui-icon layui-icon-delete"></i> 批量删除
			</button>
		</div>
		
		<!-- 数据表格 -->
		<table id="dataTable" lay-filter="dataTable"></table>
	</div>
</div>

<!-- 行操作模板 -->
<script type="text/html" id="rowTools">
	<div class="layui-btn-group">
		<a class="layui-btn layui-btn-xs" lay-event="edit">编辑</a>
		<a class="layui-btn layui-btn-xs layui-btn-danger" lay-event="del">删除</a>
	</div>
</script>

<script src="/pear/component/layui/layui.js"></script>
<script>
layui.use(['table', 'form', 'layer'], function() {
	var table = layui.table;
	var form = layui.form;
	var layer = layui.layer;
	var $ = layui.$;
	
	// 表格列配置
	var arrCols = [
		{ type: 'checkbox', fixed: 'left' },
		{{TABLE_COLUMNS}}
		{ title: '操作', width: 120, fixed: 'right', toolbar: '#rowTools' }
	];
	
	// 渲染表格
	table.render({
		elem: '#dataTable',
		url: '{{DATA_API_URL}}',
		method: 'get',
		cols: [arrCols],
		page: true,
		limits: [10, 20, 50, 100],
		limit: 20,
		parseData: function(res) {
			return {
				code: res.code,
				msg: res.msg,
				count: res.count,
				data: res.data
			};
		}
	});
	
	// 搜索
	form.on('submit(search)', function(data) {
		table.reload('dataTable', {
			where: data.field,
			page: { curr: 1 }
		});
		return false;
	});
	
	// 添加
	$('#btnAdd').on('click', function() {
		layer.open({
			type: 2,
			title: '添加{{MODEL_TITLE}}',
			content: '{{ADD_PAGE_URL}}',
			area: ['800px', '600px'],
			end: function() {
				table.reload('dataTable');
			}
		});
	});
	
	// 行操作
	table.on('tool(dataTable)', function(obj) {
		var data = obj.data;
		
		if ( obj.event === 'edit' ) {
			layer.open({
				type: 2,
				title: '编辑{{MODEL_TITLE}}',
				content: '{{EDIT_PAGE_URL}}?id=' + data.id,
				area: ['800px', '600px'],
				end: function() {
					table.reload('dataTable');
				}
			});
		} else if ( obj.event === 'del' ) {
			layer.confirm('确定删除此记录吗？', function(index) {
				fetch('{{DATA_API_URL}}', {
					method: 'DELETE',
					headers: { 'Content-Type': 'application/json' },
					body: JSON.stringify({ id: data.id })
				})
				.then(function(res) { return res.json(); })
				.then(function(res) {
					if ( res.code === 0 ) {
						layer.msg('删除成功');
						obj.del();
					} else {
						layer.msg(res.msg || '删除失败');
					}
				});
				layer.close(index);
			});
		}
	});
	
	// 批量删除
	$('#btnBatchDel').on('click', function() {
		var checkStatus = table.checkStatus('dataTable');
		var data = checkStatus.data;
		
		if ( data.length === 0 ) {
			layer.msg('请选择要删除的记录');
			return;
		}
		
		layer.confirm('确定删除选中的 ' + data.length + ' 条记录吗？', function(index) {
			var arrIds = [];
			for ( var i = 0; i < data.length; i++ ) {
				arrIds.push(data[i].id);
			}
			
			fetch('{{DATA_API_URL}}', {
				method: 'DELETE',
				headers: { 'Content-Type': 'application/json' },
				body: JSON.stringify({ ids: arrIds })
			})
			.then(function(res) { return res.json(); })
			.then(function(res) {
				if ( res.code === 0 ) {
					layer.msg('删除成功');
					table.reload('dataTable');
				} else {
					layer.msg(res.msg || '删除失败');
				}
			});
			layer.close(index);
		});
	});
});
</script>

</body>
</html>
