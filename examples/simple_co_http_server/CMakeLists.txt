conan_cmake_run(
  REQUIRES
    lyra/1.4.0
  BASIC_SETUP CMAKE_TARGETS
  BUILD outdated)

add_executable(simple_co_http_server main.cpp)
target_link_libraries(simple_co_http_server cppcoro::http CONAN_PKG::lyra)
