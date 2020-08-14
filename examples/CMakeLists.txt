add_executable(readme readme.cpp)
target_link_libraries(readme PRIVATE cppcoro::http fmt::fmt)

add_executable(hello_world hello_world.cpp)
target_link_libraries(hello_world PRIVATE cppcoro::http fmt::fmt)
