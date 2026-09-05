<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>编辑{{MODEL_TITLE}}</title>
<link rel="stylesheet" href="/pear/component/layui/css/layui.css">
<link rel="stylesheet" href="/pear/admin/css/admin.css">
<link rel="stylesheet" href="/css/custom.css">
</head>
<body class="pear-container">

<form class="layui-form" lay-filter="dataForm" style="padding: 20px;">
	
	<input type="hidden" name="id" id="dataId">
	
	{{FORM_FIELDS}}
	
	<div class="layui-form-item" style="text-align: center; padding-top: 20px;">
		<button class="layui-btn" lay-submit lay-filter="submit">
			<i class="layui-icon layui-icon-ok"></i> 保存
		</button>
		<button type="button" class="layui-btn layui-btn-primary" id="btnCancel">
			<i class="layui-icon layui-icon-close"></i> 取消
		</button>
	</div>
	
</form>

<script src="/pear/component/layui/layui.js"></script>
<script>
layui.use(['form', 'layer'], function() {
	var form = layui.form;
	var layer = layui.layer;
	var $ = layui.$;
	
	// 获取URL参数
	function procGetUrlParam(sName)
	{
		var sUrl = window.location.search;
		var objReg = new RegExp('(^|&)' + sName + '=([^&]*)(&|$)');
		var arrResult = sUrl.substr(1).match(objReg);
		if ( arrResult != null ) {
			return decodeURIComponent(arrResult[2]);
		}
		return null;
	}
	
	// 加载数据
	var iId = procGetUrlParam('id');
	if ( iId ) {
		$('#dataId').val(iId);
		
		fetch('{{DATA_API_URL}}?id=' + iId)
		.then(function(res) { return res.json(); })
		.then(function(res) {
			if ( res.code === 0 && res.data ) {
				form.val('dataForm', res.data);
			} else {
				layer.msg(res.msg || '加载失败', { icon: 2 });
			}
		})
		.catch(function(err) {
			layer.msg('请求失败', { icon: 2 });
		});
	}
	
	// 表单提交
	form.on('submit(submit)', function(data) {
		fetch('{{DATA_API_URL}}', {
			method: 'PUT',
			headers: { 'Content-Type': 'application/json' },
			body: JSON.stringify(data.field)
		})
		.then(function(res) { return res.json(); })
		.then(function(res) {
			if ( res.code === 0 ) {
				layer.msg('保存成功', { icon: 1 }, function() {
					// 关闭当前弹窗
					var iIndex = parent.layer.getFrameIndex(window.name);
					parent.layer.close(iIndex);
				});
			} else {
				layer.msg(res.msg || '保存失败', { icon: 2 });
			}
		})
		.catch(function(err) {
			layer.msg('请求失败', { icon: 2 });
		});
		
		return false;
	});
	
	// 取消按钮
	$('#btnCancel').on('click', function() {
		var iIndex = parent.layer.getFrameIndex(window.name);
		parent.layer.close(iIndex);
	});
});
</script>

</body>
</html>
