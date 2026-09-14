<style>
.managed-task-page{padding:16px}
.managed-task-filter{display:flex;align-items:center;gap:8px;margin-bottom:10px;flex-wrap:wrap}
.managed-task-filter .layui-input{width:170px}
.managed-task-detail{position:relative;min-height:420px;padding:18px 22px 76px;box-sizing:border-box}
.managed-task-detail .task-actions{position:absolute;left:0;right:0;bottom:0;height:58px;box-sizing:border-box;padding:10px 22px;border-top:1px solid #eee;background:#fff;text-align:right}
.managed-task-detail .task-actions .layui-btn{min-width:92px}
.managed-task-detail .task-json{max-height:180px;overflow:auto;background:#fafafa;border:1px solid #eee;padding:10px;white-space:pre-wrap;word-break:break-all}
</style>

<div class="managed-task-page">
  <div class="managed-task-filter">
    <input type="text" id="TaskType_{{PLUGIN_DOM_ID_BASE}}" class="layui-input" placeholder="任务类型">
    <input type="text" id="TargetType_{{PLUGIN_DOM_ID_BASE}}" class="layui-input" placeholder="目标类型">
    <input type="text" id="TaskStatus_{{PLUGIN_DOM_ID_BASE}}" class="layui-input" placeholder="状态：0/1/2/-1/-2">
    <input type="number" id="TaskLimit_{{PLUGIN_DOM_ID_BASE}}" class="layui-input" value="100" placeholder="上限">
    <button type="button" class="layui-btn layui-btn-sm" id="TaskFilter_{{PLUGIN_DOM_ID_BASE}}"><i class="layui-icon layui-icon-search"></i> 筛选</button>
    <button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="TaskClear_{{PLUGIN_DOM_ID_BASE}}"><i class="layui-icon layui-icon-close"></i> 清空</button>
    <button type="button" class="layui-btn layui-btn-primary layui-btn-sm" id="TaskReload_{{PLUGIN_DOM_ID_BASE}}"><i class="layui-icon layui-icon-refresh"></i> 刷新</button>
  </div>
  <table class="layui-hide" id="TaskTable_{{PLUGIN_DOM_ID_BASE}}" lay-filter="TaskTable_{{PLUGIN_DOM_ID_BASE}}"></table>
</div>

<script type="text/html" id="TaskToolbar_{{PLUGIN_DOM_ID_BASE}}">
  <div class="layui-clear-space">
    <a class="layui-btn layui-btn-xs layui-bg-blue" lay-event="detail">详情</a>
    {{# if(Number(d.status || 0) === -1){ }}
    <a class="layui-btn layui-btn-xs layui-bg-orange" lay-event="retry">重试</a>
    {{# } }}
    {{# if(Number(d.status || 0) === 0 || Number(d.status || 0) === 1){ }}
    <a class="layui-btn layui-btn-xs layui-bg-red" lay-event="cancel">取消</a>
    {{# } }}
  </div>
</script>

<div id="TaskDetail_{{PLUGIN_DOM_ID_BASE}}" class="managed-task-detail" style="display:none;">
  <table class="layui-table">
    <tbody id="TaskDetailRows_{{PLUGIN_DOM_ID_BASE}}"></tbody>
  </table>
  <div class="layui-row layui-col-space12">
    <div class="layui-col-md6">
      <div class="layui-font-13" style="margin:8px 0;">Payload</div>
      <pre class="task-json" id="TaskPayload_{{PLUGIN_DOM_ID_BASE}}"></pre>
    </div>
    <div class="layui-col-md6">
      <div class="layui-font-13" style="margin:8px 0;">Result</div>
      <pre class="task-json" id="TaskResult_{{PLUGIN_DOM_ID_BASE}}"></pre>
    </div>
  </div>
  <div class="task-actions">
    <button type="button" class="layui-btn layui-btn-primary" id="TaskDetailClose_{{PLUGIN_DOM_ID_BASE}}">取消</button>
    <button type="button" class="layui-btn layui-btn-warm" id="TaskDetailRetry_{{PLUGIN_DOM_ID_BASE}}">重试</button>
    <button type="button" class="layui-btn layui-btn-danger" id="TaskDetailCancel_{{PLUGIN_DOM_ID_BASE}}">取消任务</button>
  </div>
</div>

<script>
layui.use(['table','layer'], function(){
  var table = layui.table;
  var layer = layui.layer;
  var pluginXid = '{{PLUGIN_XID}}';
  var domBase = '{{PLUGIN_DOM_ID_BASE}}';
  var currentTask = null;
  var detailLayerIndex = 0;
  function api(path){ return '/admin/api/plugin/' + pluginXid + path; }
  function byId(id){ return document.getElementById(id); }
  function esc(v){ return String(v == null ? '' : v).replace(/[&<>"]/g,function(c){return {'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c];}); }
  function statusText(v){
    v = Number(v || 0);
    if(v === 0) return '待处理';
    if(v === 1) return '处理中';
    if(v === 2) return '完成';
    if(v === -1) return '失败';
    if(v === -2) return '已取消';
    return String(v);
  }
  function query(){
    var params = new URLSearchParams();
    var taskType = byId('TaskType_' + domBase).value.trim();
    var targetType = byId('TargetType_' + domBase).value.trim();
    var status = byId('TaskStatus_' + domBase).value.trim();
    var limit = byId('TaskLimit_' + domBase).value.trim();
    if(taskType) params.set('taskType', taskType);
    if(targetType) params.set('targetType', targetType);
    if(status) params.set('status', status);
    if(limit) params.set('limit', limit);
    return params.toString();
  }
  async function getJson(path){
    var res = await fetch(api(path), {cache:'no-store'});
    return await res.json();
  }
  async function postJson(path, data){
    var res = await fetch(api(path), {method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(data || {})});
    return await res.json();
  }
  function load(){
    table.render({
      elem:'#TaskTable_' + domBase,
      url:api('/task/list') + '?' + query(),
      method:'GET',
      parseData:function(res){
        return {code:res && res.result ? 0 : 1,msg:(res && res.message) || '',count:(res && res.data ? res.data.length : 0),data:(res && res.data) || []};
      },
      page:{limit:20,limits:[20,50,100]},
      toolbar:false,
      cols:[[
        {field:'id',title:'ID',width:90,sort:true},
        {field:'taskType',title:'任务类型',minWidth:150},
        {field:'targetType',title:'目标类型',width:130},
        {field:'targetId',title:'目标ID',width:110},
        {field:'status',title:'状态',width:100,templet:function(d){return statusText(d.status);}},
        {field:'progress',title:'进度',width:90},
        {field:'retryCount',title:'重试',width:80},
        {field:'errorMessage',title:'错误信息',minWidth:180},
        {field:'updateTimeText',title:'更新时间',width:170},
        {title:'操作',toolbar:'#TaskToolbar_' + domBase,width:170}
      ]],
      text:{none:'暂无任务'}
    });
  }
  function fillDetail(row){
    currentTask = row || {};
    byId('TaskDetailRows_' + domBase).innerHTML =
      '<tr><td>ID</td><td>'+esc(currentTask.id)+'</td><td>状态</td><td>'+esc(statusText(currentTask.status))+'</td></tr>' +
      '<tr><td>任务类型</td><td>'+esc(currentTask.taskType)+'</td><td>目标</td><td>'+esc(currentTask.targetType)+' / '+esc(currentTask.targetId)+'</td></tr>' +
      '<tr><td>进度</td><td>'+esc(currentTask.progress)+'</td><td>重试次数</td><td>'+esc(currentTask.retryCount)+'</td></tr>' +
      '<tr><td>创建时间</td><td>'+esc(currentTask.createTimeText)+'</td><td>完成时间</td><td>'+esc(currentTask.finishTimeText)+'</td></tr>' +
      '<tr><td>错误信息</td><td colspan="3">'+esc(currentTask.errorMessage)+'</td></tr>';
    byId('TaskPayload_' + domBase).textContent = currentTask.payload || '{}';
    byId('TaskResult_' + domBase).textContent = currentTask.result || '{}';
    byId('TaskDetailRetry_' + domBase).style.display = Number(currentTask.status || 0) === -1 ? '' : 'none';
    byId('TaskDetailCancel_' + domBase).style.display = (Number(currentTask.status || 0) === 0 || Number(currentTask.status || 0) === 1) ? '' : 'none';
  }
  async function openDetail(id){
    var ret = await getJson('/task/detail?id=' + encodeURIComponent(id));
    var rows = ret && ret.rows ? ret.rows : [];
    fillDetail(rows[0] || {});
    detailLayerIndex = layer.open({type:1,title:'任务详情',area:['760px','620px'],content:byId('TaskDetail_' + domBase)});
  }
  async function runAction(path, id){
    var ret = await postJson(path, {id:id});
    if(ret && ret.result){ layer.msg(ret.message || '操作成功'); load(); if(detailLayerIndex) layer.close(detailLayerIndex); }
    else layer.msg((ret && ret.message) || '操作失败');
  }
  byId('TaskFilter_' + domBase).onclick = load;
  byId('TaskReload_' + domBase).onclick = load;
  byId('TaskClear_' + domBase).onclick = function(){
    byId('TaskType_' + domBase).value = '';
    byId('TargetType_' + domBase).value = '';
    byId('TaskStatus_' + domBase).value = '';
    byId('TaskLimit_' + domBase).value = '100';
    load();
  };
  byId('TaskDetailClose_' + domBase).onclick = function(){ if(detailLayerIndex) layer.close(detailLayerIndex); };
  byId('TaskDetailRetry_' + domBase).onclick = function(){ if(currentTask && currentTask.id) runAction('/task/retry', currentTask.id); };
  byId('TaskDetailCancel_' + domBase).onclick = function(){ if(currentTask && currentTask.id) runAction('/task/cancel', currentTask.id); };
  table.on('tool(TaskTable_' + domBase + ')', function(obj){
    if(obj.event === 'detail') openDetail(obj.data.id);
    if(obj.event === 'retry') runAction('/task/retry', obj.data.id);
    if(obj.event === 'cancel') runAction('/task/cancel', obj.data.id);
  });
  load();
});
</script>
