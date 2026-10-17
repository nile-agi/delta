const { test } = require('node:test');
const assert = require('node:assert/strict');
const vm = require('node:vm');
const fs = require('node:fs');

function boot(status) {
  let now = 0, id = 0;
  const timers = new Map(), events = {};
  function element(tag) { return { tag, children: [], style: {}, appendChild(child) { this.children.push(child); child.parent = this; }, remove() { if (this.parent) this.parent.children = this.parent.children.filter(c => c !== this); }, setAttribute() {} }; }
  const splash = element('div'); splash.id = 'app-loading';
  const text = element('p'); splash.appendChild(text);
  const document = { documentElement: { classList: { contains: () => false } },
    createElement: element, getElementById: name => name === 'app-loading' ? splash : null,
    querySelector: () => text };
  const window = { location: { hash: '#/', pathname: '/', search: '' },
    addEventListener: (name, fn) => { (events[name] ||= []).push(fn); },
    dispatchEvent: event => (events[event.type] || []).forEach(fn => fn(event)) };
  if (status) window.__TAURI_INTERNALS__ = { invoke: status };
  const context = { window, document, history: { replaceState() {} }, Date: class extends Date { static now() { return now; } },
    CustomEvent: function(type, init) { return { type, detail: init?.detail }; },
    setTimeout: (fn, ms) => { timers.set(++id, { fn, at: now + ms }); return id; }, clearTimeout: id => timers.delete(id) };
  vm.runInNewContext(fs.readFileSync('web/app/static/startup-guard.js', 'utf8'), context);
  return { window, splash, async advance(ms) { const target = now + ms; for (;;) { const entry = [...timers].filter(([,t]) => t.at <= target).sort((a,b) => a[1].at-b[1].at)[0]; if (!entry) break; now = entry[1].at; timers.delete(entry[0]); entry[1].fn(); for (let i=0;i<5;i++) await Promise.resolve(); } now = target; } };
}

test('a rejected native bridge becomes a visible bounded startup failure', async () => {
  const app = boot(() => Promise.reject(new Error('IPC unavailable')));
  await app.advance(31000);
  assert.match(app.window.__DELTA_STARTUP__.failure.message, /start|communicat/i);
  assert.equal(app.splash.style.opacity, '1');
});
test('backend termination shows its reason immediately', async () => {
  const app = boot(() => Promise.resolve([8080,8081,false,true,'Backend terminated: signal 4']));
  await app.advance(500);
  assert.match(app.window.__DELTA_STARTUP__.failure.message, /signal 4/);
});
test('mounting cancels the startup watchdog', async () => {
  const app = boot();
  app.window.__DELTA_STARTUP__.mounted();
  await app.advance(31000);
  assert.equal(app.window.__DELTA_STARTUP__.failure, null);
});
test('an early JavaScript exception is shown even if the app never mounts', () => {
  const app = boot();
  app.window.dispatchEvent({ type: 'error', message: 'Unsupported JavaScript syntax', error: new Error('syntax') });
  assert.match(app.window.__DELTA_STARTUP__.failure.message, /Unsupported JavaScript/);
});
