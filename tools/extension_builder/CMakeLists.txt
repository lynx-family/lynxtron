cmake_minimum_required(VERSION 3.20)
project(lynx_demo_extension LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(NODE_ADDON_API_DIR "${CMAKE_SOURCE_DIR}/node_modules/node-addon-api")

add_definitions(-DUSE_PRIMJS_NAPI)

add_library(${PROJECT_NAME} SHARED
  ${CMAKE_JS_SRC}              
  bindings/bind_napi.cc
  module/demo_extension_module.cc
  module/demo_extension_module.h
  module/demo_view.cc
  module/demo_view.h
)

set_target_properties(${PROJECT_NAME} PROPERTIES
  PREFIX ""
  SUFFIX ".node"
)

target_include_directories(${PROJECT_NAME} PRIVATE
  ${CMAKE_JS_INC}
  ${CMAKE_SOURCE_DIR}
  ${NODE_ADDON_API_DIR}
  node_modules/@lynx-js/extension-builder/include/
)

if(MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE /EHsc /bigobj)
else()
  target_compile_options(${PROJECT_NAME} PRIVATE -fPIC)
endif()

if(APPLE)
  set_target_properties(${PROJECT_NAME} PROPERTIES
    MACOSX_RPATH ON
  )
endif()