add_executable(readme readme.cpp)
target_link_libraries(readme PRIVATE cppcoro::http)

add_executable(hello_world hello_world.cpp)
target_link_libraries(hello_world PRIVATE cppcoro::http)

add_executable(echo echo.cpp)
target_link_libraries(echo PRIVATE cppcoro::http)

add_executable(echo_client echo_client.cpp)
target_link_libraries(echo_client PRIVATE cppcoro::http)

add_subdirectory(simple_co_http_server)
