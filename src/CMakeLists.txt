add_executable (cqhttp cqhttp.cpp "cqhttp.h")

find_package(nlohmann_json 3.11.2 REQUIRED)
target_link_libraries(cqhttp PRIVATE nlohmann_json::nlohmann_json)

find_package(Boost COMPONENTS coroutine REQUIRED)
target_link_libraries(cqhttp INTERFACE Boost::beast PRIVATE Boost::coroutine)

if(MSVC)
    target_compile_options(cqhttp PRIVATE /bigobj)
endif()