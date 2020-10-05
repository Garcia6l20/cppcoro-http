# CppCoro Http - http coroutine library for C++

The 'cppcoro-http' provides a set of classes for creating http servers/clients.
It is built on top of [cppcoro](https://github.com/lewissbaker/cppcoro) library.

## HTTP Server

- example:

```c++
struct session {
    int id = std::rand();
};

using hello_controller_def = http::route_controller<
    R"(/hello/(\w+))",  // route definition
    session,
    http::string_request,
    struct hello_controller>;

struct hello_controller : hello_controller_def
{
    using hello_controller_def::hello_controller_def;
    // method handlers
    auto on_post(std::string_view who) -> task<http::string_response> {
        co_return http::string_response{http::status::HTTP_STATUS_OK,
                                 fmt::format("post at {}: hello {}", session().id, who)};
    }
    auto on_get(std::string_view who) -> task<http::string_response> {
        co_return http::string_response{http::status::HTTP_STATUS_OK,
                                 fmt::format("get at {}: hello {}", session().id, who)};
    }
};

io_service service;

auto do_serve = [&]() -> task<> {
    auto _ = on_scope_exit([&] {
        service.stop();
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
> - my [cppcoro](https://github.com/Garcia6l20/cppcoro) fork

```bash
mkdir build && cd build
cmake -DBUILD_EXAMPLES=ON ..
make -j
```

## Development

You can also use cppcoro without installing it for development purposes:

```bash
cmake -DCPPCORO_DEVEL=ON ..
```

## Examples

- *examples/readme.cpp*: Example in this README.
- *examples/hello_world.cpp*: Basic showcase.
- *examples/simple_co_http_server*: Same as `python3 -m http.server` in cppcoro.

## TODO

- [x] chunked transfers
- [ ] ssl support
- [ ] ...
