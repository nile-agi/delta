const { readFileSync, statSync } = require('node:fs');
const { join, basename } = require('node:path');

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

// GitHub can expose an unpublished release under a placeholder tag. Its stable ID and
// exact generated title identify our own draft; never accept another version's release.
function matchesReleaseTag(release, tag) {
  return release.tag_name === tag || (
    release.draft && /^untagged-/.test(release.tag_name) && release.name === `Delta ${tag}`
  );
}

async function releaseById({ github, context, tag, releaseId, releaseRepo }) {
  validateTag(tag);
  const id = Number(releaseId);
  if (!Number.isSafeInteger(id) || id <= 0) throw new Error('A valid release ID is required.');
  const repo = parseRepo(releaseRepo, context.repo);
  const { data: release } = await github.rest.repos.getRelease({ ...repo, release_id: id });
  if (!matchesReleaseTag(release, tag)) throw new Error('The release ID belongs to a different tag.');
  return { repo, id, release };
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
  const existing = releases.find((release) => release.tag_name === tag)
    ?? releases.find((release) => matchesReleaseTag(release, tag));
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
    tag_name: tag,
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

async function publishRelease(options) {
  const { repo, id, release } = await releaseById(options);
  if (release.draft) {
    await options.github.rest.repos.updateRelease({
      ...repo, release_id: id, draft: false,
      ...(release.tag_name !== options.tag ? { tag_name: options.tag } : {})
    });
  }
}

// Upload to the draft ID emitted by prepare-release, without published-tag discovery.
// Preflight all input files before replacing any existing assets. Notes and draft state stay intact.
async function uploadReleaseAssets(options) {
  const { github, files } = options;
  const { repo, id } = await releaseById(options);
  if (!Array.isArray(files) || !files.length) throw new Error('Release asset files are required.');
  const inputs = files.map(path => {
    const stat = statSync(path);
    if (!stat.isFile() || stat.size === 0) throw new Error(`Release asset is missing or empty: ${path}`);
    return { path, name: basename(path) };
  });
  if (new Set(inputs.map(file => file.name)).size !== inputs.length) {
    throw new Error('Duplicate release asset names.');
  }
  const assets = await github.paginate(github.rest.repos.listReleaseAssets, { ...repo, release_id: id, per_page: 100 });
  for (const file of inputs) {
    const data = readFileSync(file.path);
    for (const asset of assets.filter(asset => asset.name === file.name)) {
      await github.rest.repos.deleteReleaseAsset({ ...repo, asset_id: asset.id });
    }
    await github.rest.repos.uploadReleaseAsset({
      ...repo, release_id: id, name: file.name, data,
      headers: { 'content-type': 'application/octet-stream', 'content-length': data.length }
    });
  }
  return inputs.length;
}

module.exports = { prepareRelease, publishRelease, publicNotes, uploadReleaseAssets };
