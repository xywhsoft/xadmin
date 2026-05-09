<style>
.managed-dashboard-page{padding:16px}
.managed-dashboard-page .x-grid{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:12px;margin-bottom:14px}
.managed-dashboard-page .x-card{border:1px solid #e6e6e6;background:#fff;padding:14px 16px}
.managed-dashboard-page .x-card-title{font-size:12px;color:#666;margin-bottom:8px}
.managed-dashboard-page .x-card-value{font-size:22px;line-height:28px;color:#222}
.managed-dashboard-page .x-section{margin-top:14px}
@media(max-width:900px){.managed-dashboard-page .x-grid{grid-template-columns:repeat(2,minmax(0,1fr))}}
</style>
<div class="managed-dashboard-page">
  <div class="layui-btn-container">
    <button class="layui-btn layui-btn-sm" id="Reload_{{PLUGIN_DOM_ID_BASE}}"><i class="layui-icon layui-icon-refresh"></i> &#21047;&#26032;</button>
  </div>
  <div class="x-grid" id="Cards_{{PLUGIN_DOM_ID_BASE}}"></div>
  <div class="x-section">
    <table class="layui-hide" id="TopTable_{{PLUGIN_DOM_ID_BASE}}" lay-filter="TopTable_{{PLUGIN_DOM_ID_BASE}}"></table>
  </div>
  <div class="x-section">
    <table class="layui-hide" id="TrendTable_{{PLUGIN_DOM_ID_BASE}}" lay-filter="TrendTable_{{PLUGIN_DOM_ID_BASE}}"></table>
  </div>
</div>
<script>
layui.use(['table','layer'], function(){
  var table = layui.table;
  var pluginXid = '{{PLUGIN_XID}}';
  var domBase = '{{PLUGIN_DOM_ID_BASE}}';
  var ids = {
    cards: 'Cards_' + domBase,
    top: 'TopTable_' + domBase,
    trend: 'TrendTable_' + domBase,
    reload: 'Reload_' + domBase
  };
  function api(path){ return '/admin/api/plugin/' + pluginXid + path; }
  function byId(id){ return document.getElementById(id); }
  function esc(v){ return String(v == null ? '' : v).replace(/[&<>"]/g,function(c){return {'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c];}); }
  async function getJson(path){
    var res = await fetch(api(path), { cache:'no-store' });
    return await res.json();
  }
  function card(title, value){
    return '<div class="x-card"><div class="x-card-title">'+esc(title)+'</div><div class="x-card-value">'+esc(value)+'</div></div>';
  }
  function dailyTrendRows(data){
    var list = data && data.data;
    if (!Array.isArray(list)) list = [];
    return list.map(function(row){
      return {
        date: row.statDate || row.stat_date || '',
        contentId: row.contentId || row.content_id || 0,
        views: row.viewCount || row.view_count || 0,
        uniqueViews: row.uniqueViewCount || row.unique_view_count || 0
      };
    });
  }
  async function load(){
    var cards = '';
    var rows = [];
    var trendRows = [];
    var likeStats = await getJson('/like/stats').catch(function(){ return null; });
    if (likeStats && likeStats.result && likeStats.data) {
      cards += card('Total likes', likeStats.data.totalLikes || 0);
      cards += card('Liked contents', likeStats.data.contentCount || 0);
      rows.push({type:'like',metric:'recordCount',value:likeStats.data.recordCount || 0});
      rows.push({type:'like',metric:'activeRecordCount',value:likeStats.data.activeRecordCount || 0});
    }
    var viewStats = await getJson('/view/stats').catch(function(){ return null; });
    if (viewStats && viewStats.result && viewStats.data) {
      cards += card('Total views', viewStats.data.totalViews || 0);
      cards += card('Unique views', viewStats.data.uniqueViews || 0);
      rows.push({type:'view',metric:'logCount',value:viewStats.data.logCount || 0});
      rows.push({type:'view',metric:'visitorCount',value:viewStats.data.visitorCount || 0});
    }
    var dailyStats = await getJson('/view/daily/list').catch(function(){ return null; });
    if (dailyStats && dailyStats.result) {
      trendRows = dailyTrendRows(dailyStats);
    }
    byId(ids.cards).innerHTML = cards || '<div class="x-card"><div class="x-card-title">Metrics</div><div class="x-card-value">0</div></div>';
    table.render({
      elem:'#' + ids.top,
      data:rows,
      page:false,
      cols:[[
        {field:'type',title:'Type',width:120},
        {field:'metric',title:'Metric',minWidth:180},
        {field:'value',title:'Value',width:140}
      ]],
      text:{none:'No metric data'}
    });
    table.render({
      elem:'#' + ids.trend,
      data:trendRows,
      page:{limit:20,limits:[20,50,100]},
      cols:[[
        {field:'date',title:'Date',width:140},
        {field:'contentId',title:'Content ID',width:140},
        {field:'views',title:'Views',width:140},
        {field:'uniqueViews',title:'Unique Views',width:160}
      ]],
      text:{none:'No daily trend data'}
    });
  }
  byId(ids.reload).onclick = load;
  load();
});
</script>
