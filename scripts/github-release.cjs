#!/usr/bin/env node
/**
 * The GitHub side of a release, for CI outside GitHub Actions (CircleCI):
 *
 *   node scripts/github-release.cjs prepare <tag> <out.json>   draft release with notes -> {tag, release_id}
 *   node scripts/github-release.cjs upload <release_id> <file>...   attach files, replacing same-named ones
 *   node scripts/github-release.cjs publish <tag> <release_id>   publish the draft
 *
 * RELEASE_REPO_TOKEN writes to RELEASE_REPO (default nile-agi/delta-release). SOURCE_REPO_TOKEN
 * reads the tag and generates notes in SOURCE_REPO (default: the CircleCI project).
 */
const { readFileSync, writeFileSync, statSync } = require('node:fs');
const { basename } = require('node:path');
const { prepareRelease, publishRelease } = require('./release-notes.cjs');

const API = 'https://api.github.com';
const UPLOADS = 'https://uploads.github.com';

/** The slice of Octokit that release-notes.cjs uses, over fetch. */
function client(token, fetchImpl = fetch) {
  if (!token) throw new Error('A GitHub token is required.');
  async function request(method, url, body, headers = {}) {
    const res = await fetchImpl(url.startsWith('http') ? url : `${API}${url}`, {
      method,
      headers: {
        Accept: 'application/vnd.github+json',
        Authorization: `Bearer ${token}`,
        'X-GitHub-Api-Version': '2022-11-28',
        'User-Agent': 'delta-release',
        ...(body !== undefined && !Buffer.isBuffer(body) ? { 'Content-Type': 'application/json' } : {}),
        ...headers
      },
      body: body === undefined ? undefined : Buffer.isBuffer(body) ? body : JSON.stringify(body)
    });
    const text = await res.text();
    const data = text ? JSON.parse(text) : null;
    if (!res.ok) {
      const error = new Error(`${method} ${url} failed (${res.status}): ${data?.message ?? text}`);
      error.status = res.status;
      throw error;
    }
    return { data };
  }
  const repoPath = ({ owner, repo }) => `/repos/${owner}/${repo}`;
  const without = (params, ...keys) => {
    const out = { ...params };
    for (const k of ['owner', 'repo', ...keys]) delete out[k];
    return out;
  };
  return {
    request,
    rest: {
      git: {
        getRef: (p) => request('GET', `${repoPath(p)}/git/ref/${p.ref}`)
      },
      repos: {
        listReleases: (p) =>
          request('GET', `${repoPath(p)}/releases?per_page=${p.per_page ?? 30}&page=${p.page ?? 1}`),
        getRelease: (p) => request('GET', `${repoPath(p)}/releases/${p.release_id}`),
        createRelease: (p) => request('POST', `${repoPath(p)}/releases`, without(p)),
        updateRelease: (p) => request('PATCH', `${repoPath(p)}/releases/${p.release_id}`, without(p, 'release_id')),
        generateReleaseNotes: (p) => request('POST', `${repoPath(p)}/releases/generate-notes`, without(p))
      }
    },
    async paginate(method, params) {
      const all = [];
      const perPage = params.per_page ?? 30;
      for (let page = 1; ; page++) {
        const { data } = await method({ ...params, per_page: perPage, page });
        all.push(...data);
        if (data.length < perPage) return all;
      }
    }
  };
}

function parseRepo(slug) {
  const [owner, repo] = String(slug).split('/');
  if (!owner || !repo) throw new Error(`Repository must be "owner/name", got "${slug}".`);
  return { owner, repo };
}

/** Attaches files to a release, replacing any asset with the same name (like `gh release upload --clobber`). */
async function uploadAssets({ github, repo, releaseId, files }) {
  const { data: release } = await github.rest.repos.getRelease({ ...repo, release_id: releaseId });
  for (const file of files) {
    const name = basename(file);
    const old = release.assets?.find((a) => a.name === name);
    if (old) await github.request('DELETE', `/repos/${repo.owner}/${repo.repo}/releases/assets/${old.id}`);
    const body = readFileSync(file);
    await github.request(
      'POST',
      `${UPLOADS}/repos/${repo.owner}/${repo.repo}/releases/${releaseId}/assets?name=${encodeURIComponent(name)}`,
      body,
      { 'Content-Type': 'application/octet-stream', 'Content-Length': String(statSync(file).size) }
    );
    console.log(`Uploaded ${name}`);
  }
}

async function main(argv, env = process.env) {
  const [command, ...args] = argv;
  const releaseRepo = env.RELEASE_REPO || 'nile-agi/delta-release';
  const sourceSlug =
    env.SOURCE_REPO || `${env.CIRCLE_PROJECT_USERNAME ?? ''}/${env.CIRCLE_PROJECT_REPONAME ?? ''}`;
  const github = client(env.RELEASE_REPO_TOKEN);
  const context = { repo: parseRepo(sourceSlug) };

  if (command === 'prepare') {
    const [tag, out] = args;
    const sourceGithub = client(env.SOURCE_REPO_TOKEN || env.RELEASE_REPO_TOKEN);
    const id = await prepareRelease({ github, sourceGithub, context, tag, releaseRepo });
    writeFileSync(out, JSON.stringify({ tag, release_id: id }) + '\n');
    console.log(`Draft release ${id} ready for ${tag} in ${releaseRepo}`);
  } else if (command === 'upload') {
    const [releaseId, ...files] = args;
    if (!files.length) throw new Error('No files to upload.');
    await uploadAssets({ github, repo: parseRepo(releaseRepo), releaseId: Number(releaseId), files });
  } else if (command === 'publish') {
    const [tag, releaseId] = args;
    await publishRelease({ github, context, tag, releaseId, releaseRepo });
    console.log(`Published ${tag}`);
  } else {
    throw new Error('Usage: github-release.cjs prepare|upload|publish ...');
  }
}

if (require.main === module) {
  main(process.argv.slice(2)).catch((error) => {
    console.error(error.message);
    process.exit(1);
  });
}

module.exports = { client, uploadAssets, main };
