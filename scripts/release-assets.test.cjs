const assert = require('node:assert/strict');
const { test } = require('node:test');
const { mkdtempSync, writeFileSync, rmSync } = require('node:fs');
const { join } = require('node:path');
const { tmpdir } = require('node:os');
const { uploadReleaseAssets } = require('./release-notes.cjs');

function fixture(t, tagName = 'untagged-example') {
  const directory = mkdtempSync(join(tmpdir(), 'delta-release-assets-'));
  t.after(() => rmSync(directory, { recursive: true, force: true }));
  const file = join(directory, 'cli.tar.gz');
  writeFileSync(file, 'new archive');
  const release = { id: 7, tag_name: tagName, name: 'Delta v1.0.18-rc.1', draft: true, body: 'Reviewed notes' };
  const assets = [{ id: 1, name: 'desktop.dmg' }, { id: 2, name: 'cli.tar.gz' }];
  const calls = [];
  const github = {
    paginate: async (_method, args) => { assert.equal(args.release_id, 7); return assets; },
    rest: { repos: {
      getRelease: async args => { calls.push(['get', args]); return { data: release }; },
      listReleaseAssets() {},
      deleteReleaseAsset: async args => { calls.push(['delete', args]); },
      uploadReleaseAsset: async args => { calls.push(['upload', args]); return { data: { id: 3 } }; }
    } }
  };
  return { github, context: { repo: { owner: 'nile-agi', repo: 'delta' } }, releaseRepo: 'nile-agi/delta-release', tag: 'v1.0.18-rc.1', releaseId: '7', files: [file], release, calls };
}

test('uploads directly to the prepared draft ID and replaces only matching CLI assets', async t => {
  const f = fixture(t);
  assert.equal(await uploadReleaseAssets(f), 1);
  assert.equal(f.calls[0][1].repo, 'delta-release');
  assert.deepEqual(f.calls.filter(([method]) => method === 'delete').map(([, args]) => args.asset_id), [2]);
  const upload = f.calls.find(([method]) => method === 'upload')[1];
  assert.equal(upload.release_id, 7);
  assert.equal(upload.name, 'cli.tar.gz');
  assert.equal(upload.data.toString(), 'new archive');
  assert.equal(f.release.draft, true);
  assert.equal(f.release.body, 'Reviewed notes');
});

test('fails before deleting assets if a required archive is missing', async t => {
  const f = fixture(t);
  f.files.push(join(tmpdir(), 'delta-does-not-exist', 'missing.zip'));
  await assert.rejects(uploadReleaseAssets(f));
  assert.ok(!f.calls.some(([method]) => method === 'delete' || method === 'upload'));
});

test('does not upload to a different tag or a differently named placeholder', async t => {
  for (const tag of ['v1.0.17', 'untagged-example']) {
    const f = fixture(t, tag);
    f.release.name = 'Delta v1.0.17';
    await assert.rejects(uploadReleaseAssets(f), /tag/);
    assert.ok(!f.calls.some(([method]) => method === 'delete' || method === 'upload'));
  }
});

test('invalid release IDs never reach the API', async t => {
  const f = fixture(t);
  f.releaseId = '';
  await assert.rejects(uploadReleaseAssets(f), /valid release ID/);
  assert.deepEqual(f.calls, []);
});
