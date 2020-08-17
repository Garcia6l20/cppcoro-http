FetchContent_Declare(_fetch_catch
  GIT_REPOSITORY https://github.com/catchorg/catch2
  GIT_TAG master
  )
FetchContent_MakeAvailable(_fetch_catch)
include(${_fetch_catch_SOURCE_DIR}/contrib/Catch.cmake)

link_libraries(cppcoro::http Catch2::Catch2 fmt::fmt)

add_executable(test_route_controller test_route_controller.cpp)
catch_discover_tests(test_route_controller)

add_executable(test_server test_server.cpp)
catch_discover_tests(test_server)

