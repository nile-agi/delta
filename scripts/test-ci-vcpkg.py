#!/usr/bin/env python3
"""Check bounded download retries and immediate compiler-failure propagation."""
import os,pathlib,subprocess,tempfile
script=pathlib.Path(__file__).with_name('install-ci-vcpkg.sh').resolve()
with tempfile.TemporaryDirectory(prefix='delta-vcpkg-test-') as folder:
 root=pathlib.Path(folder);bin=root/'bin';bin.mkdir()
 (bin/'vcpkg').write_text('''#!/bin/bash
n=0; if [ -f "$TEST_COUNT" ]; then read -r n < "$TEST_COUNT"; fi
n=$((n+1)); printf '%s\\n' "$n" > "$TEST_COUNT"
if [ "$TEST_MODE" = retry ] && [ "$n" -gt 1 ]; then exit 0; fi
if [ "$TEST_MODE" = build ]; then echo 'error: building curl failed: compiler error'; exit 3; fi
echo 'error: curl operation failed with error code 35 (SSL connect error).'; exit 7
''')
 (bin/'sleep').write_text('#!/bin/sh\nexit 0\n')
 for p in bin.iterdir():p.chmod(0o755)
 for mode,count,code in [('retry',2,0),('build',1,3),('persistent',3,7)]:
  counter=root/'counter';counter.unlink(missing_ok=True)
  env=dict(os.environ,PATH=str(bin)+os.pathsep+os.environ['PATH'],TEST_COUNT=str(counter),TEST_MODE=mode)
  r=subprocess.run(['/bin/bash',str(script),'install','curl:x64-windows-static-md'],env=env,capture_output=True,text=True)
  assert r.returncode==code,(mode,r.returncode,r.stderr)
  assert int(counter.read_text())==count,(mode,counter.read_text())
  print(mode,'passed')
