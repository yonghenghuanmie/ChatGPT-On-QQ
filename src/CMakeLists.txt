add_executable (websocket-client-coro
    websocket_client_coro.cpp
)

find_package(Boost REQUIRED COMPONENTS coroutine)
target_link_libraries(websocket-client-coro INTERFACE Boost::beast PRIVATE Boost::coroutine)

if(MSVC)
    target_compile_options(websocket-client-coro PRIVATE /bigobj)
endif()