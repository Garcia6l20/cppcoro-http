#basic_test(test_ws_server.cpp)
add_executable(test_ws_server test_ws_server.cpp)
target_link_libraries(test_ws_server cppcoro::http cppcoro-http-test-main)
