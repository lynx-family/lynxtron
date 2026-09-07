#!/bin/bash
#
# Lynxtron HarmonyOS HAP build script.
# Builds the HarmonyOS application package from a completed native GN build.
#
# Usage:
#   ./build_hap.sh           # build unsigned hap (default)
#   ./build_hap.sh --signed  # additionally sign the hap
#
# Requires:
#   - A HarmonyOS Command Line Tools installation whose bundled SDK is at
#     least as new as build-profile.json5's compileSdkVersion.  Point
#     COMMAND_LINE_TOOLS or DEVECO_SDK_HOME at it, or leave it in one of the
#     searched roots (see below); OHOS_TOOLS adds another root to search.
#   - A Java 17 JDK.  Set JAVA_HOME, or have `java` on PATH.
#   - A completed native GN build in out/harmony_arm64_Release.
#
#   Signing (--signed only) is configured separately; see the signing section
#   at the end of this file.

set -e

# Java JDK.  hvigor 6.x is built and tested against Java 17.
if [ -z "${JAVA_HOME:-}" ] && [ "$(uname -s)" = "Darwin" ] && \
   [ -x /usr/libexec/java_home ]; then
  JAVA_HOME=$(/usr/libexec/java_home -v 17 2>/dev/null || true)
fi
if [ -z "${JAVA_HOME:-}" ] && command -v java >/dev/null 2>&1; then
  # Resolve `java` through any symlinks and strip the trailing /bin/java.
  JAVA_BIN=$(command -v java)
  if command -v readlink >/dev/null 2>&1; then
    JAVA_BIN=$(readlink -f "${JAVA_BIN}" 2>/dev/null || echo "${JAVA_BIN}")
  fi
  JAVA_HOME=$(dirname "$(dirname "${JAVA_BIN}")")
fi
if [ -z "${JAVA_HOME:-}" ] || [ ! -x "${JAVA_HOME}/bin/java" ]; then
  echo "[build_hap] a Java 17 JDK is required; set JAVA_HOME or put java on PATH."
  exit 2
fi
export JAVA_HOME
export PATH=${JAVA_HOME}/bin:${PATH}

# Repo paths
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
LYNXTRON_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
GN_OUT_DIR=${LYNXTRON_ROOT}/out/harmony_arm64_Release
LIBS_DIR=${SCRIPT_DIR}/entry/libs/arm64-v8a

# OHOS command-line tools.
#
# hvigor resolves compileSdkVersion against the SDK bundled with the tools it
# runs from, and reports nothing but "SDK component missing" when that SDK is
# older than the project asks for.  Several Command Line Tools releases are
# usually installed side by side, so rather than hard-coding one directory
# name, pick the newest installation that actually satisfies build-profile's
# compileSdkVersion and fail with a message that names the shortfall.
REQUIRED_API=$(sed -n 's/.*"compileSdkVersion" *: *"[^(]*(\([0-9]\+\))".*/\1/p' \
               "${SCRIPT_DIR}/build-profile.json5" | head -n1)
REQUIRED_API=${REQUIRED_API:-24}

# api_version_of <command-line-tools dir> -> apiVersion from version.txt, or ''.
api_version_of() {
  sed -n 's/^apiVersion *: *\([0-9]\+\).*/\1/p' "$1/version.txt" 2>/dev/null | head -n1
}

if [ -n "${COMMAND_LINE_TOOLS:-}" ]; then
  :
elif [ -n "${DEVECO_SDK_HOME:-}" ]; then
  COMMAND_LINE_TOOLS=$(cd "${DEVECO_SDK_HOME}/.." && pwd)
else
  # Roots that may hold several `<name>/command-line-tools` installations, plus
  # OHOS_SDK_VERSION for callers that still pin one explicitly.
  CLT_CANDIDATES=()
  if [ -n "${OHOS_SDK_VERSION:-}" ]; then
    CLT_CANDIDATES+=("${OHOS_TOOLS}/${OHOS_SDK_VERSION}/command-line-tools")
  fi
  for ROOT in "${LYNXTRON_ROOT}/deveco" "${LYNXTRON_ROOT}/.." "${OHOS_TOOLS:-}"; do
    [ -n "${ROOT}" ] && [ -d "${ROOT}" ] || continue
    for CANDIDATE in "${ROOT}"/command-line-tools "${ROOT}"/*/command-line-tools; do
      if [ -d "${CANDIDATE}/sdk" ]; then
        CLT_CANDIDATES+=("${CANDIDATE}")
      fi
    done
  done

  BEST_API=0
  for CANDIDATE in "${CLT_CANDIDATES[@]}"; do
    API=$(api_version_of "${CANDIDATE}")
    [ -n "${API}" ] || continue
    if [ "${API}" -ge "${REQUIRED_API}" ] && [ "${API}" -gt "${BEST_API}" ]; then
      BEST_API=${API}
      COMMAND_LINE_TOOLS=${CANDIDATE}
    fi
  done

  if [ -z "${COMMAND_LINE_TOOLS:-}" ]; then
    echo "[build_hap] ERROR: no Command Line Tools installation provides API ${REQUIRED_API}"
    echo "[build_hap]        (build-profile.json5 compileSdkVersion). Searched:"
    for CANDIDATE in "${CLT_CANDIDATES[@]}"; do
      API=$(api_version_of "${CANDIDATE}")
      echo "[build_hap]          ${CANDIDATE} (apiVersion ${API:-unknown})"
    done
    echo "[build_hap]        Set COMMAND_LINE_TOOLS or DEVECO_SDK_HOME to a newer install."
    exit 2
  fi
fi

export DEVECO_SDK_HOME=${DEVECO_SDK_HOME:-${COMMAND_LINE_TOOLS}/sdk}
export PATH=${DEVECO_SDK_HOME}:${PATH}
export PATH=${COMMAND_LINE_TOOLS}/bin:${PATH}
export PATH=${COMMAND_LINE_TOOLS}/ohpm/bin:${PATH}
export PATH=${COMMAND_LINE_TOOLS}/tool/node/bin:${PATH}
export PATH=${COMMAND_LINE_TOOLS}/hvigor/bin:${PATH}
echo "[build_hap] command line tools: ${COMMAND_LINE_TOOLS} (apiVersion $(api_version_of "${COMMAND_LINE_TOOLS}"))"

cd "${SCRIPT_DIR}"

# A target-level clean removes these runtime files without necessarily making
# `lynxtron_app` rebuild them.  Regenerate them before staging, rather than
# silently packaging stale copies left in entry/src/main/resources/resfile.
ninja -C "${GN_OUT_DIR}" icudtl.dat snapshot_blob.bin default_app_asar

# Stage both .so files:
#   liblynxtron.so       — big main library (chromium + V8 + Node + Lynx)
#   liblynxtron_napi.so  — small OHOS NAPI bridge that ETS imports
mkdir -p "${LIBS_DIR}"
for SO in liblynxtron.so liblynxtron_napi.so; do
  if [ -f "${GN_OUT_DIR}/${SO}" ]; then
    cp "${GN_OUT_DIR}/${SO}" "${LIBS_DIR}/"
    echo "[build_hap] staged ${SO} ($(stat -c%s "${LIBS_DIR}/${SO}") bytes)"
  else
    echo "[build_hap] WARNING: ${GN_OUT_DIR}/${SO} not found."
  fi
done

# Stage runtime resources into the HAP's resfile directory so the chromium
# base / Node.js / V8 init code can locate them at runtime via
# /data/storage/el1/bundle/entry/resources/resfile/.
RESFILE_DIR=${SCRIPT_DIR}/entry/src/main/resources/resfile
mkdir -p "${RESFILE_DIR}"

# Top-level files: ICU data, V8 snapshots, default app asar bundle.
for RES in icudtl.dat snapshot_blob.bin v8_context_snapshot.bin; do
  if [ -f "${GN_OUT_DIR}/${RES}" ]; then
    cp "${GN_OUT_DIR}/${RES}" "${RESFILE_DIR}/"
    echo "[build_hap] staged resfile/${RES} ($(stat -c%s "${RESFILE_DIR}/${RES}") bytes)"
  else
    echo "[build_hap] (skip) ${GN_OUT_DIR}/${RES} not found."
  fi
done

# Node.js bootstrap asar — searched by NodeBindings::CreateEnvironment via
# appSearchPaths = {"app.asar", "app", "default_app.asar"} relative to
# resourcesPath = DIR_ASSETS(resfile) + "/resources".
mkdir -p "${RESFILE_DIR}/resources"
if [ -f "${GN_OUT_DIR}/resources/default_app.asar" ]; then
  cp "${GN_OUT_DIR}/resources/default_app.asar" "${RESFILE_DIR}/resources/"
  echo "[build_hap] staged resfile/resources/default_app.asar ($(stat -c%s "${RESFILE_DIR}/resources/default_app.asar") bytes)"
else
  echo "[build_hap] (skip) ${GN_OUT_DIR}/resources/default_app.asar not found — build src:default_app_asar first."
fi

# Lynx JS core runtime. BTSRuntime::ReadCoreJS asks for it as
# `assets://lynx_core.js`, which lynx-resource-fetcher.ts resolves against
# resourcesPath. Without it the background JS runtime never defines `loadCard`,
# App::LoadApp fails with kAppLoadFailed, and every JS event -- bindtap, input,
# all of it -- is dropped even though lepus still renders the first screen.
# Build it with:
#   lynx/tools/js_tools/build.py --platform android   (harmony uses the android
#   flavor, same as lynx/explorer/harmony/script/build.py does)
LYNX_CORE_JS=${LYNX_CORE_JS_OVERRIDE:-${LYNXTRON_ROOT}/lynx/js_libraries/lynx-core/output/lynx_core.js}
if [ ! -f "${LYNX_CORE_JS}" ]; then
  echo "[build_hap] lynx_core.js is absent; building the Android-compatible runtime."
  (
    cd "${LYNXTRON_ROOT}/lynx"
    python3 tools/js_tools/build.py --platform android
  )
fi
if [ -f "${LYNX_CORE_JS}" ]; then
  cp "${LYNX_CORE_JS}" "${RESFILE_DIR}/resources/lynx_core.js"
  echo "[build_hap] staged resfile/resources/lynx_core.js ($(stat -c%s "${RESFILE_DIR}/resources/lynx_core.js") bytes)"
else
  echo "[build_hap] ERROR: ${LYNX_CORE_JS} was not generated."
  echo "[build_hap]        Refusing to package an app with a non-functional Lynx JS runtime."
  exit 1
fi

# Step 2 render: stage a Lynx demo bundle as resfile/resources/main.lynx.bundle.
# default_app/main.ts loads this staged bundle in preference to its built-in
# welcome app, so swapping it here changes which @lynx-example demo renders on
# the device — used to verify Lynxtron's rendering completeness across
# subsystems (layout/text/image/scroll) without any code change.
#
# Pick the demo with the LYNX_DEMO env var (default: view). Either a shorthand
# key from the table below, or an absolute path to any *.lynx.bundle.
# Default to Lynxtron's packaged default application so device validation
# immediately shows whether the real application bootstrapped.  Individual
# Lynx demo bundles remain available via e.g. `LYNX_DEMO=view`.
LYNX_DEMO=${LYNX_DEMO:-none}
PNPM_DIR=${SCRIPT_DIR}/../lynx/node_modules/.pnpm
case "${LYNX_DEMO}" in
  view)   LYNX_BUNDLE_SRC=${PNPM_DIR}/@lynx-example+view@0.3.0/node_modules/@lynx-example/view/dist/main.lynx.bundle ;;
  image)  LYNX_BUNDLE_SRC=${PNPM_DIR}/@lynx-example+image@0.3.0/node_modules/@lynx-example/image/dist/main.lynx.bundle ;;
  text)   LYNX_BUNDLE_SRC=${PNPM_DIR}/@lynx-example+text@0.6.2/node_modules/@lynx-example/text/dist/text_style.lynx.bundle ;;
  text-shadow) LYNX_BUNDLE_SRC=${PNPM_DIR}/@lynx-example+text@0.6.2/node_modules/@lynx-example/text/dist/shadow_and_stroke.lynx.bundle ;;
  list)   LYNX_BUNDLE_SRC=${PNPM_DIR}/@lynx-example+list@0.3.0/node_modules/@lynx-example/list/dist/base.lynx.bundle ;;
  waterfall) LYNX_BUNDLE_SRC=${PNPM_DIR}/@lynx-example+list@0.3.0/node_modules/@lynx-example/list/dist/waterfall.lynx.bundle ;;
  none)   LYNX_BUNDLE_SRC="" ;;  # fall back to built-in default_app welcome page
  /*)     LYNX_BUNDLE_SRC="${LYNX_DEMO}" ;;  # explicit absolute path
  *)      LYNX_BUNDLE_SRC="${LYNX_DEMO}" ;;
esac
echo "[build_hap] LYNX_DEMO=${LYNX_DEMO}"
if [ "${LYNX_DEMO}" = "none" ]; then
  rm -f "${RESFILE_DIR}/resources/main.lynx.bundle"
  echo "[build_hap] no demo staged — will render built-in default_app welcome page"
elif [ -f "${LYNX_BUNDLE_SRC}" ]; then
  cp "${LYNX_BUNDLE_SRC}" "${RESFILE_DIR}/resources/main.lynx.bundle"
  echo "[build_hap] staged resfile/resources/main.lynx.bundle from ${LYNX_BUNDLE_SRC} ($(stat -c%s "${RESFILE_DIR}/resources/main.lynx.bundle") bytes)"
else
  echo "[build_hap] (skip) ${LYNX_BUNDLE_SRC} not found — no demo staged."
fi

# Configure registries (use Huawei mirrors so ohpm install can pull
# @ohos packages from inside CN; same as chromium132 hap_build flow).
npm config set registry=https://repo.huaweicloud.com/repository/npm/
npm config set @ohos:registry=https://repo.harmonyos.com/npm/
ohpm config set registry https://repo.harmonyos.com/ohpm/
ohpm config set strict_ssl false

# Refresh hvigor cache to avoid stale plugin state.
rm -rf ~/.hvigor

# Pull workspace + module deps.
ohpm install --all

# Build.
hvigorw clean --daemon
hvigorw --mode module -p product=default -p buildMode=release \
        assembleHap --analyze=normal --parallel --incremental --daemon

# Locate the unsigned hap.
HAP_OUT_DIR=${SCRIPT_DIR}/entry/build/default/outputs/default
UNSIGNED_HAP=${HAP_OUT_DIR}/entry-default-unsigned.hap
echo ""
echo "[build_hap] unsigned hap: ${UNSIGNED_HAP}"
ls -la "${UNSIGNED_HAP}" 2>/dev/null || echo "[build_hap] (not produced)"

# Optional signing.
#
# The certificate, its alias and its passwords are developer-specific secrets,
# so nothing about them is stored in the repository.  Supply them through the
# environment, or through an untracked config file next to this script:
#
#   harmony_app/signing.local.env        (git-ignored)
#
#     SIGN_CERT_DIR=/path/to/your/ohos/certs
#     SIGN_KEY_ALIAS=your-key-alias
#     SIGN_CERT_FILE=your-cert.cer            # relative to SIGN_CERT_DIR
#     SIGN_PROFILE_FILE=your-profile.p7b      # relative to SIGN_CERT_DIR
#     SIGN_KEYSTORE_FILE=your-keystore.p12    # relative to SIGN_CERT_DIR
#     SIGN_KEY_PWD=...
#     SIGN_KEYSTORE_PWD=...                   # defaults to SIGN_KEY_PWD
#
# Set SIGN_ENV_FILE to point somewhere else, e.g. a path outside the checkout.
if [ "$1" = "--signed" ]; then
  SIGN_ENV_FILE=${SIGN_ENV_FILE:-${SCRIPT_DIR}/signing.local.env}
  if [ -f "${SIGN_ENV_FILE}" ]; then
    # shellcheck disable=SC1090
    . "${SIGN_ENV_FILE}"
  fi

  SIGN_KEYSTORE_PWD=${SIGN_KEYSTORE_PWD:-${SIGN_KEY_PWD:-}}
  SIGN_SIG_ALG=${SIGN_SIG_ALG:-SHA256withECDSA}
  # hap-sign-tool.jar ships inside the Command Line Tools.
  SIGN_TOOL=${SIGN_TOOL:-${COMMAND_LINE_TOOLS}/sdk/default/openharmony/toolchains/lib/hap-sign-tool.jar}

  MISSING=""
  for VAR in SIGN_CERT_DIR SIGN_KEY_ALIAS SIGN_CERT_FILE SIGN_PROFILE_FILE \
             SIGN_KEYSTORE_FILE SIGN_KEY_PWD SIGN_KEYSTORE_PWD; do
    eval "VALUE=\${${VAR}:-}"
    if [ -z "${VALUE}" ]; then
      MISSING="${MISSING} ${VAR}"
    fi
  done
  if [ -n "${MISSING}" ]; then
    echo "[build_hap] signing requested but not configured; missing:${MISSING}"
    echo "[build_hap] set them in the environment or in ${SIGN_ENV_FILE}"
    echo "[build_hap] the unsigned hap above is still usable."
    exit 2
  fi

  if [ ! -f "${SIGN_TOOL}" ]; then
    echo "[build_hap] hap-sign-tool.jar not found at ${SIGN_TOOL}, skipping signing."
    exit 0
  fi
  if [ ! -d "${SIGN_CERT_DIR}" ]; then
    echo "[build_hap] cert dir ${SIGN_CERT_DIR} not found, skipping signing."
    exit 0
  fi

  SIGNED_OUT="${HAP_OUT_DIR}/lynxtron-default-signed.hap"
  rm -f "${SIGNED_OUT}"
  java -jar "${SIGN_TOOL}" sign-app \
    -keyAlias      "${SIGN_KEY_ALIAS}" \
    -signAlg       "${SIGN_SIG_ALG}" \
    -mode          'localSign' \
    -appCertFile   "${SIGN_CERT_DIR}/${SIGN_CERT_FILE}" \
    -profileFile   "${SIGN_CERT_DIR}/${SIGN_PROFILE_FILE}" \
    -inFile        "${UNSIGNED_HAP}" \
    -outFile       "${SIGNED_OUT}" \
    -keystoreFile  "${SIGN_CERT_DIR}/${SIGN_KEYSTORE_FILE}" \
    -keyPwd        "${SIGN_KEY_PWD}" \
    -keystorePwd   "${SIGN_KEYSTORE_PWD}"
  echo ""
  echo "[build_hap] signed hap: ${SIGNED_OUT}"
fi
