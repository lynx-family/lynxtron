cmake_minimum_required(VERSION 3.20)
project(lynx_demo_extension LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(NODE_ADDON_API_DIR "${CMAKE_SOURCE_DIR}/node_modules/node-addon-api")

# Remove global definition to avoid affecting bindings_lib

# 1. Create OBJECT library for bindings (uses NODE_ADDON_API_DIR)
add_library(bindings_lib OBJECT
  bindings/bind_napi.cc
)
target_include_directories(bindings_lib PRIVATE
  ${CMAKE_JS_INC}
  ${CMAKE_SOURCE_DIR}
  ${NODE_ADDON_API_DIR}
  node_modules/@lynx-js/extension-builder/include/
  include/
)
# Bindings do NOT use weak-node-api namespace remapping

# 2. Create OBJECT library for module (uses weak-node-api)
add_library(module_lib OBJECT
  module/demo_extension_module.cc
  module/demo_extension_module.h
  module/demo_view.cc
  module/demo_view.h
)
target_include_directories(module_lib PRIVATE
  ${CMAKE_JS_INC}
  ${CMAKE_SOURCE_DIR}
  node_modules/@lynx-js/extension-builder/include/
  include/
)
# Module uses weak-node-api namespace remapping
target_compile_definitions(module_lib PRIVATE
  NAPI_CPP_CUSTOM_NAMESPACE=weak_node_api
  USE_WEAK_SUFFIX_NAPI
  NAPI_DISABLE_CPP_RTTI
  NAPI_DISABLE_CPP_EXCEPTIONS
)
if(MSVC)
  target_compile_options(module_lib PRIVATE /EHsc /bigobj)
else()
  target_compile_options(module_lib PRIVATE -fPIC)
endif()

# 3. Main library links objects from both
add_library(${PROJECT_NAME} SHARED
  ${CMAKE_JS_SRC}
  $<TARGET_OBJECTS:bindings_lib>
  $<TARGET_OBJECTS:module_lib>
)

set_target_properties(${PROJECT_NAME} PROPERTIES
  PREFIX ""
  SUFFIX ".node"
)

# Main target includes (common ones, or just empty if handled by object libs)
target_include_directories(${PROJECT_NAME} PRIVATE
  ${CMAKE_JS_INC}
  ${CMAKE_SOURCE_DIR}
  include/
)


if(MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE /EHsc /bigobj)
  target_link_libraries(${PROJECT_NAME} PRIVATE
    "${CMAKE_SOURCE_DIR}/node_modules/@lynx-js/lynxtron/dist/win/lxtn.lib"
  )
else()
  target_compile_options(${PROJECT_NAME} PRIVATE -fPIC)
endif()

if(APPLE)
  set_target_properties(${PROJECT_NAME} PROPERTIES
    MACOSX_RPATH ON
  )
  # Add the path containing the framework to CMAKE_FRAMEWORK_PATH
  list(APPEND CMAKE_FRAMEWORK_PATH "${CMAKE_SOURCE_DIR}/node_modules/@lynx-js/lynxtron/dist/darwin/arm64/lynxtron.app/Contents/Frameworks")
  
  # Find the library (framework)
  find_library(LYNXTRON_FRAMEWORK NAMES "Lynxtron Framework")
  
  if(LYNXTRON_FRAMEWORK)
    target_link_libraries(${PROJECT_NAME} PRIVATE
      ${LYNXTRON_FRAMEWORK}
    )
  else()
    message(FATAL_ERROR "Lynxtron Framework not found")
  endif()
endif()