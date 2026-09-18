# The main library is produced by the GN build at the repository root and staged
# into entry/libs/arm64-v8a by build_hap.sh. hvigor packages .so files found
# under entry/libs directly, so this CMake project only guards that the prebuilt
# exists. Re-exporting liblynxtron.so through a CMake target here would put the
# same .so into the module twice (entry/libs + CMake intermediates) and hvigor
# fails ProcessLibs with a duplicated-files error.

cmake_minimum_required(VERSION 3.5.0)
project(lynxtron_hap)

set(LYNXTRON_GN_OUT_DIR
  "${CMAKE_CURRENT_SOURCE_DIR}/../../../libs/arm64-v8a"
  CACHE PATH "Path to the lynxtron GN out directory")

set(LYNXTRON_PREBUILT
  "${LYNXTRON_GN_OUT_DIR}/liblynxtron.so")

if(NOT EXISTS "${LYNXTRON_PREBUILT}")
  message(FATAL_ERROR
    "liblynxtron.so is missing at ${LYNXTRON_PREBUILT}. Run `ninja -C "
    "out/harmony_arm64_Release lynxtron_app` and `build_hap.sh` before "
    "building the HAP.")
endif()
