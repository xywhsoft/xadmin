<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="renderer" content="webkit">
<meta http-equiv="X-UA-Compatible" content="IE=edge,chrome=1">
<meta name="viewport" content="width=device-width, initial-scale=1">
<link href="/layui/css/layui.css" rel="stylesheet">
<script src="/layui/layui.js"></script>
</head>
<body style="height: 100vh; display: flex; flex-direction: column; overflow: hidden; background: #fff;">
<div class="layui-form managed-content-editor-layout" lay-filter="FormHost_{{PLUGIN_DOM_ID_BASE}}">
  <div class="managed-content-editor-body">
    <div id="Form_{{PLUGIN_DOM_ID_BASE}}" class="managed-content-form">
      <div class="managed-content-empty">&#27491;&#22312;&#21152;&#36733;&#34920;&#21333;...</div>
    </div>
  </div>
  <div class="managed-content-editor-footer">
    <div class="layui-input-block managed-content-editor-actions" style="margin-left: 0;">
      <button type="button" class="layui-btn layui-bg-blue" id="Save_{{PLUGIN_DOM_ID_BASE}}"><i class="layui-icon layui-icon-ok"></i> &#20445;&#23384;</button>
      <button type="button" class="layui-btn layui-btn-warm" id="SaveDraft_{{PLUGIN_DOM_ID_BASE}}"><i class="layui-icon layui-icon-file-b"></i> &#20445;&#23384;&#33609;&#31295;</button>
      <button type="button" class="layui-btn layui-btn-primary managed-workflow-action" id="WorkflowSubmit_{{PLUGIN_DOM_ID_BASE}}" style="display:none;"><i class="layui-icon layui-icon-upload"></i> &#25552;&#20132;&#23457;&#26680;</button>
      <button type="button" class="layui-btn layui-bg-green managed-workflow-action" id="WorkflowApprove_{{PLUGIN_DOM_ID_BASE}}" style="display:none;"><i class="layui-icon layui-icon-ok-circle"></i> &#21457;&#24067;</button>
      <button type="button" class="layui-btn layui-btn-danger managed-workflow-action" id="WorkflowReject_{{PLUGIN_DOM_ID_BASE}}" style="display:none;"><i class="layui-icon layui-icon-close-fill"></i> &#39539;&#22238;</button>
      <button type="button" class="layui-btn layui-btn-primary managed-workflow-action" id="WorkflowOffline_{{PLUGIN_DOM_ID_BASE}}" style="display:none;"><i class="layui-icon layui-icon-down"></i> &#19979;&#32447;</button>
      <button type="button" class="layui-btn layui-btn-primary" id="RevisionList_{{PLUGIN_DOM_ID_BASE}}" style="display:none;"><i class="layui-icon layui-icon-list"></i> &#29256;&#26412;</button>
      <button type="button" class="layui-btn layui-btn-primary" id="Cancel_{{PLUGIN_DOM_ID_BASE}}"><i class="layui-icon layui-icon-close"></i> &#21462;&#28040;</button>
    </div>
    <div class="managed-content-status" id="Status_{{PLUGIN_DOM_ID_BASE}}"></div>
  </div>
</div>

<style>
  .managed-content-editor-layout {
    height: 100%;
    display: flex;
    flex-direction: column;
  }
  .managed-content-editor-body {
    flex: 1 1 auto;
    min-height: 0;
    overflow: auto;
    padding: 18px 22px 12px;
  }
  .managed-content-editor-footer {
    flex: 0 0 auto;
    padding: 10px 20px;
    border-top: 1px solid #e6e6e6;
    background: #fff;
    text-align: right;
    z-index: 20;
  }
  .managed-content-editor-actions {
    display: flex;
    justify-content: flex-end;
    gap: 10px;
  }
  .managed-content-form .xform-group {
    border: 1px solid #eef2f7;
    border-radius: 8px;
    overflow: visible;
    margin-bottom: 16px;
    background: #fff;
  }
  .managed-content-form .xform-group-title {
    padding: 12px 16px;
    font-size: 15px;
    font-weight: 700;
    color: #243047;
    background: #f8fbff;
    border-bottom: 1px solid #eef2f7;
  }
  .managed-content-form .xform-group-desc { padding: 0 16px 12px; color: #667085; font-size: 13px; }
  .managed-content-form .xform-group-fields { padding: 16px 16px 1px; }
  .managed-content-form .xform-group-fields.xform-layout-two-column {
    display: grid;
    grid-template-columns: repeat(2, minmax(0, 1fr));
    column-gap: 16px;
  }
  .managed-content-form .xform-group-fields.xform-layout-two-column .xform-field-span-2 { grid-column: 1 / -1; }
  .managed-content-empty { padding: 42px 20px; color: #667085; text-align: center; }
  .managed-content-status { min-height: 0; color: #1677ff; font-size: 13px; text-align: right; }
  .managed-content-status:not(:empty) { margin-top: 8px; min-height: 18px; }
  @media (max-width: 900px) {
    .managed-content-form .xform-group-fields.xform-layout-two-column { grid-template-columns: 1fr; }
  }
</style>

<script>
layui.use(['form'], function(){
  var form = layui.form;
  var layer = layui.layer;
  var pluginXid = '{{PLUGIN_XID}}';
  var domBase = '{{PLUGIN_DOM_ID_BASE}}';
  var ids = {
    form: 'Form_' + domBase,
    save: 'Save_' + domBase,
    saveDraft: 'SaveDraft_' + domBase,
    workflowSubmit: 'WorkflowSubmit_' + domBase,
    workflowApprove: 'WorkflowApprove_' + domBase,
    workflowReject: 'WorkflowReject_' + domBase,
    workflowOffline: 'WorkflowOffline_' + domBase,
    revisionList: 'RevisionList_' + domBase,
    cancel: 'Cancel_' + domBase,
    status: 'Status_' + domBase
  };
  /* R5：生成期烘焙的表单 schema（零运行时 spec 解释） */
  var BAKED_FORM_SCHEMA = {{CONTENT_FORM_SCHEMA_JSON}};

  var state = {
    meta: null,
    categories: null,
    tags: null,
    topics: null,
    form: null,
    hasCategory: false,
    hasSeo: false,
    hasSlug: false,
    hasMedia: false,
    hasWorkflow: false,
    hasRevision: false,
    currentId: 0
  };

  function t(key) {
    var dict = {
      loadingForm: '\u6b63\u5728\u52a0\u8f7d\u8868\u5355...',
      xformMissing: 'XForm \u8868\u5355\u8fd0\u884c\u65f6\u52a0\u8f7d\u5931\u8d25',
      metaLoadFailed: '\u8868\u5355\u5143\u6570\u636e\u52a0\u8f7d\u5931\u8d25',
      recordLoadFailed: '\u8bb0\u5f55\u52a0\u8f7d\u5931\u8d25',
      formNotReady: '\u8868\u5355\u5c1a\u672a\u51c6\u5907\u5b8c\u6210',
      validateFailed: '\u6821\u9a8c\u5931\u8d25',
      saving: '\u6b63\u5728\u4fdd\u5b58...',
      saveFailed: '\u4fdd\u5b58\u5931\u8d25',
      saved: '\u5185\u5bb9\u5df2\u4fdd\u5b58',
      draftSaved: '\u8349\u7a3f\u5df2\u4fdd\u5b58',
      failed: '\u64cd\u4f5c\u5931\u8d25'
    };
    return dict[key] || key;
  }

  function byId(id) {
    return document.getElementById(id);
  }

  function queryValue(name) {
    var params = new URLSearchParams(window.location.search || '');
    return params.get(name) || '';
  }

  function loadStyleOnce(id, href) {
    if (byId(id)) return;
    var link = document.createElement('link');
    link.id = id;
    link.rel = 'stylesheet';
    link.href = href;
    document.head.appendChild(link);
  }

  function loadScriptOnce(id, src) {
    window.__managedContentScriptPromises = window.__managedContentScriptPromises || {};
    if (byId(id)) return window.__managedContentScriptPromises[id] || Promise.resolve();
    var script = document.createElement('script');
    script.id = id;
    script.src = src;
    script.async = true;
    var promise = new Promise(function(resolve, reject){
      script.onload = resolve;
      script.onerror = function(){ reject(new Error('Failed to load ' + src)); };
    });
    window.__managedContentScriptPromises[id] = promise;
    document.head.appendChild(script);
    return promise;
  }

  async function ensureAssets() {
    window.__xformDebug = false;
    loadStyleOnce('managed-content-editormd-css', '/lib/editormd/css/editormd.min.css');
    loadStyleOnce('managed-content-wangeditor-css', '/lib/wangEditor/css/style.css');
    loadStyleOnce('managed-content-codemirror-css', '/lib/CodeMirror/lib/codemirror.css');
    loadStyleOnce('managed-content-codemirror-monokai-css', '/lib/CodeMirror/theme/monokai.css');
    if (!window.XForm || window.XForm.version !== '20260408_01') {
      await loadScriptOnce('managed-content-xform-runtime', '/lib/xform/xform.js?v=20260408_01');
    }
    if (!window.XForm || window.XForm.version !== '20260408_01') throw new Error(t('xformMissing'));
  }

  function textOf(value) {
    return value == null ? '' : String(value);
  }

  function htmlOf(value) {
    return textOf(value).replace(/[&<>"']/g, function(ch){
      return {'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[ch];
    });
  }

  function setStatus(message, color) {
    var el = byId(ids.status);
    if (!el) return;
    el.innerText = message || '';
    el.style.color = color || '#1677ff';
  }

  async function loadMeta() {
    if (state.meta) return state.meta;
    var response = await fetch('/admin/api/plugin/' + pluginXid + '/form-meta', { cache: 'no-store' });
    var result = await response.json();
    if (!result || !result.result) throw new Error((result && result.message) || t('metaLoadFailed'));
    state.meta = result.data || {};
    if (BAKED_FORM_SCHEMA && BAKED_FORM_SCHEMA.groups) {
      state.meta.schema = BAKED_FORM_SCHEMA;
    }
    state.hasCategory = hasMountedPack(state.meta, 'content.category');
    state.hasSeo = hasMountedPack(state.meta, 'content.seo');
    state.hasSlug = hasMountedPack(state.meta, 'content.slug');
    state.hasMedia = hasMountedPack(state.meta, 'content.media');
    state.hasWorkflow = hasMountedPack(state.meta, 'content.workflow');
    state.hasRevision = hasMountedPack(state.meta, 'content.revision');
    renderWorkflowActions();
    renderRevisionAction();
    return state.meta;
  }

  function renderWorkflowActions() {
    ['workflowSubmit', 'workflowApprove', 'workflowReject', 'workflowOffline'].forEach(function(key){
      var el = byId(ids[key]);
      if (el) el.style.display = state.hasWorkflow && state.currentId > 0 ? '' : 'none';
    });
  }

  function renderRevisionAction() {
    var el = byId(ids.revisionList);
    if (el) el.style.display = state.hasRevision && state.currentId > 0 ? '' : 'none';
  }

  function categoryLabel(row) {
    var level = Number((row && row.level) || 0);
    var prefix = '';
    for (var i = 0; i < level; i++) prefix += '\u3000\u3000';
    return prefix + textOf((row && row.title) || '');
  }

  async function loadCategories() {
    if (!state.hasCategory) return [];
    if (state.categories) return state.categories;
    var response = await fetch('/admin/api/plugin/' + pluginXid + '/category/list', { cache: 'no-store' });
    var result = await response.json();
    if (!result || !result.result) {
      state.categories = [];
      return state.categories;
    }
    state.categories = Array.isArray(result.data) ? result.data : [];
    return state.categories;
  }

  function categoryOptions(categories, includeEmpty) {
    var list = [];
    if (includeEmpty) list.push({ value: 0, label: '\u672a\u5f52\u7c7b' });
    (categories || []).forEach(function(row){
      list.push({ value: Number(row.id || 0), label: categoryLabel(row) });
    });
    return list;
  }

  function hasMountedPack(meta, packId) {
    var packs = meta && meta.contracts && meta.contracts.abilityPacks;
    if (!Array.isArray(packs)) return false;
    return packs.some(function(pack){ return pack && pack.packId === packId; });
  }

  function optionList(rows, labelField) {
    return (rows || []).map(function(row){
      return { value: Number(row.id || 0), label: textOf(row[labelField] || row.name || row.title || row.id) };
    }).filter(function(item){ return item.value > 0; });
  }

  async function fetchJson(path) {
    var response = await fetch(path, { cache: 'no-store' });
    return response.json();
  }

  async function postJson(path, data) {
    var response = await fetch(path, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(data || {})
    });
    return response.json();
  }

  async function loadTags(meta) {
    if (!hasMountedPack(meta, 'content.tag')) return [];
    if (state.tags) return state.tags;
    var result = await fetchJson('/api/plugin/' + pluginXid + '/tag/list');
    state.tags = result && result.result && Array.isArray(result.data) ? result.data : [];
    return state.tags;
  }

  async function loadTopics(meta) {
    if (!hasMountedPack(meta, 'content.topic')) return [];
    if (state.topics) return state.topics;
    var result = await fetchJson('/api/plugin/' + pluginXid + '/topic/list');
    state.topics = result && result.result && Array.isArray(result.data) ? result.data : [];
    return state.topics;
  }

  async function loadRelationValues(meta, contentId) {
    var values = {};
    if (!contentId) return values;
    if (hasMountedPack(meta, 'content.tag')) {
      var tagResult = await fetchJson('/admin/api/plugin/' + pluginXid + '/tag/content/list?contentId=' + encodeURIComponent(contentId));
      values.tagIds = tagResult && tagResult.result && Array.isArray(tagResult.data) ? tagResult.data.map(function(row){ return Number(row.tagId || 0); }).filter(Boolean) : [];
    }
    if (hasMountedPack(meta, 'content.topic')) {
      var topicResult = await fetchJson('/admin/api/plugin/' + pluginXid + '/topic/content/list?contentId=' + encodeURIComponent(contentId));
      values.topicIds = topicResult && topicResult.result && Array.isArray(topicResult.data) ? topicResult.data.map(function(row){ return Number(row.topicId || 0); }).filter(Boolean) : [];
    }
    return values;
  }

  function hasSchemaField(schema, name) {
    var groups = (schema && schema.groups) || [];
    for (var i = 0; i < groups.length; i++) {
      var fields = groups[i].fields || [];
      for (var j = 0; j < fields.length; j++) {
        if (fields[j] && fields[j].name === name) return true;
      }
    }
    return false;
  }

  function withManagedSystemFields(schema, categories, meta, tags, topics) {
    var next = JSON.parse(JSON.stringify(schema || { title: 'Content', groups: [] }));
    next.groups = Array.isArray(next.groups) ? next.groups : [];
    var publishGroup = next.groups[0];
    if (state.hasCategory && !hasSchemaField(next, 'categoryId')) {
      next.groups.unshift({
        key: 'category',
        title: '\u53d1\u5e03\u8bbe\u7f6e',
        desc: '',
        fields: [{
          name: 'categoryId',
          type: 'select',
          label: '\u6240\u5c5e\u680f\u76ee',
          required: false,
          list: categoryOptions(categories, true)
        }]
      });
    }
    if (state.hasSlug && !hasSchemaField(next, 'slug')) {
      publishGroup = next.groups[0];
      if (!publishGroup || publishGroup.key !== 'category') {
        publishGroup = { key: 'category', title: '\u53d1\u5e03\u8bbe\u7f6e', desc: '', fields: [] };
        next.groups.unshift(publishGroup);
      }
      publishGroup.fields = Array.isArray(publishGroup.fields) ? publishGroup.fields : [];
      publishGroup.fields.push({
        name: 'slug',
        type: 'text',
        label: 'URL\u6807\u8bc6',
        required: false,
        placeholder: 'my-first-post'
      });
    }
    if (state.hasMedia && !hasSchemaField(next, 'cover_media_id')) {
      publishGroup = next.groups[0];
      if (!publishGroup || publishGroup.key !== 'category') {
        publishGroup = { key: 'category', title: '\u53d1\u5e03\u8bbe\u7f6e', desc: '', fields: [] };
        next.groups.unshift(publishGroup);
      }
      publishGroup.fields = Array.isArray(publishGroup.fields) ? publishGroup.fields : [];
      publishGroup.fields.push({
        name: 'cover_media_id',
        type: 'text',
        label: '\u5c01\u9762\u8d44\u6e90ID',
        required: false,
        placeholder: '1',
        actions: [{ key: 'pickCoverMedia', text: '\u9009\u62e9\u8d44\u6e90' }]
      });
      publishGroup.fields.push({
        name: 'media_ids',
        type: 'text',
        label: '\u6b63\u6587\u8d44\u6e90ID',
        required: false,
        placeholder: '1,2,3',
        actions: [{ key: 'pickBodyMedia', text: '\u9009\u62e9\u8d44\u6e90' }]
      });
    }
    publishGroup = next.groups[0];
    if (publishGroup && publishGroup.key === 'category') {
      publishGroup.fields = Array.isArray(publishGroup.fields) ? publishGroup.fields : [];
      if (hasMountedPack(meta, 'content.tag') && !hasSchemaField(next, 'tagIds')) {
        publishGroup.fields.push({
          name: 'tagIds',
          type: 'checklist',
          label: '\u6807\u7b7e',
          required: false,
          list: optionList(tags, 'name')
        });
        publishGroup.fields.push({
          name: 'tagNames',
          type: 'text',
          label: '\u65b0\u6807\u7b7e',
          required: false,
          placeholder: '\u591a\u4e2a\u6807\u7b7e\u7528\u9017\u53f7\u5206\u9694'
        });
      }
      if (hasMountedPack(meta, 'content.topic') && !hasSchemaField(next, 'topicIds')) {
        publishGroup.fields.push({
          name: 'topicIds',
          type: 'checklist',
          label: '\u4e13\u9898',
          required: false,
          list: optionList(topics, 'title')
        });
      }
    }
    if (state.hasSeo && !hasSchemaField(next, 'seo_title')) {
      next.groups.push({
        key: 'seo',
        title: 'SEO\u4f18\u5316',
        desc: '',
        fields: [
          { name: 'seo_title', type: 'text', label: 'SEO\u6807\u9898', required: false },
          { name: 'seo_keywords', type: 'text', label: 'SEO\u5173\u952e\u8bcd', required: false },
          { name: 'seo_description', type: 'textarea', label: 'SEO\u63cf\u8ff0', required: false }
        ]
      });
    }
    return next;
  }

  function formValuesFromRecord(record, relationValues) {
    var data = record && record.data && typeof record.data === 'object' ? record.data : {};
    var values = JSON.parse(JSON.stringify(data));
    if (state.hasCategory) values.categoryId = Number((record && record.categoryId) || values.categoryId || 0);
    if (relationValues) {
      if (relationValues.tagIds) values.tagIds = relationValues.tagIds;
      if (relationValues.topicIds) values.topicIds = relationValues.topicIds;
    }
    return values;
  }

  async function loadRecord(id) {
    if (!id) return null;
    var response = await fetch('/admin/api/plugin/' + pluginXid + '/get?id=' + encodeURIComponent(id), { cache: 'no-store' });
    var result = await response.json();
    if (!result || !result.result) throw new Error((result && result.message) || t('recordLoadFailed'));
    return result.data || {};
  }

  async function renderEditor() {
    await ensureAssets();
    var meta = await loadMeta();
    var categories = state.hasCategory ? await loadCategories() : [];
    var tags = await loadTags(meta);
    var topics = await loadTopics(meta);
    var record = await loadRecord(state.currentId);
    var relationValues = await loadRelationValues(meta, state.currentId);
    var host = byId(ids.form);
    host.innerHTML = '<div class="managed-content-empty">' + t('loadingForm') + '</div>';
    state.form = await window.XForm.render(host, withManagedSystemFields(meta.schema || { title: 'Content', groups: [] }, categories, meta, tags, topics), formValuesFromRecord(record, relationValues), meta.fieldTypes || {}, { onAction: handleFormAction });
    setStatus('');
  }

  async function handleFormAction(instance, field, action) {
    if (!state.hasMedia || !action) return;
    if (action.key === 'pickCoverMedia') {
      await openMediaPicker(field.name, false);
    } else if (action.key === 'pickBodyMedia') {
      await openMediaPicker(field.name, true);
    }
  }

  async function openMediaPicker(fieldName, multiple) {
    if (!state.form || !fieldName) return;
    var result = await fetchJson('/admin/api/plugin/' + pluginXid + '/media/list');
    if (!result || !result.result) throw new Error((result && result.message) || 'media list failed');
    var rows = Array.isArray(result.data) ? result.data : [];
    var html = '<div style="padding:12px 16px;"><table class="layui-table"><thead><tr><th style="width:80px">ID</th><th>Title</th><th>URL</th><th style="width:100px">Size</th><th style="width:90px">Action</th></tr></thead><tbody>';
    rows.forEach(function(row){
      html += '<tr><td>' + htmlOf(row.id || '') + '</td><td>' + htmlOf(row.title || '') + '</td><td style="word-break:break-all;">' + htmlOf(row.url || '') + '</td><td>' + htmlOf(row.width && row.height ? (row.width + 'x' + row.height) : '') + '</td><td><button type="button" class="layui-btn layui-btn-xs" data-media-id="' + htmlOf(row.id || '') + '">Select</button></td></tr>';
    });
    if (!rows.length) html += '<tr><td colspan="5" style="text-align:center;color:#667085;">No media resources</td></tr>';
    html += '</tbody></table></div>';
    var index = layer.open({ type: 1, title: multiple ? 'Select Media Resources' : 'Select Cover Resource', area: ['860px','560px'], content: html, success: function(layero){
      layero[0].addEventListener('click', function(ev){
        var btn = ev.target.closest('[data-media-id]');
        if (!btn) return;
        var id = btn.getAttribute('data-media-id') || '';
        if (!id) return;
        if (multiple) {
          var data = state.form.collect ? state.form.collect() : {};
          var values = String(data[fieldName] || '').split(',').map(function(item){ return item.trim(); }).filter(Boolean);
          if (values.indexOf(id) < 0) values.push(id);
          state.form.setValue(fieldName, values.join(','));
        } else {
          state.form.setValue(fieldName, id);
          layer.close(index);
        }
      });
    }});
  }

  function closeFrame() {
    if (parent && parent.layer) {
      parent.layer.close(parent.layer.getFrameIndex(window.name));
      return;
    }
    window.close();
  }

  async function saveRecord(asDraft) {
    if (!state.form) { layer.msg(t('formNotReady'), { icon: 2 }); return; }
    var check = state.form.validate ? state.form.validate() : { result: true };
    if (!check.result) { setStatus(check.message || t('validateFailed'), '#b91c1c'); return; }
    var values = state.form.collect ? state.form.collect() : {};
    var categoryId = state.hasCategory ? Number(values.categoryId || 0) : 0;
    var tagIds = Array.isArray(values.tagIds) ? values.tagIds.map(Number).filter(Boolean) : [];
    var topicIds = Array.isArray(values.topicIds) ? values.topicIds.map(Number).filter(Boolean) : [];
    var tagNames = textOf(values.tagNames).split(',').map(function(item){ return item.trim(); }).filter(Boolean);
    delete values.tagIds;
    delete values.topicIds;
    delete values.tagNames;
    setStatus(t('saving'));
    var response = await fetch('/admin/api/plugin/' + pluginXid + '/save', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ id: state.currentId || 0, categoryId: categoryId, isDraft: !!asDraft, data: values })
    });
    var result = await response.json();
    if (!result || !result.result) throw new Error((result && result.message) || t('saveFailed'));
    await saveAbilityRelations(result.id || state.currentId || 0, tagIds, tagNames, topicIds);
    layer.msg(asDraft ? t('draftSaved') : t('saved'), { icon: 1 });
    if (parent && parent.layui && parent.layui.table) {
      parent.layui.table.reloadData('Table_{{PLUGIN_LIST_DOM_ID_BASE}}_Articles');
      parent.layui.table.reloadData('Table_{{PLUGIN_LIST_DOM_ID_BASE}}_Drafts');
    }
    closeFrame();
  }

  async function runWorkflowAction(action) {
    if (!state.hasWorkflow || state.currentId <= 0) return;
    var reason = '';
    if (action === 'reject' || action === 'offline') {
      reason = window.prompt(action === 'reject' ? '\u8bf7\u586b\u5199\u9a73\u56de\u539f\u56e0' : '\u8bf7\u586b\u5199\u4e0b\u7ebf\u539f\u56e0', '') || '';
    }
    setStatus('\u6b63\u5728\u5904\u7406\u5de5\u4f5c\u6d41...');
    var response = await fetch('/admin/api/plugin/' + pluginXid + '/workflow/action', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ id: state.currentId, action: action, reason: reason })
    });
    var result = await response.json();
    if (!result || !result.result) throw new Error((result && result.message) || '\u5de5\u4f5c\u6d41\u64cd\u4f5c\u5931\u8d25');
    layer.msg('\u5de5\u4f5c\u6d41\u5df2\u66f4\u65b0', { icon: 1 });
    setStatus('');
    await renderEditor();
  }

  async function openRevisionList() {
    if (!state.hasRevision || state.currentId <= 0) return;
    var result = await fetchJson('/admin/api/plugin/' + pluginXid + '/revision/list?contentId=' + encodeURIComponent(state.currentId));
    if (!result || !result.result) throw new Error((result && result.message) || '\u7248\u672c\u52a0\u8f7d\u5931\u8d25');
    var rows = Array.isArray(result.data) ? result.data : [];
    var html = '<div style="padding:12px 16px;"><table class="layui-table"><thead><tr><th>ID</th><th>\u7248\u672c</th><th>\u6807\u9898</th><th>\u72b6\u6001</th><th>\u52a8\u4f5c</th><th>\u65f6\u95f4</th><th>\u64cd\u4f5c</th></tr></thead><tbody>';
    rows.forEach(function(row){
      html += '<tr><td>' + htmlOf(row.id) + '</td><td>' + htmlOf(row.revisionNo) + '</td><td>' + htmlOf(row.title) + '</td><td>' + htmlOf(row.status) + '</td><td>' + htmlOf(row.action) + '</td><td>' + htmlOf(row.createTimeText) + '</td><td><button type="button" class="layui-btn layui-btn-primary layui-btn-xs js-rev-diff" data-id="' + htmlOf(row.id) + '">Diff</button><button type="button" class="layui-btn layui-btn-danger layui-btn-xs js-rev-restore" data-id="' + htmlOf(row.id) + '">Restore</button></td></tr>';
    });
    if (!rows.length) html += '<tr><td colspan="7" style="text-align:center;color:#667085;">\u6682\u65e0\u7248\u672c</td></tr>';
    html += '</tbody></table></div>';
    layer.open({
      type:1,
      title:'\u5185\u5bb9\u7248\u672c',
      area:['860px','560px'],
      content:html,
      success:function(layero){
        Array.prototype.forEach.call(layero[0].querySelectorAll('.js-rev-diff'), function(btn){
          btn.onclick = function(){ showEditorRevisionDiff(Number(btn.getAttribute('data-id') || 0)).catch(handleError); };
        });
        Array.prototype.forEach.call(layero[0].querySelectorAll('.js-rev-restore'), function(btn){
          btn.onclick = function(){ showEditorRevisionRestore(Number(btn.getAttribute('data-id') || 0)).catch(handleError); };
        });
      }
    });
  }

  async function showEditorRevisionDiff(id) {
    if (!id) return;
    var ret = await fetchJson('/admin/api/plugin/' + pluginXid + '/revision/diff?id=' + encodeURIComponent(id));
    if (!ret || !ret.result) throw new Error((ret && ret.message) || 'revision diff failed');
    layer.open({type:1,title:'版本差异 #' + id,area:['860px','560px'],content:renderRevisionChangeDialog((ret.data && ret.data.changes) || [], ret.data)});
  }

  function revisionFieldText(row) {
    return String((row && (row.field || row.title)) || '').toLowerCase();
  }

  function revisionValueText(value) {
    if (value && typeof value === 'object') {
      try { return JSON.stringify(value, null, 2); } catch (e) {}
    }
    return String(value == null ? '' : value);
  }

  function isRevisionStructuredValue(value) {
    return value && typeof value === 'object';
  }

  function renderRevisionStructuredValue(value, peer) {
    var valueObj = isRevisionStructuredValue(value);
    var peerObj = isRevisionStructuredValue(peer);
    var keys = [];
    var seen = {};
    if (!valueObj && !peerObj) return '';
    if (Array.isArray(value) || Array.isArray(peer)) {
      return '<pre class="revision-structured-diff" style="white-space:pre-wrap;margin:0;">' + htmlOf(revisionValueText(value)) + '</pre>';
    }
    Object.keys(valueObj ? value : {}).forEach(function(k){ seen[k] = true; keys.push(k); });
    Object.keys(peerObj ? peer : {}).forEach(function(k){ if (!seen[k]) { seen[k] = true; keys.push(k); } });
    if (!keys.length) return '<pre class="revision-structured-diff" style="white-space:pre-wrap;margin:0;">' + htmlOf(revisionValueText(value)) + '</pre>';
    return '<table class="layui-table revision-structured-diff"><thead><tr><th style="width:150px">Key</th><th>Value</th></tr></thead><tbody>' + keys.map(function(k){
      var hasValue = valueObj && Object.prototype.hasOwnProperty.call(value, k);
      var hasPeer = peerObj && Object.prototype.hasOwnProperty.call(peer, k);
      var changed = hasValue && hasPeer && revisionValueText(value[k]) !== revisionValueText(peer[k]);
      var state = !hasPeer ? 'added' : (!hasValue ? 'removed' : (changed ? 'changed' : 'same'));
      return '<tr data-state="' + state + '"><td>' + htmlOf(k) + '</td><td><pre style="white-space:pre-wrap;margin:0;">' + htmlOf(hasValue ? revisionValueText(value[k]) : '') + '</pre></td></tr>';
    }).join('') + '</tbody></table>';
  }

  function isRevisionUrlValue(text) {
    return /^https?:\/\//i.test(text) || text.indexOf('/') === 0 || text.indexOf('plugin-static/') === 0 || text.indexOf('static/') === 0;
  }

  function isRevisionImageValue(text, row) {
    var path = String(text || '').split(/[?#]/)[0].toLowerCase();
    var field = revisionFieldText(row);
    return isRevisionUrlValue(text) && (/\.(png|jpe?g|gif|webp|svg|avif)$/.test(path) || field.indexOf('image') >= 0 || field.indexOf('cover') >= 0);
  }

  function renderRevisionMediaValue(value, peer, row) {
    var text = String(value == null ? '' : value);
    var field = revisionFieldText(row);
    if (!text || !isRevisionUrlValue(text)) return '';
    if (isRevisionImageValue(text, row)) {
      return '<div class="revision-media-diff"><img src="' + htmlOf(text) + '" alt="" style="display:block;max-width:220px;max-height:140px;margin-bottom:6px;border:1px solid #eaecf0;background:#f8fafc;"><pre style="white-space:pre-wrap;margin:0;">' + htmlOf(text) + '</pre></div>';
    }
    if (/url|link|path|file|media|asset|cover/.test(field)) {
      return '<div class="revision-url-diff"><a href="' + htmlOf(text) + '" target="_blank" rel="noopener">打开 URL</a><pre style="white-space:pre-wrap;margin:0;">' + htmlOf(text) + '</pre></div>';
    }
    return '';
  }

  function renderRevisionValue(value, peer, row, resolvedLabel) {
    if (row && resolvedLabel) {
      var labelClass = row.diffMode === 'relation' ? 'revision-relation-diff' : 'revision-enum-diff';
      return '<div class="' + labelClass + '"><pre style="white-space:pre-wrap;margin:0;">' + htmlOf(revisionValueText(value)) + '</pre><div style="color:#667085;font-size:12px;">label: ' + htmlOf(resolvedLabel) + '</div></div>';
    }
    if (row && row.diffMode === 'numeric' && Object.prototype.hasOwnProperty.call(row, 'numberDelta')) {
      return '<div class="revision-number-diff"><pre style="white-space:pre-wrap;margin:0;">' + htmlOf(revisionValueText(value)) + '</pre><div style="color:#667085;font-size:12px;">delta: ' + htmlOf(row.numberDelta) + '</div></div>';
    }
    var structuredHtml = renderRevisionStructuredValue(value, peer);
    if (structuredHtml) return structuredHtml;
    var mediaHtml = renderRevisionMediaValue(value, peer, row);
    if (mediaHtml) return mediaHtml;
    var text = revisionValueText(value);
    var peerText = revisionValueText(peer);
    var isLong = text.length > 160 || peerText.length > 160 || text.indexOf('\n') >= 0 || peerText.indexOf('\n') >= 0;
    if (!isLong) return '<pre style="white-space:pre-wrap;margin:0;">' + htmlOf(text) + '</pre>';
    var lines = text.split(/\r?\n/);
    var peerLines = peerText.split(/\r?\n/);
    var max = Math.min(lines.length, 200);
    var html = '<div class="revision-line-diff">';
    for (var i = 0; i < max; i++) {
      var changed = lines[i] !== peerLines[i];
      html += '<div style="display:flex;gap:8px;background:' + (changed ? '#fff7ed' : 'transparent') + ';border-bottom:1px solid #f2f4f7;"><span style="width:42px;text-align:right;color:#98a2b3;flex:none;">' + (i + 1) + '</span><pre style="white-space:pre-wrap;margin:0;flex:1;">' + htmlOf(lines[i]) + '</pre></div>';
    }
    if (lines.length > max) html += '<div style="color:#667085;font-size:12px;">... ' + htmlOf(lines.length - max) + ' more lines omitted</div>';
    return html + '</div>';
  }

  function renderRevisionChangeDialog(changes, data) {
    var rows = Array.isArray(changes) ? changes : [];
    var html = '<div style="padding:12px 16px;"><div style="color:#667085;margin-bottom:8px;">' + htmlOf((data && data.changeCount) || rows.length || 0) + ' 个字段变更</div><table class="layui-table"><thead><tr><th style="width:160px">字段</th><th>变更前</th><th>变更后</th></tr></thead><tbody>';
    rows.forEach(function(row){
      var summary = (row.changeKind || 'modified') + (row.diffMode ? ' / ' + row.diffMode : '');
      html += '<tr><td><b>' + htmlOf(row.title || row.field || '') + '</b><div style="color:#667085;font-size:12px;">' + htmlOf(row.field || '') + '</div><div style="color:#667085;font-size:12px;">' + htmlOf(summary) + '</div></td><td>' + renderRevisionValue(row.before, row.after, row, row.beforeLabel) + '</td><td>' + renderRevisionValue(row.after, row.before, row, row.afterLabel) + '</td></tr>';
    });
    if (!rows.length) html += '<tr><td colspan="3" style="text-align:center;color:#667085;">没有字段变更</td></tr>';
    return html + '</tbody></table></div>';
  }

  async function showEditorRevisionRestore(id) {
    if (!id) return;
    var ret = await fetchJson('/admin/api/plugin/' + pluginXid + '/revision/restore-preview?id=' + encodeURIComponent(id));
    if (!ret || !ret.result) throw new Error((ret && ret.message) || 'revision restore preview failed');
    layer.confirm(renderRevisionChangeDialog((ret.data && ret.data.changes) || [], ret.data), {title:'恢复版本 #' + id, area:['860px','560px']}, async function(index){
      var restore = await postJson('/admin/api/plugin/' + pluginXid + '/revision/restore', {id:id});
      layer.close(index);
      if (!restore || !restore.result) throw new Error((restore && restore.message) || 'revision restore failed');
      layer.msg('\u7248\u672c\u5df2\u6062\u590d', {icon:1});
      await renderEditor();
    });
  }

  async function saveAbilityRelations(contentId, tagIds, tagNames, topicIds) {
    var meta = await loadMeta();
    if (contentId <= 0) return;
    if (hasMountedPack(meta, 'content.tag')) {
      await fetch('/admin/api/plugin/' + pluginXid + '/tag/bind', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ contentId: contentId, tagIds: tagIds || [], tagNames: tagNames || [] })
      }).then(function(r){ return r.json(); }).then(function(result){
        if (!result || !result.result) throw new Error((result && result.message) || '\u6807\u7b7e\u7ed1\u5b9a\u5931\u8d25');
      });
    }
    if (hasMountedPack(meta, 'content.topic')) {
      await fetch('/admin/api/plugin/' + pluginXid + '/topic/bind-content', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ contentId: contentId, topicIds: topicIds || [] })
      }).then(function(r){ return r.json(); }).then(function(result){
        if (!result || !result.result) throw new Error((result && result.message) || '\u4e13\u9898\u7ed1\u5b9a\u5931\u8d25');
      });
    }
  }

  function handleError(error) {
    var message = error && error.message ? error.message : String(error || t('failed'));
    layer.msg(message, { icon: 2 });
    setStatus(message, '#b91c1c');
  }

  state.currentId = Number(queryValue('id') || 0);
  byId(ids.save).onclick = function(){ saveRecord(false).catch(handleError); };
  byId(ids.saveDraft).onclick = function(){ saveRecord(true).catch(handleError); };
  byId(ids.workflowSubmit).onclick = function(){ runWorkflowAction('submit').catch(handleError); };
  byId(ids.workflowApprove).onclick = function(){ runWorkflowAction('approve').catch(handleError); };
  byId(ids.workflowReject).onclick = function(){ runWorkflowAction('reject').catch(handleError); };
  byId(ids.workflowOffline).onclick = function(){ runWorkflowAction('offline').catch(handleError); };
  byId(ids.revisionList).onclick = function(){ openRevisionList().catch(handleError); };
  byId(ids.cancel).onclick = closeFrame;
  renderEditor().catch(handleError);
});
</script>
</body>
</html>
