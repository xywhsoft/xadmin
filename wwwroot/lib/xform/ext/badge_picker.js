(function(global){
	'use strict';

	if (!global.XForm) {
		return;
	}

	var BADGES = [
		{ value: 'calm', label: 'Calm', color: '#0ea5e9' },
		{ value: 'growth', label: 'Growth', color: '#22c55e' },
		{ value: 'warning', label: 'Warning', color: '#f59e0b' },
		{ value: 'danger', label: 'Danger', color: '#ef4444' },
		{ value: 'night', label: 'Night', color: '#475467' },
		{ value: 'royal', label: 'Royal', color: '#7c3aed' }
	];

	function escapeHtml(value) {
		return String(value == null ? '' : value)
			.replace(/&/g, '&amp;')
			.replace(/</g, '&lt;')
			.replace(/>/g, '&gt;')
			.replace(/"/g, '&quot;')
			.replace(/'/g, '&#39;');
	}

	global.XForm.register('badge_picker', {
		render: function(field, value) {
			var current = value || field.props.defaultBadge || BADGES[0].value;
			var items = BADGES.map(function(item){
				var active = item.value === current ? ' active' : '';
				return '<div class="xform-icon-item' + active + '" data-badge="' + escapeHtml(item.value) + '" style="font-size:12px;font-weight:700;color:' + escapeHtml(item.color) + ';border-color:' + escapeHtml(item.color) + '33;">' + escapeHtml(item.label) + '</div>';
			}).join('');
			return [
				'<div class="layui-form-item" data-xform-field="' + escapeHtml(field.name) + '">',
					'<label class="layui-form-label">' + escapeHtml(field.label) + '</label>',
					'<div class="layui-input-block">',
						'<div class="xform-icon-picker" data-xform-badge-picker="' + escapeHtml(field.name) + '">',
							'<div class="xform-icon-current"><span>Badge:</span><strong>' + escapeHtml(current) + '</strong></div>',
							'<div class="xform-icon-grid">' + items + '</div>',
						'</div>',
						(field.desc ? '<div class="xform-field-desc">' + escapeHtml(field.desc) + '</div>' : ''),
						'<input type="hidden" name="' + escapeHtml(field.name) + '" value="' + escapeHtml(current) + '">',
					'</div>',
				'</div>'
			].join('');
		},
		init: function(instance, field) {
			var wrap = instance.formEl.querySelector('[data-xform-badge-picker="' + String(field.name) + '"]');
			var input = instance.formEl.querySelector('input[name="' + String(field.name) + '"]');
			if (!wrap || !input) {
				return;
			}
			wrap.addEventListener('click', function(evt){
				var item = evt.target.closest('[data-badge]');
				if (!item) {
					return;
				}
				var value = item.getAttribute('data-badge') || '';
				input.value = value;
				Array.prototype.forEach.call(wrap.querySelectorAll('[data-badge]'), function(node){
					node.classList.toggle('active', node === item);
				});
				var current = wrap.querySelector('.xform-icon-current');
				if (current) {
					current.innerHTML = '<span>Badge:</span><strong>' + escapeHtml(value) + '</strong>';
				}
			});
		}
	});
})(window);
