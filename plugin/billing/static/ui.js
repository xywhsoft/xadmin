'use strict';
(() => {
  const admin = document.body.dataset.page === 'admin';
  const message = document.querySelector('#message');
  let csrf = '', memberId = '';
  const labels = {id:'编号',member_id:'会员',kind:'类型',amount:'金额',amount_micros:'金额',source:'来源',request_id:'请求编号',reason:'原因',actor:'操作人',created_at:'时间',state:'状态',charged:'实扣',expires_at:'到期时间',plan_id:'套餐',title:'名称',starts_at:'开始时间',discount_bps:'收费比例',credit_micros:'周期额度',period_seconds:'额度周期',duration_seconds:'会员时长',concurrency_limit:'同时请求',model_ids:'会员模型',enabled:'可开通',cancelled_at:'终止时间'};
  const kinds = {migration:'余额迁移',adjust:'余额调整',consume:'消费',grant:'额度发放',expire:'额度到期',refund:'退款',refund_operation:'退款确认',reserved:'已预留',pending:'待核实',settled:'已结算',released:'已释放',refunded:'已退款'};
  function notify(text, error = false) { message.textContent = text; message.classList.toggle('error', error); }
  function micros(text) {
    const value = String(text).trim();
    if (!/^-?\d+(?:\.\d{1,6})?$/.test(value)) throw new Error('金额最多支持六位小数');
    const negative = value.startsWith('-'), [whole, fraction = ''] = value.replace(/^-/, '').split('.');
    const amount = BigInt(whole) * 1000000n + BigInt(fraction.padEnd(6, '0'));
    if (amount > 1000000000000000n) throw new Error('金额超出范围');
    return Number(negative ? -amount : amount);
  }
  function money(value) {
    const amount = BigInt(value || 0), absolute = amount < 0n ? -amount : amount;
    const fraction = String(absolute % 1000000n).padStart(6, '0').replace(/0+$/, '');
    return `${amount < 0n ? '-' : ''}${absolute / 1000000n}${fraction ? '.' + fraction : ''} 元`;
  }
  async function api(path, body) {
    const response = await fetch(path, {credentials:'same-origin', headers:body ? {'Content-Type':'application/json','X-CSRF-Token':csrf} : {}, ...(body ? {method:'POST',body:JSON.stringify(body)} : {})});
    const result = await response.json();
    if (!response.ok || result.code) throw new Error(response.status === 401 ? '请先登录账号' : result.message || '请求失败');
    return result.data;
  }
  function table(target, rows, columns) {
    const parent = document.querySelector(target); if (!parent) return;
    parent.replaceChildren();
    if (!rows?.length) { const empty=document.createElement('p'); empty.className='hint';empty.textContent='暂无记录';parent.append(empty);return; }
    const wrap=document.createElement('div'), table=document.createElement('table'), head=document.createElement('thead'), title=document.createElement('tr'), body=document.createElement('tbody');wrap.className='table-scroll';
    columns.forEach(key => {const cell=document.createElement('th');cell.textContent=labels[key] || key;title.append(cell);});head.append(title);
    rows.forEach(row => {const tr=document.createElement('tr');columns.forEach(key => {
      const cell=document.createElement('td');let value=row[key];
      if (['amount','amount_micros','charged','credit_micros'].includes(key)) value=money(value);
      else if (key.endsWith('_at')) value=value ? new Date(value*1000).toLocaleString() : '—';
      else if (key==='kind' || key==='state') value=kinds[value] || value;
      else if (key==='discount_bps') value=`${value/100}%`;
      else if (key.endsWith('_seconds')) value=`${value/86400} 天`;
      else if (key==='enabled') value=value ? '是' : '否';
      cell.textContent=value ?? '—';if(['reason','request_id','model_ids'].includes(key))cell.className='long';tr.append(cell);
    });body.append(tr);});table.append(head,body);wrap.append(table);parent.append(wrap);
  }
  function account(value) {
    const parent=document.querySelector('#account');parent.replaceChildren();if(!value)return;
    [['可用余额与额度',money(value.available_micros)],['充值余额',money(value.cash_micros)],['服务额度',money(value.credit_micros)],['已预留',money(value.cash_reserved_micros+value.credit_reserved_micros)],['会员',value.plan_id || '普通账号']].forEach(([title,content]) => {
      const card=document.createElement('div'), label=document.createElement('span'), detail=document.createElement('strong');card.className='card';label.textContent=title;detail.textContent=content;card.append(label,detail);parent.append(card);
    });
  }
  async function refresh() {
    if(admin){const state=await api(`/admin/billing/state${memberId ? '?member_id='+encodeURIComponent(memberId) : ''}`);csrf=state.csrf_token;account(state.account);
      table('#plans-table',state.plans,['id','title','duration_seconds','period_seconds','credit_micros','discount_bps','concurrency_limit','model_ids','enabled']);
      table('#transactions-table',state.transactions,['id','member_id','kind','amount','source','request_id','reason','actor','created_at']);
      table('#requests-table',state.requests,['request_id','member_id','model','state','amount','charged','expires_at']);
      table('#subscriptions-table',state.subscriptions,['member_id','plan_id','title','starts_at','expires_at','cancelled_at']);
    } else {
      account(await api('/api/v1/billing/account'));
      const [plans,transactions,subscriptions]=await Promise.all([api('/api/v1/billing/plans'),api('/api/v1/billing/transactions'),api('/api/v1/billing/subscriptions')]);
      table('#plans-table',plans,['title','duration_seconds','period_seconds','credit_micros','discount_bps']);
      table('#transactions-table',transactions,['kind','amount_micros','source','request_id','reason','created_at']);
      table('#subscriptions-table',subscriptions,['title','starts_at','expires_at','cancelled_at']);
    }
  }
  document.querySelectorAll('[data-tab]').forEach(button => button.addEventListener('click',() => {
    document.querySelectorAll('[data-panel]').forEach(panel => panel.hidden=panel.dataset.panel!==button.dataset.tab);
    document.querySelectorAll('[data-tab]').forEach(tab => tab.setAttribute('aria-selected',String(tab===button)));
  }));
  document.querySelector('[data-tab]')?.setAttribute('aria-selected','true');
  document.querySelector('#lookup')?.addEventListener('submit',event => {event.preventDefault();memberId=new FormData(event.target).get('member_id');refresh().catch(error => notify(error.message,true));});
  ['adjust','grant','plan','subscribe','refund','resolve'].forEach(action => {
    const form=document.querySelector('#'+action);if(!form)return;
    form.addEventListener('input',() => delete form.dataset.operation);
    form.addEventListener('submit',async event => {
      event.preventDefault();const button=form.querySelector('button');button.disabled=true;
      try {
        const fields=Object.fromEntries(new FormData(form)), body={};
        if(fields.member_id)body.member_id=Number(fields.member_id);
        if(['adjust','grant','refund','subscribe'].includes(action)) body.operation_id=form.dataset.operation ||= crypto.randomUUID();
        if(fields.reason)body.reason=fields.reason;if(fields.request_id)body.request_id=fields.request_id;
        if(fields.amount!==undefined)body.amount_micros=micros(fields.amount);
        if(action==='grant')body.expires_at=fields.expiry ? Math.floor(new Date(fields.expiry).getTime()/1000) : 0;
        if(action==='subscribe')body.plan_id=fields.plan_id;
        if(action==='plan')Object.assign(body,{id:fields.id,title:fields.title,duration_days:Number(fields.duration_days),period_days:Number(fields.period_days),credit_micros:micros(fields.credit),discount_bps:micros(fields.discount)/10000,concurrency_limit:Number(fields.concurrency_limit),model_ids:fields.model_ids,enabled:fields.enabled==='on'});
        await api('/admin/billing/'+action,body);delete form.dataset.operation;notify('已保存');await refresh();
      } catch(error){notify(error.message,true);} finally {button.disabled=false;}
    });
  });
  refresh().catch(error => notify(error.message,true));
})();
