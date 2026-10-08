// Run with: node tests/admin_password_ui.js
// Execute the actual page scripts with native Web Crypto and minimal UI stubs.
// No server, credentials, third-party packages or browser profile are required.
const assert = require('node:assert/strict');
const { readFileSync } = require('node:fs');
const { resolve } = require('node:path');
const { createHash, webcrypto } = require('node:crypto');
const vm = require('node:vm');

const files = {
  install: 'page/install.html',
  login: 'page/admin/login.html',
  add: 'template/auth/user_add.html',
  reset: 'page/auth/user.html'
};
const username = 'Hash_测试🔑';
const password = '  Password_密碼🔐  ';
const expected = createHash('sha256').update(username + '_xywhsoft_' + password).digest('hex');

function page(kind, crypto = webcrypto, failRequest = false, mfaLogin = false, mfaReplies = []) {
  const handlers = {};
  const requests = [], messages = [], closed = [];
  const button = { disabled: false, classList: { add() {}, remove() {} } };
  const source = readFileSync(resolve(__dirname, '..', files[kind]), 'utf8');
  const inputs = Object.fromEntries(['username', 'password'].map(name => [name, {
    value: name === 'password' ? password : username, disabled: false,
    attrs: {'lay-verify': name === 'password' ? 'required|confirmPassword' : 'required'},
    focus() { this.focused = true; }
  }]));
  const modalNodes = new Map(), controls = [];
  const template = /<template id="mfaLoginTemplate">([\s\S]*?)<\/template>/.exec(source)?.[1];
  const modal = { options: null, closed: false, attrs: {}, listeners: {} };
  function element() {
    return {
      value: '', disabled: false, textContent: '', attrs: {}, listeners: {},
      classList: { toggle() {} },
      setAttribute(key, value) { this.attrs[key] = value; },
      addEventListener(type, handler) { this.listeners[type] = handler; },
      focus() { this.focused = true; }
    };
  }
  if (template) {
    for (const tag of template.matchAll(/<(\w+)\b([^>]*)>/g)) {
      const attr = /\b(data-mfa-[a-z-]+)/.exec(tag[2]);
      if (!attr) continue;
      const node = element();modalNodes.set('[' + attr[1] + ']', node);
      if (tag[1] === 'button') controls.push(node);
    }
  }
  const q = selector => {assert.ok(modalNodes.has(selector), 'missing shipped modal selector ' + selector);return modalNodes.get(selector);};
  modal.querySelector = q;
  modal.querySelectorAll = selector => selector === 'button' ? controls : [q('[data-mfa-code]'), ...controls];
  modal.setAttribute = (key, value) => { modal.attrs[key] = value; };
  modal.addEventListener = (type, handler) => { modal.listeners[type] = handler; };
  const form = { on(name, fn) { handlers[name] = fn; }, render() {}, verify() {} };
  const table = { on: form.on, render() {}, reloadData() {} };
  const layer = {
    msg(message) { messages.push(message); }, load() { return 91; },
    close(id) {
      closed.push(id);
      if (id === 94 && !modal.closed) {modal.closed = true;modal.options.end();}
    }, getFrameIndex() { return 92; },
    open(options) {
      modal.options = options;modal.closed = false;
      assert.equal(options.content, template);
      options.success([modal], 94);return 94;
    },
    prompt(options, fn) { handlers.prompt = fn; }
  };
  const jquery = () => ({ on() { return this; } });
  const layui = { form, table, layer, jquery, $: jquery, use(names, fn) { fn(); } };
  const context = vm.createContext({
    TextEncoder, Uint8Array, crypto, console, layui, layer, AbortController, setTimeout, clearTimeout,
    document: {
      getElementById(id) { return id === 'mfaLoginTemplate' ? {innerHTML: template} : button; },
      querySelectorAll() { return [inputs.username, inputs.password]; },
      querySelector(selector) { return inputs[/name="([^"]+)"/.exec(selector)[1]]; }, title: ''
    },
    window: { crypto, location: { pathname: '/private-admin-entry' } },
    parent: { layui, layer },
    async fetch(url, options = {}) {
      if (url.startsWith('/brand/admin')) return { json: async () => ({ result: false }) };
      requests.push({ url, ...options, data: JSON.parse(options.body) });
      if (failRequest) throw new Error('test request failure');
      const data = requests.at(-1).data;
      let result = { result: true, message: 'test success' };
      if (mfaLogin && !data.challenge_id) result = { result: false, mfa_required: true, challenge_id: 'c'.repeat(64) };
      else if (data.challenge_id && !data.action && mfaReplies.length) {
        result = mfaReplies.shift();
        if (result instanceof Error) throw result;
        if (typeof result === 'function') result = await result();
      }
      return { json: async () => result };
    }
  });
  // text/html scripts contain Layui templates, not executable JavaScript.
  for (const match of source.matchAll(/<script\b([^>]*)>([\s\S]*?)<\/script>/g)) {
    if (!/\b(src|type)\s*=/.test(match[1])) vm.runInContext(match[2], context, { filename: files[kind] });
  }
  if (kind !== 'reset') {
    assert.match(source, /<form\b[^>]*onsubmit="return false;"/, 'native form submission must stay blocked');
  }
  const field = { username, password, role: '1', authLevel: '0', remember: 'on' };
  function submit() {
    if (kind === 'reset') return handlers.prompt(password, 93);
    // Layui only cancels the browser default on a synchronous false result.
    const result = handlers[{login: 'submit(login)', install: 'submit(install)', add: 'submit(submit)'}[kind]]({ field, elem: button });
    assert.equal(result, false, 'async callback failed to cancel native form submission');
    return result;
  }
  if (kind === 'reset') handlers['tool(Table_Auth_User)']({ event: 'resetPwd', data: { id: 7, user: username } });
  function submitMFA(value) {
    if (value !== undefined) q('[data-mfa-code]').value = value;
    let prevented = false;
    const pending = q('[data-mfa-form]').listeners.submit({preventDefault() {prevented = true;}});
    assert.equal(prevented, true);return pending;
  }
  function modalClick(selector) { const node = q(selector);return node.listeners.click.call(node); }
  function closeModal() {if (modal.options.cancel() !== false) layer.close(94);}
  return { context, requests, messages, closed, button, submit, field, inputs, modal, q, submitMFA, modalClick, closeModal };
}

async function settle(ui, kind) {
  const deadline = Date.now() + 3000;
  while (kind === 'reset' ? ui.messages.length === 0 : ui.button.disabled) {
    assert.ok(Date.now() < deadline, 'password operation did not settle');
    await new Promise(resolve => setImmediate(resolve));
  }
}

async function main() {
  for (const kind of Object.keys(files)) {
    // Hold the digest open to deterministically exercise the original race.
    let release;
    const gate = new Promise(resolve => { release = resolve; });
    const ui = page(kind, { subtle: { async digest(...args) { await gate; return webcrypto.subtle.digest(...args); } } });
    ui.submit();
    ui.submit();
    assert.equal(ui.requests.length, 0, 'request sent before hashing finished');
    release();
    await settle(ui, kind);
    assert.equal(ui.requests.length, 1, 'duplicate submit sent another request');
    const request = ui.requests[0];
    assert.equal(request.method, 'POST');
    assert.equal(request.data.username, username);
    assert.equal(request.data.password, expected, 'client hash must match the legacy UTF-8/lowercase contract');
    assert.equal(request.url, { install: '/', login: '/private-admin-entry', add: '/admin/auth/user', reset: '/admin/auth/user/repwd' }[kind]);
    if (kind === 'install') {
      assert.equal(ui.context.window.location.href, '/admin/login');
      assert.ok(ui.closed.includes(91), 'installation spinner stayed open');
    }
    if (kind === 'login') {
      assert.equal(ui.context.window.location.href, '/admin');
      assert.ok(ui.closed.includes(91), 'login spinner stayed open');
    }
    for (const crypto of [{}, { subtle: { async digest() { throw new Error('test digest failure'); } } }]) {
      const failed = page(kind, crypto);
      await failed.submit();
      await settle(failed, kind);
      assert.equal(failed.requests.length, 0, 'hash failure sent a request');
      assert.ok(failed.messages.some(message => /HTTPS|test digest failure/.test(message)), 'hash failure was silent');
      // A failure must release the lock so the user can retry in the same page.
      await failed.submit();
      await settle(failed, kind);
      assert.equal(failed.messages.length, 2, 'hash failure prevented retry');
    }
    const failedRequest = page(kind, webcrypto, true);
    await failedRequest.submit();
    await settle(failedRequest, kind);
    assert.ok(failedRequest.messages.includes('test request failure'));
    assert.equal(failedRequest.button.disabled, false, 'request failure left the button disabled');
    console.log('PASS', kind, 'UTF-8 hash, synchronous cancellation, duplicate submit, hash/request failure and retry');
  }
  const mfa = page('login', webcrypto, false, true, [
    {code: 401, msg: 'verification invalid, reused or expired'},
    {result: true}
  ]);
  mfa.submit();await settle(mfa, 'login');
  assert.ok(mfa.modal.options, 'password proof must open a dialog');
  assert.equal(mfa.modal.options.shadeClose, false);
  assert.equal(mfa.modal.attrs.role, 'dialog');
  assert.equal(mfa.modal.attrs['aria-modal'], 'true');
  assert.equal(mfa.q('[data-mfa-code]').focused, true);
  assert.equal(mfa.inputs.password.value, '');
  for (const name of ['username', 'password']) {
    assert.equal(mfa.inputs[name].disabled, false, 'original login fields must stay intact');
    assert.ok(mfa.inputs[name].attrs['lay-verify'].includes('required'));
  }
  const loginHtml = readFileSync(resolve(__dirname, '..', files.login), 'utf8');
  assert.ok(!loginHtml.includes('id="mfaLogin"'), 'MFA must not be inserted into the password form');
  assert.match(loginHtml, /id="loginBtn"[^>]*>登 录<\/button>/);
  mfa.submit();assert.equal(mfa.requests.length, 1, 'background login must not replace an open challenge');
  await mfa.submitMFA('123');assert.equal(mfa.requests.length, 1);
  assert.match(mfa.q('[data-mfa-error]').textContent, /6 位/);
  await mfa.submitMFA('123456');
  assert.deepEqual(mfa.requests[1].data, {challenge_id: 'c'.repeat(64), code: '123456'});
  assert.equal(mfa.modal.closed, false, 'invalid code must allow retry in the same dialog');
  assert.match(mfa.q('[data-mfa-error]').textContent, /已使用或已过期/);
  assert.equal(mfa.q('[data-mfa-submit]').disabled, false);
  mfa.modalClick('[data-mfa-switch]');
  assert.equal(mfa.q('[data-mfa-code]').maxLength, 40);
  assert.equal(mfa.q('[data-mfa-code]').attrs.inputmode, 'text');
  assert.equal(mfa.q('[data-mfa-code]').value, '');
  const recoveryCode = 'abcdef01-abcdef01-abcdef01-abcdef01';
  await mfa.submitMFA('  ' + recoveryCode + '  ');
  assert.deepEqual(mfa.requests[2].data, {challenge_id: 'c'.repeat(64), code: recoveryCode});
  assert.equal(mfa.context.window.location.href, '/admin');
  assert.equal(mfa.modal.closed, true);
  assert.ok(!mfa.requests.some(request => request.data.action === 'cancel'), 'successful proof must not be cancelled');
  console.log('PASS backend modal handoff, original layout, inline retry, recovery-code switch and login redirect');

  for (const how of ['button', 'close', 'escape']) {
    const cancelled = page('login', webcrypto, false, true);
    cancelled.submit();await settle(cancelled, 'login');
    if (how === 'button') cancelled.modalClick('[data-mfa-cancel]');
    else if (how === 'close') cancelled.closeModal();
    else cancelled.modal.listeners.keydown({key: 'Escape', preventDefault() {}});
    assert.equal(cancelled.modal.closed, true);
    assert.deepEqual(cancelled.requests[1].data, {challenge_id: 'c'.repeat(64), action: 'cancel'});
    assert.equal(cancelled.inputs.password.focused, true);
    cancelled.submit();await settle(cancelled, 'login');
    assert.equal(cancelled.requests.length, 3, 'cancelled flow must permit a fresh password login');
  }
  const expired = page('login', webcrypto, false, true, [{code: 401, msg: 'MFA challenge invalid or expired'}]);
  expired.submit();await settle(expired, 'login');await expired.submitMFA('123456');
  assert.equal(expired.modal.closed, true);assert.match(expired.messages[0], /重新输入密码/);
  assert.equal(expired.context.window.location.href, undefined);

  let release;
  const gate = new Promise(resolve => {release = resolve;});
  const retry = page('login', webcrypto, false, true, [new Error('network error'), async () => {
    await gate;return {result: true};
  }]);
  retry.submit();await settle(retry, 'login');await retry.submitMFA('123456');
  assert.match(retry.q('[data-mfa-error]').textContent, /检查网络后重试/);
  assert.equal(retry.q('[data-mfa-submit]').disabled, false);
  const pending = retry.submitMFA('654321');
  await retry.submitMFA();retry.closeModal();
  assert.equal(retry.requests.length, 3, 'in-flight MFA must not duplicate its request');
  assert.equal(retry.modal.closed, false, 'in-flight proof must not be cancelled');
  release();await pending;
  assert.equal(retry.context.window.location.href, '/admin');
  console.log('PASS modal cancellation, expiry, network retry and in-flight submit lock');

}

main().catch(error => { console.error(error); process.exitCode = 1; });
