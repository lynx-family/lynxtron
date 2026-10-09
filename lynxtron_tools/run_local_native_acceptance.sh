#!/bin/bash
# All environment preparation and compilation for the local real-build probe.
set -euo pipefail
if [[ $# -lt 1 || $# -gt 3 || "$(uname -s)" != Darwin ]]; then
  echo 'Usage: bash run_local_native_acceptance.sh <isolated-checkout> [lynxtron|cef-webview] [--prepared]' >&2
  exit 2
fi
probe_tools="$(cd "$(dirname "$0")" && pwd)"
probe_checkout="$(cd "$1" && pwd)"
probe_component="${2:-lynxtron}"
case "$probe_component" in lynxtron|cef-webview) ;; *) exit 2 ;; esac
probe_prepared="${3:-}"
case "$probe_prepared" in ''|--prepared) ;; *) exit 2 ;; esac
cd "$probe_checkout"
if [[ ! -f .native-cache-acceptance-checkout ]]; then
  echo 'Refusing to prepare a checkout without its acceptance ownership marker' >&2
  exit 2
fi
export GIT_AUTHOR_NAME="Lynxtron cache experiment"
export GIT_AUTHOR_EMAIL="scripts@lynxtron.com"
export GIT_COMMITTER_NAME="$GIT_AUTHOR_NAME"
export GIT_COMMITTER_EMAIL="$GIT_AUTHOR_EMAIL"
# Existing local Habitat launchers may use python3.9. Expose an already
# installed interpreter for this process only; never change global pyenv state.
if command -v pyenv >/dev/null && ! python3.9 --version >/dev/null 2>&1; then
  if probe_python39="$(pyenv latest 3.9 2>/dev/null)"; then
    export PYENV_VERSION="$(pyenv version-name):$probe_python39"
  fi
fi
# Only prepare build dependencies. envsetup.sh also rewrites Git hooks; that
# unrelated side effect is inappropriate for this isolated cache experiment.
python3 lynxtron_tools/vpython_tools/vpython_env_setup.py --root_dir "$probe_checkout"
source .venv/bin/activate
export PATH="$probe_checkout/buildtools/gn:$probe_checkout/buildtools/ninja:$probe_checkout/buildtools/node/bin:$PATH"
if [[ "$probe_component" == lynxtron && "$probe_prepared" != --prepared ]]; then
  python3 lynxtron_tools/prepare_build_env.py
fi
python3 -m unittest discover -s "$probe_tools" -p 'test_*native*.py'
python3 "$probe_tools/run_local_native_cache.py" "$probe_checkout" --component "$probe_component"
