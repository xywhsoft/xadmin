var ContentApi = {
	parseResponse: function(resp) {
		var contentType = resp.headers.get('content-type') || '';
		if (contentType.indexOf('application/json') < 0) {
			return {result:false, message:'会话已过期或响应格式异常', errorCode:'CONTENT_HTTP_ERROR', data:null};
		}
		return resp.json().catch(function() {
			return {result:false, message:'响应 JSON 解析失败', errorCode:'CONTENT_JSON_ERROR', data:null};
		});
	},
	request: function(url, options) {
		return fetch(url, options || {}).then(ContentApi.parseResponse).catch(function() {
			return {result:false, message:'网络请求失败', errorCode:'CONTENT_NETWORK_ERROR', data:null};
		});
	},
	getCapabilities: function() {
		return ContentApi.request('/admin/content/capabilities');
	},
	getModel: function(xid) {
		return ContentApi.request('/admin/content/type?xid=' + encodeURIComponent(xid));
	},
	saveModel: function(spec) {
		return ContentApi.request('/admin/content/save', {method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify(spec)});
	},
	getRevisions: function(xid) {
		return ContentApi.request('/admin/content/revisions?xid=' + encodeURIComponent(xid));
	},
	getGenerations: function(xid) {
		return ContentApi.request('/admin/content/generations?xid=' + encodeURIComponent(xid));
	},
	preflight: function(spec) {
		return ContentApi.request('/admin/content/advisor', {method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify(spec)});
	},
	generate: function(xid) {
		return ContentApi.request('/admin/content/generate', {method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify({xid:xid})});
	}
};
