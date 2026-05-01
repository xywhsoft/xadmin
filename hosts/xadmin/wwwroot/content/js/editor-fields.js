function renderFields() {
	var box = $('side_fields');
	if (!contentState.fields.length) {
		box.innerHTML = '<div class="side-item"><div class="side-item-meta">暂无字段</div></div>';
		return;
	}
	box.innerHTML = contentState.fields.map(function(field, index) {
		return '<div class="side-item ' + (contentState.selectedField === index ? 'active' : '') + '" data-index="' + index + '">'
			+ '<div class="side-item-title">' + esc(field.title || field.name) + '</div>'
			+ '<div class="side-item-meta">' + esc(field.name) + ' | ' + esc(fieldStorageType(field)) + ' / ' + esc(fieldComponentType(field)) + '</div>'
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

function fieldComponentType(field) {
	if (field && field.component && field.component.type) return field.component.type;
	return (field && field.type) || 'text';
}

function fieldStorageType(field) {
	if (field && field.storage && field.storage.type) return field.storage.type;
	return defaultStorageType(fieldComponentType(field));
}

function fieldListText(field) {
	if (field && field.component && Array.isArray(field.component.list)) return JSON.stringify(field.component.list, null, 2);
	if (field && Array.isArray(field.list)) return JSON.stringify(field.list, null, 2);
	return '';
}

function optionTextFromField(field) {
	if (field && field.options) return field.options;
	if (field && field.component && Array.isArray(field.component.list)) {
		return field.component.list.map(function(item) {
			return String(item && item.value != null ? item.value : '') + ':' + String(item && item.label != null ? item.label : item && item.value != null ? item.value : '');
		}).filter(function(item) { return item.charAt(0) !== ':'; }).join(', ');
	}
	if (field && Array.isArray(field.list)) {
		return field.list.map(function(item) {
			return String(item && item.value != null ? item.value : '') + ':' + String(item && item.label != null ? item.label : item && item.value != null ? item.value : '');
		}).filter(function(item) { return item.charAt(0) !== ':'; }).join(', ');
	}
	return '';
}

function defaultStorageType(type) {
	type = String(type || '').toLowerCase();
	if (type === 'integer' || type === 'int') return 'integer';
	if (type === 'number' || type === 'num' || type === 'decimal' || type === 'real') return 'real';
	if (type === 'switch' || type === 'bool' || type === 'boolean') return 'boolean';
	if (type === 'date') return 'date';
	if (type === 'datetime') return 'datetime';
	if (type === 'time') return 'time';
	if (type === 'images' || type === 'files' || type === 'checkbox' || type === 'checklist' || type === 'json' || type.indexOf('range') >= 0) return 'json';
	return 'text';
}

function parseJsonField(text, fallback, message) {
	text = String(text || '').trim();
	if (!text) return fallback;
	try {
		return JSON.parse(text);
	} catch (err) {
		alert(message);
		throw err;
	}
}

function cleanProps(props) {
	Object.keys(props).forEach(function(key) {
		if (props[key] === '' || props[key] == null) delete props[key];
	});
	return props;
}

function selectField(index) {
	contentState.selectedField = index;
	var field = contentState.fields[index] || {};
	var props = (field.component && field.component.props) || {};
	var form = document.querySelector('[lay-filter="content_field_form"]');
	form.elements.name.value = field.name || '';
	form.elements.title.value = field.title || '';
	form.elements.type.value = fieldComponentType(field);
	form.elements.storageType.value = fieldStorageType(field);
	form.elements.semanticRole.value = (field.semantic && field.semantic.role) || field.semanticRole || '';
	form.elements.defaultValue.value = field.defaultValue == null ? '' : (typeof field.defaultValue === 'string' ? field.defaultValue : JSON.stringify(field.defaultValue));
	form.elements.placeholder.value = props.placeholder || field.placeholder || '';
	form.elements.layoutSpan.value = field.layoutSpan == null ? '' : String(field.layoutSpan);
	form.elements.options.value = optionTextFromField(field);
	form.elements.listJson.value = fieldListText(field);
	form.elements.required.checked = !!field.required;
	form.elements.nullable.checked = !!field.nullable;
	form.elements.list.checked = field.list !== false;
	form.elements.detail.checked = field.detail !== false;
	form.elements.readonly.checked = !!field.readonly;
	form.elements.disabled.checked = !!field.disabled;
	form.elements.sortable.checked = !!field.sortable;
	form.elements.filterable.checked = !!field.filterable;
	form.elements.searchable.checked = !!field.searchable;
	form.elements.uploadUrl.value = props.uploadUrl || '';
	form.elements.accept.value = props.accept || '';
	form.elements.acceptMime.value = props.acceptMime || '';
	form.elements.buttonText.value = props.buttonText || '';
	var extraProps = {};
	Object.keys(props).forEach(function(key) {
		if (['placeholder','uploadUrl','accept','acceptMime','buttonText'].indexOf(key) < 0) extraProps[key] = props[key];
	});
	form.elements.componentPropsJson.value = Object.keys(extraProps).length ? JSON.stringify(extraProps, null, 2) : '';
	form.elements.description.value = field.description || '';
	renderFieldOptions(optionTextFromField(field));
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
	contentState.fields.push({
		name:'field_' + (contentState.fields.length + 1),
		title:'新字段',
		type:'text',
		storage:{type:'text'},
		component:{type:'text'},
		required:false,
		list:true,
		detail:true,
		showInForm:true,
		showInList:true,
		showInDetail:true
	});
	selectField(contentState.fields.length - 1);
}

function applyField() {
	syncOptionText();
	var data = formData('content_field_form');
	var componentType = data.type || 'text';
	var storageType = data.storageType || defaultStorageType(componentType);
	var componentProps = parseJsonField(data.componentPropsJson, {}, '组件属性 JSON 必须是合法对象');
	var optionList = parseJsonField(data.listJson, null, '选项 JSON 必须是合法数组');
	var defaultValue = data.defaultValue;
	if (!data.name) { alert('字段名不能为空'); return; }
	if (!data.title) data.title = data.name;
	if (!componentProps || typeof componentProps !== 'object' || Array.isArray(componentProps)) { alert('组件属性 JSON 必须是对象'); return; }
	if (optionList && !Array.isArray(optionList)) { alert('选项 JSON 必须是数组'); return; }
	if (data.placeholder) componentProps.placeholder = data.placeholder;
	if (data.uploadUrl) componentProps.uploadUrl = data.uploadUrl;
	if (data.accept) componentProps.accept = data.accept;
	if (data.acceptMime) componentProps.acceptMime = data.acceptMime;
	if (data.buttonText) componentProps.buttonText = data.buttonText;
	if (defaultValue && /^[\[{"]|^-?\d+(\.\d+)?$|^(true|false|null)$/.test(defaultValue.trim())) {
		defaultValue = parseJsonField(defaultValue, defaultValue, '默认值 JSON 必须合法；纯文本默认值不要以 JSON 符号开头');
	}
	var field = {
		name: data.name,
		title: data.title,
		type: componentType,
		storage: {type: storageType},
		component: {type: componentType},
		defaultValue: defaultValue,
		options: data.options,
		required: !!data.required,
		nullable: !!data.nullable,
		list: !!data.list,
		detail: !!data.detail,
		showInForm: true,
		showInList: !!data.list,
		showInDetail: !!data.detail,
		readonly: !!data.readonly,
		disabled: !!data.disabled,
		sortable: !!data.sortable,
		filterable: !!data.filterable,
		searchable: !!data.searchable,
		description: data.description
	};
	componentProps = cleanProps(componentProps);
	if (Object.keys(componentProps).length) field.component.props = componentProps;
	if (optionList) {
		field.component.list = optionList;
	} else {
		var parsedOptions = parseOptionText(data.options);
		if (parsedOptions.length) field.component.list = parsedOptions;
	}
	if (data.semanticRole) field.semantic = {role:data.semanticRole};
	if (data.layoutSpan) field.layoutSpan = Number(data.layoutSpan) || 1;
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
