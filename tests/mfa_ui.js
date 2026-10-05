// Execute the shipped enrollment script against the real HTML selectors.
// Run: node tests/mfa_ui.js
const assert = require('node:assert/strict');
const {readFileSync} = require('node:fs');
const {resolve} = require('node:path');
const {webcrypto, createHash} = require('node:crypto');
const vm = require('node:vm');

class Element {
  constructor(tag = 'DIV') {
    this.tagName = tag.toUpperCase();this.hidden = false;this.disabled = false;
    this.value = '';this.textContent = '';this.dataset = {};this.attrs = {};
    this.listeners = {};this.classList = {toggle() {}};this.elements = {};
  }
  addEventListener(type, handler) {this.listeners[type] = handler;}
  removeAttribute(name) {delete this.attrs[name];}
  reset() {for (const input of Object.values(this.elements)) input.value = '';}
  focus() {this.focused = true;}
}

function fixture(admin) {
  const html = readFileSync(resolve(__dirname, '..', admin ? 'page/admin/mfa.html' : 'wwwroot/account/index.html'), 'utf8');
  const nodes = new Map(), controls = [];
  for (const tag of html.matchAll(/<(\w+)\b([^>]*)>/g)) {
    const element = new Element(tag[1]);
    for (const attr of tag[2].matchAll(/\b(data-mfa-[a-z-]+)(?:="([^"]*)")?/g)) {
      const selector = '[' + attr[1] + (attr[2] === undefined ? '' : '="' + attr[2] + '"') + ']';
      nodes.set(selector, element);
      if (attr[1] === 'data-mfa-action') element.dataset.mfaAction = attr[2];
      if (element.tagName === 'BUTTON' && !controls.includes(element)) controls.push(element);
    }
  }
  const root = nodes.get('[data-mfa-root]');assert.ok(root);
  const q = selector => {assert.ok(nodes.has(selector), 'missing real HTML selector ' + selector);return nodes.get(selector);};
  const actions = [...nodes].filter(([key]) => key.startsWith('[data-mfa-action=')).map(([, node]) => node);
  root.dataset.realm = admin ? 'admin' : 'member';root.querySelector = q;
  root.querySelectorAll = selector => selector === 'button' ? controls : selector === '[data-mfa-action]' ? actions : [];
  const form = q('[data-mfa-setup]');form.elements.code = new Element('INPUT');
  const events = [], requests = [], downloads = [], blobs = new Map(), timers = new Map();
  const state = {enabled: false, fail: false}, base = admin ? '/admin/auth/mfa' : '/api/v1/profile/mfa';
  const recovery = Array.from({length: 10}, (_, i) => ('' + i).repeat(8) + '-abcdef01-abcdef01-abcdef01');
  let serial = 0;
  const context = vm.createContext({
    console, crypto: webcrypto, TextEncoder, Uint8Array, Blob, AbortController, Event,
    confirm: () => true,
    FormData: class {constructor(form) {this.form = form;}get(key) {return this.form.elements[key].value;}},
    setTimeout(fn) {const id = ++serial;timers.set(id, fn);return id;}, clearTimeout(id) {timers.delete(id);},
    URL: {createObjectURL(blob) {const id = 'blob:test-' + ++serial;blobs.set(id, blob);return id;}, revokeObjectURL() {}},
    document: {
      cookie: 'MCSRF=' + 'd'.repeat(64),
      querySelector: selector => selector === '[data-mfa-root]' ? root : null,
      createElement(tag) {const anchor = new Element(tag);anchor.click = () => downloads.push(anchor.href);return anchor;}
    },
    window: {addEventListener() {}, dispatchEvent(event) {events.push(event.type);}},
    async fetch(path, options) {
      const data = options.body ? JSON.parse(options.body) : null;requests.push({path, ...options, data});
      if (state.fail && options.method === 'POST') return {ok: false, status: 500, json: async () => ({code: 500, msg: 'MFA unavailable'})};
      let result;
      if (path === base) result = {enabled: state.enabled, username: 'Hash_测试🔑', recovery_codes_remaining: 10, csrf_token: (state.enabled ? 'b' : 'a').repeat(64)};
      else if (path.endsWith('/setup')) result = {setup_id: 'c'.repeat(64), secret: 'ABCDEFGHIJKLMNOP234567', qr_svg: '<svg xmlns="http://www.w3.org/2000/svg"/>', expires_in: 600};
      else if (path.endsWith('/confirm')) {state.enabled = true;result = {enabled: true, recovery_codes: recovery};}
      else if (path.endsWith('/recovery-codes')) result = {enabled: true, recovery_codes: recovery};
      else if (path.endsWith('/disable')) {state.enabled = false;result = {enabled: false};}
      else result = null;
      return {ok: true, status: 200, json: async () => ({code: 0, data: result})};
    }
  });
  vm.runInContext(readFileSync(resolve(__dirname, '../wwwroot/account/mfa.js'), 'utf8'), context);
  return {q, actions, controls, form, requests, downloads, blobs, events, state, recovery, timers, base};
}

async function settle() {for (let i = 0; i < 24; i++) await new Promise(resolve => setImmediate(resolve));}
async function main() {
  for (const admin of [false, true]) {
    const ui = fixture(admin);await settle();
    const action = name => ui.actions.find(node => node.dataset.mfaAction === name).listeners.click();
    assert.match(ui.q('[data-mfa-status]').textContent, /尚未启用/);
    const password = '  密碼🔐  ';ui.q('[data-mfa-password]').value = password;
    await action('setup');await settle();
    const setupRequest = ui.requests.find(request => request.path.endsWith('/setup'));
    const expected = admin ? createHash('sha256').update('Hash_测试🔑_xywhsoft_' + password).digest('hex') : password;
    assert.equal(setupRequest.data.password, expected);
    assert.equal(setupRequest.headers['X-CSRF-Token'], (admin ? 'a' : 'd').repeat(64));
    assert.equal(ui.q('[data-mfa-password]').value, '');
    assert.equal(ui.form.hidden, false);assert.equal(ui.form.elements.code.focused, true);
    assert.match(ui.q('[data-mfa-qr]').src, /^blob:/);
    assert.match(await ui.blobs.get(ui.q('[data-mfa-qr]').src).text(), /^<svg/);
    const qrBefore = ui.q('[data-mfa-qr]').src;
    await action('reauth');await settle();
    assert.equal(ui.form.hidden, false, 'reauth must preserve an unfinished enrollment');
    assert.equal(ui.q('[data-mfa-qr]').src, qrBefore);
    ui.form.elements.code.value = '123456';
    let prevented = false;
    ui.form.listeners.submit({target: ui.form, preventDefault() {prevented = true;}});await settle();
    assert.equal(prevented, true);
    assert.match(ui.q('[data-mfa-status]').textContent, /已启用/);
    assert.equal(ui.form.hidden, true);assert.equal(ui.q('[data-mfa-secret]').value, '');
    assert.equal(ui.q('[data-mfa-recovery-text]').textContent.split('\n').length, 10);
    ui.q('[data-mfa-download]').listeners.click();
    const downloaded = await ui.blobs.get(ui.downloads[0]).text();
    assert.ok(downloaded.includes(ui.recovery.join('\n')));assert.ok(!downloaded.includes('\\n'));
    if (!admin) assert.ok(ui.events.includes('xadmin:mfa-changed'));
    await action('recovery-codes');await settle();
    const last = ui.requests.filter(request => request.path.endsWith('/recovery-codes')).at(-1);
    assert.equal(last.headers['X-CSRF-Token'], (admin ? 'b' : 'd').repeat(64));
    ui.state.fail = true;await action('recovery-codes');await settle();
    assert.match(ui.q('[data-mfa-flash]').textContent, /暂时不可用/);
    assert.ok(ui.controls.every(control => !control.disabled), 'failure must release the submit lock');
    ui.q('[data-mfa-clear-recovery]').listeners.click();
    assert.equal(ui.q('[data-mfa-recovery-text]').textContent, '');
    assert.equal(ui.q('[data-mfa-recovery]').hidden, true);
    console.log('PASS', admin ? 'backend' : 'frontend', 'real HTML selectors, primary proof, local QR, confirmation, CSRF rotation, recovery download, clearing and request failure');
  }
}
main().catch(error => {console.error(error);process.exitCode = 1;});
