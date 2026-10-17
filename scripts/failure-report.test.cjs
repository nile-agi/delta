const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
function api(invoke) {
  const window = { addEventListener() {}, __TAURI_INTERNALS__: { invoke }, location: { href: 'tauri://localhost/' } };
  vm.runInNewContext(fs.readFileSync('web/app/static/failure-report.js', 'utf8'), { window, navigator: { userAgent: 'test-webview' }, document: {}, console, setTimeout, clearTimeout, URL, Blob });
  return window.__DELTA_DIAGNOSTICS__;
}
test('removes credentials and paths including Windows names', () => {
  const report = api();
  const clean = report.redact('Load /home/Asha/private.gguf C:\\Users\\Asha\\model.gguf\nAuthorization: Bearer abc123\nhttps://user:pass@host/a?token=secret');
  for (const secret of ['Asha','abc123','user:pass','token=secret']) assert.ok(!clean.includes(secret));
});
test('a long Unicode report produces a bounded URL pointing only to public release issues', () => {
  const report = api();
  const url = report.issueUrl('version: 1.0.18\n' + 'error 😀'.repeat(10000));
  assert.ok(url.length <= 1800);
  assert.equal(new URL(url).origin, 'https://github.com');
  assert.equal(new URL(url).pathname, '/nile-agi/delta-release/issues/new');
  assert.match(new URL(url).searchParams.get('body'), /Full report/);
});
test('collecting a report does not open a browser or submit an issue', async () => {
  const calls = [];
  const report = api(async (command, args) => { calls.push(command); return {version:'1.0.18',os:'Mint',stage:args.stage,message:'Model load failed',logs:[]}; });
  const text = await report.collect({ stage:'model-load', model:'gemma3:1b', message:'Failed' });
  assert.match(text, /1.0.18/);
  assert.deepEqual(calls, ['get_diagnostic_report']);
});
test('missing IPC still gives a sanitized frontend fallback report', async () => {
  const report = api(async () => { throw Error('bridge unavailable'); });
  const text = await report.collect({ stage:'frontend',message:'Error loading /Users/Asha/app.js' });
  assert.match(text, /unavailable/);
  assert.ok(!text.includes('Asha'));
});
test('quoted personal paths with spaces do not expose a surname', () => {
  const report = api();
  for (const path of ['"C:\\Users\\Asha Smith\\models\\x.gguf"', '"/Users/Asha Smith/models/x.gguf"']) {
    const clean = report.redact('Load failed: ' + path);
    assert.ok(!clean.includes('Asha') && !clean.includes('Smith'), clean);
  }
});
