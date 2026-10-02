const assert = require('node:assert/strict');
const { test } = require('node:test');
const { prepareRelease, publishRelease } = require('./release-notes.cjs');

function fixture(existing = []) {
  const calls = [];
  const records = structuredClone(existing);
  const github = {
    paginate: async () => records,
    rest: {
      git: { getRef: async () => ({ data: { ref: 'refs/tags/v1.0.14' } }) },
      repos: {
        listReleases: () => {},
        generateReleaseNotes: async (args) => {
          calls.push(['notes', args]);
          return { data: { body: '## Fixes\n- Fix Settings (#160)\n\n**Full Changelog**: compare/v1.0.13...v1.0.14' } };
        },
        createRelease: async (args) => {
          calls.push(['create', args]);
          const record = { id: 19, ...args };
          records.push(record);
          return { data: record };
        },
        updateRelease: async (args) => {
          calls.push(['update', args]);
          const record = records.find((item) => item.id === args.release_id);
          Object.assign(record, args);
          return { data: record };
        },
        getRelease: async ({ release_id }) => ({ data: records.find((item) => item.id === release_id) })
      }
    }
  };
  return { github, calls, records, context: { repo: { owner: 'nile-agi', repo: 'delta' } }, tag: 'v1.0.14', installationNotes: '## Downloads\nInstall Delta.' };
}

test('a new tag gets generated notes and installation instructions in one draft', async () => {
  const f = fixture();
  assert.equal(await prepareRelease(f), 19);
  const draft = f.records[0];
  assert.equal(draft.tag_name, 'v1.0.14');
  assert.equal(draft.name, 'Delta v1.0.14');
  assert.equal(draft.draft, true);
  assert.equal(draft.prerelease, false);
  assert.match(draft.body, /Fix Settings \(#160\)/);
  assert.match(draft.body, /compare\/v1\.0\.13\.\.\.v1\.0\.14/);
  assert.ok(draft.body.endsWith(f.installationNotes));
  assert.equal(f.calls[0][1].configuration_file_path, '.github/release.yml');
});

test('rerunning a draft updates its notes without duplicating instructions or creating another release', async () => {
  const f = fixture([{ id: 7, tag_name: 'v1.0.14', draft: true, body: 'Old notes' }]);
  assert.equal(await prepareRelease(f), 7);
  const firstBody = f.records[0].body;
  assert.equal(await prepareRelease(f), 7);
  assert.equal(f.records.length, 1);
  assert.equal(f.records[0].body, firstBody);
  assert.equal(f.calls.filter(([name]) => name === 'create').length, 0);
});

test('rerunning a published release preserves its body and publication state', async () => {
  const f = fixture([{ id: 7, tag_name: 'v1.0.14', draft: false, body: 'Reviewed release notes' }]);
  assert.equal(await prepareRelease(f), 7);
  assert.deepEqual(f.calls, []);
  assert.equal(f.records[0].body, 'Reviewed release notes');
  assert.equal(f.records[0].draft, false);
});

test('a missing tag cannot accidentally create a tag or release', async () => {
  const f = fixture();
  f.github.rest.git.getRef = async () => { throw new Error('Tag not found'); };
  await assert.rejects(prepareRelease(f), /Tag not found/);
  assert.deepEqual(f.calls, []);
});

test('manual dispatch requires a version tag rather than a branch', async () => {
  const f = fixture();
  await assert.rejects(prepareRelease({ ...f, tag: 'main' }), /version tag/);
  assert.deepEqual(f.calls, []);
});

test('a generation failure leaves the repository without a new release', async () => {
  const f = fixture();
  f.github.rest.repos.generateReleaseNotes = async () => { throw new Error('API unavailable'); };
  await assert.rejects(prepareRelease(f), /API unavailable/);
  assert.equal(f.records.length, 0);
});

test('prerelease tags retain prerelease status when published', async () => {
  const f = fixture();
  f.tag = 'v1.0.14-rc.1';
  const id = await prepareRelease(f);
  assert.equal(f.records[0].prerelease, true);
  await publishRelease({ ...f, releaseId: id });
  assert.equal(f.records[0].draft, false);
  assert.equal(f.records[0].prerelease, true);
});

test('empty generated notes cannot create a release', async () => {
  const f = fixture();
  f.github.rest.repos.generateReleaseNotes = async () => ({ data: { body: '  ' } });
  await assert.rejects(prepareRelease(f), /empty release notes/);
  assert.equal(f.records.length, 0);
});

test('publication rejects an invalid release ID before accessing a release', async () => {
  const f = fixture();
  f.github.rest.repos.getRelease = async () => { throw new Error('Unexpected release lookup'); };
  for (const releaseId of ['', 'bad-id', -1, 1.5]) {
    await assert.rejects(publishRelease({ ...f, releaseId }), /valid release ID/);
  }
});

test('publication only changes draft state and preserves notes and assets', async () => {
  const f = fixture([{ id: 7, tag_name: 'v1.0.14', draft: true, body: 'Notes', assets: ['desktop.dmg', 'cli.tar.gz'] }]);
  await publishRelease({ ...f, releaseId: 7 });
  assert.equal(f.records[0].draft, false);
  assert.equal(f.records[0].body, 'Notes');
  assert.deepEqual(f.records[0].assets, ['desktop.dmg', 'cli.tar.gz']);
  assert.deepEqual(f.calls[0][1], { owner: 'nile-agi', repo: 'delta', release_id: 7, draft: false });
});

test('publication is idempotent for an already published release', async () => {
  const f = fixture([{ id: 7, tag_name: 'v1.0.14', draft: false }]);
  await publishRelease({ ...f, releaseId: 7 });
  assert.deepEqual(f.calls, []);
});

test('publication refuses a release that belongs to another tag', async () => {
  const f = fixture([{ id: 7, tag_name: 'v1.0.13', draft: true }]);
  await assert.rejects(publishRelease({ ...f, releaseId: 7 }), /tag/);
  assert.deepEqual(f.calls, []);
});
