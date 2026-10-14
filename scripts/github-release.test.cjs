const assert = require('node:assert/strict');
const { mkdtempSync, writeFileSync, readFileSync } = require('node:fs');
const { tmpdir } = require('node:os');
const { join } = require('node:path');
const { test } = require('node:test');
const { client, uploadAssets, main } = require('./github-release.cjs');

// A stand-in for api.github.com that records each call.
function fakeGitHub(routes) {
  const calls = [];
  const fetchImpl = async (url, init) => {
    const path = url.replace(/^https:\/\/(api|uploads)\.github\.com/, '');
    calls.push({ method: init.method, path, headers: init.headers, body: init.body });
    for (const [pattern, reply] of routes) {
      if (`${init.method} ${path}`.match(pattern)) {
        const { status = 200, data = {} } = typeof reply === 'function' ? reply(init) : reply;
        return { ok: status < 400, status, text: async () => (data === null ? '' : JSON.stringify(data)) };
      }
    }
    return { ok: false, status: 404, text: async () => JSON.stringify({ message: 'Not Found' }) };
  };
  return { calls, fetchImpl };
}

test('requests carry the token and JSON, and errors name the call', async () => {
  const gh = fakeGitHub([[/^GET \/repos\/o\/r\/releases\/7$/, { data: { id: 7 } }]]);
  const github = client('secret', gh.fetchImpl);
  assert.deepEqual((await github.rest.repos.getRelease({ owner: 'o', repo: 'r', release_id: 7 })).data, { id: 7 });
  assert.equal(gh.calls[0].headers.Authorization, 'Bearer secret');
  await assert.rejects(github.rest.repos.getRelease({ owner: 'o', repo: 'r', release_id: 8 }), /GET .*releases\/8 failed \(404\): Not Found/);
  assert.throws(() => client(''), /token is required/);
});

test('paginate keeps reading until a short page', async () => {
  const page = (n, size) => Array.from({ length: size }, (_, i) => ({ id: n * 100 + i }));
  const gh = fakeGitHub([
    [/page=1$/, { data: page(1, 2) }],
    [/page=2$/, { data: page(2, 1) }]
  ]);
  const github = client('t', gh.fetchImpl);
  const all = await github.paginate(github.rest.repos.listReleases, { owner: 'o', repo: 'r', per_page: 2 });
  assert.equal(all.length, 3);
  assert.equal(gh.calls.length, 2);
});

test('owner and repo go in the path, the rest in the body', async () => {
  const gh = fakeGitHub([[/^PATCH \/repos\/o\/r\/releases\/5$/, (init) => ({ data: JSON.parse(init.body) })]]);
  const github = client('t', gh.fetchImpl);
  const { data } = await github.rest.repos.updateRelease({ owner: 'o', repo: 'r', release_id: 5, draft: false });
  assert.deepEqual(data, { draft: false });
});

test('uploading replaces an asset with the same name', async () => {
  const dir = mkdtempSync(join(tmpdir(), 'gh-release-'));
  const file = join(dir, 'delta-cli-linux-x86_64.tar.gz');
  writeFileSync(file, 'archive');
  const gh = fakeGitHub([
    [/^GET \/repos\/o\/r\/releases\/9$/, { data: { assets: [{ id: 41, name: 'delta-cli-linux-x86_64.tar.gz' }] } }],
    [/^DELETE \/repos\/o\/r\/releases\/assets\/41$/, { status: 204, data: null }],
    [/^POST \/repos\/o\/r\/releases\/9\/assets\?name=delta-cli-linux-x86_64\.tar\.gz$/, { status: 201, data: { id: 42 } }]
  ]);
  await uploadAssets({ github: client('t', gh.fetchImpl), repo: { owner: 'o', repo: 'r' }, releaseId: 9, files: [file] });
  assert.deepEqual(gh.calls.map((c) => c.method), ['GET', 'DELETE', 'POST']);
  assert.equal(gh.calls[2].headers['Content-Type'], 'application/octet-stream');
  assert.equal(gh.calls[2].body.toString(), 'archive');
});

test('prepare writes the draft id for later jobs', async () => {
  const out = join(mkdtempSync(join(tmpdir(), 'gh-release-')), 'release.json');
  const originalFetch = global.fetch;
  const gh = fakeGitHub([
    [/^GET \/repos\/nile-agi\/delta\/git\/ref\/tags\/v1\.0\.17$/, { data: {} }],
    [/^GET \/repos\/nile-agi\/delta-release\/releases\?/, { data: [] }],
    [/^POST \/repos\/nile-agi\/delta\/releases\/generate-notes$/, { data: { body: '## Fixes\n- Read scanned pages (#207)' } }],
    [/^POST \/repos\/nile-agi\/delta-release\/releases$/, (init) => ({ status: 201, data: { id: 77, ...JSON.parse(init.body) } })]
  ]);
  global.fetch = gh.fetchImpl;
  try {
    await main(['prepare', 'v1.0.17', out], {
      RELEASE_REPO_TOKEN: 'w',
      SOURCE_REPO_TOKEN: 'r',
      CIRCLE_PROJECT_USERNAME: 'nile-agi',
      CIRCLE_PROJECT_REPONAME: 'delta'
    });
  } finally {
    global.fetch = originalFetch;
  }
  assert.deepEqual(JSON.parse(readFileSync(out, 'utf8')), { tag: 'v1.0.17', release_id: 77 });
  const create = gh.calls.find((c) => c.method === 'POST' && c.path.endsWith('/releases'));
  const draft = JSON.parse(create.body);
  assert.equal(draft.draft, true);
  assert.equal(draft.tag_name, 'v1.0.17');
  assert.equal(gh.calls.find((c) => c.path.includes('generate-notes')).headers.Authorization, 'Bearer r');
  assert.equal(create.headers.Authorization, 'Bearer w');
});
