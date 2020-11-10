conan_cmake_run(
  REQUIRES
    catch2/2.13.2
  BASIC_SETUP CMAKE_TARGETS
  BUILD outdated)

include(${CONAN_BUILD_DIRS_CATCH2}/Catch.cmake)

add_library(cppcoro-http-test-main STATIC main.cpp)
target_link_libraries(cppcoro-http-test-main PUBLIC cppcoro::http CONAN_PKG::catch2)

macro(basic_test _filename)
  get_filename_component(_target ${_filename} NAME_WE)
  get_filename_component(_target_dir ${_filename} DIRECTORY)
  if (NOT "${_target_dir}" STREQUAL "")
    set(_target ${_target_dir}_${_target})
  endif()
  add_executable(${_target} ${_filename})
  target_link_libraries(${_target} PRIVATE cppcoro-http-test-main)
  catch_discover_tests(${_target})
  set_target_properties(${_target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)
endmacro()

macro(add_tests)
  set(test_file_list "${ARGN}")
  foreach(test_file IN LISTS test_file_list)
    basic_test(${test_file})
  endforeach()
endmacro()

if(CPPCORO_HTTP_MBEDTLS)
  add_tests(ssl/test_ssl_socket.cpp)
endif()

add_tests(
  router/test_router.cpp
  net/test_server.cpp
  http/test_server.cpp
#  http/test_requests.cpp
#  http/test_router.cpp
#  http/test_route_controller.cpp
  http/test_chunked.cpp
#  http/test_uri.cpp
  web_socket/test_server.cpp
  )

