'use strict';
(() => {
  const root = document.querySelector('[data-mfa-root]'); if (!root) return;
  const q = selector => root.querySelector(selector);
  const admin = root.dataset.realm === 'admin', base = admin ? '/admin/auth/mfa' : '/api/v1/profile/mfa';
  let state = null, csrf = '', setup = null, qrURL = null, timer = null, recovery = [], busy = false;
  let focusPending = !admin && location.hash === '#two-factor-auth';
  function flash(text, error = false) {q('[data-mfa-flash]').textContent = text; q('[data-mfa-flash]').hidden = false; q('[data-mfa-flash]').classList.toggle('error', error);}
  function clearSetup() {
    setup = null; clearTimeout(timer); q('[data-mfa-setup]').hidden = true; q('[data-mfa-setup]').reset();
    q('[data-mfa-secret]').value = ''; q('[data-mfa-qr]').removeAttribute('src');
    if (qrURL) URL.revokeObjectURL(qrURL); qrURL = null;
  }
  function clearRecovery() {recovery = []; q('[data-mfa-recovery-text]').textContent = ''; q('[data-mfa-recovery]').hidden = true;}
  async function api(path, method = 'GET', data) {
    const token = admin ? csrf : document.cookie.split(';').map(s => s.trim()).find(s => s.startsWith('MCSRF='))?.slice(6);
    const headers = {Accept: 'application/json'};
    if (data !== undefined) headers['Content-Type'] = 'application/json';
    if (method !== 'GET' && token) headers['X-CSRF-Token'] = token;
    const controller = new AbortController(), timeout = setTimeout(() => controller.abort(), 15000);
    let response, result;
    try {
      response = await fetch(path, {method, headers, credentials: 'same-origin', signal: controller.signal,
        ...(data === undefined ? {} : {body: JSON.stringify(data)})});
      result = await response.json();
    } catch (error) {
      if (error.name === 'AbortError') throw new Error('请求超时，请重试。');
      throw error;
    } finally {clearTimeout(timeout);}
    if (!response.ok || result.code !== 0) {
      const messages = {
        'confirm your identity again': '身份确认已过期，请先重新确认当前身份。',
        'confirm your identity and MFA again': '请验证当前密码或原登录方式，并提供当前验证器验证码或恢复码。',
        'verification invalid or expired': '验证码无效或已过期，请使用验证器中的新验证码。',
        'setup invalid or expired': '绑定已过期或尝试次数过多，请重新开始绑定。',
        'too many verification attempts': '验证尝试过多，请稍后再试。',
        'CSRF verification failed': '页面凭据已变化，请刷新后重试。',
        'MFA unavailable': '两步验证暂时不可用，请稍后重试。'
      };
      const error = new Error(messages[result.msg] || result.msg || '请求失败'); error.status = response.status; throw error;
    }
    return result.data;
  }
  async function refresh() {
    try {state = await api(base);}
    catch (error) {if (error.status === 401) {root.hidden = true;clearSetup();clearRecovery();return;}throw error;}
    root.hidden = false; csrf = state.csrf_token || '';
    if (focusPending) {
      const member = document.getElementById('member');
      if (member && !member.hidden) {root.scrollIntoView({block: 'start'});focusPending = false;}
    }
    q('[data-mfa-status]').textContent = state.enabled
      ? '已启用。所有登录方式都需要两步验证，剩余恢复码 ' + state.recovery_codes_remaining + ' 个。'
      : '尚未启用。绑定后，每次登录需要验证器验证码或一次性恢复码。';
    q('[data-mfa-old-code-label]').hidden = !state.enabled;
    q('[data-mfa-action="setup"]').textContent = state.enabled ? '更换验证器' : '绑定验证器';
    for (const action of ['disable', 'recovery-codes']) q('[data-mfa-action="' + action + '"]').hidden = !state.enabled;
  }
  async function proof() {
    let password = q('[data-mfa-password]').value;
    if (admin && password) {
      if (!crypto.subtle) throw new Error('密码哈希需要 HTTPS 或 localhost 环境。');
      const digest = await crypto.subtle.digest('SHA-256', new TextEncoder().encode(state.username + '_xywhsoft_' + password));
      password = Array.from(new Uint8Array(digest), byte => byte.toString(16).padStart(2, '0')).join('');
    }
    return {password, code: q('[data-mfa-code]').value.trim()};
  }
  async function run(action) {
    if (busy) return; busy = true;
    const controls = [...root.querySelectorAll('button')]; controls.forEach(node => node.disabled = true);
    try {await action();}
    catch (error) {flash(error.message, true);}
    finally {
      q('[data-mfa-password]').value = '';q('[data-mfa-code]').value = '';
      controls.forEach(node => node.disabled = false);busy = false;
    }
  }
  function showRecovery(data) {
    clearRecovery();
    if (data?.recovery_codes) {recovery = data.recovery_codes.slice();q('[data-mfa-recovery-text]').textContent = recovery.join('\n');q('[data-mfa-recovery]').hidden = false;}
  }
  root.querySelectorAll('[data-mfa-action]').forEach(button => button.addEventListener('click', () => run(async () => {
    const action = button.dataset.mfaAction;
    if (action === 'disable' && !confirm('关闭两步验证后，登录将只验证原登录凭据。继续吗？')) return;
    if (action === 'recovery-codes' && !confirm('生成新恢复码后，全部旧恢复码都会失效。继续吗？')) return;
    const data = await api(base + '/' + action, 'POST', await proof());
    if (action === 'setup') {
      clearSetup();clearRecovery();setup = data.setup_id; q('[data-mfa-secret]').value = data.secret;
      qrURL = URL.createObjectURL(new Blob([data.qr_svg], {type: 'image/svg+xml'})); q('[data-mfa-qr]').src = qrURL;
      q('[data-mfa-setup]').hidden = false; q('[data-mfa-setup]').elements.code.focus();
      timer = setTimeout(() => {clearSetup();flash('绑定已过期，请重新开始。', true);}, data.expires_in * 1000);
      flash('扫描二维码，再输入新验证器的六位验证码。原绑定会保留到确认完成。');
    } else {
      if (action !== 'reauth') {clearSetup();showRecovery(data);}
      await refresh();
      if (!admin) window.dispatchEvent(new Event('xadmin:mfa-changed'));
      flash(action === 'disable' ? '两步验证已关闭，旧登录凭据已撤销。' : action === 'reauth' ? '身份已确认。' : '新恢复码已生成，请立即保存。');
    }
  })));
  q('[data-mfa-setup]').addEventListener('submit', event => {
    event.preventDefault(); run(async () => {
      if (!setup) throw new Error('请重新开始绑定。');
      const data = await api(base + '/confirm', 'POST', {setup_id: setup, code: new FormData(event.target).get('code')});
      clearSetup();showRecovery(data);await refresh();
      if (!admin) window.dispatchEvent(new Event('xadmin:mfa-changed'));
      flash('两步验证已启用，旧登录凭据已撤销。请保存恢复码。');
    });
  });
  q('[data-mfa-clear-setup]').addEventListener('click', clearSetup);
  q('[data-mfa-clear-recovery]').addEventListener('click', clearRecovery);
  q('[data-mfa-download]').addEventListener('click', () => {
    if (!recovery.length) return;
    const url = URL.createObjectURL(new Blob(['xAdmin ' + root.dataset.realm + ' 恢复码\n' + recovery.join('\n') + '\n每个只能使用一次，请离线妥善保存。'], {type: 'text/plain;charset=utf-8'}));
    const anchor = document.createElement('a');anchor.href = url;anchor.download = 'xadmin-' + root.dataset.realm + '-recovery-codes.txt';anchor.click();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
  });
  window.addEventListener('xadmin:account', () => refresh().catch(error => flash(error.message, true)));
  window.addEventListener('pagehide', () => {clearSetup();clearRecovery();});
  refresh().catch(error => flash(error.message, true));
})();
