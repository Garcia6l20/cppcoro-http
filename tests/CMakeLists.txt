FetchContent_Declare(_fetch_catch
  GIT_REPOSITORY https://github.com/catchorg/catch2
  GIT_TAG master
  )
FetchContent_MakeAvailable(_fetch_catch)
include(${_fetch_catch_SOURCE_DIR}/contrib/Catch.cmake)

link_libraries(cppcoro::http Catch2::Catch2)

macro(basic_test _filename)
  get_filename_component(_target ${_filename} NAME_WE)
  add_executable(${_target} ${_filename})
  catch_discover_tests(${_target})
endmacro()

basic_test(test_requests.cpp)
basic_test(test_route_controller.cpp)
basic_test(test_server.cpp)
basic_test(test_chunked.cpp)

if(CPPCORO_HTTP_MBEDTLS)
  add_subdirectory(ssl)
endif()
