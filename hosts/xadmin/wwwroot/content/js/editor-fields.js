function renderFields() {
	var box = $('side_fields');
	if (!contentState.fields.length) {
		box.innerHTML = '<div class="side-item"><div class="side-item-meta">暂无字段</div></div>';
		return;
	}
	box.innerHTML = contentState.fields.map(function(field, index) {
		return '<div class="side-item ' + (contentState.selectedField === index ? 'active' : '') + '" data-index="' + index + '">'
			+ '<div class="side-item-title">' + esc(field.title || field.name) + '</div>'
			+ '<div class="side-item-meta">' + esc(field.name) + ' | ' + esc(field.type) + '</div>'
			+ '<div class="field-flags">'
			+ (field.required ? '<span class="flag required">必填</span>' : '')
			+ (field.list ? '<span class="flag">列表</span>' : '')
			+ (field.detail ? '<span class="flag">详情</span>' : '')
			+ '</div></div>';
	}).join('');
	Array.prototype.forEach.call(box.querySelectorAll('.side-item'), function(item) {
		item.onclick = function() { selectField(Number(item.getAttribute('data-index'))); };
	});
	updateImpact();
}

function selectField(index) {
	contentState.selectedField = index;
	var field = contentState.fields[index] || {};
	var form = document.querySelector('[lay-filter="content_field_form"]');
	form.elements.name.value = field.name || '';
	form.elements.title.value = field.title || '';
	form.elements.type.value = field.type || 'text';
	form.elements.defaultValue.value = field.defaultValue || '';
	form.elements.options.value = field.options || '';
	form.elements.required.checked = !!field.required;
	form.elements.list.checked = field.list !== false;
	form.elements.detail.checked = field.detail !== false;
	form.elements.description.value = field.description || '';
	renderFieldOptions(field.options || '');
	if (layuiForm) layuiForm.render();
	renderFields();
}

function parseOptionText(text) {
	return String(text || '').split(',').map(function(item) {
		var pair = item.split(':');
		var value = (pair[0] || '').trim();
		var label = (pair.length > 1 ? pair.slice(1).join(':') : pair[0] || '').trim();
		return value ? {value:value, label:label || value} : null;
	}).filter(Boolean);
}

function optionRowsToText() {
	var rows = [];
	Array.prototype.forEach.call(document.querySelectorAll('#field_option_rows .option-row'), function(row) {
		var value = row.querySelector('[data-option="value"]').value.trim();
		var label = row.querySelector('[data-option="label"]').value.trim();
		if (value) rows.push(value + ':' + (label || value));
	});
	return rows.join(', ');
}

function syncOptionText() {
	var form = document.querySelector('[lay-filter="content_field_form"]');
	form.elements.options.value = optionRowsToText();
}

function renderFieldOptions(text) {
	var box = $('field_option_rows');
	var rows = parseOptionText(text);
	if (!rows.length) rows = [{value:'', label:''}];
	box.innerHTML = rows.map(function(row) {
		return '<div class="option-row">'
			+ '<input class="layui-input" data-option="value" placeholder="值" value="' + esc(row.value) + '">'
			+ '<input class="layui-input" data-option="label" placeholder="标签" value="' + esc(row.label) + '">'
			+ '<button type="button" class="layui-btn layui-btn-primary layui-btn-xs option-remove">移除</button>'
			+ '</div>';
	}).join('');
	Array.prototype.forEach.call(box.querySelectorAll('input'), function(input) {
		input.oninput = syncOptionText;
	});
	Array.prototype.forEach.call(box.querySelectorAll('.option-remove'), function(btn) {
		btn.onclick = function() {
			btn.parentNode.parentNode.removeChild(btn.parentNode);
			if (!box.querySelector('.option-row')) renderFieldOptions('');
			syncOptionText();
		};
	});
}

function addFieldOption() {
	var box = $('field_option_rows');
	var div = document.createElement('div');
	div.className = 'option-row';
	div.innerHTML = '<input class="layui-input" data-option="value" placeholder="值">'
		+ '<input class="layui-input" data-option="label" placeholder="标签">'
		+ '<button type="button" class="layui-btn layui-btn-primary layui-btn-xs option-remove">移除</button>';
	box.appendChild(div);
	Array.prototype.forEach.call(div.querySelectorAll('input'), function(input) { input.oninput = syncOptionText; });
	div.querySelector('.option-remove').onclick = function() {
		box.removeChild(div);
		syncOptionText();
	};
}

function newField() {
	contentState.fields.push({name:'field_' + (contentState.fields.length + 1), title:'新字段', type:'text', required:false, list:true, detail:true});
	selectField(contentState.fields.length - 1);
}

function applyField() {
	syncOptionText();
	var data = formData('content_field_form');
	if (!data.name) { alert('字段名不能为空'); return; }
	if (!data.title) data.title = data.name;
	var field = {
		name: data.name,
		title: data.title,
		type: data.type || 'text',
		defaultValue: data.defaultValue,
		options: data.options,
		required: !!data.required,
		list: !!data.list,
		detail: !!data.detail,
		description: data.description
	};
	if (contentState.selectedField < 0) contentState.fields.push(field);
	else contentState.fields[contentState.selectedField] = field;
	renderFields();
}

function deleteField() {
	if (contentState.selectedField < 0) return;
	contentState.fields.splice(contentState.selectedField, 1);
	contentState.selectedField = contentState.fields.length ? 0 : -1;
	if (contentState.selectedField >= 0) selectField(contentState.selectedField);
	else renderFields();
}

function copyField() {
	if (contentState.selectedField < 0) return;
	var source = contentState.fields[contentState.selectedField];
	var copy = {};
	Object.keys(source).forEach(function(key) { copy[key] = source[key]; });
	copy.name = (source.name || 'field') + '_copy';
	copy.title = (source.title || source.name || '字段') + ' 副本';
	contentState.fields.splice(contentState.selectedField + 1, 0, copy);
	selectField(contentState.selectedField + 1);
}

function moveField(offset) {
	var from = contentState.selectedField;
	var to = from + offset;
	if (from < 0 || to < 0 || to >= contentState.fields.length) return;
	var item = contentState.fields[from];
	contentState.fields.splice(from, 1);
	contentState.fields.splice(to, 0, item);
	selectField(to);
}
