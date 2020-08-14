# CppCoro Http - http coroutine library for C++

The 'cppcoro-http' provides a set of classes for creating http servers/clients.
It is built on top of [cppcoro](https://github.com/lewissbaker/cppcoro) library.

## HTTP Server

- Example:

```c++
io_service service;

auto do_serve = [&]() -> task<> {
    http::route_server server{service, *net::ip_endpoint::from_string("127.0.0.1:4242")};
    // route definition
    auto &route = server.add_route<R"(/hello/(\w+))">();
    // complete handler
    route.on_complete([](const std::string &who) -> task<std::tuple<http::status, std::string>> {
        co_return std::tuple{
            http::status::HTTP_STATUS_OK,
            fmt::format("Hello {} !!", who)
        };
    });
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
