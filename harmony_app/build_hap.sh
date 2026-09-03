#!/bin/bash
#
# Lynxtron HarmonyOS HAP build script.
# Builds the HarmonyOS application package from a completed native GN build.
#
# Usage:
#   ./build_hap.sh           # build unsigned hap (default)
#   ./build_hap.sh --signed  # also run b_sign_hap_release.sh
#
# Requires:
#   /opt/compilers/ohos_tools/commandline-tools-linux-x64-6.0.2.640/
#     command-line-tools/{hvigor/bin,ohpm/bin,bin,tool/node/bin}/
#   /opt/compilers/ohos_tools/jdk-17.0.6/
#   /opt/compilers/ohos_tools/tools/haps_signed/  (only for --signed)

set -e

OHOS_TOOLS=${OHOS_TOOLS:-/opt/compilers/ohos_tools}
OHOS_SDK_VERSION=${OHOS_SDK_VERSION:-commandline-tools-linux-x64-6.0.2.640}
HAP_SIGNED_DIR=${OHOS_TOOLS}/tools/haps_signed

# Java JDK
export JAVA_HOME=${OHOS_TOOLS}/jdk-17.0.6
export PATH=${JAVA_HOME}/bin:${PATH}
export CLASSPATH=.:${JAVA_HOME}/lib/dt.jar:${JAVA_HOME}/lib/tools.jar

# OHOS command-line tools
export DEVECO_SDK_HOME=${DEVECO_SDK_HOME:-${OHOS_TOOLS}/${OHOS_SDK_VERSION}/command-line-tools/sdk}
export PATH=${DEVECO_SDK_HOME}:${PATH}
export PATH=${OHOS_TOOLS}/${OHOS_SDK_VERSION}/command-line-tools/bin:${PATH}
export PATH=${OHOS_TOOLS}/${OHOS_SDK_VERSION}/command-line-tools/ohpm/bin:${PATH}
export PATH=${OHOS_TOOLS}/${OHOS_SDK_VERSION}/command-line-tools/tool/node/bin:${PATH}
export PATH=${OHOS_TOOLS}/${OHOS_SDK_VERSION}/command-line-tools/hvigor/bin:${PATH}

# Repo paths
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
LYNXTRON_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
GN_OUT_DIR=${LYNXTRON_ROOT}/out/harmony_arm64_Release
LIBS_DIR=${SCRIPT_DIR}/entry/libs/arm64-v8a

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
if [ -f "${LYNX_CORE_JS}" ]; then
  cp "${LYNX_CORE_JS}" "${RESFILE_DIR}/resources/lynx_core.js"
  echo "[build_hap] staged resfile/resources/lynx_core.js ($(stat -c%s "${RESFILE_DIR}/resources/lynx_core.js") bytes)"
else
  echo "[build_hap] WARNING: ${LYNX_CORE_JS} not found — Lynx JS runtime will"
  echo "[build_hap]          fail to load and no JS event will fire."
fi

# Step 2 render: stage a Lynx demo bundle as resfile/resources/main.lynx.bundle.
# default_app/main.ts loads this staged bundle in preference to its built-in
# welcome app, so swapping it here changes which @lynx-example demo renders on
# the device — used to verify Lynxtron's rendering completeness across
# subsystems (layout/text/image/scroll) without any code change.
#
# Pick the demo with the LYNX_DEMO env var (default: view). Either a shorthand
# key from the table below, or an absolute path to any *.lynx.bundle.
LYNX_DEMO=${LYNX_DEMO:-view}
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

# Optional signing. The certificate, its alias and its passwords are
# developer-specific secrets and are not stored in the repository; supply them
# through the environment.
if [ "$1" = "--signed" ]; then
  CERT_DIR=${SIGN_CERT_DIR:?set SIGN_CERT_DIR to your certificate directory}
  CERT_PW=${SIGN_KEY_PWD:?set SIGN_KEY_PWD to your keystore password}
  SIGN_TOOL="${HAP_SIGNED_DIR}/hap-sign-tool.jar"

  if [ ! -f "${SIGN_TOOL}" ]; then
    echo "[build_hap] hap-sign-tool.jar not found at ${SIGN_TOOL}, skipping signing."
    exit 0
  fi
  if [ ! -d "${CERT_DIR}" ]; then
    echo "[build_hap] cert dir ${CERT_DIR} not found, skipping signing."
    exit 0
  fi

  SIGNED_OUT="${HAP_OUT_DIR}/lynxtron-default-signed.hap"
  rm -f "${SIGNED_OUT}"
  java -jar "${SIGN_TOOL}" sign-app \
    -keyAlias "${SIGN_KEY_ALIAS:?set SIGN_KEY_ALIAS}" \
    -signAlg 'SHA256withECDSA' \
    -mode 'localSign' \
    -appCertFile "${CERT_DIR}/${SIGN_CERT_FILE:?set SIGN_CERT_FILE}" \
    -profileFile "${CERT_DIR}/${SIGN_PROFILE_FILE:?set SIGN_PROFILE_FILE}" \
    -inFile      "${UNSIGNED_HAP}" \
    -outFile     "${SIGNED_OUT}" \
    -keystoreFile "${CERT_DIR}/${SIGN_KEYSTORE_FILE:?set SIGN_KEYSTORE_FILE}" \
    -keyPwd      "${CERT_PW}" \
    -keystorePwd "${CERT_PW}"
  echo ""
  echo "[build_hap] signed hap: ${SIGNED_OUT}"
fi
