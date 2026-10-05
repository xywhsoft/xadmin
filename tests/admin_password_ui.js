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

function page(kind, crypto = webcrypto, failRequest = false, mfaLogin = false) {
  const handlers = {};
  const requests = [], messages = [], closed = [];
  const button = { disabled: false, classList: { add() {}, remove() {} } };
  const inputs = Object.fromEntries(['username', 'password', 'mfaCode'].map(name => [name, {
    value: name === 'password' ? password : '', disabled: false, dataset: {}, attrs: {},
    getAttribute(key) { return this.attrs[key]; }, setAttribute(key, value) { this.attrs[key] = value; },
    removeAttribute(key) { delete this.attrs[key]; }, focus() {}
  }]));
  const mfaPane = { hidden: true };
  const restart = { addEventListener(name, fn) { handlers['restart:' + name] = fn; } };
  const form = { on(name, fn) { handlers[name] = fn; }, render() {}, verify() {} };
  const table = { on: form.on, render() {}, reloadData() {} };
  const layer = {
    msg(message) { messages.push(message); }, load() { return 91; },
    close(id) { closed.push(id); }, getFrameIndex() { return 92; },
    prompt(options, fn) { handlers.prompt = fn; }
  };
  const jquery = () => ({ on() { return this; } });
  const layui = { form, table, layer, jquery, $: jquery, use(names, fn) { fn(); } };
  const context = vm.createContext({
    TextEncoder, Uint8Array, crypto, console, layui, layer,
    document: {
      getElementById(id) { return id === 'mfaLogin' ? mfaPane : id === 'mfaRestart' ? restart : button; },
      querySelectorAll() { return [inputs.username, inputs.password]; },
      querySelector(selector) { return inputs[/name="([^"]+)"/.exec(selector)[1]]; }, title: ''
    },
    window: { crypto, location: { pathname: '/private-admin-entry' } },
    parent: { layui, layer },
    async fetch(url, options = {}) {
      if (url.startsWith('/brand/admin')) return { json: async () => ({ result: false }) };
      requests.push({ url, ...options, data: JSON.parse(options.body) });
      if (failRequest) throw new Error('test request failure');
      return { json: async () => (mfaLogin && requests.length === 1
        ? { result: false, mfa_required: true, challenge_id: 'c'.repeat(64) }
        : { result: true, message: 'test success' }) };
    }
  });
  const source = readFileSync(resolve(__dirname, '..', files[kind]), 'utf8');
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
  return { context, requests, messages, closed, button, submit, field, inputs, mfaPane };
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
  const mfa = page('login', webcrypto, false, true);
  mfa.submit();await settle(mfa, 'login');
  assert.equal(mfa.mfaPane.hidden, false);
  assert.equal(mfa.inputs.password.value, '');
  assert.equal(mfa.inputs.username.disabled, true);
  assert.equal(mfa.inputs.password.disabled, true);
  mfa.field.mfaCode = 'abcdef01-abcdef01-abcdef01-abcdef01';
  mfa.submit();await settle(mfa, 'login');
  assert.deepEqual(mfa.requests[1].data, {
    challenge_id: 'c'.repeat(64), code: mfa.field.mfaCode
  });
  assert.equal(mfa.context.window.location.href, '/admin');
  console.log('PASS backend MFA handoff, password clearing, recovery-code submission and login redirect');

}

main().catch(error => { console.error(error); process.exitCode = 1; });
