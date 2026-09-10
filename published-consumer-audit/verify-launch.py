"""Observe a fresh consumer without modifying its published dependencies."""
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time

phase, *command = sys.argv[1:]
evidence = Path(os.environ['AUDIT_EVIDENCE'])
evidence.mkdir(parents=True, exist_ok=True)
log = evidence / f'{phase}.log'
app_log = evidence / f'{phase}-app.log'
markers = json.loads(os.environ['AUDIT_MARKERS'])
assert markers, 'Explicit runtime success markers are required'
env = dict(os.environ, LYNXTRON_AUDIT_LOG=str(app_log))
fallback_log = Path(tempfile.gettempdir()) / 'fresh-alpha-audit.jsonl'
fallback_log.unlink(missing_ok=True)
result = {'phase': phase, 'command': command, 'markers': {}, 'success': False}
if phase == 'installed':
    # Exercise LaunchServices on the application copied out of the DMG.
    command = ['open', '-n', '-W', '--stdout', str(log), '--stderr', str(log), command[0]]
with log.open('w') as output:
    proc = subprocess.Popen(command, stdout=output, stderr=subprocess.STDOUT, env=env, start_new_session=True)
    try:
        deadline = time.monotonic() + 150
        while time.monotonic() < deadline:
            content = log.read_text(errors='replace')
            if app_log.exists():
                content += app_log.read_text(errors='replace')
            if fallback_log.exists():
                content += fallback_log.read_text(errors='replace')
            result['markers'] = {marker: marker in content for marker in markers}
            if all(result['markers'].values()):
                time.sleep(5)
                result['success'] = proc.poll() is None
                break
            if proc.poll() is not None:
                result['exit_code'] = proc.returncode
                break
            time.sleep(2)
        try:
            capture = subprocess.run(['screencapture', '-x', str(evidence / f'{phase}.png')], capture_output=True, text=True, timeout=20)
            result['screenshot'] = {'exit_code': capture.returncode, 'error': capture.stderr}
        except subprocess.TimeoutExpired:
            result['screenshot'] = {'error': 'screencapture timed out'}
        with (evidence / f'{phase}-processes.txt').open('w') as process_log:
            subprocess.run(['ps', '-axo', 'pid,ppid,command'], stdout=process_log, check=False, timeout=10)
    finally:
        try:
            os.killpg(proc.pid, signal.SIGTERM)
        except ProcessLookupError:
            pass
        try:
            proc.wait(timeout=10)
        except subprocess.TimeoutExpired:
            os.killpg(proc.pid, signal.SIGKILL)
            proc.wait(timeout=10)
        # The fixture owns this specific loopback port on an isolated CI runner.
        owners = subprocess.run(['lsof', '-t', '-iTCP:' + os.environ['AUDIT_PORT'], '-sTCP:LISTEN'], capture_output=True, text=True, timeout=10)
        for pid in owners.stdout.split():
            try:
                os.kill(int(pid), signal.SIGTERM)
            except ProcessLookupError:
                pass
        time.sleep(3)
(evidence / f'{phase}.json').write_text(json.dumps(result, indent=2))
print(log.read_text(errors='replace'))
if app_log.exists():
    print(app_log.read_text(errors='replace'))
if fallback_log.exists():
    fallback_content = fallback_log.read_text(errors='replace')
    (evidence / f'{phase}-launchservices.jsonl').write_text(fallback_content)
    print(fallback_content)
print(json.dumps(result, indent=2))
sys.exit(0 if result['success'] else 1)
