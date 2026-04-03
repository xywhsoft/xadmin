/**
 * Layui 图标选择器 - 下拉面板形式
 */
var IconPicker = {
	// layui 图标列表
	icons: [
		'layui-icon-heart-fill', 'layui-icon-heart', 'layui-icon-light', 'layui-icon-time',
		'layui-icon-bluetooth', 'layui-icon-at', 'layui-icon-mute', 'layui-icon-mike',
		'layui-icon-key', 'layui-icon-gift', 'layui-icon-email', 'layui-icon-rss',
		'layui-icon-wifi', 'layui-icon-logout', 'layui-icon-android', 'layui-icon-ios',
		'layui-icon-windows', 'layui-icon-transfer', 'layui-icon-service',
		'layui-icon-subtraction', 'layui-icon-addition', 'layui-icon-slider',
		'layui-icon-print', 'layui-icon-export', 'layui-icon-cols',
		'layui-icon-screen-restore', 'layui-icon-screen-full',
		'layui-icon-rate-half', 'layui-icon-rate', 'layui-icon-rate-solid',
		'layui-icon-cellphone', 'layui-icon-vercode', 'layui-icon-login-weibo',
		'layui-icon-login-qq', 'layui-icon-login-wechat',
		'layui-icon-username', 'layui-icon-password', 'layui-icon-refresh-3',
		'layui-icon-auz', 'layui-icon-spread-left', 'layui-icon-shrink-right',
		'layui-icon-snowflake', 'layui-icon-tips', 'layui-icon-note',
		'layui-icon-home', 'layui-icon-senior', 'layui-icon-refresh',
		'layui-icon-refresh-1', 'layui-icon-flag', 'layui-icon-theme',
		'layui-icon-notice', 'layui-icon-website', 'layui-icon-console',
		'layui-icon-face-smile-b', 'layui-icon-face-cry',
		'layui-icon-cart-simple', 'layui-icon-cart',
		'layui-icon-face-surprised', 'layui-icon-face-smile',
		'layui-icon-layer', 'layui-icon-template', 'layui-icon-template-1',
		'layui-icon-fonts-del', 'layui-icon-fonts-code', 'layui-icon-fonts-html',
		'layui-icon-fonts-strong', 'layui-icon-unlink', 'layui-icon-picture',
		'layui-icon-link', 'layui-icon-align-center',
		'layui-icon-align-right', 'layui-icon-align-left',
		'layui-icon-fonts-u', 'layui-icon-fonts-i',
		'layui-icon-tabs', 'layui-icon-radio', 'layui-icon-circle',
		'layui-icon-edit-circle', 'layui-icon-dialogue',
		'layui-icon-fonts-clear', 'layui-icon-survey', 'layui-icon-read',
		'layui-icon-list', 'layui-icon-release', 'layui-icon-ok',
		'layui-icon-help', 'layui-icon-about', 'layui-icon-location',
		'layui-icon-dollar', 'layui-icon-diamond', 'layui-icon-log',
		'layui-icon-user', 'layui-icon-find-fill', 'layui-icon-loading',
		'layui-icon-loading-1', 'layui-icon-add-1', 'layui-icon-play',
		'layui-icon-pause', 'layui-icon-headset', 'layui-icon-video',
		'layui-icon-set', 'layui-icon-set-fill', 'layui-icon-group',
		'layui-icon-share', 'layui-icon-edit', 'layui-icon-delete',
		'layui-icon-close', 'layui-icon-component', 'layui-icon-app',
		'layui-icon-search', 'layui-icon-date', 'layui-icon-prev',
		'layui-icon-next', 'layui-icon-upload-drag', 'layui-icon-upload',
		'layui-icon-download-circle', 'layui-icon-upload-circle',
		'layui-icon-file', 'layui-icon-file-b', 'layui-icon-more',
		'layui-icon-more-vertical', 'layui-icon-left', 'layui-icon-right',
		'layui-icon-up', 'layui-icon-down', 'layui-icon-circle-dot',
		'layui-icon-top', 'layui-icon-close-fill', 'layui-icon-ok-circle',
		'layui-icon-add-circle-fine', 'layui-icon-table', 'layui-icon-chart',
		'layui-icon-chart-screen', 'layui-icon-engine', 'layui-icon-form',
		'layui-icon-triangle-d', 'layui-icon-triangle-r', 'layui-icon-star',
		'layui-icon-star-fill', 'layui-icon-camera', 'layui-icon-camera-fill',
		'layui-icon-fire', 'layui-icon-return', 'layui-icon-404', 'layui-icon-folder',
		'layui-icon-female', 'layui-icon-male', 'layui-icon-code-circle'
	],
	
	// 当前打开的面板
	_panel: null,
	_callback: null,
	_isOpen: false,
	
	// 关闭面板
	close: function() {
		if ( this._panel ) {
			this._panel.remove();
			this._panel = null;
		}
		this._isOpen = false;
	},
	
	// 打开图标选择下拉面板
	open: function(btnElement, callback) {
		var self = this;
		var $ = layui.$;
		
		// 如果已打开则关闭
		if ( this._isOpen ) {
			this.close();
			return;
		}
		
		// 关闭已有面板
		this.close();
		this._callback = callback;
		this._isOpen = true;
		
		// 创建面板内容
		var sHtml = '<div class="icon-picker-panel" style="position: absolute; z-index: 19999; background: #fff; border: 1px solid #d2d2d2; border-radius: 4px; box-shadow: 0 2px 10px rgba(0,0,0,0.15); max-height: 291px; overflow-y: auto; padding: 10px;">';
		sHtml += '<div style="display: flex; flex-wrap: wrap; gap: 5px;">';
		
		// 添加"无"选项
		sHtml += '<div class="icon-picker-item" data-icon="" style="width: 36px; height: 36px; display: flex; align-items: center; justify-content: center; border: 1px solid #e6e6e6; border-radius: 3px; cursor: pointer; font-size: 12px; color: #999;" title="清除图标">×</div>';
		
		for ( var i = 0; i < this.icons.length; i++ ) {
			var sIcon = this.icons[i];
			sHtml += '<div class="icon-picker-item" data-icon="layui-icon ' + sIcon + '" style="width: 36px; height: 36px; display: flex; align-items: center; justify-content: center; border: 1px solid #e6e6e6; border-radius: 3px; cursor: pointer;" title="' + sIcon + '">';
			sHtml += '<i class="layui-icon ' + sIcon + '" style="font-size: 18px;"></i>';
			sHtml += '</div>';
		}
		
		sHtml += '</div></div>';
		
		// 创建面板元素
		var $panel = $(sHtml);
		$('body').append($panel);
		this._panel = $panel;
		
		// 计算位置（在按钮下方，靠左对齐，宽度覆盖整个表单区域）
		var $btn = $(btnElement);
		var $container = $btn.closest('.layui-input-block');
		var containerOffset = $container.offset();
		var containerWidth = $container.outerWidth();
		var btnOffset = $btn.offset();
		var btnHeight = $btn.outerHeight();
		
		$panel.css({
			left: containerOffset.left + 'px',
			top: (btnOffset.top + btnHeight + 5) + 'px',
			width: (containerWidth - 7) + 'px'
		});
		
		// 绑定图标点击事件
		$panel.on('click', '.icon-picker-item', function(e) {
			e.stopPropagation();
			e.preventDefault();
			var sSelectedIcon = $(this).attr('data-icon');
			if ( self._callback ) {
				self._callback(sSelectedIcon);
			}
			self.close();
		});
		
		// 鼠标悬停效果
		$panel.on('mouseenter', '.icon-picker-item', function() {
			$(this).css({'border-color': '#1E9FFF', 'background': '#f0f9ff'});
		}).on('mouseleave', '.icon-picker-item', function() {
			$(this).css({'border-color': '#e6e6e6', 'background': 'transparent'});
		});
		
		// 点击其他地方关闭面板
		setTimeout(function() {
			$(document).on('click.iconpicker', function(e) {
				// 如果点击的不是面板内部，则关闭
				if ( self._panel && !$(e.target).closest('.icon-picker-panel').length && !$(e.target).closest('#btnSelectIcon').length ) {
					self.close();
					$(document).off('click.iconpicker');
				}
			});
		}, 100);
		
		// 阻止面板内点击冒泡
		$panel.on('click', function(e) {
			e.stopPropagation();
		});
	}
};
