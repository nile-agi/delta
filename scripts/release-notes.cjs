const { readFileSync } = require('node:fs');
const { join } = require('node:path');

function validateTag(tag) {
  if (!/^v\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?(?:\+[0-9A-Za-z.-]+)?$/.test(tag)) {
    throw new Error('Choose an existing version tag, such as v1.0.14.');
  }
}

// "owner/name" -> { owner, repo }; empty means the workflow's own repository.
function parseRepo(slug, fallback) {
  if (!slug) return fallback;
  const [owner, repo, ...rest] = slug.split('/');
  if (!owner || !repo || rest.length) throw new Error(`Release repository must be "owner/name", got "${slug}".`);
  return { owner, repo };
}

function sameRepo(a, b) {
  return a.owner.toLowerCase() === b.owner.toLowerCase() && a.repo.toLowerCase() === b.repo.toLowerCase();
}

// Generated notes link every PR and the compare view to the source repository. When that
// repository is private and the release is public, those links only lead readers to a 404, so
// keep the titles and authors and drop the links.
function publicNotes(body, source) {
  const base = `https://github.com/${source.owner}/${source.repo}`.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
  return body
    .split('\n')
    .filter((line) => !new RegExp(`${base}/compare/`, 'i').test(line))
    .map((line) => line.replace(new RegExp(` in ${base}/(?:pull|issues)/\\d+`, 'gi'), ''))
    .join('\n')
    .replace(/\n{3,}/g, '\n\n')
    .trim();
}

/**
 * Creates (or refreshes) the draft release for `tag`.
 *
 * The tag and its notes come from the workflow's repository (`context.repo`, read with
 * `sourceGithub`). The release itself is created in `releaseRepo` with `github`, which for a
 * separate public repository needs a token that can write there. In that repository the tag is
 * created on `targetCommitish`, since the source commit does not exist there.
 */
async function prepareRelease({
  github,
  sourceGithub = github,
  context,
  tag,
  installationNotes,
  releaseRepo,
  targetCommitish = 'main'
}) {
  validateTag(tag);
  const source = context.repo;
  const target = parseRepo(releaseRepo, source);
  const separate = !sameRepo(source, target);
  // Never let a manual dispatch manufacture a tag from the workflow's branch.
  await sourceGithub.rest.git.getRef({ ...source, ref: `tags/${tag}` });
  // Listing includes drafts, unlike the published-release lookup by tag.
  const releases = await github.paginate(github.rest.repos.listReleases, { ...target, per_page: 100 });
  const existing = releases.find((release) => release.tag_name === tag);
  if (existing && !existing.draft) return existing.id;

  const { data: notes } = await sourceGithub.rest.repos.generateReleaseNotes({
    ...source,
    tag_name: tag,
    configuration_file_path: '.github/release.yml'
  });
  if (!notes.body?.trim()) throw new Error('GitHub returned empty release notes.');
  const changes = separate ? publicNotes(notes.body, source) : notes.body.trim();
  const instructions = installationNotes ?? readFileSync(join(__dirname, '../.github/release-installation.md'), 'utf8').trim();
  const metadata = {
    ...target,
    name: `Delta ${tag}`,
    body: `${changes}\n\n---\n\n${instructions}`,
    draft: true,
    prerelease: tag.split('+')[0].includes('-')
  };
  const { data: release } = existing
    ? await github.rest.repos.updateRelease({ ...metadata, release_id: existing.id })
    : await github.rest.repos.createRelease({
        ...metadata,
        tag_name: tag,
        ...(separate ? { target_commitish: targetCommitish } : {})
      });
  return release.id;
}

async function publishRelease({ github, context, tag, releaseId, releaseRepo }) {
  validateTag(tag);
  const id = Number(releaseId);
  if (!Number.isSafeInteger(id) || id <= 0) throw new Error('A valid release ID is required.');
  const repo = parseRepo(releaseRepo, context.repo);
  const { data: release } = await github.rest.repos.getRelease({ ...repo, release_id: id });
  if (release.tag_name !== tag) throw new Error('The release ID belongs to a different tag.');
  if (release.draft) {
    await github.rest.repos.updateRelease({ ...repo, release_id: id, draft: false });
  }
}

module.exports = { prepareRelease, publishRelease, publicNotes };
