FetchContent_Declare(_fetch_catch
  GIT_REPOSITORY https://github.com/catchorg/catch2
  GIT_TAG master
  )
FetchContent_MakeAvailable(_fetch_catch)
include(${_fetch_catch_SOURCE_DIR}/contrib/Catch.cmake)

add_executable(test_router test_router.cpp)
target_link_libraries(test_router PRIVATE cppcoro::http Catch2::Catch2 fmt::fmt)
catch_discover_tests(test_router)
