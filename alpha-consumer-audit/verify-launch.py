import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import time

phase, *command = sys.argv[1:]
evidence = Path(os.environ['AUDIT_EVIDENCE'])
evidence.mkdir(parents=True, exist_ok=True)
log = evidence / f'{phase}.log'
markers = ['HOST_READY', 'WIDGET_INITIALIZED', 'CEF_INITIALIZED', 'UI_MOUNTED', 'WIDGET_INVOKE_OK', 'CEF_LOAD_OK']
result = {'phase': phase, 'command': command, 'markers': {}, 'success': False}
with log.open('w') as output:
    proc = subprocess.Popen(command, stdout=output, stderr=subprocess.STDOUT, start_new_session=True)
    try:
        deadline = time.monotonic() + 150
        while time.monotonic() < deadline:
            content = log.read_text(errors='replace')
            result['markers'] = {marker: marker in content for marker in markers}
            if all(result['markers'].values()):
                time.sleep(5)
                result['success'] = proc.poll() is None
                break
            if proc.poll() is not None:
                result['exit_code'] = proc.returncode
                break
            time.sleep(2)
        subprocess.run(['screencapture', '-x', str(evidence / f'{phase}.png')], check=False)
        with (evidence / f'{phase}-processes.txt').open('w') as process_log:
            subprocess.run(['ps', '-axo', 'pid,ppid,command'], stdout=process_log, check=False)
    finally:
        try:
            os.killpg(proc.pid, signal.SIGTERM)
        except ProcessLookupError:
            pass
        try:
            proc.wait(timeout=10)
        except subprocess.TimeoutExpired:
            os.killpg(proc.pid, signal.SIGKILL)
            proc.wait()
        # Stop only the dedicated fixture's localhost server owner if a launcher
        # detached it from the process group. This is an ephemeral audit runner.
        owners = subprocess.run(['lsof', '-t', '-iTCP:17981', '-sTCP:LISTEN'], capture_output=True, text=True)
        for pid in owners.stdout.split():
            try:
                os.kill(int(pid), signal.SIGTERM)
            except ProcessLookupError:
                pass
        time.sleep(3)
(evidence / f'{phase}.json').write_text(json.dumps(result, indent=2))
print(log.read_text(errors='replace'))
print(json.dumps(result, indent=2))
sys.exit(0 if result['success'] else 1)
