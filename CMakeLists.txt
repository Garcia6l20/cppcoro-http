cmake_minimum_required(VERSION 3.17)
project(cppcoro_http)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED  ON)

include(FetchContent)

#
# cppcoro
#
option(CPPCORO_DEVEL "Fetch cppcoro for development" ON)
if(CPPCORO_DEVEL)
  FetchContent_Declare(_fetch_cppcoro
    GIT_REPOSITORY https://github.com/Garcia6l20/cppcoro
    )
  FetchContent_MakeAvailable(_fetch_cppcoro)
else()
  find_package(cppcoro REQUIRED)
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

#
# hanickadot/compile-time-regular-expressions
#

set(CTRE_BUILD_TESTS OFF CACHE INTERNAL "")
set(CTRE_BUILD_PACKAGE OFF CACHE INTERNAL "")
FetchContent_Declare(_fetch_ctre
  GIT_REPOSITORY https://github.com/hanickadot/compile-time-regular-expressions
  GIT_TAG master
  )
FetchContent_MakeAvailable(_fetch_ctre)

#
# fmtlib/fmt
#

FetchContent_Declare(_fetch_fmt
  GIT_REPOSITORY https://github.com/fmtlib/fmt
  GIT_TAG master
  )
FetchContent_MakeAvailable(_fetch_fmt)

#
# cppcoro/http
#
add_library(${PROJECT_NAME} STATIC
  include/cppcoro/tcp/tcp.hpp
  include/cppcoro/http/http.hpp
  include/cppcoro/http/http_request.hpp
  include/cppcoro/http/http_server.hpp
  include/cppcoro/http/http_client.hpp
  include/cppcoro/http/http_connection.hpp
  include/cppcoro/http/router.hpp
  include/cppcoro/http/route_parameter.hpp
  src/http.cpp
  )
target_include_directories(${PROJECT_NAME} PUBLIC include)
target_link_libraries(${PROJECT_NAME} PUBLIC cppcoro::cppcoro http_parser::http_parser ctre::ctre)
target_precompile_headers(${PROJECT_NAME} INTERFACE
  <ctre/functions.hpp>
  <ctll/fixed_string.hpp>
  )
add_library(cppcoro::http ALIAS ${PROJECT_NAME})

option(BUILD_EXAMPLES "Build examples" ON)
if (BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

enable_testing()
if (BUILD_TESTING)
  add_subdirectory(tests)
endif()
