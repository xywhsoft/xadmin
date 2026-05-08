<!-- Main table -->
<div style="padding: 16px;">
  <table class="layui-hide" id="Table_{{PLUGIN_DOM_ID_BASE}}" lay-filter="Table_{{PLUGIN_DOM_ID_BASE}}"></table>
</div>

<!-- Table toolbar -->
<script type="text/html" id="Toolbar_{{PLUGIN_DOM_ID_BASE}}">
  <div class="layui-inline">
    <input type="text" id="Search_{{PLUGIN_DOM_ID_BASE}}" placeholder="&#25628;&#32034;&#26631;&#39064;&#12289;&#25688;&#35201;&#12289;Slug..." class="layui-input" style="width: 260px; height: 32px; margin-right: 10px;">
  </div>
  <div class="layui-inline" id="CategoryWrap_{{PLUGIN_DOM_ID_BASE}}" style="display:none;">
    <select id="Category_{{PLUGIN_DOM_ID_BASE}}" class="layui-select" style="height:32px; min-width:150px; margin-right:10px;">
      <option value="">&#20840;&#37096;&#26639;&#30446;</option>
    </select>
  </div>
  <div class="layui-inline" id="TagWrap_{{PLUGIN_DOM_ID_BASE}}" style="display:none;">
    <select id="Tag_{{PLUGIN_DOM_ID_BASE}}" class="layui-select" style="height:32px; min-width:140px; margin-right:10px;">
      <option value="">&#20840;&#37096;&#26631;&#31614;</option>
    </select>
  </div>
  <div class="layui-inline" id="TopicWrap_{{PLUGIN_DOM_ID_BASE}}" style="display:none;">
    <select id="Topic_{{PLUGIN_DOM_ID_BASE}}" class="layui-select" style="height:32px; min-width:140px; margin-right:10px;">
      <option value="">&#20840;&#37096;&#19987;&#39064;</option>
    </select>
  </div>
  <div class="layui-inline">
    <button class="layui-btn layui-btn-sm" lay-event="Search_{{PLUGIN_DOM_ID_BASE}}"><i class="layui-icon layui-icon-search"></i> &#25628;&#32034;</button>
    <button class="layui-btn layui-btn-sm layui-btn-primary" lay-event="Refresh_{{PLUGIN_DOM_ID_BASE}}"><i class="layui-icon layui-icon-refresh"></i> &#21047;&#26032;</button>
  </div>
  <div class="layui-inline" style="float: right;">
    <button class="layui-btn layui-btn-sm" lay-event="Add_{{PLUGIN_DOM_ID_BASE}}"><i class="layui-icon layui-icon-addition"></i> &#26032;&#22686;&#20869;&#23481;</button>
  </div>
</script>

<!-- Row tools -->
<script type="text/html" id="Tool_{{PLUGIN_DOM_ID_BASE}}">
  <div class="layui-clear-space">
    <a class="layui-btn layui-btn-xs layui-bg-blue" lay-event="edit">&#32534;&#36753;</a>
    <a class="layui-btn layui-btn-xs layui-bg-orange" lay-event="delete">&#21024;&#38500;</a>
  </div>
</script>

<!-- Page logic -->
<script>
layui.use(['table', 'form'], function(){
  var table = layui.table;
  var form = layui.form;
  var layer = layui.layer;
  var $ = layui.$;
  var pluginXid = '{{PLUGIN_XID}}';
  var domBase = '{{PLUGIN_DOM_ID_BASE}}';
  var pageKind = '{{PLUGIN_PAGE_KIND}}';
  var ids = {
    table: 'Table_' + domBase,
    toolbar: 'Toolbar_' + domBase,
    tool: 'Tool_' + domBase,
    search: 'Search_' + domBase,
    category: 'Category_' + domBase,
    categoryWrap: 'CategoryWrap_' + domBase,
    tag: 'Tag_' + domBase,
    tagWrap: 'TagWrap_' + domBase,
    topic: 'Topic_' + domBase,
    topicWrap: 'TopicWrap_' + domBase
  };
  var state = {
    meta: null,
    categories: null,
    tags: null,
    topics: null,
    categoryMap: {},
    hasCategory: false,
    hasTag: false,
    hasTopic: false,
    hasComment: false,
    hasLike: false,
    hasView: false
  };

  function t(key) {
    var dict = {
      addTitle: '\u65b0\u589e\u5185\u5bb9',
      editTitle: '\u7f16\u8f91\u5185\u5bb9',
      confirmDelete: '\u786e\u5b9a\u5220\u9664 "',
      confirmDeleteEnd: '" \u5417\uff1f',
      deleteConfirm: '\u5220\u9664\u786e\u8ba4',
      deleteFailed: '\u5220\u9664\u5931\u8d25',
      deleted: '\u5df2\u5220\u9664',
      failed: '\u64cd\u4f5c\u5931\u8d25',
      draft: '\u8349\u7a3f',
      published: '\u5df2\u53d1\u5e03',
      title: '\u6807\u9898',
      status: '\u72b6\u6001',
      summary: '\u6458\u8981',
      createTime: '\u521b\u5efa\u65f6\u95f4',
      updateTime: '\u4fee\u6539\u65f6\u95f4',
      operation: '\u64cd\u4f5c'
    };
    return dict[key] || key;
  }

  function byId(id) {
    return document.getElementById(id);
  }

  function textOf(value) {
    return value == null ? '' : String(value);
  }

  function escapeHtml(value) {
    return textOf(value)
      .replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;')
      .replace(/\"/g, '&quot;')
      .replace(/'/g, '&#39;');
  }

  function rowTitle(row) {
    return textOf((row && (row.title || (row.data && row.data.title))) || '-');
  }

  function statusTemplate(row) {
    if (row && row.isDraft) return '<span class="layui-badge layui-bg-orange">' + t('draft') + '</span>';
    return '<span class="layui-badge layui-bg-green">' + t('published') + '</span>';
  }

  function searchValue() {
    var el = byId(ids.search);
    return el ? el.value : '';
  }

  function categoryValue() {
    if (!state.hasCategory) return '';
    var el = byId(ids.category);
    return el ? el.value : '';
  }

  function tagValue() {
    var el = byId(ids.tag);
    return el ? el.value : '';
  }

  function topicValue() {
    var el = byId(ids.topic);
    return el ? el.value : '';
  }

  function reloadTable(resetPage) {
    var options = { where: { q: searchValue(), categoryId: categoryValue(), tagId: tagValue(), topicId: topicValue() } };
    if (resetPage !== false) options.page = { curr: 1 };
    table.reloadData(ids.table, options);
  }

  function categoryLabel(row) {
    var level = Number((row && row.level) || 0);
    var prefix = '';
    for (var i = 0; i < level; i++) prefix += '\u3000\u3000';
    return prefix + textOf((row && row.title) || '');
  }

  async function loadCategories() {
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

  async function loadMeta() {
    if (state.meta) return state.meta;
    var response = await fetch('/admin/api/plugin/' + pluginXid + '/form-meta', { cache: 'no-store' });
    var result = await response.json();
    state.meta = result && result.result ? (result.data || {}) : {};
    state.hasCategory = hasMountedPack(state.meta, 'content.category');
    state.hasTag = hasMountedPack(state.meta, 'content.tag');
    state.hasTopic = hasMountedPack(state.meta, 'content.topic');
    state.hasComment = hasMountedPack(state.meta, 'content.comment');
    state.hasLike = hasMountedPack(state.meta, 'content.like');
    state.hasView = hasMountedPack(state.meta, 'content.view-stat');
    return state.meta;
  }

  function hasMountedPack(meta, packId) {
    var packs = meta && meta.contracts && meta.contracts.abilityPacks;
    if (!Array.isArray(packs)) return false;
    return packs.some(function(pack){ return pack && pack.packId === packId; });
  }

  async function loadTags() {
    if (!state.hasTag) return [];
    if (state.tags) return state.tags;
    var response = await fetch('/api/plugin/' + pluginXid + '/tag/list', { cache: 'no-store' });
    var result = await response.json();
    state.tags = result && result.result && Array.isArray(result.data) ? result.data : [];
    return state.tags;
  }

  async function loadTopics() {
    if (!state.hasTopic) return [];
    if (state.topics) return state.topics;
    var response = await fetch('/api/plugin/' + pluginXid + '/topic/list', { cache: 'no-store' });
    var result = await response.json();
    state.topics = result && result.result && Array.isArray(result.data) ? result.data : [];
    return state.topics;
  }

  function categoryOptions(categories, includeEmpty) {
    var list = [];
    if (includeEmpty) list.push({ value: 0, label: '\u672a\u5f52\u7c7b' });
    (categories || []).forEach(function(row){
      list.push({ value: Number(row.id || 0), label: categoryLabel(row) });
    });
    return list;
  }

  function categoryTitle(id) {
    var key = String(id || 0);
    if (key === '0') return '\u672a\u5f52\u7c7b';
    return state.categoryMap[key] || '-';
  }

  async function renderCategoryFilter() {
    var el = byId(ids.category);
    if (!el) return;
    if (!state.hasCategory) {
      var wrap = byId(ids.categoryWrap);
      if (wrap) wrap.style.display = 'none';
      return;
    }
    var categories = await loadCategories();
    state.categoryMap = {};
    (categories || []).forEach(function(row){
      state.categoryMap[String(row.id || 0)] = categoryLabel(row);
    });
    el.innerHTML = '<option value="">\u5168\u90e8\u680f\u76ee</option>' + categoryOptions(categories, false).map(function(item){
      return '<option value="' + escapeHtml(item.value) + '">' + escapeHtml(item.label) + '</option>';
    }).join('');
    var wrap = byId(ids.categoryWrap);
    if (wrap) wrap.style.display = 'inline-block';
    form.render('select');
  }

  async function renderTaxonomyFilters() {
    var tagWrap = byId(ids.tagWrap);
    var topicWrap = byId(ids.topicWrap);
    var tagEl = byId(ids.tag);
    var topicEl = byId(ids.topic);
    if (state.hasTag && tagWrap && tagEl) {
      tagWrap.style.display = '';
      tagEl.innerHTML = '<option value="">\u5168\u90e8\u6807\u7b7e</option>' + (await loadTags()).map(function(row){
        return '<option value="' + escapeHtml(row.id || 0) + '">' + escapeHtml(row.name || row.id) + '</option>';
      }).join('');
    }
    if (state.hasTopic && topicWrap && topicEl) {
      topicWrap.style.display = '';
      topicEl.innerHTML = '<option value="">\u5168\u90e8\u4e13\u9898</option>' + (await loadTopics()).map(function(row){
        return '<option value="' + escapeHtml(row.id || 0) + '">' + escapeHtml(row.title || row.id) + '</option>';
      }).join('');
    }
    form.render('select');
  }

  function openEditor(id) {
    var currentId = Number(id || 0);
    var url = '/admin/view/plugin/' + pluginXid + '/editor';
    if (currentId > 0) url += '?id=' + encodeURIComponent(currentId);
    layer.open({
      title: currentId > 0 ? t('editTitle') : t('addTitle'),
      type: 2,
      area: ['78%', '86%'],
      shadeClose: false,
      content: url,
      end: function(){ reloadTable(false); }
    });
  }

  function deleteRecord(row) {
    var id = row && row.id;
    if (!id) return;
    layer.confirm(t('confirmDelete') + rowTitle(row) + t('confirmDeleteEnd'), { icon: 3, title: t('deleteConfirm') }, function(index){
      fetch('/admin/api/plugin/' + pluginXid + '/delete', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ id: id })
      }).then(function(response){ return response.json(); }).then(function(result){
        if (!result || !result.result) throw new Error((result && result.message) || t('deleteFailed'));
        layer.msg(t('deleted'), { icon: 1 });
        reloadTable(false);
      }).catch(handleError);
      layer.close(index);
    });
  }

  function handleError(error) {
    var message = error && error.message ? error.message : String(error || t('failed'));
    layer.msg(message, { icon: 2 });
  }

  function tableColumns() {
    var cols = [
      { field: 'id', width: 80, title: 'ID' },
      { title: t('title'), minWidth: 220, templet: function(d){ return '<code>' + escapeHtml(rowTitle(d)) + '</code>'; } },
      { title: t('status'), width: 100, templet: statusTemplate },
      { field: 'summary', minWidth: 180, title: t('summary'), templet: function(d){ return escapeHtml(d.summary || '-'); } },
    ];
    if (state.hasCategory) cols.push({ field: 'categoryId', width: 150, title: '\u680f\u76ee', templet: function(d){ return escapeHtml(categoryTitle(d.categoryId)); } });
    if (state.hasTag) cols.push({ field: 'tagNamesText', minWidth: 140, title: '\u6807\u7b7e', templet: function(d){ return escapeHtml(d.tagNamesText || '-'); } });
    if (state.hasTopic) cols.push({ field: 'topicTitlesText', minWidth: 140, title: '\u4e13\u9898', templet: function(d){ return escapeHtml(d.topicTitlesText || '-'); } });
    if (state.hasComment) cols.push({ field: 'visibleCommentCount', width: 100, title: '\u8bc4\u8bba', templet: function(d){ return escapeHtml(d.visibleCommentCount || 0); } });
    if (state.hasLike) cols.push({ field: 'likeCount', width: 100, title: '\u70b9\u8d5e', templet: function(d){ return escapeHtml(d.likeCount || 0); } });
    if (state.hasView) cols.push({ field: 'viewCount', width: 100, title: '\u8bbf\u95ee', templet: function(d){ return escapeHtml(d.viewCount || 0); } });
    cols = cols.concat([
      { field: 'createTimeText', width: 170, title: t('createTime') },
      { field: 'updateTimeText', width: 170, title: t('updateTime') },
      { width: 140, title: t('operation'), fixed: 'right', templet: '#' + ids.tool }
    ]);
    return [cols];
  }

  function renderTable() {
    table.render({
      elem: '#' + ids.table,
      toolbar: '#' + ids.toolbar,
      url: '/admin/api/plugin/' + pluginXid + (pageKind === 'drafts' ? '/drafts' : '/list'),
      method: 'get',
      where: { q: '', categoryId: '', tagId: '', topicId: '' },
      request: { pageName: 'page', limitName: 'limit' },
      parseData: function(result){
        return {
          code: result && result.result ? 0 : 1,
          msg: (result && result.message) || '',
          count: (result && result.count) || 0,
          data: (result && result.data) || []
        };
      },
      defaultToolbar: ['filter', 'exports', 'print'],
      height: 'full-135',
      cellMinWidth: 80,
      skin: 'line',
      page: {
        limit: 20,
        groups: 11,
        layout: ['count', 'prev', 'page', 'next', 'limit', 'refresh', 'skip']
      },
      cols: tableColumns()
    });
  }

  table.on('toolbar(' + ids.table + ')', function(obj){
    switch (obj.event) {
      case 'Search_{{PLUGIN_DOM_ID_BASE}}':
        reloadTable(true);
        break;
      case 'Refresh_{{PLUGIN_DOM_ID_BASE}}':
        reloadTable(false);
        break;
      case 'Add_{{PLUGIN_DOM_ID_BASE}}':
        openEditor(0);
        break;
    }
  });

  table.on('tool(' + ids.table + ')', function(obj){
    if (obj.event === 'edit') {
      openEditor((obj.data || {}).id);
    } else if (obj.event === 'delete') {
      deleteRecord(obj.data || {});
    }
  });

  async function initPage() {
    await loadMeta();
    renderTable();
    await renderCategoryFilter();
    await renderTaxonomyFilters();
    byId(ids.search).addEventListener('keydown', function(event){
      if (event.key === 'Enter') reloadTable(true);
    });
    if (state.hasCategory && byId(ids.category)) byId(ids.category).addEventListener('change', function(){ reloadTable(true); });
    if (byId(ids.tag)) byId(ids.tag).addEventListener('change', function(){ reloadTable(true); });
    if (byId(ids.topic)) byId(ids.topic).addEventListener('change', function(){ reloadTable(true); });
    reloadTable(false);
  }

  initPage().catch(handleError);
});
</script>
