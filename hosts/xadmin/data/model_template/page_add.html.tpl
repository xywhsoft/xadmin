<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>添加{{MODEL_TITLE}}</title>
<link rel="stylesheet" href="/pear/component/layui/css/layui.css">
<link rel="stylesheet" href="/pear/admin/css/admin.css">
<link rel="stylesheet" href="/css/custom.css">
</head>
<body class="pear-container">

<form class="layui-form" lay-filter="dataForm" style="padding: 20px;">
	
	{{FORM_FIELDS}}
	
	<div class="layui-form-item" style="text-align: center; padding-top: 20px;">
		<button class="layui-btn" lay-submit lay-filter="submit">
			<i class="layui-icon layui-icon-ok"></i> 提交
		</button>
		<button type="reset" class="layui-btn layui-btn-primary">
			<i class="layui-icon layui-icon-refresh"></i> 重置
		</button>
	</div>
	
</form>

<script src="/pear/component/layui/layui.js"></script>
<script>
layui.use(['form', 'layer'], function() {
	var form = layui.form;
	var layer = layui.layer;
	var $ = layui.$;
	
	// 表单提交
	form.on('submit(submit)', function(data) {
		fetch('{{DATA_API_URL}}', {
			method: 'POST',
			headers: { 'Content-Type': 'application/json' },
			body: JSON.stringify(data.field)
		})
		.then(function(res) { return res.json(); })
		.then(function(res) {
			if ( res.code === 0 ) {
				layer.msg('添加成功', { icon: 1 }, function() {
					// 关闭当前弹窗
					var iIndex = parent.layer.getFrameIndex(window.name);
					parent.layer.close(iIndex);
				});
			} else {
				layer.msg(res.msg || '添加失败', { icon: 2 });
			}
		})
		.catch(function(err) {
			layer.msg('请求失败', { icon: 2 });
		});
		
		return false;
	});
});
</script>

</body>
</html>
