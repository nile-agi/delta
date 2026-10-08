const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const { test } = require('node:test');
const { ASSETS, MANIFESTS, releaseInfo } = require('./update-package-manifests.cjs');

const root = path.join(__dirname, '..');

function apiRelease(tag = 'v2.3.4', overrides = {}) {
  return {
    tag_name: tag,
    draft: false,
    assets: ASSETS.map((name, i) => ({
      name,
      size: 1000 + i,
      digest: `sha256:${String(i + 1).repeat(64)}`
    })),
    ...overrides
  };
}

test('every manifest is rewritten to the new version and checksums', () => {
  const release = releaseInfo(apiRelease());
  for (const [file, update] of Object.entries(MANIFESTS)) {
    const before = fs.readFileSync(path.join(root, file), 'utf8');
    const after = update(before, release);
    assert.match(after, /2\.3\.4/, `${file} names the new version`);
    assert.doesNotMatch(after, /1\.0\.13/, `${file} keeps no trace of the old version`);
  }
});

test('each archive gets its own checksum', () => {
  const release = releaseInfo(apiRelease());
  const brew = MANIFESTS['dist/homebrew/delta-cli.rb'](fs.readFileSync(path.join(root, 'dist/homebrew/delta-cli.rb'), 'utf8'), release);
  assert.match(brew, /delta-cli-macos-arm64\.tar\.gz"\s*\n\s*sha256 "1{64}"/);
  assert.match(brew, /delta-cli-macos-x86_64\.tar\.gz"\s*\n\s*sha256 "2{64}"/);
  assert.match(brew, /delta-cli-linux-x86_64\.tar\.gz"\s*\n\s*sha256 "3{64}"/);

  const port = MANIFESTS['dist/macports/Portfile'](fs.readFileSync(path.join(root, 'dist/macports/Portfile'), 'utf8'), release);
  assert.match(port, /delta-cli-macos-arm64\s*\n\s*checksums\s+sha256\s+1{64}\s*\\\s*\n\s*size\s+1000/);
  assert.match(port, /delta-cli-macos-x86_64\s*\n\s*checksums\s+sha256\s+2{64}\s*\\\s*\n\s*size\s+1001/);

  const wg = MANIFESTS['dist/winget/delta-cli.yaml'](fs.readFileSync(path.join(root, 'dist/winget/delta-cli.yaml'), 'utf8'), release);
  assert.match(wg, /InstallerSha256: 4{64}/);
  assert.match(wg, /RelativeFilePath: delta-cli-2\.3\.4\\bin\\delta\.exe/);
  assert.match(wg, /download\/v2\.3\.4\/delta-cli-windows-x64\.zip/);
});

test('updating twice gives the same result', () => {
  const release = releaseInfo(apiRelease());
  for (const [file, update] of Object.entries(MANIFESTS)) {
    const once = update(fs.readFileSync(path.join(root, file), 'utf8'), release);
    assert.equal(update(once, release), once, file);
  }
});

test('a draft, a missing archive or a missing digest is refused', () => {
  assert.throws(() => releaseInfo(apiRelease('v2.3.4', { draft: true })), /draft/);
  assert.throws(() => releaseInfo(apiRelease('v2.3.4', { assets: [] })), /has no delta-cli/);
  const noDigest = apiRelease();
  noDigest.assets[0].digest = null;
  assert.throws(() => releaseInfo(noDigest), /SHA-256/);
});

test('a manifest that changed shape fails instead of being half-updated', () => {
  const release = releaseInfo(apiRelease());
  assert.throws(() => MANIFESTS['dist/nix/default.nix']('version = "1.0.0";', release), /Nix sha256/);
});
