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

set(HOST_EXECUTABLE_NAME "lynxtron")
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
  )

add_library(${CEF_EXTENSION_TARGET} SHARED ${CEF_EXTENSION_SRCS})
target_compile_definitions(${CEF_EXTENSION_TARGET} PRIVATE USE_PRIMJS_NAPI)

# Add /FS option to resolve PDB file access conflicts
if(MSVC)
  target_compile_options(${CEF_EXTENSION_TARGET} PRIVATE /FS)
endif()
target_include_directories(${CEF_EXTENSION_TARGET} PRIVATE
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
  # Determine architecture
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(ARCHITECTURE "x64")
  else()
    set(ARCHITECTURE "x86")
  endif()
  
  target_link_libraries(${CEF_EXTENSION_TARGET} 
    cef_webview
    libcef_dll_wrapper 
    libcef_lib
    ${CEF_STANDARD_LIBS}
    "${CMAKE_SOURCE_DIR}/node_modules/lynxtron/dist/win32/${ARCHITECTURE}/lynxtron/lxtn.dll.lib"
    imm32.lib
    )
endif()

set_target_properties(${CEF_EXTENSION_TARGET} PROPERTIES
  PREFIX ""
  SUFFIX ".node"
)

# Display configuration settings.
PRINT_CEF_CONFIG()
