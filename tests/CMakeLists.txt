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
  add_executable(${_target} ${_filename})
  target_link_libraries(${_target} PRIVATE cppcoro-http-test-main)
  catch_discover_tests(${_target})
endmacro()

macro(add_tests)
  set(test_file_list "${ARGN}")
  foreach(test_file IN LISTS test_file_list)
    basic_test(${test_file})
  endforeach()
endmacro()

add_tests(
  http/test_requests.cpp
  http/test_route_controller.cpp
  http/test_chunked.cpp
  http/test_uri.cpp
  net/test_server.cpp
  )

add_subdirectory(web_socket)

if(CPPCORO_HTTP_MBEDTLS)
  add_tests(ssl/test_ssl_socket.cpp)
endif()
