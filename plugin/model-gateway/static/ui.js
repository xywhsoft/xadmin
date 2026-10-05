'use strict';
(() => {
  const admin = document.body.dataset.page === 'admin',
    message = document.querySelector('#message');
  const rateNames = {
    input: '普通输入',
    cache_read: '缓存读取',
    cache_write_5m: '缓存写入（5 分钟）',
    cache_write_1h: '缓存写入（1 小时）',
    output: '输出（含思考）'
  };
  const columns = {
    id: '编号',
    member_id: '用户',
    model_id: '模型',
    channel_id: '渠道',
    protocol: '协议',
    state: '状态',
    reserved: '预留',
    charged: '实扣',
    charged_micros: '实扣',
    cost: '成本',
    input_tokens: '普通输入',
    cache_read_tokens: '缓存读取',
    cache_write_5m_tokens: '缓存写入 5m',
    cache_write_1h_tokens: '缓存写入 1h',
    output_tokens: '输出',
    reasoning_tokens: '思考子集',
    upstream_status: '上游状态',
    error: '错误',
    created_at: '时间',
    first_byte_ms: '首块 ms',
    total_ms: '总时长 ms',
    requests: '次数',
    known_input_tokens: '已知输入',
    known_output_tokens: '已知输出',
    unknown_usage_requests: '未知用量次数'
  };
  const states = {
    admitted: '已受理',
    running: '进行中',
    reserved: '已预留',
    pending: '待核实',
    settling: '结算处理中',
    settled: '已结算',
    released: '已释放',
    rejected: '未执行',
    refunded: '已退款'
  };
  let csrf = '',
    snapshot = {
      channels: [],
      models: []
    };
  const elem = (tag, text, className) => {
    const node = document.createElement(tag);
    if (text !== undefined) node.textContent = text;
    if (className) node.className = className;
    return node;
  };

  function notify(text, error = false) {
    message.textContent = text;
    message.classList.toggle('error', error);
  }

  function amount(text) {
    const value = String(text).trim();
    if (!/^\d+(?:\.\d{1,6})?$/.test(value)) throw new Error('金额最多六位小数，且不能为负');
    const [a, b = ''] = value.split('.');
    const n = BigInt(a) * 1000000n + BigInt(b.padEnd(6, '0'));
    if (n > 1000000000000n) throw new Error('报价过大');
    return Number(n);
  }

  function money(n, unit = true) {
    if (n === null || n === undefined) return '未知';
    const value = BigInt(n),
      negative = value < 0n,
      x = negative ? -value : value;
    const fraction = String(x % 1000000n).padStart(6, '0').replace(/0+$/, '');
    return `${negative?'-':''}${x/1000000n}${fraction?'.'+fraction:''}${unit?' 元':''}`;
  }
  async function api(path, body) {
    const response = await fetch(path, {
      credentials: 'same-origin',
      headers: body ? {
        'Content-Type': 'application/json',
        'X-CSRF-Token': csrf
      } : {},
      ...(body ? {
        method: 'POST',
        body: JSON.stringify(body)
      } : {})
    });
    const value = await response.json();
    if (!response.ok || value.code) throw new Error(response.status === 401 ? '请先登录账号' : value.message ||
      '请求失败');
    return value.data;
  }

  function card(parent, title, value) {
    const node = elem('div', undefined, 'card');
    node.append(elem('span', title), elem('strong', value));
    parent.append(node);
  }

  function table(target, rows, keys, receipt = false) {
    const parent = document.querySelector(target);
    parent.replaceChildren();
    if (!rows?.length) {
      parent.append(elem('p', '暂无记录', 'hint'));
      return;
    }
    const wrap = elem('div', undefined, 'table-scroll'),
      table = elem('table'),
      head = elem('thead'),
      tr = elem('tr'),
      body = elem('tbody');
    keys.forEach(key => tr.append(elem('th', columns[key] || key)));
    head.append(tr);
    rows.forEach(row => {
      const tr = elem('tr');
      keys.forEach(key => {
        let value = row[key],
          cell = elem('td');
        if (['reserved', 'charged', 'cost', 'charged_micros'].includes(key)) value = money(value);
        else if (key === 'created_at') value = new Date(value * 1000).toLocaleString();
        else if (key === 'state') value = states[value] || value;
        if (receipt && key === 'id') {
          const button = elem('button', value);
          button.type = 'button';
          button.addEventListener('click', () => showReceipt(row.id).catch(error => notify(error
            .message, true)));
          cell.append(button);
        } else cell.textContent = value === null || value === undefined ? '未知' : String(value);
        tr.append(cell);
      });
      body.append(tr);
    });
    table.append(head, body);
    wrap.append(table);
    parent.append(wrap);
  }
  async function showReceipt(id) {
    const value = await api('/api/v1/ai/requests/' + encodeURIComponent(id)),
      dialog = document.querySelector('#receipt'),
      details = dialog.querySelector('.receipt-details');
    details.replaceChildren();
    const section = (title, rows) => {
      const list = elem('dl', undefined, 'receipt-fields');
      rows.forEach(([label, text]) => list.append(elem('dt', label), elem('dd', text)));
      details.append(elem('h3', title), list);
    };
    section('本次调用', [
      ['模型', value.model_id],
      ['状态', states[value.state] || value.state],
      ['时间', new Date(value.created_at * 1000).toLocaleString()],
      ['请求编号', value.id]
    ]);
    section('费用', [
      ['预留金额', money(value.reserved_micros)],
      ['原始消费', money(value.charged_micros)],
      ['已退金额', money(value.refunded_micros)],
      ['净消费', money(value.net_charged_micros)]
    ]);
    if (value.usage) {
      section('平台确认的 token 用量', Object.entries({
        input_tokens: '普通输入',
        cache_read_tokens: '缓存读取',
        cache_write_5m_tokens: '缓存写入（5 分钟）',
        cache_write_1h_tokens: '缓存写入（1 小时）',
        output_tokens: '输出（包含思考）',
        reasoning_tokens: '其中思考'
      }).map(([key, title]) => [title, value.usage[key]?.toLocaleString() ?? '未知']));
    } else details.append(elem('p', '尚未获得可确认的用量，请稍后刷新。', 'hint'));
    if (value.error) details.append(elem('p', '调用信息：' + value.error, 'hint'));
    section('响应时间', [
      ['首个响应', value.first_byte_ms === null ? '未知' : value.first_byte_ms + ' 毫秒'],
      ['总时长', value.total_ms === null ? '未知' : value.total_ms + ' 毫秒']
    ]);
    dialog.showModal();
  }

  function modelCards(models) {
    const parent = document.querySelector('#model-list');
    parent.replaceChildren();
    if (!models.length) {
      parent.append(elem('p', '尚未配置在线模型', 'hint'));
      return;
    }
    models.forEach(model => {
      const node = elem('article', undefined, 'model-card'),
        heading = elem('h3', model.title);
      heading.append(elem('span', model.free ? '免费' : model.member_only ? '会员模型' : '按用量付费', 'pill'));
      node.append(heading, elem('p', model.description || model.id), elem('p',
        `上下文 ${model.context_window.toLocaleString()} · 最大输出 ${model.max_output.toLocaleString()} · ${model.default_protocol}`,
        'hint'));
      const prices = Object.keys(rateNames).map(key =>
        `${rateNames[key]} ${money(model.sale_rates[key],false)}`).join(' / ');
      node.append(elem('p', `${prices} 元 / 百万 token`, 'hint'));
      if (model.reasoning_efforts) node.append(elem('p', '思考强度：' + model.reasoning_efforts, 'hint'));
      if (admin) {
        const edit = elem('button', '编辑模型');
        edit.type = 'button';
        edit.addEventListener('click', () => fillModel(model));
        node.append(edit);
      } else if (!model.available) node.append(elem('p', model.unavailable_reason ===
        'membership_required' ? '需要对应会员套餐' : '渠道暂不可用', 'hint'));
      else node.append(elem('p', '当前账号可用', 'hint'));
      parent.append(node);
    });
  }

  function rateFields(parent, prefix) {
    Object.entries(rateNames).forEach(([key, title]) => {
      const label = elem('label', title),
        input = elem('input');
      input.name = prefix + key;
      input.value = '0';
      input.inputMode = 'decimal';
      input.required = true;
      label.append(input);
      parent.append(label);
    });
  }
  function toggleRates(parent, visible) {
    parent.hidden = !visible;
    parent.querySelectorAll('input').forEach(input => input.disabled = !visible);
  }

  function readRates(form, prefix) {
    return Object.fromEntries(Object.keys(rateNames).map(key => [key, amount(form.elements[prefix + key]
      .value)]));
  }

  function fillRates(form, prefix, rates) {
    Object.keys(rateNames).forEach(key => form.elements[prefix + key].value = money(rates?.[key] || 0,
      false));
  }

  function fillForm(form, value) {
    Object.entries(value).forEach(([key, v]) => {
      const input = form.elements[key];
      if (!input) return;
      if (input.type === 'checkbox') input.checked = Boolean(v);
      else input.value = v ?? '';
    });
  }

  function routeRow(route = {}) {
    const node = elem('div', undefined, 'route'),
      grid = elem('div', undefined, 'field-grid');
    node.append(grid);
    const field = (title, name, type = 'input') => {
      const label = elem('label', title),
        input = elem(type);
      input.name = name;
      label.append(input);
      grid.append(label);
      return input;
    };
    const protocol = field('协议', 'protocol', 'select');
    ['chat', 'responses', 'anthropic'].forEach(value => {
      const option = elem('option', value);
      option.value = value;
      protocol.append(option);
    });
    protocol.value = route.protocol || 'chat';
    const channel = field('渠道', 'channel_id', 'select');

    function choices() {
      const previous = channel.value;
      channel.replaceChildren();
      snapshot.channels.filter(c => c.protocol === protocol.value).forEach(c => {
        const option = elem('option', c.title + (c.enabled ? '' : '（已停用）'));
        option.value = c.id;
        channel.append(option);
      });
      if ([...channel.options].some(o => o.value === previous)) channel.value = previous;
    }
    protocol.addEventListener('change', choices);
    node.updateChannels = choices;
    choices();
    channel.value = route.channel_id || channel.value;
    channel.required = true;
    const wire = field('上游模型名称', 'wire_model');
    wire.value = route.wire_model || '';
    wire.required = true;
    wire.maxLength = 128;
    const priority = field('优先级', 'priority');
    priority.type = 'number';
    priority.min = 0;
    priority.max = 1000;
    priority.value = route.priority || 0;
    priority.required = true;
    const label = elem('label', '覆盖本路由成本', 'check'),
      known = elem('input');
    known.type = 'checkbox';
    known.name = 'route_cost_known';
    known.checked = Boolean(route.cost_rates);
    label.prepend(known);
    node.append(label);
    const prices = elem('div', undefined, 'field-grid rates');
    rateFields(prices, 'route_cost_');
    node.append(prices);
    toggleRates(prices, known.checked);
    known.addEventListener('change', () => toggleRates(prices, known.checked));
    Object.keys(rateNames).forEach(key => prices.querySelector(`[name="route_cost_${key}"]`).value = money(
      route.cost_rates?.[key] || 0, false));
    const remove = elem('button', '移除此路由');
    remove.type = 'button';
    remove.addEventListener('click', () => node.remove());
    node.append(remove);
    document.querySelector('#routes').append(node);
  }

  function fillModel(model) {
    const form = document.querySelector('#model');
    fillForm(form, model);
    fillRates(form, 'sale_', model.sale_rates);
    fillRates(form, 'cost_', model.cost_rates);
    form.elements.cost_known.checked = Boolean(model.cost_rates);
    toggleRates(document.querySelector('#cost-rates'), Boolean(model.cost_rates));
    document.querySelector('#routes').replaceChildren();
    model.routes.forEach(routeRow);
    document.querySelector('#model-heading').textContent = '编辑模型：' + model.title;
    form.scrollIntoView({
      behavior: 'smooth',
      block: 'start'
    });
  }

  function channelCards() {
    const parent = document.querySelector('#channel-list');
    parent.replaceChildren();
    snapshot.channels.forEach(channel => {
      const node = elem('article', undefined, 'model-card');
      node.append(elem('h3', channel.title), elem('p',
        `${channel.id} · ${channel.protocol} · ${channel.enabled?'启用':'停用'} · ${channel.key_configured?'密钥已配置':'缺少密钥'}${channel.key_from_environment?'（环境变量）':''}`
        ), elem('p', channel.url, 'hint'));
      const edit = elem('button', '编辑渠道');
      edit.type = 'button';
      edit.addEventListener('click', () => {
        fillForm(document.querySelector('#channel'), channel);
        document.querySelector('#channel').scrollIntoView({
          behavior: 'smooth'
        });
      });
      node.append(edit);
      parent.append(node);
    });
    const select = document.querySelector('#credentials [name="channel_id"]'),
      previous = select.value;
    select.replaceChildren();
    snapshot.channels.forEach(channel => {
      const option = elem('option', channel.title);
      option.value = channel.id;
      select.append(option);
    });
    if ([...select.options].some(o => o.value === previous)) select.value = previous;
  }
  async function refresh() {
    if (admin) {
      snapshot = await api('/admin/model-gateway/state');
      csrf = snapshot.csrf_token;
      modelCards(snapshot.models);
      channelCards();
      document.querySelectorAll('#routes .route').forEach(row => row.updateChannels?.());
      document.querySelector('#repair-label').hidden = !snapshot.credentials_invalid;
      const limits = document.querySelector('#limits');
      limits.replaceChildren();
      [
        ['全站同时请求', snapshot.limits.max_concurrent],
        ['每人每分钟', snapshot.limits.minute_limit],
        ['每人每天', snapshot.limits.daily_limit],
        ['全站每天', snapshot.limits.global_daily_limit],
        ['每人每日消费上限', money(snapshot.limits.daily_budget_micros)]
      ].forEach(([name, value]) => card(limits, name, value));
      table('#request-list', snapshot.requests, ['id', 'member_id', 'model_id', 'protocol', 'state',
        'charged', 'cost', 'input_tokens', 'cache_read_tokens', 'output_tokens', 'reasoning_tokens',
        'error', 'first_byte_ms', 'total_ms', 'created_at'
      ]);
    } else {
      const [catalog, usage] = await Promise.all([api('/api/v1/ai/catalog'), api('/api/v1/ai/usage')]);
      modelCards(catalog.models);
      const parent = document.querySelector('#totals');
      parent.replaceChildren();
      const totals = usage.totals[0];
      [
        ['最近 30 天请求', totals.requests],
        ['实际消费', money(totals.charged_micros)],
        ['待核实或在途', totals.unresolved_requests || 0],
        ['未知用量', totals.unknown_usage_requests || 0]
      ].forEach(([name, value]) => card(parent, name, value));
      table('#model-usage', usage.by_model, ['model_id', 'requests', 'charged_micros',
        'known_input_tokens', 'known_output_tokens', 'unknown_usage_requests'
      ]);
      table('#request-list', usage.requests, ['id', 'model_id', 'protocol', 'state', 'charged_micros',
        'error', 'created_at'
      ], true);
    }
  }
  document.querySelector('#refresh').addEventListener('click', () => refresh().then(() => notify('已刷新'))
    .catch(e => notify(e.message, true)));
  if (admin) {
    rateFields(document.querySelector('#sale-rates'), 'sale_');
    rateFields(document.querySelector('#cost-rates'), 'cost_');
    toggleRates(document.querySelector('#cost-rates'), false);
    document.querySelector('[name="cost_known"]').addEventListener('change', event =>
      toggleRates(document.querySelector('#cost-rates'), event.target.checked));
    document.querySelectorAll('[data-tab]').forEach(button => button.addEventListener('click', () => {
      document.querySelectorAll('[data-panel]').forEach(panel => panel.hidden = panel.dataset
        .panel !== button.dataset.tab);
      document.querySelectorAll('[data-tab]').forEach(tab => tab.setAttribute('aria-selected', String(
        tab === button)));
    }));
    document.querySelector('[data-tab]').setAttribute('aria-selected', 'true');
    document.querySelector('#add-route').addEventListener('click', () => routeRow());
    document.querySelector('#new-model').addEventListener('click', () => {
      document.querySelector('#model').reset();
      document.querySelector('#routes').replaceChildren();
      document.querySelector('#model-heading').textContent = '创建模型';
      toggleRates(document.querySelector('#cost-rates'), false);
      routeRow();
    });
    document.querySelector('#new-channel').addEventListener('click', () => document.querySelector(
      '#channel').reset());
    ['channel', 'model', 'credentials'].forEach(name => {
      const form = document.querySelector('#' + name);
      form.addEventListener('submit', async event => {
        event.preventDefault();
        const button = form.querySelector('button[type="submit"],button:not([type])');
        button.disabled = true;
        try {
          const f = Object.fromEntries(new FormData(form)),
            body = {
              ...f
            };
          if (name === 'channel') {
            ['timeout_ms', 'first_byte_ms', 'idle_ms', 'max_concurrent'].forEach(k => body[k] =
              Number(f[k]));
            ['enabled', 'allow_http'].forEach(k => body[k] = form.elements[k].checked);
          }
          if (name === 'model') {
            ['context_window', 'max_output'].forEach(k => body[k] = Number(f[k]));
            ['enabled', 'member_only', 'tool_calling', 'vision'].forEach(k => body[k] = form
              .elements[k].checked);
            body.sale_rates = readRates(form, 'sale_');
            body.cost_rates = form.elements.cost_known.checked ? readRates(form, 'cost_') : null;
            body.routes = [...document.querySelectorAll('#routes .route')].map(row => ({
              protocol: row.querySelector('[name="protocol"]').value,
              channel_id: row.querySelector('[name="channel_id"]').value,
              wire_model: row.querySelector('[name="wire_model"]').value,
              priority: Number(row.querySelector('[name="priority"]').value),
              cost_rates: row.querySelector('[name="route_cost_known"]').checked ? Object
                .fromEntries(Object.keys(rateNames).map(k => [k, amount(row.querySelector(
                  `[name="route_cost_${k}"]`).value)])) : null
            }));
            Object.keys(body).forEach(k => {
              if (k.startsWith('sale_') && k !== 'sale_rates' || k.startsWith('cost_') &&
                k !== 'cost_rates' || ['protocol', 'channel_id', 'wire_model', 'priority',
                  'route_cost_known', 'cost_known'
                ].includes(k) || k.startsWith('route_cost_')) delete body[k];
            });
          }
          if (name === 'credentials') body.replace_invalid = form.elements.replace_invalid
          .checked;
          await api('/admin/model-gateway/' + name, body);
          if (name === 'credentials') form.elements.api_key.value = '';
          await refresh();
          notify('已保存');
        } catch (error) {
          notify(error.message, true);
        } finally {
          button.disabled = false;
        }
      });
    });
  }
  refresh().then(() => {
    if (admin && !document.querySelector('#routes').children.length) routeRow();
  }).catch(error => notify(error.message, true));
})();
