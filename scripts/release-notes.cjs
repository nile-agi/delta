const { readFileSync } = require('node:fs');
const { join } = require('node:path');

function validateTag(tag) {
  if (!/^v\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?(?:\+[0-9A-Za-z.-]+)?$/.test(tag)) {
    throw new Error('Choose an existing version tag, such as v1.0.14.');
  }
}

async function prepareRelease({ github, context, tag, installationNotes }) {
  validateTag(tag);
  const repo = context.repo;
  // Never let a manual dispatch manufacture a tag from the workflow's branch.
  await github.rest.git.getRef({ ...repo, ref: `tags/${tag}` });
  // Listing includes drafts, unlike the published-release lookup by tag.
  const releases = await github.paginate(github.rest.repos.listReleases, { ...repo, per_page: 100 });
  const existing = releases.find((release) => release.tag_name === tag);
  if (existing && !existing.draft) return existing.id;

  const { data: notes } = await github.rest.repos.generateReleaseNotes({
    ...repo,
    tag_name: tag,
    configuration_file_path: '.github/release.yml'
  });
  if (!notes.body?.trim()) throw new Error('GitHub returned empty release notes.');
  const instructions = installationNotes ?? readFileSync(join(__dirname, '../.github/release-installation.md'), 'utf8').trim();
  const metadata = {
    ...repo,
    name: `Delta ${tag}`,
    body: `${notes.body.trim()}\n\n---\n\n${instructions}`,
    draft: true,
    prerelease: tag.split('+')[0].includes('-')
  };
  const { data: release } = existing
    ? await github.rest.repos.updateRelease({ ...metadata, release_id: existing.id })
    : await github.rest.repos.createRelease({ ...metadata, tag_name: tag });
  return release.id;
}

async function publishRelease({ github, context, tag, releaseId }) {
  validateTag(tag);
  const id = Number(releaseId);
  if (!Number.isSafeInteger(id) || id <= 0) throw new Error('A valid release ID is required.');
  const repo = context.repo;
  const { data: release } = await github.rest.repos.getRelease({ ...repo, release_id: id });
  if (release.tag_name !== tag) throw new Error('The release ID belongs to a different tag.');
  if (release.draft) {
    await github.rest.repos.updateRelease({ ...repo, release_id: id, draft: false });
  }
}

module.exports = { prepareRelease, publishRelease };
