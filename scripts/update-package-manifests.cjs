#!/usr/bin/env node
/**
 * Points the package manifests in dist/ at a published release and writes in its checksums.
 *
 * The manifests install the prebuilt CLI archives from the public release repository, so each one
 * names a version and the SHA-256 (and, for MacPorts, the size) of its archive. Those exist only
 * once the release is out, so run this after publishing, then commit the result:
 *
 *   node scripts/update-package-manifests.cjs v1.0.16
 *
 * Values come from the digests GitHub records for each asset; nothing is downloaded.
 */
const fs = require('node:fs');
const path = require('node:path');

const RELEASE_REPO = 'nile-agi/delta-release';
const ASSETS = [
  'delta-cli-macos-arm64.tar.gz',
  'delta-cli-macos-x86_64.tar.gz',
  'delta-cli-linux-x86_64.tar.gz',
  'delta-cli-windows-x64.zip'
];

const escape = (s) => s.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');

// Replaces exactly one match, so a manifest that changed shape fails loudly instead of being
// left half-updated.
function replaceOnce(text, pattern, replacement, what) {
  const matches = text.match(new RegExp(pattern.source, pattern.flags.includes('g') ? pattern.flags : pattern.flags + 'g'));
  if (!matches || matches.length !== 1) {
    throw new Error(`Expected exactly one ${what}, found ${matches ? matches.length : 0}.`);
  }
  return text.replace(pattern, replacement);
}

function homebrew(text, release) {
  let out = replaceOnce(text, /^(\s*version ")[^"]*(")/m, `$1${release.version}$2`, 'Homebrew version');
  for (const name of ASSETS.filter((a) => !a.endsWith('.zip'))) {
    out = replaceOnce(
      out,
      new RegExp(`(url "https://github\\.com/${escape(RELEASE_REPO)}/releases/download/)v[^/]+(/${escape(name)}"\\s*\\n\\s*sha256 ")[0-9a-f]*(")`),
      `$1v${release.version}$2${release.assets[name].sha256}$3`,
      `Homebrew url/sha256 pair for ${name}`
    );
  }
  return out;
}

function nix(text, release) {
  let out = replaceOnce(text, /(\bversion = ")[^"]*(")/, `$1${release.version}$2`, 'Nix version');
  for (const name of ASSETS.filter((a) => !a.endsWith('.zip'))) {
    out = replaceOnce(
      out,
      new RegExp(`(asset = "${escape(name)}"; sha256 = ")[0-9a-f]*(")`),
      `$1${release.assets[name].sha256}$2`,
      `Nix sha256 for ${name}`
    );
  }
  return out;
}

function macports(text, release) {
  let out = replaceOnce(text, /^(version\s+)\S+/m, `$1${release.version}`, 'MacPorts version');
  out = replaceOnce(out, /^(revision\s+)\d+/m, '$10', 'MacPorts revision');
  for (const name of ['delta-cli-macos-arm64.tar.gz', 'delta-cli-macos-x86_64.tar.gz']) {
    const base = name.replace(/\.tar\.gz$/, '');
    const { sha256, size } = release.assets[name];
    out = replaceOnce(
      out,
      new RegExp(`(distname\\s+${escape(base)}\\s*\\n\\s*checksums\\s+sha256\\s+)[0-9a-f]+(\\s*\\\\\\s*\\n\\s*size\\s+)\\d+`),
      `$1${sha256}$2${size}`,
      `MacPorts checksums for ${name}`
    );
  }
  return out;
}

function winget(text, release) {
  const name = 'delta-cli-windows-x64.zip';
  let out = replaceOnce(text, /^(PackageVersion: )\S+/m, `$1${release.version}`, 'winget PackageVersion');
  out = replaceOnce(
    out,
    /(RelativeFilePath: delta-cli-)[^\\]+(\\)/,
    `$1${release.version}$2`,
    'winget nested file path'
  );
  out = replaceOnce(
    out,
    new RegExp(`(InstallerUrl: https://github\\.com/${escape(RELEASE_REPO)}/releases/download/)v[^/]+(/${escape(name)})`),
    `$1v${release.version}$2`,
    'winget InstallerUrl'
  );
  out = replaceOnce(out, /^(\s*InstallerSha256: )\S+/m, `$1${release.assets[name].sha256.toUpperCase()}`, 'winget InstallerSha256');
  return out;
}

const MANIFESTS = {
  'dist/homebrew/delta-cli.rb': homebrew,
  'dist/nix/default.nix': nix,
  'dist/macports/Portfile': macports,
  'dist/winget/delta-cli.yaml': winget
};

/** Version, SHA-256 and size of every CLI archive, from a GitHub release API response. */
function releaseInfo(json) {
  const version = String(json.tag_name || '').replace(/^v/, '');
  if (!/^\d+\.\d+\.\d+(?:[-+].*)?$/.test(version)) throw new Error(`Unexpected tag "${json.tag_name}".`);
  if (json.draft) throw new Error(`${json.tag_name} is still a draft; publish it first.`);
  const assets = {};
  for (const name of ASSETS) {
    const asset = (json.assets || []).find((a) => a.name === name);
    if (!asset) throw new Error(`${json.tag_name} has no ${name}.`);
    const digest = /^sha256:([0-9a-f]{64})$/.exec(asset.digest || '');
    if (!digest) throw new Error(`${name} has no SHA-256 recorded by GitHub.`);
    assets[name] = { sha256: digest[1], size: asset.size };
  }
  return { version, assets };
}

async function main() {
  const tag = process.argv[2];
  if (!tag || !/^v\d+\.\d+\.\d+/.test(tag)) {
    console.error('Usage: node scripts/update-package-manifests.cjs vX.Y.Z');
    process.exit(2);
  }
  const res = await fetch(`https://api.github.com/repos/${RELEASE_REPO}/releases/tags/${tag}`, {
    headers: { Accept: 'application/vnd.github+json', 'User-Agent': 'delta-update-package-manifests' }
  });
  if (!res.ok) throw new Error(`No published release ${tag} in ${RELEASE_REPO} (HTTP ${res.status}).`);
  const release = releaseInfo(await res.json());
  const root = path.join(__dirname, '..');
  for (const [file, update] of Object.entries(MANIFESTS)) {
    const full = path.join(root, file);
    fs.writeFileSync(full, update(fs.readFileSync(full, 'utf8'), release));
    console.log(`  updated ${file}`);
  }
  console.log(`Package manifests now point at ${tag}.`);
}

if (require.main === module) {
  main().catch((error) => {
    console.error(error.message);
    process.exit(1);
  });
}

module.exports = { ASSETS, MANIFESTS, homebrew, nix, macports, winget, releaseInfo };
