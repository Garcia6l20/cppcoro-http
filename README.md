# CppCoro Http - http coroutine library for C++

The 'cppcoro-http' provides a set of classes for creating http servers/clients.
It is built on top of [cppcoro](https://github.com/lewissbaker/cppcoro) library.

## HTTP Server

- Example:

```c++
int main() {
    io_service service;

    auto do_serve = [&]() -> task<> {
        http::server server{service, *net::ip_endpoint::from_string("127.0.0.1:4242")};
        auto handle_conn = [](http::connection conn) mutable -> task<> {
            http::router router;
            // route definition
            auto &route = router.add_route<R"(/hello/(\w+))">();
            // complete handler
            route.complete([](const std::string &who) -> task<std::tuple<http::status, std::string>> {
                co_return std::tuple{
                    http::status::HTTP_STATUS_OK,
                    fmt::format("Hello {} !!", who)
                };
            });
            while (true) {
                try {
                    // wait next connection request
                    auto req = co_await conn.next();
                    if (req == nullptr)
                        break; // connection closed
                    // wait next router response
                    auto resp = co_await router.process(*req);
                    co_await conn.send(resp);
                } catch (std::system_error &err) {
                    if (err.code() == std::errc::connection_reset) {
                        break; // connection reset by peer
                    } else {
                        throw err;
                    }
                }
            }
        };
        async_scope scope;
        while (true) {
            scope.spawn(handle_conn(co_await server.listen()));
        }
    };
    (void) sync_wait(when_all(do_serve(), [&]() -> task<> {
        service.process_events();
        co_return;
    }()));
    return 0;
}
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
