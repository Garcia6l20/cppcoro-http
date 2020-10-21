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
option(CPPCORO_DEVEL "Fetch cppcoro for development" OFF)
if(CPPCORO_DEVEL)
  FetchContent_Declare(_fetch_cppcoro
    GIT_REPOSITORY https://github.com/Garcia6l20/cppcoro
    GIT_TAG io_ring
    )
  FetchContent_MakeAvailable(_fetch_cppcoro)
  add_library(cppcoro::cppcoro ALIAS cppcoro)
else()
  find_package(cppcoro REQUIRED)
endif()

option(CPPCORO_HTTP_MBEDTLS "Enable SSL support with mbedTLS" OFF)
if(CPPCORO_HTTP_MBEDTLS)
  find_package(mbedtls REQUIRED)
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
    fmt/7.0.1
    spdlog/1.7.0
    ctre/2.8.2
  BASIC_SETUP CMAKE_TARGETS
  BUILD outdated)

#
# cppcoro/http
#
add_library(${PROJECT_NAME} STATIC
  include/cppcoro/tcp/tcp.hpp
  include/cppcoro/http/http.hpp
  include/cppcoro/http/http_message.hpp
  include/cppcoro/http/http_request.hpp
  include/cppcoro/http/http_response.hpp
  include/cppcoro/http/http_server.hpp
  include/cppcoro/http/http_client.hpp
  include/cppcoro/http/http_connection.hpp
  include/cppcoro/http/request_processor.hpp
  include/cppcoro/http/route_controller.hpp
  include/cppcoro/http/route_parameter.hpp

  include/cppcoro/http/details/router.hpp
  include/cppcoro/http/details/static_parser_handler.hpp

  include/cppcoro/details/function_traits.hpp
  include/cppcoro/details/type_index.hpp

  src/http.cpp
  )
target_include_directories(${PROJECT_NAME} PUBLIC include)
target_link_libraries(${PROJECT_NAME} PUBLIC
  cppcoro::cppcoro
  http_parser::http_parser
  CONAN_PKG::ctre
  CONAN_PKG::fmt
  CONAN_PKG::spdlog)
target_precompile_headers(${PROJECT_NAME} INTERFACE
  <ctre/functions.hpp>
  <ctll/fixed_string.hpp>
  )
add_library(cppcoro::http ALIAS ${PROJECT_NAME})

if(CPPCORO_HTTP_MBEDTLS)
  target_link_libraries(${PROJECT_NAME} INTERFACE
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
