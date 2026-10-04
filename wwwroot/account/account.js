'use strict';
(() => {
  const $ = id => document.getElementById(id);
  let providers = {providers: []}, authChallenge = null, contactChallenge = null;
  const messages = {
    'identifier or password is incorrect': '登录标识或密码不正确。',
    'identifier or password is invalid': '请输入有效的账号、手机或邮箱。',
    'confirm your identity again': '请先在“确认当前身份”中重新验证。',
    'cannot remove the last login method': '请先添加另一种登录方式，再进行解绑。',
    'account name is reserved': '这个账号名已被使用，请更换。',
    'CSRF verification failed': '页面凭据已变化，请刷新后重试。',
    'unauthorized': '会话已失效，请重新登录。'
  };
  function flash(text, error = false) {
    $('flash').textContent = text; $('flash').classList.toggle('error', error); $('flash').hidden = false;
  }
  function csrf() {
    return document.cookie.split(';').map(s => s.trim()).find(s => s.startsWith('MCSRF='))?.slice(6) || '';
  }
  async function api(path, method = 'GET', data) {
    const headers = {Accept: 'application/json'};
    if (data !== undefined) headers['Content-Type'] = 'application/json';
    if (method !== 'GET' && csrf()) headers['X-CSRF-Token'] = csrf();
    const controller = new AbortController(), timeout = setTimeout(() => controller.abort(), 45000);
    try {
      const response = await fetch(path, {method, headers, credentials: 'same-origin', signal: controller.signal,
        ...(data === undefined ? {} : {body: JSON.stringify(data)})});
      const result = await response.json();
      if (!response.ok || result.code !== 0) {
        const error = new Error(messages[result.msg] || result.msg || `请求失败 (${response.status})`);
        error.status = response.status; throw error;
      }
      return result.data;
    } finally { clearTimeout(timeout); }
  }
  function run(element, action) {
    if (element.dataset.busy) return;
    element.dataset.busy = 'true';
    const inputs = [...element.querySelectorAll('input,select,button')];
    if (element.tagName === 'BUTTON') inputs.push(element);
    const disabled = inputs.map(input => input.disabled);
    inputs.forEach(input => input.disabled = true);
    Promise.resolve().then(action).catch(error => flash(error.name === 'AbortError' ? '请求超时，请稍后重试。' : error.message, true))
      .finally(() => {inputs.forEach((input, i) => input.disabled = disabled[i]); delete element.dataset.busy;});
  }
  function form(id, action) {
    $(id).addEventListener('submit', event => {
      event.preventDefault(); const data = Object.fromEntries(new FormData(event.target));
      run(event.target, () => action(data));
    });
  }
  function button(text, action, secondary = true) {
    const node = document.createElement('button'); node.type = 'button'; node.textContent = text;
    if (secondary) node.className = 'secondary';
    node.addEventListener('click', () => run(node, action)); return node;
  }
  function row(title, detail, control) {
    const node = document.createElement('div'); node.className = 'row';
    const description = document.createElement('div'), strong = document.createElement('strong');
    strong.textContent = title; description.append(strong);
    const sub = document.createElement('p'); sub.className = 'hint'; sub.textContent = detail; description.append(sub);
    node.append(description); if (control) node.append(control); return node;
  }
  function tab(id) {
    for (const name of ['login', 'register', 'otp', 'recover']) $(name).hidden = name !== id;
    document.querySelectorAll('[data-tab]').forEach(node => node.setAttribute('aria-pressed', String(node.dataset.tab === id)));
    authChallenge = null; $('auth-code').reset(); $('auth-code').hidden = true;
  }
  document.querySelectorAll('[data-tab]').forEach(node => node.addEventListener('click', () => tab(node.dataset.tab)));
  async function oauth(provider, action = 'start') {
    const base = action === 'start' ? '/api/v1/auth/oauth/' : '/api/v1/profile/identities/';
    const data = await api(base + provider + '/' + action, 'POST', {});
    const url = new URL(data.authorization_url);
    if (url.protocol !== 'https:' || !['github.com', 'open.weixin.qq.com'].includes(url.hostname)) throw new Error('授权地址无效。');
    location.assign(url.href);
  }
  const providerName = id => id === 'github' ? 'GitHub' : '微信';
  const time = value => new Date(value * 1000).toLocaleString();
  async function refresh() {
    let session;
    try {session = await api('/api/v1/session');}
    catch (error) {
      if (error.status !== 401) throw error;
      $('member').hidden = $('logout').hidden = true; $('auth').hidden = false;
      $('heading').textContent = '登录网站账户'; return;
    }
    const [profile, identities, sessions] = await Promise.all([
      api('/api/v1/profile'), api('/api/v1/profile/identities'), api('/api/v1/sessions')]);
    $('auth').hidden = true; $('member').hidden = $('logout').hidden = false;
    $('heading').textContent = profile.nickname || profile.username || '我的账户';
    $('member-id').textContent = `用户 ${profile.id} · 账号：${profile.username || '尚未设置'}`;
    $('profile').elements.nickname.value = profile.nickname || '';
    $('profile').elements.avatar.value = profile.avatar || '';
    $('credentials').elements.username.value = profile.username || '';
    $('proof').textContent = session.reauth_until > Date.now() / 1000 ? `身份已确认，有效至 ${time(session.reauth_until)}` : '敏感变更前需要重新确认身份。';
    $('contact-list').replaceChildren();
    for (const channel of ['phone', 'email']) {
      const name = channel === 'phone' ? '手机' : '邮箱', verified = profile[channel + '_verified'];
      const remove = profile[channel] ? button('解绑', async () => {
        if (!confirm(`解绑${name}后，将退出其他设备。继续吗？`)) return;
        await api('/api/v1/profile/contacts', 'DELETE', {channel}); await refresh(); flash(`${name}已解绑。`);
      }) : null;
      $('contact-list').append(row(name, (profile[channel] || '未绑定') + (profile[channel] ? (verified ? ' · 已验证' : ' · 未验证，不能登录') : ''), remove));
    }
    $('identity-list').replaceChildren(); $('provider-reauth').replaceChildren();
    for (const identity of identities) {
      $('identity-list').append(row(providerName(identity.provider), identity.subject, button('解绑', async () => {
        if (!confirm('解绑此第三方身份后，将退出其他设备。继续吗？')) return;
        await api('/api/v1/profile/identities/' + identity.id, 'DELETE'); await refresh(); flash('第三方身份已解绑。');
      })));
      if (providers.providers.some(p => p.id === identity.provider))
        $('provider-reauth').append(button('通过 ' + providerName(identity.provider) + ' 确认', () => oauth(identity.provider, 'reauth')));
    }
    if (!identities.length) $('identity-list').append(row('尚未绑定', providers.providers.length ? '可绑定网站启用的第三方登录方式。' : '网站尚未启用第三方登录。'));
    $('provider-bind').replaceChildren();
    for (const p of providers.providers) if (!identities.some(identity => identity.provider === p.id))
      $('provider-bind').append(button('绑定 ' + p.name, () => oauth(p.id, 'bind')));
    $('session-list').replaceChildren();
    for (const device of sessions) $('session-list').append(row(device.current ? '当前设备' : '其他设备',
      `${device.user_agent || 'API 客户端'} · ${device.ip} · 最近使用 ${time(device.last_used)}`,
      device.current ? null : button('退出', async () => {await api('/api/v1/sessions', 'DELETE', {session_id: device.id}); await refresh(); flash('设备已退出。');})));
  }
  form('login', async data => {await api('/api/v1/login', 'POST', data); $('login').reset(); await refresh(); flash('登录成功。');});
  form('register', async data => {await api('/api/v1/register', 'POST', data); $('register').reset(); tab('login'); $('login').elements.identifier.value = data.username; flash('注册成功，请登录。');});
  for (const id of ['otp', 'recover']) {
    form(id, async data => {
      const challenge = await api('/api/v1/auth/challenges', 'POST', {...data, purpose: id === 'otp' ? 'login' : 'recover'});
      authChallenge = {id: challenge.challenge_id, recover: id === 'recover'};
      $('recovery-password').hidden = !authChallenge.recover; $('auth-code').elements.newPassword.required = authChallenge.recover;
      $('auth-code').hidden = false; $('auth-code').elements.code.focus();
      flash(challenge.delivery === 'unknown' ? '发送结果待确认；如已收到验证码，可以继续验证。' : '验证码已发送。');
    });
    $(id).addEventListener('input', () => {authChallenge = null; $('auth-code').hidden = true;});
  }
  form('auth-code', async data => {
    if (!authChallenge) throw new Error('请重新发送验证码。');
    const payload = {challenge_id: authChallenge.id, code: data.code}, recovery = authChallenge.recover;
    if (recovery) payload.newPassword = data.newPassword;
    await api('/api/v1/auth/challenges/verify', 'POST', payload); authChallenge = null; $('auth-code').reset();
    if (recovery) {tab('login'); flash('密码已重置，请重新登录。');}
    else {await refresh(); flash('登录成功。');}
  });
  form('profile', async data => {await api('/api/v1/profile', 'PUT', data); await refresh(); flash('资料已保存。');});
  form('credentials', async data => {await api('/api/v1/profile/credentials', 'POST', data); $('credentials').elements.password.value = ''; await refresh(); flash('账号和密码已更新，其他设备已退出。');});
  form('reauth', async data => {await api('/api/v1/profile/reauth', 'POST', data); $('reauth').reset(); await refresh(); flash('身份已确认。');});
  form('contact', async data => {
    const challenge = await api('/api/v1/profile/contacts/challenge', 'POST', data);
    contactChallenge = challenge.challenge_id; $('contact-code').hidden = false; $('contact-code').elements.code.focus();
    flash(challenge.delivery === 'unknown' ? '发送结果待确认；如已收到验证码，可以继续验证。' : '验证码已发送至新目标。');
  });
  $('contact').addEventListener('input', () => {contactChallenge = null; $('contact-code').hidden = true;});
  form('contact-code', async data => {
    if (!contactChallenge) throw new Error('请重新发送绑定验证码。');
    await api('/api/v1/profile/contacts/confirm', 'POST', {...data, challenge_id: contactChallenge});
    contactChallenge = null; $('contact-code').hidden = true; $('contact-code').reset(); await refresh(); flash('联系方式已验证并绑定，其他设备已退出。');
  });
  $('logout').addEventListener('click', () => run($('logout'), async () => {await api('/api/v1/logout', 'POST'); await refresh(); flash('已退出登录。');}));
  $('revoke-others').addEventListener('click', () => run($('revoke-others'), async () => {await api('/api/v1/sessions', 'DELETE', {others: true}); await refresh(); flash('其他设备已退出。');}));
  async function init() {
    providers = await api('/api/v1/auth/providers');
    document.querySelector('[data-tab="register"]').hidden = !providers.registration;
    const verifiedLogin = providers.phone_verification || providers.email_verification;
    for (const id of ['otp', 'recover']) document.querySelector(`[data-tab="${id}"]`).hidden = !verifiedLogin;
    $('contact').hidden = !verifiedLogin;
    if (!verifiedLogin) $('contact-hint').textContent = '网站尚未开启验证码送达，暂时无法绑定手机或邮箱。';
    document.querySelectorAll('.channels').forEach(select => {
      for (const channel of ['phone', 'email']) if (providers[channel + '_verification']) {
        const option = document.createElement('option'); option.value = channel; option.textContent = channel === 'phone' ? '手机' : '邮箱'; select.append(option);
      }
    });
    for (const p of providers.providers) $('provider-login').append(button('通过 ' + p.name + ' 登录', () => oauth(p.id)));
    await refresh();
  }
  init().catch(error => flash('无法加载账户页面：' + error.message, true));
})();
