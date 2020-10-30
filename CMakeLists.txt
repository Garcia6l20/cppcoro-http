cmake_minimum_required(VERSION 3.16)
project(cppcoro_http)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT EXISTS "${CMAKE_BINARY_DIR}/conan.cmake")
  message(STATUS "Downloading conan.cmake from https://github.com/conan-io/cmake-conan")
  file(DOWNLOAD "https://github.com/conan-io/cmake-conan/raw/v0.15/conan.cmake"
    "${CMAKE_BINARY_DIR}/conan.cmake")
endif()
include("${CMAKE_BINARY_DIR}/conan.cmake")

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

include(FetchContent)

#
# cppcoro
#
if(CPPCORO_PATH)
  add_subdirectory(${CPPCORO_PATH} cppcoro)
  add_library(cppcoro::cppcoro ALIAS cppcoro)
else()
  find_package(cppcoro REQUIRED)
endif()

add_library(cppcoro_http_options INTERFACE)

option(CPPCORO_HTTP_MBEDTLS "Enable SSL support with mbedTLS" OFF)
if(CPPCORO_HTTP_MBEDTLS)
  find_package(mbedtls REQUIRED)
  target_compile_definitions(cppcoro_http_options INTERFACE -DCPPCORO_HTTP_MBEDTLS=1)
endif()

#
# nodejs/http_parser
#
FetchContent_Declare(_fetch_http_parser
  GIT_REPOSITORY https://github.com/nodejs/http-parser
  GIT_TAG master
  CONFIGURE_COMMAND ""
  INSTALL_COMMAND ""
  BUILD_COMMAND ""
  )
FetchContent_MakeAvailable(_fetch_http_parser)
FetchContent_GetProperties(_fetch_http_parser)
if(NOT EXISTS ${_fetch_http_parser_SOURCE_DIR}/http_parser.c)
endif()
add_library(http_parser STATIC
  ${_fetch_http_parser_SOURCE_DIR}/http_parser.c
  ${_fetch_http_parser_SOURCE_DIR}/http_parser.h
  )
target_include_directories(http_parser PUBLIC ${_fetch_http_parser_SOURCE_DIR}/)
add_library(http_parser::http_parser ALIAS http_parser)

conan_cmake_run(
  REQUIRES
    fmt/7.0.3
    spdlog/1.8.0
    ctre/2.8.2
    nlohmann_json/3.9.1
  BASIC_SETUP CMAKE_TARGETS
  BUILD outdated)

#
# cppcoro/http
#
add_library(${PROJECT_NAME} STATIC
  src/uri.cpp)

target_include_directories(${PROJECT_NAME} PUBLIC include)
target_link_libraries(${PROJECT_NAME} PUBLIC
  cppcoro_http_options
  cppcoro::cppcoro
  http_parser::http_parser
  CONAN_PKG::ctre
  CONAN_PKG::fmt
  CONAN_PKG::spdlog
  CONAN_PKG::nlohmann_json)
target_precompile_headers(${PROJECT_NAME} PUBLIC
  <ctre/functions.hpp>
  <ctll/fixed_string.hpp>
  )
add_library(cppcoro::http ALIAS ${PROJECT_NAME})

if(CPPCORO_HTTP_MBEDTLS)
  target_link_libraries(${PROJECT_NAME} PUBLIC
    mbed::tls
    mbed::crypto
    mbed::x509
    )
endif()

option(BUILD_EXAMPLES "Build examples" ON)
if (BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

enable_testing()
if (BUILD_TESTING)
  add_subdirectory(tests)
endif()
