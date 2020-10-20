# CppCoro Http - http coroutine library for C++

The 'cppcoro-http' provides a set of classes for creating http servers/clients.
It is built on top of [cppcoro](https://github.com/lewissbaker/cppcoro) library.

## HTTP Server

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

## Examples

- *[readme](./examples/readme.cpp)* : Example in this README.
- *[hello_world](./examples/hello_world.cpp)* : Basic showcase.
- *[simple_co_http_server](./examples/simple_co_http_server)* : Same as `python3 -m http.server` in cppcoro.

## Building


### Linux

> requirements:
> - GCC11 (GCC >= 10 & clang >= 10 should be fine but not tested yet)
> - linux kernel version >= 5.5

1. Install liburing
    
    - arch:
    ```bash
    sudo pacman -Sy liburing
    ```
    
    - others:
    > Make sure your kernel is >= 5.6
    ```
    uname -r
    [...]
    git clone https://github.com/axboe/liburing.git
    mkdir liburing/build && cd liburing/build
    make -j && sudo make install
    ```

1. Install cppcoro
    
    ```bash
    git clone https://github.com/Garcia6l20/cppcoro.git
    mkdir cppcoro/build && cd cppcoro/build
    cmake -DCPPCORO_USE_IO_RING=ON ..
    make -j && sudo make install
    ```

1. Install conan
    ```bash
    python -m pip install --upgrade conan
    ```

1. Build

    ```bash
    mkdir build && cd build
    cmake -DBUILD_EXAMPLES=ON ..
    make -j
    ```

### Windows

> requirements:
> - Linux
   
### Development

You can also use cppcoro without installing it for development purposes:

```bash
cmake -DCPPCORO_DEVEL=ON ..
```

## TODO

- [x] chunked transfers (server-side)
- [ ] chunked transfers (client-side)
- [ ] ssl support ([mbed-tls](https://github.com/ARMmbed/mbedtls))
- [ ] ...
