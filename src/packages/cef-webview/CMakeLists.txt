# Copyright (c) 2014 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

# For VS2022 and Xcode 12+ support.
cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Only generate Debug and Release configuration types.
set(CMAKE_CONFIGURATION_TYPES Debug Release)

# Project name.
project(cef_extension)

# Use folders in the resulting project files.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

#
if(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
  set(PLATFORM_PATH "darwin")
  set(FRAMEWORK_DST_PATH ${CMAKE_BINARY_DIR}/frameworks)
endif()

set(CEF_WEBVIEW_HELPER_NAME "LynxtronWebview")
set(CEF_WEBVIEW_HELPER_BUNDLE_ID "org.lynxjs.lynxtron.webview.helper")
set(PROJECT_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../..)
set(LYNX_ROOT_DIR ${PROJECT_ROOT_DIR}/lynx)

#
# CEF_ROOT setup.
# This variable must be set to locate the binary distribution.
#
set(CEF_ROOT "${PROJECT_ROOT_DIR}/third_party/cef_binary")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CEF_ROOT}/cmake")

#
# Load the CEF configuration.
#
# Execute FindCEF.cmake which must exist in CMAKE_MODULE_PATH.
find_package(CEF REQUIRED)

#
# Define CEF-based targets.
#
# Include the libcef_dll_wrapper target.
# Comes from the libcef_dll/CMakeLists.txt file in the binary distribution
# directory.
add_subdirectory(${CEF_LIBCEF_DLL_WRAPPER_PATH} libcef_dll_wrapper)

execute_process(
  COMMAND node -p
          "require('node:path').dirname(require.resolve('@lynx-js/lynx-library-headers/package.json'))"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  OUTPUT_VARIABLE LYNX_LIBRARY_HEADERS_DIR
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
)

if(NOT LYNX_LIBRARY_HEADERS_DIR)
  message(
    FATAL_ERROR
    "Unable to resolve @lynx-js/lynx-library-headers. Run yarn install first."
  )
endif()

include("${LYNX_LIBRARY_HEADERS_DIR}/cmake/lynx-library-headers.cmake")

execute_process(
  COMMAND node -p
          "require('node:path').dirname(require.resolve('node-addon-api/package.json'))"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  OUTPUT_VARIABLE NODE_ADDON_API_DIR
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
)

if(NOT NODE_ADDON_API_DIR)
  message(FATAL_ERROR "Unable to resolve node-addon-api. Run yarn install first.")
endif()

set(CEF_PLUGIN_SRC_DIR "${LYNX_ROOT_DIR}/lynx/platform/embedder/plugin/cef/src")
if(NOT EXISTS "${CEF_PLUGIN_SRC_DIR}")
  set(CEF_PLUGIN_SRC_DIR "${LYNX_ROOT_DIR}/platform/embedder/plugin/cef/src")
endif()
add_subdirectory(${CEF_PLUGIN_SRC_DIR} cef_webview)

set(CEF_EXTENSION_TARGET "cef_extension")
set(CEF_EXTENSION_SRCS
  bindings/bind_napi.cc
  bindings/register_webview.cc
  )

add_library(${CEF_EXTENSION_TARGET} SHARED ${CEF_EXTENSION_SRCS})
target_compile_definitions(${CEF_EXTENSION_TARGET} PRIVATE USE_PRIMJS_NAPI)

# Add /FS option to resolve PDB file access conflicts
if(MSVC)
  target_compile_options(${CEF_EXTENSION_TARGET} PRIVATE /FS)
endif()
target_include_directories(${CEF_EXTENSION_TARGET} PRIVATE
  ${LYNX_LIBRARY_HEADERS_DIR}/include
  ${CMAKE_JS_INC}
  ${NODE_ADDON_API_DIR}
  ${PROJECT_ROOT_DIR}
  ${LYNX_ROOT_DIR}
  ${LYNX_ROOT_DIR}/lynx
  ${LYNX_ROOT_DIR}/lynx/platform/embedder/public
  ${LYNX_ROOT_DIR}/platform/embedder/public
  )

add_dependencies(${CEF_EXTENSION_TARGET} cef_webview)
if (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
  target_link_libraries(${CEF_EXTENSION_TARGET}
    cef_webview
    libcef_dll_wrapper 
    ${CEF_STANDARD_LIBS}
    )
endif()
if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
  # Add libcef logical target
  ADD_LOGICAL_TARGET("libcef_lib" "${CEF_LIB_DEBUG}" "${CEF_LIB_RELEASE}")

  add_executable(cef_subprocess WIN32 bindings/cef_subprocess_win.cc)
  target_include_directories(cef_subprocess PRIVATE ${CEF_ROOT})
  target_link_libraries(
    cef_subprocess PRIVATE cef_webview libcef_dll_wrapper libcef_lib
                           ${CEF_STANDARD_LIBS}
  )

  target_link_libraries(${CEF_EXTENSION_TARGET} PRIVATE
    cef_webview
    libcef_dll_wrapper 
    libcef_lib
    ${CEF_STANDARD_LIBS}
    imm32.lib
    )
  lynx_link_lynxtron_runtime(${CEF_EXTENSION_TARGET})

  # Keep the CEF runtime next to the Node addon. The Windows loader resolves
  # libcef.dll and its dependencies from this directory, and CEF resolves its
  # resource packs and locales from the same module directory by default.
  COPY_FILES(
    "${CEF_EXTENSION_TARGET}"
    "${CEF_BINARY_FILES}"
    "${CEF_BINARY_DIR}"
    "$<TARGET_FILE_DIR:${CEF_EXTENSION_TARGET}>"
  )
  COPY_FILES(
    "${CEF_EXTENSION_TARGET}"
    "${CEF_RESOURCE_FILES}"
    "${CEF_RESOURCE_DIR}"
    "$<TARGET_FILE_DIR:${CEF_EXTENSION_TARGET}>"
  )
endif()

set_target_properties(${CEF_EXTENSION_TARGET} PROPERTIES
  PREFIX ""
  SUFFIX ".node"
)

# Display configuration settings.
PRINT_CEF_CONFIG()
