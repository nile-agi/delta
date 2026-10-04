const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { execFileSync } = require('node:child_process');

for (const newline of ['\n', '\r\n']) {
  test(`version syncing preserves Cargo metadata with ${newline === '\n' ? 'LF' : 'CRLF'} endings`, () => {
    const root = fs.mkdtempSync(path.join(os.tmpdir(), 'delta-version-'));
    try {
      fs.mkdirSync(path.join(root, 'scripts'));
      fs.mkdirSync(path.join(root, 'src-tauri'));
      const script = path.join(root, 'scripts/bump-version.sh');
      fs.copyFileSync(path.join(__dirname, 'bump-version.sh'), script);
      const manifest = '[package]\nname = "delta-app"\nversion = "1.0.0"\nedition = "2021"\n\n[dependencies]\nversion = "2.3.4"\n'.replaceAll('\n', newline);
      const cargo = path.join(root, 'src-tauri/Cargo.toml');
      fs.writeFileSync(cargo, manifest);
      execFileSync('bash', [script, '1.2.3']);
      assert.equal(fs.readFileSync(cargo, 'utf8'), manifest.replace('version = "1.0.0"', 'version = "1.2.3"'));
      execFileSync('bash', [script]);
      assert.equal(fs.readFileSync(cargo, 'utf8'), manifest.replace('version = "1.0.0"', 'version = "1.2.3"'));
    } finally {
      fs.rmSync(root, { recursive: true, force: true });
    }
  });
}
