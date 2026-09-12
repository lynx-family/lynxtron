#!/bin/bash
#
# Stage native libraries and runtime resources into the HAP module directories.
# After running this, use DevEco Studio to build and sign the HAP.
#
# Usage:
#   ./stage_resources.sh              # default demo: view
#   ./stage_resources.sh image        # any shorthand: view/image/text/text-shadow/list/waterfall/none
#   ./stage_resources.sh /path/to/demo.lynx.bundle  # custom bundle
#
# Required: gantt-built artifacts in out/harmony_arm64_Release/

set -e

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
LYNXTRON_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
GN_OUT_DIR=${LYNXTRON_ROOT}/out/harmony_arm64_Release
LIBS_DIR=${SCRIPT_DIR}/entry/libs/arm64-v8a

cd "${SCRIPT_DIR}"

echo "=== Step 1: stage .so libraries ==="
mkdir -p "${LIBS_DIR}"
for SO in liblynxtron.so liblynxtron_napi.so; do
  if [ -f "${GN_OUT_DIR}/${SO}" ]; then
    cp "${GN_OUT_DIR}/${SO}" "${LIBS_DIR}/"
    echo "[stage] ${SO} ($(stat -c%s "${LIBS_DIR}/${SO}") bytes)"
  else
    echo "[stage] WARNING: ${GN_OUT_DIR}/${SO} not found."
  fi
done

echo ""
echo "=== Step 2: stage runtime resources ==="
RESFILE_DIR=${SCRIPT_DIR}/entry/src/main/resources/resfile
mkdir -p "${RESFILE_DIR}"

for RES in icudtl.dat snapshot_blob.bin v8_context_snapshot.bin; do
  if [ -f "${GN_OUT_DIR}/${RES}" ]; then
    cp "${GN_OUT_DIR}/${RES}" "${RESFILE_DIR}/"
    echo "[stage] resfile/${RES} ($(stat -c%s "${RESFILE_DIR}/${RES}") bytes)"
  else
    echo "[stage] (skip) ${GN_OUT_DIR}/${RES} not found."
  fi
done

mkdir -p "${RESFILE_DIR}/resources"
if [ -f "${GN_OUT_DIR}/resources/default_app.asar" ]; then
  cp "${GN_OUT_DIR}/resources/default_app.asar" "${RESFILE_DIR}/resources/"
  echo "[stage] resfile/resources/default_app.asar ($(stat -c%s "${RESFILE_DIR}/resources/default_app.asar") bytes)"
else
  echo "[stage] (skip) ${GN_OUT_DIR}/resources/default_app.asar not found — build src:default_app_asar first."
fi

echo ""
echo "=== Step 3: stage Lynx demo bundle ==="
LYNX_DEMO=${1:-view}
PNPM_DIR=${SCRIPT_DIR}/../lynx/node_modules/.pnpm
case "${LYNX_DEMO}" in
  view)   LYNX_BUNDLE_SRC=${PNPM_DIR}/@lynx-example+view@0.3.0/node_modules/@lynx-example/view/dist/main.lynx.bundle ;;
  image)  LYNX_BUNDLE_SRC=${PNPM_DIR}/@lynx-example+image@0.3.0/node_modules/@lynx-example/image/dist/main.lynx.bundle ;;
  text)   LYNX_BUNDLE_SRC=${PNPM_DIR}/@lynx-example+text@0.6.2/node_modules/@lynx-example/text/dist/text_style.lynx.bundle ;;
  text-shadow) LYNX_BUNDLE_SRC=${PNPM_DIR}/@lynx-example+text@0.6.2/node_modules/@lynx-example/text/dist/shadow_and_stroke.lynx.bundle ;;
  list)   LYNX_BUNDLE_SRC=${PNPM_DIR}/@lynx-example+list@0.3.0/node_modules/@lynx-example/list/dist/base.lynx.bundle ;;
  waterfall) LYNX_BUNDLE_SRC=${PNPM_DIR}/@lynx-example+list@0.3.0/node_modules/@lynx-example/list/dist/waterfall.lynx.bundle ;;
  none)   LYNX_BUNDLE_SRC="" ;;
  /*)     LYNX_BUNDLE_SRC="${LYNX_DEMO}" ;;
  *)      LYNX_BUNDLE_SRC="${LYNX_DEMO}" ;;
esac
echo "[stage] LYNX_DEMO=${LYNX_DEMO}"
if [ "${LYNX_DEMO}" = "none" ]; then
  rm -f "${RESFILE_DIR}/resources/main.lynx.bundle"
  echo "[stage] no demo staged — will render built-in default_app welcome page"
elif [ -f "${LYNX_BUNDLE_SRC}" ]; then
  cp "${LYNX_BUNDLE_SRC}" "${RESFILE_DIR}/resources/main.lynx.bundle"
  echo "[stage] resfile/resources/main.lynx.bundle ($(stat -c%s "${RESFILE_DIR}/resources/main.lynx.bundle") bytes)"
else
  echo "[stage] (skip) ${LYNX_BUNDLE_SRC} not found — no demo staged."
fi

echo ""
echo "=== Done. Open the project in DevEco Studio to build and sign the HAP. ==="
