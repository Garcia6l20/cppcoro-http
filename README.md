# CppCoro Http - http coroutine library for C++

The 'cppcoro-http' provides a set of classes for creating http servers/clients.
It is built on top of [cppcoro](https://github.com/lewissbaker/cppcoro) library.

## HTTP Server

```c++
template<typename ConfigT>
struct session : http::session<ConfigT>
{
	explicit session(http::server<ConfigT>& server)
		: http::session<ConfigT>{ server }
	{
	}
	int id = std::rand();
};

using config = http::config<session, tcp::ipv4_socket_provider>;

template<typename ConfigT>
struct hello_controller;

template<typename ConfigT>
using hello_controller_def = http::route_controller<
	R"(/hello/(\w+))",  // route definition
	ConfigT,
	http::string_request,
	hello_controller>;

template<typename ConfigT>
struct hello_controller : hello_controller_def<ConfigT>
{
	using hello_controller_def<ConfigT>::hello_controller_def;
	// method handlers
	auto on_post(std::string_view who) -> task<http::string_response>
	{
		co_return http::string_response{ http::status::HTTP_STATUS_OK,
										 fmt::format(
											 "post at {}: hello {}", this->session().id, who) };
	}
	auto on_get(std::string_view who) -> task<http::string_response>
	{
		co_return http::string_response{ http::status::HTTP_STATUS_OK,
										 fmt::format(
											 "get at {}: hello {}", this->session().id, who) };
	}
};

struct hello_chunk_provider : http::abstract_chunk_base
{
	std::string_view who;
	using http::abstract_chunk_base::abstract_chunk_base;
	hello_chunk_provider(io_service& service, std::string_view who)
		: http::abstract_chunk_base{ service }
		, who{ who }
	{
	}
	async_generator<std::string_view> read(size_t)
	{
		co_yield "hello\n";
		co_yield fmt::format("{}\n", who);
	}
};
using hello_chunked_response = http::abstract_response<hello_chunk_provider>;

template<typename ConfigT>
struct hello_chunk_controller;

template<typename ConfigT>
using hello_chunk_controller_def = http::route_controller<
	R"(/chunk/(\w+))",  // route definition
	ConfigT,
	http::string_request,
	hello_chunk_controller>;

template<typename ConfigT>
struct hello_chunk_controller : hello_chunk_controller_def<ConfigT>
{
	using hello_chunk_controller_def<ConfigT>::hello_chunk_controller_def;
	// method handlers
	task<hello_chunked_response> on_get(std::string_view who)
	{
		co_return hello_chunked_response{ http::status::HTTP_STATUS_OK,
										  hello_chunk_provider{ this->service(), who } };
	}
};

int main()
{
	io_service service;

	auto do_serve = [&]() -> task<> {
		auto _ = on_scope_exit([&] { service.stop(); });
		http::controller_server<config, hello_controller, hello_chunk_controller> server{
			service, *net::ip_endpoint::from_string("127.0.0.1:4242")
		};
		co_await server.serve();
	};
	(void)sync_wait(when_all(do_serve(), [&]() -> task<> {
		service.process_events();
		co_return;
	}()));
	return 0;
}
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

#### SSL support

```bash
mkdir build && cd build
cmake -DBUILD_EXAMPLES=ON ..
make -j
./examples/simple_co_http_server/bin/simple_co_http_server --ssl 127.0.0.1:4242 .
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
- [x] ssl support ([mbed-tls](https://github.com/ARMmbed/mbedtls))
- [x] cookies
- [ ] web sockets
- [ ] ...
