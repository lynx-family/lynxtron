if(NOT COMMAND lynx_resolve_node_package)
  function(lynx_resolve_node_package PACKAGE_NAME OUT_VAR)
    if(NOT DEFINED NODE_EXECUTABLE)
      find_program(NODE_EXECUTABLE NAMES node)
    endif()

    if(NOT NODE_EXECUTABLE)
      message(FATAL_ERROR "Unable to find node. Install Node.js before configuring Lynx library targets.")
    endif()

    if(NOT DEFINED LYNX_LIBRARY_PACKAGE_ROOT)
      set(LYNX_LIBRARY_PACKAGE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()

    execute_process(
      COMMAND
        "${NODE_EXECUTABLE}"
        -p
        "require('node:path').dirname(require.resolve('${PACKAGE_NAME}/package.json'))"
      WORKING_DIRECTORY "${LYNX_LIBRARY_PACKAGE_ROOT}"
      RESULT_VARIABLE NODE_PACKAGE_RESULT
      OUTPUT_VARIABLE NODE_PACKAGE_ROOT
      ERROR_VARIABLE NODE_PACKAGE_ERROR
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if(NOT NODE_PACKAGE_RESULT EQUAL 0 OR NOT NODE_PACKAGE_ROOT)
      string(REPLACE "\n" " " NODE_PACKAGE_ERROR "${NODE_PACKAGE_ERROR}")
      message(FATAL_ERROR "Unable to resolve ${PACKAGE_NAME}. Run npm install first. ${NODE_PACKAGE_ERROR}")
    endif()

    set(${OUT_VAR} "${NODE_PACKAGE_ROOT}" PARENT_SCOPE)
  endfunction()
endif()

get_filename_component(
  LYNX_LIBRARY_HEADERS_PACKAGE_ROOT
  "${CMAKE_CURRENT_LIST_DIR}/.."
  ABSOLUTE
)

function(lynx_resolve_lynxtron_import_library OUT_VAR)
  if(NOT WIN32)
    set(${OUT_VAR} "" PARENT_SCOPE)
    return()
  endif()

  if(DEFINED LYNXTRON_IMPORT_LIB)
    file(TO_CMAKE_PATH "${LYNXTRON_IMPORT_LIB}" LYNXTRON_IMPORT_LIB_PATH)
    if(NOT EXISTS "${LYNXTRON_IMPORT_LIB_PATH}")
      message(FATAL_ERROR "LYNXTRON_IMPORT_LIB does not exist: ${LYNXTRON_IMPORT_LIB_PATH}")
    endif()

    set(${OUT_VAR} "${LYNXTRON_IMPORT_LIB_PATH}" PARENT_SCOPE)
    return()
  endif()

  if(NOT DEFINED NODE_EXECUTABLE)
    find_program(NODE_EXECUTABLE NAMES node)
  endif()

  if(NOT NODE_EXECUTABLE)
    message(FATAL_ERROR "Unable to find node. Install Node.js before linking Lynxtron native libraries.")
  endif()

  if(NOT DEFINED LYNX_LIBRARY_PACKAGE_ROOT)
    set(LYNX_LIBRARY_PACKAGE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
  endif()

  execute_process(
    COMMAND
      "${NODE_EXECUTABLE}"
      -e
      "const path=require('node:path');const {createRequire}=require('node:module');try{const r=createRequire(path.join(process.argv[1],'package.json'));const p=r('@lynx-js/lynxtron/native-paths');process.stdout.write(p.importLibraryPath||'')}catch(e){process.exit(1)}"
      "${LYNX_LIBRARY_HEADERS_PACKAGE_ROOT}"
    WORKING_DIRECTORY "${LYNX_LIBRARY_PACKAGE_ROOT}"
    RESULT_VARIABLE LYNXTRON_NATIVE_PATHS_RESULT
    OUTPUT_VARIABLE LYNXTRON_IMPORT_LIB_FROM_PACKAGE
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )

  if(LYNXTRON_NATIVE_PATHS_RESULT EQUAL 0 AND LYNXTRON_IMPORT_LIB_FROM_PACKAGE)
    file(TO_CMAKE_PATH "${LYNXTRON_IMPORT_LIB_FROM_PACKAGE}" LYNXTRON_IMPORT_LIB_FROM_PACKAGE_PATH)
    if(EXISTS "${LYNXTRON_IMPORT_LIB_FROM_PACKAGE_PATH}")
      set(${OUT_VAR} "${LYNXTRON_IMPORT_LIB_FROM_PACKAGE_PATH}" PARENT_SCOPE)
      return()
    endif()
  endif()

  execute_process(
    COMMAND
      "${NODE_EXECUTABLE}"
      -p
      "const path=require('node:path');const {createRequire}=require('node:module');const r=createRequire(path.join(process.argv[1],'package.json'));let p;try{p=r.resolve('@lynx-js/lynxtron/package.json')}catch(e){p=r.resolve('@lynx-js/lynxtron')}path.dirname(p)"
      "${LYNX_LIBRARY_HEADERS_PACKAGE_ROOT}"
    WORKING_DIRECTORY "${LYNX_LIBRARY_PACKAGE_ROOT}"
    RESULT_VARIABLE LYNXTRON_PACKAGE_ROOT_RESULT
    OUTPUT_VARIABLE LYNXTRON_PACKAGE_ROOT
    ERROR_VARIABLE LYNXTRON_PACKAGE_ROOT_ERROR
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )

  if(NOT LYNXTRON_PACKAGE_ROOT_RESULT EQUAL 0 OR NOT LYNXTRON_PACKAGE_ROOT)
    string(REPLACE "\n" " " LYNXTRON_PACKAGE_ROOT_ERROR "${LYNXTRON_PACKAGE_ROOT_ERROR}")
    message(FATAL_ERROR "Unable to resolve @lynx-js/lynxtron. Run npm install first. ${LYNXTRON_PACKAGE_ROOT_ERROR}")
  endif()

  set(LYNXTRON_IMPORT_LIB_CANDIDATES
    "${LYNXTRON_PACKAGE_ROOT}/dist/lynxtron.dll.lib"
    "${LYNXTRON_PACKAGE_ROOT}/dist/win32/x64/lynxtron.dll.lib"
    "${LYNXTRON_PACKAGE_ROOT}/dist/win32/x64/lynxtron/lynxtron.dll.lib"
  )

  foreach(LYNXTRON_IMPORT_LIB_CANDIDATE IN LISTS LYNXTRON_IMPORT_LIB_CANDIDATES)
    if(EXISTS "${LYNXTRON_IMPORT_LIB_CANDIDATE}")
      set(${OUT_VAR} "${LYNXTRON_IMPORT_LIB_CANDIDATE}" PARENT_SCOPE)
      return()
    endif()
  endforeach()

  message(FATAL_ERROR
    "Unable to find lynxtron.dll.lib under @lynx-js/lynxtron. "
    "Run npm install so @lynx-js/lynxtron downloads the Windows runtime, "
    "or pass -DLYNXTRON_IMPORT_LIB=/absolute/path/to/lynxtron.dll.lib."
  )
endfunction()

function(lynx_link_lynxtron_runtime TARGET_NAME)
  if(NOT WIN32)
    return()
  endif()

  lynx_resolve_lynxtron_import_library(LYNXTRON_RESOLVED_IMPORT_LIB)
  target_link_libraries("${TARGET_NAME}" PRIVATE "${LYNXTRON_RESOLVED_IMPORT_LIB}")
endfunction()
