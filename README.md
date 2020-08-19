# CppCoro Http - http coroutine library for C++

The 'cppcoro-http' provides a set of classes for creating http servers/clients.
It is built on top of [cppcoro](https://github.com/lewissbaker/cppcoro) library.

## HTTP Server

- example:

```c++
struct session {
    int id = std::rand();
};

struct hello_controller : http::route_controller<
    R"(/hello/(\w+))",  // route definition
    session,
    hello_controller>
{
    // method handlers
    auto on_post(std::string_view who) -> task<http::response> {
        co_return http::response{http::status::HTTP_STATUS_OK,
            fmt::format("post at {}: hello {}", session().id, who)};
    }
    auto on_get(std::string_view who) -> task<http::response> {
        co_return http::response{http::status::HTTP_STATUS_OK,
            fmt::format("get at {}: hello {}", session().id, who)};
    }
};

io_service service;

auto do_serve = [&]() -> task<> {
    auto _ = on_scope_exit([&] {
        ios.stop();
    });
    http::controller_server<session, hello_controller> server{
        service,
        *net::ip_endpoint::from_string("127.0.0.1:4242")};
    co_await server.serve();
};
(void) sync_wait(when_all(
    do_serve(),
    [&]() -> task<> {
        service.process_events();
        co_return;
    }()));
```

## Building

> requirements:
> - GCC11
> - linux kernel version >= 5.5

```bash
mkdir build && cd build
cmake -DBUILD_EXAMPLES=ON ..
make -j
```

## TODO

- [ ] chunked transfers
- [ ] ssl support
- [ ] ...
